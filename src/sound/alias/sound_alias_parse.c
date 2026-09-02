#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sound_alias_private.h"

enum {
    SOUND_ALIAS_CHANNEL_LIST_BUFFER_SIZE = 16384,
    SOUND_ALIAS_LOADSPEC_MAX_CHARS = 65535,
    SOUND_ALIAS_LOADSPEC_COPY_SIZE = SOUND_ALIAS_LOADSPEC_MAX_CHARS + 1,
    SOUND_ALIAS_ALL_MP_COMPARE_CHARS = 6,
    SOUND_ALIAS_GAME_PREFIX_CHARS = 5
};

#define SOUND_ALIAS_TOKEN_BOUNDARY_MIN '!'
#define SOUND_ALIAS_SPACE ' '

/* Original Windows pointer table 0x005c51a4.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * ten ordered sound-channel name targets. */
static const char *const soundAliasChannelNames[SND_ALIAS_CHANNEL_COUNT] = {"auto", "menu",  "weapon", "voice",     "item",
                                                                            "body", "local", "music",  "announcer", "shellshock"};

/* Original Windows 19-entry field-name table at 0x005c5158, shared by CSV
 * loading and the developer localization rewrite path.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * 19 original pointers, including the leading NULL. */
const char *const soundAliasFieldNames[SND_ALIAS_FIELD_COUNT] = {
    NULL,       "name",    "sequence", "file", "subtitle",    "vol_min",  "vol_max",     "pitch_min", "pitch_max", "dist_min",
    "dist_max", "channel", "type",     "loop", "probability", "loadspec", "masterslave", "lod_min",   "lod_max"};

/* CoDUOMP.exe 0x00436770..0x00436824 and coduo_lnxded
 * 0x0806c9c8..0x0806cae6; canonical name confirmed by the supporting Mac
 * engine symbol. */
void Com_LoadSoundAliasDefaults(snd_alias_parse_node_t *node, qboolean defaultLoadspec)
{
    const size_t sourceFileLength = strlen(com_soundAliasCurrentFile);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (sourceFileLength >= sizeof(node->sourceFile)) {
        Com_Error(ERR_DROP, "\x15Sound alias source filename exceeds %i bytes", (int32_t)sizeof(node->sourceFile) - 1);
        return;
    }
    strcpy(node->sourceFile, com_soundAliasCurrentFile);
    node->aliasName[0] = '\0';
    node->sequence = 0;
    node->soundFile[0] = '\0';
    node->subtitle[0] = '\0';
    node->volumeMin = 1.0f;
    node->volumeMax = 1.0f;
    node->pitchMin = 1.0f;
    node->pitchMax = 1.0f;
    node->distanceMin = 120.0f;
    node->distanceMax = 0.0f;
    node->channel = SND_ALIAS_CHANNEL_AUTO;
    node->type = SND_ALIAS_TYPE_LOADED;
    node->loop = qfalse;
    node->selectionWeight = 1.0f;
    node->matchesLoadSpecification = defaultLoadspec != qfalse;
    node->isMaster = 0;
    node->isSlave = 0;
    node->slavePercentage = 1.0f;
    node->lodMin = -1.0f;
    node->lodMax = -1.0f;
    node->next = 0;
}

/* CoDUOMP.exe 0x00436830..0x00436911 and coduo_lnxded
 * 0x0806cae6..0x0806cc0f; canonical name confirmed by the supporting Mac
 * engine symbol. */
sndAliasChannel_t Com_SoundAliasChannelForName(const char *text)
{
    char expectedChannels[SOUND_ALIAS_CHANNEL_LIST_BUFFER_SIZE];
    int length;
    int channel;

    for (channel = 0; channel < SND_ALIAS_CHANNEL_COUNT; channel++) {
        if (Q_stricmp(text, soundAliasChannelNames[channel]) == 0) {
            return channel;
        }
    }

    length = 0;
    for (channel = 0; channel < SND_ALIAS_CHANNEL_COUNT; channel++) {
        length += sprintf(expectedChannels + length, "%s", soundAliasChannelNames[channel]);
        if (channel < SND_ALIAS_CHANNEL_ANNOUNCER) {
            length += sprintf(expectedChannels + length, ", ");
        } else if (channel == SND_ALIAS_CHANNEL_ANNOUNCER) {
            length += sprintf(expectedChannels + length, " or ");
        }
    }

    Com_Error(ERR_DROP,
              "\x15"
              "Sound alias file %s: Unknown sound channel '%s'; "
              "should be %s\n",
              com_soundAliasCurrentFile, text, expectedChannels);
    return SND_ALIAS_CHANNEL_AUTO;
}

/* CoDUOMP.exe 0x00436920..0x00436972 and coduo_lnxded
 * 0x0806cc0f..0x0806cc85; canonical name confirmed by the supporting Mac
 * engine symbol. */
