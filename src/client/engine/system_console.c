#include "system_console.h"

#include "system_localization.h"
#include "system_platform.h"
#include "system_input.h"
#include "system_event.h"
#include "client/cgame.h"
#include "client/console.h"
#include "platform/crt_boundary.h"
#include "renderer/renderer_cvars.h"
#include "server/server.h"
#include "sound/miles_boundary.h"
#include "qcommon/q_string.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

enum {
    SYS_CONSOLE_PRINT_SOURCE_LIMIT = 16383,
    SYS_CONSOLE_PRINT_BUFFER_CAPACITY = 32768,
    SYS_CONSOLE_TEXT_RETAIN_LIMIT = 16384,
    SYS_CONSOLE_ERROR_TEXT_CAPACITY = 512,
    SYS_CONSOLE_INPUT_CAPACITY = 512,
    SYS_CONSOLE_INPUT_APPEND_LIMIT = 507,
    SYS_CONSOLE_HISTORY_COUNT = 64,
    SYS_CONSOLE_COPY_BUTTON_ID = 1,
    SYS_CONSOLE_QUIT_BUTTON_ID = 2,
    SYS_CONSOLE_CLEAR_BUTTON_ID = 3,
    SYS_CONSOLE_HIDDEN = 0,
    SYS_CONSOLE_VISIBLE = 1,
    SYS_CONSOLE_MINIMIZED = 2,
    SYS_ERROR_DIALOG_TYPE = 16
};

#define CODUOMP_WIN_ALLOW_ALT_TAB_DEFAULT "0"

void *sysApplicationInstance; /* original 0x0489bb8c */
void *sysConsoleWindow;       /* original 0x009d5950 */
void *sysConsoleOutputWindow; /* original 0x009d5954 */
void *sysConsoleErrorWindow;  /* original 0x009d5964 */
void *sysConsoleFont;         /* original 0x009d597c */
void *sysConsoleInputWindow;  /* original 0x009d5984 */
qboolean sysWindowActive;     /* original 0x0489bb90 */
qboolean sysWindowMinimized;  /* original 0x0489bb94 */

static int32_t sysConsoleVisibility; /* original 0x009d5f88 */
static qboolean sysConsoleQuitOnClose; /* original 0x009d5f8c */
static char sysConsoleErrorText[SYS_CONSOLE_ERROR_TEXT_CAPACITY];
static qboolean winAltTabDisabled; /* original 0x009d5fa8 */
/* Original producer state is the Win32 console edit control at
 * 0x009d5b88; Sys_ConsoleInput transfers one completed line to the separate
 * return buffer at 0x009d5d88. */
static char sysConsoleInputLine[SYS_CONSOLE_INPUT_CAPACITY];
static char sysConsoleReturnedLine[SYS_CONSOLE_INPUT_CAPACITY];
#if defined(_WIN32)
static console_input_field_t
    sysConsoleHistory[SYS_CONSOLE_HISTORY_COUNT]; /* original 0x009d0920 */
static int32_t
    sysConsoleHistoryHead; /* original 0x0389fde4 */
static int32_t
    sysConsoleHistoryLine; /* original 0x0389fde8 */
static int32_t
    sysConsoleCompletionMatchCount; /* original 0x009d5020 */
static char
    sysConsoleCompletionShortestMatch[CON_COMPLETION_MATCH_SIZE];
    /* original 0x009d5028 */
static uint32_t sysConsoleTextLength; /* original 0x009d5428 */
static int32_t
    sysConsoleCompletionEnumerationIndex; /* original 0x009d542c */
static console_input_field_t
    sysConsoleField; /* original 0x009d5430 */
static int32_t
    sysConsoleCompletionMatchIndex; /* original 0x009d554c */
static char
    sysConsoleCompletionString[CON_COMPLETION_MATCH_SIZE];
    /* original 0x009d5550 */
static HWND sysConsoleClearButton;   /* original 0x009d5958 */
static HWND sysConsoleCopyButton;    /* original 0x009d595c */
static HWND sysConsoleQuitButton;    /* original 0x009d5960 */
static HBRUSH sysConsoleOutputBrush; /* original 0x009d5974 */
static HBRUSH sysConsoleErrorBrush;  /* original 0x009d5978 */
static qboolean sysConsoleClassRegistered;
static int32_t sysConsoleWidth;      /* original 0x009d5f90 */
static int32_t sysConsoleHeight;     /* original 0x009d5f94 */
static WNDPROC
    sysConsoleInputWndProc;          /* original 0x009d5f98 */
static qboolean sysConsoleErrorFlash; /* original 0x009d5f9c */
static int32_t
    sysConsoleCompletionPrefixLength; /* original 0x009d5fa0 */
static cvar_t *vid_xpos;             /* original 0x0489bc38 */
static cvar_t *vid_ypos;             /* original 0x0489bc30 */
static cvar_t *win_allowAltTab;      /* original 0x0489bc34 */
static UINT registeredMouseWheelMessage; /* original 0x009d5fa4 */
#endif

#if defined(_WIN32)
/* Source: CoDUOMP.exe 0x0046e4f0..0x0046e918.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0046e4f0_0046e919.mcode, the original switch
 * tables at 0x0046e91c..0x0046e93f, and PE imports for the Win32 calls.
 * Provisional established id-engine name: callback registered by
 * Sys_CreateConsole at 0x0046f34f. */
