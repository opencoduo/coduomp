// Source: uo_cgame_mp_x86.dll 0x30044c10..0x30044cbe
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30044c10_30044cbe.mcode
//
// CG_WeaponSway_ApplyShellShock — the cgame-side weapon-sway entry point.
//
// NOTE on the assigned mechanical name: the .mcode header carries the size-guess
// "PlayerCmd_isInVehicle" (matched purely on byte size 0xae). That is REJECTED:
// this is not a predicate and returns nothing. The body reads the shell-shock
// sway time window and the active weapon's swayShellShockScale, eases a smoothstep
// envelope, and calls the weapon-sway core BG_CalculateWeaponPosition_Sway
// (0x30015ca0). The mechanical global owner label bg_smoothweaponswayvalue (on
// cg_predictedPlayerState.currentWeapon, first-touched here) and the same-module PPC
// BG_CalculateWeaponPosition_Sway family corroborate the weapon-sway identity.
// Name provisional by proven role; exact original source name unresolved.
//
// The smoothstep envelope, matching the machine code exactly:
//   remaining = cg_shellShockSwayStartTime + cg_shellShockSwayDuration - cg.time
//   if (remaining <= 0)             scale = 1.0f;                 // 0x30044c88
//   else {
//       duration = cg_shellShockSwayParams->duration;            // *(int*)0x3048bfdc
//       x = (remaining >= duration) ? 1.0f                       // JGE keeps FLD 1.0f
//                                   : remaining / duration;       // FILD/FIDIV (exact ints)
//       s = x * x * (3.0f - 2.0f * x);                           // smoothstep
//       scale = 1.0f + s * (weapon->swayShellShockScale - 1.0f);
//   x and s are carried in st0 (never stored), so they are long double locals; the
//   only float rounding is the FSTP DWORD store of `scale` at 0x30044c80.
//   }
// then BG_CalculateWeaponPosition_Sway(swayOut, scale, cg.frametime).
//
// The float constants are read from .rdata (exact addresses dumped, not inferred):
//   1.0f at 0x3007bce0, 3.0f at 0x3007be5c. The 2.0f*x term is the FADD ST0,ST1
//   idiom (x + x), so no separate 2.0f constant is loaded.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

/*
 * The weapon-sway core writes three fixed cgame output vectors, passed here as its
 * state/output pointers (EBP = 0x30487ad4 previous view angles, EDI =
 * 0x30487ae0 sway angles, EAX = 0x30487aec positional sway) alongside
 * ESI = &cg.predictedPlayerState (0x304831c4). Those three vectors
 * are still exported as the mechanical per-dword symbols g_data_cg_addscalefade_*:
 * resolving them to a single named typed object requires reconstructing both the
 * sway core (0x30015ca0) and its consumers. The previous-view vector at
 * 0x30487ad4 is the first pushed stack argument; the other two arrive in
 * registers (documented on BG_CalculateWeaponPosition_Sway's declaration).
 */

void CG_WeaponSway_ApplyShellShock(void)
{
    /* bg_weaponInfos[cg.predictedPlayerState.currentWeapon] (0x30044c13..0x30044c1e):
     * EAX = currentWeapon; ECX = bg_weaponInfos; EDX = bg_weaponInfos[EAX]. */
    const weaponInfo_t *weapon =
        bg_weaponInfos[cg_predictedPlayerState.currentWeapon];

    /* remaining = (duration - cg.time) + startTime, evaluated as signed 32-bit
     * (0x30044c21..0x30044c34: MOV EAX,[end]; SUB EAX,[cg.time]; ADD EAX,ESI[=start];
     * TEST EAX,EAX; JLE is a SIGNED test). cg_time is stored unsigned, so cast it to
     * int32_t to keep the subtraction and the <= 0 branch signed like the ALU op. */
    int32_t remaining = coduo_int32_from_bits(
        ((uint32_t)cg_shellShockSwayDuration - (uint32_t)cg_time) +
        (uint32_t)cg_shellShockSwayStartTime);

    float scale;

    if (remaining <= 0) {
        /* 0x30044c88: MOV [ESP+8],0x3f800000 -> 1.0f. */
        scale = 1.0f;
    } else {
        /* duration = *cg_shellShockSwayParams (0x30044c3e MOV ECX,[params];
         * 0x30044c4a MOV ECX,[ECX]). Treated as a signed count for the CMP/FIDIV. */
        int32_t duration = cg_shellShockSwayParams->blurDivisor;

        /* x: FLD 1.0f is pre-loaded (0x30044c44); the JGE at 0x30044c52 KEEPS that
         * 1.0f when remaining >= duration (envelope saturated). Only when
         * remaining < duration is the 1.0f discarded (FSTP ST0) and replaced by
         * remaining / duration (FILD [remaining] / FIDIV [duration]). The FILD
         * feeds FIDIV with NO intervening float store, so the integers enter the
         * divide exactly (long double casts, not (float) -- a (float) cast would
         * insert a rounding the bare FILD does not). x then lives in st0 and is
         * NEVER stored to a float slot: it is carried 80-bit through the smoothstep,
         * so it must be long double, not float, or it would round here. 1.0f is the
         * .rdata constant at 0x3007bce0. */
        long double x = (remaining >= duration)
                      ? 1.0f
                      : (long double)remaining / (long double)duration;

        /* smoothstep s = x*x*(3 - 2x) (0x30044c5e..0x30044c6a):
         *   FLD ST0 ; FADD ST0,ST1 (= 2x) ; FSUBR 3.0f (= 3 - 2x) ;
         *   FMUL ST1 (* x) ; FMUL ST1 (* x). 3.0f from .rdata 0x3007be5c. s is also
         * kept in st0 (no store), so it is long double: the ONLY rounding in the
         * whole else-branch is the FSTP DWORD [ESP+0x8] of `scale` at 0x30044c80. */
        long double s = x * x * (3.0f - (x + x));

        /* scale = 1 + s * (swayShellShockScale - 1) (0x30044c6c..0x30044c7a):
         *   FLD [weapon+0x2fc] ; FSUB 1.0f ; FMULP (* s) ; FADD 1.0f ; FSTP DWORD.
         * The store to `float scale` is that single rounding (0x30044c80).
         * 1.0f from .rdata 0x3007bce0. */
        scale = 1.0f + s * (weapon->swayShellShockScale - 1.0f);
    }

    /* CALL 0x30015ca0 (0x30044c90..0x30044cb0). Stack args pushed right-to-left:
     * cg.frametime (0x304831ac), scale, &previousViewAngles (0x30487ad4).
     * Register args set immediately before the call: EDI = &swayAngles
     * (0x30487ae0), EAX = &swayOffset (0x30487aec),
     * ESI = &cg.predictedPlayerState (0x304831c4).
     * Caller cleans the 3 pushed dwords (ADD ESP,0xc at 0x30044cb5). */
    BG_CalculateWeaponPosition_Sway(
        &cg_predictedPlayerState,
        cg_weaponSwayViewAngles,                   /* stack: previous view angles */
        cg_weaponSwayOffset,                       /* EAX: positional sway */
        cg_weaponSwayAngles,                       /* EDI: sway angles */
        scale, cg_frametime);
}
