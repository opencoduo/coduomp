#include "server_authorize.h"

#include "qcommon/com_parse.h"
#include "compat/coduo_int32_bits.h"
#include "compat/crt/random_compat.h"
#include "filesystem/filesystem.h"
#include "qcommon/net_compare.h"
#include "qcommon/net_text.h"
#include "qcommon/netchan.h"
#include "qcommon/q_command.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_endian.h"
#include "qcommon/q_string.h"
#include "server_client_message.h"
#include "server_commands.h"
#include "server_master.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define SV_BAN_STRNICMP _strnicmp
#else
#include <strings.h>
#define SV_BAN_STRNICMP strncasecmp
#endif

enum {
    SERVER_AUTHORIZE_MILLISECONDS_PER_SECOND = 1000,
    SERVER_AUTHORIZE_TIMEOUT_MSEC = 5000,
    SERVER_AUTHORIZE_PORT = 20600
};

extern serverStatic_t svs;
extern cvar_t *sv_kickBanTime;
extern cvar_t *sv_onlyVisibleClients;

void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
qboolean Sys_IsLANAddress(netadr_t address);
static const char sv_authorizeServerName[] =
    "coduoauthorize.activision.com";

#ifndef CODUO_DISABLE_SERVER_AUTH
#define CODUO_DISABLE_SERVER_AUTH 0
#endif

/*
 * Complete authorization handshake, GUID cache, and persistent ban-file
 * subsystem shared by the Windows client engine and Linux dedicated engine.
 * The supporting Mac client exports the same canonical function names.
 *
 * Function                              Windows       Linux
 * SV_GetChallenge                       0x00458c50    0x080897d4
 * SV_AuthorizeRequest                    0x00459020    0x08089c3f
 * SV_AuthorizeIpPacket                   0x00459590    0x0808a1f0
 * SV_AuthorizeGuidRecentlySeen          0x00459160    0x08089d79
 * SV_AuthorizeGuidOnBanList             0x004591d0    0x08089dfc
 * SV_AuthorizeGuidCacheSelectSlot       0x004592a0    0x08089e9a
 * SV_AuthorizeGuidCacheStore            0x004592e0    0x08089ef9
 * SV_BanClient                          0x00459300    0x08089f25
 * SV_UnbanClient                        0x004593d0    0x0808a021
 *
 * Linux calls Com_Parse where Windows inlines its unget-token prelude before
 * calling Com_ParseExt(..., qtrue); the common source uses that canonical
 * wrapper.  Linux calls Q_strncpyz where Windows emits its exact strncpy-then-
 * terminate body inline.  The ban-name comparison retains only the CRT name
 * selected by each original platform: _strnicmp on Windows and strncasecmp on
 * Linux/Mac. SV_GetChallenge uses the selected server random numeric domain.
 */

void SV_GetChallenge(netadr_t from)
{
    int32_t oldestSlot = 0;
    int32_t oldestTime = INT32_MAX;
    int32_t slot;
    challenge_t *challenge;

    for (slot = 0; slot < MAX_CHALLENGES; ++slot) {
        challenge = &svs.challenges[slot];
        if (challenge->connected == qfalse &&
            NET_CompareAdr(from, challenge->address) != qfalse) {
            break;
        }
        if (challenge->slotTimestamp < oldestTime) {
            oldestTime = challenge->slotTimestamp;
            oldestSlot = slot;
        }
    }

    if (slot == MAX_CHALLENGES) {
        challenge = &svs.challenges[oldestSlot];
        const uint32_t randomHigh =
            (uint32_t)coduo_server_rand() << 16;
        const uint32_t randomLow =
            (uint32_t)coduo_server_rand();

        challenge->challengeNumber = coduo_int32_from_bits(
            randomHigh ^ randomLow ^ (uint32_t)svs.time);
        challenge->address = from;
        challenge->authorizeStartTime = svs.realTime;
        challenge->firstPingMsec = 0;
        challenge->slotTimestamp = svs.realTime;
        challenge->connected = qfalse;
    } else {
        challenge = &svs.challenges[slot];
    }

#if CODUO_DISABLE_SERVER_AUTH
    challenge->pingStartTime = svs.realTime;
    if (sv_onlyVisibleClients->integer != 0) {
        NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i %i",
                           challenge->challengeNumber,
                           sv_onlyVisibleClients->integer);
    } else {
        NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i",
                           challenge->challengeNumber);
    }
    return;
