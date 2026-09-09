#include "q_command.h"

#include "q_checksum.h"
#include "q_memory.h"
#include "q_path.h"
#include "q_string.h"
#include "qcommon_limits.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "q_command.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    Q_COMMAND_PB_CONSOLE_OPCODE = 14,
    Q_COMMAND_PB_CONSOLE_CLIENT_NUM = -1
};

/* Host boundaries called by the original common command subsystem. */
void Com_Printf(const char *format, ...);
void Com_Error(errorParm_t code, const char *format, ...);
int32_t FS_ReadFile(const char *path, void **buffer);
void FS_FreeFile(void *buffer);
const char *Cvar_VariableString(const char *name);
qboolean Cvar_Command(void);
qboolean CL_GameCommand(void);
qboolean UI_GameCommand(void);
qboolean SV_GameCommand(void);
void CL_ForwardCommandToServer(const char *text);
extern cvar_t *cl_running;
extern cvar_t *sv_running;

#if defined(WINDOWS_BEHAVIOR)
cvar_t *Cvar_FindVar(const char *name);
void Sys_OutOfMemory(void);
qboolean PB_ClientTrapConsole(const char *text);
void PB_DispatchClientConsoleCommand(const char *text);
void PB_DispatchServerConsoleCommand(const char *text);
#else
int32_t Cvar_VariableIntegerValue(const char *name);
void PB_CallServerSbGlobal(int32_t command, int32_t clientNum,
                           uint32_t length, const char *text);
#endif

#include "q_command_services.h"

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */

void Cmd_Wait_f(void)
{
    if (Cmd_Argc() == 2) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        cmd_wait = atoi(Cmd_Argv(1));
    } else {
        cmd_wait = 1;
    }
}

void Cbuf_Init(void)
{
    cmd_text.data = cmd_textData;
    cmd_text.maxsize = (int32_t)sizeof(cmd_textData);
    cmd_text.cursize = 0;
}

void Cbuf_AddText(const char *text)
{
    const size_t textLength = strlen(text);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (cmd_text.cursize < 0 || cmd_text.maxsize <= cmd_text.cursize ||
        textLength >= (size_t)(cmd_text.maxsize - cmd_text.cursize)) {
        Com_Printf("Cbuf_AddText: overflow\n");
        return;
    }

    memcpy(cmd_text.data + cmd_text.cursize, text, textLength);
    cmd_text.cursize += (int32_t)textLength;
}

void Cbuf_InsertText(const char *text)
{
    const size_t textLength = strlen(text);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (cmd_text.cursize < 0 || cmd_text.maxsize <= cmd_text.cursize ||
        textLength >= (size_t)(cmd_text.maxsize - cmd_text.cursize)) {
        Com_Printf("Cbuf_InsertText overflowed\n");
        return;
    }
    const int32_t insertLength = (int32_t)textLength + 1;
    const int32_t newSize = cmd_text.cursize + insertLength;

    /* Both authoritative i386 bodies copy overlapping bytes backwards and
     * skip the move when cursize is negative.  A size_t memmove expression
     * would not preserve that corrupt-state behavior on native64. */
    for (int32_t index = cmd_text.cursize - 1;
         index >= 0;
         --index) {
        cmd_text.data[insertLength + index] = cmd_text.data[index];
    }

    memcpy(cmd_text.data, text, textLength);
    cmd_text.data[insertLength - 1] = '\n';
    cmd_text.cursize = newSize;
}

void Cbuf_ExecuteText(cbufExec_t executionMode, const char *text)
{
    switch (executionMode) {
    case EXEC_NOW:
        if (text != NULL && text[0] != '\0') {
            Cmd_ExecuteString(text);
        } else {
            Cbuf_Execute();
        }
        return;

    case EXEC_INSERT:
        Cbuf_InsertText(text);
        return;

    case EXEC_APPEND:
        Cbuf_AddText(text);
        return;

    default:
        /* The leading control byte is present at CoDUOMP.exe 0x00596410 and
         * coduo_lnxded 0x080dcb00. */
        Com_Error(ERR_FATAL, "\x15" "Cbuf_ExecuteText: bad exec_when");
    }
}

