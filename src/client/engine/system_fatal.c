#include "system_fatal.h"

#include "system_localization.h"
#include "system_platform.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#else
#include "platform/sdl_platform.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: present explicit session-only recovery choices
 * after localization startup, including when the renderer is unavailable. */
int coduomp_config_recovery_dialog(const char *message, const char *backup)
{
#if defined(_WIN32)
    enum { CODUOMP_DIALOG_BACKUP = 1001, CODUOMP_DIALOG_DEFAULTS, CODUOMP_DIALOG_EXIT };
    char detail[6144];
    snprintf(detail, sizeof(detail), "%s\n\n%s%s\n\nThe original settings stay protected. To replace them later, use config_restore in the console; it preserves the original first.",
        message, backup ? "Available recovery snapshot:\n" : "No complete recovery snapshot is available.", backup ? backup : "");
    wchar_t wide[6144];
    MultiByteToWideChar(CP_ACP, 0, detail, -1, wide, (int)(sizeof(wide) / sizeof(wide[0])));
    HMODULE controls = LoadLibraryW(L"comctl32.dll");
    typedef HRESULT (WINAPI *coduomp_task_dialog_t)(const TASKDIALOGCONFIG *, int *, int *, BOOL *);
    coduomp_task_dialog_t show = controls ? (coduomp_task_dialog_t)(void *)GetProcAddress(controls, "TaskDialogIndirect") : NULL;
    TASKDIALOG_BUTTON buttons[] = {
        {CODUOMP_DIALOG_BACKUP, L"Use backup for this session"},
        {CODUOMP_DIALOG_DEFAULTS, L"Use temporary defaults"}, {CODUOMP_DIALOG_EXIT, L"Exit"}
    };
    TASKDIALOGCONFIG dialog = {0};
    dialog.cbSize = sizeof(dialog);
    dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
    dialog.pszWindowTitle = L"Settings could not be loaded";
    dialog.pszContent = wide;
    dialog.cButtons = backup ? 3 : 2;
    dialog.pButtons = backup ? buttons : buttons + 1;
    dialog.nDefaultButton = CODUOMP_DIALOG_EXIT;
    int selected = CODUOMP_DIALOG_EXIT;
    HRESULT status = show ? show(&dialog, &selected, NULL, NULL) : E_NOTIMPL;
    if (controls)
        FreeLibrary(controls);
    if (SUCCEEDED(status))
        return selected == CODUOMP_DIALOG_BACKUP ? 1 : selected == CODUOMP_DIALOG_DEFAULTS ? 0 : -1;
    /* Older common-controls installations still get an explicit mapping and
     * default to cancellation instead of selecting a reset. */
    snprintf(detail, sizeof(detail), "%s\n\nYes: %s\nNo: Use temporary defaults\nCancel: Exit", message,
        backup ? "Use backup for this session" : "Use temporary defaults (no backup available)");
    int response = MessageBoxA(NULL, detail, "Settings could not be loaded", MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON3);
    return response == IDYES && backup ? 1 : response == IDYES || response == IDNO ? 0 : -1;
#else
    return coduomp_sdl_config_recovery_dialog(message, backup);
#endif
}

enum {
    SYS_ERROR_DIALOG_TYPE = 16,
    SYS_FATAL_EXIT_STATUS = -1
};

#if !defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: native platform implementation of the original
 * Win32 error-dialog operation. */
void Sys_ShowErrorDialog(const char *message, const char *title,
                         uint32_t dialogType)
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
