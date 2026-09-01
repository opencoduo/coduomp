#ifndef CODUO_CORE_RUNTIME_PRIVATE_H
#define CODUO_CORE_RUNTIME_PRIVATE_H

#include <float.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#if !defined(_WIN32)
#include <termios.h>
#endif

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "qcommon/com_command_handlers.h"
#include "qcommon/com_config.h"
#include "qcommon/com_frame.h"
#include "qcommon/com_event_queue.h"
#include "qcommon/com_event_loop.h"
#include "qcommon/com_lifecycle.h"
#include "qcommon/com_memory.h"
#include "qcommon/com_string.h"
#include "qcommon/com_parse.h"
#include "qcommon/com_redirect.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/com_startup_commands.h"
#include "qcommon/hunk.h"
#include "qcommon/net_text.h"
#include "qcommon/q_bits.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_path.h"
#include "qcommon/q_shared_misc.h"
#include "qcommon/q_string.h"

#if defined(_WIN32)
typedef jmp_buf coduo_jump_buffer_t;
typedef struct coduo_terminal_state_s {
    int unused;
} coduo_terminal_state_t;
#define CODUO_SETJMP(buffer, saveMask) setjmp(buffer)
#else
typedef sigjmp_buf coduo_jump_buffer_t;
typedef struct termios coduo_terminal_state_t;
#define CODUO_SETJMP(buffer, saveMask) sigsetjmp(buffer, saveMask)
#endif

struct stat;

#define CODUO_COM_PRINT_CHANNEL_DEFAULT 0
#define CODUO_COM_PRINT_CHANNEL_DEVELOPER 4
#define CODUO_COM_PB_PRINT_LIMIT 4096
#define CODUO_COM_PRINT_FORMAT_BUFFER_SIZE 0x1000
#define CODUO_COM_REDIRECT_NUL_BYTE 1
#define CODUO_COM_DEDICATED_DISABLED 0
#define CODUO_COM_LOGFILE_DISABLED 0
#define CODUO_COM_LOG_FILE_CLOSED_HANDLE 0
#define CODUO_COM_LOGFILE_SYNC_THRESHOLD 1
#define CODUO_COM_CONSOLE_STUB_ARG2 0
#define CODUO_COM_CONSOLE_STUB_ARG3 0

#define SYS_STDIN_FILE_DESCRIPTOR 0

enum {
    SYS_DELAYED_PROCESS_COMMAND_SIZE = 1024,
    SYS_TTY_INPUT_RETURN_BUFFER_SIZE = 256,
    SYS_STDIN_INACTIVE = 0,
    SYS_STDIN_ACTIVE = 1,
    SYS_TTY_HISTORY_RESET_CURSOR = -1
};
#define SYS_F_GETFL_COMMAND 3
#define SYS_F_GETFL_UNUSED_ARGUMENT 0
#define SYS_F_SETFL_COMMAND 4
#define SYS_LINUX_O_NONBLOCK 0x800
#define SYS_ERROR_EXIT_STATUS 1
#define SYS_OUT_OF_MEMORY_EXIT_STATUS -1

#ifndef CODUO_ENGINE_HOST_LONG_DOUBLE_IS_X87
#define CODUO_ENGINE_HOST_LONG_DOUBLE_IS_X87 \
    (LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384)
#endif

#ifndef CODUO_ENGINE_HAS_X87_INLINE_ASM
#if (defined(__i386__) || defined(__i486__) || defined(__i586__) || \
     defined(__i686__) || defined(__x86_64__)) && \
    CODUO_ENGINE_HOST_LONG_DOUBLE_IS_X87 && \
    (defined(__GNUC__) || defined(__clang__))
#define CODUO_ENGINE_HAS_X87_INLINE_ASM 1
#else
#define CODUO_ENGINE_HAS_X87_INLINE_ASM 0
#endif
#endif

