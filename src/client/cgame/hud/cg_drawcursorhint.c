// Source: uo_cgame_mp_x86.dll 0x300303a0..0x30030c5a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300303a0_30030c5a.mcode
//
// CG_DrawCursorhint — the "draw" half of the animated on-screen usable-entity /
// crosshair hint overlay (pick-up ammo/weapon/health, swap weapons, LMG mount,
// generic activate/use). Its seed/latch half is CG_LatchOverlaySource
// (0x3001a5b0), whose only caller is this drawer (call at 0x300303db); that
// routine copies cg_snap source fields into the persistent overlay block
// (cg_usableHintKind/ColorByte/CommandIndex + the CG_FadeColor timing pair) and
// this function immediately consumes them.
//
// NAME ADJUDICATION: the .mcode header names this PM_Footsteps, an explicit SIZE
// guess ("win size 0x8ba, matched size 0x878"), REJECTED per the no-size-matching
// rule. The body touches NO pmove/footstep state; it resolves localized hint
// strings ("CGAME_PICKUPAMMO"/"CGAME_PICKUPNEWWEAPON"/"CGAME_SWAPWEAPONS"/
// "CGAME_PICKUPHEALTH"/"GMI_CGAME_LMGMOUNTPOINT"/"Hint String", ...) and the key
// bound to "+activate"/"toggle cl_run"/"+speed", fades a HUD color via CG_FadeColor, and draws
// hint text + an optional weapon icon and bar. The Mac cgame symbol
// CG_DrawCursorhint shares 11 direct named callees with this body; its only callset
// differences are the platform translation wrapper and trap_R_SetColor. Behavior
// and the cross-architecture call fingerprint therefore resolve the source name.
//
// REGISTER ABI (proven from the sole caller at 0x300321c4): ECX = &hintRect
// (LEA ECX,[esp+0x18]); three dwords are then pushed (edx,ecx,eax) and read by
// this callee at [esp+0x830]/[esp+0x834]/[esp+0x838]; the caller cleans them (ADD
// ESP,0xc). Bare RET => cdecl caller-cleanup of the three stack args, plus the ECX
// register pointer. hintRect is a vec4 of screen coords {x,y,width,height} read as
// [EBX+0]/[EBX+4]/[EBX+8]/[EBX+0xc]. The three stack dwords are the font/scale/
// style triplet: arg0 (EBP, [entry+4]) and arg1 ([entry+8], e.g. the
// [esp+0x83c] reload at 0x30030749) feed every trap_R_Text_Width/Height call,
// and arg2 ([entry+0xc], e.g. [esp+0x858] at 0x3003076b) is passed only as the
// trailing parameter of the trap_R_Text_Paint call.

#include "../client_recovered.h"
#include "../globals.h"
#include "client/common/client_format_validation.h"
#include "compat/coduo_native_x87.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// Exact .rdata float constants (dumped from the binary, not inferred):
//   0x3007bce0 = 1.0    0x3007bce8 = 0.5     0x3007c188 = 1/150 (0.006666667)
//   0x3007bdb4 = 0.01   0x3007bda4 = 10.0    0x3007bf50 = -0.5
//   0x3007be24 = 1/255  0x3007be40 = 4.0     immediate 0x41000000 = 8.0f, 0x40000000 = 2.0f
#define HINT_PHASE_RATE   0.006666666828095913f  /* 0x3007c188 */
#define HINT_MS_TO_FRAC   0.009999999776482582f  /* 0x3007bdb4 (1/100) */
#define HINT_BYTE_TO_FRAC 0.003921568859368563f  /* 0x3007be24 (1/255) */

// cgame_syscall id 72 (CG_R_SETCOLOR / trap_R_SetColor) and the dispatcher are
// declared in the shared headers.

// Style/animation selector cg_cursorHints_vmCvar.integer (0x305301ec).
enum {
    HINT_STYLE_PULSE_SUBSEC = 2,  // pulse on fractional seconds of the pickup timer
    HINT_STYLE_WOBBLE       = 3   // sine-wobble the hint rect height; suppress pulse
};

