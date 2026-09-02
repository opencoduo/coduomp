// Source: uo_cgame_mp_x86.dll 0x3002eec0..0x3002efb9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002eec0_3002efb9.mcode
//
// CG_DrawPlayerWeaponModeIcon — draw the local player's currently-selected weapon
// HUD icon (a 2D stretch-pic) at a caller-supplied screen rectangle, modulated
// by a caller-supplied color, and only during the brief post-switch window.
//
// Name: the mechanical header name "G_GetTankIndex" is a pure size-guess
// (win 0xf9 ~ matched 0xfa from game_mp_uo) and is REJECTED: this function does
// no server tank lookup — it reads cg.predictedPlayerState flags, resolves the
// selected weapon's cgWeaponInfo_t record, and issues cgame 2D-draw traps
// (CG_R_SETCOLOR/72 and CG_DrawPic->CG_R_DRAWSTRETCHPIC/73). It is one entry of
// the cgame HUD-element dispatcher (0x30032050), a sibling of the selected-
// weapon-NAME text draw CG_DrawPlayerWeaponName (0x3002ec10) which shares the
// exact same "resolve current weapon record" preamble. The Mac
// CG_DrawPlayerWeaponModeIcon has the same picture/fade calls and corresponding
// weapon/vehicle gates, resolving the source name.
//
// Calling convention (custom regparm, proven from the two call sites inside the
// HUD dispatcher at 0x30032465 and 0x30032482):
//   ECX  = `mode` selector (0 at 0x30032465, 1 at 0x30032482). Register arg.
//   EDI  = pointer to a caller local float[4] rectangle {x, y, w, h}. Set by the
//          caller with `LEA EDI,[ESP+n]` immediately before the CALL and read
//          here as [EDI+0..0xc]. A nonstandard register-passed pointer arg.
//   [ESP+4] (one pushed cdecl stack dword) = the draw color (a float rgba[4]*),
//          forwarded to CG_R_SETCOLOR. The caller pushes it and cleans it
//          (ADD ESP,4) after the call; this function ends with a plain RET.
// The function saves/restores ESI (i386 callee-saved) and cleans the 0x24 bytes
// it pushes for the two syscalls + CG_DrawPic itself; those are ABI details.
//
// All behavior below is proven against the .mcode instruction stream.

#include <stddef.h>  /* NULL */

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* Caller-observed register-arg ABI for CG_FadeColor / CG_DrawPic is provided by
 * their own reconstructions (client_recovered.h). CG_FadeColor takes
 * (startMsec, totalMsec) in (EDX, ECX); the calling C compiler model reuses the
 * declared source-order parameters. */

