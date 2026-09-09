#include "q_command.h"
#include "config_script.h"
#include "config_profile.h"

#include "q_checksum.h"
#include "q_cvar.h"
#include "q_memory.h"
#include "q_path.h"
#include "q_string.h"
#include "qcommon_limits.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

typedef struct coduomp_command_origin_s {
    struct coduomp_command_origin_s *parent;
    coduomp_config_profile_t *profile;
    size_t references;
    unsigned depth, line, includeLine, commands;
    qboolean canceled, checked;
    char source[CODUOMP_CONFIG_PATH];
} coduomp_command_origin_t;

typedef struct coduomp_command_frame_s {
    struct coduomp_command_frame_s *previous;
    coduomp_command_origin_t *origin;
} coduomp_command_frame_t;

enum { CODUOMP_EXEC_DEPTH = 32, CODUOMP_EXEC_COMMANDS = 8192 };
static coduomp_command_origin_t *coduomp_command_origins[CBUF_TEXT_CAPACITY];
static coduomp_command_frame_t *coduomp_command_frame;

/* NOT_FROM_ORIGINAL_SOURCE: generated commands inherit their executing file's
 * origin, including while a command recursively executes another command. */
static coduomp_command_origin_t *coduomp_command_origin(void)
{
    return coduomp_command_frame ? coduomp_command_frame->origin : NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: release ownership only after all queued bytes,
 * active execution frames, and descendant includes have finished. */
static void coduomp_command_release(coduomp_command_origin_t *origin)
{
    if (!origin || --origin->references)
        return;
    coduomp_command_origin_t *parent = origin->parent;
    if (origin->checked)
        coduomp_config_load_reference(origin->profile, -1);
    free(origin);
    coduomp_command_release(parent);
}

/* NOT_FROM_ORIGINAL_SOURCE: report failures with the complete include chain. */
static void coduomp_command_failure(coduomp_command_origin_t *origin, const char *reason)
{
    if (!origin || !origin->checked)
        return;
    coduomp_config_fail(origin->profile, origin->source, origin->line, reason);
    for (coduomp_command_origin_t *child = origin; child->parent; child = child->parent)
        Com_Printf("  included from %s:%u\n", child->parent->source, child->includeLine);
    for (coduomp_command_origin_t *ancestor = origin; ancestor; ancestor = ancestor->parent) {
        if (ancestor->checked)
            ancestor->canceled = qtrue;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: ordinary exec failures remain console diagnostics
 * and cannot place the protected settings profile into recovery. */
static void coduomp_command_reject(coduomp_config_profile_t *profile, const char *source, unsigned line, const char *reason, qboolean checked)
{
    if (checked)
        coduomp_config_fail(profile, source, line, reason);
    else
        Com_Printf("%s:%u: %s\n", source, line, reason);
}

/* NOT_FROM_ORIGINAL_SOURCE: match metadata moves to command-buffer moves. */
static void coduomp_command_tag(size_t offset, size_t length, coduomp_command_origin_t *origin)
{
    for (size_t i = 0; i < length; ++i)
        coduomp_command_origins[offset + i] = origin;
    if (origin)
        origin->references += length;
}

/* NOT_FROM_ORIGINAL_SOURCE: preflight saved settings and queue an entire owned
 * file, with no visible internal sentinels or partially inserted tail. Ordinary
 * exec scripts preserve the interpreter's per-command error handling. */
static qboolean coduomp_command_queue_owned(coduomp_config_profile_t *profile, const char *source, const char *data, size_t size, coduomp_command_origin_t *parent, qboolean append, qboolean checked)
{
    coduomp_config_error_t error;
    if (checked && !coduomp_config_validate(data, size, &error)) {
        coduomp_config_fail(profile, source, error.line, error.reason);
        return qfalse;
    }
    if ((parent && parent->depth >= CODUOMP_EXEC_DEPTH) || cmd_text.cursize < 0 ||
        size + 1 > (size_t)(cmd_text.maxsize - cmd_text.cursize)) {
        coduomp_command_reject(profile, source, 1, parent && parent->depth >= CODUOMP_EXEC_DEPTH ?
            "config include depth exceeded" : "command queue cannot accept this complete file", checked);
        return qfalse;
    }
    coduomp_command_origin_t *origin = calloc(1, sizeof(*origin));
    if (!origin) {
        coduomp_command_reject(profile, source, 1, "not enough memory to queue config", checked);
        return qfalse;
    }
    origin->profile = profile;
    origin->checked = checked;
    origin->parent = parent;
    origin->includeLine = parent ? parent->line : 0;
    origin->line = 1;
    origin->depth = parent ? parent->depth + 1 : 1;
    snprintf(origin->source, sizeof(origin->source), "%s", source);
    if (parent)
        ++parent->references;
    if (checked)
        coduomp_config_load_reference(profile, 1);
    size_t inserted = size + 1;
    size_t offset = append ? (size_t)cmd_text.cursize : 0;
    if (!append) {
        memmove(cmd_text.data + inserted, cmd_text.data, (size_t)cmd_text.cursize);
        memmove(coduomp_command_origins + inserted, coduomp_command_origins, (size_t)cmd_text.cursize * sizeof(*coduomp_command_origins));
    }
    memcpy(cmd_text.data + offset, data, size);
    cmd_text.data[offset + size] = '\n';
    coduomp_command_tag(offset, inserted, origin);
    cmd_text.cursize += (int32_t)inserted;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: ordinary includes retain their executing parent. */
qboolean coduomp_command_queue_config(coduomp_config_profile_t *profile, const char *source, const char *data, size_t size)
{
    return coduomp_command_queue_owned(profile, source, data, size, coduomp_command_origin(), qfalse, qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: filesystem transitions capture the selected
 * profile's bytes now, before another restart can change VFS resolution. */
void coduomp_command_queue_profile(const char *file, qboolean append)
{
    coduomp_config_profile_t *profile = coduomp_config_current_profile();
    char *data, source[CODUOMP_CONFIG_PATH];
    size_t size;
    if (coduomp_config_read(profile, file, qfalse, qtrue, &data, &size, source) == 1) {
        coduomp_command_queue_owned(profile, source, data, size, NULL, append, qtrue);
        free(data);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: used to keep recovery actions out of scripts. */
qboolean coduomp_command_config_active(void)
{
    return coduomp_command_origin() != NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: longjmp cannot leave a successful-looking load or
 * a dangling execution frame. Remaining bytes from failed origins are skipped. */
void coduomp_command_abort_config(void)
{
    coduomp_config_abort_load();
    while (coduomp_command_frame) {
        coduomp_command_frame_t *frame = coduomp_command_frame;
        coduomp_command_frame = frame->previous;
        coduomp_command_failure(frame->origin, "command execution did not complete");
        coduomp_command_release(frame->origin);
        free(frame);
    }
}

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
    for (int32_t i = 0; i < cmd_text.cursize; ++i)
        coduomp_command_release(coduomp_command_origins[i]);
    memset(coduomp_command_origins, 0, sizeof(coduomp_command_origins));
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
        coduomp_command_failure(coduomp_command_origin(), "generated input exceeds command queue capacity");
        return;
    }

    coduomp_command_tag((size_t)cmd_text.cursize, textLength, coduomp_command_origin());
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
        coduomp_command_failure(coduomp_command_origin(), "generated input exceeds command queue capacity");
        return;
    }
    const int32_t insertLength = (int32_t)textLength + 1;
    const int32_t newSize = cmd_text.cursize + insertLength;
    memmove(coduomp_command_origins + insertLength, coduomp_command_origins, (size_t)cmd_text.cursize * sizeof(*coduomp_command_origins));
    coduomp_command_tag(0, (size_t)insertLength, coduomp_command_origin());

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

/* NOT_FROM_ORIGINAL_SOURCE: saved-settings boundaries match preflight, while
 * ordinary commands retain the original delimiters. Queued origins remain
 * live across wait, nested exec, and filesystem changes. */
void Cbuf_Execute(void)
{
    char command[CBUF_COMMAND_CAPACITY];
    while (cmd_text.cursize != 0) {
        if (cmd_wait != 0) {
            --cmd_wait;
            return;
        }
        coduomp_command_origin_t *origin = coduomp_command_origins[0];
        size_t length = coduomp_command_span(cmd_text.data, (size_t)cmd_text.cursize, origin && origin->checked);
        size_t consumed = length + (length < (size_t)cmd_text.cursize);
        coduomp_command_frame_t *frame = malloc(sizeof(*frame));
        if (!frame) {
            coduomp_command_failure(origin, "not enough memory to execute settings");
            return;
        }
        frame->previous = coduomp_command_frame;
        frame->origin = origin;
        coduomp_command_frame = frame;
        if (origin)
            ++origin->references;
        qboolean execute = length < sizeof(command);
        if (!execute) {
            Com_Printf("Command exceeds execution capacity; skipped.\n");
            if (length >= sizeof(command))
                coduomp_command_failure(origin, "command exceeds execution capacity");
        } else {
            memcpy(command, cmd_text.data, length);
            command[length] = '\0';
        }
        if (origin && origin->checked && origin->profile != coduomp_config_current_profile()) {
            coduomp_command_failure(origin, "queued config belongs to a departed profile");
            coduomp_config_pause("a profile changed before queued settings finished");
            execute = qfalse;
        }
        unsigned lines = 0;
        for (size_t i = 0; i < consumed; ++i) {
            if (cmd_text.data[i] == '\n' || (cmd_text.data[i] == '\r' &&
                (i + 1 == (size_t)cmd_text.cursize || cmd_text.data[i + 1] != '\n')))
                ++lines;
            coduomp_command_release(coduomp_command_origins[i]);
        }
        cmd_text.cursize -= (int32_t)consumed;
        memmove(cmd_text.data, cmd_text.data + consumed, (size_t)cmd_text.cursize);
        memmove(coduomp_command_origins, coduomp_command_origins + consumed,
            (size_t)cmd_text.cursize * sizeof(*coduomp_command_origins));
        for (coduomp_command_origin_t *ancestor = origin; ancestor; ancestor = ancestor->parent) {
            if (ancestor->canceled)
                execute = qfalse;
        }
        if (execute)
            Cmd_ExecuteString(command);
        if (origin)
            origin->line += lines;
        coduomp_command_frame = frame->previous;
        coduomp_command_release(origin);
        free(frame);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: saved settings are checked as whole files; ordinary
 * server/mod scripts retain command-by-command execution. Both queue their
 * exact read snapshot with its source profile and bounded include ancestry. */
void Cmd_Exec_f(void)
{
    if (Cmd_Argc() != 2) {
        Com_Printf("exec <filename> : execute a script file\n");
        if (coduomp_command_origin() && coduomp_command_origin()->checked)
            coduomp_command_failure(coduomp_command_origin(), "exec needs a filename");
        return;
    }
    char filename[MAX_QPATH];
    const char *argument = Cmd_Argv(1);
    if (!coduomp_config_filename(argument, filename, sizeof(filename))) {
        coduomp_command_failure(coduomp_command_origin(), "exec filename exceeds supported length");
        Com_Printf("Config filename is too long.\n");
        return;
    }
    coduomp_command_origin_t *parent = coduomp_command_origin();
    const qboolean checked = (parent && parent->checked) || coduomp_config_is_primary(filename);
    coduomp_config_profile_t *profile = parent ? parent->profile : coduomp_config_current_profile();
    char *data, source[CODUOMP_CONFIG_PATH];
    size_t size;
    if (coduomp_config_read(profile, filename, parent != NULL, checked, &data, &size, source) != 1) {
        Com_Printf("couldn't exec %s\n", filename);
        if (parent && parent->checked)
            coduomp_command_failure(parent, "included config could not be loaded");
        return;
    }
    const cvar_t *consoleLockout = Cvar_FindVar("sv_console_lockout");
    char notice[CBUF_COMMAND_CAPACITY] = "";
    if (consoleLockout && consoleLockout->integer)
        snprintf(notice, sizeof(notice), "say Server exec: %s, size: %i, checksum: %i", filename,
            (int)size, (int32_t)Com_BlockChecksum(data, (int32_t)size));
    /* Ordinary exec inserts a NUL-terminated string but reports the complete
     * file's size and checksum. Checked settings reject embedded NULs. */
    const size_t queuedSize = checked ? size : strlen(data);
    if (queuedSize + 1 + (notice[0] ? strlen(notice) + 1 : 0) > (size_t)(cmd_text.maxsize - cmd_text.cursize)) {
        coduomp_command_reject(profile, source, 1, "command queue cannot accept this complete file", checked);
        coduomp_command_failure(parent, "included config could not be queued");
        free(data);
        return;
    }
    if (notice[0])
        Cbuf_InsertText(notice);
    if (coduomp_command_queue_owned(profile, source, data, queuedSize, parent, qfalse, checked))
        Com_Printf("execing %s\n", filename);
    else
        coduomp_command_failure(parent, "included config could not be queued");
    free(data);
}

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

/* NOT_FROM_ORIGINAL_SOURCE: publish bounded execution tokens using the same
 * parser as config validation, while retaining end-of-input token termination.
 * Saved settings have already passed the stricter whole-file preflight. */
void Cmd_TokenizeString2(const char *text, int32_t maxTokens)
{
    cmd_argc = 0;
    if (!text)
        return;
    coduomp_config_tokens_t tokens;
    coduomp_config_error_t error;
    const coduomp_command_origin_t *origin = coduomp_command_origin();
    if (!coduomp_command_tokens(text, strlen(text), maxTokens, origin && origin->checked, &tokens, &error)) {
        Com_Printf("Command could not be parsed: %s\n", error.reason);
        coduomp_command_failure(coduomp_command_origin(), error.reason);
        return;
    }
    size_t used = tokens.argc ? (size_t)(tokens.argv[tokens.argc - 1] - tokens.text) + strlen(tokens.argv[tokens.argc - 1]) + 1 : 0;
    if (used)
        memcpy(cmd_tokenBuffer, tokens.text, used);
    for (int i = 0; i < tokens.argc; ++i)
        cmd_argv[i] = cmd_tokenBuffer + (tokens.argv[i] - tokens.text);
    cmd_argc = tokens.argc;
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

/* NOT_FROM_ORIGINAL_SOURCE: bound actual command expansion, without charging
 * blank lines or comments against a config's execution budget. */
static qboolean coduomp_command_count(void)
{
    for (coduomp_command_origin_t *origin = coduomp_command_origin(); origin; origin = origin->parent) {
        if (origin->canceled)
            return qfalse;
        if (++origin->commands > CODUOMP_EXEC_COMMANDS) {
            coduomp_command_failure(origin, "config command expansion limit exceeded");
            return qfalse;
        }
    }
    return qtrue;
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
    if (!coduomp_command_count())
        return;

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
    if (!coduomp_command_count())
        return;

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