// Latched serverCursorHint (cg_usableHintKind, 0x3048ae08) dispatch values.
// Values 0..9 use the shared cursorHint_t names. Values above that base
// encode the two per-weapon pickup ranges proved below; they are hint selectors,
// not EV_* entity-event ids.
enum {
    HINT_KIND_PICKUP_LO     = 0x0b,  // weapon-pickup range #1 low
    HINT_KIND_PICKUP_HI     = 0x8a,  // weapon-pickup range #1 high
    HINT_KIND_PICKUP2_LO    = 0x8b,  // weapon-pickup range #2 low
    HINT_KIND_PICKUP2_HI    = 0x10a, // weapon-pickup range #2 high
    HINT_KIND_WEAPON_BIAS   = 0x0a,  // pickup kind - 10 == weapon index (ADD ESI,-0xa)
    HINT_KIND_PICKUP2_BIAS  = 0x8a   // range #2: kind - 0x8a == weapon index
};

enum { HINT_CFGSTRING_BASE = 1365 }; // gameState hint slot base (index + 1365)

/* NOT_FROM_ORIGINAL_SOURCE: mounted localization supplies the cursor-hint
 * formats. Keep malformed templates visible as literal text while preventing
 * them from consuming nonexistent variadic arguments. */
static qboolean cgame_compat_hint_format_is_valid(
    const char *format, const char *signature)
{
    if (client_compat_validate_format_signature(format, signature) == qfalse) {
        Com_Printf("WARNING: rejected invalid cursor-hint format\n");
        return qfalse;
    }
    return qtrue;
}

// The screen-space object passed in ECX is the shared rectDef_t.

// Splice the bound key name into a localized hint template at its "[%s]" marker
// (.rdata 0x30079840 = 5b 25 73 5d 00 = "[%s]"): the output is the head THROUGH
// the '[' + keyName + the tail FROM the ']' (marker+3), i.e. "...[KEY]...".
// The head copy is Q_strncpyz(dst, tmpl, markerIdx+2) in its inlined MSVC form:
// CRT strncpy (0x3005be00) with n = markerIdx+1 followed by an explicit NUL store
// at dst[markerIdx+1] (0x300306ba / 0x30030819 / 0x30030a03). The two appends are
// the inline REP MOVSD/MOVSB strcat runs (0x300306c4.., 0x30030820.., 0x30030a40..).
/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
static qboolean cgame_compat_splice_hint_key(
    char *dst, size_t dstSize, const char *tmpl, const char *marker,
    const char *keyName)
{
    const size_t headLength = (size_t)(marker - tmpl) + 1;
    const size_t keyLength = strlen(keyName);
    const size_t tailLength = strlen(marker + 3);
    size_t remaining;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (headLength >= dstSize) {
        goto too_long;
    }
    remaining = dstSize - headLength;
    if (keyLength >= remaining) {
        goto too_long;
    }
    remaining -= keyLength;
    if (tailLength >= remaining) {
        goto too_long;
    }

    Q_strncpyz(dst, tmpl, (int32_t)headLength + 1);  // head incl. the '['
    coduo_client_crt_strcpy(dst + strlen(dst), keyName);     // append key name
    coduo_client_crt_strcpy(dst + strlen(dst), marker + 3);  // resume at ']'
    return qtrue;

too_long:
    Com_Printf(
        "WARNING: rejected oversized cursor-hint key substitution\n");
    return qfalse;
}

