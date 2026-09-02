#include "sound_alias_private.h"
#include "compat/coduo_int32_bits.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

enum {
    SOUND_ALIAS_COMPARE_LIMIT = 99999,
    SOUND_ALIAS_INDEX_NONE = 0
};

static const float SOUND_ALIAS_STRING_SELECTOR = -1.0f;

/* CoDUOMP.exe 0x004366b0 and coduo_lnxded 0x0806c8ba implement the same
 * half-open LOD test. */
qboolean Com_ValidateSoundAliasLOD(const snd_alias_t *alias,
                                   float selector)
{
    if (alias->lodMin < 0.0f) {
        return qtrue;
    }

    if (selector < 0.0f) {
        return alias->lodMin <= 0.0f;
    }

    return selector >= alias->lodMin && selector < alias->lodMax;
}

/* CoDUOMP.exe 0x00436710 and coduo_lnxded 0x0806c93d walk the same bank hash
 * chain and return the first name/LOD match. */
snd_alias_t *Com_FindSoundAlias(const char *name,
                                sndAliasBank_t bank,
                                float selector)
{
    snd_alias_t *alias;

    if (name == NULL) {
        return NULL;
    }

    alias = com_soundAliasHash[bank][Com_HashAliasName(name)];
    while (alias != NULL) {
        if (alias->aliasName != NULL &&
            Q_stricmpn(alias->aliasName, name,
                       SOUND_ALIAS_COMPARE_LIMIT) == 0 &&
            Com_ValidateSoundAliasLOD(alias, selector)) {
            return alias;
        }
        alias = alias->hashNext;
    }

    return NULL;
}

/* CoDUOMP.exe 0x004382e0..0x004382f4 and coduo_lnxded
 * 0x0806e663..0x0806e6a2. The supporting Mac engine symbol confirms the
 * canonical name. */
const char *Com_SoundAliasString(const char *name, sndAliasBank_t bank)
{
    snd_alias_t *alias =
        Com_FindSoundAlias(name, bank, SOUND_ALIAS_STRING_SELECTOR);
    return alias != NULL ? alias->aliasName : NULL;
}

/* CoDUOMP.exe 0x00438560..0x0043857e and coduo_lnxded
 * 0x0806e850..0x0806e89c. Sound handles are one-based; zero and out-of-range
 * values are the null handle. */
snd_alias_t *Com_GetSoundAlias(int32_t soundHandle, sndAliasBank_t bank)
{
    if (soundHandle <= 0 || soundHandle > com_soundAliasCount[bank]) {
        return NULL;
    }

    return &com_soundAliases[bank][soundHandle - 1];
}

/* CoDUOMP.exe 0x00438580..0x004385ac and coduo_lnxded
 * 0x0806e89c..0x0806e8eb. The original source-level operation is one-based
 * pointer subtraction. MSVC performs signed division by the 0x4c record size;
 * Linux recovers a valid record index through dword division and the modular
 * inverse of 19. Preserve both raw i386 results before the common range check.
 * Native widened records use their actual host stride. */
int32_t Com_SoundAliasIndex(const snd_alias_t *alias, sndAliasBank_t bank)
{
#if UINTPTR_MAX == UINT32_MAX
    const uint32_t rawOffset =
        (uint32_t)(uintptr_t)alias -
        (uint32_t)(uintptr_t)com_soundAliases[bank] +
        (uint32_t)sizeof(*alias);

#if defined(WINDOWS_BEHAVIOR)
    const int32_t soundHandle =
        coduo_int32_from_bits(rawOffset) / (int32_t)sizeof(*alias);
#elif defined(LINUX_BEHAVIOR)
    enum {
        SOUND_ALIAS_STRIDE_WORD_INVERSE = UINT32_C(0x286bca1b)
    };
    const uint32_t wordOffset = coduo_int32_sar_bits(rawOffset, 2U);
    const int32_t soundHandle =
        coduo_int32_from_bits(
            wordOffset * SOUND_ALIAS_STRIDE_WORD_INVERSE);
#else
#error "Com_SoundAliasIndex requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif
#else
    if (alias == NULL) {
        return SOUND_ALIAS_INDEX_NONE;
    }

    const uintptr_t aliasAddress = (uintptr_t)alias;
    const uintptr_t tableAddress =
        (uintptr_t)com_soundAliases[bank];
    if (aliasAddress < tableAddress) {
        return SOUND_ALIAS_INDEX_NONE;
    }

    const uintptr_t byteOffset = aliasAddress - tableAddress;
    if (byteOffset % sizeof(*alias) != 0) {
        return SOUND_ALIAS_INDEX_NONE;
    }

    const uintptr_t wideHandle = byteOffset / sizeof(*alias) + 1U;
    if (wideHandle > INT32_MAX) {
        return SOUND_ALIAS_INDEX_NONE;
    }
    const int32_t soundHandle = (int32_t)wideHandle;
#endif

    if (soundHandle <= 0 || soundHandle > com_soundAliasCount[bank]) {
        return SOUND_ALIAS_INDEX_NONE;
    }

    return soundHandle;
}

/* CoDUOMP.exe 0x004385b0..0x004385b7 and coduo_lnxded
 * 0x0806e8eb..0x0806e8fa; canonical name confirmed by the supporting Mac
 * engine symbol. */
int32_t Com_SoundAliasChecksum(sndAliasBank_t bank)
{
    return (int32_t)com_soundAliasChecksum[bank];
}
