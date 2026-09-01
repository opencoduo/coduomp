#include "backend.h"

#include "platform_gamma.h"
#include "renderer_cvars.h"
#include "wgl_debug.h"

#if !defined(_WIN32)
#include "../platform/sdl_platform.h"
#endif

#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

enum {
    GLIMP_GAMMA_CHANNEL_COUNT = 3,
    GLIMP_GAMMA_ENTRY_COUNT = 256,
    GLIMP_W2K_CLAMP_ENTRY_COUNT = 128,
    GLIMP_W2K_CLAMP_BASE = 128,
    GLIMP_W2K_MAXIMUM_RAMP_VALUE = 254,
    RESTORE_GAMMA_WINDOW_CAPACITY = 5
};

renderer_wgl_device_context_t rendererWin32DeviceContext;

/* Original 0x0389f6e0..0x0389fce0. GLimp_InitGamma captures the desktop
 * device's ramp here so the platform shutdown path can restore it. */
uint16_t rendererOriginalGammaRamp
    [GLIMP_GAMMA_CHANNEL_COUNT][GLIMP_GAMMA_ENTRY_COUNT];

#if !defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE_STORAGE: SDL's gamma API changes the complete
 * display on macOS. Retain the current game ramp so native focus transitions
 * can expose system UI under the captured desktop ramp and restore the game
 * mapping without recalculating renderer state. */
static uint16_t coduompCurrentGammaRamp
    [GLIMP_GAMMA_CHANNEL_COUNT][GLIMP_GAMMA_ENTRY_COUNT];
static qboolean coduompCurrentGammaRampValid;
static qboolean coduompGammaWindowActive = qtrue;
#endif

/* NOT_FROM_ORIGINAL_SOURCE: stable renderer-facing query shared with the
 * compatibility provider selected by non-stock builds. */
qboolean coduomp_gamma_output_available(void)
{
    return glConfig.deviceSupportsGamma;
}

#if defined(_WIN32)
/* The callback appends only to these original globals at 0x005d06c8 and
 * 0x005d06cc; no instruction in the executable reads either object. The next
 * original global starts at 0x005d06e0, proving the five-HWND capacity. */
int32_t restoreGammaWindowCount;
HWND restoreGammaWindows[RESTORE_GAMMA_WINDOW_CAPACITY];

/* Source: CoDUOMP.exe 0x00401000..0x004010aa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401000_004010ab.mcode.
 * Provisional role name: EnumThreadWindows callback used only by
 * RestoreSystemGammas. An empty window title follows the original matching
 * path; a nonempty title must exactly match all 33 bytes including the NUL. */
BOOL CALLBACK RestoreGammaWindowState(HWND window, LPARAM context)
{
    static const char gameWindowTitle[] =
        "CoD:United Offensive Multiplayer";
    char title[MAX_STRING_CHARS];
    (void)context;

    if (GetWindowTextA(window, title, sizeof(title)) != 0 &&
        memcmp(title, gameWindowTitle, sizeof(gameWindowTitle)) != 0) {
        return TRUE;
    }

    LONG style = GetWindowLongA(window, GWL_STYLE);
    LONG extendedStyle = GetWindowLongA(window, GWL_EXSTYLE);
    if ((style & WS_VISIBLE) != 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((uint32_t)restoreGammaWindowCount <
            RESTORE_GAMMA_WINDOW_CAPACITY) {
            restoreGammaWindows[restoreGammaWindowCount] = window;
            ++restoreGammaWindowCount;
        }
        style &= ~WS_VISIBLE;
        SetWindowLongA(window, GWL_STYLE, style);
        extendedStyle &= ~WS_EX_TOPMOST;
        SetWindowLongA(window, GWL_EXSTYLE, extendedStyle);
    }
    return TRUE;
}

#endif

static uint16_t GLimp_ExpandGammaByte(uint8_t value)
{
    /* NOT_FROM_ORIGINAL_SOURCE: source-level expression for the repeated
     * byte-to-WORD expansion in GLimp_SetGamma's unrolled PE loop. The native
     * SDL display boundary consumes the same 16-bit ramp representation. */
    return (uint16_t)(((uint16_t)value << 8) | value);
}

/* Source: CoDUOMP.exe 0x004010b0..0x00401145.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004010b0_00401146.mcode.
 * Name: exact same-module Mac symbol RestoreSystemGammas. The Windows body
 * resets the display mode, normalizes matching thread-window styles through
 * its callback, then installs an exact 0x0000,0x0101,...,0xffff linear ramp
 * on the desktop device context. */
