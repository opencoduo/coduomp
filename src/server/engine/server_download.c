#include "server_download.h"

#include "qcommon/com_sprintf.h"
#include "filesystem/filesystem.h"
#include "filesystem/filesystem_path_security.h"
#include "qcommon/q_command.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"
#include "server_client_gamestate.h"
#include "server_client_message.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

enum {
    SERVER_DOWNLOAD_FAILURE_BLOCK = 0,
    SERVER_DOWNLOAD_FAILURE_SIZE = -1,
    SERVER_DOWNLOAD_REDIRECT_BLOCK = -1,
    SERVER_DOWNLOAD_BLOCK_SIZE = 2048,
    SERVER_DOWNLOAD_MAX_RATE_MINIMUM = 1000,
    SERVER_DOWNLOAD_STALL_RETRANSMIT_MSEC = 1000
};

extern serverStatic_t svs;
extern cvar_t *sv_allowDownload;
extern cvar_t *sv_maxRate;
extern cvar_t *sv_pure;
extern cvar_t *sv_wwwBaseURL;
extern cvar_t *sv_wwwDlDisconnected;
extern cvar_t *sv_wwwDownload;

void Com_DPrintf(const char *format, ...);
void Com_Printf(const char *format, ...);
/*
 * Complete server download lifecycle shared by the Windows listen-server
 * engine and Linux dedicated engine.
 *
 * Function                    Windows       Linux
 * SV_CloseDownload            0x0045ad00    0x0808c016
 * SV_StopDownload_f           0x0045ad60    0x0808c09f
 * SV_DoneDownload_f           0x0045adb0    0x0808c0ef
 * SV_RetransmitDownload_f     0x0045ade0    0x0808c11a
 * SV_NextDownload_f           0x0045ae20    0x0808c159
 * SV_BeginDownload_f          0x0045af00    0x0808c25d
 * SV_WWWDownload_f            0x0045af40    0x0808c298
 * SV_BadDownload              0x0045b0e0    0x0808c503
 * SV_CheckFallbackURL         0x0045b150    0x0808c54f
 * SV_WriteDownloadToClient    0x0045b160    0x0808c559
 *
 * The state transitions, packet fields, window arithmetic, diagnostics, and
 * file ownership agree. Linux retains dead fallback diagnostics around the
 * same constant-true helper that MSVC folds away. Windows inlines the zeroing
 * zone allocation used for download blocks; Linux emits the ordinary call.
 * The supporting Mac client supplies the canonical function names above.
 */

void SV_CloseDownload(client_t *client)
{
    if (client->download.fileHandle != 0) {
        FS_FCloseFile(client->download.fileHandle);
    }

    client->download.fileHandle = 0;
    client->download.fileName[0] = '\0';
    for (int32_t block = 0; block < MAX_DOWNLOAD_WINDOW; ++block) {
        if (client->download.blockData[block] != NULL) {
            Z_FreeInternal(client->download.blockData[block]);
            client->download.blockData[block] = NULL;
        }
    }
}

void SV_StopDownload_f(client_t *client)
{
    if (client->download.fileName[0] != '\0') {
        Com_DPrintf("clientDownload: %d : file \"%s\" aborted\n", (int32_t)(client - svs.clients), client->download.fileName);
    }
    SV_CloseDownload(client);
}

void SV_DoneDownload_f(client_t *client)
{
    Com_DPrintf("clientDownload: %s Done\n", client->name);
    SV_SendClientGameState(client);
}

void SV_RetransmitDownload_f(client_t *client)
{
    const int32_t block = atoi(Cmd_Argv(1));
    if (block == client->download.nextAcknowledgmentBlock) {
        client->download.nextTransmitBlock = client->download.nextAcknowledgmentBlock;
    }
}

void SV_NextDownload_f(client_t *client)
{
    const int32_t block = atoi(Cmd_Argv(1));
    if (block != client->download.nextAcknowledgmentBlock) {
        SV_DropClient(client, "broken download");
        return;
    }

    const int32_t clientNum = (int32_t)(client - svs.clients);
    Com_DPrintf("clientDownload: %d : client acknowledge of block %d\n", clientNum, block);

    const int32_t blockSlot = client->download.nextAcknowledgmentBlock % MAX_DOWNLOAD_WINDOW;
    if (client->download.blockByteCounts[blockSlot] == 0) {
        Com_Printf("clientDownload: %d : file \"%s\" completed\n", clientNum, client->download.fileName);
        SV_CloseDownload(client);
        return;
    }

    client->download.lastBlockActivityTime = svs.realTime;
    ++client->download.nextAcknowledgmentBlock;
}

