#ifndef QCOMMON_Q_COMMAND_H
#define QCOMMON_Q_COMMAND_H

#include "command_types.h"
#include "q_shared_types.h"
#include "qcommon_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*name_completion_callback_t)(const char *name);

/* The original Windows client and Linux dedicated engine keep different
 * physical arrangements for this storage.  Each engine tree therefore owns
 * its original globals while the common command implementation uses the same
 * canonical names and types. */
extern int32_t cmd_wait;
extern char cmd_textData[CBUF_TEXT_CAPACITY];
extern cbuf_t cmd_text;
extern char cmd_args[CMD_ARGS_CAPACITY];
extern cmd_function_t *cmd_functions;
extern const char *cmd_argv[CMD_ARGUMENT_CAPACITY];
extern char cmd_tokenBuffer[CMD_TOKEN_BUFFER_CAPACITY];
extern int32_t cmd_argc;

void Cbuf_Init(void);
void Cbuf_AddText(const char *text);
void Cbuf_InsertText(const char *text);
void Cbuf_ExecuteText(cbufExec_t executionMode, const char *text);
void Cbuf_Execute(void);

int32_t Cmd_Argc(void);
const char *Cmd_Argv(int32_t argumentIndex);
void Cmd_ArgvBuffer(int32_t argumentIndex, char *buffer,
                    int32_t bufferLength);
char *Cmd_Args(int32_t firstArgument);
void Cmd_ArgsBuffer(char *buffer, int32_t bufferLength);
void Cmd_TokenizeString2(const char *text, int32_t maxTokens);
void Cmd_TokenizeString(const char *text);

void Cmd_AddCommand(const char *name, xcommand_t function);
void Cmd_RemoveCommand(const char *name);
void Cmd_Shutdown(void);
void Cmd_CommandCompletion(name_completion_callback_t callback);
void Cmd_ExecuteString(const char *text);

void Cmd_Wait_f(void);
void Cmd_Exec_f(void);
void Cmd_ShowChecksum_f(void);
void Cmd_Vstr_f(void);
void Cmd_Echo_f(void);
void Cmd_List_f(void);
void Cmd_Init(void);

#ifdef __cplusplus
}
#endif

#endif
