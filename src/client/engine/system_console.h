#ifndef CODUOMP_SYSTEM_CONSOLE_H
#define CODUOMP_SYSTEM_CONSOLE_H

#include "q_shared.h"

#if defined(_WIN32)
#include <windows.h>
#endif

extern void *sysApplicationInstance;
extern void *sysConsoleWindow;
extern void *sysConsoleOutputWindow;
extern void *sysConsoleInputWindow;
extern void *sysConsoleErrorWindow;
extern void *sysConsoleFont;
extern qboolean sysWindowActive;
extern qboolean sysWindowMinimized;

void Sys_Print(const char *text);
char *Sys_ConsoleInput(void);
void Sys_CreateConsole(void);
void Sys_DestroyConsole(void);
void Sys_SetErrorText(const char *text);
void Sys_ShowConsole(int32_t visibility, qboolean quitOnClose);
void WIN_DisableAltTab(void);
void WIN_EnableAltTab(void);
void AppActivate(qboolean active, qboolean minimized);

#if defined(_WIN32)
LRESULT CALLBACK ConWndProc(HWND window, UINT message,
                            WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InputLineWndProc(HWND window, UINT message,
                                  WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MainWndProc(HWND window, UINT message,
                             WPARAM wParam, LPARAM lParam);
#endif

#endif