LRESULT CALLBACK ConWndProc(HWND window, UINT message,
                            WPARAM wParam, LPARAM lParam)
{
    enum {
        SYS_CONSOLE_ERROR_TIMER_ID = 1,
        SYS_CONSOLE_ERROR_TIMER_MSEC = 1000,
        SYS_CONSOLE_SIDE_INSET = 5,
        SYS_CONSOLE_CONTROL_WIDTH_INSET = 15,
        SYS_CONSOLE_OUTPUT_TOP = 40,
        SYS_CONSOLE_OUTPUT_HEIGHT_INSET = 100,
        SYS_CONSOLE_INPUT_BOTTOM_INSET = 52,
        SYS_CONSOLE_INPUT_HEIGHT = 20,
        SYS_CONSOLE_BUTTON_BOTTOM_INSET = 28,
        SYS_CONSOLE_BUTTON_HEIGHT = 24,
        SYS_CONSOLE_BUTTON_GAP = 7
    };
    static const COLORREF outputBackground = (COLORREF)0x00647556u;
    static const COLORREF errorBackground = (COLORREF)0x00808080u;
    static const COLORREF outputText = (COLORREF)0x00ffffffu;
    static const COLORREF errorFlashText = (COLORREF)0x000000ffu;

    switch (message) {
    case WM_CREATE:
        sysConsoleOutputBrush = CreateSolidBrush(outputBackground);
        sysConsoleErrorBrush = CreateSolidBrush(errorBackground);
        (void)SetTimer(window, SYS_CONSOLE_ERROR_TIMER_ID,
                       SYS_CONSOLE_ERROR_TIMER_MSEC, NULL);
        break;

    case WM_SIZE:
    {
        const int32_t width = (int32_t)LOWORD((DWORD_PTR)lParam);
        const int32_t height = (int32_t)HIWORD((DWORD_PTR)lParam);

        (void)SetWindowPos(
            (HWND)sysConsoleOutputWindow, NULL,
            SYS_CONSOLE_SIDE_INSET, SYS_CONSOLE_OUTPUT_TOP,
            width - SYS_CONSOLE_CONTROL_WIDTH_INSET,
            height - SYS_CONSOLE_OUTPUT_HEIGHT_INSET, 0);
        (void)SetWindowPos(
            (HWND)sysConsoleInputWindow, NULL,
            SYS_CONSOLE_SIDE_INSET,
            height - SYS_CONSOLE_INPUT_BOTTOM_INSET,
            width - SYS_CONSOLE_CONTROL_WIDTH_INSET,
            SYS_CONSOLE_INPUT_HEIGHT, 0);

        /* The original x87 sequence multiplies the integer width by the
         * exact float at 0x005b9f18 (1/620), then by 72.0f, and truncates.
         * Long-double evaluation retains the same non-fused operation shape. */
        long double scaledButtonWidth = (long double)width;
        scaledButtonWidth *=
            (long double)0.001612903201021254f;
        scaledButtonWidth *= (long double)72.0f;
        const int32_t buttonWidth = (int32_t)scaledButtonWidth;
        const int32_t buttonY =
            height - SYS_CONSOLE_BUTTON_BOTTOM_INSET;

        (void)SetWindowPos(
            sysConsoleCopyButton, NULL,
            SYS_CONSOLE_SIDE_INSET, buttonY,
            buttonWidth, SYS_CONSOLE_BUTTON_HEIGHT, 0);
        (void)SetWindowPos(
            sysConsoleClearButton, NULL,
            buttonWidth + SYS_CONSOLE_BUTTON_GAP, buttonY,
            buttonWidth, SYS_CONSOLE_BUTTON_HEIGHT, 0);
        (void)SetWindowPos(
            sysConsoleQuitButton, NULL,
            width - buttonWidth - SYS_CONSOLE_CONTROL_WIDTH_INSET, buttonY,
            buttonWidth, SYS_CONSOLE_BUTTON_HEIGHT, 0);
        sysConsoleWidth = width;
        sysConsoleHeight = height;
        break;
    }

    case WM_ACTIVATE:
        if (LOWORD((DWORD_PTR)wParam) != WA_INACTIVE)
            (void)SetFocus((HWND)sysConsoleInputWindow);

        if (dedicated != NULL && dedicated->integer == 0 &&
            com_viewlog != NULL) {
            if (com_viewlog->integer == SYS_CONSOLE_VISIBLE &&
                HIWORD((DWORD_PTR)wParam) != 0) {
                (void)Cvar_Set2("viewlog", "2", qtrue);
            } else if (com_viewlog->integer == SYS_CONSOLE_MINIMIZED &&
                       HIWORD((DWORD_PTR)wParam) == 0) {
                (void)Cvar_Set2("viewlog", "1", qtrue);
            }
        }
        break;

    case WM_CLOSE:
        if (dedicated != NULL && dedicated->integer != 0) {
            char *const command = CopyStringInternal("quit");
            Sys_QueEvent(0, SE_CONSOLE, 0, 0,
                         (int32_t)strlen(command) + 1, command);
            return 0;
        }
        if (sysConsoleQuitOnClose != qfalse) {
            PostQuitMessage(0);
            return 0;
        }

        sysConsoleQuitOnClose = qfalse;
        if (sysConsoleVisibility != SYS_CONSOLE_HIDDEN) {
            sysConsoleVisibility = SYS_CONSOLE_HIDDEN;
            if (sysConsoleWindow != NULL)
                (void)ShowWindow((HWND)sysConsoleWindow, SW_HIDE);
        }
        (void)Cvar_Set2("viewlog", "0", qtrue);
        return 0;

    case WM_CTLCOLORSTATIC:
    {
        HDC const deviceContext = (HDC)wParam;
        HWND const control = (HWND)lParam;

        if (control == (HWND)sysConsoleOutputWindow) {
            (void)SetBkColor(deviceContext, outputBackground);
            (void)SetTextColor(deviceContext, outputText);
            return (LRESULT)sysConsoleOutputBrush;
        }
        if (control == (HWND)sysConsoleErrorWindow) {
            (void)SetBkColor(deviceContext, errorBackground);
            (void)SetTextColor(
                deviceContext,
                sysConsoleErrorFlash != qfalse ? errorFlashText : 0);
            return (LRESULT)sysConsoleErrorBrush;
        }
        break;
    }

    case WM_TIMER:
        if (wParam == SYS_CONSOLE_ERROR_TIMER_ID) {
            sysConsoleErrorFlash =
                sysConsoleErrorFlash == qfalse ? qtrue : qfalse;
            if (sysConsoleErrorWindow != NULL) {
                (void)InvalidateRect(
                    (HWND)sysConsoleErrorWindow, NULL, FALSE);
            }
        }
        break;

    case WM_COMMAND:
        switch (wParam) {
        case SYS_CONSOLE_COPY_BUTTON_ID:
            (void)SendMessageA(
                (HWND)sysConsoleOutputWindow, EM_SETSEL, 0, -1);
            (void)SendMessageA(
                (HWND)sysConsoleOutputWindow, WM_COPY, 0, 0);
            break;

        case SYS_CONSOLE_QUIT_BUTTON_ID:
            if (sysConsoleQuitOnClose != qfalse) {
                PostQuitMessage(0);
            } else {
                char *const command = CopyStringInternal("quit");
                Sys_QueEvent(0, SE_CONSOLE, 0, 0,
                             (int32_t)strlen(command) + 1, command);
            }
            break;

        case SYS_CONSOLE_CLEAR_BUTTON_ID:
            (void)SendMessageA(
                (HWND)sysConsoleOutputWindow, EM_SETSEL, 0, -1);
            (void)SendMessageA(
                (HWND)sysConsoleOutputWindow, EM_REPLACESEL, 0,
                (LPARAM)"");
            (void)UpdateWindow((HWND)sysConsoleOutputWindow);
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return DefWindowProcA(window, message, wParam, lParam);
}

/* Source: CoDUOMP.exe 0x0046e940..0x0046e994.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0046e940_0046e995.mcode.
 * Provisional role name: completion callback that selects the current
 * case-insensitive prefix match while cycling repeated Tab presses. */
static void Sys_ConsoleSelectMatch(const char *candidate)
{
    const int32_t prefixLength =
        (int32_t)strlen(sysConsoleCompletionString);

    if (Q_stricmpn(sysConsoleCompletionString, candidate,
                   prefixLength) != 0) {
        return;
    }

    if (sysConsoleCompletionEnumerationIndex ==
        sysConsoleCompletionMatchIndex) {
        Q_strncpyz(sysConsoleCompletionShortestMatch, candidate,
                   CON_COMPLETION_MATCH_SIZE);
    }
    ++sysConsoleCompletionEnumerationIndex;
}

/* Source: CoDUOMP.exe 0x0046e9a0..0x0046ea38.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0046e9a0_0046ea39.mcode.
 * Provisional role name: command/cvar enumeration callback that counts prefix
 * matches and shortens the common completion text. */
static void Sys_ConsoleFindMatches(const char *candidate)
{
    const int32_t prefixLength =
        (int32_t)strlen(sysConsoleCompletionString);

    if (Q_stricmpn(sysConsoleCompletionString, candidate,
                   prefixLength) != 0) {
        return;
    }

    ++sysConsoleCompletionMatchCount;
    if (sysConsoleCompletionMatchCount == 1) {
        Q_strncpyz(sysConsoleCompletionShortestMatch, candidate,
                   CON_COMPLETION_MATCH_SIZE);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int32_t index = 0;
    while (index < CON_COMPLETION_MATCH_SIZE - 1 &&
           candidate[index] != '\0') {
        if (coduo_crt_tolower(
                (int32_t)(int8_t)
                    sysConsoleCompletionShortestMatch[index]) !=
            coduo_crt_tolower(
                (int32_t)(int8_t)candidate[index])) {
            sysConsoleCompletionShortestMatch[index] = '\0';
        }
        ++index;
    }
    sysConsoleCompletionShortestMatch[index] = '\0';
}

/* Source: CoDUOMP.exe 0x0046ea40..0x0046ec07.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0046ea40_0046ec08.mcode.
 * Provisional role name: appends tokenized arguments to the native console
 * field, preserving the original quote-if-space behavior. */
static void Sys_ConsoleConcatArgs(void)
{
    const int32_t argumentCount = Cmd_Argc();

    for (int32_t argumentIndex = 1;
         argumentIndex < argumentCount; ++argumentIndex) {
        const char *const argument = Cmd_Argv(argumentIndex);
        const qboolean quoteArgument =
            strchr(argument, ' ') != NULL ? qtrue : qfalse;

        Q_strcat(sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE, " ");
        if (quoteArgument != qfalse) {
            Q_strcat(sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE, "\"");
        }
        Q_strcat(sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE, argument);
        if (quoteArgument != qfalse) {
            Q_strcat(sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE, "\"");
        }
    }
}

/* Source: CoDUOMP.exe 0x0046ec10..0x0046ec51.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046ec10_0046ec52.mcode.
 * Provisional role name: retains the source suffix following a completed
 * token, or reconstructs it from the tokenized argument array. */
static void Sys_ConsoleConcatRemaining(const char *source,
                                       const char *separator)
{
    const char *const remaining = strstr(source, separator);

    if (remaining == NULL) {
        Sys_ConsoleConcatArgs();
        return;
    }

    Q_strcat(sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE,
             remaining + strlen(separator));
}

/* Source: CoDUOMP.exe 0x0046ec60..0x0046ec91.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0046ec60_0046ec92.mcode.
 * Provisional role name: prints command-name completion matches. */
static void Sys_ConsolePrintMatches(const char *candidate)
{
    if (Q_stricmpn(sysConsoleCompletionShortestMatch, candidate,
                   sysConsoleCompletionPrefixLength) == 0) {
        Sys_Print(va("  ^9%s^0\n", candidate));
    }
}

/* Source: CoDUOMP.exe 0x0046eca0..0x0046ece9.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_0046eca0_0046ecea.mcode.
 * Provisional role name: prints cvar completion matches with their values. */
static void Sys_ConsolePrintCvarMatches(const char *candidate)
{
    if (Q_stricmpn(sysConsoleCompletionShortestMatch, candidate,
                   sysConsoleCompletionPrefixLength) == 0) {
        const cvar_t *const cvar = Cvar_FindVar(candidate);
        Sys_Print(va("  ^9%s = ^5%s^0\n", candidate,
                     cvar != NULL ? cvar->string : ""));
    }
}

/* Source: CoDUOMP.exe 0x0046ecf0..0x0046f0a1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046ecf0_0046f0a2.mcode.
 * Provisional role name: completes the native Win32 console edit field,
 * cycling individual matches on later Tab presses. */
static void Sys_ConsoleCompleteCommand(qboolean showMatches)
{
    char *command = sysConsoleField.buffer;
    if (*command == '/' || *command == '\\')
        ++command;

    if (coduo_crt_strnicmp(command, "pb_", 3) == 0) {
        char punkBusterCommand[CON_INPUT_BUFFER_SIZE];

        Q_strncpyz(punkBusterCommand, command,
                   CON_INPUT_BUFFER_SIZE);
        if (coduo_crt_strnicmp(
                punkBusterCommand, "pb_sv_", 6) == 0) {
            PbServerCompleteCommand(
                punkBusterCommand, CON_INPUT_BUFFER_SIZE - 1);
        } else {
            PbClientCompleteCommand(
                punkBusterCommand, CON_INPUT_BUFFER_SIZE - 1);
        }

        Com_sprintf(sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE,
                    "%s", punkBusterCommand);
        sysConsoleField.cursor =
            (int32_t)strlen(sysConsoleField.buffer);
        return;
    }

    if (sysConsoleCompletionPrefixLength == 0) {
        Cmd_TokenizeString(sysConsoleField.buffer);
        Q_strncpyz(sysConsoleCompletionString, Cmd_Argv(0),
                   CON_COMPLETION_MATCH_SIZE);
        if (sysConsoleCompletionString[0] == '/' ||
            sysConsoleCompletionString[0] == '\\') {
            const size_t shiftedLength =
                strlen(sysConsoleCompletionString);
            memmove(sysConsoleCompletionString,
                    &sysConsoleCompletionString[1], shiftedLength);
        }

        sysConsoleCompletionMatchCount = 0;
        sysConsoleCompletionMatchIndex = 0;
        sysConsoleCompletionShortestMatch[0] = '\0';
        if (sysConsoleCompletionString[0] == '\0')
            return;

        Cmd_CommandCompletion(Sys_ConsoleFindMatches);
        Cvar_CommandCompletion(Sys_ConsoleFindMatches);
        if (sysConsoleCompletionMatchCount == 0)
            return;

        const console_input_field_t originalField = sysConsoleField;
        Com_sprintf(sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE,
                    "%s", sysConsoleCompletionShortestMatch);

        if (sysConsoleCompletionMatchCount == 1) {
            if (Cmd_Argc() == 1) {
                Q_strcat(sysConsoleField.buffer,
                         CON_INPUT_BUFFER_SIZE, " ");
            } else {
                Sys_ConsoleConcatRemaining(
                    originalField.buffer,
                    sysConsoleCompletionString);
            }
            sysConsoleField.cursor =
                (int32_t)strlen(sysConsoleField.buffer);
        } else {
            sysConsoleField.cursor =
                (int32_t)strlen(sysConsoleField.buffer);
            sysConsoleCompletionPrefixLength =
                sysConsoleField.cursor;
            Sys_ConsoleConcatRemaining(
                originalField.buffer,
                sysConsoleCompletionString);
            showMatches = qtrue;
        }
    } else if (sysConsoleCompletionMatchCount != 1) {
        char previousMatch[CON_COMPLETION_MATCH_SIZE];
        Q_strncpyz(previousMatch,
                   sysConsoleCompletionShortestMatch,
                   CON_COMPLETION_MATCH_SIZE);

        ++sysConsoleCompletionMatchIndex;
        if (sysConsoleCompletionMatchIndex ==
            sysConsoleCompletionMatchCount) {
            sysConsoleCompletionMatchIndex = 0;
        }

        sysConsoleCompletionEnumerationIndex = 0;
        Cmd_CommandCompletion(Sys_ConsoleSelectMatch);
        Cvar_CommandCompletion(Sys_ConsoleSelectMatch);

        const console_input_field_t originalField = sysConsoleField;
        Com_sprintf(sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE,
                    "%s", sysConsoleCompletionShortestMatch);
        sysConsoleField.cursor =
            (int32_t)strlen(sysConsoleField.buffer);
        Sys_ConsoleConcatRemaining(
            originalField.buffer, previousMatch);
    }

    if (sysConsoleCompletionMatchCount == 1) {
        sysConsoleCompletionPrefixLength =
            (int32_t)strlen(sysConsoleCompletionShortestMatch);
    }

    if (showMatches != qfalse &&
        sysConsoleCompletionMatchCount > 0) {
        console_input_field_t displayedField = sysConsoleField;
        displayedField.buffer[
            sysConsoleCompletionPrefixLength] = '\0';
        Sys_Print(va("] %s\n", displayedField.buffer));
        Cmd_CommandCompletion(Sys_ConsolePrintMatches);
        Cvar_CommandCompletion(Sys_ConsolePrintCvarMatches);
    }
}

/* Source: CoDUOMP.exe 0x0046f0b0..0x0046f30d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046f0b0_0046f310.mcode and PE imports
 * identifying the Win32 edit-control operations.
 * Name: established id-engine Win32 console edit-control subclass. */
LRESULT CALLBACK InputLineWndProc(HWND window, UINT message,
                                  WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CHAR:
        (void)GetWindowTextA(
            (HWND)sysConsoleInputWindow,
            sysConsoleField.buffer, CON_INPUT_BUFFER_SIZE);
        (void)SendMessageA(
            (HWND)sysConsoleInputWindow, EM_GETSEL, 0,
            (LPARAM)&sysConsoleField.cursor);
        sysConsoleField.scroll = 0;

        if (wParam == '\r') {
            const size_t queuedLength =
                strlen(sysConsoleInputLine);
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (queuedLength > SYS_CONSOLE_INPUT_APPEND_LIMIT) {
                Sys_Print("WARNING: console input queue is full\n");
                return 0;
            }
            strncat(
                sysConsoleInputLine, sysConsoleField.buffer,
                (size_t)SYS_CONSOLE_INPUT_APPEND_LIMIT -
                    queuedLength);

            char *const lineEnd =
                &sysConsoleInputLine[
                    strlen(sysConsoleInputLine)];
            lineEnd[0] = '\n';
            lineEnd[1] = '\0';

            (void)SetWindowTextA(
                (HWND)sysConsoleInputWindow, "");
            Sys_Print(va("]%s\n", sysConsoleField.buffer));
            sysConsoleCompletionPrefixLength = 0;

            if (sysConsoleField.buffer[0] == '\0')
                return 0;

            sysConsoleHistory[
                sysConsoleHistoryHead %
                    SYS_CONSOLE_HISTORY_COUNT] =
                sysConsoleField;
            ++sysConsoleHistoryHead;
            sysConsoleHistoryLine = sysConsoleHistoryHead;
            return 0;
        }

        if (wParam == '\t') {
            Sys_ConsoleCompleteCommand(qfalse);
            (void)SetWindowTextA(
                (HWND)sysConsoleInputWindow,
                sysConsoleField.buffer);
            (void)SendMessageA(
                (HWND)sysConsoleInputWindow, EM_SETSEL,
                (WPARAM)sysConsoleField.cursor,
                (LPARAM)sysConsoleField.cursor);
            return 0;
        }

        sysConsoleCompletionPrefixLength = 0;
        break;

    case WM_KEYDOWN:
        if (wParam == VK_UP) {
            if (sysConsoleHistoryHead - sysConsoleHistoryLine <
                    SYS_CONSOLE_HISTORY_COUNT &&
                sysConsoleHistoryLine > 0) {
                --sysConsoleHistoryLine;
            }
        } else if (wParam == VK_DOWN) {
            if (sysConsoleHistoryLine < sysConsoleHistoryHead)
                ++sysConsoleHistoryLine;
        } else {
            break;
        }

        sysConsoleField =
            sysConsoleHistory[
                sysConsoleHistoryLine %
                    SYS_CONSOLE_HISTORY_COUNT];
        (void)SetWindowTextA(
            (HWND)sysConsoleInputWindow,
            sysConsoleField.buffer);
        (void)SendMessageA(
            (HWND)sysConsoleInputWindow, EM_SETSEL,
            (WPARAM)sysConsoleField.cursor,
            (LPARAM)sysConsoleField.cursor);
        sysConsoleCompletionPrefixLength = 0;
        return 0;

    case WM_KILLFOCUS:
        if ((HWND)wParam == (HWND)sysConsoleWindow ||
            (HWND)wParam == (HWND)sysConsoleErrorWindow) {
            (void)SetFocus((HWND)sysConsoleInputWindow);
            return 0;
        }
        break;

    default:
        break;
    }

    return CallWindowProcA(
        sysConsoleInputWndProc, window, message, wParam, lParam);
}
#endif

/* Source: CoDUOMP.exe 0x0046f310..0x0046f673.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046f310_0046f674.mcode, its literal
 * window/class strings, and PE imports identifying every Win32 operation.
 * Name and no-argument signature: exact same-module Mac symbol
 * Sys_CreateConsole. */
void Sys_CreateConsole(void)
{
#if defined(_WIN32)
    enum {
        SYS_CONSOLE_ICON_RESOURCE_ID = 133,
        SYS_CONSOLE_CLIENT_WIDTH = 620,
        SYS_CONSOLE_CLIENT_HEIGHT = 450,
        /* The original centers the 620-wide client using 600 here. */
        SYS_CONSOLE_CENTERING_WIDTH = 600,
        SYS_CONSOLE_FONT_POINT_SIZE = 8,
        SYS_CONSOLE_POINTS_PER_INCH = 72,
        SYS_CONSOLE_INPUT_CONTROL_ID = 101,
        SYS_CONSOLE_OUTPUT_CONTROL_ID = 100,
        SYS_CONSOLE_INPUT_X = 6,
        SYS_CONSOLE_INPUT_Y = 400,
        SYS_CONSOLE_INPUT_WIDTH = 608,
        SYS_CONSOLE_INPUT_HEIGHT = 20,
        SYS_CONSOLE_BUTTON_WIDTH = 72,
        SYS_CONSOLE_BUTTON_HEIGHT = 24,
        SYS_CONSOLE_BUTTON_Y = 425,
        SYS_CONSOLE_COPY_BUTTON_X = 5,
        SYS_CONSOLE_CLEAR_BUTTON_X = 82,
        SYS_CONSOLE_QUIT_BUTTON_X = 542,
        SYS_CONSOLE_OUTPUT_X = 6,
        SYS_CONSOLE_OUTPUT_Y = 70,
        SYS_CONSOLE_OUTPUT_WIDTH = 606,
        SYS_CONSOLE_OUTPUT_HEIGHT = 324
    };
    static const char consoleClassName[] =
        "CoD United Offensive WinConsole";
    static const char consoleWindowTitle[] =
        "CoD United Offensive Console";
    static const DWORD consoleWindowStyle =
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    static const DWORD consoleInputStyle =
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    static const DWORD consoleButtonStyle =
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
    static const DWORD consoleOutputStyle =
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY;

    const HINSTANCE instance = (HINSTANCE)sysApplicationInstance;
    WNDCLASSA consoleClass;
    memset(&consoleClass, 0, sizeof(consoleClass));
    consoleClass.lpfnWndProc = ConWndProc;
    consoleClass.hInstance = instance;
    consoleClass.hIcon = LoadIconA(
        instance,
        MAKEINTRESOURCEA(SYS_CONSOLE_ICON_RESOURCE_ID));
    consoleClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
    /* A WNDCLASS background brush may encode COLOR_* + 1 as a pseudo-handle;
     * COLOR_MENU + 1 is the original scalar value 5. */
    consoleClass.hbrBackground =
        (HBRUSH)(INT_PTR)(COLOR_MENU + 1);
    consoleClass.lpszClassName = consoleClassName;
    if (RegisterClassA(&consoleClass) == 0)
        return;
    sysConsoleClassRegistered = qtrue;

    RECT windowRect = {
        0, 0,
        SYS_CONSOLE_CLIENT_WIDTH,
        SYS_CONSOLE_CLIENT_HEIGHT
    };
    (void)AdjustWindowRect(
        &windowRect, consoleWindowStyle, FALSE);

    HDC desktopContext = GetDC(GetDesktopWindow());
    const int32_t screenWidth =
        GetDeviceCaps(desktopContext, HORZRES);
    const int32_t screenHeight =
        GetDeviceCaps(desktopContext, VERTRES);
    (void)ReleaseDC(GetDesktopWindow(), desktopContext);

    sysConsoleWidth =
        windowRect.right - windowRect.left + 1;
    sysConsoleHeight =
        windowRect.bottom - windowRect.top + 1;
    sysConsoleWindow = CreateWindowExA(
        0, consoleClassName, consoleWindowTitle,
        consoleWindowStyle,
        (screenWidth - SYS_CONSOLE_CENTERING_WIDTH) / 2,
        (screenHeight - SYS_CONSOLE_CLIENT_HEIGHT) / 2,
        sysConsoleWidth, sysConsoleHeight,
        NULL, NULL, instance, NULL);
    if (sysConsoleWindow == NULL) {
        Sys_DestroyConsole();
        return;
    }

    HDC consoleContext = GetDC((HWND)sysConsoleWindow);
    const int32_t fontHeight = -MulDiv(
        SYS_CONSOLE_FONT_POINT_SIZE,
        GetDeviceCaps(consoleContext, LOGPIXELSY),
        SYS_CONSOLE_POINTS_PER_INCH);
    sysConsoleFont = CreateFontA(
        fontHeight, 0, 0, 0, FW_LIGHT,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
        "Courier New");
    (void)ReleaseDC((HWND)sysConsoleWindow, consoleContext);

    sysConsoleInputWindow = CreateWindowExA(
        WS_EX_CLIENTEDGE, "edit", NULL,
        consoleInputStyle,
        SYS_CONSOLE_INPUT_X, SYS_CONSOLE_INPUT_Y,
        SYS_CONSOLE_INPUT_WIDTH, SYS_CONSOLE_INPUT_HEIGHT,
        (HWND)sysConsoleWindow,
        (HMENU)(INT_PTR)SYS_CONSOLE_INPUT_CONTROL_ID,
        instance, NULL);

    sysConsoleCopyButton = CreateWindowExA(
        0, "button", NULL, consoleButtonStyle,
        SYS_CONSOLE_COPY_BUTTON_X, SYS_CONSOLE_BUTTON_Y,
        SYS_CONSOLE_BUTTON_WIDTH, SYS_CONSOLE_BUTTON_HEIGHT,
        (HWND)sysConsoleWindow,
        (HMENU)(INT_PTR)SYS_CONSOLE_COPY_BUTTON_ID,
        instance, NULL);
    (void)SendMessageA(
        sysConsoleCopyButton, WM_SETTEXT, 0, (LPARAM)"copy");

    sysConsoleClearButton = CreateWindowExA(
        0, "button", NULL, consoleButtonStyle,
        SYS_CONSOLE_CLEAR_BUTTON_X, SYS_CONSOLE_BUTTON_Y,
        SYS_CONSOLE_BUTTON_WIDTH, SYS_CONSOLE_BUTTON_HEIGHT,
        (HWND)sysConsoleWindow,
        (HMENU)(INT_PTR)SYS_CONSOLE_CLEAR_BUTTON_ID,
        instance, NULL);
    (void)SendMessageA(
        sysConsoleClearButton, WM_SETTEXT, 0, (LPARAM)"clear");

    sysConsoleQuitButton = CreateWindowExA(
        0, "button", NULL, consoleButtonStyle,
        SYS_CONSOLE_QUIT_BUTTON_X, SYS_CONSOLE_BUTTON_Y,
        SYS_CONSOLE_BUTTON_WIDTH, SYS_CONSOLE_BUTTON_HEIGHT,
        (HWND)sysConsoleWindow,
        (HMENU)(INT_PTR)SYS_CONSOLE_QUIT_BUTTON_ID,
        instance, NULL);
    (void)SendMessageA(
        sysConsoleQuitButton, WM_SETTEXT, 0, (LPARAM)"quit");

    sysConsoleOutputWindow = CreateWindowExA(
        WS_EX_CLIENTEDGE, "edit", NULL,
        consoleOutputStyle,
        SYS_CONSOLE_OUTPUT_X, SYS_CONSOLE_OUTPUT_Y,
        SYS_CONSOLE_OUTPUT_WIDTH, SYS_CONSOLE_OUTPUT_HEIGHT,
        (HWND)sysConsoleWindow,
        (HMENU)(INT_PTR)SYS_CONSOLE_OUTPUT_CONTROL_ID,
        instance, NULL);
    (void)SendMessageA(
        (HWND)sysConsoleOutputWindow, WM_SETFONT,
        (WPARAM)sysConsoleFont, 0);

    /* SetWindowLongPtr is the pointer-width-safe spelling of the original
     * i386 SetWindowLongA(GWL_WNDPROC) operation. */
    sysConsoleInputWndProc = (WNDPROC)SetWindowLongPtrA(
        (HWND)sysConsoleInputWindow, GWLP_WNDPROC,
        (LONG_PTR)InputLineWndProc);
    (void)SendMessageA(
        (HWND)sysConsoleInputWindow, WM_SETFONT,
        (WPARAM)sysConsoleFont, 0);

    (void)SetFocus((HWND)sysConsoleInputWindow);
    (void)UpdateWindow((HWND)sysConsoleWindow);
    (void)SetForegroundWindow((HWND)sysConsoleWindow);
    (void)SetFocus((HWND)sysConsoleInputWindow);
    sysConsoleVisibility = SYS_CONSOLE_HIDDEN;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: modern non-Windows application shells do
     * not create the original Win32 edit-control console. */
#endif
}

/* Source: CoDUOMP.exe 0x0046f680..0x0046f6b5.
 * Name: established id-engine Win32 console boundary. The compiler inlines
 * the same body into Sys_Quit at 0x0046b4fd..0x0046b531. */
void Sys_DestroyConsole(void)
{
#if defined(_WIN32)
    if (sysConsoleWindow != NULL) {
        ShowWindow((HWND)sysConsoleWindow, SW_HIDE);
        CloseWindow((HWND)sysConsoleWindow);
        if (DestroyWindow((HWND)sysConsoleWindow) == FALSE)
            return;
        sysConsoleWindow = NULL;
        sysConsoleOutputWindow = NULL;
        sysConsoleErrorWindow = NULL;
        sysConsoleInputWindow = NULL;
        sysConsoleClearButton = NULL;
        sysConsoleCopyButton = NULL;
        sysConsoleQuitButton = NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (sysConsoleFont != NULL) {
        (void)DeleteObject((HGDIOBJ)sysConsoleFont);
        sysConsoleFont = NULL;
    }
    if (sysConsoleOutputBrush != NULL) {
        (void)DeleteObject(sysConsoleOutputBrush);
        sysConsoleOutputBrush = NULL;
    }
    if (sysConsoleErrorBrush != NULL) {
        (void)DeleteObject(sysConsoleErrorBrush);
        sysConsoleErrorBrush = NULL;
    }
    if (sysConsoleClassRegistered != qfalse) {
        (void)UnregisterClassA("CoD United Offensive WinConsole",
                               (HINSTANCE)sysApplicationInstance);
        sysConsoleClassRegistered = qfalse;
    }
#endif
}

/* Source: CoDUOMP.exe 0x0046f6c0..0x0046f72f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046f6c0_0046f730.mcode.
 * Name and two-argument signature: exact same-module Mac symbol
 * Sys_ShowConsole. */
void Sys_ShowConsole(int32_t visibility, qboolean quitOnClose)
{
    sysConsoleQuitOnClose = quitOnClose;
    if (visibility == sysConsoleVisibility)
        return;

    sysConsoleVisibility = visibility;
    if (sysConsoleWindow == NULL)
        return;

#if defined(_WIN32)
    switch (visibility) {
    case SYS_CONSOLE_HIDDEN:
        ShowWindow((HWND)sysConsoleWindow, SW_HIDE);
        return;
    case SYS_CONSOLE_VISIBLE:
        ShowWindow((HWND)sysConsoleWindow, SW_SHOWNORMAL);
        SendMessageA((HWND)sysConsoleOutputWindow, EM_LINESCROLL,
                     0, 65535);
        return;
    case SYS_CONSOLE_MINIMIZED:
        ShowWindow((HWND)sysConsoleWindow, SW_MINIMIZE);
        return;
    default:
        Sys_Error("Invalid visLevel %d sent to Sys_ShowConsole\n",
                  visibility);
    }
#else
    /* NOT_FROM_ORIGINAL_SOURCE: no separate native console window exists
     * until the application shell supplies one. */
    (void)visibility;
#endif
}

/* Source: CoDUOMP.exe 0x0046f760..0x0046f8e3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046f760_0046f8e4.mcode.
 * Name and argument: exact same-module Mac symbol Sys_Print. The Windows
 * console keeps only the last 16383 source bytes, expands line endings to
 * CRLF, and strips id color escapes before appending to its edit control. */
void Sys_Print(const char *text)
{
#if defined(_WIN32)
    char output[SYS_CONSOLE_PRINT_BUFFER_CAPACITY];
    size_t sourceLength = strlen(text);
    if (sourceLength > SYS_CONSOLE_PRINT_SOURCE_LIMIT) {
        text += sourceLength - SYS_CONSOLE_PRINT_SOURCE_LIMIT;
    }

    size_t sourceIndex = 0;
    size_t outputLength = 0;
    while (text[sourceIndex] != '\0' &&
           outputLength < SYS_CONSOLE_PRINT_BUFFER_CAPACITY - 1) {
        const char character = text[sourceIndex];

        if (character == '\n' && text[sourceIndex + 1] == '\r') {
            output[outputLength++] = '\r';
            output[outputLength++] = '\n';
            ++sourceIndex;
        } else if (character == '\r' || character == '\n') {
            output[outputLength++] = '\r';
            output[outputLength++] = '\n';
        } else if (character == '^' &&
                   text[sourceIndex + 1] != '\0' &&
                   text[sourceIndex + 1] != '^' &&
                   text[sourceIndex + 1] >= '0' &&
                   text[sourceIndex + 1] <= '9') {
            ++sourceIndex;
        } else {
            output[outputLength++] = character;
        }
        ++sourceIndex;
    }
    /* The retained source suffix contains at most 16,383 bytes, and each
     * source byte emits at most two bytes. Therefore outputLength cannot
     * exceed 32,766 and this terminator remains inside the 32,768-byte array. */
    output[outputLength] = '\0';

    sysConsoleTextLength += (uint32_t)outputLength;
    if (sysConsoleTextLength >
        (uint32_t)SYS_CONSOLE_TEXT_RETAIN_LIMIT) {
        SendMessageA((HWND)sysConsoleOutputWindow, EM_SETSEL, 0, -1);
        sysConsoleTextLength = (uint32_t)outputLength;
    } else {
        SendMessageA((HWND)sysConsoleOutputWindow, EM_SETSEL, -1, -1);
    }
    SendMessageA((HWND)sysConsoleOutputWindow, EM_LINESCROLL, 0, 65535);
    SendMessageA((HWND)sysConsoleOutputWindow, EM_SCROLLCARET, 0, 0);
    SendMessageA((HWND)sysConsoleOutputWindow, EM_REPLACESEL, 0,
                 (LPARAM)output);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native terminal output corresponding to the
     * Win32 edit-control console boundary. */
    fputs(text, stderr);
#endif
}

/* Source: CoDUOMP.exe 0x0046f730..0x0046f759.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046f730_0046f75a.mcode.
 * Name and return contract: exact same-module Mac symbol Sys_ConsoleInput.
 * The complete NUL-terminated line is copied before the producer buffer's
 * first byte is cleared. */
char *Sys_ConsoleInput(void)
{
    if (sysConsoleInputLine[0] == '\0')
        return NULL;

    char *source = sysConsoleInputLine;
    char *destination = sysConsoleReturnedLine;
    do {
        *destination++ = *source;
    } while (*source++ != '\0');
    sysConsoleInputLine[0] = '\0';
    return sysConsoleReturnedLine;
}

#if defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical "arch"
 * cvar and "winnt" comparisons in the original disable/enable functions. */
static qboolean WIN_UsesNtAltTabHotkey(void)
{
    const cvar_t *const architecture = Cvar_FindVar("arch");
    const char *const name =
        architecture != NULL && architecture->string != NULL
            ? architecture->string
            : "";
    return Q_stricmp(name, "winnt") == 0 ? qtrue : qfalse;
}
#endif

/* Source: CoDUOMP.exe 0x0046f9a0..0x0046fa10.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046f9a0_0046fa11.mcode.
 * Provisional Windows-family name: disables Alt-Tab once, using an Alt+Tab
 * hotkey reservation on NT and SPI_SETSCREENSAVERRUNNING on older Windows. */
void WIN_DisableAltTab(void)
{
    if (winAltTabDisabled != qfalse)
        return;

#if defined(_WIN32)
    if (WIN_UsesNtAltTabHotkey() != qfalse) {
        RegisterHotKey(NULL, 0, MOD_ALT, VK_TAB);
    } else {
        BOOL previousState;
        SystemParametersInfoA(SPI_SETSCREENSAVERRUNNING, TRUE,
                              &previousState, 0);
    }
#endif
    winAltTabDisabled = qtrue;
}

/* Source: CoDUOMP.exe 0x0046fa20..0x0046fa8c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046fa20_0046fa8d.mcode.
 * Provisional Windows-family name: exact inverse of WIN_DisableAltTab. */
void WIN_EnableAltTab(void)
{
    if (winAltTabDisabled == qfalse)
        return;

#if defined(_WIN32)
    if (WIN_UsesNtAltTabHotkey() != qfalse) {
        UnregisterHotKey(NULL, 0);
    } else {
        BOOL previousState;
        SystemParametersInfoA(SPI_SETSCREENSAVERRUNNING, FALSE,
                              &previousState, 0);
    }
#endif
    winAltTabDisabled = qfalse;
}

/* Source: CoDUOMP.exe 0x0046fa90..0x0046fadf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046fa90_0046fae0.mcode.
 * Name and two Boolean roles: established Win32 id-engine window boundary;
 * the WM_ACTIVATE caller proves active comes from LOWORD(wParam) !=
 * WA_INACTIVE and minimized from HIWORD(wParam). */
void AppActivate(qboolean active, qboolean minimized)
{
    sysWindowMinimized = minimized;
    Key_ClearStates();

    if (active != qfalse && minimized == qfalse) {
        sysWindowActive = qtrue;
        sysInputAppActive = qtrue;
        return;
    }

    sysWindowActive = qfalse;
    sysInputAppActive = qfalse;
    IN_DeactivateMouse();
}

/* Source: CoDUOMP.exe 0x0046fae0..0x0046fb4c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046fae0_0046fb4d.mcode and the original
 * byte tables at 0x005ca528 and 0x005ca64c.
 * Name: established id-engine Win32 keyboard boundary. keyData is the low
 * 32-bit Win32 key-message lParam; its scan code and extended-key bit choose
 * between the original two translation-table columns. */
#if defined(_WIN32)
static int32_t MapKey(uint32_t virtualKey, uint32_t keyData)
{
    enum {
        WIN32_SCAN_CODE_SHIFT = 16,
        WIN32_SCAN_CODE_MASK = 255,
        WIN32_GRAVE_SCAN_CODE = 41,
        WIN32_EXTENDED_KEY_SHIFT = 24,
        WIN32_MAP_VIRTUAL_KEY_TO_CHARACTER = 2,
        WIN32_ASCII_LIMIT = 127
    };
    typedef struct win32_foreign_key_s {
        uint8_t windowsCharacter;
        uint8_t engineCharacter;
    } win32_foreign_key_t;
#if UINTPTR_MAX == UINT32_MAX
    _Static_assert(_Alignof(win32_foreign_key_t) == 1,
                   "Win32 foreign-key pair alignment changed");
    _Static_assert(offsetof(win32_foreign_key_t, windowsCharacter) == 0x00,
                   "Win32 foreign-key input byte moved");
    _Static_assert(offsetof(win32_foreign_key_t, engineCharacter) == 0x01,
                   "Win32 foreign-key output byte moved");
    _Static_assert(sizeof(win32_foreign_key_t) == 0x02,
                   "Win32 foreign-key pair stride changed");
#endif
    static const win32_foreign_key_t foreignKeys[] = {
        {181, 128}, {191, 129}, {223, 130}, {224, 131}, {225, 132},
        {228, 133}, {229, 134}, {230, 135}, {231, 136}, {232, 137},
        {233, 138}, {236, 139}, {241, 140}, {242, 141}, {243, 142},
        {246, 143}, {248, 144}, {249, 145}, {250, 146}, {252, 147}
    };
    const qboolean extended =
        (qboolean)((keyData >> WIN32_EXTENDED_KEY_SHIFT) & 1u);

    if (((keyData >> WIN32_SCAN_CODE_SHIFT) & WIN32_SCAN_CODE_MASK) ==
        WIN32_GRAVE_SCAN_CODE) {
        return '~';
    }

    switch (virtualKey) {
    case VK_LBUTTON: return K_MOUSE1;
    case VK_RBUTTON: return K_MOUSE2;
    case VK_MBUTTON: return K_MOUSE3;
    case VK_XBUTTON1: return K_MOUSE4;
    case VK_XBUTTON2: return K_MOUSE5;
    case VK_BACK: return K_BACKSPACE;
    case VK_TAB: return K_TAB;
    case VK_CLEAR: return extended != qfalse ? 0 : K_KP_5;
    case VK_RETURN: return extended != qfalse ? K_KP_ENTER : K_ENTER;
    case VK_SHIFT: return K_SHIFT;
    case VK_CONTROL: return K_CTRL;
    case VK_MENU: return K_ALT;
    case VK_PAUSE: return K_PAUSE;
    case VK_CAPITAL: return K_CAPSLOCK;
    case VK_ESCAPE: return K_ESCAPE;
    case VK_SPACE: return K_SPACE;
    case VK_PRIOR: return extended != qfalse ? K_PGUP : K_KP_PGUP;
    case VK_NEXT: return extended != qfalse ? K_PGDN : K_KP_PGDN;
    case VK_END: return extended != qfalse ? K_END : K_KP_END;
    case VK_HOME: return extended != qfalse ? K_HOME : K_KP_HOME;
    case VK_LEFT: return extended != qfalse ? K_LEFTARROW : K_KP_LEFTARROW;
    case VK_UP: return extended != qfalse ? K_UPARROW : K_KP_UPARROW;
    case VK_RIGHT:
        return extended != qfalse ? K_RIGHTARROW : K_KP_RIGHTARROW;
    case VK_DOWN: return extended != qfalse ? K_DOWNARROW : K_KP_DOWNARROW;
    case VK_INSERT: return extended != qfalse ? K_INS : K_KP_INS;
    case VK_DELETE: return extended != qfalse ? K_DEL : K_KP_DEL;
    case VK_NUMPAD0: return K_KP_INS;
    case VK_NUMPAD1: return K_KP_END;
    case VK_NUMPAD2: return K_KP_DOWNARROW;
    case VK_NUMPAD3: return K_KP_PGDN;
    case VK_NUMPAD4: return K_KP_LEFTARROW;
    case VK_NUMPAD5: return K_KP_5;
    case VK_NUMPAD6: return K_KP_RIGHTARROW;
    case VK_NUMPAD7: return K_KP_HOME;
    case VK_NUMPAD8: return K_KP_UPARROW;
    case VK_NUMPAD9: return K_KP_PGUP;
    case VK_MULTIPLY: return K_KP_STAR;
    case VK_ADD: return K_KP_PLUS;
    case VK_SUBTRACT: return K_KP_MINUS;
    case VK_DECIMAL: return K_KP_DEL;
    case VK_DIVIDE: return K_KP_SLASH;
    case VK_NUMLOCK: return K_KP_NUMLOCK;
    default:
        break;
    }

    if (virtualKey >= '0' && virtualKey <= '9')
        return (int32_t)virtualKey;
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return (int32_t)(extended != qfalse
            ? virtualKey
            : virtualKey + ('a' - 'A'));
    }
    if (virtualKey >= VK_F1 && virtualKey <= VK_F12)
        return K_F1 + (int32_t)(virtualKey - VK_F1);

    int32_t key =
        (int32_t)(MapVirtualKeyA(virtualKey,
                                 WIN32_MAP_VIRTUAL_KEY_TO_CHARACTER) &
                  WIN32_SCAN_CODE_MASK);
    if (key <= WIN32_ASCII_LIMIT)
        return key;

    for (size_t index = 0;
         index < sizeof(foreignKeys) / sizeof(foreignKeys[0]);
         ++index) {
        if (key == foreignKeys[index].windowsCharacter)
            return foreignKeys[index].engineCharacter;
    }
    return key;
}

/* Source: CoDUOMP.exe 0x0046fb50..0x0046ffb5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046fb50_0046ffb6.mcode, its original
 * jump tables at 0x0046ffb8..0x00470102, and PE imports identifying every
 * Win32 call target.
 * Name: established id-engine Win32 window callback. The source keeps
 * DefWindowProcA as the common tail because the original forwards every
 * handled message except Alt-Enter, blocked screensaver commands, and the two
 * denied suspend-query messages. */
LRESULT CALLBACK MainWndProc(HWND window, UINT message,
                             WPARAM wParam, LPARAM lParam)
{
    enum {
        WIN_POWER_QUERY_SUSPEND = 0,
        WIN_POWER_QUERY_STANDBY = 1
    };
    static const LRESULT broadcastQueryDeny = (LRESULT)0x424d5144u;

    (void)SetThreadExecutionState(ES_DISPLAY_REQUIRED);

    if (message == registeredMouseWheelMessage) {
        const int32_t key =
            (int32_t)wParam > 0 ? K_MWHEELUP : K_MWHEELDOWN;
        Sys_QueEvent(sysMsgTime, SE_KEY, key, qtrue, 0, NULL);
        Sys_QueEvent(sysMsgTime, SE_KEY, key, qfalse, 0, NULL);
        return DefWindowProcA(window, message, wParam, lParam);
    }

    switch (message) {
    case WM_CREATE:
        win32MainWindow = window;
        MSS_SetWindowHandle(window);
        vid_xpos = Cvar_Get("vid_xpos", "3", CVAR_ARCHIVE);
        vid_ypos = Cvar_Get("vid_ypos", "22", CVAR_ARCHIVE);
        r_fullscreen = Cvar_Get(
            "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH);
        win_allowAltTab = Cvar_Get(
            "win_allowalttab", CODUOMP_WIN_ALLOW_ALT_TAB_DEFAULT,
            CVAR_ARCHIVE | CVAR_LATCH);
        registeredMouseWheelMessage =
            RegisterWindowMessageA("MSWHEEL_ROLLMSG");

        if (r_fullscreen->integer != 0 &&
            win_allowAltTab->integer == 0) {
            WIN_DisableAltTab();
        } else {
            WIN_EnableAltTab();
        }
        break;

    case WM_DESTROY:
        win32MainWindow = NULL;
        if (r_fullscreen->integer != 0)
            WIN_EnableAltTab();
        break;

    case WM_MOVE:
        if (r_fullscreen->integer == 0) {
            RECT border = {0, 0, 1, 1};
            const LONG style = GetWindowLongA(window, GWL_STYLE);
            (void)AdjustWindowRect(&border, (DWORD)style, FALSE);

            const int32_t x =
                (int32_t)(int16_t)LOWORD((DWORD_PTR)lParam) + border.left;
            const int32_t y =
                (int32_t)(int16_t)HIWORD((DWORD_PTR)lParam) + border.top;
            Cvar_SetValue("vid_xpos", (float)x);
            Cvar_SetValue("vid_ypos", (float)y);
            vid_xpos->modified = qfalse;
            vid_ypos->modified = qfalse;

            if (sysWindowActive != qfalse)
                sysInputAppActive = qtrue;
        }
        break;

    case WM_ACTIVATE:
        AppActivate(
            LOWORD((DWORD_PTR)wParam) != WA_INACTIVE ? qtrue : qfalse,
            (qboolean)HIWORD((DWORD_PTR)wParam));
        break;

    case WM_CLOSE:
        Cbuf_AddText("quit");
        break;

    case WM_KEYDOWN:
    {
        const int32_t key = MapKey((uint32_t)wParam, (uint32_t)lParam);
        if (key != 0)
            Sys_QueEvent(sysMsgTime, SE_KEY, key, qtrue, 0, NULL);
        break;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        const int32_t key = MapKey((uint32_t)wParam, (uint32_t)lParam);
        if (key != 0)
            Sys_QueEvent(sysMsgTime, SE_KEY, key, qfalse, 0, NULL);
        break;
    }

    case WM_CHAR:
        Sys_QueEvent(sysMsgTime, SE_CHAR, (int32_t)wParam, 0, 0, NULL);
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN) {
            if (cls.state != CA_LOADING && r_fullscreen != NULL) {
                Cvar_SetValue(
                    "r_fullscreen",
                    r_fullscreen->integer == 0 ? 1.0f : 0.0f);
                Cbuf_AddText("vid_restart");
            }
            return 0;
        }
        {
            const int32_t key = MapKey((uint32_t)wParam, (uint32_t)lParam);
            if (key != 0)
                Sys_QueEvent(sysMsgTime, SE_KEY, key, qtrue, 0, NULL);
        }
        break;

    case WM_SYSCOMMAND:
        if (wParam == SC_SCREENSAVE)
            return 0;
        break;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    {
        int32_t buttonMask = 0;
        if ((wParam & MK_LBUTTON) != 0)
            buttonMask |= 1;
        if ((wParam & MK_RBUTTON) != 0)
            buttonMask |= 2;
        if ((wParam & MK_MBUTTON) != 0)
            buttonMask |= 4;
        IN_MouseEvent(buttonMask);
        break;
    }

    case WM_MOUSEWHEEL:
    {
        const int32_t key =
            (int16_t)HIWORD((DWORD_PTR)wParam) > 0
                ? K_MWHEELUP
                : K_MWHEELDOWN;
        Sys_QueEvent(sysMsgTime, SE_KEY, key, qtrue, 0, NULL);
        Sys_QueEvent(sysMsgTime, SE_KEY, key, qfalse, 0, NULL);
        break;
    }

    case WM_POWERBROADCAST:
        if (wParam == WIN_POWER_QUERY_SUSPEND ||
            wParam == WIN_POWER_QUERY_STANDBY) {
            return broadcastQueryDeny;
        }
        break;

    default:
        break;
    }

    return DefWindowProcA(window, message, wParam, lParam);
}
#endif

/* Source: CoDUOMP.exe 0x0046f8f0..0x0046f99f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046f8f0_0046f9a0.mcode.
 * Provisional Windows-source name: installs fatal text in the console,
 * replaces its input control with a static error control, and also presents
 * the localized modal error dialog. */
void Sys_SetErrorText(const char *text)
{
    strncpy(sysConsoleErrorText, text,
            SYS_CONSOLE_ERROR_TEXT_CAPACITY - 1);
    sysConsoleErrorText[SYS_CONSOLE_ERROR_TEXT_CAPACITY - 1] = '\0';

#if defined(_WIN32)
    if (sysConsoleErrorWindow != NULL)
        return;

    sysConsoleErrorWindow = CreateWindowExA(
        0, "static", NULL,
        WS_CHILD | WS_VISIBLE | SS_SUNKEN,
        6, 5, 606, 60,
        (HWND)sysConsoleWindow, (HMENU)(uintptr_t)10,
        (HINSTANCE)sysApplicationInstance, NULL);
    SendMessageA((HWND)sysConsoleErrorWindow, WM_SETFONT,
                 (WPARAM)sysConsoleFont, 0);
    SetWindowTextA((HWND)sysConsoleErrorWindow, sysConsoleErrorText);
    DestroyWindow((HWND)sysConsoleInputWindow);
    sysConsoleInputWindow = NULL;
    MessageBoxA(NULL, text, Sys_LocalizeString("WIN_ERROR"),
                SYS_ERROR_DIALOG_TYPE);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native application-shell error presentation
     * is intentionally separate from the terminal Sys_Print boundary. */
    (void)text;
#endif
}
