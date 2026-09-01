// Source: uo_cgame_mp_x86.dll 0x3002ec10..0x3002ed53
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ec10_3002ed53.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_DrawPlayerWeaponName (0x3002ec10)
 *
 * A member of the cgame trap-54 2D-text draw family (siblings CG_DrawObituaryLine
 * 0x30031a90, CG_DrawRedScore 0x30031510, etc.). It draws the brief
 * on-screen weapon-name overlay shown for ~1800 ms after the local player
 * (re)selects a weapon, horizontally centered on the string's measured pixel
 * width. Nothing is drawn once the fade expires.
 *
 * Name adjudication: the .mcode header's size-matched guess
 * "BG_CalculateWeaponPosition_GunRecoil_SingleAngle" is REJECTED. That name is a
 * bg_pmove weapon-position/recoil helper; this function computes no recoil, reads
 * no view-angle state, and returns void. Its actual behavior is a HUD text draw:
 * it gates on player/vehicle state and a fade timer, selects the current weapon's
 * weaponInfo_t record, formats the weapon display name with va(), measures it with
 * cgame trap 52, and emits it with cgame trap 54 (via the shared 9-arg wrapper
 * trap_R_Text_Paint, 0x3003de30). The match was a pure 0x143/0x144 byte-size collision,
 * not evidence. The Mac CG_DrawPlayerWeaponName shares the fade, formatting, and
 * text-paint calls and performs the same weapon/mode-name draw, resolving the name. The
 * engine services behind traps 52/54 are likewise unproven (no cgame syscall-id
 * table recovered), matching how CG_R_TEXT_WIDTH/54 are treated in client_recovered.h.
 *
 * Register-argument ABI (custom regparm, matching the trap-54 family and proven
 * from the sole caller at 0x30032425):
 *   - EAX -> ESI: `color`, a pointer to 3 dwords (RGB) copied
 *                 into a local block (MOV [ESI]/[ESI+4]/[ESI+8]).
 *   - EDI       : `obj`, the rectDef_t* whose floats f_0/f_4/f_8 position the
 *                 draw (FLD [EDI+8]; FADD [EDI]; MOV [EDI+4]). Never spilled.
 *   - EBX       : `regWord`, forwarded verbatim to both traps. Incoming register
 *                 argument (used before any write, never saved/restored).
 *   - two cdecl stack args: arg0 = [E+4] (pushed EDX at the caller), arg1 = [E+8]
 *                 (pushed ECX). The function ends with `ADD ESP,0x14; RET` (no
 *                 immediate): the two incoming stack slots are caller-cleaned.
 * Modeled as source-order parameters; no calling-convention attribute is added
 * because the syntax-only build does not require one.
 *
 * Control flow (proven instruction-by-instruction):
 *   0x3002ec16 if (cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE):
 *                 // in the vehicle/turret HUD view: only draw while riding
 *                 if (cg_predictedPlayerState.vehicleType != 1) return;
 *                 if (cg_predictedPlayerState.vehiclePosition != 3) return;
 *   0x3002ec3c fade = CG_FadeColor(cg_weaponSelectTime, 1800);  // startMsec, totalMsec
 *   0x3002ec4e if (fade == NULL) return;   // fade not started / already expired
 *
 *   // Copy the anchor vec3 and the fade alpha into a 4-dword local passed by
 *   // pointer to the draw trap (a4 = &params, params = {origin[0..2], fade[3]}).
 *   0x3002ec54..ec72 params.origin[0..2] = origin[0..2]; params.alpha = fade[3];
 *
 *   // Select the weaponInfo_t record for the currently-selected weapon. Prefer
 *   // bg_weaponInfos[cg_weaponSelect_vmCvar.integer] when the index is in range AND the weapon's
 *   // held bit is set; otherwise fall back to cg_currentWeaponInfo.
 *   0x3002ec6b..ecab:
 *     int w = cg_weaponSelect_vmCvar.integer;
 *     if (w >= 0 && w < bg_numWeapons &&
 *         (cg_predictedPlayerState.weaponBits[w >> 5] & (1u << (w & 31))))
 *         wi = bg_weaponInfos[w];
 *     else
 *         wi = cg_currentWeaponInfo;
 *   0x3002ecab weaponIndex = wi->weaponIndex; if (weaponIndex == 0) return;
 *
 *   // cg_weaponInfos[weaponIndex] holds the display-name pointers.
 *   0x3002ecb8/ecc0 cgWeaponInfo_t *cwi = &cg_weaponInfos[weaponIndex];
 *   0x3002ecb5/ecbe/ecc5:
 *     if (wi->modeName != NULL && wi->modeName[0] != 0)
 *         text = va("%s / %s", cwi->displayName, cwi->modeName);
 *     else
 *         text = va("%s", cwi->displayName);
 *
 *   // Measure, then center and emit.
 *   0x3002ed0d width = cgame_syscall(CG_R_TEXT_WIDTH, text, regWord, arg0, 0);
 *   0x3002ed13 float x = obj->w + obj->x;             // FLD f_8; FADD f_0
 *   0x3002ed2b x -= (float)width;                          // FISUB dword width
 *   0x3002ed3c x -= 28.0f;                                 // FSUB 0x3007bdc8 (28.0f)
 *   0x3002ed46 trap_R_Text_Paint(x, obj->y, regWord, arg0, &params, text, 0, 0, arg1);
 *
 * Float precision: x is computed entirely on the x87 stack at single precision
 * (FLD/FADD float, FISUB of an int32, FSUB float), stored via FSTP to a 4-byte
 * slot, and forwarded to the variadic trap as a raw 32-bit bit pattern.
 * obj->y is likewise forwarded as its raw dword. Both go through CG_FloatBits so
 * the bit pattern is reproduced exactly (no double promotion), matching the family.
 *
 * The FSUB constant at .rdata 0x3007bdc8 is the single-precision float 0x41e00000
 * (= 28.0f). The va() format strings are "%s / %s" (0x30079998) and "%s"
 * (0x30076cd8). The two-name form pairs cg_weaponInfos[weaponIndex]->displayName
 * with ->modeName (the "%s / %s" fixed order at 0x3002ecc9/eccf pushes
 * +0xb4 then +0xb0, so +0xb0 is the first "%s").
 */