void SV_BeginDownload_f(client_t *client)
{
    SV_CloseDownload(client);
    const char *const fileName = Cmd_Argv(1);
    /* NOT_FROM_ORIGINAL_SOURCE: require a relative virtual download path at
     * the network boundary; filesystem opens independently repeat the policy. */
    if (coduo_compat_path_is_safe_relative(fileName) == qfalse || coduo_compat_path_is_pk3(fileName) == qfalse) {
        Com_Printf("WARNING: client '%s' requested invalid download path '%s'\n", client->name, fileName);
        SV_DropClient(client, "invalid download path");
        return;
    }
    Q_strncpyz(client->download.fileName, fileName, sizeof(client->download.fileName));
}

void SV_WWWDownload_f(client_t *client)
{
    const char *const subcommand = Cmd_Argv(1);

    if (client->download.redirectActive == qfalse) {
        Com_Printf("SV_WWWDownload: unexpected wwwdl '%s' for client '%s'\n", subcommand, client->name);
        SV_DropClient(client, "GMI_EXE_UNEXPECTEDWWWDOWLOADMESSAGE");
        return;
    }

    if (Q_stricmp(subcommand, "ack") == 0) {
        if (client->download.redirectAcknowledged != qfalse) {
            Com_Printf("WARNING: dupe wwwdl ack from client '%s'\n", client->name);
        }
        client->download.redirectAcknowledged = qtrue;
        return;
    }

    if (Q_stricmp(subcommand, "bbl8r") == 0) {
        SV_DropClient(client, "GMI_EXE_DOWNLOADDISCONNECTED");
        return;
    }

    if (client->download.redirectAcknowledged == qfalse) {
        Com_Printf("SV_WWWDownload: unexpected wwwdl '%s' for client '%s'\n", subcommand, client->name);
        SV_DropClient(client, "GMI_EXE_UNEXPECTEDWWWDOWLOADMESSAGE");
        return;
    }

    if (Q_stricmp(subcommand, "done") == 0) {
        client->download.fileHandle = 0;
        client->download.fileName[0] = '\0';
        client->download.redirectAcknowledged = qfalse;
        return;
    }

    if (Q_stricmp(subcommand, "fail") == 0) {
        client->download.fileHandle = 0;
        client->download.fileName[0] = '\0';
        client->download.redirectAcknowledged = qfalse;
        client->download.redirectFailed = qtrue;
        SV_SendClientGameState(client);
        return;
    }

    if (Q_stricmp(subcommand, "chkfail") == 0) {
        Com_Printf("WARNING: client '%s' reports that the redirect download "
                   "for '%s' had wrong checksum.\n",
                   client->name, client->download.fileName);
        Com_Printf("         you should check your download redirect "
                   "configuration.\n");
        client->download.fileHandle = 0;
        client->download.fileName[0] = '\0';
        client->download.redirectAcknowledged = qfalse;
        client->download.redirectFailed = qtrue;
        SV_SendClientGameState(client);
        return;
    }

    Com_Printf("SV_WWWDownload: unknown wwwdl subcommand '%s' "
               "for client '%s'\n",
               subcommand, client->name);
    SV_DropClient(client, "GMI_EXE_UNEXPECTEDWWWDOWLOADMESSAGE");
}

void SV_BadDownload(client_t *client, msg_t *message)
{
    MSG_WriteByte(message, SERVER_SVC_DOWNLOAD);
    MSG_WriteShort(message, SERVER_DOWNLOAD_FAILURE_BLOCK);
    MSG_WriteLong(message, SERVER_DOWNLOAD_FAILURE_SIZE);
    client->download.fileName[0] = '\0';
}

qboolean SV_CheckFallbackURL(void)
{
    return qtrue;
}

