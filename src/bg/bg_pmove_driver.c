#include "bg_pmove.h"

#include "compat/coduo_int32_bits.h"

#include <stddef.h>
#include <stdint.h>

enum {
    PMOVE_SUBSTEP_MSEC_MAX = 66,
    PMOVE_MAX_TIME_WINDOW = 1000,
    PMOVE_JUMP_HELD_UPMOVE = 20
};

/*
 * Complete outer pmove driver. The Windows cgame and game bodies are
 * instruction-identical apart from relocated globals and the PmoveSingle
 * call:
 *
 *   uo_cgame_mp_x86.dll  0x3000e740..0x3000e7c5
 *   uo_game_mp_x86.dll   0x2000e4f0..0x2000e575
 *
 * Linux game exports the same interface and state machine at RVA 0x0002e1c7.
 * Its unoptimized ordering publishes pm before clearing numtouch; no callback
 * or other observation occurs between those stores.
 */
void Pmove(pmove_t *move)
{
    playerState_t *ps = move->ps;
    const int32_t finalTime = move->command.commandTime;

    if (finalTime < ps->commandTime) {
        return;
    }

    const int32_t windowEnd = coduo_int32_from_bits((uint32_t)ps->commandTime + (uint32_t)PMOVE_MAX_TIME_WINDOW);
    if (finalTime > windowEnd) {
        ps->commandTime = coduo_int32_from_bits((uint32_t)finalTime - (uint32_t)PMOVE_MAX_TIME_WINDOW);
    }

    move->numtouch = 0;
    pm = move;

    while (move->ps->commandTime != finalTime) {
        const int32_t startTime = move->ps->commandTime;
        int32_t msec = coduo_int32_from_bits((uint32_t)finalTime - (uint32_t)startTime);

        if (move->pmove_msec_min != 0) {
            if (msec > move->pmove_msec_max) {
                msec = move->pmove_msec_max;
            }
        } else if (msec > PMOVE_SUBSTEP_MSEC_MAX) {
            msec = PMOVE_SUBSTEP_MSEC_MAX;
        }

        move->command.commandTime = coduo_int32_from_bits((uint32_t)startTime + (uint32_t)msec);
        PmoveSingle(move);

        if ((move->ps->playerStateFlags & PMF_JUMP_HELD) != 0) {
            move->command.upmove = PMOVE_JUMP_HELD_UPMOVE;
        }
    }

    pm = NULL;
}