extern cvar_t *dedicated;
extern cvar_t *com_developer;
extern cvar_t *com_logfile;
extern cvar_t *ttycon;
extern cvar_t *cl_shownet;
extern cvar_t *host_cvar_com_animCheck_pointer_slot;
extern cvar_t *host_cvar_com_maxfps_pointer_slot;
extern cvar_t *com_speeds;
extern cvar_t *cl_running;
extern cvar_t *host_cvar_developer_script_pointer_slot;
extern cvar_t *sv_running;
extern cvar_t *host_cvar_viewlog_pointer_slot;

extern coduo_jump_buffer_t com_frameAbortContext;
extern int32_t com_errorEntered;
extern int32_t com_frameNumber;
extern int32_t com_timeBackend;
extern int32_t com_timeFrontend;
extern int32_t com_timeGame;
extern qboolean com_printMessageOpeningLog;
extern qboolean com_vprintfOpeningLog;
extern int32_t com_frameTime;
extern qboolean sv_frameRunning;

extern int32_t hunk_logFile;
extern hunk_log_block_t *hunk_logBlocks;
extern size_t hunk_totalZoneSize;

extern int32_t sys_stdinActive;
extern int32_t sys_ttyConsoleActive;
extern uint16_t sys_snapVectorSavedFpuControlWord;
extern uint16_t sys_snapVectorFpuControlWord;
extern coduo_terminal_state_t sys_ttyOriginalTermios;
extern int32_t sys_ttyEraseChar;
extern int32_t sys_ttyEofChar;
extern int32_t
    sys_ttyOutputSuppressionDepth;
extern int32_t sys_ttyHistoryCursor;
extern int32_t sys_ttyHistoryCount;
extern char sys_ttyInputReturnBuffer[SYS_TTY_INPUT_RETURN_BUFFER_SIZE];
extern console_input_field_t sys_ttyCurrentLine;
extern console_input_field_t sys_ttyHistory[CON_HISTORY_FIELD_COUNT];
extern sysEvent_t sys_eventQueue[SYS_EVENT_QUEUE_COUNT];
extern int32_t sys_eventQueueProducer;
extern int32_t sys_eventQueueConsumer;
extern uint8_t sys_packetBuffer[MAX_MSGLEN];
extern char sys_delayedProcessCommand[SYS_DELAYED_PROCESS_COMMAND_SIZE];

void PB_Print(const char *message, int32_t limit);
void PB_StartServer(void);
void PB_RunServerFrame(void);
void Cbuf_ExecuteText(cbufExec_t exec_when, const char *text);
cvar_t *Cvar_Get(const char *name, const char *value,
                              uint32_t flags);
void Cvar_Set(const char *name, const char *value);
const char *Cvar_VariableString(const char *name);
void CL_ConsolePrint(int32_t channel, const char *message,
                     int32_t arg2, int32_t arg3);
void CL_MapLoading(void);
qboolean CL_CDKeyValidate(const char *cdkey, const char *hash);
void CL_Frame(int32_t rawMsec, int32_t scaledMsec);
void CL_MouseEvent(int32_t value,
                   int32_t value2,
                   int32_t time);
void CL_PacketEvent(netadr_t from, msg_t *msg,
                    int32_t time);
void CL_CharEvent(int32_t value);
void CL_KeyEvent(int32_t value,
                 int32_t value2,
                 int32_t time);
void CL_JoystickEvent(int32_t value,
                      int32_t value2,
                      int32_t time);
void Key_WriteBindings(int32_t handle);
void Sys_Print(const char *message);
qboolean FS_Initialized(void);
int32_t FS_FOpenFileWrite(const char *qpath);
int32_t FS_FOpenTextFileWrite(const char *qpath);
void FS_ForceFlush(int32_t handle);
int32_t FS_LoadStack(void);
int32_t FS_Read(void *buffer, int32_t length, int32_t handle);
int32_t FS_Write(const void *buffer, int32_t length,
                 int32_t handle);
