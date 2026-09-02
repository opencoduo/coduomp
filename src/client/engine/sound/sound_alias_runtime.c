#include "sound/alias/sound_alias.h"

#include "filesystem/filesystem.h"
#include "client/engine/platform/crt_boundary.h"
#include "client/engine/sound/miles_boundary.h"

static const char SOUND_ALIAS_TYPE_MISMATCH_WARNING_EXEMPT_FILE[] =
    "temp.wav";

/* Source: CoDUOMP.exe 0x00437a70..0x00437c66.
 * Name and source-level structure: same-module Mac symbol
 * Com_LoadSoundAliasSounds. Permanent-table construction canonicalizes
 * duplicate filenames to one pointer, which this routine uses to share the
 * loaded record or streamed-file result. */
void Com_LoadSoundAliasSounds(sndAliasBank_t bank)
{
#if defined(CODUOMP_DISABLE_AUDIO)
    /* NOT_FROM_ORIGINAL_SOURCE: an explicit no-audio build retains alias
     * metadata but has no digital driver that can consume audio payloads. */
    (void)bank;
    return;
#endif

    snd_alias_t *const aliases = com_soundAliases[bank];
    const int32_t aliasCount = com_soundAliasCount[bank];
    int32_t missingSoundCount = 0;

    for (int32_t aliasIndex = 0; aliasIndex < aliasCount; ++aliasIndex) {
        snd_alias_t *const alias = &aliases[aliasIndex];

        if (alias->type == SND_ALIAS_TYPE_LOADED) {
            qboolean warnedTypeMismatch = qfalse;
            qboolean sharedSound = qfalse;

            for (int32_t priorIndex = 0;
                 priorIndex < aliasIndex; ++priorIndex) {
                snd_alias_t *const prior = &aliases[priorIndex];

                if (prior->soundFile != alias->soundFile) {
                    continue;
                }
                if (prior->type == SND_ALIAS_TYPE_LOADED) {
                    alias->soundFileInfo = prior->soundFileInfo;
                    sharedSound = qtrue;
                    break;
                }
                if (warnedTypeMismatch == qfalse &&
                    coduo_crt_stricmp(
                        alias->soundFile,
                        SOUND_ALIAS_TYPE_MISMATCH_WARNING_EXEMPT_FILE) != 0) {
                    Com_Printf(
                        "WARNING: sound file '%s' used as streamed "
                        "in alias '%s' and loaded in alias '%s'\n",
                        alias->soundFile, prior->aliasName,
                        alias->aliasName);
                    warnedTypeMismatch = qtrue;
                }
            }

            if (sharedSound != qfalse) {
                continue;
            }

            alias->soundFileInfo = MSS_LoadSoundFile(alias->soundFile);
            if (alias->soundFileInfo == NULL) {
                Com_Printf(
                    "WARNING: loaded sound file 'sound/%s' "
                    "couldn't be read\n",
                    alias->soundFile);
                ++missingSoundCount;
            }
            continue;
        }

        if (alias->type == SND_ALIAS_TYPE_STREAMED) {
            qboolean warnedTypeMismatch = qfalse;
            qboolean sharedSound = qfalse;

            for (int32_t priorIndex = 0;
                 priorIndex < aliasIndex; ++priorIndex) {
                snd_alias_t *const prior = &aliases[priorIndex];

                if (prior->soundFile != alias->soundFile) {
                    continue;
                }
                if (prior->type == SND_ALIAS_TYPE_STREAMED) {
                    alias->streamedFileExists =
                        prior->streamedFileExists;
                    sharedSound = qtrue;
                    break;
                }
                if (warnedTypeMismatch == qfalse &&
                    coduo_crt_stricmp(
                        alias->soundFile,
                        SOUND_ALIAS_TYPE_MISMATCH_WARNING_EXEMPT_FILE) != 0) {
                    Com_Printf(
                        "WARNING: sound file '%s' used as streamed "
                        "in alias '%s' and loaded in alias '%s'\n",
                        alias->soundFile, alias->aliasName,
                        prior->aliasName);
                    warnedTypeMismatch = qtrue;
                }
            }

            if (sharedSound != qfalse) {
                continue;
            }

            int32_t fileHandle;
            (void)FS_FOpenFileRead(
                va("sound/%s", alias->soundFile),
                &fileHandle, qfalse);

            if (fileHandle != 0) {
                FS_FCloseFile(fileHandle);
                alias->streamedFileExists = 1;
            } else {
                alias->streamedFileExists = 0;
                Com_Printf(
                    "^1WARNING: streamed sound 'sound/%s' not found\n",
                    alias->soundFile);
                ++missingSoundCount;
            }
        }
    }

    if (missingSoundCount != 0 && mss_errorOnMissing->integer != 0) {
        const errorParm_t errorCode =
            bank == SND_ALIAS_BANK_COMMON ? ERR_FATAL : ERR_DROP;
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        Com_Error(errorCode,
                  "%i sound file(s) are missing or in a bad format\n",
                  missingSoundCount);
    }
}

/* Source: CoDUOMP.exe 0x00437c70..0x00437cd8.
 * Name: same-module Mac symbol Com_UnloadSoundAliasSounds. */
void Com_UnloadSoundAliasSounds(sndAliasBank_t bank)
{
    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);

    snd_alias_t *const aliases = com_soundAliases[bank];
    const int32_t aliasCount = com_soundAliasCount[bank];

    for (int32_t aliasIndex = 0; aliasIndex < aliasCount; ++aliasIndex) {
        snd_alias_t *const alias = &aliases[aliasIndex];

        if (alias->type != SND_ALIAS_TYPE_LOADED) {
            continue;
        }

        qboolean sharedSound = qfalse;
        for (int32_t priorIndex = 0;
             priorIndex < aliasIndex; ++priorIndex) {
            const snd_alias_t *const prior = &aliases[priorIndex];

            if (prior->type == SND_ALIAS_TYPE_LOADED &&
                prior->soundFile == alias->soundFile) {
                sharedSound = qtrue;
                break;
            }
        }

        if (sharedSound == qfalse) {
            MSS_UnloadSoundFile(alias->soundFileInfo);
        }
        alias->soundFileInfo = NULL;
    }
}
