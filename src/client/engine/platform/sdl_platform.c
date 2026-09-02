#include "sdl_platform.h"

#if !defined(_WIN32)

#include "../client/console.h"
#include "../system_console.h"
#include "../system_event.h"
#include "../system_input.h"
#include "../renderer/platform_gamma.h"
#include "qcommon/q_memory.h"

#include <SDL.h>

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#endif

#include <stdint.h>
#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: all private globals in this
 * translation unit belong to the native SDL application shell. */
static SDL_Window *coduoSdlWindow;
static SDL_GLContext coduoSdlGlContext;
static qboolean coduoSdlInitialized;

enum {
    CODUO_SDL_CHAR_PASTE = 22
};

#if defined(__APPLE__)
static uint32_t coduoSdlReportedMouseButtons;
static uint32_t coduoSdlStaleMouseButtons;

enum {
    CODUO_SDL_SUPPORTED_MOUSE_BUTTON_MASK = SDL_BUTTON_LMASK | SDL_BUTTON_RMASK | SDL_BUTTON_MMASK | SDL_BUTTON_X1MASK | SDL_BUTTON_X2MASK
};

/* NOT_FROM_ORIGINAL_SOURCE: SDL's Cocoa global-state query reads the physical
 * NSEvent button mask rather than SDL's per-window event latch. Keep the
 * compatibility boundary limited to the five buttons exposed by the engine. */
static uint32_t coduomp_sdl_current_mouse_buttons_compat(void)
{
    int cursorX;
    int cursorY;

    return SDL_GetGlobalMouseState(&cursorX, &cursorY) & CODUO_SDL_SUPPORTED_MOUSE_BUTTON_MASK;
}

/* NOT_FROM_ORIGINAL_SOURCE: SDL_GetMouseState exposes SDL's retained
 * per-input-source event latch. Comparing it with the physical state above
 * identifies exactly which releases were lost during window replacement. */
static uint32_t coduomp_sdl_latched_mouse_buttons_compat(void)
{
    int cursorX;
    int cursorY;

    return SDL_GetMouseState(&cursorX, &cursorY) & CODUO_SDL_SUPPORTED_MOUSE_BUTTON_MASK;
}

/* NOT_FROM_ORIGINAL_SOURCE: establish an engine-facing baseline without
 * turning a focus/activation click into gameplay input, and remember only
 * SDL latches proven stale against the physical Cocoa button state. */
static void coduomp_sdl_rebase_mouse_buttons_compat(void)
{
    coduoSdlReportedMouseButtons = coduomp_sdl_current_mouse_buttons_compat();
    coduoSdlStaleMouseButtons = coduomp_sdl_latched_mouse_buttons_compat() & ~coduoSdlReportedMouseButtons;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: SDL provides the native application, window,
 * OpenGL-context, and input boundary that Win32 supplied to the recovered
 * executable. Game events remain expressed through the original engine queue. */
qboolean CoduoSDL_Init(void)
{
    if (coduoSdlInitialized != qfalse)
        return qtrue;

#if defined(__APPLE__)
    SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "0");
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): SDL's Cocoa backend
     * uses a native fullscreen Space by default. Keep it on the ordinary
     * desktop so switching applications can reveal their windows without
     * minimizing the game window to the Dock. */
    SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
#endif
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        return qfalse;
    }

    coduoSdlInitialized = qtrue;
    SDL_StartTextInput();
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: releases the native resources owned by the SDL
 * application shell. The recovered engine still performs its own subsystem
 * shutdown before reaching this boundary. */