sndAliasType_t Com_SoundAliasTypeForName(const char *text)
{
    if (Q_stricmp(text, "streamed") == 0) {
        return SND_ALIAS_TYPE_STREAMED;
    }

    if (Q_stricmp(text, "loaded") == 0) {
        return SND_ALIAS_TYPE_LOADED;
    }

    Com_Error(ERR_DROP,
              "\x15"
              "Sound alias file %s: Unknown sound type '%s'; "
              "should be streamed or loaded\n",
              com_soundAliasCurrentFile, text);
    return SND_ALIAS_TYPE_UNKNOWN;
}

/* CoDUOMP.exe 0x00436980..0x004369c9 and coduo_lnxded
 * 0x0806cc85..0x0806ccfb; canonical name confirmed by the supporting Mac
 * engine symbol. */
qboolean Com_SoundAliasLoop(const char *text)
{
    if (Q_stricmp(text, "looping") == 0) {
        return qtrue;
    }

    if (Q_stricmp(text, "nonlooping") == 0) {
        return qfalse;
    }

    Com_Error(ERR_DROP,
              "\x15"
              "Sound alias file %s: Unknown sound looping type '%s'; "
              "should be looping or nonlooping\n",
              com_soundAliasCurrentFile, text);
    return qfalse;
}

/* CoDUOMP.exe 0x004369d0..0x00436c77 and coduo_lnxded
 * 0x0806ccfb..0x0806d096; canonical name confirmed by the supporting Mac
 * engine symbol. */
qboolean Com_SoundAliasLoadSpec(const char *sourceFile, const char *text)
{
    size_t sourceLength;
    char loadspec[SOUND_ALIAS_LOADSPEC_COPY_SIZE];
    char *cursor;
    char *match;
    qboolean foundModifier;

    Cvar_Get("fs_game", "", 0);
    sourceLength = strlen(sourceFile);
    loadspec[SOUND_ALIAS_LOADSPEC_MAX_CHARS] = '\0';
    strncpy(loadspec, text, sizeof(loadspec));
    if (loadspec[SOUND_ALIAS_LOADSPEC_MAX_CHARS] != '\0') {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Sound alias file %s: loadspec is > %i "
                  "characters\n",
                  com_soundAliasCurrentFile, SOUND_ALIAS_LOADSPEC_MAX_CHARS);
    }

    Q_strlwr(loadspec);
    cursor = loadspec;

    if (loadspec[0] == '!') {
        do {
            ++cursor;
            if (SOUND_ALIAS_SPACE < *cursor) {
                break;
            }
        } while (*cursor != '\0');

        if (strcmp(sourceFile, "menu") == 0 && strstr(cursor, "menu") == NULL) {
            return 0;
        }

        match = strstr(cursor, "all_");
        if (match != NULL && strncmp(match, "all_mp", SOUND_ALIAS_ALL_MP_COMPARE_CHARS) == 0) {
            return 0;
        }

        if (fs_gameDirVar[0] != '\0') {
            match = strstr(cursor, "game_");
            if (match != NULL && strncmp(match + SOUND_ALIAS_GAME_PREFIX_CHARS, fs_gameDirVar, strlen(fs_gameDirVar)) == 0) {
                return 0;
            }
        }

        while ((cursor = strstr(cursor, sourceFile)) != NULL) {
            if ((cursor == loadspec || cursor[-1] < SOUND_ALIAS_TOKEN_BOUNDARY_MIN) &&
                cursor[sourceLength] < SOUND_ALIAS_TOKEN_BOUNDARY_MIN) {
                return 0;
            }
            ++cursor;
        }

        return 1;
    }

    while ((match = strstr(cursor, sourceFile)) != NULL) {
        if ((match == loadspec || match[-1] < SOUND_ALIAS_TOKEN_BOUNDARY_MIN) && match[sourceLength] < SOUND_ALIAS_TOKEN_BOUNDARY_MIN) {
            return 1;
        }
        cursor = match + 1;
    }

    foundModifier = qfalse;
    if (strcmp(sourceFile, "menu") == 0 && strstr(cursor, "menu") == NULL) {
        return 0;
    }

    match = strstr(cursor, "all_");
    if (match != NULL) {
        foundModifier = qtrue;
        if (strncmp(match, "all_mp", SOUND_ALIAS_ALL_MP_COMPARE_CHARS) != 0) {
            return 0;
        }
    }

    if (fs_gameDirVar[0] != '\0') {
        match = strstr(cursor, "game_");
        if (match != NULL) {
            foundModifier = qtrue;
            if (strncmp(match + SOUND_ALIAS_GAME_PREFIX_CHARS, fs_gameDirVar, strlen(fs_gameDirVar)) != 0) {
                return 0;
            }
        }
    }

    if (foundModifier) {
        return 1;
    }

    return 0;
}

/* CoDUOMP.exe 0x00436c80..0x00436cbd and coduo_lnxded
 * 0x0806d096..0x0806d0f7; canonical name confirmed by the supporting Mac
 * engine symbol. */
