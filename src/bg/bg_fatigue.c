#include "bg_pmove.h"

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"

#include <stdint.h>

enum {
    PM_FATIGUE_SOUND_INTERVAL = 1700,
    PM_FATIGUE_REGEN_DELAY = 1000
};

/* The Linux game module exports these immutable values from .rodata at
 * 0x0009b4e8..0x0009b4f0.  The optimized Windows cgame/game bodies fold the
 * same binary32 values directly into PM_UpdateFatigue. */
const float pm_sprintFatigue = 0.33333334f;
const float pm_fatigueRegen = 0.14285715f;
const int32_t pm_fatigueRegenDelay = PM_FATIGUE_REGEN_DELAY;

/*
 * The authoritative Windows cgame/game bodies are instruction-identical apart
 * from relocated globals:
 *
 *   uo_cgame_mp_x86.dll  PM_PlayFatigueSound 0x3000c3a0
 *   uo_game_mp_x86.dll   PM_PlayFatigueSound 0x2000c160
 *   uo_cgame_mp_x86.dll  PM_UpdateFatigue    0x3000c420
 *   uo_game_mp_x86.dll   PM_UpdateFatigue    0x2000c1e0
 *
 * Linux retains the same state machine at RVAs 0x0002a968 and 0x0002aa22.
 * Its unoptimized body loads the named constants above rather than folding
 * them, and calls PM_AddEvent where the Windows compiler inlines that leaf.
 */

void PM_PlayFatigueSound(void)
{
    playerState_t *const ps = pm->ps;
    const int32_t nextSoundTime = coduo_int32_from_bits(
        (uint32_t)ps->fatigueSoundTime + PM_FATIGUE_SOUND_INTERVAL);

    if ((ps->playerStateFlags & PMF_FATIGUED) != 0) {
        if (nextSoundTime < pm->command.commandTime) {
            PM_AddEvent(EV_FATIGUE_LAST_SOUND);
            pm->ps->fatigueSoundTime = pm->command.commandTime;
        }
    } else if (ps->fatigueSoundTime > 0 &&
               nextSoundTime < pm->command.commandTime) {
        ps->fatigueSoundTime = 0;
    }
}

void PM_UpdateFatigue(void)
{
    playerState_t *const ps = pm->ps;

    if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
        if (ps->pmType == PM_TYPE_NOCLIP || bg_nofatigue.integer != 0) {
            return;
        }

        ps->lastSprintTime = pm->command.commandTime;
#if EMULATE_X87
        ps->fatigueScale = x87f_store_f32(x87f_sub(
            x87f_load_f32(ps->fatigueScale),
            x87f_mul(
                x87f_mul(x87f_load_i32(pml.msec),
                         x87f_load_f32(0.001f)),
                x87f_load_f32(pm_sprintFatigue))));
#else
        ps->fatigueScale = (float)(
            (long double)ps->fatigueScale -
            (long double)pml.msec * (long double)0.001f *
                (long double)pm_sprintFatigue);
#endif

        if (ps->fatigueScale < 0.5f) {
            PM_AddEvent(EV_FATIGUE_LAST_SOUND);
            pm->ps->playerStateFlags |= PMF_FATIGUED;
        }
        if (pm->ps->fatigueScale < 0.0f) {
            pm->ps->fatigueScale = 0.0f;
        }
        return;
    }

    if ((pm->command.buttons & PM_BUTTON_SPRINT) == 0) {
        const int32_t regenTime = coduo_int32_from_bits(
            (uint32_t)ps->lastSprintTime +
            (uint32_t)pm_fatigueRegenDelay);

        if (pm->command.commandTime < regenTime) {
            return;
        }

#if EMULATE_X87
        ps->fatigueScale = x87f_store_f32(x87f_add(
            x87f_load_f32(ps->fatigueScale),
            x87f_mul(
                x87f_mul(x87f_load_i32(pml.msec),
                         x87f_load_f32(0.001f)),
                x87f_load_f32(pm_fatigueRegen))));
#else
        ps->fatigueScale = (float)(
            (long double)ps->fatigueScale +
            (long double)pml.msec * (long double)0.001f *
                (long double)pm_fatigueRegen);
#endif

        if (ps->fatigueScale >= 1.0f) {
            ps->fatigueScale = 1.0f;
            ps->playerStateFlags &= ~(uint32_t)PMF_FATIGUED;
        }
    }

    PM_PlayFatigueSound();
}