void Cbuf_Execute(void)
{
    char command[CBUF_COMMAND_CAPACITY];

    while (cmd_text.cursize != 0) {
        if (cmd_wait != 0) {
            --cmd_wait;
            return;
        }

        int32_t quoteCount = 0;
        int32_t commandLength;
        for (commandLength = 0;
             commandLength < cmd_text.cursize;
             ++commandLength) {
            const char character = cmd_text.data[commandLength];
            if (character == '"') {
                ++quoteCount;
            }

            if ((((quoteCount & 1) == 0) && character == ';') ||
                character == '\n' || character == '\r') {
                break;
            }
        }

        if (commandLength > CBUF_COMMAND_CAPACITY - 2) {
            commandLength = CBUF_COMMAND_CAPACITY - 1;
        }

        memcpy(command, cmd_text.data, (size_t)commandLength);
        command[commandLength] = '\0';

        if (commandLength == cmd_text.cursize) {
            cmd_text.cursize = 0;
        } else {
            ++commandLength;
            cmd_text.cursize -= commandLength;
            memmove(cmd_text.data, cmd_text.data + commandLength,
                    (size_t)(uint32_t)cmd_text.cursize);
        }

        Cmd_ExecuteString(command);
    }
}

#if defined(WINDOWS_BEHAVIOR)
void Cmd_Exec_f(void)
{
    char filename[MAX_QPATH];
    void *fileBuffer;

    if (Cmd_Argc() != 2) {
        Com_Printf("exec <filename> : execute a script file\n");
        return;
    }

    Q_strncpyz(filename, Cmd_Argv(1), (int32_t)sizeof(filename));
    Com_DefaultExtension(filename, (int32_t)sizeof(filename), ".cfg");
    const int32_t fileLength = FS_ReadFile(filename, &fileBuffer);

    if (fileBuffer == NULL) {
        Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
        return;
    }

    Com_Printf("execing %s\n", Cmd_Argv(1));
    const cvar_t *const consoleLockout =
        Cvar_FindVar("sv_console_lockout");
    if (consoleLockout != NULL && consoleLockout->integer != 0) {
        const int32_t checksum =
            (int32_t)Com_BlockChecksum(fileBuffer, fileLength);
        Cbuf_InsertText(
            va("say Server exec: %s, size: %i, checksum: %i",
               filename, fileLength, checksum));
    }

    Cbuf_InsertText((const char *)fileBuffer);
    FS_FreeFile(fileBuffer);
}
#else
void Cmd_Exec_f(void)
{
    char filename[MAX_QPATH];
    void *fileBuffer;

    if (Cmd_Argc() != 2) {
        Com_Printf("exec <filename> : execute a script file\n");
        return;
    }

    Q_strncpyz(filename, Cmd_Argv(1), (int32_t)sizeof(filename));
    Com_DefaultExtension(filename, (int32_t)sizeof(filename), ".cfg");
    const int32_t fileLength = FS_ReadFile(filename, &fileBuffer);

    if (fileBuffer == NULL) {
        Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
        return;
    }

    Com_Printf("execing %s\n", Cmd_Argv(1));
    if (Cvar_VariableIntegerValue("sv_console_lockout") != 0) {
        const int32_t checksum =
            (int32_t)Com_BlockChecksum(fileBuffer, fileLength);
        Cbuf_InsertText(
            va("say Server exec: %s, size: %i, checksum: %i",
               filename, fileLength, checksum));
    }

    Cbuf_InsertText((const char *)fileBuffer);
    FS_FreeFile(fileBuffer);
}
#endif

void Cmd_ShowChecksum_f(void)
{
    char filename[MAX_QPATH];
    void *fileBuffer;

    if (Cmd_Argc() != 2) {
        Com_Printf(
            "showchecksum <filename> : prints size and checksum of a file\n");
        return;
    }

    Q_strncpyz(filename, Cmd_Argv(1), (int32_t)sizeof(filename));
    Com_DefaultExtension(filename, (int32_t)sizeof(filename), ".cfg");
    const int32_t fileLength = FS_ReadFile(filename, &fileBuffer);

    if (fileBuffer == NULL) {
        Com_Printf("couldn't find %s\n", Cmd_Argv(1));
        return;
    }

    const int32_t checksum =
        (int32_t)Com_BlockChecksum(fileBuffer, fileLength);
    Com_Printf("ShowChecksum: %s, size: %i, checksum: %i",
               filename, fileLength, checksum);
    FS_FreeFile(fileBuffer);
}

