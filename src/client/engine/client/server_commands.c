#include "cgame.h"

#include "client/common/client_legacy_crt.h"
#include "client/common/client_format_validation.h"
#include "console.h"
#include "../renderer/renderer_api.h"
#include "qcommon/fx_types.h"

#include <stdlib.h>
#include <string.h>

enum {
    CL_RELIABLE_COMMAND_MASK = CODUO_RELIABLE_COMMAND_COUNT - 1,
    CL_BIG_CONFIG_STRING_CAPACITY = 8192,
    CL_CONFIGSTRING_SYSTEMINFO = 1,
    CL_SERVER_COMMAND_CONFIGSTRING = 'd',
    CL_SERVER_COMMAND_DISCONNECT = 'w',
    CL_SERVER_COMMAND_BIG_CONFIG_START = 'x',
    CL_SERVER_COMMAND_BIG_CONFIG_CONTINUE = 'y',
    CL_SERVER_COMMAND_BIG_CONFIG_END = 'z',
    CL_SERVER_COMMAND_RESET_B = 'B',
    CL_SERVER_COMMAND_RESET_N = 'n'
};

/* Original 0x005d06e0..0x005d26df. The x/y/z server commands build one
 * complete "d index value" command here before normal command processing. */
static char cl_bigConfigString[CL_BIG_CONFIG_STRING_CAPACITY];

/* Original Win32 pointer 0x04958194; nonzero prints every consumed reliable
 * server command before it is interpreted. */
cvar_t *cl_showServerCommands;

/* Source: CoDUOMP.exe 0x00401230..0x00401235.
 * Name and signature: exact same-module Mac symbol
 * CL_GetCurrentCmdNumber. */
int32_t CL_GetCurrentCmdNumber(void)
{
    return cl.cmdNumber;
}

/* Source: CoDUOMP.exe 0x004011c0..0x00401222.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004011c0_00401223.mcode.
 * Name and signature: exact same-module Mac symbol CL_GetUserCmd; the cgame
 * syscall caller at 0x00403468 proves commandNumber followed by the output
 * usercmd_t pointer. */
qboolean CL_GetUserCmd(int32_t commandNumber, usercmd_t *command)
{
    if (commandNumber > cl.cmdNumber) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CL_GetUserCmd: %i >= %i",
                  commandNumber, cl.cmdNumber);
    }

    if (commandNumber <= cl.cmdNumber - CODUO_USERCMD_BACKUP_COUNT) {
        return qfalse;
    }

    *command = cl.cmds[commandNumber & (CODUO_USERCMD_BACKUP_COUNT - 1)];
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00401480..0x00401623.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401480_00401624.mcode.
 * Name: exact same-module Mac symbol CL_ConfigstringModified. */
void CL_ConfigstringModified(void)
{
    const int32_t index = coduo_crt_atoi(Cmd_Argv(1));
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_Error(ERR_DROP, "\x15"
                            "configstring > MAX_CONFIGSTRINGS");
    }

    const char *const newString = Cmd_Argv(2);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (index >= CS_EFFECTS && index < CS_FX && strcspn(newString, ".") >= FX_EFFECT_TEMPLATE_NAME_CAPACITY) {
        Com_Error(ERR_DROP, "\x15"
                            "CL_ConfigstringModified: effect name is too long");
    }

    const char *const oldString = &cl.gameState.stringData[cl.gameState.stringOffsets[index]];
    if (strcmp(oldString, newString) == 0)
        return;

    const gameState_t oldState = cl.gameState;
    memset(&cl.gameState, 0, sizeof(cl.gameState));
    cl.gameState.dataCount = 1;

    for (int32_t configString = 0; configString < MAX_CONFIGSTRINGS; ++configString) {
        const char *value;
        if (configString == index) {
            value = newString;
        } else {
            value = &oldState.stringData[oldState.stringOffsets[configString]];
        }

        if (*value == '\0')
            continue;

        const int32_t valueBytes = (int32_t)strlen(value) + 1;
        if (cl.gameState.dataCount + valueBytes > (int32_t)sizeof(cl.gameState.stringData)) {
            Com_Error(ERR_DROP, "\x15"
                                "MAX_GAMESTATE_CHARS exceeded");
        }

        cl.gameState.stringOffsets[configString] = cl.gameState.dataCount;
        memcpy(&cl.gameState.stringData[cl.gameState.dataCount], value, (size_t)valueBytes);
        cl.gameState.dataCount += valueBytes;
    }

    if (index == CL_CONFIGSTRING_SYSTEMINFO)
        CL_SystemInfoChanged();
}

/* Source: CoDUOMP.exe 0x00401630..0x00401944.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401630_00401945.mcode, its switch
 * tables at 0x00401948 and 0x00401964, and exact strings at
 * 0x0058e9c9..0x0058eac8.
 * Name and return type: exact same-module Mac symbol CL_GetServerCommand;
 * the cgame syscall caller proves the command-number argument. */
