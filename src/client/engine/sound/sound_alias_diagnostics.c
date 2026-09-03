#include "sound/alias/sound_alias.h"

#include "sound_system.h"

static const float SOUND_ALIAS_BYTES_TO_KILOBYTES =
    1.0f / 1024.0f;
static const float SOUND_ALIAS_BYTES_TO_MEGABYTES =
    1.0f / (1024.0f * 1024.0f);

/* Source: CoDUOMP.exe 0x00436430..0x00436512.
 * Name: same-module Mac symbol Com_LoadedSoundList. */
void Com_LoadedSoundList(sndAliasBank_t bank)
{
    int32_t totalBytes = 0;
    snd_alias_t *aliases;
    int32_t aliasCount;

    if (!com_soundAliasBankActive[bank]) {
        return;
    }

    aliases = com_soundAliases[bank];
    aliasCount = com_soundAliasCount[bank];
    for (int32_t aliasIndex = 0; aliasIndex < aliasCount; ++aliasIndex) {
        snd_alias_t *alias = &aliases[aliasIndex];
        qboolean alreadyListed = qfalse;

        if (alias->type != SND_ALIAS_TYPE_LOADED) {
            continue;
        }

        for (int32_t previousIndex = 0;
             previousIndex < aliasIndex; ++previousIndex) {
            const snd_alias_t *previous = &aliases[previousIndex];

            if (previous->type == SND_ALIAS_TYPE_LOADED &&
                previous->soundFile == alias->soundFile) {
                alreadyListed = qtrue;
                break;
            }
        }
        if (alreadyListed) {
            continue;
        }

        if (alias->soundFileInfo != NULL) {
            const int32_t soundBytes =
                (int32_t)(sizeof(*alias->soundFileInfo) +
                          alias->soundFileInfo->dataLength);

            totalBytes += soundBytes;
            Com_Printf("%-64s %7.1f KB\n", alias->soundFile,
                       (double)soundBytes *
                           SOUND_ALIAS_BYTES_TO_KILOBYTES);
        } else {
            Com_Printf("%-64s FAILED TO LOAD\n", alias->soundFile);
        }
    }

    Com_Printf("\ntotal usage %7.3f MB\n",
               (double)totalBytes *
                   SOUND_ALIAS_BYTES_TO_MEGABYTES);
}

/* Source: CoDUOMP.exe 0x00436520..0x004365a7.
 * Name: same-module Mac symbol Com_StreamedSoundList. */
void Com_StreamedSoundList(sndAliasBank_t bank)
{
    snd_alias_t *aliases;
    int32_t aliasCount;

    if (!com_soundAliasBankActive[bank]) {
        return;
    }

    aliases = com_soundAliases[bank];
    aliasCount = com_soundAliasCount[bank];
    for (int32_t aliasIndex = 0; aliasIndex < aliasCount; ++aliasIndex) {
        snd_alias_t *alias = &aliases[aliasIndex];
        qboolean alreadyListed = qfalse;

        if (alias->type != SND_ALIAS_TYPE_STREAMED) {
            continue;
        }

        for (int32_t previousIndex = 0;
             previousIndex < aliasIndex; ++previousIndex) {
            const snd_alias_t *previous = &aliases[previousIndex];

            if (previous->type == SND_ALIAS_TYPE_STREAMED &&
                previous->soundFile == alias->soundFile) {
                alreadyListed = qtrue;
                break;
            }
        }
        if (alreadyListed) {
            continue;
        }

        if (alias->streamedFileExists) {
            Com_Printf("%-64s\n", alias->soundFile);
        } else {
            Com_Printf("%-64s FILE NOT FOUND\n", alias->soundFile);
        }
    }
}

/* Source: CoDUOMP.exe 0x004365b0..0x00436607.
 * Name: same-module Mac symbol Com_SoundList_f. */
void Com_SoundList_f(void)
{
    Com_Printf("\n________________________________________\n"
               "currently streamed menu sounds:\n");
    Com_StreamedSoundList(SND_ALIAS_BANK_COMMON);

    Com_Printf("\n________________________________________\n"
               "currently streamed in-game sounds:\n");
    Com_StreamedSoundList(SND_ALIAS_BANK_CGAME);

    Com_Printf("________________________________________\n"
               "currently loaded menu sounds:\n");
    Com_LoadedSoundList(SND_ALIAS_BANK_COMMON);

    Com_Printf("\n________________________________________\n"
               "currently loaded in-game sounds:\n");
    Com_LoadedSoundList(SND_ALIAS_BANK_CGAME);

    Com_Printf("\n");
}
