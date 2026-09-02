// Source: uo_cgame_mp_x86.dll 0x30030c60..0x30030f10
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30030c60_30030f10.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_DrawPlayerAmmoValue (0x30030c60) — HUD ammo-count element for the current
 * weapon. Draws the magazine (clip) count and the remaining reserve count inside
 * a rect: for a normal magazine weapon the clip count is drawn left-aligned and
 * the reserve count right-aligned within the rect; for a clip-only weapon (or a
 * weapon whose clip count is unavailable) only the single available count is
 * drawn horizontally centered. Both numbers are clamped to the [0,999] display
 * range and produced through the ring-buffer formatter va().
 *
 * The .mcode header's mechanical name "SpectatorThink" is REJECTED: it is a pure
 * size-match guess (win size 0x2b0 ~ 0x2b1) and is behaviorally wrong. This
 * routine performs no player-movement/think logic; it reads HUD/predicted-player
 * gates, computes ammo counts, and issues the 2D draw traps trap_R_Text_Paint/trap_R_Text_Width.
 * Retail UO routes CG_PLAYER_AMMO_VALUE, CG_PLAYER_AMMOCLIP_VALUE, and
 * CG_PLAYER_AMMO_VALUE_VEHICLE to this handler. The macOS owner-draw jump table
 * routes those same three ids to CG_DrawPlayerAmmoValue, establishing the exact
 * original function name.
 *
 * Non-default register ABI (proven from both callers at 0x3003218a / 0x300321b5):
 *   ECX = viewMode          (0 or 1; selects which entityStateFlags gate applies)
 *   ESI = &rect             (rectDef_t*, only x/y/w consumed)
 *   EBX = drawFontOrScale   (pass-through draw parameter)
 * plus three cdecl stack args (caller cleans 0xc = three dwords):
 *   arg0 (EDX slot) = drawStyleA   (pass-through draw parameter)
 *   arg1 (ECX slot) = drawStyleB   (pass-through draw parameter)
 *   arg2 (EAX slot) = drawColor    (pass-through draw parameter)
 * The four EBX/style/color values are forwarded verbatim into the draw traps;
 * their exact draw-parameter meaning (font handle, scale, style flags, color) is
 * not individually proven here, so they carry role-shaped pass-through names.
 * EBP save/restore, EDI save/restore, and the RET are i386 conventions.
 *
 * Prologue: EBP is zeroed (XOR EBP,EBP) and used as the default reserve/ammo
 * string pointer (NULL) so an un-drawn count is simply a null string.
 */

/* Value clamp used for both counts: the display never shows more than 999. */
enum {
    CG_AMMO_COUNT_DISPLAY_MAX = 999
}; /* 0x3e7 */