void RestoreSystemGammas(void)
{
#if defined(_WIN32)
    uint16_t linearRamp
        [GLIMP_GAMMA_CHANNEL_COUNT][GLIMP_GAMMA_ENTRY_COUNT];

    ChangeDisplaySettingsA(NULL, 0);
    EnumThreadWindows(GetCurrentThreadId(),
                      RestoreGammaWindowState, 0);

    HWND desktopWindow = GetDesktopWindow();
    HDC desktopDeviceContext = GetDC(desktopWindow);
    for (int32_t entry = 0; entry < GLIMP_GAMMA_ENTRY_COUNT; ++entry) {
        const uint16_t value = (uint16_t)(entry * 257);
        for (int32_t channel = 0;
             channel < GLIMP_GAMMA_CHANNEL_COUNT; ++channel) {
            linearRamp[channel][entry] = value;
        }
    }
    SetDeviceGammaRamp(desktopDeviceContext, linearRamp);
    ReleaseDC(desktopWindow, desktopDeviceContext);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: the normal native restore occurs in
     * GLimp_Shutdown while SDL still owns the window/display association. */
#endif
}

/* Source: CoDUOMP.exe 0x00523c80..0x00523d65.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523c80_00523d66.mcode.
 * Name and no-argument source boundary: exact same-module Mac symbol
 * GLimp_InitGamma. The PE imports prove the GetDesktopWindow/GetDC,
 * GetDeviceGammaRamp, and ReleaseDC sequence. */
