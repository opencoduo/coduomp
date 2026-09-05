#include "cgame.h"
#include "cinematic.h"
#include "console.h"
#include "debug_lines.h"
#include "server_browser.h"

#include "../effects/fx_api.h"
#include "../effects/fx_runtime.h"
#include "qcommon/q_string.h"
#include "filesystem/filesystem.h"
#include "../math/vector_math.h"
#include "qcommon/hunk.h"
#include "qcommon/com_config.h"
#include "../platform/crt_boundary.h"
#include "../platform/punkbuster_boundary.h"
#include "../renderer/renderer_api.h"
#include "../renderer/renderer_cvars.h"
#include "../server/server.h"
#include "../sound/sound_system.h"
#include "../system_event.h"
#include "../system_platform.h"
#include "../ui/ui_client_state.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CL_CONCAT_ARGS_CAPACITY = 1024,
    CL_LOCAL_CONNECT_RESEND_TIME = -3000,
    CL_LOCAL_PACKET_SEND_TIME = -9999,
    CL_AUTHORIZE_SERVER_PORT = 20600,
    CL_CDKEY_CHECKED_LENGTH = 16,
    CL_CDKEY_CHECKSUM_LENGTH = 4,
    CL_CDKEY_AUTH_BUFFER_SIZE = 64,
    CL_CDKEY_AUTH_COPY_LIMIT = 32,
    CL_PURE_COMMAND_FIRST_CHAR_DELTA = 13,
    CL_PURE_COMMAND_SECOND_CHAR_DELTA = 15,
    CL_RCON_PACKET_CAPACITY = 1024,
    CL_RCON_CONNECTIONLESS_MARKER_SIZE = sizeof(int32_t),
    CL_FIRST_COMMAND_ARGUMENT = 1,
    CL_SETENV_ASSIGNMENT_CAPACITY = CMD_TOKEN_BUFFER_CAPACITY,
    CL_DEFAULT_SERVER_PORT = 28960,
    CL_CONNECT_ARGUMENT_COUNT = 2,
    CL_CONNECT_SERVER_ARGUMENT = 1,
    CL_CONNECT_INITIAL_RESEND_TIME = -99999,
    CL_SETENV_NAME_ARGUMENT = 1,
    CL_SETENV_FIRST_VALUE_ARGUMENT = 2,
    CL_VOICE_CHAT_TEXT_CAPACITY = 32,
    CL_VOICE_CHAT_DELAY_MSEC = 250,
    CL_CONNECT_RESEND_MSEC = 3000,
    CL_CONNECT_PACKET_CAPACITY = 1024,
    CL_REF_PRINT_ORIGINAL_BUFFER_CAPACITY = 4096,
    CL_REF_PRINT_BUFFER_CAPACITY = 8192
};

#define CL_MAXPACKETS_DEFAULT_TEXT "30"
#define CL_RATE_DEFAULT_TEXT "25000"
#define CL_SNAPS_DEFAULT_TEXT "20"
#define CL_NOAUTOUPDATE_DEFAULT_TEXT "0"

/* Original Win32 clientStatic_t at 0x04ad3d60..0x04dc464b. */
clientStatic_t cls;
/* Client counterpart to sv_frameRunning. Cgame/render entry paths bracket
 * skeleton work with this flag so common error cleanup can unwind the cache. */
qboolean cl_frameRunning; /* original 0x0389fcf0 */
/* Source: CoDUOMP.exe 0x0389fce4 (.data). Registered as "cl_shownet" by the
 * client initialization path and consumed by message delta diagnostics. */
cvar_t *cl_shownet;

/* Source: CoDUOMP.exe 0x0040f920..0x0040f92a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040f920_0040f92b.mcode.
 * Name: exact same-module Mac symbol CL_CDDialog. The Windows implementation
 * defers presentation until CL_Frame can enter the UI safely. */
void CL_CDDialog(void)
{
    cls.cdDialogRequested = qtrue;
}

/* Client cvar handles populated by CL_Init. These are native pointers; the
 * comments retain their original Win32 storage addresses for binary audits. */
cvar_t *cl_autocmd;                 /* 0x04e19974 */
cvar_t *cl_timeout;                 /* 0x04ad3cdc */
cvar_t *cl_shownuments;             /* 0x04e1998c */
cvar_t *cl_visibleClients;          /* 0x04dc8838 */
cvar_t *cl_showSend;                /* 0x04958074 */
cvar_t *rcon_client_password;       /* 0x04e199a4 */
cvar_t *cl_avidemo;                 /* 0x04958084 */
cvar_t *cl_forceavidemo;            /* 0x04ad3cd8 */
cvar_t *rcon_client_address;        /* 0x04958080 */
cvar_t *cl_yawspeed;                /* 0x04e1cbcc */
cvar_t *cl_pitchspeed;              /* 0x04e1cbe8 */
cvar_t *cl_anglespeedkey;           /* 0x04e1cbf4 */
cvar_t *cl_maxpackets;              /* 0x04df969c */
cvar_t *cl_packetdup;               /* 0x04dc8848 */
cvar_t *cl_run;                     /* 0x04e1cbec */
cvar_t *cl_stance;                  /* 0x04e1cbc0 */
cvar_t *cl_stanceTemp;              /* 0x04e1cbc8 */
cvar_t *cl_goStandJumpTime;         /* 0x04e1cbc4 */
cvar_t *sensitivity;                /* 0x04ad3cd4 */
cvar_t *cl_mouseAccel;              /* 0x04dc8850 */
cvar_t *cl_freelook;                /* 0x04e199a0 */
cvar_t *cl_showmouserate;           /* 0x04dc8834 */
cvar_t *cl_allowDownload;           /* 0x04958070 */
cvar_t *cl_serverAllowDownload;     /* 0x04e19998 */
cvar_t *cl_wwwDownload;             /* 0x04dc883c */
cvar_t *cl_conXOffset;              /* 0x04958064 */
cvar_t *cl_viewPitchCompensate;     /* 0x04e1cbe4 */
cvar_t *cl_viewYawCompensate;       /* 0x04e1cbf8 */
cvar_t *cl_bypassMouseInput;        /* 0x04e1cbdc */
cvar_t *m_pitch;                    /* 0x04ad3ce4 */
cvar_t *m_yaw;                      /* 0x04ad3ce0 */
cvar_t *m_forward;                  /* 0x04dc8844 */
cvar_t *m_side;                     /* 0x04e19960 */
cvar_t *m_filter;                   /* 0x04e1996c */
cvar_t *cl_motdString;              /* 0x04df96a4 */
cvar_t *cl_ingame;                  /* 0x04e19954 */
cvar_t *cl_waitForFire;             /* 0x04dc882c */
cvar_t *cl_updateAvailable;         /* 0x04e1997c */
cvar_t *cl_updateFiles;             /* 0x04dc8828 */
cvar_t *cl_updateOldVersion;        /* 0x04e19988 */
cvar_t *cl_updateVersion;           /* 0x04e19970 */
cvar_t *cl_serverLoadMap;           /* 0x04958190 */
cvar_t *cl_serverLoadGameType;      /* 0x04e19940 */
cvar_t *cl_serverLoadWaiting;       /* 0x04e19944 */
cvar_t *cg_announcerSounds;         /* 0x04dc884c */
cvar_t *cl_executeString;           /* 0x0495807c */

/* Original Win32 CL_Shutdown recursion guard at 0x0389fcec. */
static qboolean cl_shutdownInProgress;

/* Original Win32 ConcatArgs return buffer at 0x008ce4e0. */
static char cl_concatArgs[CL_CONCAT_ARGS_CAPACITY];

/* Source: CoDUOMP.exe 0x004105e0..0x00410671.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004105e0_00410672.mcode.
 * Name and argument: exact same-module Mac symbol ConcatArgs. */
const char *ConcatArgs(int32_t start)
{
    int32_t length = 0;
    const int32_t argumentCount = Cmd_Argc();

    for (int32_t argument = start;
         argument < argumentCount;
         ++argument) {
        const char *const text = Cmd_Argv(argument);
        const int32_t textLength = (int32_t)strlen(text);
        if (length + textLength >= CL_CONCAT_ARGS_CAPACITY - 1)
            break;

        memcpy(&cl_concatArgs[length], text, (size_t)textLength);
        length += textLength;
        if (argument != argumentCount - 1)
            cl_concatArgs[length++] = ' ';
    }

    cl_concatArgs[length] = '\0';
    return cl_concatArgs;
}

/* Source: CoDUOMP.exe 0x00410680..0x00410715, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410680_00410716.mcode.
 * Name and command registration: exact same-module Mac symbol CL_Vsay_f and
 * the original "vsay" registration at 0x00414eef. */
void CL_Vsay_f(void)
{
    const char *const text = ConcatArgs(1);

    if (clc.voiceChatText[0] != '\0' &&
        strcmp(text, clc.voiceChatText) == 0) {
        ++clc.voiceChatRepeatCount;
        clc.voiceChatTime = cls.realTime;
        return;
    }

    clc.voiceChatRepeatCount = 0;
    strncpy(clc.voiceChatText, text,
            sizeof(clc.voiceChatText) - 1);
    clc.voiceChatText[sizeof(clc.voiceChatText) - 1] = '\0';
    clc.voiceChatTime = cls.realTime;
}

