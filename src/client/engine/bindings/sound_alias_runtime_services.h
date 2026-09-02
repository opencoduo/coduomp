#ifndef CODUOMP_SOUND_ALIAS_RUNTIME_SERVICES_H
#define CODUOMP_SOUND_ALIAS_RUNTIME_SERVICES_H

#include "qcommon/q_command.h"
#include "math/q_math.h"
#include "client/engine/sound/miles_boundary.h"
#include "sound/alias/sound_alias.h"

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for the client-only command and
 * audio work surrounding the common bank activation. */
static inline void sound_alias_compat_before_bank_activation(sndAliasBank_t bank)
{
    if ((bank == SND_ALIAS_BANK_COMMON || bank == SND_ALIAS_BANK_CGAME) && !com_soundAliasBankActive[SND_ALIAS_BANK_COMMON] &&
        !com_soundAliasBankActive[SND_ALIAS_BANK_CGAME]) {
        Cmd_AddCommand("snd_list", Com_SoundList_f);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for loading client audio after
 * the common bank state becomes active. */
static inline void sound_alias_compat_after_bank_activation(sndAliasBank_t bank)
{
    if (bank == SND_ALIAS_BANK_COMMON || bank == SND_ALIAS_BANK_CGAME) {
        Com_LoadSoundAliasSounds(bank);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for releasing client audio before
 * the common alias table is freed. */
static inline void sound_alias_compat_before_bank_deactivation(sndAliasBank_t bank)
{
    if (bank != SND_ALIAS_BANK_GAME) {
        Com_UnloadSoundAliasSounds(bank);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for removing the client command
 * after the last client-audio bank becomes inactive. */
static inline void sound_alias_compat_after_bank_deactivation(sndAliasBank_t bank)
{
    if ((bank == SND_ALIAS_BANK_COMMON || bank == SND_ALIAS_BANK_CGAME) && !com_soundAliasBankActive[SND_ALIAS_BANK_COMMON] &&
        !com_soundAliasBankActive[SND_ALIAS_BANK_CGAME]) {
        Cmd_RemoveCommand("snd_list");
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for the original client selector,
 * which uses -1 for a string-only lookup and listener distance otherwise. */
static inline float sound_alias_compat_pick_selector(const vec3_t origin)
{
    if (origin[0] == 0.0f && origin[1] == 0.0f && origin[2] == 0.0f) {
        return -1.0f;
    }

    vec3_t listenerOrigin;
    MSS_GetListener(NULL, listenerOrigin, NULL);
    return VectorDistance(listenerOrigin, origin);
}

#define SOUND_ALIAS_COMPAT_RANDOM_SCALE 32768.0f

#endif