void CoduoSDL_Shutdown(void)
{
    if (coduoSdlInitialized == qfalse)
        return;

    CoduoSDL_DestroyOpenGLWindow();
    SDL_StopTextInput();
    SDL_Quit();
    coduoSdlInitialized = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: SDL owns the native clipboard integration on
 * macOS and Unix. Copy its temporary allocation into the malloc-owned domain
 * expected by Sys_GetClipboardData and the recovered field/UI callers. */
char *coduomp_sdl_get_clipboard_text_compat(void)
{
    char *const nativeText = SDL_GetClipboardText();
    if (nativeText == NULL)
        return NULL;

    char *const clipboardText = CopyStringInternal(nativeText);
    SDL_free(nativeText);
    return clipboardText;
}

/* NOT_FROM_ORIGINAL_SOURCE: native equivalent of the Win32 window, pixel
 * format, and WGL-context setup in GLW_CreateWindow/GLW_InitDriver. A legacy
 * compatibility context is required because the original renderer uses the
 * OpenGL fixed-function pipeline. This stock provider uses only the original
 * boolean window-mode domain, leaving the borderless extension unreachable. */
qboolean CoduoSDL_CreateOpenGLWindow(int32_t width, int32_t height, int32_t colorBits, int32_t depthBits, int32_t stencilBits,
                                     int32_t windowMode)
{
    enum {
        CODUO_WINDOW_MODE_FULLSCREEN = 1,
        CODUO_WINDOW_MODE_BORDERLESS = 2
    };
    const char *windowTitle = "Call of Duty: United Offensive Multiplayer";
    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN;

    if (CoduoSDL_Init() == qfalse)
        return qfalse;

    CoduoSDL_DestroyOpenGLWindow();

    if (colorBits <= 0)
        colorBits = 24;
    if (depthBits <= 0)
        depthBits = colorBits > 16 ? 24 : 16;
    if (stencilBits < 0)
        stencilBits = 0;
    if (depthBits < 24)
        stencilBits = 0;

    SDL_GL_ResetAttributes();
    (void)SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    (void)SDL_GL_SetAttribute(SDL_GL_RED_SIZE, colorBits >= 24 ? 8 : 5);
    (void)SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, colorBits >= 24 ? 8 : 6);
    (void)SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, colorBits >= 24 ? 8 : 5);
    /* The retail PFD explicitly leaves cAlphaBits at zero. Cocoa may still
     * realize an 8-bit alpha channel, but it is not a renderer requirement. */
    (void)SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    (void)SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depthBits);
    (void)SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencilBits);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    if (windowMode == CODUO_WINDOW_MODE_FULLSCREEN) {
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): never capture or
         * switch the OS display mode for fullscreen. Preserve every connected
         * monitor and let the renderer present its hardware-sized image into
         * this non-exclusive fullscreen surface on every SDL platform. */
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    } else if (windowMode == CODUO_WINDOW_MODE_BORDERLESS) {
        SDL_DisplayMode desktopMode;

        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): a desktop-sized
         * undecorated window avoids the OS-exclusive fullscreen state while
         * retaining a seamless presentation for fast task switching. */
        memset(&desktopMode, 0, sizeof(desktopMode));
        if (SDL_GetCurrentDisplayMode(0, &desktopMode) == 0) {
            width = desktopMode.w;
            height = desktopMode.h;
        }
        flags |= SDL_WINDOW_BORDERLESS;
    }

    coduoSdlWindow = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (coduoSdlWindow == NULL)
        return qfalse;

    coduoSdlGlContext = SDL_GL_CreateContext(coduoSdlWindow);
    if (coduoSdlGlContext == NULL) {
        SDL_DestroyWindow(coduoSdlWindow);
        coduoSdlWindow = NULL;
        return qfalse;
    }

    if (SDL_GL_MakeCurrent(coduoSdlWindow, coduoSdlGlContext) != 0) {
        CoduoSDL_DestroyOpenGLWindow();
        return qfalse;
    }

    (void)SDL_GL_SetSwapInterval(0);
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): original
     * GLW_CreateWindow explicitly calls SetForegroundWindow and SetFocus at
     * CoDUOMP.exe 0x004f4742..0x004f4756. Re-establish focus after replacing
     * the SDL window, rebase the translated mouse state without treating the
     * focus click as gameplay input, then reproduce AppActivate so stale
     * engine key state cannot survive the replacement. */
    SDL_RaiseWindow(coduoSdlWindow);
