// Source: uo_cgame_mp_x86.dll 0x3003c230..0x3003c524
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c230_3003c524.mcode
//
// CG_UpdateShellShockSound — advance the sound/reverb part of one active
// shellshock.  It fades the ten sound-category levels, applies the room/wet mix,
// crossfades the loop and silent-loop aliases, and schedules the end sound.
//
// The .mcode name `CG_GetViewFov` is rejected: this routine reads the sound fields
// populated from cg_shock_sound*, registers shellshock_loop/end aliases, and drives
// sound traps 196/198/199/220/221.  The same-module PPC name
// CG_UpdateShellShockSound and the direct CG_EndShellShockSound sibling prove the
// role.

#include "../client_recovered.h"
#include "../globals.h"

void CG_UpdateShellShockSound(shellshock_t *params, int32_t elapsed, int32_t duration)
{
    int32_t modifierRemaining;
    float fade;
    float mixedVolume[SND_ALIAS_CHANNEL_COUNT];
    int32_t i;
    int32_t loopRemaining;
    int32_t soundEndTime;

    if (!params->soundEnabled) {
        CG_EndShellShockSound();
        return;
    }

    /* All four time expressions are i386 32-bit ADD/SUB chains. */
    modifierRemaining =
        (int32_t)((uint32_t)params->soundModEndDelay - (uint32_t)elapsed + (uint32_t)params->soundFadeOutTime + (uint32_t)duration);

    /* Both ramps are FILD/FIDIV (0x3003c26b/0x3003c26f and 0x3003c3c0/0x3003c3c4):
     * the numerator enters the chain straight off FILD and the denominator is an
     * integer FIDIV operand -- neither is rounded to float. Only the quotient is,
     * at the shared FSTP float [ESP+0xc] (0x3003c272). A (float) cast on either
     * operand would compile to a real fstps/flds round under -fexcess-precision=
     * standard; the (long double) denominator keeps the int exact and still makes
     * the division floating-point. */
    if (modifierRemaining < params->soundFadeOutTime) {
        fade = modifierRemaining / (long double)params->soundFadeOutTime;
        if (fade < 0.0f) {
            fade = 0.0f;
        }
    } else if (elapsed < params->soundFadeInTime) {
        fade = elapsed / (long double)params->soundFadeInTime;
        if (fade < 0.0f) {
            fade = 0.0f;
        }
    } else {
        fade = 1.0f;
    }

    for (i = 0; i < SND_ALIAS_CHANNEL_COUNT; i++) {
        mixedVolume[i] = (params->soundVolume[i] - 1.0f) * fade + 1.0f;
    }

    cgame_syscall(CG_MSS_FADE_SELECT_SOUNDS, (intptr_t)mixedVolume, 0);

    if (fade != 0.0f) {
        /* 0x3003c39f: FLD float [ESP+0x10] with one PUSH live = frame slot
         * [E-0x34] = fade (the wet level scales with the fade envelope), not
         * the modifierRemaining int at [E-0x30]. */
        cgame_syscall(CG_MSS_SET_ENVIRONMENT_EFFECTS, (intptr_t)params->soundRoomType, CG_FloatBits(fade * params->soundWetLevel), 0);
    } else {
        cgame_syscall(CG_MSS_SET_ENVIRONMENT_EFFECTS, (intptr_t)cg_genericShellshockAliasName, 0, 0);
    }

    loopRemaining =
        (int32_t)((uint32_t)params->soundLoopEndDelay - (uint32_t)elapsed + (uint32_t)duration + (uint32_t)params->soundLoopFadeTime);
    if (loopRemaining > 0) {
        snd_alias_t *loopAlias = trap_Com_PickSoundAlias(cg_shellshockLoopAliasName, vec3_origin);
        snd_alias_t *silentAlias = trap_Com_PickSoundAlias(cg_shellshockSilentLoopAliasName, vec3_origin);
        /* long double: the FIDIV/FSUBR result is clamp-compared against 0.0f
         * straight in st0 (0x3003c45d FCOM) and only rounded to float once, at
         * the call-argument store (0x3003c47a FSTP). */
        long double loopVolume;

        if (params->soundLoopFadeTime != 0) {
            /* FILD loopRemaining (0x3003c44f) / FIDIV soundLoopFadeTime
             * (0x3003c453): both integers enter the chain exact, no float store.
             * The (long double) denominator keeps them exact and defers the sole
             * round to the (float) call-argument store at 0x3003c47a. */
            loopVolume = 1.0f - loopRemaining / (long double)params->soundLoopFadeTime;
            if (loopVolume < 0.0f) {
                loopVolume = 0.0f;
            }
        } else {
            /* 0x3003c474: FLD float [ESP+0x10] with one PUSH live = frame slot
             * [E-0x34] = fade (loopRemaining is the int at [E-0x30]); the
             * zero-divisor leg reuses the fade envelope as the loop volume. */
            loopVolume = fade;
        }

        trap_MSS_PlayBlendedSoundAliases(loopAlias, silentAlias, (float)loopVolume, 1023, vec3_origin, 0);
    }

    soundEndTime = (int32_t)((uint32_t)params->soundLoopEndDelay - (uint32_t)elapsed + (uint32_t)cg_time + (uint32_t)duration);

    if ((int32_t)cg_time < soundEndTime) {
        if (cg_shellshockSoundEndTime != 0) {
            snd_alias_t *alias;
            cg_shellshockSoundEndTime = 0;
            alias = trap_Com_PickSoundAlias(cg_shellshockEndAbortAliasName, vec3_origin);
            (void)trap_MSS_PlaySoundAlias(alias, 1023, vec3_origin, 0);
        }
    } else if (soundEndTime != cg_shellshockSoundEndTime) {
        snd_alias_t *alias;
        cg_shellshockSoundEndTime = soundEndTime;
        alias = trap_Com_PickSoundAlias(cg_shellshockEndAliasName, vec3_origin);
        (void)trap_MSS_PlaySoundAlias(alias, 1023, vec3_origin, (int32_t)((uint32_t)cg_time - (uint32_t)soundEndTime));
    }
}
