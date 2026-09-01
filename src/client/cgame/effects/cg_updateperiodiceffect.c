#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30042110..0x3004215b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30042110_3004215b.mcode
//
// CG_UpdatePeriodicEffect - throttled periodic effect emitter. Once its interval
// is armed and enough client-game time has elapsed since the last emit, it
// registers a single stored effect name and replays the returned handle at a
// fixed stored world origin, then re-stamps the emit time. It is otherwise a no-op.
//
// Naming (by BEHAVIOR + CALL GRAPH, NOT size):
//   * The .mcode name `PM_GetEffectiveStance` is a pure size match (win 0x4b) and is
//     REJECTED per AGENTS.md: there is no playerstate, stance, or bg_pmove logic in
//     this body at all. The whole function is an effect-emit throttle built on two
//     cgame traps (0xe2 register, 0xe7 play-at-origin) and cg.time.
//   * CG_FxTest writes the shared state at 0x3048b004..0x3048b057; this
//     function consumes it for the register/play throttle.
//
// ABI: no arguments, no return value (bare RET, caller cleans nothing because there
//   is nothing to clean). The two syscalls are __cdecl through the trap dispatcher
//   (cgame_syscall, *0x30085e9c); the single `ADD ESP,0x14` after them balances the
//   0x14 = 5 dwords pushed for both calls (0xe2,&def + 0xe7,handle,&origin).
//
// Instruction-by-instruction proof:
//   30042110 MOV EAX,[0x3048b054]              EAX = cg_periodicEffectInterval
//   30042115 CMP EAX,0x1 / 30042118 JL 3004215a  interval < 1 -> disarmed, return
//   3004211a MOV ECX,[0x3048b050]              ECX = cg_periodicEffectLastTime
//   30042120 ADD ECX,EAX                       ECX = lastTime + interval (next-due time)
//   30042122 CMP [0x304831b0],ECX              compare cg.time, next-due
//   30042128 JLE 3004215a                      cg.time <= next-due -> not yet, return
//   3004212a PUSH 0x3048b004                   arg = cg_periodicEffectName
//   3004212f PUSH 0xe2                          command = CG_FX_REGISTER_EFFECT (register effect)
//   30042134 CALL [0x30085e9c]                 EAX = registered qhandle_t
//   3004213a PUSH 0x3048b044                   play arg: &cg_periodicEffectOrigin
//   3004213f PUSH EAX                           play arg: the handle
//   30042140 PUSH 0xe7                          command = CG_PLAY_EFFECT_ORIGIN
//   30042145 CALL [0x30085e9c]                 play the effect at the origin
//   3004214b MOV EDX,[0x304831b0]              EDX = cg.time
//   30042151 ADD ESP,0x14                       clean 5 pushed dwords (both calls)
//   30042154 MOV [0x3048b050],EDX              cg_periodicEffectLastTime = cg.time
//   3004215a RET

void CG_UpdatePeriodicEffect(void)
{
    int32_t interval = cg_periodicEffectInterval;

    /* Disarmed unless the interval is a positive millisecond count. */
    if (interval < 1) {
        return;
    }

    /* Emit only once the throttle window has fully elapsed. cg_time is stored as
     * an unsigned dword but the DLL compares it signed (JLE), so mirror that. */
    int32_t nextEffectTime = coduo_int32_from_bits(
        (uint32_t)cg_periodicEffectLastTime + (uint32_t)interval);
    if (coduo_int32_from_bits(cg_time) <= nextEffectTime) {
        return;
    }

    qhandle_t effect = (qhandle_t)cgame_syscall(
        CG_FX_REGISTER_EFFECT, (intptr_t)cg_periodicEffectName);

    cgame_syscall(CG_PLAY_EFFECT_ORIGIN,
                  (int32_t)effect,
                  (intptr_t)&cg_periodicEffectOrigin);

    cg_periodicEffectLastTime = coduo_int32_from_bits(cg_time);
}