#if defined(__APPLE__)
    coduomp_sdl_rebase_mouse_buttons_compat();
#endif
    AppActivate(qtrue, qfalse);
    return qtrue;
}

void CoduoSDL_DestroyOpenGLWindow(void)
{
    if (coduoSdlGlContext != NULL) {
        SDL_GL_DeleteContext(coduoSdlGlContext);
        coduoSdlGlContext = NULL;
    }
    if (coduoSdlWindow != NULL) {
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): original
         * GLimp_Shutdown hides the window before DestroyWindow at
         * CoDUOMP.exe 0x004f7024..0x004f7034. The synchronous WM_ACTIVATE
         * path reaches AppActivate, whose first action at 0x0046fa95 is
         * Key_ClearStates. Perform that complete transition before SDL can
         * discard the old window's pending mouse-button release. */
        AppActivate(qfalse, qfalse);
        SDL_DestroyWindow(coduoSdlWindow);
        coduoSdlWindow = NULL;
    }
}

void CoduoSDL_GetDesktopMode(int32_t *width, int32_t *height, int32_t *refreshRate)
{
    SDL_DisplayMode mode;

    memset(&mode, 0, sizeof(mode));
    if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
        *width = 0;
        *height = 0;
        *refreshRate = 0;
        return;
    }

    *width = mode.w;
    *height = mode.h;
    *refreshRate = mode.refresh_rate;
}

#if defined(__APPLE__)
/* NOT_FROM_ORIGINAL_SOURCE: selects the physical panel mode identified by
 * CoreGraphics. Scaled modes can have larger backing surfaces than the panel,
 * so neither their logical dimensions nor pixel dimensions are native. */
static qboolean coduomp_sdl_find_native_display_mode_compat(CFArrayRef modes, int32_t *width, int32_t *height, int32_t *refreshRate)
{
    size_t selectedWidth = 0;
    size_t selectedHeight = 0;
    double selectedRefreshRate = 0.0;
    qboolean selectedIsNative = qfalse;
    const CFIndex modeCount = CFArrayGetCount(modes);

    for (CFIndex index = 0; index < modeCount; ++index) {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, index);
        const size_t pixelWidth = CGDisplayModeGetPixelWidth(mode);
        const size_t pixelHeight = CGDisplayModeGetPixelHeight(mode);
        const double modeRefreshRate = CGDisplayModeGetRefreshRate(mode);
        const qboolean modeIsNative = (CGDisplayModeGetIOFlags(mode) & kDisplayModeNativeFlag) != 0 ? qtrue : qfalse;
        const qboolean modeIsUnscaled =
            CGDisplayModeGetWidth(mode) == pixelWidth && CGDisplayModeGetHeight(mode) == pixelHeight ? qtrue : qfalse;

        if (!CGDisplayModeIsUsableForDesktopGUI(mode) || (modeIsNative == qfalse && modeIsUnscaled == qfalse) ||
            (selectedIsNative != qfalse && modeIsNative == qfalse)) {
            continue;
        }

        if ((modeIsNative != qfalse && selectedIsNative == qfalse) || pixelWidth * pixelHeight > selectedWidth * selectedHeight ||
            (pixelWidth == selectedWidth && pixelHeight == selectedHeight && modeRefreshRate > selectedRefreshRate)) {
            selectedWidth = pixelWidth;
            selectedHeight = pixelHeight;
            selectedRefreshRate = modeRefreshRate;
            selectedIsNative = modeIsNative;
        }
    }

    if (selectedWidth == 0 || selectedHeight == 0)
        return qfalse;

    *width = (int32_t)selectedWidth;
    *height = (int32_t)selectedHeight;
    *refreshRate = (int32_t)selectedRefreshRate;
    return qtrue;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: reports the primary display's hardware-sized
 * pixel mode. Desktop scaling remains a separate logical-window concern. */