void GLimp_InitGamma(void)
{
    glConfig.deviceSupportsGamma = qfalse;

#if !defined(_WIN32)
    coduompCurrentGammaRampValid = qfalse;
#endif

    if (r_ignorehwgamma->integer != 0)
        return;

#if defined(_WIN32)
    HWND desktopWindow = GetDesktopWindow();
    HDC desktopDeviceContext = GetDC(desktopWindow);
    glConfig.deviceSupportsGamma = GetDeviceGammaRamp(
        desktopDeviceContext, rendererOriginalGammaRamp);
    ReleaseDC(desktopWindow, desktopDeviceContext);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: SDL supplies the native display/window
     * boundary corresponding to the original desktop-device gamma calls. */
    glConfig.deviceSupportsGamma = coduomp_sdl_get_window_gamma_ramp(
        rendererOriginalGammaRamp[0],
        rendererOriginalGammaRamp[1],
        rendererOriginalGammaRamp[2]);
#endif

    if (glConfig.deviceSupportsGamma == qfalse)
        return;

    if ((uint8_t)(rendererOriginalGammaRamp[0][255] >> 8) <=
            (uint8_t)(rendererOriginalGammaRamp[0][0] >> 8) ||
        (uint8_t)(rendererOriginalGammaRamp[1][255] >> 8) <=
            (uint8_t)(rendererOriginalGammaRamp[1][0] >> 8) ||
        (uint8_t)(rendererOriginalGammaRamp[2][255] >> 8) <=
            (uint8_t)(rendererOriginalGammaRamp[2][0] >> 8)) {
        glConfig.deviceSupportsGamma = qfalse;
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: device has broken gamma support, generated "
                  "gamma.dat\n");
    }

    /* Some drivers report a saturated restoration ramp. The PE tests the
     * high byte of channel 0, entry 181 and replaces entries 0..254 with the
     * exact linear values below, deliberately retaining entry 255. */
    if ((uint8_t)(rendererOriginalGammaRamp[0][181] >> 8) == 255) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: suspicious gamma tables, using linear ramp for "
                  "restoration\n");
        for (int32_t entry = 0; entry < 255; ++entry) {
            const uint16_t linearValue = (uint16_t)(entry << 8);
            for (int32_t channel = 0;
                 channel < GLIMP_GAMMA_CHANNEL_COUNT; ++channel) {
                rendererOriginalGammaRamp[channel][entry] = linearValue;
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x00523d70..0x0052401c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523d70_0052401d.mcode.
 * Name and three 256-byte channel arguments: exact same-module Mac symbol
 * GLimp_SetGamma plus the R_SetColorMappings call at 0x0050a837. */
void GLimp_SetGamma(const uint8_t red[GLIMP_GAMMA_ENTRY_COUNT],
                    const uint8_t green[GLIMP_GAMMA_ENTRY_COUNT],
                    const uint8_t blue[GLIMP_GAMMA_ENTRY_COUNT])
{
    if (glConfig.deviceSupportsGamma == qfalse ||
        r_ignorehwgamma->integer != 0) {
        return;
    }
#if defined(_WIN32)
    if (rendererWin32DeviceContext == NULL)
        return;
#endif

    uint16_t gammaRamp
        [GLIMP_GAMMA_CHANNEL_COUNT][GLIMP_GAMMA_ENTRY_COUNT];
    for (int32_t entry = 0; entry < GLIMP_GAMMA_ENTRY_COUNT; ++entry) {
        gammaRamp[0][entry] = GLimp_ExpandGammaByte(red[entry]);
        gammaRamp[1][entry] = GLimp_ExpandGammaByte(green[entry]);
        gammaRamp[2][entry] = GLimp_ExpandGammaByte(blue[entry]);
    }

#if defined(_WIN32)
    OSVERSIONINFOA versionInformation;
    versionInformation.dwOSVersionInfoSize = sizeof(versionInformation);
    GetVersionExA(&versionInformation);

    if (versionInformation.dwMajorVersion == 5 &&
        versionInformation.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        Com_DPrintf("performing W2K gamma clamp.\n");
        for (int32_t channel = 0;
             channel < GLIMP_GAMMA_CHANNEL_COUNT; ++channel) {
            for (int32_t entry = 0;
                 entry < GLIMP_W2K_CLAMP_ENTRY_COUNT; ++entry) {
                const uint16_t maximumValue = (uint16_t)(
                    (GLIMP_W2K_CLAMP_BASE + entry) << 8);
                if (gammaRamp[channel][entry] > maximumValue)
                    gammaRamp[channel][entry] = maximumValue;
            }

            const uint16_t maximumRampValue =
                (uint16_t)(GLIMP_W2K_MAXIMUM_RAMP_VALUE << 8);
            if (gammaRamp[channel][127] > maximumRampValue)
                gammaRamp[channel][127] = maximumRampValue;
        }
    } else {
        Com_DPrintf("skipping W2K gamma clamp.\n");
    }
#endif

    for (int32_t channel = 0;
         channel < GLIMP_GAMMA_CHANNEL_COUNT; ++channel) {
        for (int32_t entry = 1;
             entry < GLIMP_GAMMA_ENTRY_COUNT; ++entry) {
            if (gammaRamp[channel][entry] <
                gammaRamp[channel][entry - 1]) {
                gammaRamp[channel][entry] =
                    gammaRamp[channel][entry - 1];
            }
        }
    }

#if defined(_WIN32)
    if (SetDeviceGammaRamp((HDC)rendererWin32DeviceContext,
                           gammaRamp) == FALSE) {
        Com_Printf("SetDeviceGammaRamp failed.\n");
    }
#else
    /* NOT_FROM_ORIGINAL_SOURCE: apply the original renderer's computed ramp
     * through SDL to the display that owns the native game window. */
    memcpy(coduompCurrentGammaRamp, gammaRamp,
           sizeof(coduompCurrentGammaRamp));
    coduompCurrentGammaRampValid = qtrue;
    if (coduompGammaWindowActive != qfalse &&
        coduomp_sdl_set_window_gamma_ramp(
            gammaRamp[0], gammaRamp[1], gammaRamp[2]) == qfalse) {
        Com_Printf("SDL_SetWindowGammaRamp failed: %s\n",
                   coduomp_sdl_error_compat());
    }
#endif
}

#if !defined(_WIN32)
/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): SDL implements window gamma
 * with a display-wide ramp on macOS. Restore the captured desktop mapping
 * while another application or a system overlay has focus, then reapply the
 * last renderer mapping when the game regains focus. This keeps the original
 * live r_gamma behavior without brightening unrelated macOS UI. */
void coduomp_gamma_window_focus_changed(qboolean active)
{
    coduompGammaWindowActive = active;

    if (glConfig.deviceSupportsGamma == qfalse)
        return;

    if (active != qfalse &&
        coduompCurrentGammaRampValid != qfalse &&
        r_ignorehwgamma->integer == 0) {
        (void)coduomp_sdl_set_window_gamma_ramp(
            coduompCurrentGammaRamp[0],
            coduompCurrentGammaRamp[1],
            coduompCurrentGammaRamp[2]);
    } else {
        (void)coduomp_sdl_set_window_gamma_ramp(
            rendererOriginalGammaRamp[0],
            rendererOriginalGammaRamp[1],
            rendererOriginalGammaRamp[2]);
    }
}
#endif

/* Source: CoDUOMP.exe 0x00524020..0x00524054.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00524020_00524055.mcode.
 * Provisional role name: retained out-of-line body of the gamma restoration
 * helper inlined by the Windows compiler into GLimp_Shutdown at 0x004f6f51.
 * It restores the exact desktop ramp captured by GLimp_InitGamma. */
void GLimp_RestoreGamma(void)
{
    if (glConfig.deviceSupportsGamma == qfalse)
        return;

#if defined(_WIN32)
    HWND desktopWindow = GetDesktopWindow();
    HDC desktopDeviceContext = GetDC(desktopWindow);
    SetDeviceGammaRamp(desktopDeviceContext, rendererOriginalGammaRamp);
    ReleaseDC(GetDesktopWindow(), desktopDeviceContext);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: restore the ramp captured before the native
     * renderer installed its game-specific gamma mapping. */
    (void)coduomp_sdl_set_window_gamma_ramp(
        rendererOriginalGammaRamp[0],
        rendererOriginalGammaRamp[1],
        rendererOriginalGammaRamp[2]);
#endif
}