#endif

    if (net_lanauthorize->integer == 0 &&
        Sys_IsLANAddress(from) != qfalse) {
        challenge->pingStartTime = svs.realTime;
        if (sv_onlyVisibleClients->integer != 0) {
            NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i %i",
                               challenge->challengeNumber,
                               sv_onlyVisibleClients->integer);
        } else {
            NET_OutOfBandPrint(NS_SERVER, from, "challengeResponse %i",
                               challenge->challengeNumber);
        }
        return;
    }

    if (svs.authorizeServerAddress.ip[0] == 0 &&
        svs.authorizeServerAddress.type != NA_BOT) {
        Com_Printf("Resolving %s\n", sv_authorizeServerName);
        if (NET_StringToAdr(sv_authorizeServerName,
                            &svs.authorizeServerAddress) == qfalse) {
            Com_Printf("Couldn't resolve address\n");
            return;
        }
        svs.authorizeServerAddress.port =
            (uint16_t)BigShort((int16_t)SERVER_AUTHORIZE_PORT);
        Com_Printf("%s resolved to %i.%i.%i.%i:%i\n",
                   sv_authorizeServerName,
                   svs.authorizeServerAddress.ip[0],
                   svs.authorizeServerAddress.ip[1],
                   svs.authorizeServerAddress.ip[2],
                   svs.authorizeServerAddress.ip[3],
                   (uint16_t)BigShort(
                       (int16_t)svs.authorizeServerAddress.port));
    }

    if (svs.realTime - challenge->authorizeStartTime >
        SERVER_AUTHORIZE_TIMEOUT_MSEC) {
        challenge->pingStartTime = svs.realTime;
        if (NET_CompareAdr(from, *SV_MasterAddress()) == qfalse) {
            /* Linux 0x08089b39 calls Com_DPrintf at 0x08070297; the former
             * recovered Com_Printf spelling was a transcription error. */
            Com_DPrintf("authorize server timed out\n");
            if (sv_onlyVisibleClients->integer != 0) {
                NET_OutOfBandPrint(NS_SERVER, challenge->address,
                                   "challengeResponse %i %i",
                                   challenge->challengeNumber,
                                   sv_onlyVisibleClients->integer);
            } else {
                NET_OutOfBandPrint(NS_SERVER, challenge->address,
                                   "challengeResponse %i",
                                   challenge->challengeNumber);
            }
            return;
        }
    }

    SV_AuthorizeRequest(from, challenge->challengeNumber);
}

void SV_AuthorizeRequest(netadr_t from, int32_t challenge)
{
#if CODUO_DISABLE_SERVER_AUTH
    (void)from;
    (void)challenge;
#else
    char fsGame[MAX_STRING_CHARS];

    if (svs.authorizeServerAddress.type == NA_BOT) {
        return;
    }

    fsGame[0] = '\0';
    cvar_t *const fsGameCvar =
        Cvar_Get("fs_game", "", CVAR_SYSTEMINFO | CVAR_INIT);
    if (fsGameCvar != NULL && fsGameCvar->string[0] != '\0') {
        const size_t fsGameLength = strlen(fsGameCvar->string);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (fsGameLength >= sizeof(fsGame)) {
            Com_Printf("SV_AuthorizeRequest: fs_game exceeds MAX_STRING_CHARS\n");
            return;
        }
        memcpy(fsGame, fsGameCvar->string, fsGameLength + 1u);
    }

    Com_DPrintf("sending getIpAuthorize for %s\n", NET_AdrToString(from));
    cvar_t *const allowAnonymous =
        Cvar_Get("sv_allowAnonymous", "0", CVAR_SERVERINFO);
    NET_OutOfBandPrint(NS_SERVER, svs.authorizeServerAddress,
                       "getIpAuthorize %i %i.%i.%i.%i %s %i",
                       challenge,
                       from.ip[0], from.ip[1], from.ip[2], from.ip[3],
                       fsGame, allowAnonymous->integer);
#endif
}