qboolean coduomp_sdl_get_native_display_mode_compat(int32_t *width, int32_t *height, int32_t *refreshRate)
{
#if defined(__APPLE__)
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
    qboolean found;

    if (modes == NULL)
        return qfalse;
    found = coduomp_sdl_find_native_display_mode_compat(modes, width, height, refreshRate);
    CFRelease(modes);
    return found;
#else
    int32_t selectedWidth = 0;
    int32_t selectedHeight = 0;
    int32_t selectedRefreshRate = 0;
    const int modeCount = SDL_GetNumDisplayModes(0);

    /* SDL2 does not expose an output's preferred-mode marker. Select the
     * largest mode the active display actually enumerates instead of calling
     * the scaled/current desktop mode native. */
    for (int index = 0; index < modeCount; ++index) {
        SDL_DisplayMode mode;

        memset(&mode, 0, sizeof(mode));
        if (SDL_GetDisplayMode(0, index, &mode) != 0 || mode.w <= 0 || mode.h <= 0) {
            continue;
        }
        if ((int64_t)mode.w * mode.h > (int64_t)selectedWidth * selectedHeight ||
            (mode.w == selectedWidth && mode.h == selectedHeight && mode.refresh_rate > selectedRefreshRate)) {
            selectedWidth = mode.w;
            selectedHeight = mode.h;
            selectedRefreshRate = mode.refresh_rate;
        }
    }

    if (selectedWidth <= 0 || selectedHeight <= 0)
        return qfalse;
    *width = selectedWidth;
    *height = selectedHeight;
    *refreshRate = selectedRefreshRate;
    return qtrue;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: converts requested drawable pixels to SDL window
 * coordinates using the active macOS backing scale. Other SDL platforms use
 * pixel-sized window coordinates and leave the request unchanged. */
void coduomp_sdl_window_size_for_drawable_compat(int32_t *width, int32_t *height)
{
#if defined(__APPLE__)
    CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(CGMainDisplayID());

    if (currentMode == NULL)
        return;

    const size_t logicalWidth = CGDisplayModeGetWidth(currentMode);
    const size_t logicalHeight = CGDisplayModeGetHeight(currentMode);
    const size_t pixelWidth = CGDisplayModeGetPixelWidth(currentMode);
    const size_t pixelHeight = CGDisplayModeGetPixelHeight(currentMode);

    if (logicalWidth != 0 && logicalHeight != 0 && pixelWidth != 0 && pixelHeight != 0) {
        *width = (int32_t)(((int64_t)*width * (int64_t)logicalWidth + (int64_t)pixelWidth / 2) / (int64_t)pixelWidth);
        *height = (int32_t)(((int64_t)*height * (int64_t)logicalHeight + (int64_t)pixelHeight / 2) / (int64_t)pixelHeight);
    }
    CGDisplayModeRelease(currentMode);
#else
    (void)width;
    (void)height;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: reports whether the primary display exposes an
 * exact preset resolution. macOS display modes can include supersampled
 * backing surfaces larger than the physical panel, so cap candidates at the
 * mode that Core Graphics identifies as native. */
qboolean coduomp_sdl_display_mode_available_compat(int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0)
        return qfalse;

#if defined(__APPLE__)
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
    int32_t nativeWidth = 0;
    int32_t nativeHeight = 0;
    int32_t nativeRefreshRate = 0;
    qboolean available = qfalse;

    if (modes == NULL)
        return qfalse;

    if (coduomp_sdl_find_native_display_mode_compat(modes, &nativeWidth, &nativeHeight, &nativeRefreshRate) == qfalse) {
        CFRelease(modes);
        return qfalse;
    }

    if (width <= nativeWidth && height <= nativeHeight) {
        const CFIndex modeCount = CFArrayGetCount(modes);

        for (CFIndex index = 0; index < modeCount; ++index) {
            CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, index);

            if (CGDisplayModeIsUsableForDesktopGUI(mode) && CGDisplayModeGetPixelWidth(mode) == (size_t)width &&
                CGDisplayModeGetPixelHeight(mode) == (size_t)height) {
                available = qtrue;
                break;
            }
        }
    }

    CFRelease(modes);
    return available;
#else
    const int modeCount = SDL_GetNumDisplayModes(0);

    for (int index = 0; index < modeCount; ++index) {
        SDL_DisplayMode mode;

        memset(&mode, 0, sizeof(mode));
        if (SDL_GetDisplayMode(0, index, &mode) == 0 && mode.w == width && mode.h == height) {
            return qtrue;
        }
    }
    return qfalse;
#endif
}