/* Source: CoDUOMP.exe 0x00410720..0x004109a9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410720_004109aa.mcode.
 * Name and source-level token-pair selection: exact same-module Mac symbol
 * CL_PlayVoiceChat. The request text contains alternating voice-command token
 * pairs. Repeated identical vsay requests rotate among those pairs after the
 * original 250-millisecond coalescing delay. */
void CL_PlayVoiceChat(void)
{
    char selectedCommand[sizeof(clc.voiceChatText)];
    char *cursor;
    int32_t tokenCount = 0;

    if (clc.voiceChatTime == 0 ||
        clc.voiceChatTime + CL_VOICE_CHAT_DELAY_MSEC >= cls.realTime) {
        return;
    }

    cursor = clc.voiceChatText;
    while (Com_ParseExt(&cursor, qfalse)[0] != '\0')
        ++tokenCount;

    if ((tokenCount & 1) != 0)
        --tokenCount;
    if (tokenCount > 0) {
        const int32_t pairCount = tokenCount / 2;
        clc.voiceChatRepeatCount %= pairCount;

        cursor = clc.voiceChatText;
        for (int32_t pair = 0;
             pair < clc.voiceChatRepeatCount;
             ++pair) {
            (void)Com_ParseExt(&cursor, qfalse);
            (void)Com_ParseExt(&cursor, qfalse);
        }

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        const char *token = Com_ParseExt(&cursor, qfalse);
        size_t selectedLength = strlen(token);

        if (selectedLength > sizeof(selectedCommand) - 2u) {
            Com_Printf("CL_PlayVoiceChat: selected command exceeds source capacity\n");
        } else {
            memcpy(selectedCommand, token, selectedLength);
            selectedCommand[selectedLength++] = ' ';
            token = Com_ParseExt(&cursor, qfalse);
            const size_t tokenLength = strlen(token);

            if (tokenLength > sizeof(selectedCommand) - selectedLength - 1u) {
                Com_Printf("CL_PlayVoiceChat: selected command exceeds source capacity\n");
            } else {
                memcpy(&selectedCommand[selectedLength], token, tokenLength);
                selectedLength += tokenLength;
                selectedCommand[selectedLength] = '\0';
                CL_AddReliableCommand(va("voice \"%s\"", selectedCommand));
            }
        }
    }

    clc.voiceChatTime = 0;
    clc.voiceChatRepeatCount = 0;
    clc.voiceChatText[0] = '\0';
}

/* Source: CoDUOMP.exe 0x004109b0..0x004109c0, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004109b0_004109c1.mcode.
 * Name: exact same-module Mac symbol CL_ClearState. The REP STOSD count
 * proves the complete 0x17bb34-byte clientActive_t extent. */
void CL_ClearState(void)
{
    memset(&cl, 0, sizeof(cl));
}

/* Source: CoDUOMP.exe 0x004109d0..0x004109e6, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004109d0_004109e7.mcode.
 * Name: exact same-module Mac symbol CL_ClearStaticDownload. The individual
 * byte stores clear the three strings without erasing their unused tails. */
void CL_ClearStaticDownload(void)
{
    cls.staticDownload.downloadRestart = qfalse;
    cls.staticDownload.downloadTempName[0] = '\0';
    cls.staticDownload.downloadName[0] = '\0';
    cls.staticDownload.originalDownloadName[0] = '\0';
}

/* NOT_FROM_ORIGINAL_SOURCE: tear down the non-stock isolated server-cache mod
 * at the client boundary shared by manual disconnects, connection errors,
 * demo transitions, and direct server switches. A crash leaves the cache
 * unmounted at the next process start. A graceful exit restores front-end
 * cvars and search paths immediately, then schedules config and hunk users at
 * the next safe command-buffer boundary. */

/* Source: CoDUOMP.exe 0x004109f0..0x00410b54.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004109f0_00410b55.mcode.
 * Name and show-main-menu argument: exact same-module Mac symbol
 * CL_Disconnect. The two REP STOSD operations are expressed through their
 * complete typed owners, clientActive_t and clientConnection_t. */
void CL_Disconnect(qboolean showMainMenu)
{
    if (cl_running == NULL || cl_running->integer == 0)
        return;

    (void)Cvar_Set2("r_uiFullScreen", "1", qtrue);

    if (clc.demoRecording != qfalse)
        CL_StopRecord_f();

    if (cls.wwwDownloadDisconnected == 0) {
        if (clc.downloadFile != 0) {
            FS_FCloseFile(clc.downloadFile);
            clc.downloadFile = 0;
        }
        (void)Cvar_Set2("cl_downloadName", "", qtrue);
    }

    cl_updateStarted = qfalse;
    /* 0x00410a78 clears the standalone update filename at 0x04ad3d00.
     * Preserve cls.serverName so a disconnected WWW download can reconnect
     * to the server that supplied it. */
    cl_updateFileName[0] = '\0';

    if (clc.demoFile != 0) {
        FS_FCloseFile(clc.demoFile);
        clc.demoFile = 0;
    }

    if (coduo_uiVm != NULL && showMainMenu != qfalse) {
        (void)VM_Call(coduo_uiVm, UIVM_SET_ACTIVE_MENU,
                      0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 0);
    }

    SCR_StopCinematic();

    if (cls.state >= CA_CONNECTED &&
        clc.reliableSequence - clc.reliableAcknowledge <=
            CODUO_RELIABLE_COMMAND_COUNT) {
        CL_AddReliableCommand("disconnect");
        CL_WritePacket();
        CL_WritePacket();
        CL_WritePacket();
    }

    CL_ClearState();
    memset(&clc, 0, sizeof(clc));
    if (cls.wwwDownloadDisconnected == 0)
        CL_ClearStaticDownload();

    cls.state = CA_DISCONNECTED;
    cl_connectedToPureServer = qfalse;
    fs_checksumFeed = 0;
}

/* Source: CoDUOMP.exe 0x00410b60..0x00410baf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410b60_00410bb0.mcode.
 * Name and string argument: exact same-module Mac symbol
 * CL_ForwardCommandToServer. */
void CL_ForwardCommandToServer(const char *command)
{
    const int32_t argumentCount = Cmd_Argc();
    const char *const commandName =
        argumentCount > 0 ? Cmd_Argv(0) : "";

    if (commandName[0] == '-')
        return;

    if (clc.demoPlayback != qfalse ||
        cls.state < CA_CONNECTED ||
        commandName[0] == '+') {
        Com_Printf("Unknown command \"%s\"\n", commandName);
        return;
    }

    CL_AddReliableCommand(
        argumentCount > 1 ? command : commandName);
}

/* Source: CoDUOMP.exe 0x00410da0..0x00410ddb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410da0_00410ddc.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_ForwardToServer_f. */
void CL_ForwardToServer_f(void)
{
    if (cls.state != CA_ACTIVE || clc.demoPlayback != qfalse) {
        Com_Printf("Not connected to a server.\n");
        return;
    }

    if (Cmd_Argc() > CL_FIRST_COMMAND_ARGUMENT) {
        CL_AddReliableCommand(
            Cmd_Args(CL_FIRST_COMMAND_ARGUMENT));
    }
}

/* Source: CoDUOMP.exe 0x00410de0..0x00410f22.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410de0_00410f23.mcode.
 * Name and no-argument signature: exact same-module Mac symbol CL_Setenv_f.
 * The original command deliberately leaves one trailing space after a
 * multi-word value and uses the MSVC _putenv copying contract. */
void CL_Setenv_f(void)
{
    const int32_t argumentCount = Cmd_Argc();

    if (argumentCount > CL_SETENV_FIRST_VALUE_ARGUMENT) {
        char assignment[CL_SETENV_ASSIGNMENT_CAPACITY];
        const char *const name = Cmd_Argv(CL_SETENV_NAME_ARGUMENT);
        const size_t nameLength = strlen(name);
        size_t assignmentLength;

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (nameLength > sizeof(assignment) - 2u) {
            Com_Printf("setenv: assignment exceeds command capacity\n");
            return;
        }
        memcpy(assignment, name, nameLength);
        assignmentLength = nameLength;
        assignment[assignmentLength++] = '=';
        for (int32_t argumentIndex = CL_SETENV_FIRST_VALUE_ARGUMENT;
             argumentIndex < argumentCount;
             ++argumentIndex) {
            const char *const value = Cmd_Argv(argumentIndex);
            const size_t valueLength = strlen(value);
            const size_t remainingCapacity = sizeof(assignment) - assignmentLength;

            if (remainingCapacity < 2u || valueLength > remainingCapacity - 2u) {
                Com_Printf("setenv: assignment exceeds command capacity\n");
                return;
            }
            memcpy(&assignment[assignmentLength], value, valueLength);
            assignmentLength += valueLength;
            assignment[assignmentLength++] = ' ';
        }
        assignment[assignmentLength] = '\0';

        (void)coduomp_crt_putenv_copy(assignment);
        return;
    }

    if (argumentCount == CL_SETENV_FIRST_VALUE_ARGUMENT) {
        const char *const name = Cmd_Argv(CL_SETENV_NAME_ARGUMENT);
        const char *const value = getenv(name);

        if (value != NULL)
            Com_Printf("%s=%s\n", name, value);
        else
            Com_Printf("%s undefined\n", name);
    }
}