void SV_AuthorizeIpPacket(netadr_t from)
{
    /* Windows calls NET_CompareBaseAdrSigned directly; Linux calls its
     * boolean NET_CompareBaseAdr wrapper at 0x080848cb. Both intentionally
     * ignore the packet's source port. */
    if (NET_CompareBaseAdr(from, svs.authorizeServerAddress) == qfalse) {
        Com_Printf("SV_AuthorizeIpPacket: not from authorize server\n");
        return;
    }

    const int32_t challengeNumber = atoi(Cmd_Argv(1));
    int32_t challengeIndex;
    for (challengeIndex = 0;
         challengeIndex < MAX_CHALLENGES;
         ++challengeIndex) {
        if (svs.challenges[challengeIndex].challengeNumber ==
            challengeNumber) {
            break;
        }
    }
    if (challengeIndex == MAX_CHALLENGES) {
        Com_Printf("SV_AuthorizeIpPacket: challenge not found\n");
        return;
    }

    challenge_t *const challenge = &svs.challenges[challengeIndex];
    challenge->pingStartTime = svs.realTime;

    const char *const response = Cmd_Argv(2);
    const char *const reason = Cmd_Argv(3);
    Q_strncpyz(challenge->authGuidString, Cmd_Argv(5),
               sizeof(challenge->authGuidString));

    if (Q_stricmp(response, "demo") == 0) {
        if (Cvar_VariableValue("fs_restrict") != 0.0f) {
            NET_OutOfBandPrint(NS_SERVER, challenge->address,
                               "challengeResponse %i",
                               challenge->challengeNumber);
        } else {
            NET_OutOfBandPrint(NS_SERVER, challenge->address,
                               "error\nEXE_ERR_NOT_A_DEMO_SERVER");
            memset(challenge, 0, sizeof(*challenge));
        }
        return;
    }

    if (Q_stricmp(response, "accept") == 0) {
        challenge->numericGuid = atoi(Cmd_Argv(4));
        if (SV_AuthorizeGuidOnBanList(challenge->numericGuid) != qfalse) {
            Com_Printf(
                "rejected connection from permanently banned GUID %i\n",
                challenge->numericGuid);
            NET_OutOfBandPrint(
                NS_SERVER, challenge->address,
                "error\n\x15" "You are permanently banned from this server");
            memset(challenge, 0, sizeof(*challenge));
            return;
        }
        if (SV_AuthorizeGuidRecentlySeen(challenge->numericGuid) != qfalse) {
            Com_Printf(
                "rejected connection from temporarily banned GUID %i\n",
                challenge->numericGuid);
            NET_OutOfBandPrint(
                NS_SERVER, challenge->address,
                "error\n\x15" "You are temporarily banned from this server");
            memset(challenge, 0, sizeof(*challenge));
            return;
        }

        if (challenge->connected == qfalse) {
            if (sv_onlyVisibleClients->integer != 0) {
                NET_OutOfBandPrint(NS_SERVER, challenge->address,
                                   "challengeResponse %i %i",
                                   challenge->challengeNumber,
                                   sv_onlyVisibleClients->integer);
            } else {
                NET_OutOfBandPrint(NS_SERVER, challenge->address,
                                   "challengeResponse %i",
                                   challenge->challengeNumber);
            }
        }
        return;
    }

    if (Q_stricmp(response, "deny") == 0) {
        if (reason == NULL || reason[0] == '\0') {
            NET_OutOfBandPrint(NS_SERVER, challenge->address,
                               "error\nEXE_ERR_CDKEY_IN_USE");
        } else if (Q_stricmp(reason, "CLIENT_UNKNOWN_TO_AUTH") == 0 ||
                   Q_stricmp(reason, "BAD_CDKEY") == 0) {
            NET_OutOfBandPrint(NS_SERVER, challenge->address, "needcdkey");
        } else if (Q_stricmp(reason, "INVALID_CDKEY") == 0) {
            NET_OutOfBandPrint(NS_SERVER, challenge->address,
                               "error\nEXE_ERR_CDKEY_IN_USE");
        } else {
            NET_OutOfBandPrint(NS_SERVER, challenge->address,
                               "error\nEXE_ERR_BAD_CDKEY");
        }
        memset(challenge, 0, sizeof(*challenge));
        return;
    }

    if (reason == NULL || reason[0] == '\0') {
        NET_OutOfBandPrint(NS_SERVER, challenge->address,
                           "error\nEXE_ERR_BAD_CDKEY");
    } else {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the authorization reason as data in a
         * literal conversion; the common packet sink enforces full capacity. */
        NET_OutOfBandPrint(NS_SERVER, challenge->address,
                           "error\n%s", reason);
    }
    memset(challenge, 0, sizeof(*challenge));
}

qboolean SV_AuthorizeGuidRecentlySeen(int32_t guid)
{
    if (guid == 0) {
        return qfalse;
    }

    for (int32_t slot = 0;
         slot < SERVER_AUTHORIZE_GUID_CACHE_ENTRY_COUNT;
         ++slot) {
        const serverAuthorizeGuidCacheEntry_t *const entry =
            &svs.authorizeGuidCache[slot];
        if (entry->numericGuid == guid) {
            const long double age =
                (long double)(svs.time - entry->cacheTime);
            const long double duration =
                (long double)sv_kickBanTime->value *
                (long double)(float)
                    SERVER_AUTHORIZE_MILLISECONDS_PER_SECOND;

            /* Both originals retain the operands on x87 and accept equality
             * and unordered results along with age less than duration. */
            if (!(age > duration)) {
                return qtrue;
            }
        }
    }
    return qfalse;
}