void CoduoSDL_GetFramebufferSize(int32_t *width, int32_t *height)
{
    if (coduoSdlWindow == NULL) {
        *width = 0;
        *height = 0;
        return;
    }
    SDL_GL_GetDrawableSize(coduoSdlWindow, width, height);
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the SDL window's realized logical size for
 * resolution-independent console composition. On high-DPI macOS displays this
 * is measured in screen coordinates and can differ from drawable pixels. */
void coduomp_sdl_get_window_size_compat(int32_t *width, int32_t *height)
{
    if (coduoSdlWindow == NULL) {
        *width = 0;
        *height = 0;
        return;
    }
    SDL_GetWindowSize(coduoSdlWindow, width, height);
}

/* NOT_FROM_ORIGINAL_SOURCE: native display-gamma boundary used by the
 * recovered renderer's existing 256-entry RGB ramp. SDL selects the display
 * containing this window and maps the operation to the host window system. */
qboolean coduomp_sdl_get_window_gamma_ramp(uint16_t red[256], uint16_t green[256], uint16_t blue[256])
{
    if (coduoSdlWindow == NULL)
        return qfalse;

    return SDL_GetWindowGammaRamp(coduoSdlWindow, red, green, blue) == 0 ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: install a renderer-generated gamma ramp on the
 * display containing the native game window. */
qboolean coduomp_sdl_set_window_gamma_ramp(const uint16_t red[256], const uint16_t green[256], const uint16_t blue[256])
{
    if (coduoSdlWindow == NULL)
        return qfalse;

    return SDL_SetWindowGammaRamp(coduoSdlWindow, red, green, blue) == 0 ? qtrue : qfalse;
}


/* NOT_FROM_ORIGINAL_SOURCE: retain SDL's diagnostic for a failed portable
 * display operation instead of reducing every backend failure to one generic
 * renderer message. */
const char *coduomp_sdl_error_compat(void)
{
    return SDL_GetError();
}

void CoduoSDL_GetOpenGLFormat(int32_t *colorBits, int32_t *depthBits, int32_t *stencilBits)
{
    int redBits = 0;
    int greenBits = 0;
    int blueBits = 0;
    int alphaBits = 0;

    (void)SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &redBits);
    (void)SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &greenBits);
    (void)SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &blueBits);
    (void)SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &alphaBits);
    (void)SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, depthBits);
    (void)SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, stencilBits);
    *colorBits = redBits + greenBits + blueBits + alphaBits;
}

/* NOT_FROM_ORIGINAL_SOURCE: confines SDL's object-pointer carrier for OpenGL
 * entry points to the same typed-copy boundary used by the native loader. */
void CoduoSDL_GetOpenGLSymbol(const char *name, void *destination, size_t destinationSize)
{
    void *symbol = SDL_GL_GetProcAddress(name);

    memset(destination, 0, destinationSize);
    if (destinationSize <= sizeof(symbol))
        memcpy(destination, &symbol, destinationSize);
}

void CoduoSDL_SwapWindow(void)
{
    if (coduoSdlWindow != NULL)
        SDL_GL_SwapWindow(coduoSdlWindow);
}

