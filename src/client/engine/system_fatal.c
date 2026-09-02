#include "system_fatal.h"

#include "system_localization.h"
#include "system_platform.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include "platform/sdl_platform.h"
#endif

#include <stdlib.h>

enum {
    SYS_ERROR_DIALOG_TYPE = 16,
    SYS_FATAL_EXIT_STATUS = -1
};

#if !defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: native platform implementation of the original
 * Win32 error-dialog operation. */
void Sys_ShowErrorDialog(const char *message, const char *title, uint32_t dialogType)
{
    (void)dialogType;
    CoduoSDL_ShowErrorDialog(message, title);
}
#endif

/* Source: CoDUOMP.exe 0x0046af50..0x0046af81.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046af50_0046af82.mcode. The original
 * calls MessageBoxA directly; Sys_ShowErrorDialog is the portable platform
 * boundary for that one operation. */
void Sys_OutOfMemory(void)
{
    const char *title;
    const char *message;

    Sys_Shutdown();
    title = Sys_LocalizeString("WIN_OUT_OF_MEM_TITLE");
    message = Sys_LocalizeString("WIN_OUT_OF_MEM_BODY");
#if defined(_WIN32)
    MessageBoxA(NULL, message, title, SYS_ERROR_DIALOG_TYPE);
#else
    Sys_ShowErrorDialog(message, title, SYS_ERROR_DIALOG_TYPE);
#endif
    exit(SYS_FATAL_EXIT_STATUS);
}
