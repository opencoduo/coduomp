// Source: uo_cgame_mp_x86.dll 0x3002ed60..0x3002eebf
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ed60_3002eebf.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_DrawPlayerWeaponNameBack (0x3002ed60)
 *
 * The stretch-pic-plate variant of the brief on-screen "selected weapon name"
 * overlay. It is the immediate sibling of CG_DrawPlayerWeaponName
 * (0x3002ec10): same gate (player/vehicle state + the ~1800 ms post-switch fade),
 * same weaponInfo_t selection, and the same va()-formatted weapon display name.
 * Where the sibling emits the *text* through the trap-54 family, this variant
 * measures that text and draws a right-aligned background PLATE sized to it:
 * it sets the 2D draw color to the caller's RGB with the fade alpha
 * (trap_R_SetColor), draws a stretch-pic (CG_DrawPic) of width == textWidth+36
 * anchored at the right edge of a caller rect, then resets the draw color
 * (trap_R_SetColor(NULL)).
 *
 * Name adjudication: the .mcode header size-guess "script_method_player_allowcomplaint"
 * is REJECTED. That name is a server script method (game_mp_uo) with no HUD/draw
 * behavior; this function reads cgame HUD/weapon state, formats a weapon name with
 * va(), measures it via cgame trap 52, and draws a stretch-pic via CG_DrawPic
 * (trap 73) bracketed by trap_R_SetColor (trap 72). The match was a pure 0x15f
 * byte-size collision, not evidence. The Mac CG_DrawPlayerWeaponNameBack shares
 * the fade, formatting, and picture calls and draws the same measured background
 * plate, resolving the name. The engine services behind traps 52/72/73 are likewise unproven
 * (no cgame syscall-id table recovered), matching CG_R_TEXT_WIDTH/CG_R_SETCOLOR in
 * client_recovered.h.
 *
 * Register-argument ABI (custom regparm; proven from the sole caller 0x3003244b):
 *   - EAX -> EDI : `color`, a pointer to 3 dwords (RGB color vec3), copied into a
 *                  4-dword local (MOV [EDI]/[EDI+4]/[EDI+8]); the 4th slot is the
 *                  fade alpha. EDI is not otherwise spilled.
 *   - ESI       : `rect`, a rectDef_t* (same anchor object the sibling calls
 *                  `obj`): floats f_0/f_4/f_8, plus f_c which THIS variant reads as
 *                  the plate height. Incoming register argument, never written.
 *   - three cdecl stack args (pushed by the caller at 0x30032440/45/46):
 *       arg0 metricA  = [E+4]  -> trap-52 measure param (== sibling's regWord slot)
 *       arg1 metricB  = [E+8]  -> trap-52 measure param (== sibling's arg0 slot)
 *       arg2 hShader  = [E+0xc]-> the plate/background shader handle for CG_DrawPic
 *     The function ends with `ADD ESP,0x38; RET` (no immediate): the three incoming
 *     stack slots are caller-cleaned. Modeled as source-order parameters; no
 *     calling-convention attribute is added (syntax-only build does not need one).
 *
 * Control flow (proven instruction-by-instruction against the .mcode):
 *   0x3002ed66 if (cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) {
 *                 // vehicle/turret HUD view: draw only while riding as driver
 *                 if (cg_predictedPlayerState.vehicleType != 1) return;      // 0x3002ed72
 *                 if (cg_predictedPlayerState.vehiclePosition != 3) return;  // 0x3002ed7f
 *              }
 *   0x3002ed8c fade = CG_FadeColor(cg_weaponSelectTime, 1800);   // EDX=start, ECX=total
 *   0x3002ed9e if (fade == NULL) return;
 *   0x3002eda4..edc2 color4[0..2] = color[0..2]; color4[3] = fade[3]; // raw dword copies
 *   0x3002edbb..edf3 select weaponInfo_t:
 *              int w = cg_weaponSelect_vmCvar.integer;                          // 0x3044034c
 *              if (w >= 0 && w < bg_numWeapons &&
 *                  (cg_predictedPlayerState.weaponBits[w>>5] & (1u << (w&31))))     // SAR 5 / AND 0x1f
 *                  wi = bg_weaponInfos[w];
 *              else
 *                  wi = cg_currentWeaponInfo;
 *   0x3002edfb weaponIndex = wi->weaponIndex; if (weaponIndex == 0) return;
 *   0x3002ee05..ee47 name = (wi->modeName && wi->modeName[0])
 *                          ? va("%s / %s", cwi->displayName, cwi->modeName)
 *                          : va("%s", cwi->displayName);
 *              where cwi = &cg_weaponInfos[weaponIndex].
 *   0x3002ee4a width = cgame_syscall(52, name, metricA, metricB, 0); // int, FILD'd
 *   0x3002ee69 plateWidth = (float)width + 36.0f;                   // FILD; FADD 0x3007bfc0
 *   0x3002ee6d push color4; 0x3002ee72 push trap 72;
 *   0x3002ee7e plateX = (rect->w + rect->x) - plateWidth;       // FLD f_8; FADD f_0; FSUB
 *   0x3002ee8b trap_R_SetColor(color4);                         // delayed call
 *   0x3002eea8 CG_DrawPic(plateX, rect->y, plateWidth, rect->h, hShader);
 *   0x3002eeb1 trap_R_SetColor(NULL);                               // trap 72, 0
 *
 * Float precision: plateWidth and plateX are computed on the x87 stack at single
 * precision (FILD int, FADD/FSUB of single-precision float ptr, FSTP to 4-byte
 * slots) and forwarded to CG_DrawPic, which takes plain floats. rect->y and
 * rect->h are forwarded as their raw single-precision dwords.
 */

/* Plate padding added to the measured text width (.rdata 0x3007bfc0 = 0x42100000
 * == 36.0f), proven by dumping the referenced dword. FADD after FILD of the width. */
#define CG_WEAPON_NAME_PLATE_PAD 36.0f

/* CG_FadeColor lifetime for the selected-weapon-name overlay (ms). MOV ECX,0x708. */
enum { CG_WEAPON_NAME_FADE_MS = 1800 };

/* Vehicle-view gate discriminants (proven from the CMP immediates at 0x3002ed72/
 * 0x3002ed7f). Exact CoD enum names for vehicleType/vehiclePosition are unproven;
 * named by proven value, matching the sibling CG_DrawPlayerWeaponName. */
enum {
    CG_VEHICLE_TYPE_RIDING = 1,      /* cg_predictedPlayerState.vehicleType == 1 to draw */
    CG_VEHICLE_POSITION_DRIVER = 3   /* cg_predictedPlayerState.vehiclePosition == 3 to draw */
};

void CG_DrawPlayerWeaponNameBack(const vec3_t color, rectDef_t *rect,
                                         int32_t metricA, int32_t metricB,
                                         qhandle_t hShader)
{
    weaponInfo_t *wi;
    const char *name;
    float color4[4]; /* {color[0], color[1], color[2], fadeColor[3]} */
    int32_t w;

    /*
     * 0x3002ed66 TEST cg_predictedPlayerState.entityStateFlags, EF_IN_VEHICLE:
     * when set (vehicle/turret HUD view), draw only while riding in the driver
     * position; otherwise the flag path is skipped entirely (JZ 0x3002ed8c).
     */
    if (cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) {
        if (cg_predictedPlayerState.vehicleType != CG_VEHICLE_TYPE_RIDING)
            return;
        if (cg_predictedPlayerState.vehiclePosition != CG_VEHICLE_POSITION_DRIVER)
            return;
    }

    /*
     * 0x3002ed8c CG_FadeColor(cg_weaponSelectTime, 1800): NULL once the ~1800 ms
     * post-switch fade has expired (or has not started) -> draw nothing.
     */
    {
        vec_t *fade = CG_FadeColor(cg_weaponSelectTime, CG_WEAPON_NAME_FADE_MS);
        if (fade == NULL)
            return;

        /* 0x3002eda4..edc2: copy the caller RGB and the fade alpha into the 4-dword
         * local later handed to trap_R_SetColor by address. The RGB dwords are
         * copied verbatim (MOV, not FLD); the color pointer aliases 3 floats. */
        color4[0] = color[0];
        color4[1] = color[1];
        color4[2] = color[2];
        color4[3] = fade[3];
    }

    /*
     * 0x3002edbb..edf3: choose the weaponInfo_t record for the selected weapon.
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

    /* 0x3002edfb..edff: weaponIndex 0 means "no weapon" -> draw nothing. */
    {
        int32_t weaponIndex = wi->weaponIndex;
        if (weaponIndex == 0)
            return;

        /*
         * 0x3002ee05..ee47: format the weapon display name. The bg_weaponInfos
         * record's modeName (+0x78) selects the paired "%s / %s" form
         * (primary / secondary names from cg_weaponInfos[weaponIndex]) when it is a
         * non-empty string; otherwise the single "%s" primary name. The "%s / %s"
         * form pushes +0xb4 then +0xb0 (0x3002ee25/ee26), so +0xb0 is the first "%s".
         */
        {
            cgWeaponInfo_t *cwi = &cg_weaponInfos[weaponIndex];
            const char *modeReference = wi->modeName;

            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (modeReference != NULL && modeReference[0] != '\0') {
                /* 0x3002ee19 loads the translated mode label from
                 * cg_weaponInfos[weaponIndex] +0xb4. The weapon-file pointer at
                 * wi +0x78 is only the non-empty gate above. */
                name = va("%s / %s", cwi->displayName, cwi->modeName);
            } else {
                name = va("%s", cwi->displayName);
            }
        }
    }

    /*
     * 0x3002ee4a: measure the formatted name's pixel width (int32 return).
     * cgame_syscall(52, name, metricA, metricB, 0).
     */
    {
        int32_t width = coduo_int32_from_bits((uint32_t)cgame_syscall(
                                      CG_R_TEXT_WIDTH,
                                      (intptr_t)name,
                                      metricA,
                                      metricB,
                                      0));

        /* 0x3002ee69 FILD [width]; 0x3002ee74 FADD 36.0f -- width is FILDed straight
         * into the add (no float store), so it stays exact in 80-bit; no (float)
         * cast. Plate width = measured width + 36.0f padding. */
        float plateWidth = (float)((long double)width +
                                   (long double)CG_WEAPON_NAME_PLATE_PAD);

        /*
         * 0x3002ee7e..ee87 computes the right-aligned plate left edge after
         * the color trap's argument words have been pushed but before the
         * indirect call executes:
         *   plateX = (rect->w + rect->x) - plateWidth   (FLD f_8; FADD f_0; FSUB)
         * i.e. rect.right - plateWidth, so the plate hugs the rect's right edge.
         */
        {
            float plateX = (float)(((long double)rect->w +
                                    (long double)rect->x) -
                                   (long double)plateWidth);

            /* 0x3002ee8b: set the 2D draw color only after both plate
             * coordinates have been materialized. */
            cgame_syscall(CG_R_SETCOLOR, (intptr_t)color4);

            /*
             * 0x3002ee8b..eea8: draw the plate. rect->y is the y anchor, rect->h
             * the plate height, arg2 the shader handle.
             */
            CG_DrawPic(plateX, rect->y, plateWidth, rect->h, hShader);
        }

        /* 0x3002eeb1: reset the global 2D draw color (trap 72 with NULL). */
        cgame_syscall(CG_R_SETCOLOR, 0);
    }
}