void CoduoSDL_SetSwapInterval(int32_t interval)
{
    if (coduoSdlGlContext != NULL)
        (void)SDL_GL_SetSwapInterval(interval);
}

void CoduoSDL_SetRelativeMouse(qboolean active)
{
    if (coduoSdlWindow == NULL)
        return;

    (void)SDL_SetRelativeMouseMode(active != qfalse ? SDL_TRUE : SDL_FALSE);
    SDL_SetWindowGrab(coduoSdlWindow, active != qfalse ? SDL_TRUE : SDL_FALSE);
}

qboolean CoduoSDL_HasOpenGLWindow(void)
{
    return coduoSdlWindow != NULL && coduoSdlGlContext != NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: portable replacement for the original Win32
 * MessageBoxA fatal-error boundary. */
void CoduoSDL_ShowErrorDialog(const char *message, const char *title)
{
    (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, coduoSdlWindow);
}

static int32_t CoduoSDL_MapKey(SDL_Keycode key)
{
    if (key >= SDLK_SPACE && key <= SDLK_z)
        return (int32_t)key;

    switch (key) {
    case SDLK_TAB:
        return K_TAB;
    case SDLK_RETURN:
        return K_ENTER;
    case SDLK_ESCAPE:
        return K_ESCAPE;
    case SDLK_BACKSPACE:
        return K_BACKSPACE;
    case SDLK_CAPSLOCK:
        return K_CAPSLOCK;
    case SDLK_PAUSE:
        return K_PAUSE;
    case SDLK_UP:
        return K_UPARROW;
    case SDLK_DOWN:
        return K_DOWNARROW;
    case SDLK_LEFT:
        return K_LEFTARROW;
    case SDLK_RIGHT:
        return K_RIGHTARROW;
    case SDLK_LALT:
    case SDLK_RALT:
        return K_ALT;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
        return K_CTRL;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
        return K_SHIFT;
    case SDLK_LGUI:
    case SDLK_RGUI:
        return K_COMMAND;
    case SDLK_INSERT:
        return K_INS;
    case SDLK_DELETE:
        return K_DEL;
    case SDLK_PAGEDOWN:
        return K_PGDN;
    case SDLK_PAGEUP:
        return K_PGUP;
    case SDLK_HOME:
        return K_HOME;
    case SDLK_END:
        return K_END;
    case SDLK_F1:
        return K_F1;
    case SDLK_F2:
        return K_F2;
    case SDLK_F3:
        return K_F3;
    case SDLK_F4:
        return K_F4;
    case SDLK_F5:
        return K_F5;
    case SDLK_F6:
        return K_F6;
    case SDLK_F7:
        return K_F7;
    case SDLK_F8:
        return K_F8;
    case SDLK_F9:
        return K_F9;
    case SDLK_F10:
        return K_F10;
    case SDLK_F11:
        return K_F11;
    case SDLK_F12:
        return K_F12;
    case SDLK_KP_7:
        return K_KP_HOME;
    case SDLK_KP_8:
        return K_KP_UPARROW;
    case SDLK_KP_9:
        return K_KP_PGUP;
    case SDLK_KP_4:
        return K_KP_LEFTARROW;
    case SDLK_KP_5:
        return K_KP_5;
    case SDLK_KP_6:
        return K_KP_RIGHTARROW;
    case SDLK_KP_1:
        return K_KP_END;
    case SDLK_KP_2:
        return K_KP_DOWNARROW;
    case SDLK_KP_3:
        return K_KP_PGDN;
    case SDLK_KP_ENTER:
        return K_KP_ENTER;
    case SDLK_KP_0:
        return K_KP_INS;
    case SDLK_KP_PERIOD:
        return K_KP_DEL;
    case SDLK_KP_DIVIDE:
        return K_KP_SLASH;
    case SDLK_KP_MINUS:
        return K_KP_MINUS;
    case SDLK_KP_PLUS:
        return K_KP_PLUS;
    case SDLK_NUMLOCKCLEAR:
        return K_KP_NUMLOCK;
    case SDLK_KP_MULTIPLY:
        return K_KP_STAR;
    case SDLK_KP_EQUALS:
        return K_KP_EQUALS;
    default:
        return 0;
    }
}

static int32_t CoduoSDL_MapMouseButton(uint8_t button)
{
    switch (button) {
    case SDL_BUTTON_LEFT:
        return K_MOUSE1;
    case SDL_BUTTON_RIGHT:
        return K_MOUSE2;
    case SDL_BUTTON_MIDDLE:
        return K_MOUSE3;
    case SDL_BUTTON_X1:
        return K_MOUSE4;
    case SDL_BUTTON_X2:
        return K_MOUSE5;
    default:
        return 0;
    }
}

#if defined(__APPLE__)
/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): SDL 2.32 retains its
 * per-input-source mouse-button latch when a window is destroyed. If the old
 * window loses a release, SDL_PrivateSendMouseButton suppresses the first
 * press on the replacement window as a duplicate. Publish changes through a
 * separate engine-facing latch so physical-state reconciliation can restore
 * the missing transition without duplicating ordinary SDL button events. */
