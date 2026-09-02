#include "bg_pmove.h"

#include "compat/coduo_int32_bits.h"

/*
 * The Windows cgame/game PM_DropTimers bodies are instruction-identical apart
 * from their pm/pml addresses (0x3000c320 and 0x2000c0e0).  Linux game RVA
 * 0x0002a829 retains the same signed expiry tests, PMF_ALL_TIMES mask, wrapping
 * subtractions, and zero clamps.
 */
void PM_DropTimers(void)
{
    if (pm->ps->pmTime != 0) {
        if (pml.msec < pm->ps->pmTime) {
            pm->ps->pmTime = coduo_int32_from_bits(
                (uint32_t)pm->ps->pmTime - (uint32_t)pml.msec);
        } else {
            pm->ps->playerStateFlags &= ~(uint32_t)PMF_ALL_TIMES;
            pm->ps->pmTime = 0;
        }
    }

    if (pm->ps->legsTimer > 0) {
        pm->ps->legsTimer = coduo_int32_from_bits(
            (uint32_t)pm->ps->legsTimer - (uint32_t)pml.msec);
        if (pm->ps->legsTimer < 0) {
            pm->ps->legsTimer = 0;
        }
    }

    if (pm->ps->torsoTimer > 0) {
        pm->ps->torsoTimer = coduo_int32_from_bits(
            (uint32_t)pm->ps->torsoTimer - (uint32_t)pml.msec);
        if (pm->ps->torsoTimer < 0) {
            pm->ps->torsoTimer = 0;
        }
    }
}
