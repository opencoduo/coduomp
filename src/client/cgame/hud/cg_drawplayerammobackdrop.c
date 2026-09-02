#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002eb90..0x3002ec0b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002eb90_3002ec0b.mcode
//
// CG_DrawPlayerAmmoBackdrop — draw one 2D stretch-pic (`rect` = {x,y,w,h}, shader
// `hShader`) modulated by `color`, but only when the local predicted-player view
// state permits it. The draw is bracketed by trap_R_SetColor(color) / CG_DrawPic /
// trap_R_SetColor(NULL): set the 2D draw color, draw the pic, then reset the color
// to opaque white. This is a HUD/effect draw-command handler dispatched by the
// cgame command interpreter at 0x300320e0 (two call sites: 0x30032140 with
// wantVehicleView == 0, and 0x30032162 with wantVehicleView == 1).
//
// Gate (all reads from cg.predictedPlayerState, base 0x304831c4):
//   * entityStateFlags (+0x84): if bits 0x2000|0x4000 (0x6000) are set -> abort.
//   * bit 0x100000 of entityStateFlags must MATCH wantVehicleView:
//       - wantVehicleView == 0 requires the bit clear;
//       - wantVehicleView != 0 requires the bit set, and additionally the vehicle
//         state vehicleType(+0x618)==1 && vehiclePosition(+0x614)==3.
//     A mismatch aborts.
//   * currentWeapon (+0xd8) must be nonzero.
// Any failed condition returns without drawing.
//
// The .mcode's size-matched candidate "BG_SmoothWeaponSwayValue" (win 0x7b ==
// game_mp_uo 0x7b) is REJECTED by behavior: this function reads cgame predicted
// playerState globals and issues 2D-draw cgame traps (R_SetColor / DrawPic); it is
// not a weapon-sway math routine. Size is not evidence. Retail UO routes
// CG_PLAYER_AMMO_BACKDROP and CG_PLAYER_AMMO_BACKDROP_VEHICLE here; the macOS
// owner-draw jump table routes those same ids to CG_DrawPlayerAmmoBackdrop,
// establishing the exact original name.
//
// ABI (proven from the callers at 0x30032130 / 0x3003214f): `wantVehicleView`
// arrives in ECX (xor ecx,ecx / mov ecx,1 at the two call sites), `rect` arrives
// in ESI (lea esi,[esp+0x14] -> a local float[4]), and `color` then `hShader` are
// the two cdecl stack arguments (push eax=color; push edx=hShader; add esp,8 after
// return). Register-argument inputs are written here as ordinary parameters, per
// the codebase convention for MSVC register-passed args.
void CG_DrawPlayerAmmoBackdrop(int32_t wantVehicleView, const rectDef_t *rect, const float *color, qhandle_t hShader)
{
    // 0x3002eb90 MOV EAX,[0x30483248]; 0x3002eb95 TEST AH,0x60; 0x3002eb98 JNZ ret.
    // AH holds bits 8..15 of entityStateFlags, so AH&0x60 tests flags&0x6000.
    uint32_t flags = cg_predictedPlayerState.entityStateFlags;
    if ((flags & 0x6000u) != 0u) {
        return;
    }

    // 0x3002eb9a TEST ECX,ECX / JNZ 0xeba6, then both paths AND EAX,0x100000.
    // ecx==0 reaches the state block only when the 0x100000 bit is CLEAR (else RET);
    // ecx!=0 reaches it only when the bit is SET (else RET). The MSVC-emitted flow is
    // exactly: proceed iff (wantVehicleView != 0) == (bit 0x100000 set).
    uint32_t inVehicleViewBit = flags & 0x100000u;
    if (wantVehicleView == 0) {
        // 0x3002eb9e AND EAX,0x100000; 0x3002eba3 JZ 0xebad (proceed); else 0xeba5 RET.
        if (inVehicleViewBit != 0u) {
            return;
        }
        // Falls through to the currentWeapon gate with no vehicle-state check.
    } else {
        // 0x3002eba6 AND EAX,0x100000; 0x3002ebab JZ 0xeba5 RET; else fall to 0xebad.
        if (inVehicleViewBit == 0u) {
            return;
        }
        // 0x3002ebad TEST EAX,EAX / JZ 0xebc3: EAX==0x100000 here (nonzero), so the
        // vehicle-state checks below run.
        // 0x3002ebb1 CMP [0x304837dc],1 / JNZ ret: vehicleType must be 1.
        if (cg_predictedPlayerState.vehicleType != 1) {
            return;
        }
        // 0x3002ebba CMP [0x304837d8],3 / JNZ ret: vehiclePosition must be 3.
        if (cg_predictedPlayerState.vehiclePosition != 3) {
            return;
        }
    }

    // 0x3002ebc3 MOV EAX,[0x3048329c]; 0x3002ebc8 TEST EAX,EAX; 0x3002ebca JZ ret.
    if (cg_predictedPlayerState.currentWeapon == 0) {
        return;
    }

    // 0x3002ebcc..0x3002ebd3 PUSH color; PUSH 0x48; CALL [cgame_syscall]:
    //   trap(72, color) -> set the 2D draw color modulation. Return value ignored.
    trap_R_SetColor(color);

    // 0x3002ebd9..0x3002ebed: push (rect[0],rect[1],rect[2],rect[3],hShader) and
    // CALL 0x3001caa0 (CG_DrawPic). The rect words are moved as raw dwords and read
    // as floats inside CG_DrawPic (FLD), i.e. rect is a float[4] = {x,y,w,h}.
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, hShader);

    // 0x3002ebf5..0x3002ec05 MOV [ESP+8],0; MOV [ESP+4],0x48; JMP [cgame_syscall]:
    //   the two arg slots are overwritten in place to (72, 0) and the vector is
    //   tail-jumped, i.e. trap(72, NULL) -> reset the draw color to opaque white.
    trap_R_SetColor(NULL);
}