static void coduomp_sdl_publish_mouse_button_compat(uint8_t button, qboolean down, int32_t time)
{
    const int32_t key = CoduoSDL_MapMouseButton(button);
    const uint32_t buttonBit = SDL_BUTTON(button);
    const qboolean wasDown = (coduoSdlReportedMouseButtons & buttonBit) != 0 ? qtrue : qfalse;

    if (key == 0 || wasDown == down)
        return;

    if (down != qfalse)
        coduoSdlReportedMouseButtons |= buttonBit;
    else
        coduoSdlReportedMouseButtons &= ~buttonBit;

    Sys_QueEvent(time, SE_KEY, key, down, 0, NULL);
}

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): an unmatched release for a
 * button whose SDL latch was proven stale means SDL suppressed that click's
 * press. Reconstruct both transitions so even a click completed during a
 * blocked restart frame reaches the original +button impulse logic. */
static void coduomp_sdl_publish_mouse_event_compat(uint8_t button, qboolean down, int32_t time)
{
    const uint32_t buttonBit = SDL_BUTTON(button);

    if (CoduoSDL_MapMouseButton(button) == 0)
        return;

    if (down == qfalse && (coduoSdlReportedMouseButtons & buttonBit) == 0 && (coduoSdlStaleMouseButtons & buttonBit) != 0) {
        coduomp_sdl_publish_mouse_button_compat(button, qtrue, time);
    }

    coduoSdlStaleMouseButtons &= ~buttonBit;
    coduomp_sdl_publish_mouse_button_compat(button, down, time);
}

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): retain ordinary
 * event-driven clicks while repairing a suppressed press that remains
 * physically held at the pump boundary. */