void SV_WriteDownloadToClient(client_t *client, msg_t *message)
{
    char errorText[MAX_STRING_CHARS];

    if (client->download.fileName[0] == '\0' || client->download.redirectAcknowledged != qfalse) {
        return;
    }

    if (client->download.fileHandle == 0) {
        const int32_t clientNum = (int32_t)(client - svs.clients);
        qboolean idPak;

        Com_DPrintf("clientDownload: %d : begining \"%s\"\n", clientNum, client->download.fileName);
        idPak = FS_idPak(client->download.fileName, "main", fs_basegame->string);

        if (sv_allowDownload->integer == 0 || idPak != qfalse) {
            if (idPak != qfalse) {
                Com_Printf("clientDownload: %d : \"%s\" cannot download id "
                           "pk3 files\n",
                           clientNum, client->download.fileName);
                Com_sprintf(errorText, sizeof(errorText), "EXE_CANTAUTODLGAMEPAK\x15%s", client->download.fileName);
            } else {
                Com_Printf("clientDownload: %d : \"%s\" download disabled", clientNum, client->download.fileName);
                if (sv_pure->integer == 0) {
                    Com_sprintf(errorText, sizeof(errorText), "EXE_AUTODL_SERVERDISABLED\x15%s", client->download.fileName);
                } else {
                    Com_sprintf(errorText, sizeof(errorText), "EXE_AUTODL_SERVERDISABLED_PURE\x15%s", client->download.fileName);
                }
            }

            MSG_WriteByte(message, SERVER_SVC_DOWNLOAD);
            MSG_WriteShort(message, SERVER_DOWNLOAD_FAILURE_BLOCK);
            MSG_WriteLong(message, SERVER_DOWNLOAD_FAILURE_SIZE);
            MSG_WriteString(message, errorText);
            client->download.fileName[0] = '\0';
            return;
        }

        if (sv_wwwDownload->integer != 0) {
            if (client->download.redirectAllowedByClient == qfalse) {
                if (SV_CheckFallbackURL() != qfalse) {
                    return;
                }
                Com_Printf("Client '%s' is not configured for www download\n", client->name);
            } else if (client->download.redirectFailed == qfalse) {
                int32_t redirectHandle;
                const int32_t redirectSize = FS_SV_FOpenFileRead(client->download.fileName, &redirectHandle);

                if (redirectSize != 0) {
                    FS_FCloseFile(redirectHandle);
                    Q_strncpyz(client->download.redirectUrl, va("%s/%s", sv_wwwBaseURL->string, client->download.fileName),
                               sizeof(client->download.redirectUrl));
                    Com_DPrintf("Redirecting client '%s' to %s\n", client->name, client->download.redirectUrl);
                    client->download.redirectActive = qtrue;
                    MSG_WriteByte(message, SERVER_SVC_DOWNLOAD);
                    MSG_WriteShort(message, SERVER_DOWNLOAD_REDIRECT_BLOCK);
                    MSG_WriteString(message, client->download.redirectUrl);
                    MSG_WriteLong(message, redirectSize);
                    MSG_WriteLong(message, sv_wwwDlDisconnected->integer != 0);
                    return;
                }

                /* NOT_FROM_ORIGINAL_SOURCE: supply both required diagnostic
                 * values; this affects only local logging. */
                Com_Printf("ERROR: Client '%s': couldn't extract file size "
                           "for %s\n",
                           client->name, client->download.fileName);
            } else {
                client->download.redirectFailed = qfalse;
                if (SV_CheckFallbackURL() != qfalse) {
                    return;
                }
                /* NOT_FROM_ORIGINAL_SOURCE: supply both required values to the
                 * Linux-only fallback diagnostic. */
                Com_Printf("Client '%s': falling back to regular downloading "
                           "for failed file %s\n",
                           client->name, client->download.fileName);
            }
        }

        client->download.redirectActive = qfalse;
        client->download.fileSize = FS_SV_FOpenFileRead(client->download.fileName, &client->download.fileHandle);
        if (client->download.fileSize <= 0) {
            Com_Printf("clientDownload: %d : \"%s\" file not found on "
                       "server\n",
                       clientNum, client->download.fileName);
            Com_sprintf(errorText, sizeof(errorText), "EXE_AUTODL_FILENOTONSERVER\x15%s", client->download.fileName);
            SV_BadDownload(client, message);
            MSG_WriteString(message, errorText);
            return;
        }

        client->download.nextTransmitBlock = 0;
        client->download.nextAcknowledgmentBlock = 0;
        client->download.nextBufferedBlock = 0;
        client->download.bytesRead = 0;
        client->download.eofBlockQueued = qfalse;
    }

    while (client->download.nextBufferedBlock - client->download.nextAcknowledgmentBlock < MAX_DOWNLOAD_WINDOW &&
           client->download.fileSize != client->download.bytesRead) {
        const int32_t blockSlot = client->download.nextBufferedBlock % MAX_DOWNLOAD_WINDOW;

        if (client->download.blockData[blockSlot] == NULL) {
            client->download.blockData[blockSlot] = Z_MallocInternal(SERVER_DOWNLOAD_BLOCK_SIZE);
        }

        client->download.blockByteCounts[blockSlot] =
            FS_Read(client->download.blockData[blockSlot], SERVER_DOWNLOAD_BLOCK_SIZE, client->download.fileHandle);
        if (client->download.blockByteCounts[blockSlot] < 0) {
            client->download.bytesRead = client->download.fileSize;
            break;
        }

        client->download.bytesRead += client->download.blockByteCounts[blockSlot];
        ++client->download.nextBufferedBlock;
    }

    if (client->download.bytesRead == client->download.fileSize && client->download.eofBlockQueued == qfalse &&
        client->download.nextBufferedBlock - client->download.nextAcknowledgmentBlock < MAX_DOWNLOAD_WINDOW) {
        const int32_t blockSlot = client->download.nextBufferedBlock % MAX_DOWNLOAD_WINDOW;
        client->download.blockByteCounts[blockSlot] = 0;
        ++client->download.nextBufferedBlock;
        client->download.eofBlockQueued = qtrue;
    }

    int32_t rate = client->rate;
    if (sv_maxRate->integer != 0) {
        if (sv_maxRate->integer < SERVER_DOWNLOAD_MAX_RATE_MINIMUM) {
            Cvar_Set("sv_MaxRate", "1000");
        }
        if (sv_maxRate->integer < rate) {
            rate = sv_maxRate->integer;
        }
    }

    int32_t blockBudget;
    if (rate == 0) {
        blockBudget = 1;
    } else {
        const int32_t bytesPerSnapshot = (rate * client->snapshotMsec) / 1000;
        int32_t roundedBytes = bytesPerSnapshot + SERVER_DOWNLOAD_BLOCK_SIZE;
        if (roundedBytes < 0) {
            roundedBytes = bytesPerSnapshot + SERVER_DOWNLOAD_BLOCK_SIZE * 2 - 1;
        }
        blockBudget = roundedBytes / SERVER_DOWNLOAD_BLOCK_SIZE;
    }
    if (blockBudget < 0) {
        blockBudget = 1;
    }

    while (blockBudget-- > 0) {
        if (client->download.nextAcknowledgmentBlock == client->download.nextBufferedBlock) {
            return;
        }

        if (client->download.nextTransmitBlock == client->download.nextBufferedBlock) {
            if (svs.realTime - client->download.lastBlockActivityTime <= SERVER_DOWNLOAD_STALL_RETRANSMIT_MSEC) {
                return;
            }
            client->download.nextTransmitBlock = client->download.nextAcknowledgmentBlock;
        }

        const int32_t blockSlot = client->download.nextTransmitBlock % MAX_DOWNLOAD_WINDOW;
        MSG_WriteByte(message, SERVER_SVC_DOWNLOAD);
        MSG_WriteShort(message, client->download.nextTransmitBlock);
        if (client->download.nextTransmitBlock == 0) {
            MSG_WriteLong(message, client->download.fileSize);
        }
        MSG_WriteShort(message, client->download.blockByteCounts[blockSlot]);
        if (client->download.blockByteCounts[blockSlot] != 0) {
            MSG_WriteData(message, client->download.blockData[blockSlot], client->download.blockByteCounts[blockSlot]);
        }

        Com_DPrintf("clientDownload: %d : writing block %d\n", (int32_t)(client - svs.clients), client->download.nextTransmitBlock);
        ++client->download.nextTransmitBlock;
        client->download.lastBlockActivityTime = svs.realTime;
    }
}