/* Vertical/horizontal draw bias: FSUB 28.0f (.rdata 0x3007bdc8, 0x41e00000). */
#define CG_WEAPON_NAME_X_BIAS 28.0f

/* CG_FadeColor lifetime for the selected-weapon-name overlay (ms). MOV ECX,0x708. */
enum { CG_WEAPON_NAME_FADE_MS = 1800 };

/* Vehicle-view gate discriminants (proven from the CMP immediates). Exact CoD
 * enum names for vehiclePosition/vehicleType are unproven; named by proven value. */
enum {
    CG_VEHICLE_TYPE_RIDING = 1,      /* cg_predictedPlayerState.vehicleType == 1 to draw */
    CG_VEHICLE_POSITION_DRIVER = 3   /* cg_predictedPlayerState.vehiclePosition == 3 to draw */
};

void CG_DrawPlayerWeaponName(const vec3_t color, rectDef_t *obj,
                               int32_t regWord, int32_t arg0, int32_t arg1)
{
    weaponInfo_t *wi;
    const char *text;
    float params[4]; /* {color[0], color[1], color[2], fadeColor[3]} */
    int32_t w;

    /*
     * 0x3002ec16 TEST cg_predictedPlayerState.entityStateFlags, EF_IN_VEHICLE:
     * when the flag is set (vehicle/turret HUD view), the overlay is drawn only
     * while the local player is riding in the driver position.
     */
    if (cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) {
        if (cg_predictedPlayerState.vehicleType != CG_VEHICLE_TYPE_RIDING)
            return;
        if (cg_predictedPlayerState.vehiclePosition != CG_VEHICLE_POSITION_DRIVER)
            return;
    }

    /*
     * 0x3002ec3c CG_FadeColor(cg_weaponSelectTime, 1800): NULL once the ~1800 ms
     * post-switch fade has expired (or has not started) -> draw nothing.
     * The returned vec4_t is white RGB with a ramping alpha at index 3.
     */
    {
        vec_t *fade = CG_FadeColor(cg_weaponSelectTime, CG_WEAPON_NAME_FADE_MS);
        if (fade == NULL)
            return;

        /* 0x3002ec54..ec72: copy the RGB color and the fade alpha into the
         * 4-dword local later handed to the draw trap by address. */
        params[0] = color[0];
        params[1] = color[1];
        params[2] = color[2];
        params[3] = fade[3];
    }

    /*
     * 0x3002ec6b..ecab: choose the weaponInfo_t record for the selected weapon.
     * bg_weaponInfos[cg_weaponSelect_vmCvar.integer] is used only when the index is in range and
     * the weapon's held bit is set; otherwise the cached cg_currentWeaponInfo.
     */
    w = cg_weaponSelect_vmCvar.integer;
    if (w >= 0 && w < bg_numWeapons &&
        (cg_predictedPlayerState.weaponBits[(uint32_t)w >> 5] & (1u << ((uint32_t)w & 31)))) {
        wi = bg_weaponInfos[w];
    } else {
        wi = cg_currentWeaponInfo;
    }

    /* 0x3002ecab..ecaf: weaponIndex 0 means "no weapon" -> draw nothing. */
    {
        int32_t weaponIndex = wi->weaponIndex;
        if (weaponIndex == 0)
            return;

        /*
         * 0x3002ecb5..ece6: format the weapon display name. The bg_weaponInfos
         * record's modeName (+0x78) selects the paired "%s / %s" form
         * (primary / secondary display names from cg_weaponInfos[weaponIndex])
         * when it is a non-empty string; otherwise the single "%s" primary name.
         */
        {
            cgWeaponInfo_t *cwi = &cg_weaponInfos[weaponIndex];
            const char *modeReference = wi->modeName;

            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (modeReference != NULL && modeReference[0] != '\0') {
                /* 0x3002ecc9 loads the translated mode label from
                 * cg_weaponInfos[weaponIndex] +0xb4. The weapon-file pointer at
                 * wi +0x78 is only the non-empty gate above. */
                text = va("%s / %s", cwi->displayName, cwi->modeName);
            } else {
                text = va("%s", cwi->displayName);
            }
        }
    }

    /*
     * 0x3002ed0d: measure the formatted string's pixel width (int32 return).
     * cgame_syscall(52, text, regWord, arg0, 0).
     */
    {
        int32_t width = coduo_int32_from_bits((uint32_t)cgame_syscall(
                                      CG_R_TEXT_WIDTH,
                                      (intptr_t)text,
                                      regWord,
                                      arg0,
                                      0));

        /*
         * 0x3002ed13..ed3c: centered left edge, computed as ONE 80-bit x87 chain:
         *   FLD [f_8]; FADD [f_0]; FISUB dword [width]; FSUB 0x3007bdc8 (28.0f)
         * with the ONLY rounding at 0x3002ed43 (FSTP DWORD [ESP], the call-boundary
         * store of the drawX argument). Written as one rvalue so no intermediate is
         * rounded. `width` enters via FISUB -- an INTEGER subtract, so it is NOT
         * converted to float first; the (long double) keeps it exact as x87 does.
         */
        long double drawX = ((long double)obj->w + (long double)obj->x)
                            - (long double)width
                            - (long double)CG_WEAPON_NAME_X_BIAS;

        /*
         * 0x3002ed46: emit the draw via the shared 9-arg trap-54 wrapper.
         * trap_R_Text_Paint(a0=drawX, a1=obj->y, a2=regWord, a3=arg0, a4=&params,
         *           a5=text, a6=0, a7=0, a8=arg1) -> cgame_syscall(54, a0..a8).
         * drawX and obj->y are forwarded as raw float bit patterns.
         */
        trap_R_Text_Paint(CG_FloatBits(drawX),
                  CG_FloatBits(obj->y),
                  regWord,
                  arg0,
                  (intptr_t)params,
                  (intptr_t)text,
                  0,
                  0,
                  arg1);
    }
}