void Cmd_Vstr_f(void)
{
    if (Cmd_Argc() == 2) {
        Cbuf_InsertText(
            va("%s\n", Cvar_VariableString(Cmd_Argv(1))));
    } else {
        Com_Printf(
            "vstr <variablename> : execute a variable command\n");
    }
}

void Cmd_Echo_f(void)
{
    for (int32_t argumentIndex = 1;
         argumentIndex < Cmd_Argc();
         ++argumentIndex) {
        Com_Printf("%s ", Cmd_Argv(argumentIndex));
    }
    Com_Printf("\n");
}

int32_t Cmd_Argc(void)
{
    return cmd_argc;
}

const char *Cmd_Argv(int32_t argumentIndex)
{
    if ((uint32_t)argumentIndex < (uint32_t)cmd_argc) {
        return cmd_argv[argumentIndex];
    }
    return "";
}

void Cmd_ArgvBuffer(int32_t argumentIndex, char *buffer,
                    int32_t bufferLength)
{
    Q_strncpyz(buffer, Cmd_Argv(argumentIndex), bufferLength);
}

char *Cmd_Args(int32_t firstArgument)
{
    cmd_args[0] = '\0';

    size_t requiredCapacity = 1;
    for (int32_t argumentIndex = firstArgument;
         argumentIndex < cmd_argc;
         ++argumentIndex) {
        const size_t argumentLength = strlen(cmd_argv[argumentIndex]);

        /* NOT_FROM_ORIGINAL_SOURCE: preflight the complete argument string,
         * its separators, and final NUL before writing any partial command. */
        if (argumentLength > CMD_ARGS_CAPACITY - requiredCapacity) {
            Com_Printf("Cmd_Args: arguments exceed output capacity\n");
            return cmd_args;
        }
        requiredCapacity += argumentLength;
        if (argumentIndex != cmd_argc - 1) {
            if (requiredCapacity == CMD_ARGS_CAPACITY) {
                Com_Printf("Cmd_Args: arguments exceed output capacity\n");
                return cmd_args;
            }
            ++requiredCapacity;
        }
    }

    char *output = cmd_args;
    for (int32_t argumentIndex = firstArgument;
         argumentIndex < cmd_argc;
         ++argumentIndex) {
        const size_t argumentLength = strlen(cmd_argv[argumentIndex]);
        memcpy(output, cmd_argv[argumentIndex], argumentLength);
        output += argumentLength;
        if (argumentIndex != cmd_argc - 1) {
            *output++ = ' ';
        }
    }
    *output = '\0';

    return cmd_args;
}

void Cmd_ArgsBuffer(char *buffer, int32_t bufferLength)
{
    Q_strncpyz(buffer, Cmd_Args(1), bufferLength);
}

