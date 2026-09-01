#ifndef QCOMMON_COM_COMMAND_HANDLERS_H
#define QCOMMON_COM_COMMAND_HANDLERS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
[[noreturn]] void Com_Quit_f(void);
#else
_Noreturn void Com_Quit_f(void);
#endif
void Com_Error_f(void);
void Com_Freeze_f(void);
void Com_Crash_f(void);

#ifdef __cplusplus
}
#endif

#endif