/* Source: CoDUOMP.exe 0x00410f30..0x00410f7a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410f30_00410f7b.mcode.
 * Name and command binding: exact same-module Mac symbol CL_Disconnect_f.
 * SCR_StopCinematic contains the same handle bounds check, cinematic stop,
 * all-sounds stop, and handle reset that MSVC inlined here. */
void CL_Disconnect_f(void)
{
    SCR_StopCinematic();

    if (cls.state != CA_DISCONNECTED &&
        cls.state != CA_CINEMATIC &&
        cls.state != CA_LOGO) {
        Com_Error(ERR_DISCONNECT,
                  "EXE_DISCONNECTED_FROM_SERVER");
    }
}

/* Source: CoDUOMP.exe 0x00410f80..0x00410fdb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410f80_00410fdc.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_Reconnect_f. Reconnecting to the local listen-server name is rejected;
 * all other remembered server names are requeued through the command buffer. */
void CL_Reconnect_f(void)
{
    if (cls.serverName[0] != '\0' &&
        strcmp(cls.serverName, "localhost") != 0) {
        Cbuf_AddText(va("connect %s\n", cls.serverName));
        return;
    }

    Com_Printf("Can't reconnect to localhost.\n");
}

/* Source: CoDUOMP.exe 0x00410fe0..0x004111b8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410fe0_004111b9.mcode.
 * Name and no-argument signature: exact same-module Mac symbol CL_Connect_f.
 * The client first tears down any local listen server and old connection,
 * resolves the requested address, validates the retail key for non-local
 * servers, then primes the challenge resend state. */
void CL_Connect_f(void)
{
    if (Cmd_Argc() != CL_CONNECT_ARGUMENT_COUNT) {
        Com_Printf("usage: connect [server]\n");
        return;
    }

    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    (void)Cvar_Set2("r_uiFullScreen", "0", qtrue);
    clc.serverMessage[0] = '\0';

    const char *const server = Cmd_Argv(CL_CONNECT_SERVER_ARGUMENT);
    if (sv_running->integer != 0 &&
        strcmp(server, "localhost") == 0) {
        Com_Shutdown("EXE_SERVERQUIT");
    }

    (void)Cvar_Set2("sv_killserver", "1", qtrue);
    SV_Frame(0);
    CL_Disconnect(qtrue);
    Con_Close();

    strncpy(cls.serverName, server, sizeof(cls.serverName) - 1);
    cls.serverName[sizeof(cls.serverName) - 1] = '\0';

    if (NET_StringToAdr(cls.serverName, &clc.serverAddress) == qfalse) {
        Com_Printf("Bad server address\n");
        cls.state = CA_DISCONNECTED;
        (void)Cvar_Set2("ui_dl_running", "0", qtrue);
        return;
    }

    if (clc.serverAddress.port == 0) {
        clc.serverAddress.port = (uint16_t)BigShort(
            (int16_t)CL_DEFAULT_SERVER_PORT);
    }

    Com_Printf("%s resolved to %i.%i.%i.%i:%i\n",
               cls.serverName,
               clc.serverAddress.ip[0],
               clc.serverAddress.ip[1],
               clc.serverAddress.ip[2],
               clc.serverAddress.ip[3],
               (int32_t)(int16_t)BigShort(
                   (int16_t)clc.serverAddress.port));

    if (clc.serverAddress.type != NA_LOOPBACK &&
        clc.serverAddress.type != NA_BAD &&
        CL_CDKeyValidate(
            &cl_cdkey[CL_PRIMARY_CDKEY_OFFSET],
            &cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET]) == qfalse) {
        Com_Error(ERR_DROP, "EXE_ERR_INVALID_CD_KEY");
    }

    if (clc.serverAddress.type == NA_LOOPBACK ||
        clc.serverAddress.type == NA_BAD) {
        cls.state = CA_CHALLENGING;
    } else {
        cls.state = CA_CONNECTING;
    }

    cls.keyCatchers = 0;
    clc.connectTime = CL_CONNECT_INITIAL_RESEND_TIME;
    clc.connectPacketCount = 0;
    (void)Cvar_Set2(
        "cl_currentServerAddress", server, qtrue);
}

/* Source: CoDUOMP.exe 0x004111c0..0x00411445.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004111c0_00411446.mcode.
 * Name and no-argument signature: exact same-module Mac symbol CL_Rcon_f.
 * Arguments containing control/space characters, including an empty
 * argument, are quoted as one remote-console token. The bounded append loops
 * preserve the original all-or-nothing 1024-byte packet limit. */