void CG_DrawCursorhint(rectDef_t *hintRect,
                       int32_t fontContext, int32_t scaleBits,
                       const vec4_t color, int32_t textStyle)
{
    rectDef_t *r = hintRect;  // EBX = ECX
    // fontContext == arg0 (EBP), scaleBits == arg1 (both feed every
    // trap_R_Text_Width/Height call), color == arg2 (the generic owner-draw
    // color slot, ignored by this painter), and textStyle == arg3 (the
    // trap_R_Text_Paint trailing parameter only).
    (void)color;

    int32_t drawTextWidth = 0;   // trap_R_Text_Width result (text width metric)
    int32_t drawTextHeight = 0;  // trap_R_Text_Height result (line height metric)

    // 0x300303a0: canary; 0x300303b2..0x300303d4: bail out if the hint is disabled.
    // The style latch cg_cursorHints_vmCvar.integer is read here as the enable gate.
    vec_t pulseScale = 1.0f;   // [ESP+0x20] seeded 1.0f
    vec_t pulseHalf  = 0.0f;   // [ESP+0x1c] seeded 0

    if (cg_cursorHints_vmCvar.integer == 0) {
        return;  // 0x300303d4 JZ -> canary + epilogue, no draw.
    }

    // 0x300303db: seed the overlay source from cg_snap.
    CG_LatchOverlaySource();

    // 0x300303e0: dispatch selector + validity-table gate. The original indexes
    // one physically contiguous qhandle_t band beginning at 0x3044b6f8:
    // selector -1 is cgs_media_hudNoWeaponIcon, 0..9 are fixed hint shaders,
    // 10..137 are weapon HUD icons, and 138..265 are weapon ammo icons.
    int32_t kind = cg_usableHintKind;                 // [0x3048ae08]
    qhandle_t hintShader = 0;

    /* The instruction is one flat unchecked load, but its sole snapshot
     * producer is an unsigned eight-bit netfield, so every remotely reachable
     * selector is 0..255 and remains inside this physical band. Spell the band
     * with its native typed objects so their separate C-array provenance and
     * linker placement stay valid on 64-bit hosts. The -1 and 256..265 cases
     * retain the original local-state mapping too; selector 266 reads the
     * original zero padding after the band. */
    if (kind == -1) {
        hintShader = cgs_media_hudNoWeaponIcon;
    } else if (kind >= CURSOR_HINT_OFF &&
               kind < CURSOR_HINT_BUILTIN_ICON_COUNT) {
        hintShader = cgs_media_usableHintShaders[kind];
    } else if (kind >= CURSOR_HINT_WEAPON_BASE &&
               kind < CURSOR_HINT_WEAPON_BASE + MAX_WEAPONS) {
        hintShader = cg_weaponHudIcons[kind - CURSOR_HINT_WEAPON_BASE];
    } else if (kind >= CURSOR_HINT_WEAPON_BASE + MAX_WEAPONS &&
               kind < CURSOR_HINT_WEAPON_BASE + 2 * MAX_WEAPONS) {
        hintShader = cg_weaponAmmoIcons[
            kind - (CURSOR_HINT_WEAPON_BASE + MAX_WEAPONS)];
    }

    // 0x300303f3: kind<0 skips the <=1 test; kinds 0/1 draw nothing.
    if (kind >= CURSOR_HINT_OFF && kind <= CURSOR_HINT_NONE) {
        return;  // 0x300303f8 JLE 0x30030c44
    }
    if (hintShader == 0) {
        return;  // 0x30030400 JZ 0x30030c44
    }

    // 0x30030406: fade the hint color. EDX=startMsec, ECX=totalMsec.
    vec_t *fadeColor = CG_FadeColor(
        coduo_int32_from_bits((uint32_t)cg_overlayFadeStartTime),
        coduo_int32_from_bits((uint32_t)cg_overlayFadeDuration));
    if (fadeColor == (vec_t *)0) {
        // 0x30030422: fully faded -> reset draw color, clear the latch, stop.
        cgame_syscall(CG_R_SETCOLOR, (uint32_t)0);
        cg_usableHintKind = CURSOR_HINT_OFF;          // MOV [0x3048ae08],EDI (EDI==0)
        return;
    }

    // 0x3003044b: style discriminant.
    int32_t style = cg_cursorHints_vmCvar.integer;               // [0x305301ec]

    // 0x30030455: style 3 sine-modulates the fade color's alpha in place.
    //   fadeColor[3] = (sin(cg_time * 1/150) + 1.0) * fadeColor[3] * 0.5
    // (FILD cg.time; FMUL 1/150; FSIN; FADD 1.0; FMUL [EDI+0xc]; FMUL 0.5; FSTP [EDI+0xc]).
    if (style == HINT_STYLE_WOBBLE) {
        const long double phase = coduo_x87_sinl(
            (long double)coduo_int32_from_bits((uint32_t)cg_time) *
            (long double)HINT_PHASE_RATE);
        fadeColor[3] = (float)((phase + 1.0L) *
                               (long double)fadeColor[3] * 0.5L);
        style = cg_cursorHints_vmCvar.integer;                   // 0x30030475 reload
        kind  = cg_usableHintKind;                    // 0x3003047a reload ESI
    }

    // 0x30030483..0x300304eb: compute the pulse color scale. pulseScale goes to
    // [ESP+0x20]; pulseHalf ([ESP+0x1c]) is always pulseScale * 0.5 (shared tail at
    // 0x300304dd) except in the style>=3 case where both are zero.
    if (style >= HINT_STYLE_WOBBLE) {
        pulseScale = 0.0f;   // [ESP+0x20]
        pulseHalf  = 0.0f;   // [ESP+0x1c]
    } else {
        if (style == HINT_STYLE_PULSE_SUBSEC) {
            // fractional milliseconds of the pickup timer (0x3048ae0c reused as a
            // signed ms value % 1000), scaled by 1/100.
            int32_t rem =
                coduo_int32_from_bits((uint32_t)cg_overlayFadeStartTime) % 1000;
            pulseScale = (float)((long double)rem *
                                 (long double)HINT_MS_TO_FRAC);
        } else {
            // default style: fixed sine pulse (sin+1.0)*0.5*10.0.
            const long double phase = coduo_x87_sinl(
                (long double)coduo_int32_from_bits((uint32_t)cg_time) *
                (long double)HINT_PHASE_RATE);
            pulseScale = (float)((phase + 1.0L) * 0.5L * 10.0L);
        }
        pulseHalf = (float)((long double)pulseScale * 0.5L);
    }
    // pulseScale/pulseHalf are consumed by the icon-rect composition at
    // 0x30030b95..0x30030bd2 below.

    // Shared draw state built by the per-kind branches.
    char pickupTextBuf[MAX_STRING_CHARS]; // original [ESP+0x30..0x42f]
    char rawHintTextBuf[MAX_STRING_CHARS]; // original [ESP+0x430..0x82f]
    const char *drawText = 0;        // the localized/spliced text to draw
    cgWeaponInfo_t *cgWeap = 0;      // cg_weaponInfos[] entry (weapon-icon draw), or NULL
    vec_t iconWidthScale = 1.0f;     // [ESP+0x28]
    vec_t iconWideBias = 0.0f;       // [ESP+0x24]
    char *activateKey = 0;
    char *outKey = 0;

    if (kind >= HINT_KIND_PICKUP_LO && kind <= HINT_KIND_PICKUP_HI) {
        // ---- weapon-pickup range #1 (0x0b..0x8a) ----
        // 0x30030500: bg_weaponInfos[kind - 10] (machine indexes [base + kind*4 - 0x28],
        // i.e. -0x28 bytes == -10 pointer entries).
        weaponInfo_t *weap = bg_weaponInfos[kind - HINT_KIND_WEAPON_BIAS];
        int32_t weaponIndex = kind - HINT_KIND_WEAPON_BIAS;   // ADD ESI,-0xa
        cgWeap = &cg_weaponInfos[weaponIndex];                // [ESP+0x10]

        // 0x3003051e: wide-list-icon weapons -> doubled icon width, -0.5*WIDTH bias
        // (0x30030528 FLD dword [EBX+0x8] = rect.width).
        if (weap->wideListIcon != 0) {
            iconWideBias = (float)((long double)r->w *
                                   (long double)-0.5f);
            iconWidthScale = 2.0f;                // 0x40000000
        }

        // 0x3003053d: is there a free inventory slot for this weapon?
        //   ECX = weaponIndex, EDX = &cg.predictedPlayerState (0x304831c4).
        int32_t emptySlot =
            BG_GetEmptySlotForWeapon(&cg_predictedPlayerState, weaponIndex);
        if (emptySlot == 0) {
            // 0x30030551: no free slot -> "swap weapons" hint (or nothing if identical).
            Controls_GetConfig();
            UI_KeysStringForBinding("+activate", &activateKey);

            int32_t curWeaponIndex = cg_predictedPlayerState.currentWeapon;      // [0x3048329c]
            weaponInfo_t *curWeap = bg_weaponInfos[curWeaponIndex];
            if (curWeap->slot == weap->slot) {
                // 0x30030581: same slot.
                if (curWeaponIndex == weaponIndex) {
                    return;  // identical weapon -> draw nothing (0x30030583).
                }
                const char *forStr =
                    CG_SafeTranslateString_Internal("cgame", "CGAME_FOR");             // 0x30079880
                const char *swapFmt =
                    CG_SafeTranslateString_Internal("cgame", "CGAME_SWAPWEAPONS");     // 0x3007986c
                const char *pickName = cgWeap->displayName;                    // [+0x10]+0xb0
                const char *curName =
                    cg_weaponInfos[curWeaponIndex].displayName; // +0xb0
                // 0x300305ce: va(swapFmt, activateKey, curName, forStr, pickName);
                // then ADD ESP,0x8 pops only fmt+activateKey, so the second va
                // (0x300305dc, fmt 0x30079860 = "%s %s %s %s") re-reads the three
                // leftover varargs: va("%s %s %s %s", swapMsg, curName, forStr,
                // pickName). The partial-cleanup idiom is reproduced by passing the
                // machine's real argument list explicitly.
                const char *swapMsg;
                /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
                if (cgame_compat_hint_format_is_valid(swapFmt, "s") ==
                    qfalse) {
                    swapMsg = swapFmt;
                } else {
                    swapMsg =
                        va(swapFmt, activateKey, curName, forStr, pickName);
                }
                drawText = va("%s %s %s %s", swapMsg, curName, forStr, pickName);
            } else {
                // 0x300305e9: different slot -> use the picked-up weapon's own
                // predicted-slot occupant (cg_predictedPlayerState.weaponSlots[weap->slot]).
                int32_t occupant =
                    (int8_t)cg_predictedPlayerState.weaponSlots[weap->slot];   // MOVSX byte
                if (occupant == weaponIndex) {
                    return;  // 0x300305f2 -> draw nothing.
                }
                const char *forStr = CG_SafeTranslateString_Internal("cgame", "CGAME_FOR");
                const char *swapFmt = CG_SafeTranslateString_Internal("cgame", "CGAME_SWAPWEAPONS");
                const char *pickName = cgWeap->displayName;
                int32_t occSlotWeapon =
                    (int8_t)cg_predictedPlayerState.weaponSlots[weap->slot]; // MOVSX [ECX+0x30483708]
                /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
                if ((uint32_t)occSlotWeapon >= MAX_WEAPONS) {
                    Com_Printf(
                        "WARNING: invalid cursor-hint weapon slot value %i\n",
                        occSlotWeapon);
                    return;
                }
                const char *occName =
                    cg_weaponInfos[occSlotWeapon].displayName;
                // 0x30030644: va(swapFmt, activateKey, occName, forStr, pickName);
                // 0x30030649 ADD ESP,0x8 leaves occName/forStr/pickName in the
                // vararg area for the second va (0x30030652), same partial-cleanup
                // idiom as above.
                const char *swapMsg;
                if (cgame_compat_hint_format_is_valid(swapFmt, "s") ==
                    qfalse) {
                    swapMsg = swapFmt;
                } else {
                    swapMsg =
                        va(swapFmt, activateKey, occName, forStr, pickName);
                }
                drawText = va("%s %s %s %s", swapMsg, occName, forStr, pickName);
            }
            goto measure_and_paint;
        }

        // 0x3003065f: a free slot exists -> "pick up NEW weapon".
        const char *tmpl = CG_SafeTranslateString_Internal("cgame", "CGAME_PICKUPNEWWEAPON"); // 0x30079848
        const char *marker = strstr(tmpl, "[%s]");     // 0x30030670 PUSH 0x30079840 = "[%s]"
        if (marker != 0 && *marker != '\0') {
            if (cgame_compat_hint_format_is_valid(tmpl, "ss") == qfalse) {
                drawText = tmpl;
                goto measure_and_paint;
            }
            Controls_GetConfig();
            UI_KeysStringForBinding("+activate", &outKey);
            if (cgame_compat_splice_hint_key(
                    pickupTextBuf, sizeof(pickupTextBuf), tmpl, marker,
                    outKey) == qfalse) {
                drawText = tmpl;
                goto measure_and_paint;
            }
            // 0x3003073a: va(textBuf, cgWeap->displayName) into the ring buffer.
            if (cgame_compat_hint_format_is_valid(pickupTextBuf, "s") ==
                qfalse) {
                drawText = pickupTextBuf;
            } else {
                drawText = va(pickupTextBuf, cgWeap->displayName);
            }
        } else {
            // 0x30030718: no marker -> copy the template as-is, then va() the name.
            if (cgame_compat_hint_format_is_valid(tmpl, "s") == qfalse) {
                drawText = tmpl;
                goto measure_and_paint;
            }
            coduo_client_crt_strcpy(pickupTextBuf, tmpl);
            drawText = va(pickupTextBuf, cgWeap->displayName);
        }
        goto measure_and_paint;
    }

    if (kind >= HINT_KIND_PICKUP2_LO && kind <= HINT_KIND_PICKUP2_HI) {
        // ---- weapon-pickup range #2 (0x8b..0x10a): "pick up ammo" ----
        int32_t weaponIndex = kind - HINT_KIND_PICKUP2_BIAS;   // ADD ESI,0xffffff76 == -0x8a
        cgWeap = &cg_weaponInfos[weaponIndex];                 // [ESP+0x10]
        const char *tmpl = CG_SafeTranslateString_Internal("cgame", "CGAME_PICKUPAMMO"); // 0x3007982c
        const char *marker = strstr(tmpl, "[%s]");     // 0x300307cc PUSH 0x30079840
        if (marker != 0 && *marker != '\0') {
            if (cgame_compat_hint_format_is_valid(tmpl, "ss") == qfalse) {
                drawText = tmpl;
                goto measure_and_paint;
            }
            Controls_GetConfig();
            UI_KeysStringForBinding("+activate", &outKey);
            if (cgame_compat_splice_hint_key(
                    pickupTextBuf, sizeof(pickupTextBuf), tmpl, marker,
                    outKey) == qfalse) {
                drawText = tmpl;
                goto measure_and_paint;
            }
        } else {
            if (cgame_compat_hint_format_is_valid(tmpl, "s") == qfalse) {
                drawText = tmpl;
                goto measure_and_paint;
            }
            coduo_client_crt_strcpy(pickupTextBuf, tmpl);
        }
        // 0x3003088a (join of both paths): va(textBuf, cgWeap->displayName) —
        // the ammo template gets the same displayName substitution as the
        // pickup-weapon branch (MOV EDX,[ECX+0xb0]; PUSH; LEA EAX,textBuf; PUSH;
        // CALL 0x3004e8a0).
        if (cgame_compat_hint_format_is_valid(pickupTextBuf, "s") == qfalse) {
            drawText = pickupTextBuf;
        } else {
            drawText = va(pickupTextBuf, cgWeap->displayName);
        }
        goto measure_and_paint;
    }

    if (kind == CURSOR_HINT_LMG) {
        // ---- LMG-mount hint: prefer "toggle cl_run", else +speed. ----
        // 0x300308fc: first lookup ECX = 0x30076a98 = "toggle cl_run"; only when
        // the result stricmp-equals "Unbound" does 0x30030920 re-look-up
        // ECX = 0x30076a90 = "+speed" (string bytes dumped from .rdata).
        Controls_GetConfig();
        UI_KeysStringForBinding("toggle cl_run", &outKey);     // 0x30076a98
        if (coduo_crt_stricmp(outKey, "Unbound") == 0) {      // 0x30079824
            UI_KeysStringForBinding("+speed", &outKey);        // 0x30076a90
        }
        const char *tmpl =
            CG_SafeTranslateString_Internal("cgame", "GMI_CGAME_LMGMOUNTPOINT"); // 0x3007980c
        if (cgame_compat_hint_format_is_valid(tmpl, "s") == qfalse) {
            drawText = tmpl;
        } else {
            drawText = va(tmpl, outKey);                       // 0x3003093f
        }
        goto measure_and_paint;
    }

    // 0x30030973: raw-command / mount-health branch, keyed by cg_usableHintCommandIndex.
    if (cg_usableHintCommandIndex >= 0) {
        // ---- gameState hint config string (index + 0x555). ----
        const int32_t cfgIndex = coduo_int32_from_bits(
            (uint32_t)cg_usableHintCommandIndex + HINT_CFGSTRING_BASE);
        const char *cfg = CG_ConfigString(cfgIndex);
        if (cfg == 0 || *cfg == '\0') {
            // 0x30030990/0x30030999: empty -> JZ 0x30030b8c, i.e. straight to the
            // SetColor(fadeColor) + icon + bar tail (no text, no early return).
            goto set_color_and_icon;
        }
        // 0x3003099f: CG_TranslateMessage(cfg, "Hint String").
        const char *hint = CG_TranslateMessage(cfg, "Hint String");     // 0x30079800
        // 0x300309c0: inline strcpy(textBuf(esp+0x430), hint), then lowercase the
        // COPY (0x300309d1 CALL Q_strlwr) — the lowercase buffer exists only to
        // make the marker probe case-insensitive; the drawn/spliced text is built
        // from the ORIGINAL translated string, preserving its casing.
        coduo_client_crt_strcpy(rawHintTextBuf, hint);
        Q_strlwr(rawHintTextBuf);
        const char *marker = strstr(rawHintTextBuf, "[%s]");
        if (marker != 0) {
            int32_t markerIdx = (int32_t)(marker - rawHintTextBuf);
            Controls_GetConfig();
            int32_t n = UI_KeysStringForBinding("+activate", &outKey);
            if (n == 0) {
                // 0x30030a28: no binding -> fall back to the localized "KEY_USE".
                outKey = (char *)CG_SafeTranslateString_Internal("cgame", "KEY_USE"); // 0x300797f8
            }
            // 0x300309fc/0x30030a69: head Q_strncpyz'd from the ORIGINAL translated
            // string (source ECX = [ESP+0x10]) into the stack buffer, tail resumed
            // at translated + markerIdx + 3 (ADD EBP,0x3).
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            if (cgame_compat_splice_hint_key(
                    rawHintTextBuf, sizeof(rawHintTextBuf), hint,
                    hint + markerIdx, outKey) == qfalse) {
                drawText = hint;
            } else {
                drawText = rawHintTextBuf;
            }
        } else {
            // 0x30030aa7 (MOV EDI,ESI): no marker -> draw the ORIGINAL translated
            // string, not the lowercased buffer.
            drawText = hint;
        }
        goto measure_and_paint;
    }

    if (kind == CURSOR_HINT_HEALTH) {
        // ---- health hint (+activate binding). ----
        Controls_GetConfig();
        UI_KeysStringForBinding("+activate", &outKey);         // 0x3007988c
        const char *tmpl =
            CG_SafeTranslateString_Internal("cgame", "CGAME_PICKUPHEALTH");  // 0x300797e4
        if (cgame_compat_hint_format_is_valid(tmpl, "s") == qfalse) {
            drawText = tmpl;
        } else {
            drawText = va(tmpl, outKey);                        // 0x30030b1a
        }
        goto measure_and_paint;
    }

    // 0x30030aec: any other kind -> JNZ 0x30030b8c: skip the text, but still set
    // the faded color and draw the icon (and bar) like every other path.
    goto set_color_and_icon;

measure_and_paint:
    // 0x30030749 (weapon), 0x3003088a.. (ammo), 0x30030944.. (use), 0x30030ab0..
    // (raw command), 0x30030b1f.. (mount/health): every text path measures with the
    // same shape before the shared trap_R_Text_Paint join at 0x30030b5e/0x30030b5f.
    {
        // trap_R_Text_Width(drawText, fontContext, scaleBits, 0) — pushes 0,
        // arg1 (e.g. EDI = [esp+0x83c] at 0x30030749), EBP (arg0), text.
        drawTextWidth  = trap_R_Text_Width(drawText, fontContext, scaleBits, 0);
        // trap_R_Text_Height(fontContext, scaleBits).
        drawTextHeight = trap_R_Text_Height(fontContext, scaleBits);
    }

    // 0x30030b5e..0x30030b80: draw the hint text with trap_R_Text_Paint
    // (0x3003de30). Placement proven from the x87 stream at 0x30030b5f (ST0 =
    // heightF FILD'd last, ST1 = widthF):
    //   y = rect.y - textHeight                       (FLD [ebx+4]; FSUB ST0,ST1)
    //   x = rect.x + (rect.width - textWidth) * 0.5   (FLD [ebx+8]; FSUB; *0.5; +[ebx])
    // Argument list (bottom-up, ADD ESP,0x24 = 9 dwords at 0x30030b89):
    //   (x, y, fontContext, scaleBits, fadeColor, text, 0, 0, textStyle).
    {
        float placeY = (float)((long double)r->y -
                               (long double)drawTextHeight);
        float placeX = (float)((long double)r->x +
            ((long double)r->w - (long double)drawTextWidth) * 0.5L);
        trap_R_Text_Paint((intptr_t)CG_FloatBits(placeX),
                          (intptr_t)CG_FloatBits(placeY),
                          fontContext, scaleBits,
                          (intptr_t)fadeColor, (intptr_t)drawText,
                          0, 0, textStyle);
    }

set_color_and_icon:
    // 0x30030b8c: set the hint color to the faded RGBA. Also the join target of
    // the empty-config (0x30030990/0x30030999) and unknown-kind (0x30030aec)
    // paths, which skip the text but still draw the icon and bar.
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)fadeColor);

    // 0x30030b95..0x30030bd2: draw the hint icon on EVERY path (no weapon gate).
    // The rect grows by the pulse (width/height + pulseScale) and shifts up-left
    // by pulseHalf; wide-list weapon icons add the width scale/bias computed
    // earlier. CG_DrawPic's hShader is the selector-indexed handle saved from the
    // 0x3044b6f8 media band at 0x300303e8..0x300303ef, reloaded at 0x30030b98.
    {
        float iconX = (float)((long double)r->x -
                              (long double)pulseHalf +
                              (long double)iconWideBias);
        float iconY = (float)((long double)r->y -
                              (long double)pulseHalf);
        float iconW = (float)((long double)r->w *
                              (long double)iconWidthScale +
                              (long double)pulseScale);
        float iconH = (float)((long double)r->h +
                              (long double)pulseScale);
        CG_DrawPic(iconX, iconY, iconW, iconH, hintShader);
    }

    // 0x30030bd7: reset the draw color.
    cgame_syscall(CG_R_SETCOLOR, (uint32_t)0);

    // 0x30030be1: optional filled-bar overlay keyed by the latched color byte.
    if (cg_usableHintColorByte != 0) {
        // 0x30030bed: the bar color {0, 0, 1.0, 0.5} is written THROUGH the
        // CG_FadeColor buffer (MOV [EDI],0 / [EDI+4],0 / [EDI+8],1.0 / [EDI+0xc],0.5).
        float frac = (float)(
            (long double)coduo_int32_from_bits((uint32_t)cg_usableHintColorByte) *
            (long double)HINT_BYTE_TO_FRAC);
        fadeColor[0] = 0.0f;
        fadeColor[1] = 0.0f;
        fadeColor[2] = 1.0f;
        fadeColor[3] = 0.5f;
        // 0x30030c08..0x30030c3b: CG_FilledBar(flags=0 (EBX), fill=NULL (ECX),
        // color3=NULL (EDX), then the six stack dwords bottom-up:
        //   x = rect.x (EAX = [ebx]), y = rect.h + rect.y + 4.0
        //   (FLD [ebx+0xc]; FADD [ebx+4]; FADD 4.0), w = rect.width (EDX=[ebx+8]),
        //   h = 8.0f (imm 0x41000000), borderColor = fadeColor, frac.
        CG_FilledBar(0, NULL, NULL,
                     r->x,
                     (float)((long double)r->h +
                             (long double)r->y + 4.0L),
                     r->w, 8.0f,
                     fadeColor, frac);
    }

    // 0x30030c43..0x30030c59: canary check + epilogue.
}