void CG_DrawPlayerWeaponModeIcon(int mode, const rectDef_t *rect, const float *color)
{
    /* 0x3002eec0 MOV EAX,[0x30483248]: cg.predictedPlayerState.entityStateFlags. */
    uint32_t flags = cg_predictedPlayerState.entityStateFlags;

    /* 0x3002eec5 TEST AH,0x60 / 0x3002eec8 JNZ ret: bits 13|14 (0x2000|0x4000)
     * are the scope/zoom FOV pair. If either is set, draw nothing. */
    if (flags & EF_ZOOM_FOV_MASK) {
        return;
    }

    /* 0x3002eece TEST ECX,ECX: the `mode` selector chooses the sense of the
     * EF_IN_VEHICLE (0x100000) gate.
     *   mode == 0 (0x3002eed2): if the flag is SET, draw nothing.
     *   mode != 0 (0x3002eedf): if the flag is CLEAR, draw nothing.
     * In both paths EAX is reduced to (flags & 0x100000) and reused below. */
    uint32_t stateFlag20 = flags & EF_IN_VEHICLE;
    if (mode == 0) {
        /* 0x3002eed2 AND EAX,0x100000 / 0x3002eed7 JNZ ret. */
        if (stateFlag20 != 0) {
            return;
        }
        /* 0x3002eedd JMP 0x3002eeea (fall through to the vehicle gate). */
    } else {
        /* 0x3002eedf AND EAX,0x100000 / 0x3002eee4 JZ ret. */
        if (stateFlag20 == 0) {
            return;
        }
    }

    /* 0x3002eeea TEST EAX,EAX / 0x3002eeec JZ 0x3002ef08: EAX is now either 0 or
     * 0x100000. Only when EF_IN_VEHICLE is set (reachable only via the
     * mode!=0 path) do the vehicle-state gates apply. */
    if (stateFlag20 != 0) {
        /* 0x3002eeee CMP [0x304837dc],0x1 / 0x3002eef5 JNZ ret. */
        if (cg_predictedPlayerState.vehicleType != 1) {
            return;
        }
        /* 0x3002eefb CMP [0x304837d8],0x3 / 0x3002ef02 JNZ ret. */
        if (cg_predictedPlayerState.vehiclePosition != 3) {
            return;
        }
    }

    /* 0x3002ef08 MOV EAX,[0x3048329c] / 0x3002ef0d TEST EAX,EAX / 0x3002ef0f JNZ:
     * cg.predictedPlayerState.currentWeapon. When it is 0 (no weapon currently
     * selected) the icon is drawn only while the post-switch fade is still live:
     * 0x3002ef11 CG_FadeColor(cg_weaponSelectTime, 1800) — EDX=start, ECX=total.
     * 0x3002ef21 TEST EAX,EAX / 0x3002ef23 JZ ret: fade expired -> nothing. */
    if (cg_predictedPlayerState.currentWeapon == 0) {
        if (CG_FadeColor(cg_weaponSelectTime, 1800) == NULL) {
            return;
        }
    }

    /* Resolve the weaponInfo_t record for the local player's selected weapon.
     * 0x3002ef29 EAX = cg_weaponSelect_vmCvar.integer (index into bg_weaponInfos). */
    weaponInfo_t *weaponInfo;
    int32_t weapon = cg_weaponSelect_vmCvar.integer;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (weapon >= 0 && weapon <= bg_numWeapons &&
        (cg_predictedPlayerState.weaponBits[(uint32_t)weapon >> 5] & (1u << ((uint32_t)weapon & 31))) != 0) {
        /* 0x3002ef55 EDX = bg_weaponInfos / 0x3002ef5b EAX = bg_weaponInfos[weapon]. */
        weaponInfo = bg_weaponInfos[weapon];
    } else {
        /* 0x3002ef60 EAX = cg_currentWeaponInfo. */
        weaponInfo = cg_currentWeaponInfo;
    }

    /* 0x3002ef65 MOV EAX,[EAX]: weaponInfo_t::weaponIndex (+0), the parallel index
     * into cg_weaponInfos[]. 0x3002ef67 TEST/JZ: index 0 draws nothing. */
    int32_t infoIndex = weaponInfo->weaponIndex;
    if (infoIndex == 0) {
        return;
    }

    /* 0x3002ef6b IMUL EAX,EAX,0x1c4 / 0x3002ef71 ADD EAX,0x30413580 / MOV ESI,EAX:
     * ESI = &cg_weaponInfos[infoIndex]. */
    cgWeaponInfo_t *info = &cg_weaponInfos[infoIndex];

    /* 0x3002ef78 MOV EAX,[ESI+0x148] / 0x3002ef7e TEST/JZ: the weapon's HUD icon
     * shader handle. A zero handle means "no icon" -> draw nothing. */
    qhandle_t hShader = (qhandle_t)info->modeIconShader;
    if (hShader == 0) {
        return;
    }

    /* 0x3002ef82 MOV EAX,[ESP+8] (the color stack arg) / PUSH EAX / PUSH 0x48 /
     * CALL [cgame_syscall]: set the 2D draw color for the following stretch-pic.
     * CG_R_SETCOLOR == 72 == 0x48. */
    cgame_syscall(CG_R_SETCOLOR, color);

    /* 0x3002ef8f MOV ECX,[ESI+0x148] (hShader again) and 0x3002ef95..0x3002efa4
     * push the rect fields, then 0x3002efa5 CALL 0x3001caa0:
     *   CG_DrawPic([EDI], [EDI+4], [EDI+8], [EDI+0xc], hShader)
     * i.e. CG_DrawPic(rect.x, rect.y, rect.w, rect.h, hShader). The rect fields
     * are read as raw dwords and reinterpreted as floats by CG_DrawPic. */
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, hShader);

    /* 0x3002efaa PUSH 0 / PUSH 0x48 / CALL [cgame_syscall]: reset the 2D draw
     * color to opaque white (NULL argument). */
    cgame_syscall(CG_R_SETCOLOR, (const float *)NULL);

    /* 0x3002efb4 ADD ESP,0x24 / 0x3002efb7 POP ESI / 0x3002efb8 RET. */
}