void Com_SoundAliasMasterSlave(const char *text, snd_alias_parse_node_t *node)
{
    if (Q_stricmp(text, "master") == 0) {
        node->isMaster = 1;
        node->isSlave = 0;
    } else {
        node->isMaster = 0;
        node->isSlave = 1;
        node->slavePercentage = (float)atof(text);
    }
}

/* CoDUOMP.exe 0x00436cc0..0x00436f86 and coduo_lnxded
 * 0x0806d0f8..0x0806d4ad; canonical name confirmed by the supporting Mac
 * engine symbol. */
void Com_LoadSoundAliasField(const char *sourceFile, const char *text, sndAliasField_t column, uint8_t seenColumns[SND_ALIAS_FIELD_COUNT],
                             snd_alias_parse_node_t *node)
{
    size_t length;
    int index;

    if (column == SND_ALIAS_FIELD_UNKNOWN) {
        return;
    }

    if (seenColumns[column] != 0) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Sound alias file %s: Duplicate entries for the "
                  "'%s' column\n",
                  com_soundAliasCurrentFile, soundAliasFieldNames[column]);
    }

    seenColumns[column] = 1;

    switch (column) {
    case SND_ALIAS_FIELD_NAME:
        length = strlen(text);
        if (length >= sizeof(node->aliasName) - 1) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "Sound alias file %s: Alias name '%s' is "
                      "longer than %i characters\n",
                      com_soundAliasCurrentFile, text, (int)(sizeof(node->aliasName) - 1));
        }
        if (Com_IsValidAliasName(text) == qfalse) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "Sound alias file %s: Alias name '%s' is "
                      "invalid\n",
                      com_soundAliasCurrentFile, text);
        }
        strcpy(node->aliasName, text);
        break;
    case SND_ALIAS_FIELD_SEQUENCE:
        node->sequence = atoi(text);
        break;
    case SND_ALIAS_FIELD_FILE:
        length = strlen(text);
        if (length >= sizeof(node->soundFile) - 1) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "Sound alias file %s: Sound file '%s' is "
                      "longer than %i characters\n",
                      com_soundAliasCurrentFile, text, (int)(sizeof(node->soundFile) - 1));
        }
        strcpy(node->soundFile, text);
        break;
    case SND_ALIAS_FIELD_SUBTITLE:
        length = strlen(text);
        if (length >= sizeof(node->subtitle) - 1) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "Sound alias file %s: Subtitle '%s' is longer "
                      "than %i characters\n",
                      com_soundAliasCurrentFile, text, (int)(sizeof(node->subtitle) - 1));
        }
        for (index = 0; text[index] != '\0'; index++) {
            if ((signed char)text[index] < 0) {
                Com_Error(ERR_DROP,
                          "\x15"
                          "Sound alias file %s: Subtitle '%s' has "
                          "invalid character '%c' ascii %i\n",
                          com_soundAliasCurrentFile, text, (int)(signed char)text[index], (unsigned char)text[index]);
            }
        }
        strcpy(node->subtitle, text);
        break;
    case SND_ALIAS_FIELD_VOLUME_MIN:
        node->volumeMin = (float)atof(text);
        if (seenColumns[SND_ALIAS_FIELD_VOLUME_MAX] == 0) {
            node->volumeMax = node->volumeMin;
        }
        break;
    case SND_ALIAS_FIELD_VOLUME_MAX:
        node->volumeMax = (float)atof(text);
        break;
    case SND_ALIAS_FIELD_PITCH_MIN:
        node->pitchMin = (float)atof(text);
        if (seenColumns[SND_ALIAS_FIELD_PITCH_MAX] == 0) {
            node->pitchMax = node->pitchMin;
        }
        break;
    case SND_ALIAS_FIELD_PITCH_MAX:
        node->pitchMax = (float)atof(text);
        break;
    case SND_ALIAS_FIELD_DISTANCE_MIN:
        node->distanceMin = (float)atof(text);
        break;
    case SND_ALIAS_FIELD_DISTANCE_MAX:
        node->distanceMax = (float)atof(text);
        break;
    case SND_ALIAS_FIELD_CHANNEL:
        node->channel = Com_SoundAliasChannelForName(text);
        break;
    case SND_ALIAS_FIELD_TYPE:
        node->type = Com_SoundAliasTypeForName(text);
        break;
    case SND_ALIAS_FIELD_LOOP:
        node->loop = Com_SoundAliasLoop(text);
        break;
    case SND_ALIAS_FIELD_PROBABILITY:
        node->selectionWeight = (float)atof(text);
        break;
    case SND_ALIAS_FIELD_LOAD_SPEC:
        node->matchesLoadSpecification = Com_SoundAliasLoadSpec(sourceFile, text);
        break;
    case SND_ALIAS_FIELD_MASTER_SLAVE:
        Com_SoundAliasMasterSlave(text, node);
        break;
    case SND_ALIAS_FIELD_LOD_MIN:
        node->lodMin = (float)atof(text);
        break;
    case SND_ALIAS_FIELD_LOD_MAX:
        node->lodMax = (float)atof(text);
        break;
    default:
        break;
    }
}
