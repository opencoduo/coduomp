#ifndef CODUO_LNXDED_SOUND_ALIAS_RUNTIME_SERVICES_H
#define CODUO_LNXDED_SOUND_ALIAS_RUNTIME_SERVICES_H

#include "sound/alias/sound_alias.h"

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated engine has no client-audio work at
 * this bank-lifecycle boundary. */
static inline void sound_alias_compat_before_bank_activation(
    sndAliasBank_t bank)
{
    (void)bank;
}

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated engine has no client-audio work at
 * this bank-lifecycle boundary. */
static inline void sound_alias_compat_after_bank_activation(
    sndAliasBank_t bank)
{
    (void)bank;
}

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated engine has no client-audio work at
 * this bank-lifecycle boundary. */
static inline void sound_alias_compat_before_bank_deactivation(
    sndAliasBank_t bank)
{
    (void)bank;
}

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated engine has no client-audio work at
 * this bank-lifecycle boundary. */
static inline void sound_alias_compat_after_bank_deactivation(
    sndAliasBank_t bank)
{
    (void)bank;
}

/* NOT_FROM_ORIGINAL_SOURCE: target boundary for the original dedicated
 * selector, which ignores the supplied origin and always uses LOD 10. */
static inline float sound_alias_compat_pick_selector(const vec3_t origin)
{
    (void)origin;
    return 10.0f;
}

#if defined(WINDOWS_BEHAVIOR)
#define SOUND_ALIAS_COMPAT_RANDOM_SCALE 32768.0f
#else
#define SOUND_ALIAS_COMPAT_RANDOM_SCALE (-2147483648.0f)
#endif

#endif
