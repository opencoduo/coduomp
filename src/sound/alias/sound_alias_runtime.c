#include "sound_alias_private.h"

#include "sound_alias_runtime_services.h"

#include <string.h>

enum {
    SOUND_ALIAS_MAP_PREFIX_LENGTH = 5,
    SOUND_ALIAS_MAP_PATH_SKIP = 8,
    SOUND_ALIAS_BSP_EXTENSION_LENGTH = 4,
    SOUND_ALIAS_LOADSPEC_PREFIX_LENGTH = sizeof("soundloadspecs/mp/") - 1,
    SOUND_ALIAS_LOADSPEC_EXTENSION_LENGTH = sizeof(".csv") - 1,
    SOUND_ALIAS_LOADSPEC_NAME_CAPACITY = SND_ALIAS_SOURCE_NAME_CAPACITY -
        SOUND_ALIAS_LOADSPEC_PREFIX_LENGTH -
        SOUND_ALIAS_LOADSPEC_EXTENSION_LENGTH - 1
};

/*
 * Complete common alias-bank load core.  The authoritative bodies agree on
 * source normalization, list ownership, temporary-hunk lifetime, table
 * construction, bank sharing, and activation:
 *
 *   CoDUOMP.exe   0x00437f40..0x0043823e
 *   coduo_lnxded  0x0806e35a..0x0806e5c1
 *
 * The Windows client additionally registers snd_list and loads client audio;
 * those operations remain in its target service boundary around the common
 * activation store.
 */