void CG_DrawPlayerAmmoValue(int32_t viewMode /*ECX*/, const rectDef_t *rect /*ESI*/, int32_t font /*EBX*/, int32_t scaleBits /*stack arg0*/,
                            const vec4_t color /*stack arg1*/, int32_t textStyle /*stack arg2*/)
{
    /* EAX at entry: cg.predictedPlayerState.entityStateFlags (0x30483248). */
    uint32_t entityFlags = cg_predictedPlayerState.entityStateFlags;

    /* 0x30030c6b TEST AH,0x60 / JNZ: any scope/zoom-FOV bit set suppresses the
     * whole HUD ammo element. */
    if ((entityFlags & EF_ZOOM_FOV_MASK) != 0) {
        return;
    }

    /* 0x30030c78..0x30030c8e: the EF_IN_VEHICLE (0x100000) gate flips with
     * viewMode. viewMode==0 requires the flag CLEAR; viewMode!=0 requires it SET. */
    uint32_t flag20 = entityFlags & EF_IN_VEHICLE;
    if (viewMode == 0) {
        if (flag20 != 0) {
            return;
        }
        /* JMP 0x30030c94 with flag20 == 0 */
    } else {
        if (flag20 == 0) {
            return;
        }
    }

    /* 0x30030c94 CMP EAX,0 / JZ 0x30030cb6: when flag20 is set (only reachable via
     * viewMode!=0 here) the element additionally requires a specific vehicle state:
     * vehicleType == 1 and vehiclePosition == 3. When flag20 is clear (viewMode==0)
     * this block is skipped. */
    if (flag20 != 0) {
        if (cg_predictedPlayerState.vehicleType != 1) {
            return;
        }
        if (cg_predictedPlayerState.vehiclePosition != 3) {
            return;
        }
    }

    /* 0x30030cb6: require a current weapon. */
    if (cg_predictedPlayerState.currentWeapon == 0) {
        return;
    }

    /* 0x30030cc2..0x30030cff: resolve which weapon index to display ammo for.
     * Default is the followed/local snapshot entity's weaponIndex (the
     * centity_t at cg_entities[cg_snap->ps.psClientNum]); but for the active local
     * player, use a valid local weapon selection instead. */
    const snapshot_t *snap = cg_snap;

    int32_t weapon;
    {
        /* cg_entities base 0x3048c6e0, stride 0x288 (centity_t), indexed by
         * snapshot clientNum (0x30030cc8: [cg_snap+0xe0], IMUL 0x288, ADD base). */
        const centity_t *ent = cg_entities + snap->ps.psClientNum;
        uint32_t psFlags = snap->ps.playerStateFlags; /* [cg_snap+0x18] */

        weapon = ent->currentState.weapon; /* default: [ent+0xcc] */
        if ((psFlags & PSF_ACTIVE_PLAYER) != 0) {
            int32_t localSel = cg_weaponSelect_vmCvar.integer; /* 0x3044034c */
            if (localSel >= 0 && localSel < bg_numWeapons) {
                weapon = localSel;
            }
        }
    }

    if (weapon == 0) {
        return; /* 0x30030cff JZ: no weapon to draw */
    }

    /* 0x30030d0c/0x30030d10: both count-present flags default to shown (1). */
    int clipShown = 1; /* S-0xc */
    int ammoShown = 1; /* S-0x4 */

    /* 0x30030d14: total reserve+clip ammo usable through this weapon. */
    int32_t ammoValue = BG_GetTotalAmmoReserve(&cg_predictedPlayerState, weapon);

    weaponInfo_t *wi = bg_weaponInfos[weapon]; /* [0x30134cd8 + weapon*4] */

    /* 0x30030d22..0x30030d47: derive the clip (magazine) count. A clip-only weapon
     * (weaponInfo_t.clipRequired != 0) has no separate reserve display, so its clip
     * value is forced to the "unavailable" sentinel -1; otherwise the clip count
     * is the predicted clips[] entry for this weapon's clipIndex. A negative clip
     * count clears the clip-shown flag. */
    int32_t clipValue;
    if (wi->clipRequired != 0) {
        clipValue = -1;
        clipShown = 0;
    } else {
        clipValue = cg_predictedPlayerState.clips[wi->clipIndex]; /* [clipIndex*4 + 0x304834f8] */
        if (clipValue < 0) {
            clipShown = 0;
        }
    }

    /* 0x30030d51: clamp the clip count to the display maximum. */
    if (clipValue > CG_AMMO_COUNT_DISPLAY_MAX) {
        clipValue = CG_AMMO_COUNT_DISPLAY_MAX;
    }

    /* 0x30030d5e: a negative total ammo count clears the ammo-shown flag. */
    if (ammoValue < 0) {
        ammoShown = 0;
    }

    /* 0x30030d6a: clamp the total ammo count to the display maximum. */
    if (ammoValue > CG_AMMO_COUNT_DISPLAY_MAX) {
        ammoValue = CG_AMMO_COUNT_DISPLAY_MAX;
    }

    /* 0x30030d7d..0x30030dad: format the two counts into ring-buffer strings. The
     * clip string is a right-justified two-column "%2i"; the reserve/total string
     * is a plain "%i". EBP (reserveStr) is NULL unless the ammo count is shown. */
    const char *clipStr = 0;   /* S-0x8 */
    const char *reserveStr = 0; /* EBP, initialized NULL at entry */

    if (clipShown) {
        clipStr = va("%2i", clipValue);
    }
    if (ammoShown) {
        reserveStr = va("%i", ammoValue);
    }

    /* 0x30030db3..: three draw layouts.
     *   clipShown && ammoShown : clip left-aligned, reserve right-aligned
     *   clipShown && !ammoShown : clip only, centered
     *   !clipShown && ammoShown : reserve only, centered
     *   !clipShown && !ammoShown: nothing
     */
    if (clipShown) {
        if (ammoShown) {
            /* 0x30030dc7 block: left clip + centered "|" separator + right reserve. */

            /* Left-aligned clip string drawn at (rect.x, rect.y). */
            trap_R_Text_Paint(CG_FloatBits(rect->x), CG_FloatBits(rect->y), font, scaleBits, (intptr_t)color, (intptr_t)clipStr, 0, 0,
                              textStyle);

            /* Reserve string width, then right-align it inside the rect:
             * x = rect.w + rect.x - reserveWidth. */
            /* 0x30030de8..0x30030e04 forms and retains the right edge in
             * ST0 across the width callback, then subtracts the returned
             * integer without an intervening binary32 store. */
            long double rightEdge = (long double)rect->w + (long double)rect->x;
            int32_t reserveWidth = trap_R_Text_Width(reserveStr, font, scaleBits, 0);
            float rightX = (float)(rightEdge - (long double)reserveWidth);

            trap_R_Text_Paint(CG_FloatBits(rightX), CG_FloatBits(rect->y), font, scaleBits, (intptr_t)color, (intptr_t)reserveStr, 0, 0,
                              textStyle);

            /* 0x30030e25..0x30030e69: draw the "|" clip/reserve separator centered.
             * After trap_R_Text_Width(reserveStr), 0x30030e0c reloads the caller's
             * color pointer into EBP from the expanded stack.  The separator draw
             * therefore receives the same color as both number draws; EBP no longer
             * holds reserveStr at 0x30030e4f. */
            int32_t separatorWidth = trap_R_Text_Width(cg_ammoCountSeparator, font, scaleBits, 0);
            /* 0x30030e3b FILD separatorWidth; FSUBR rect->w -- fed straight into the
             * subtract with no FSTP DWORD, so no (float) cast. */
            float separatorX = (float)((long double)rect->x + ((long double)rect->w - (long double)separatorWidth) * 0.5L);

            trap_R_Text_Paint(CG_FloatBits(separatorX), CG_FloatBits(rect->y), font, scaleBits, (intptr_t)color,
                              (intptr_t)cg_ammoCountSeparator, 0, 0, textStyle);
        } else {
            /* 0x30030e72 block: clip only, horizontally centered. */
            int32_t clipWidth = trap_R_Text_Width(clipStr, font, scaleBits, 0);
            /* 0x30030e8b FILD clipWidth; FSUBR rect->w (no FSTP DWORD) -> no cast. */
            float centerX = (float)((long double)rect->x + ((long double)rect->w - (long double)clipWidth) * 0.5L);

            trap_R_Text_Paint(CG_FloatBits(centerX), CG_FloatBits(rect->y), font, scaleBits, (intptr_t)color, (intptr_t)clipStr, 0, 0,
                              textStyle);
        }
    } else if (ammoShown) {
        /* 0x30030ebf block: reserve/total only, horizontally centered. */
        int32_t reserveWidth = trap_R_Text_Width(reserveStr, font, scaleBits, 0);
        /* 0x30030edc FILD reserveWidth; FSUBR rect->w (no FSTP DWORD) -> no cast. */
        float centerX = (float)((long double)rect->x + ((long double)rect->w - (long double)reserveWidth) * 0.5L);

        trap_R_Text_Paint(CG_FloatBits(centerX), CG_FloatBits(rect->y), font, scaleBits, (intptr_t)color, (intptr_t)reserveStr, 0, 0,
                          textStyle);
    }
}