int32_t FS_Seek(int32_t handle, int32_t offset, int32_t origin);
void Cmd_Shutdown(void);
void Cvar_Shutdown(void);
/*
 * Checked 2026-06-29: these remaining FUN_ hooks are default-named no-op or
 * identity client/platform stubs in the Linux dedicated binary. Their bodies
 * and current callers do not prove source-level names.
 */
void Com_SetRecommended(void);
void CL_Disconnect(qboolean showMainMenu);
void CL_Shutdown(void);
void CL_ShutdownAll(void);
void CL_InitKeyCommands(void);
void CL_StartHunkUsers(void);
void CL_CDDialog(void);
void Sys_Input(void);
void Sys_SendKeyEvents(void);
void Com_ClearServerFrameRunningFlag(void);
void Sys_TTYResetLine(console_input_field_t *line);
void Sys_TTYCompleteLine(console_input_field_t *line);
void Sys_TTYDrainInput(void);
void Sys_TTYErasePreviousChar(void);
void Sys_TTYHideInputLine(void);
void Sys_TTYShowInputLine(void);
const char *NET_ErrorString(void);
void Sys_TTYStoreHistoryLine(console_input_field_t *line);
console_input_field_t *Sys_TTYPreviousHistoryLine(void);
console_input_field_t *Sys_TTYNextHistoryLine(void);
void Sys_ShowConsole(int32_t visLevel, qboolean quitOnClose);
/*
 * Checked 2026-06-29: the remaining Sys_* address-band FUN_ hooks are
 * constant-return or empty platform hooks; no source-level names are proven by
 * the binary.
 */
int32_t FUN_080cb267(void);
void FUN_080cb271(void);
void FUN_080cb276(void);
void CL_Init(void);
void Com_InitServerRuntimePools(void);
const char *Com_BuildVersionString(void);

void Com_PrintMessage(int32_t channel, const char *message);
int32_t Com_VPrintf(const char *format, va_list args);
void Com_Error(errorParm_t code, const char *format, ...);
void Com_ErrorCleanup(void);
void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
void Com_LogPrintf(const char *format, ...);
void Com_Init(char *commandLine);
void Com_Frame(void);
int32_t Com_ApplyConfigureFileChecksum(void);
void Com_InitScriptRuntime(void);
void Sys_Error(const char *format, ...);
void Sys_OutOfMemory(void);
int32_t Sys_Milliseconds(void);
char *Sys_ConsoleInput(void);
const char *Sys_Cwd(void);
void Sys_Init(void);
void Sys_CheckCrashOrRerun(void);
int32_t Sys_GetProcessorId(void);
qboolean Sys_InfoChanged(void);
qboolean Sys_ConfigureChecksumChanged(int32_t checksum);
qboolean Sys_GetPacket(netadr_t *from, msg_t *msg);
qboolean Sys_IsLANAddress(netadr_t adr);
qboolean Sys_StringToAdr(const char *text, netadr_t *adr);
void Sys_SendPacket(int32_t length,
                    const void *data,
                    netadr_t to);
void Sys_SendPacketByName(const char *address, uint16_t port,
                          const void *data, int32_t length);
void NET_Sleep(int32_t msec);
sysEvent_t Sys_GetEvent(void);
void *Com_ZoneDebugAlloc(size_t size);
void *Com_ZoneDebugAllocClear(size_t size);
void Com_DebugFree(void *ptr);
const char *Sys_GetCurrentUser(void);
void Sys_SnapVector(vec3_t vector);
void Sys_SnapVectorWithControlWord(vec3_t vector, uint16_t controlWord);
void Sys_InitInput(void);
void Sys_ShutdownInput(void);
void IN_Restart_f(void);
void Sys_ShutdownTerminalConsole(void);
void Sys_InitTerminalConsole(void);
int32_t Sys_Stat(const char *path, struct stat *statbuf);
void Sys_Exit(int status);
void Sys_Quit(void);
void NET_Init(void);
void MSG_Init(msg_t *msg, uint8_t *data,
              int32_t length);

#endif
