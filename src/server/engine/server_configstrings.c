#include "server_configstrings.h"

#include "qcommon/fx_types.h"
#include "qcommon/game_state_types.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"
#include "qcommon/qcommon_runtime_types.h"
#include "server_commands.h"
#include "qcommon/server_runtime_types.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    SERVER_CONFIGSTRING_CHUNK_COPY_SIZE = 1000,
    SERVER_CONFIGSTRING_CHUNK_PAYLOAD_SIZE =
        SERVER_CONFIGSTRING_CHUNK_COPY_SIZE - 1
};

extern serverHeader_t sv;
extern serverStatic_t svs;
extern cvar_t *sv_maxclients;
extern char *sv_configstrings[MAX_CONFIGSTRINGS];

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Complete server configstring subsystem shared by both engines:
 *
 *                                  Windows client       Linux dedicated
 * SV_SetConfigstring               0x0045ecf0           0x08090da8
 * SV_GetConfigstring               0x0045eee0           0x08090f8c
 * SV_GetConfigstringConst          0x0045ef40           0x08091016
 * SV_GetConfigValueForKey          0x0045ef60           0x08091042
 * SV_SetConfigValueForKey          0x0045efc0           0x080910b8
 *
 * The bodies agree on validation, storage ownership, chunking, wire commands,
 * table traversal, and error behavior.  In SV_SetConfigstring, Linux compares
 * the client state against CS_CONNECTED and takes the send path when greater;
 * that is the same CS_PRIMED-or-later test emitted by Windows.  The former
 * recovered Linux `state <= CS_PRIMED` condition was a transcription error.
 * The original key lookups call their platform CRT case-insensitive compare;
 * Q_strcasecmp preserves the equality result used here for cvar identifiers.
 */

void SV_SetConfigstring(int32_t index, const char *value)
{
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_Error(ERR_DROP,
                  "\x15" "SV_SetConfigstring: bad index %i\n", index);
    }

    if (value == NULL) {
        value = "";
    }
    /* NOT_FROM_ORIGINAL_SOURCE: an effect configstring's key prefix must fit
     * the client's fixed scheduler field before publication. */
    if (index >= CS_EFFECTS && index < CS_FX &&
        strcspn(value, ".") >= FX_EFFECT_TEMPLATE_NAME_CAPACITY) {
        Com_Error(ERR_DROP, "\x15" "SV_SetConfigstring: effect name is too long");
    }

    if (strcmp(value, sv_configstrings[index]) == 0) {
        return;
    }

    Z_FreeInternal(sv_configstrings[index]);
    sv_configstrings[index] = CopyStringInternal(value);

    if (sv.state != SS_GAME && sv.restarting == qfalse) {
        return;
    }

    for (int32_t clientNum = 0;
         clientNum < sv_maxclients->integer;
         ++clientNum) {
        client_t *const client = &svs.clients[clientNum];
        if (client->state < CS_PRIMED) {
            continue;
        }

        const size_t length = strlen(value);
        if (length < SERVER_CONFIGSTRING_CHUNK_COPY_SIZE) {
            SV_SendServerCommand(client, qtrue, "d %i %s", index, value);
            continue;
        }

        char chunk[MAX_STRING_CHARS];
        size_t offset = 0;
        size_t remaining = length;
        while (remaining > 0) {
            const char *command;
            if (offset == 0) {
                command = "x";
            } else if (remaining < SERVER_CONFIGSTRING_CHUNK_COPY_SIZE) {
                command = "z";
            } else {
                command = "y";
            }

            Q_strncpyz(chunk, value + offset,
                       SERVER_CONFIGSTRING_CHUNK_COPY_SIZE);
            SV_SendServerCommand(client, qtrue,
                                 "%s %i %s", command, index, chunk);
            offset += SERVER_CONFIGSTRING_CHUNK_PAYLOAD_SIZE;
            if (remaining <= SERVER_CONFIGSTRING_CHUNK_PAYLOAD_SIZE) {
                remaining = 0;
            } else {
                remaining -= SERVER_CONFIGSTRING_CHUNK_PAYLOAD_SIZE;
            }
        }
    }
}

void SV_GetConfigstring(int32_t index, char *buffer, int32_t bufferSize)
{
    if (bufferSize < 1) {
        Com_Error(ERR_DROP,
                  "\x15" "SV_GetConfigstring: bufferSize == %i",
                  bufferSize);
    }
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_Error(ERR_DROP,
                  "\x15" "SV_GetConfigstring: bad index %i\n", index);
    }

    if (sv_configstrings[index] == NULL) {
        buffer[0] = '\0';
    } else {
        Q_strncpyz(buffer, sv_configstrings[index], bufferSize);
    }
}

const char *SV_GetConfigstringConst(int32_t index)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)index >= MAX_CONFIGSTRINGS) {
        return "";
    }

    const char *const value = sv_configstrings[index];
    return value != NULL ? value : "";
}

const char *SV_GetConfigValueForKey(int32_t base, int32_t count,
                                    const char *key)
{
    for (int32_t slot = 0; slot < count; ++slot) {
        const char *const storedKey = sv_configstrings[base + slot];
        if (storedKey[0] == '\0') {
            break;
        }
        if (Q_strcasecmp(key, storedKey) == 0) {
            return sv_configstrings[base + count + slot];
        }
    }
    return "";
}

void SV_SetConfigValueForKey(int32_t base, int32_t count,
                             const char *key, const char *value)
{
    int32_t slot;
    for (slot = 0; slot < count; ++slot) {
        const char *const storedKey = sv_configstrings[base + slot];
        if (storedKey[0] == '\0') {
            SV_SetConfigstring(base + slot, key);
            break;
        }
        if (Q_strcasecmp(key, storedKey) == 0) {
            break;
        }
    }

    if (slot == count) {
        Com_Error(ERR_DROP,
                  "\x15" "SV_SetConfigValueForKey: overflow");
    }
    SV_SetConfigstring(base + count + slot, value);
}
