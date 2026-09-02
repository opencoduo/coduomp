#include <stdint.h>
#include <string.h>

#include "../abi/ui_module_abi.h"

enum {
    UI_GAMMA_WINDOW_CAPACITY = 4,
    UI_WINDOW_TITLE_BYTES = 33,
    UI_WINDOW_TEXT_CAPACITY = 1024
};

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HWND uiNativeWindow_t;
typedef LPARAM uiNativeWindowParam_t;
#define UI_WINDOW_CALLBACK BOOL CALLBACK
#else
typedef void *uiNativeWindow_t;
typedef intptr_t uiNativeWindowParam_t;
#define UI_WINDOW_CALLBACK qboolean
#endif

#if defined(__GNUC__)
#define UI_STORAGE_USED __attribute__((used))
#else
#define UI_STORAGE_USED
#endif

// Source: uo_ui_mp_x86.dll data 0x40041800..0x40041813.
// These slots retain the windows hidden by UI_HideGameWindow.
static uint32_t ui_hiddenWindowCount UI_STORAGE_USED; // 0x40041800
// 0x40041804
static uiNativeWindow_t ui_hiddenWindows[UI_GAMMA_WINDOW_CAPACITY]
    UI_STORAGE_USED;

/* NOT_FROM_ORIGINAL_SOURCE: restore the PE loader's empty hidden-window list
 * at each portable dllEntry lifecycle boundary. */
void ui_compat_reset_window_state(void)
{
    ui_hiddenWindowCount = 0;
    memset(ui_hiddenWindows, 0, sizeof(ui_hiddenWindows));
}

// Source: uo_ui_mp_x86.dll 0x40001000..0x400010ab
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40001000_400010ab.mcode
UI_WINDOW_CALLBACK UI_HideGameWindow(uiNativeWindow_t window,
                                     uiNativeWindowParam_t unused)
{
#if defined(_WIN32)
    static const char gameWindowTitle[UI_WINDOW_TITLE_BYTES] =
        "CoD:United Offensive Multiplayer";
    char title[UI_WINDOW_TEXT_CAPACITY];
    LONG style;
    LONG extendedStyle;
    int32_t titleIndex;

    (void)unused;
    if (GetWindowTextA(window, title, sizeof(title)) != 0) {
        /* 0x40001031..0x40001043 is a 33-byte forward REPE CMPSB. Keep its
         * stop-at-first-mismatch reads instead of a host memcmp that may read
         * later, untouched stack bytes after a short title. */
        for (titleIndex = 0; titleIndex < UI_WINDOW_TITLE_BYTES;
             ++titleIndex) {
            if ((unsigned char)title[titleIndex] !=
                (unsigned char)gameWindowTitle[titleIndex]) {
                return TRUE;
            }
        }
    }

    style = GetWindowLongA(window, GWL_STYLE);
    extendedStyle = GetWindowLongA(window, GWL_EXSTYLE);
    if ((style & WS_VISIBLE) != 0) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        const uint32_t slot = ui_hiddenWindowCount;
        ui_hiddenWindows[slot] = window;
        ui_hiddenWindowCount = slot + 1u;
        SetWindowLongA(window, GWL_STYLE, style & ~WS_VISIBLE);
        SetWindowLongA(window, GWL_EXSTYLE,
                       extendedStyle & ~WS_EX_TOPMOST);
    }
    return TRUE;
#else
    // NOT_FROM_ORIGINAL_SOURCE: portable replacement for the Win32 callback.
    (void)window;
    (void)unused;
    return qtrue;
#endif
}

// Source: uo_ui_mp_x86.dll 0x400010b0..0x40001146
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400010b0_40001146.mcode
void UI_RestoreDesktopDisplay(void)
{
#if defined(_WIN32)
    WORD gammaRamp[3][256];
    HWND desktopWindow;
    HDC desktopDevice;
    int32_t index;

    ChangeDisplaySettingsA(NULL, 0);
    EnumThreadWindows(GetCurrentThreadId(), UI_HideGameWindow, 0);
    desktopWindow = GetDesktopWindow();
    desktopDevice = GetDC(desktopWindow);
    for (index = 0; index < 256; ++index) {
        WORD value = (WORD)(index * 257);
        gammaRamp[0][index] = value;
        gammaRamp[1][index] = value;
        gammaRamp[2][index] = value;
    }
    SetDeviceGammaRamp(desktopDevice, gammaRamp);
    ReleaseDC(desktopWindow, desktopDevice);
#else
    // NOT_FROM_ORIGINAL_SOURCE: non-Windows restoration is engine-owned.
#endif
}