void Cmd_TokenizeString2(const char *text, int32_t maxTokens)
{
    cmd_argc = 0;
    if (text == NULL) {
        return;
    }

    size_t textLength = 0;
    while (textLength < CMD_TOKEN_BUFFER_CAPACITY &&
           text[textLength] != '\0') {
        ++textLength;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: tokenization requires the complete input and
     * its NUL inside the owned buffer capacity; rejection leaves no arguments
     * published. */
    if (textLength == CMD_TOKEN_BUFFER_CAPACITY) {
        Com_Printf("Cmd_TokenizeString2: command exceeds token buffer\n");
        return;
    }

    char *token = cmd_tokenBuffer;
    while (cmd_argc != CMD_ARGUMENT_CAPACITY) {
        --maxTokens;
        if (maxTokens == 0) {
            if (*text == '\0') {
                return;
            }

            cmd_argv[cmd_argc++] = token;
            while (*text != '\0') {
                *token++ = *text++;
            }
            *token = '\0';
            return;
        }

        for (;;) {
            while (*text != '\0' && (int8_t)*text <= (int8_t)' ') {
                ++text;
            }

            if (*text == '\0') {
                return;
            }
            if (text[0] == '/' && text[1] == '/') {
                return;
            }
            if (text[0] != '/' || text[1] != '*') {
                break;
            }

            while (*text != '\0' &&
                   (text[0] != '*' || text[1] != '/')) {
                ++text;
            }
            if (*text == '\0') {
                return;
            }
            text += 2;
        }

        cmd_argv[cmd_argc++] = token;

        if (*text == '"') {
            ++text;
            while (*text != '\0' && *text != '"') {
                *token++ = *text++;
            }
            *token++ = '\0';

            if (*text == '\0') {
                return;
            }
            ++text;
            if (*text == '\0') {
                return;
            }
            if ((int8_t)*text <= (int8_t)' ') {
                ++text;
            }
        } else {
            while ((int8_t)*text > (int8_t)' ' &&
                   *text != '"' &&
                   (text[0] != '/' || text[1] != '/') &&
                   (text[0] != '/' || text[1] != '*')) {
                *token++ = *text++;
            }
            *token++ = '\0';

            if (*text == '\0') {
                return;
            }
            if ((int8_t)*text <= (int8_t)' ') {
                ++text;
            }
        }
    }
}

void Cmd_TokenizeString(const char *text)
{
    Cmd_TokenizeString2(text, 0);
}

#if defined(WINDOWS_BEHAVIOR)
void Cmd_AddCommand(const char *name, xcommand_t function)
{
    for (cmd_function_t *command = cmd_functions;
         command != NULL;
         command = command->next) {
        if (strcmp(name, command->name) == 0) {
            if (function != NULL) {
                Com_Printf("Cmd_AddCommand: %s already defined\n", name);
            }
            return;
        }
    }

    cmd_function_t *command = malloc(sizeof(*command));
    if (command == NULL) {
        Sys_OutOfMemory();
    }
    memset(command, 0, sizeof(*command));

    const size_t nameBytes = strlen(name) + 1;
    command->name = malloc(nameBytes);
    if (command->name == NULL) {
        Sys_OutOfMemory();
    }
    memset(command->name, 0, nameBytes);
    memcpy(command->name, name, nameBytes);

    command->function = function;
    command->next = cmd_functions;
    cmd_functions = command;
}

void Cmd_RemoveCommand(const char *name)
{
    cmd_function_t **link = &cmd_functions;

    while (*link != NULL) {
        cmd_function_t *const command = *link;
        if (strcmp(name, command->name) == 0) {
            *link = command->next;
            if (command->name != NULL) {
                free(command->name);
            }
            free(command);
            return;
        }
        link = &command->next;
    }
}

void Cmd_Shutdown(void)
{
    while (cmd_functions != NULL) {
        cmd_function_t *const command = cmd_functions;
        cmd_functions = command->next;
        free(command->name);
        free(command);
    }
}
#else
void Cmd_AddCommand(const char *name, xcommand_t function)
{
    for (cmd_function_t *command = cmd_functions;
         command != NULL;
         command = command->next) {
        if (strcmp(name, command->name) == 0) {
            if (function != NULL) {
                Com_Printf("Cmd_AddCommand: %s already defined\n", name);
            }
            return;
        }
    }

    cmd_function_t *command =
        Z_MallocInternal(sizeof(*command));
    command->name = CopyStringInternal(name);
    command->function = function;
    command->next = cmd_functions;
    cmd_functions = command;
}

void Cmd_RemoveCommand(const char *name)
{
    cmd_function_t **link = &cmd_functions;

    while (*link != NULL) {
        cmd_function_t *const command = *link;
        if (strcmp(name, command->name) == 0) {
            *link = command->next;
            if (command->name != NULL) {
                Z_FreeInternal(command->name);
            }
            Z_FreeInternal(command);
            return;
        }
        link = &command->next;
    }
}

void Cmd_Shutdown(void)
{
    while (cmd_functions != NULL) {
        cmd_function_t *const command = cmd_functions;
        cmd_functions = command->next;
        Z_FreeInternal(command->name);
        Z_FreeInternal(command);
    }
}
#endif

void Cmd_CommandCompletion(name_completion_callback_t callback)
{
    for (cmd_function_t *command = cmd_functions;
         command != NULL;
         command = command->next) {
        callback(command->name);
    }
}

#if defined(WINDOWS_BEHAVIOR)
void Cmd_ExecuteString(const char *text)
{
    if (PB_ClientTrapConsole(text) != qfalse) {
        return;
    }

    Cmd_TokenizeString(text);
    if (cmd_argc == 0) {
        return;
    }

    cmd_function_t **link = &cmd_functions;
    while (*link != NULL) {
        cmd_function_t *const command = *link;
        if (Q_stricmp(cmd_argv[0], command->name) == 0) {
            *link = command->next;
            command->next = cmd_functions;
            cmd_functions = command;

            if (command->function != NULL) {
                command->function();
                return;
            }
            break;
        }
        link = &command->next;
    }

    if (Q_stricmpn(text, "pb_", 3) == 0) {
        if (Q_stricmpn(text + 3, "sv_", 3) == 0) {
            PB_DispatchServerConsoleCommand(text);
        } else {
            PB_DispatchClientConsoleCommand(text);
        }
        return;
    }

    if (Cvar_Command() != qfalse) {
        return;
    }
    if (cl_running != NULL && cl_running->integer != 0 &&
        CL_GameCommand() != qfalse) {
        return;
    }
    if (sv_running != NULL && sv_running->integer != 0 &&
        SV_GameCommand() != qfalse) {
        return;
    }
    if (cl_running != NULL && cl_running->integer != 0 &&
        UI_GameCommand() != qfalse) {
        return;
    }

    CL_ForwardCommandToServer(text);
}
#else
void Cmd_ExecuteString(const char *text)
{
    Cmd_TokenizeString(text);
    if (cmd_argc == 0) {
        return;
    }

    cmd_function_t **link = &cmd_functions;
    while (*link != NULL) {
        cmd_function_t *const command = *link;
        if (Q_stricmp(cmd_argv[0], command->name) == 0) {
            *link = command->next;
            command->next = cmd_functions;
            cmd_functions = command;

            if (command->function != NULL) {
                command->function();
                return;
            }
            break;
        }
        link = &command->next;
    }

    if (Q_strncmp(text, "pb_", 3) == 0) {
        if (Q_strncmp(text + 3, "sv_", 3) == 0) {
            PB_CallServerSbGlobal(Q_COMMAND_PB_CONSOLE_OPCODE,
                                  Q_COMMAND_PB_CONSOLE_CLIENT_NUM,
                                  (uint32_t)strlen(text) + UINT32_C(1),
                                  text);
        }
        return;
    }

    if (Cvar_Command() != qfalse) {
        return;
    }
    if (cl_running != NULL && cl_running->integer != 0 &&
        CL_GameCommand() != qfalse) {
        return;
    }
    if (sv_running != NULL && sv_running->integer != 0 &&
        SV_GameCommand() != qfalse) {
        return;
    }
    if (cl_running != NULL && cl_running->integer != 0 &&
        UI_GameCommand() != qfalse) {
        return;
    }

    CL_ForwardCommandToServer(text);
}
#endif

void Cmd_List_f(void)
{
    const char *filter = Cmd_Argc() > 1 ? Cmd_Argv(1) : NULL;
    int32_t commandCount = 0;

    for (cmd_function_t *command = cmd_functions;
         command != NULL;
         command = command->next) {
        if (filter == NULL ||
            Com_Filter(filter, command->name, qfalse) != qfalse) {
            Com_Printf("%s\n", command->name);
            ++commandCount;
        }
    }

    Com_Printf("%i commands\n", commandCount);
}

void Cmd_Init(void)
{
    Cmd_AddCommand("cmdlist", Cmd_List_f);
    Cmd_AddCommand("exec", Cmd_Exec_f);
    Cmd_AddCommand("vstr", Cmd_Vstr_f);
    Cmd_AddCommand("echo", Cmd_Echo_f);
    Cmd_AddCommand("wait", Cmd_Wait_f);
    Cmd_AddCommand("showchecksum", Cmd_ShowChecksum_f);
}