static void coduomp_sdl_reconcile_mouse_buttons_compat(void)
{
    static const uint8_t buttons[] = {SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, SDL_BUTTON_MIDDLE, SDL_BUTTON_X1, SDL_BUTTON_X2};
    const uint32_t physicalButtons = coduomp_sdl_current_mouse_buttons_compat();

    for (size_t index = 0; index < sizeof(buttons) / sizeof(buttons[0]); ++index) {
        const uint8_t button = buttons[index];
        const qboolean down = (physicalButtons & SDL_BUTTON(button)) != 0 ? qtrue : qfalse;

        coduomp_sdl_publish_mouse_button_compat(button, down, 0);
    }
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: translates SDL's portable native events into the
 * recovered engine's original key/character/mouse event queue. */
void CoduoSDL_PumpEvents(void)
{
    SDL_Event event;

    if (coduoSdlInitialized == qfalse)
        return;

    while (SDL_PollEvent(&event) != 0) {
        sysMsgTime = (int32_t)event.common.timestamp;

        switch (event.type) {
        case SDL_QUIT:
            Com_Quit_f();
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                Com_Quit_f();
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                coduomp_gamma_window_focus_changed(qtrue);
#if defined(__APPLE__)
                coduomp_sdl_rebase_mouse_buttons_compat();
#endif
                AppActivate(qtrue, qfalse);
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                coduomp_gamma_window_focus_changed(qfalse);
                AppActivate(qfalse, qfalse);
                IN_ClearStates();
            }
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            const int32_t key = CoduoSDL_MapKey(event.key.keysym.sym);
            if (key != 0) {
                Sys_QueEvent(sysMsgTime, SE_KEY, key, event.type == SDL_KEYDOWN ? qtrue : qfalse, 0, NULL);
            }
            /* The original Win32 pump receives both WM_KEYDOWN/VK_BACK and
             * WM_CHAR/'\b'. SDL_TEXTINPUT does not report control characters,
             * so reproduce the missing character event at this platform
             * boundary. Field_KeyDownEvent deliberately does not edit on
             * K_BACKSPACE; Field_CharEvent performs the original deletion. */
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
                Sys_QueEvent(sysMsgTime, SE_CHAR, '\b', 0, 0, NULL);
            }
            /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): Win32 emits
             * character 22 for Ctrl-V, which the recovered field editor uses
             * to paste. SDL_TEXTINPUT omits shortcut control characters, so
             * synthesize that original event for Ctrl-V on Unix and for the
             * native Command-V chord on macOS. */
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_v && (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0) {
                Sys_QueEvent(sysMsgTime, SE_CHAR, CODUO_SDL_CHAR_PASTE, 0, 0, NULL);
            }
            break;
        }

        case SDL_TEXTINPUT:
            for (const uint8_t *text = (const uint8_t *)event.text.text; *text != '\0'; ++text) {
                if (*text < 0x80U) {
                    Sys_QueEvent(sysMsgTime, SE_CHAR, *text, 0, 0, NULL);
                }
            }
            break;

        case SDL_MOUSEMOTION:
            /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): SDL can report
             * desktop pointer motion after a fullscreen macOS window loses
             * focus. Win32 stops delivering the original window mouse
             * messages in that state. Preserve that boundary so inactive
             * motion cannot reach UI hover handling or gameplay input. */
            if (sysInputAppActive == qfalse)
                break;
            if (event.motion.xrel != 0 || event.motion.yrel != 0) {
                Sys_QueEvent(sysMsgTime, SE_MOUSE, event.motion.xrel, event.motion.yrel, 0, NULL);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            if (sysInputAppActive == qfalse)
                break;
#if defined(__APPLE__)
            if (coduoSdlWindow == NULL || event.button.windowID != SDL_GetWindowID(coduoSdlWindow)) {
                break;
            }
            coduomp_sdl_publish_mouse_event_compat(event.button.button, event.type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse, sysMsgTime);
#else
            const int32_t key = CoduoSDL_MapMouseButton(event.button.button);
            if (key != 0) {
                Sys_QueEvent(sysMsgTime, SE_KEY, key, event.type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse, 0, NULL);
            }
#endif
            break;
        }

        case SDL_MOUSEWHEEL: {
            if (sysInputAppActive == qfalse)
                break;
            int32_t wheelY = event.wheel.y;
            const int32_t key = wheelY > 0 ? K_MWHEELUP : K_MWHEELDOWN;
            if (wheelY != 0) {
                Sys_QueEvent(sysMsgTime, SE_KEY, key, qtrue, 0, NULL);
                Sys_QueEvent(sysMsgTime, SE_KEY, key, qfalse, 0, NULL);
            }
            break;
        }

        default:
            break;
        }
    }

#if defined(__APPLE__)
    if (sysInputAppActive != qfalse)
        coduomp_sdl_reconcile_mouse_buttons_compat();
#endif
}

#endif