void Com_LoadSoundAliases(const char *sourceName, sndAliasBank_t bank)
{
    const size_t sourceNameLength = strlen(sourceName);

    /* NOT_FROM_ORIGINAL_SOURCE: the complete source identity and NUL must fit
     * both the local and persistent alias destinations. */
    if (sourceNameLength >= SND_ALIAS_SOURCE_NAME_CAPACITY) {
        Com_Printf("WARNING: sound alias source name exceeds %i bytes\n", SND_ALIAS_SOURCE_NAME_CAPACITY - 1);
        return;
    }

    if (bank == SND_ALIAS_BANK_CGAME &&
        Q_stricmp(com_soundAliasLocalizedSource, sourceName) == 0) {
        com_soundAliases[SND_ALIAS_BANK_CGAME] =
            com_soundAliases[SND_ALIAS_BANK_GAME];
        com_soundAliasCount[SND_ALIAS_BANK_CGAME] =
            com_soundAliasCount[SND_ALIAS_BANK_GAME];
        memcpy(com_soundAliasHash[SND_ALIAS_BANK_CGAME],
               com_soundAliasHash[SND_ALIAS_BANK_GAME],
               sizeof(com_soundAliasHash[SND_ALIAS_BANK_CGAME]));
    } else {
        char normalizedSource[SND_ALIAS_SOURCE_NAME_CAPACITY];
        size_t normalizedLength;

        if (Q_stricmpn(sourceName, "maps/",
                       SOUND_ALIAS_MAP_PREFIX_LENGTH) == 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: form the map-relative offset only when
             * it belongs to the source, and test a suffix only when complete. */
            if (sourceNameLength < SOUND_ALIAS_MAP_PATH_SKIP) {
                Com_Printf("WARNING: sound alias map source is too short\n");
                return;
            }
            normalizedLength = sourceNameLength - SOUND_ALIAS_MAP_PATH_SKIP;
            memcpy(normalizedSource, sourceName + SOUND_ALIAS_MAP_PATH_SKIP, normalizedLength + 1);

            if (normalizedLength >= SOUND_ALIAS_BSP_EXTENSION_LENGTH &&
                Q_stricmp(normalizedSource + normalizedLength - SOUND_ALIAS_BSP_EXTENSION_LENGTH, ".bsp") == 0) {
                normalizedLength -= SOUND_ALIAS_BSP_EXTENSION_LENGTH;
                normalizedSource[normalizedLength] = '\0';
            }
        } else {
            normalizedLength = sourceNameLength;
            memcpy(normalizedSource, sourceName, normalizedLength + 1);
        }
        Q_strlwr(normalizedSource);

        char **aliasFiles;
        int32_t fileCount;

        if (bank == SND_ALIAS_BANK_CGAME ||
            bank == SND_ALIAS_BANK_GAME) {
            char loadSpecPath[SND_ALIAS_SOURCE_NAME_CAPACITY];

            /* NOT_FROM_ORIGINAL_SOURCE: require the complete loadspec path and
             * NUL to fit; never substitute a truncated filename. */
            if (normalizedLength > SOUND_ALIAS_LOADSPEC_NAME_CAPACITY) {
                Com_Printf("WARNING: sound alias loadspec name exceeds %i bytes\n", SOUND_ALIAS_LOADSPEC_NAME_CAPACITY);
                return;
            }
            memcpy(loadSpecPath, "soundloadspecs/mp/", SOUND_ALIAS_LOADSPEC_PREFIX_LENGTH);
            memcpy(loadSpecPath + SOUND_ALIAS_LOADSPEC_PREFIX_LENGTH, normalizedSource, normalizedLength);
            memcpy(loadSpecPath + SOUND_ALIAS_LOADSPEC_PREFIX_LENGTH + normalizedLength, ".csv", SOUND_ALIAS_LOADSPEC_EXTENSION_LENGTH + 1);
            aliasFiles = Com_ParseLoadSpecFile(loadSpecPath, &fileCount);
        } else {
            aliasFiles =
                FS_ListFiles("soundaliases", "csv", &fileCount);
        }

        if (fileCount == 0) {
            Com_Printf(
                "WARNING: can't find any sound alias files "
                "(soundaliases/*.csv)\n");
            return;
        }

        Hunk_SetMarkTemp();
        int32_t aliasCount = 0;
        for (int32_t fileIndex = 0;
             fileIndex < fileCount; ++fileIndex) {
            com_soundAliasCurrentFile = aliasFiles[fileIndex];
            aliasCount = Com_LoadSoundAliasFile(
                va("soundaliases/%s", com_soundAliasCurrentFile),
                normalizedSource, aliasCount, (qboolean)bank);
        }

        if (aliasCount != 0) {
            Com_MakeSoundAliasesPermanent(aliasCount, bank);
        }
        Hunk_ClearToMarkTemp();

        if (bank == SND_ALIAS_BANK_CGAME ||
            bank == SND_ALIAS_BANK_GAME) {
            Com_FreeFileList(aliasFiles);
        } else {
            FS_FreeFileList(aliasFiles);
        }

        if (bank == SND_ALIAS_BANK_GAME) {
            memcpy(com_soundAliasLocalizedSource, sourceName, sourceNameLength + 1);
        }
    }

    sound_alias_compat_before_bank_activation(bank);
    com_soundAliasBankActive[bank] = 1;
    sound_alias_compat_after_bank_activation(bank);
}

/*
 * Complete common alias-bank teardown core.  CoDUOMP.exe
 * 0x00438240..0x004382d7 and coduo_lnxded 0x0806e5c2..0x0806e662 agree on
 * table ownership, hash clearing, and activation state.  Client audio unload
 * and command removal remain in the target boundary around those operations.
 */
void Com_UnloadSoundAliases(sndAliasBank_t bank)
{
    if (!com_soundAliasBankActive[bank]) {
        return;
    }

    if (bank == SND_ALIAS_BANK_GAME) {
        com_soundAliasLocalizedSource[0] = '\0';
    } else {
        sound_alias_compat_before_bank_deactivation(bank);
    }

    if (com_soundAliases[bank] != NULL) {
        if (bank != SND_ALIAS_BANK_CGAME ||
            com_soundAliases[SND_ALIAS_BANK_GAME] == NULL) {
            Z_FreeInternal(com_soundAliases[bank]);
        }

        com_soundAliases[bank] = NULL;
        com_soundAliasCount[bank] = 0;
        memset(com_soundAliasHash[bank], 0,
               sizeof(com_soundAliasHash[bank]));
    }

    com_soundAliasBankActive[bank] = 0;
    sound_alias_compat_after_bank_deactivation(bank);
}