void CL_Rcon_f(void)
{
    char message[CL_RCON_PACKET_CAPACITY];
    int32_t messageLength = CL_RCON_CONNECTIONLESS_MARKER_SIZE;
    netadr_t destination;

    /* An empty cvar still permits the password as the first command argument. */
    if (rcon_client_password->string == NULL) {
        Com_Printf(
            "You must set 'rcon_password' before\n"
            "issuing an rcon command.\n");
        return;
    }

    memset(message, UINT8_MAX, CL_RCON_CONNECTIONLESS_MARKER_SIZE);
    message[messageLength] = '\0';

    const char *source = "rcon ";
    while (*source != '\0' &&
           messageLength < CL_RCON_PACKET_CAPACITY) {
        message[messageLength++] = *source++;
    }

    source = rcon_client_password->string;
    while (*source != '\0' &&
           messageLength < CL_RCON_PACKET_CAPACITY) {
        message[messageLength++] = *source++;
    }

    for (int32_t argumentIndex = CL_FIRST_COMMAND_ARGUMENT;
         argumentIndex < Cmd_Argc();
         ++argumentIndex) {
        source = " ";
        while (*source != '\0' &&
               messageLength < CL_RCON_PACKET_CAPACITY) {
            message[messageLength++] = *source++;
        }

        const char *const argument = Cmd_Argv(argumentIndex);
        const int32_t remainingCapacity =
            CL_RCON_PACKET_CAPACITY - messageLength;
        qboolean quoteArgument =
            argument[0] == '\0' ? qtrue : qfalse;

        for (int32_t offset = 0;
             quoteArgument == qfalse &&
             offset < remainingCapacity &&
             argument[offset] != '\0';
             ++offset) {
            if ((int8_t)(uint8_t)argument[offset] <= ' ')
                quoteArgument = qtrue;
        }

        if (quoteArgument != qfalse &&
            messageLength < CL_RCON_PACKET_CAPACITY) {
            message[messageLength++] = '"';
        }

        source = argument;
        while (*source != '\0' &&
               messageLength < CL_RCON_PACKET_CAPACITY) {
            message[messageLength++] = *source++;
        }

        if (quoteArgument != qfalse &&
            messageLength < CL_RCON_PACKET_CAPACITY) {
            message[messageLength++] = '"';
        }
    }

    if (messageLength == CL_RCON_PACKET_CAPACITY) {
        Com_Printf("rcon command too long\n");
        return;
    }
    message[messageLength] = '\0';

    if (cls.state >= CA_CONNECTED) {
        destination = clc.netchan.remoteAddress;
    } else {
        if (rcon_client_address->string[0] == '\0') {
            Com_Printf(
                "You must either be connected,\n"
                "or set the 'rconAddress' cvar\n"
                "to issue rcon commands\n");
            return;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (NET_StringToAdr(rcon_client_address->string, &destination) ==
            qfalse) {
            Com_Printf("Bad rcon address\n");
            return;
        }
        if (destination.port == 0) {
            destination.port = (uint16_t)BigShort(
                (int16_t)CL_DEFAULT_SERVER_PORT);
        }
    }

    CL_Netchan_SendOOBPacket(
        destination, message, (int32_t)strlen(message) + 1);
}

/* Source: CoDUOMP.exe 0x00411450..0x0041153c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411450_0041153d.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_SendPureChecksums. The source deliberately builds the command prefix as
 * "Va " and adjusts its first two bytes to "cp" only after appending the
 * checksum text; the trailing space is byte-proven at .rdata 0x005946aa and
 * separates the command name from its first checksum. Preserve that ordering
 * before the inlined CL_AddReliableCommand. */
void CL_SendPureChecksums(void)
{
    const char *const checksums = FS_ReferencedPakPureChecksums();
    char command[MAX_STRING_CHARS];

    Com_sprintf(command, sizeof(command), "Va ");

    const size_t prefixLength = strlen(command);
    Q_strncpyz(command + prefixLength, checksums,
               (int32_t)(sizeof(command) - prefixLength));

    command[0] += CL_PURE_COMMAND_FIRST_CHAR_DELTA;
    command[1] += CL_PURE_COMMAND_SECOND_CHAR_DELTA;

    CL_AddReliableCommand(command);
}

/* Source: CoDUOMP.exe 0x00411540..0x00411553, recovered from an exporter
 * function-boundary gap. The Windows optimizer also inlines this source helper
 * at CL_Vid_Restart_f 0x00411665..0x00411674. The exact same-module Mac symbol
 * CL_ResetPureClientAtServer identifies the boundary; both Windows copies
 * prove the original "vdr" reliable command and va call. */
void CL_ResetPureClientAtServer(void)
{
    CL_AddReliableCommand(va("vdr"));
}

/* Source: CoDUOMP.exe 0x00413b70..0x00413ba1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413b70_00413ba2.mcode.
 * Name and signature: exact same-module Mac symbol CL_ShutdownRef. The Windows
 * optimizer also inlines this source helper at CL_Vid_Restart_f
 * 0x00411635..0x00411664 and Sys_Shutdown. The renderer export-table and
 * StatMon clears occur only when a renderer shutdown callback is installed. */
void CL_ShutdownRef(void)
{
    if (rendererExports.Shutdown == NULL)
        return;

    rendererExports.Shutdown(qtrue);
    memset(&rendererExports, 0, sizeof(rendererExports));
    StatMon_Reset();
}

/* Source: CoDUOMP.exe 0x00413ab0..0x00413b63.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413ab0_00413b64.mcode.
 * Name and variadic signature: exact same-module Mac symbol CL_RefPrintf.
 * The Windows body formats into its 4096-byte stack buffer before selecting
 * the ordinary, developer, or warning print prefix from printLevel. */
void CL_RefPrintf(int32_t printLevel, const char *format, ...)
{
#if defined(_WIN32)
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    char message[CL_REF_PRINT_ORIGINAL_BUFFER_CAPACITY];
#else
    /* NOT_FROM_ORIGINAL_SOURCE: the original buffer was 4096 bytes. Native
     * OpenGL drivers can report extension strings larger than that, so the SDL
     * portability path carries additional formatting headroom. */
    char message[CL_REF_PRINT_BUFFER_CAPACITY];
#endif
    va_list arguments;

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    switch (printLevel) {
    case R_PRINT_ALL:
        Com_Printf("%s", message);
        break;
    case R_PRINT_DEVELOPER:
        Com_DPrintf("^1%s", message);
        break;
    case R_PRINT_WARNING:
        Com_Printf("^3%s", message);
        break;
    }
}

/* Source: CoDUOMP.exe 0x00413f40..0x00413f55, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413f40_00413f56.mcode.
 * Name and short model-index argument: exact same-module Mac symbol
 * CG_GetGameModel. */
int32_t CG_GetGameModel(int16_t modelIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)(int32_t)modelIndex >= (uint32_t)CS_MODELS_COUNT) {
        return 0;
    }

    return (int32_t)VM_Call(
        coduo_cgameVm, CGVM_GET_MODEL_HANDLE, modelIndex,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x00413f60..0x00413f7e, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413f60_00413f7f.mcode.
 * Name and three forwarded arguments: exact same-module Mac symbol
 * CG_DObjCalcPose. */
void CG_DObjCalcPose(void *owner, struct DObj_s *obj,
                     uint32_t *partBits)
{
    (void)VM_Call(
        coduo_cgameVm, CGVM_DOBJ_CALC_POSE,
        (intptr_t)owner, (intptr_t)obj, (intptr_t)partBits,
        0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x00413f80..0x00413fa4, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413f80_00413fa5.mcode.
 * Name and arguments: exact same-module Mac symbol CL_GetFontInfo. The
 * Windows x87 multiply uses the exact 100.0f constant before truncating the
 * scale to the integer hundredths consumed by UIVM_GET_FONT. */
fontInfo_t *CL_GetFontInfo(int32_t fontHandle, float scale)
{
    const int32_t scaleHundredths = (int32_t)(scale * 100.0f);

    return (fontInfo_t *)VM_Call(
        coduo_uiVm, UIVM_GET_FONT, fontHandle, scaleHundredths,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Source: CoDUOMP.exe 0x00413fb0..0x00414164.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413fb0_00414165.mcode.
 * Name and renderer API boundary: exact same-module Mac symbol CL_InitRef.
 * Windows constructs all 38 imports, requests API version 14, then copies all
 * 59 returned exports into the client-owned renderer table. */
void CL_InitRef(void)
{
    refimport_t imports = {
        .Printf = CL_RefPrintf,
        .Error = Com_Error,
        .Milliseconds = CL_ScaledMilliseconds,
        .Hunk_Alloc = Hunk_AllocInternal,
        .Hunk_AllocateTempMemory = Hunk_AllocateTempMemoryInternal,
        .Z_Malloc = Z_MallocInternal,
        .Z_Free = Z_FreeInternal,
        .Hunk_FreeTempMemory = Hunk_FreeTempMemory,
        .Cvar_Get = Cvar_Get,
        .Cvar_FindVar = Cvar_FindVar,
        .Cvar_Set = Cvar_Set,
        .Cmd_AddCommand = Cmd_AddCommand,
        .Cmd_RemoveCommand = Cmd_RemoveCommand,
        .Cmd_Argc = Cmd_Argc,
        .Cmd_Argv = Cmd_Argv,
        .Cmd_ExecuteText = Cbuf_ExecuteText,
        .Com_SaveCvarsToBuffer = Com_SaveCvarsToBuffer,
        .Com_LoadCvarsFromBuffer = Com_LoadCvarsFromBuffer,
        .FS_FileIsInPAK = FS_FileIsInPAK,
        .FS_ReadFile = FS_ReadFile,
        .FS_FreeFile = FS_FreeFile,
        .FS_ListFiles = FS_ListFiles,
        .FS_FreeFileList = FS_FreeFileList,
        .FS_WriteFile = FS_WriteFile,
        .FS_FileExists = FS_FileExists,
        .FS_FOpenFileRead = FS_FOpenFileRead,
        .FS_FCloseFile = FS_FCloseFile,
        .FS_Read = FS_Read,
        .FS_Write = FS_Write,
        .CM_SaveLump = CM_SaveLump,
        .CM_PlaneForIndex = CM_PlaneForIndex,
        .CIN_UploadCinematic = CIN_UploadCinematic,
        .CIN_PlayCinematic = CIN_PlayCinematic,
        .CIN_RunCinematic = CIN_RunCinematic,
        .CG_GetGameModel = CG_GetGameModel,
        .CG_DObjCalcPose = CG_DObjCalcPose,
        .AdjustFrom640 = SCR_AdjustFrom640,
        .CL_GetFontInfo = CL_GetFontInfo
    };

    Com_Printf("----- Initializing Renderer ----\n");
    refexport_t *const exports =
        GetRefAPI(RENDERER_API_VERSION, &imports);
    Com_Printf("-------------------------------\n");

    if (exports == NULL)
        Com_Error(0, "EXE_ERR_COULDNT_INIT_REFRESH");

    memcpy(&rendererExports, exports, sizeof(rendererExports));
    (void)Cvar_Set2("cl_paused", "0", qtrue);
}

/* Source: CoDUOMP.exe 0x00414170..0x00414181, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00414170_00414182.mcode.
 * Name and command binding: exact same-module Mac symbol
 * CL_startSingleplayer_f. */
void CL_startSingleplayer_f(void)
{
    Sys_StartProcess("CoDUOSP.exe", qtrue);
}

/* Source: CoDUOMP.exe 0x00414190..0x004141a0, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00414190_004141a1.mcode.
 * Name and value argument: exact same-module Mac symbol set_cl_punkbuster. */
void set_cl_punkbuster(const char *value)
{
    (void)Cvar_Set2("cl_punkbuster", value, qtrue);
}

/* Source: CoDUOMP.exe 0x004141b0..0x004142e4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004141b0_004142e5.mcode.
 * Name and no-argument signature: exact same-module Mac symbol CL_DrawLogo.
 * The logo is registered as two vertically stacked images. Its split is two
 * thirds of the video height, using the executable's exact 0x3eaaaaab float
 * (nominally one third) after adding the height to itself. */
void CL_DrawLogo(void)
{
    const int32_t elapsed = cls.realTime - cls.logoStartTime;
    long double alpha;

    if (elapsed < cls.logoFadeInDuration) {
        alpha = (long double)elapsed /
                (long double)cls.logoFadeInDuration;
    } else if (elapsed >
               cls.logoTotalDuration - cls.logoFadeOutDuration) {
        alpha =
            (long double)(cls.logoTotalDuration - elapsed) /
            (long double)cls.logoFadeOutDuration;
    } else {
        alpha = 1.0f;
    }

    if (alpha > 1.0f)
        alpha = 1.0f;
    else if (alpha < 0.0f)
        alpha = 0.0f;

    const float storedAlpha = (float)alpha;
    const vec4_t color = {
        storedAlpha, storedAlpha, storedAlpha, 1.0f
    };
    const float width = (float)cls.rendererConfig.vidWidth;
    const float height = (float)cls.rendererConfig.vidHeight;
    const float splitY = (float)(
        ((long double)cls.rendererConfig.vidHeight +
         (long double)cls.rendererConfig.vidHeight) *
        (long double)0.3333333432674408f);
    const float bottomHeight =
        (float)((long double)height - (long double)splitY);

    rendererExports.SetColor(color);
    rendererExports.StretchPic(
        0.0f, 0.0f, width, splitY,
        0.0f, 0.0f, 1.0f, 1.0f, cls.logoShaderTop);
    rendererExports.StretchPic(
        0.0f, splitY, width, bottomHeight,
        0.0f, 0.0f, 1.0f, 1.0f, cls.logoShaderBottom);
    rendererExports.SetColor(NULL);

    if (elapsed > cls.logoTotalDuration)
        cls.state = CA_DISCONNECTED;
}

/* Source: CoDUOMP.exe 0x004142f0..0x004142fa, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004142f0_004142fb.mcode.
 * Name and no-argument signature: exact same-module Mac symbol CL_StopLogo. */
void CL_StopLogo(void)
{
    cls.state = CA_DISCONNECTED;
}

/* Source: CoDUOMP.exe 0x00414300..0x004144f7, recovered after repairing the
 * missing Ghidra function boundary.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00414300_004144f8.mcode.
 * Name and command syntax: exact same-module Mac symbol CL_PlayLogo_f. The
 * Windows optimizer inlines SCR_StopCinematic and the one-millisecond
 * MSS_FadeAllSounds reset before entering logo state. */
void CL_PlayLogo_f(void)
{
    enum {
        CL_LOGO_ARGUMENT_COUNT = 5,
        CL_LOGO_SHADER_LOAD_MODE = 2,
        CL_LOGO_START_DELAY_MSEC = 100,
        CL_LOGO_AUDIO_FADE_MSEC = 1
    };

    if (Cmd_Argc() != CL_LOGO_ARGUMENT_COUNT) {
        Com_Printf(
            "USAGE: logo <image name> <fadein seconds> "
            "<full duration seconds> <fadeout seconds>\n");
        return;
    }

    Com_DPrintf("CL_PlayLogo_f\n");

    if (cls.state == CA_CINEMATIC) {
        SCR_StopCinematic();
    } else if (cls.state != CA_LOGO &&
               cls.state != CA_DISCONNECTED) {
        return;
    }

    cls.state = CA_LOGO;
    if (coduo_uiVm != NULL) {
        (void)VM_Call(
            coduo_uiVm, UIVM_SET_ACTIVE_MENU, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    MSS_FadeAllSounds(1.0f, CL_LOGO_AUDIO_FADE_MSEC);

    const char *const imageName = Cmd_Argv(1);
    cls.logoFadeInDuration = FastRound(
        (float)(atof(Cmd_Argv(2)) * 1000.0));
    const int32_t fullDuration = FastRound(
        (float)(atof(Cmd_Argv(3)) * 1000.0));
    cls.logoFadeOutDuration = FastRound(
        (float)(atof(Cmd_Argv(4)) * 1000.0));
    cls.logoTotalDuration =
        cls.logoFadeInDuration + fullDuration +
        cls.logoFadeOutDuration;

    cls.logoShaderTop = rendererExports.RegisterShaderNoMip(
        va("%s1", imageName), CL_LOGO_SHADER_LOAD_MODE);
    cls.logoShaderBottom = rendererExports.RegisterShaderNoMip(
        va("%s2", imageName), CL_LOGO_SHADER_LOAD_MODE);
    cls.logoStartTime = cls.realTime + CL_LOGO_START_DELAY_MSEC;
}

/* Source: CoDUOMP.exe 0x00411560..0x00411793.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411560_00411794.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_Vid_Restart_f. MSVC inlines CL_ResetPureClientAtServer,
 * FS_ClearPakReferences, CL_ShutdownRef, and much of Hunk_Clear into this
 * range; the maintained source preserves those original subsystem boundaries.
 */
void CL_Vid_Restart_f(void)
{
    uint8_t *savedCgameState = NULL;
    int32_t savedCgameStateSize = 0;

    if (sv_running->integer != 0) {
        Com_Printf("Listen server cannot video restart.\n");
        return;
    }

    if (coduo_cgameVm != NULL) {
        uint8_t *const temporaryState = Hunk_AllocateTempMemoryInternal(0);
        savedCgameStateSize =
            CL_SaveCgameState(
                (int32_t)Hunk_MemoryRemaining(), temporaryState);

        savedCgameState = Z_MallocInternal((size_t)savedCgameStateSize);
        memset(savedCgameState, 0, (size_t)savedCgameStateSize);
        memcpy(savedCgameState, temporaryState,
               (size_t)savedCgameStateSize);
        Hunk_FreeTempMemory(temporaryState);
    }

    Cvar_Set("com_expectedhunkusage", "-1");
    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    CL_ShutdownCGame();
    CL_ShutdownUI();
    CL_ShutdownRef();
    CL_ResetPureClientAtServer();
    FS_ClearPakReferences(qtrue);
    Hunk_ClearToStart();

    (void)Cvar_Get(
        "cl_language", "0",
        CVAR_ARCHIVE | CVAR_LATCH);
    (void)Cvar_Get(
        "cl_languagetranslate", "1", CVAR_LATCH);
    (void)Cvar_Get(
        "fs_ignoreLocalized", "0",
        CVAR_LATCH | CVAR_CHEAT);

    if (sv_running->integer == 0)
        FS_ConditionalRestart(clc.checksumFeed);
    SEH_UpdateLanguageInfo();

    cls.rendererStarted = qfalse;
    cls.uiStarted = qfalse;
    Cvar_Set("cl_paused", "0");
    CL_InitRef();
    CL_StartHunkUsers();

    if (cls.state > CA_CONNECTED &&
        cls.state != CA_CINEMATIC &&
        cls.state != CA_LOGO) {
        CL_InitCGame();
        CL_SendPureChecksums();
    }

    if (savedCgameState != NULL) {
        if (coduo_cgameVm != NULL) {
            (void)CL_RestoreCgameState(
                savedCgameStateSize, savedCgameState);
        }
        Z_FreeInternal(savedCgameState);
    }
}

/* Source: CoDUOMP.exe 0x00413c30..0x00413cb3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413c30_00413cb4.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_StartHunkUsers. The three started flags independently gate renderer,
 * Miles, and UI initialization, with a Windows event pump after each newly
 * initialized subsystem. */
void CL_StartHunkUsers(void)
{
    if (cl_running == NULL || cl_running->integer == 0)
        return;

    XModelEnforceExist(
        Cvar_Get("cl_xmodelcheck", "0",
                 CVAR_ARCHIVE | CVAR_LATCH)->integer);

    if (cls.rendererStarted == qfalse) {
        cls.rendererStarted = qtrue;
        CL_InitRenderer();
        Sys_PumpEvents();
    }

    if (cls.soundStarted == qfalse) {
        cls.soundStarted = qtrue;
        MSS_Init();
        Sys_PumpEvents();
    }

    if (cls.uiStarted == qfalse) {
        cls.uiStarted = qtrue;
        CL_InitUI();
        Sys_PumpEvents();
    }
}

/* Source: CoDUOMP.exe 0x004117a0..0x00411846, after repairing the Ghidra
 * boundary that began its record at the mid-function address 0x004117bc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004117a0_00411847.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_Snd_Restart_f. Sound state is serialized through temporary hunk memory,
 * copied to zone memory across the full sound/video restart, then restored. */
void CL_Snd_Restart_f(void)
{
    if (sv_running->integer != 0) {
        Com_Printf("Listen server cannot sound restart.\n");
        return;
    }

    uint8_t *const temporarySave =
        Hunk_AllocateTempMemoryInternal(0);
    const int32_t saveSize =
        MSS_Save(temporarySave, (int32_t)Hunk_MemoryRemaining());

    uint8_t *const retainedSave = Z_MallocInternal((size_t)saveSize);
    memset(retainedSave, 0, (size_t)saveSize);
    memcpy(retainedSave, temporarySave, (size_t)saveSize);
    Hunk_FreeTempMemory(temporarySave);

    MSS_Shutdown();
    MSS_Init();
    CL_Vid_Restart_f();

    MSS_Restore(retainedSave, saveSize);
    Z_FreeInternal(retainedSave);
}

/* Source: CoDUOMP.exe 0x00411850..0x00411863.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411850_00411864.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_OpenedPK3List_f. */
void CL_OpenedPK3List_f(void)
{
    Com_Printf("Opened PK3 Names: %s\n", FS_LoadedPakNames());
}

/* Source: CoDUOMP.exe 0x00411870..0x00411883.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411870_00411884.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_ReferencedPK3List_f. */
void CL_ReferencedPK3List_f(void)
{
    Com_Printf("Referenced PK3 Names: %s\n",
               FS_ReferencedPakNames());
}

/* Source: CoDUOMP.exe 0x00411890..0x004118da.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411890_004118db.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_Configstrings_f. */
void CL_Configstrings_f(void)
{
    if (cls.state != CA_ACTIVE) {
        Com_Printf("Not connected to a server.\n");
        return;
    }

    for (int32_t index = 0;
         index < MAX_CONFIGSTRINGS;
         ++index) {
        const int32_t offset = cl.gameState.stringOffsets[index];
        if (offset != 0) {
            Com_Printf("%4i: %s\n", index,
                       &cl.gameState.stringData[offset]);
        }
    }
}

/* Source: CoDUOMP.exe 0x004118e0..0x00411933.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004118e0_00411934.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_Clientinfo_f. */
void CL_Clientinfo_f(void)
{
    Com_Printf("--------- Client Information ---------\n");
    Com_Printf("state: %i\n", cls.state);
    Com_Printf("Server: %s\n", cls.serverName);
    Com_Printf("User info settings:\n");
    Info_Print(Cvar_InfoString(CVAR_USERINFO));
    Com_Printf("--------------------------------------\n");
}

/* Source: CoDUOMP.exe 0x00416f40..0x00416fbe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416f40_00416fbf.mcode.
 * Name and two string arguments: exact same-module Mac symbol
 * CL_CDKeyValidate. The executable sign-extends each key byte before folding
 * it into the checksum, then uses a logical right shift. */
qboolean CL_CDKeyValidate(const char *key, const char *checksum)
{
    uint32_t value = 0;

    for (int32_t keyIndex = 0;
         keyIndex < CL_CDKEY_CHECKED_LENGTH;
         ++keyIndex) {
        value ^= (uint32_t)(int32_t)(int8_t)key[keyIndex];

        for (int32_t bit = 0; bit < 8; ++bit) {
            if ((value & 1u) != 0)
                value ^= 0x14002u;
            value >>= 1;
        }
    }

    char calculatedChecksum[CL_CDKEY_CHECKSUM_LENGTH + 1];
    coduo_crt_snprintf(calculatedChecksum, sizeof(calculatedChecksum),
                         "%04x", value);

    if (checksum != NULL &&
        Q_stricmpn(calculatedChecksum, checksum,
                   CL_CDKEY_CHECKSUM_LENGTH) != 0) {
        return qfalse;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00410bb0..0x00410d9e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00410bb0_00410d9f.mcode.
 * Name and behavior: exact same-module Mac symbol CL_RequestAuthorization.
 * The Windows body performs the same CD-key validation, address resolution,
 * sanitization, and authorization request. */
void CL_RequestAuthorization(void)
{
    static const char authorizeHost[] =
        "coduoauthorize.activision.com";

    if (CL_CDKeyValidate(
            &cl_cdkey[CL_PRIMARY_CDKEY_OFFSET],
            &cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET]) == qfalse) {
        Com_Error(1, "EXE_ERR_INVALID_CD_KEY");
        return;
    }

    if (cls.cdAuthorizeAddress.port == 0) {
        Com_Printf("Resolving %s\n", authorizeHost);
        if (NET_StringToAdr(authorizeHost,
                            &cls.cdAuthorizeAddress) == qfalse) {
            Com_Printf("Couldn't resolve address\n");
            return;
        }

        cls.cdAuthorizeAddress.port =
            (uint16_t)BigShort((int16_t)CL_AUTHORIZE_SERVER_PORT);
        Com_Printf("%s resolved to %i.%i.%i.%i:%i\n",
                   authorizeHost,
                   cls.cdAuthorizeAddress.ip[0],
                   cls.cdAuthorizeAddress.ip[1],
                   cls.cdAuthorizeAddress.ip[2],
                   cls.cdAuthorizeAddress.ip[3],
                   (int32_t)BigShort(
                       (int16_t)cls.cdAuthorizeAddress.port));
    }

    if (cls.cdAuthorizeAddress.type == NA_BOT)
        return;

    char sanitizedKey[CL_CDKEY_AUTH_BUFFER_SIZE];
    const cvar_t *const restrictFilesystem =
        Cvar_FindVar("fs_restrict");

    if (restrictFilesystem != NULL &&
        restrictFilesystem->value != 0.0f) {
        strncpy(sanitizedKey, "demo", CL_CDKEY_AUTH_BUFFER_SIZE - 1);
        sanitizedKey[CL_CDKEY_AUTH_BUFFER_SIZE - 1] = '\0';
    } else {
        size_t sourceLength = strlen(cl_cdkey);
        if (sourceLength > CL_CDKEY_AUTH_COPY_LIMIT)
            sourceLength = CL_CDKEY_AUTH_COPY_LIMIT;

        size_t destinationLength = 0;
        for (size_t index = 0; index < sourceLength; ++index) {
            const char character = cl_cdkey[index];
            if ((character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z')) {
                sanitizedKey[destinationLength++] = character;
            }
        }
        sanitizedKey[destinationLength] = '\0';
    }

    const cvar_t *const anonymous =
        Cvar_Get("cl_anonymous", "", CVAR_SYSTEMINFO | CVAR_INIT);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    NET_OutOfBandPrint(
        NS_CLIENT, cls.cdAuthorizeAddress,
        "%s", va("getKeyAuthorize %i %s", anonymous->integer, sanitizedKey));
}

/* Source: CoDUOMP.exe 0x00411e60..0x004121c8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411e60_004121c9.mcode.
 * Name and source-level packet construction: exact same-module Mac symbol
 * CL_CheckForResend. Windows additionally exposes the two optional
 * PbClientConnecting notifications retained behind the modern PB boundary. */
void CL_CheckForResend(void)
{
    if (clc.demoPlayback != qfalse)
        return;

    if (cls.state != CA_CONNECTING &&
        cls.state != CA_CHALLENGING) {
        return;
    }

    if ((int32_t)((uint32_t)cls.realtime -
                  (uint32_t)clc.connectTime) <
        CL_CONNECT_RESEND_MSEC) {
        return;
    }

    clc.connectTime = cls.realtime;
    clc.connectPacketCount = (int32_t)(
        (uint32_t)clc.connectPacketCount + 1u);

    switch (cls.state) {
    case CA_CONNECTING: {
        if (net_lanauthorize->integer != 0 ||
            Sys_IsLANAddress(clc.serverAddress) == qfalse) {
            CL_RequestAuthorization();
        }

        char challengePacket[CL_CONNECT_PACKET_CAPACITY];
        strcpy(challengePacket, "getchallenge");
        int32_t challengeLength =
            (int32_t)strlen(challengePacket);
        PbClientConnecting(
            PB_CLIENT_CONNECTING_CHALLENGE,
            challengePacket, &challengeLength);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        NET_OutOfBandPrint(
            NS_CLIENT, clc.serverAddress, "%s", challengePacket);
        return;
    }

    case CA_CHALLENGING: {
        const int32_t qport =
            (int32_t)Cvar_VariableValue("net_qport");

        char info[CL_CONNECT_PACKET_CAPACITY];
        Q_strncpyz(
            info, Cvar_InfoString(CVAR_USERINFO),
            sizeof(info));
        Info_SetValueForKey(
            info, "protocol",
            va("%i", CL_NETWORK_PROTOCOL_VERSION));
        Info_SetValueForKey(info, "qport", va("%i", qport));
        Info_SetValueForKey(
            info, "challenge", va("%i", clc.challenge));

        char connectPacket[CL_CONNECT_PACKET_CAPACITY];
        strcpy(connectPacket, "connect ");
        connectPacket[8] = '"';

        const size_t infoLength = strlen(info);
        for (size_t index = 0; index < infoLength; ++index)
            connectPacket[index + 9] = info[index];
        connectPacket[infoLength + 9] = '"';
        connectPacket[infoLength + 10] = '\0';

        const int32_t connectLength =
            (int32_t)infoLength + 10;
        char punkBusterPacket[CL_CONNECT_PACKET_CAPACITY];
        memcpy(
            punkBusterPacket, connectPacket,
            (size_t)connectLength);
        int32_t punkBusterLength = connectLength;
        PbClientConnecting(
            PB_CLIENT_CONNECTING_REQUEST,
            punkBusterPacket, &punkBusterLength);

        NET_OutOfBandData(
            NS_CLIENT, clc.serverAddress,
            (const uint8_t *)connectPacket, connectLength);
        cvar_modifiedFlags &= ~((uint32_t)CVAR_USERINFO);
        return;
    }

    default:
        Com_Error(
            ERR_FATAL,
            "\x15" "CL_CheckForResend: bad cls.state");
        return;
    }
}

/* Source: CoDUOMP.exe 0x004104a0..0x004104c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004104a0_004104ca.mcode.
 * Name and signature: exact same-module Mac symbol CL_ShutdownAll. */
void CL_ShutdownAll(void)
{
    CL_ShutdownCGame();
    CL_ShutdownUI();
    if (rendererExports.Shutdown != NULL)
        rendererExports.Shutdown(qfalse);

    cls.uiStarted = qfalse;
    cls.rendererStarted = qfalse;
}

/* Source: CoDUOMP.exe 0x00413aa0..0x00413aa9, recovered from the executable
 * gap between CL_Frame and CL_RefPrintf.
 * Name: exact same-module Mac symbol CL_SetRecommended_f. The console command
 * requests the sound restart that startup-time Com_SetRecommended calls omit. */
void CL_SetRecommended_f(void)
{
    Com_SetRecommended(qtrue);
}

/* Source: CoDUOMP.exe 0x00414500..0x00414f72.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00414500_00414f73.mcode.
 * Name and no-argument signature: exact same-module Mac symbol CL_Init.
 * The original initializer registers client controls in this order, copies
 * the five autoupdate hostnames into clientStatic_t, installs console
 * commands, initializes renderer/screen state, and executes queued commands. */
void CL_Init(void)
{
    Com_Printf("----- Client Initialization -----\n");
    Con_Init();

    memset(&cl, 0, sizeof(cl));
    cls.state = CA_DISCONNECTED;
    cls.realtime = 0;
    cls.realTime = 0;

    CL_InitKeyCommands();

    cl_noprint = Cvar_Get("cl_noprint", "0", 0);
    cl_autocmd = Cvar_Get("cl_autocmd", "1", CVAR_ARCHIVE);
    cl_timeout = Cvar_Get("cl_timeout", "200", 0);
    cl_timeNudge = Cvar_Get("cl_timeNudge", "0", CVAR_TEMP);
    cl_shownet = Cvar_Get("cl_shownet", "0", CVAR_TEMP);
    cl_shownuments = Cvar_Get("cl_shownuments", "0", CVAR_TEMP);
    cl_visibleClients =
        Cvar_Get("cl_visibleClients", "0", CVAR_TEMP);
    cl_showServerCommands =
        Cvar_Get("cl_showServerCommands", "0", 0);
    cl_showSend = Cvar_Get("cl_showSend", "0", CVAR_TEMP);
    cl_showTimeDelta =
        Cvar_Get("cl_showTimeDelta", "0", CVAR_TEMP);
    cl_freezeDemo = Cvar_Get("cl_freezeDemo", "0", CVAR_TEMP);
    rcon_client_password =
        Cvar_Get("rconPassword", "", CVAR_TEMP);
    cl_activeAction = Cvar_Get("activeAction", "", CVAR_TEMP);
    cl_timedemo = Cvar_Get("timedemo", "0", 0);
    cl_avidemo = Cvar_Get("cl_avidemo", "0", 0);
    cl_forceavidemo = Cvar_Get("cl_forceavidemo", "0", 0);
    rcon_client_address = Cvar_Get("rconAddress", "", 0);

    cl_yawspeed = Cvar_Get("cl_yawspeed", "140", CVAR_ARCHIVE);
    cl_pitchspeed =
        Cvar_Get("cl_pitchspeed", "140", CVAR_ARCHIVE);
    cl_anglespeedkey =
        Cvar_Get("cl_anglespeedkey", "1.5", 0);
    cl_maxpackets =
        Cvar_Get("cl_maxpackets", CL_MAXPACKETS_DEFAULT_TEXT,
                 CVAR_ARCHIVE);
    cl_packetdup = Cvar_Get("cl_packetdup", "1", CVAR_ARCHIVE);
    cl_run = Cvar_Get("cl_run", "1", CVAR_TEMP);
    cl_stance = Cvar_Get("cl_stance", "0", CVAR_TEMP);
    cl_stanceTemp = Cvar_Get("cl_stanceTemp", "0", CVAR_TEMP);
    cl_goStandJumpTime =
        Cvar_Get("cl_goStandJumpTime", "0", CVAR_ARCHIVE);
    sensitivity = Cvar_Get("sensitivity", "5", CVAR_ARCHIVE);
    cl_mouseAccel =
        Cvar_Get("cl_mouseAccel", "0", CVAR_ARCHIVE);
    cl_freelook = Cvar_Get("cl_freelook", "1", CVAR_ARCHIVE);
    cl_showmouserate = Cvar_Get("cl_showmouserate", "0", 0);
    cl_allowDownload =
        Cvar_Get("cl_allowDownload", "0", CVAR_ARCHIVE);
    cl_serverAllowDownload = Cvar_Get(
        "sv_allowDownload", "1",
        CVAR_ARCHIVE | CVAR_SYSTEMINFO);
    cl_wwwDownload = Cvar_Get(
        "cl_wwwDownload", "1",
        CVAR_ARCHIVE | CVAR_USERINFO);
    cl_conXOffset = Cvar_Get("cl_conXOffset", "0", 0);
    r_inGameVideo =
        Cvar_Get("r_inGameVideo", "1", CVAR_ARCHIVE);
    cl_serverStatusResendTime =
        Cvar_Get("cl_serverStatusResendTime", "750", 0);

    cl_viewPitchCompensate =
        Cvar_Get("cl_viewPitchCompensate", "0", CVAR_ROM);
    cl_viewYawCompensate =
        Cvar_Get("cl_viewYawCompensate", "0", CVAR_ROM);
    cl_bypassMouseInput =
        Cvar_Get("cl_bypassMouseInput", "0", 0);
    m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE);
    m_yaw = Cvar_Get("m_yaw", "0.022", CVAR_ARCHIVE);
    m_forward = Cvar_Get("m_forward", "0.25", CVAR_ARCHIVE);
    m_side = Cvar_Get("m_side", "0.25", CVAR_ARCHIVE);
    m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
    cl_motdString = Cvar_Get("cl_motdString", "", CVAR_ROM);
    cl_ingame = Cvar_Get("cl_ingame", "0", CVAR_ROM);

    (void)Cvar_Get("cl_maxPing", "800", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_drawCompass", "1", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_drawNotifyText", "1", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_descriptiveText", "1", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_drawTeamOverlay", "2", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_drawGun", "1", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_cursorHints", "4", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_voiceSpriteTime", "6000", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_teamChatsOnly", "0", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_noVoiceChats", "0", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_noVoiceText", "0", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_crosshairSize", "48", CVAR_ARCHIVE);
    (void)Cvar_Get("cg_drawCrosshair", "1", CVAR_ARCHIVE);

    (void)Cvar_Get(
        "name", "Unknown Soldier",
        CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get(
        "rate", CL_RATE_DEFAULT_TEXT,
        CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get(
        "snaps", CL_SNAPS_DEFAULT_TEXT,
        CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get(
        "model", "",
        CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get(
        "head", "",
        CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get(
        "handicap", "100",
        CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get(
        "cl_anonymous", "0",
        CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get(
        "cl_guid", "unknown",
        CVAR_ROM | CVAR_USERINFO);
    (void)Cvar_Get(
        "cl_punkbuster", "0",
        CVAR_ROM | CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get("password", "", CVAR_USERINFO);
    (void)Cvar_Get(
        "cg_predictItems", "1",
        CVAR_ARCHIVE | CVAR_USERINFO);
    (void)Cvar_Get("cg_viewsize", "100", CVAR_ARCHIVE);

    cl_waitForFire = Cvar_Get("cl_waitForFire", "0", CVAR_ROM);
    fx_enable = Cvar_Get("fx_enable", "1", CVAR_CHEAT);
    fx_draw = Cvar_Get("fx_draw", "1", CVAR_CHEAT);
    fx_cull = Cvar_Get("fx_cull", "1", 0);
    fx_cullscale = Cvar_Get("fx_cullscale", "1", CVAR_ARCHIVE);
    fx_cullbias = Cvar_Get("fx_cullbias", "0", CVAR_ARCHIVE);
    fx_freeze = Cvar_Get("fx_freeze", "0", CVAR_CHEAT);
    fx_debug = Cvar_Get("fx_debug", "0", CVAR_CHEAT);
    fx_debugBolt = Cvar_Get("fx_debugBolt", "0", CVAR_CHEAT);
    fx_count = Cvar_Get("fx_count", "0", CVAR_CHEAT);

    cl_updateAvailable =
        Cvar_Get("cl_updateavailable", "0", CVAR_ROM);
    cl_updateFiles = Cvar_Get("cl_updatefiles", "", CVAR_ROM);
    cl_updateOldVersion =
        Cvar_Get("cl_updateoldversion", "", CVAR_ROM);
    cl_updateVersion =
        Cvar_Get("cl_updateversion", "", CVAR_ROM);
    cl_serverLoadMap =
        Cvar_Get("cl_serverloadmap", "", CVAR_ROM);
    cl_serverLoadGameType =
        Cvar_Get("cl_serverloadgametype", "", CVAR_ROM);
    cl_serverLoadWaiting =
        Cvar_Get("cl_serverloadwaiting", "0", CVAR_ROM);
    cg_announcerSounds =
        Cvar_Get("cg_announcerSounds", "1", CVAR_ARCHIVE);
    cl_executeString = Cvar_Get("cl_executeString", "", 0);

    strncpy(cls.autoUpdateServerNames[0], "au2cod1.activision.com",
            sizeof(cls.autoUpdateServerNames[0]) - 1);
    cls.autoUpdateServerNames[0]
                             [sizeof(cls.autoUpdateServerNames[0]) - 1] =
        '\0';
    strncpy(cls.autoUpdateServerNames[1], "au2cod2.activision.com",
            sizeof(cls.autoUpdateServerNames[1]) - 1);
    cls.autoUpdateServerNames[1]
                             [sizeof(cls.autoUpdateServerNames[1]) - 1] =
        '\0';
    strncpy(cls.autoUpdateServerNames[2], "au2cod3.activision.com",
            sizeof(cls.autoUpdateServerNames[2]) - 1);
    cls.autoUpdateServerNames[2]
                             [sizeof(cls.autoUpdateServerNames[2]) - 1] =
        '\0';
    strncpy(cls.autoUpdateServerNames[3], "au2cod4.activision.com",
            sizeof(cls.autoUpdateServerNames[3]) - 1);
    cls.autoUpdateServerNames[3]
                             [sizeof(cls.autoUpdateServerNames[3]) - 1] =
        '\0';
    strncpy(cls.autoUpdateServerNames[4], "au2cod5.activision.com",
            sizeof(cls.autoUpdateServerNames[4]) - 1);
    cls.autoUpdateServerNames[4]
                             [sizeof(cls.autoUpdateServerNames[4]) - 1] =
        '\0';

    Cmd_AddCommand("cmd", CL_ForwardToServer_f);
    Cmd_AddCommand("configstrings", CL_Configstrings_f);
    Cmd_AddCommand("clientinfo", CL_Clientinfo_f);
    Cmd_AddCommand("snd_restart", CL_Snd_Restart_f);
    Cmd_AddCommand("vid_restart", CL_Vid_Restart_f);
    Cmd_AddCommand("disconnect", CL_Disconnect_f);
    Cmd_AddCommand("record", CL_Record_f);
    Cmd_AddCommand("demo", CL_PlayDemo_f);
    Cmd_AddCommand("cinematic", CL_PlayCinematic_f);
    Cmd_AddCommand("logo", CL_PlayLogo_f);
    Cmd_AddCommand("stoprecord", CL_StopRecord_f);
    Cmd_AddCommand("connect", CL_Connect_f);
    Cmd_AddCommand("reconnect", CL_Reconnect_f);
    Cmd_AddCommand("localservers", CL_LocalServers_f);
    Cmd_AddCommand("globalservers", CL_GlobalServers_f);
    Cmd_AddCommand("rcon", CL_Rcon_f);
    Cmd_AddCommand("setenv", CL_Setenv_f);
    Cmd_AddCommand("ping", CL_Ping_f);
    Cmd_AddCommand("serverstatus", CL_ServerStatus_f);
    Cmd_AddCommand("showip", CL_ShowIP_f);
    Cmd_AddCommand("fs_openedList", CL_OpenedPK3List_f);
    Cmd_AddCommand("fs_referencedList", CL_ReferencedPK3List_f);
    Cmd_AddCommand("updatehunkusage", CL_UpdateLevelHunkUsage);
    Cmd_AddCommand("startSingleplayer", CL_startSingleplayer_f);
    Cmd_AddCommand("setRecommended", CL_SetRecommended_f);
    Cmd_AddCommand("updatescreen", CL_UpdateScreen_f);
    Cmd_AddCommand("cubemapShot", CL_CubemapShot_f);
    Cmd_AddCommand(
        "localizeSoundAliasFiles", Com_WriteLocalizedSoundAliasFiles);
    Cmd_AddCommand("vsay", CL_Vsay_f);

    cls_autoupdateServerResolved = qfalse;
    cl_updateStarted = qfalse;
    (void)Cvar_Get("cl_noautoupdate", CL_NOAUTOUPDATE_DEFAULT_TEXT,
                   CVAR_ROM);
    cvar_t *const noAutoUpdate = Cvar_FindVar("cl_noautoupdate");
    if (noAutoUpdate == NULL || noAutoUpdate->value == 0.0f)
        CL_CheckAutoUpdate();

    CL_InitRef();
    SCR_Init();
    Cbuf_Execute();
    (void)Cvar_Set2("cl_running", "1", qtrue);
    Com_Printf("----- Client Initialization Complete -----\n");
}

/* Source: CoDUOMP.exe 0x00414f80..0x0041517e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00414f80_0041517f.mcode.
 * Name and no-argument signature: exact same-module Mac symbol CL_Shutdown.
 * The Windows optimizer inlines CL_ShutdownRef and then clears the complete
 * clientStatic_t object with one REP STOSD. */
void CL_Shutdown(void)
{
    Com_Printf("----- CL_Shutdown -----\n");

    if (cl_shutdownInProgress != qfalse) {
        printf("recursive shutdown\n");
        return;
    }
    cl_shutdownInProgress = qtrue;

    CL_ShutdownDebugData();
    CL_Disconnect(qtrue);
    CL_ShutdownCGame();
    MSS_Shutdown();
    CL_ShutdownRef();
    CL_ShutdownUI();
    CL_ShutdownKeyCommands();

    Cmd_RemoveCommand("cmd");
    Cmd_RemoveCommand("configstrings");
    Cmd_RemoveCommand("clientinfo");
    Cmd_RemoveCommand("snd_restart");
    Cmd_RemoveCommand("vid_restart");
    Cmd_RemoveCommand("disconnect");
    Cmd_RemoveCommand("record");
    Cmd_RemoveCommand("demo");
    Cmd_RemoveCommand("cinematic");
    Cmd_RemoveCommand("stoprecord");
    Cmd_RemoveCommand("connect");
    Cmd_RemoveCommand("reconnect");
    Cmd_RemoveCommand("localservers");
    Cmd_RemoveCommand("globalservers");
    Cmd_RemoveCommand("rcon");
    Cmd_RemoveCommand("setenv");
    Cmd_RemoveCommand("ping");
    Cmd_RemoveCommand("serverstatus");
    Cmd_RemoveCommand("showip");
    Cmd_RemoveCommand("fs_openedList");
    Cmd_RemoveCommand("fs_referencedList");
    Cmd_RemoveCommand("updatehunkusage");
    Cmd_RemoveCommand("updatescreen");
    Cmd_RemoveCommand("SaveTranslations");
    Cmd_RemoveCommand("SaveNewTranslations");
    Cmd_RemoveCommand("LoadTranslations");
    Cmd_RemoveCommand("startSingleplayer");
    Cmd_RemoveCommand("buyNow");
    Cmd_RemoveCommand("singlePlayLink");
    Cmd_RemoveCommand("setRecommended");
    Cmd_RemoveCommand("cubemapShot");
    Cmd_RemoveCommand("vsay");

    (void)Cvar_Set2("cl_running", "", qtrue);
    cl_shutdownInProgress = qfalse;
    memset(&cls, 0, sizeof(cls));

    Com_Printf("-----------------------\n");
}

/* Source: CoDUOMP.exe 0x004104d0..0x004105dc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004104d0_004105dd.mcode.
 * Name and signature: exact same-module Mac symbol
 * CL_SetupForNewServerMap. This no-argument local-server transition is
 * distinct from CL_MapLoading(mapName, gameType) at 0x00416fc0. */
void CL_SetupForNewServerMap(void)
{
    static const char localServerName[] = "localhost";

    if (cl_running->integer == 0)
        return;

    Con_Close();
    cls.keyCatchers = 0;

    if (cls.state >= CA_CONNECTED &&
        Q_stricmp(cls.serverName, localServerName) == 0) {
        cls.state = CA_CONNECTED;
        memset(cls.updateInfoString, 0, sizeof(cls.updateInfoString));
        memset(clc.serverMessage, 0, sizeof(clc.serverMessage));
        memset(&cl.gameState, 0, sizeof(cl.gameState));
        clc.lastPacketSentTime = CL_LOCAL_PACKET_SEND_TIME;
        SCR_UpdateScreen();
    } else {
        Cvar_Set2("nextmap", "", qtrue);
        CL_Disconnect(qtrue);

        Q_strncpyz(cls.serverName, localServerName,
                   sizeof(cls.serverName));
        cls.state = CA_CHALLENGING;
        cls.keyCatchers = 0;
        SCR_UpdateScreen();

        (void)NET_StringToAdr(cls.serverName, &clc.serverAddress);
        clc.connectTime = CL_LOCAL_CONNECT_RESEND_TIME;
        CL_CheckForResend();
    }

    mss_masterVolume.target = 0.0f;
    mss_masterVolume.ratePerMsec = -mss_masterVolume.current;
    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
}

/* Source: CoDUOMP.exe 0x00416fc0..0x00416ffa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00416fc0_00416ffb.mcode.
 * Name and register-carried arguments: exact same-module Mac symbol
 * CL_MapLoading plus the two original packet-reader call sites. */
void CL_MapLoading(const char *mapName, const char *gameType)
{
    Com_Printf("Server changing map %s, gametype %s\n",
               mapName, gameType);
    Cvar_Set2("cl_serverloadmap", mapName, qtrue);
    Cvar_Set2("cl_serverloadgametype", gameType, qtrue);
    Cvar_Set2("cl_serverloadwaiting", "0", qtrue);
}