qboolean SV_AuthorizeGuidOnBanList(int32_t guid)
{
    void *fileBuffer;

    if (guid == 0 || FS_ReadFile("ban.txt", &fileBuffer) < 0) {
        return qfalse;
    }

    char *scan = fileBuffer;
    qboolean found = qfalse;
    for (;;) {
        char *const token = Com_Parse(&scan);
        if (token[0] == '\0') {
            break;
        }
        if (atoi(token) == guid) {
            found = qtrue;
            break;
        }
        Com_SkipRestOfLine(&scan);
    }

    FS_FreeFile(fileBuffer);
    return found;
}

int32_t SV_AuthorizeGuidCacheSelectSlot(void)
{
    int32_t oldestSlot = 0;

    for (int32_t slot = 0;
         slot < SERVER_AUTHORIZE_GUID_CACHE_ENTRY_COUNT;
         ++slot) {
        if (svs.authorizeGuidCache[slot].numericGuid == 0) {
            return slot;
        }
        if (svs.authorizeGuidCache[slot].cacheTime <
            svs.authorizeGuidCache[oldestSlot].cacheTime) {
            oldestSlot = slot;
        }
    }
    return oldestSlot;
}

void SV_AuthorizeGuidCacheStore(int32_t guid)
{
    const int32_t slot = SV_AuthorizeGuidCacheSelectSlot();
    svs.authorizeGuidCache[slot].numericGuid = guid;
    svs.authorizeGuidCache[slot].cacheTime = svs.time;
}

void SV_BanClient(client_t *client)
{
    if (client->netchan.remoteAddress.type == NA_LOOPBACK) {
        SV_SendServerCommand(NULL, qfalse,
                             "e \"EXE_CANNOTKICKHOSTPLAYER\"");
        return;
    }

    if (client->guid == 0 ||
        SV_AuthorizeGuidOnBanList(client->guid) != qfalse) {
        return;
    }

    int32_t fileHandle;
    if (FS_FOpenFileByMode("ban.txt", &fileHandle, FS_APPEND) < 0) {
        return;
    }

    char cleanName[SERVER_PLAYER_NAME_BUFFER_SIZE];
    Q_strncpyz(cleanName, client->name, SERVER_PLAYER_NAME_BUFFER_SIZE);
    Q_CleanStr(cleanName);
    FS_Printf(fileHandle, "%i %s\r\n", client->guid, cleanName);
    FS_FCloseFile(fileHandle);

    SV_DropClient(client, "EXE_PLAYERKICKED");
    client->lastPacketTime = svs.realTime;
}

void SV_UnbanClient(const char *name)
{
    void *fileBuffer;
    int32_t fileLength = FS_ReadFile("ban.txt", &fileBuffer);
    if (fileLength < 0) {
        return;
    }

    char cleanName[SERVER_PLAYER_NAME_BUFFER_SIZE];
    Q_strncpyz(cleanName, name, SERVER_PLAYER_NAME_BUFFER_SIZE);
    Q_CleanStr(cleanName);
    const size_t cleanNameLength = strlen(cleanName);

    char *const fileText = fileBuffer;
    char *scan = fileText;
    int32_t removedCount = 0;
    for (;;) {
        char *const lineStart = scan;
        char *const token = Com_Parse(&scan);
        if (token[0] == '\0') {
            break;
        }

        while (*scan != '\0' && (int8_t)*scan < '!') {
            ++scan;
        }

        const qboolean removeLine =
            SV_BAN_STRNICMP(scan, cleanName, cleanNameLength) == 0 &&
            (scan[cleanNameLength] == '\r' ||
             scan[cleanNameLength] == '\n');

        Com_SkipRestOfLine(&scan);
        if (removeLine != qfalse) {
            ++removedCount;
            memmove(lineStart, scan,
                    (size_t)(fileLength -
                             (int32_t)(scan - fileText)) + 1);
            fileLength -= (int32_t)(scan - lineStart);
            scan = lineStart;
        }
    }

    FS_WriteFile("ban.txt", fileText, fileLength);
    FS_FreeFile(fileBuffer);

    if (removedCount == 0) {
        Com_Printf("no banned user has name %s\n", cleanName);
    } else {
        Com_Printf("unbanned %i user(s) named %s\n",
                   removedCount, cleanName);
    }
}

#undef SV_BAN_STRNICMP