qboolean CL_GetServerCommand(int32_t serverCommandNumber)
{
    const char *command;

    if (serverCommandNumber <= clc.serverCommandSequence - CODUO_RELIABLE_COMMAND_COUNT) {
        if (clc.demoPlayback != qfalse)
            return qfalse;

        Com_Printf("===== CL_GetServerCommand =====\n");
        Com_Printf("serverCommandNumber: %d\n", serverCommandNumber & CL_RELIABLE_COMMAND_MASK);
        for (int32_t commandIndex = 0; commandIndex < CODUO_RELIABLE_COMMAND_COUNT; ++commandIndex) {
            Com_Printf("cmd %5d: %s\n", commandIndex, clc.serverCommands[commandIndex & CL_RELIABLE_COMMAND_MASK]);
        }
        Com_Error(ERR_DROP, "\x15"
                            "CL_GetServerCommand: "
                            "\x14"
                            "EXE_ERR_RELIABLE_CYCLED_OUT");
    }

    if (serverCommandNumber > clc.serverCommandSequence) {
        Com_Error(ERR_DROP, "\x15"
                            "CL_GetServerCommand: "
                            "\x14"
                            "EXE_ERR_NOT_RECEIVED");
    }

    clc.lastExecutedServerCommand = serverCommandNumber;
    command = clc.serverCommands[serverCommandNumber & CL_RELIABLE_COMMAND_MASK];

process_command:
    if (cl_showServerCommands->integer != 0) {
        Com_DPrintf("serverCommand: %i : %s\n", serverCommandNumber, command);
    }

    Cmd_TokenizeString(command);
    const char *const commandName = Cmd_Argc() > 0 ? Cmd_Argv(0) : "";

    switch ((uint8_t)commandName[0]) {
    case CL_SERVER_COMMAND_DISCONNECT: {
        if (Cmd_Argc() >= 2) {
            const char *const translatedReason = SEH_SafeTranslateString(Cmd_Argv(1));
            const char *const translatedFormat = SEH_SafeTranslateString("EXE_SERVERDISCONNECTREASON");
            const char *completedReason;
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (client_compat_validate_format_signature(translatedFormat, "s") == qfalse) {
                Com_Printf("WARNING: rejected invalid server-disconnect format\n");
                completedReason = translatedFormat;
            } else {
                completedReason = va(translatedFormat, translatedReason);
            }
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            Com_Error(ERR_SERVER_DISCONNECT, "%s", completedReason);
        } else {
            Com_Error(ERR_SERVER_DISCONNECT, "EXE_SERVER_DISCONNECTED");
        }
        /* The retail fallthrough is unreachable when Com_Error unwinds. */
        /* fall through */
    }

    case CL_SERVER_COMMAND_BIG_CONFIG_START: {
        Cmd_TokenizeString2(command, 3);
        const char *const value = Cmd_Argv(2);
        const char *const index = Cmd_Argv(1);
        Com_sprintf(cl_bigConfigString, sizeof(cl_bigConfigString), "d %s %s", index, value);
        return qfalse;
    }

    case CL_SERVER_COMMAND_BIG_CONFIG_CONTINUE: {
        Cmd_TokenizeString2(command, 3);
        const char *const fragment = Cmd_Argv(2);
        if (strlen(cl_bigConfigString) + strlen(fragment) >= sizeof(cl_bigConfigString)) {
            Com_Error(ERR_DROP, "\x15"
                                "bcs exceeded BIG_INFO_STRING");
        }
        strcat(cl_bigConfigString, fragment);
        return qfalse;
    }

    case CL_SERVER_COMMAND_BIG_CONFIG_END: {
        Cmd_TokenizeString2(command, 3);
        const char *const fragment = Cmd_Argv(2);
        if (strlen(cl_bigConfigString) + strlen(fragment) + 1 >= sizeof(cl_bigConfigString)) {
            Com_Error(ERR_DROP, "\x15"
                                "bcs exceeded BIG_INFO_STRING");
        }
        strcat(cl_bigConfigString, fragment);
        command = cl_bigConfigString;
        goto process_command;
    }

    case CL_SERVER_COMMAND_CONFIGSTRING:
        Cmd_TokenizeString2(command, 3);
        CL_ConfigstringModified();
        Cmd_TokenizeString2(command, 3);
        return qtrue;

    case CL_SERVER_COMMAND_RESET_B:
    case CL_SERVER_COMMAND_RESET_N:
        Con_ClearNotify();
        Con_ClearSubtitles();
        memset(cl.cmds, 0, sizeof(cl.cmds));
        rendererExports.ClearScene();
        return qtrue;

    default:
        return qtrue;
    }
}
