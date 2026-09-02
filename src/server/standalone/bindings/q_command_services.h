#ifndef QCOMMON_Q_COMMAND_SERVICES_H
#define QCOMMON_Q_COMMAND_SERVICES_H

#include <stdint.h>
#include <string.h>

void PB_CallServerSbGlobal(int32_t command, int32_t clientNum, uint32_t length, const char *text);

/* NOT_FROM_ORIGINAL_SOURCE: the standalone target has no PunkBuster client
 * console hook.  Returning false leaves ordinary command dispatch intact. */
#define PB_ClientTrapConsole(text) ((void)(text), qfalse)

/* NOT_FROM_ORIGINAL_SOURCE: the standalone target has no client PunkBuster
 * command consumer.  The Windows command body still consumes the command. */
#define PB_DispatchClientConsoleCommand(text) ((void)(text))

/* NOT_FROM_ORIGINAL_SOURCE: bind the Windows command body's server dispatch
 * edge to the standalone server backend using the same opcode, client
 * sentinel, and NUL-inclusive length as the original Linux server body. */
#define PB_DispatchServerConsoleCommand(text) \
    PB_CallServerSbGlobal(Q_COMMAND_PB_CONSOLE_OPCODE, Q_COMMAND_PB_CONSOLE_CLIENT_NUM, (uint32_t)strlen(text) + UINT32_C(1), text)

#endif
