#include "backend.h"

#include "client/common/client_legacy_crt.h"
#include "gl_api.h"
#include "gl_debug.h"
#include "platform_gamma.h"
#include "wgl_debug.h"
#include "../server/server.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include "../system_console.h"
#include "../system_input.h"
#include "../system_localization.h"
#include "../system_platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include "../platform/sdl_platform.h"
#endif

enum renderer_pixel_format_flag_e {
    R_PFD_DOUBLEBUFFER = 0x00000001,
    R_PFD_STEREO = 0x00000002,
    R_PFD_DRAW_TO_WINDOW = 0x00000004,
    R_PFD_SUPPORT_OPENGL = 0x00000020,
    R_PFD_GENERIC_FORMAT = 0x00000040,
    R_PFD_GENERIC_ACCELERATED = 0x00001000
};

enum {
    R_PFD_TYPE_RGBA = 0,
    R_PFD_MAIN_PLANE = 0,
    R_MAX_PIXEL_FORMATS = 1024,
    R_MIN_DEPTH_BITS = 15,
    R_MIN_STENCIL_BITS = 4,
    R_MIN_ACCEPTABLE_COLOR_BITS = 15,
    R_BASE_COLOR_BITS = 16,
    R_HIGH_QUALITY_DEPTH_BITS = 24,
    R_DESKTOP_COLOR_WARNING_BITS = 24,
    R_WINDOW_ICON_RESOURCE = 133,
    R_AUTOCONFIG_MAX_CVARS = 1024,
    R_AUTOCONFIG_VALUE_SIZE = 64
};


typedef enum renderer_display_change_result_e {
    R_DISPLAY_CHANGE_BAD_PARAMETER = -5,
    R_DISPLAY_CHANGE_BAD_FLAGS = -4,
    R_DISPLAY_CHANGE_NOT_UPDATED = -3,
    R_DISPLAY_CHANGE_BAD_MODE = -2,
    R_DISPLAY_CHANGE_FAILED = -1,
    R_DISPLAY_CHANGE_RESTART_REQUIRED = 1
} renderer_display_change_result_t;

cvar_t *r_allowSoftwareGL;
renderer_wgl_render_context_t rendererWin32RenderContext;
qboolean rendererWin32PixelFormatSet;
int32_t rendererWin32DesktopColorBits;
int32_t rendererWin32DesktopWidth;
int32_t rendererWin32DesktopHeight;
renderer_win32_window_proc_t rendererWin32WindowProcedure;
qboolean rendererWin32FullscreenModeSet;
#if defined(_WIN32)
static renderer_pixel_format_descriptor_t rendererWin32PixelFormat;
static qboolean rendererWin32WindowClassRegistered;
#endif

/* Source: CoDUOMP.exe 0x004f3fa0..0x004f4079.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3fa0_004f407a.mcode.
 * Provisional role name: the following GLW_ChoosePFD call receives the exact
 * 40-byte descriptor built here. Every otherwise-unused descriptor field is
 * explicitly zeroed by the original instructions. */
void GLW_BuildPFD(renderer_pixel_format_descriptor_t *descriptor,
                  int32_t colorBits, int32_t depthBits,
                  int32_t stencilBits, qboolean stereo)
{
    renderer_pixel_format_descriptor_t preferred = {0};

    preferred.size = sizeof(preferred);
    preferred.version = 1;
    preferred.flags = R_PFD_DRAW_TO_WINDOW |
                      R_PFD_SUPPORT_OPENGL |
                      R_PFD_DOUBLEBUFFER;
    preferred.pixelType = R_PFD_TYPE_RGBA;
    preferred.colorBits = (uint8_t)colorBits;
    preferred.depthBits = (uint8_t)depthBits;
    preferred.stencilBits = (uint8_t)stencilBits;
    preferred.layerType = R_PFD_MAIN_PLANE;

    if (stereo != qfalse) {
        ri.Printf(R_PRINT_ALL, "...attempting to use stereo\n");
        glConfig.stereoEnabled = qtrue;
        preferred.flags |= R_PFD_STEREO;
    } else {
        glConfig.stereoEnabled = qfalse;
    }

    *descriptor = preferred;
}

/* Source: CoDUOMP.exe 0x004f3c70..0x004f3f90.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3c70_004f3f91.mcode.
 * Name: the function identifies itself in its diagnostic strings. The local
 * array is indexed by the one-based GDI pixel-format number; element zero is
 * the scratch descriptor used by the initial count query. */
int32_t GLW_ChoosePFD(
    renderer_wgl_device_context_t deviceContext,
    renderer_pixel_format_descriptor_t *preferred)
{
#if defined(_WIN32)
    renderer_pixel_format_descriptor_t formats[R_MAX_PIXEL_FORMATS + 1];
    int32_t selected = 0;

    ri.Printf(R_PRINT_ALL, "...GLW_ChoosePFD( %d, %d, %d )\n",
              preferred->colorBits, preferred->depthBits,
              preferred->stencilBits);

    int32_t formatCount = DescribePixelFormat(
        (HDC)deviceContext, 1, sizeof(formats[0]),
        (PIXELFORMATDESCRIPTOR *)&formats[0]);
    if (formatCount > R_MAX_PIXEL_FORMATS) {
        ri.Printf(R_PRINT_WARNING,
                  "...numPFDs > MAX_PFDS (%d > %d)\n",
                  formatCount, R_MAX_PIXEL_FORMATS);
        formatCount = R_MAX_PIXEL_FORMATS;
    }
    ri.Printf(R_PRINT_ALL, "...%d PFDs found\n", formatCount - 1);

    for (int32_t index = 1; index <= formatCount; ++index) {
        renderer_pixel_format_descriptor_t *candidate = &formats[index];
        DescribePixelFormat(
            (HDC)deviceContext, index, sizeof(*candidate),
            (PIXELFORMATDESCRIPTOR *)candidate);

        if ((candidate->flags & R_PFD_GENERIC_FORMAT) != 0 &&
            r_allowSoftwareGL->integer == 0) {
            if (r_logFile->integer != 0) {
                ri.Printf(R_PRINT_ALL,
                          "...PFD %d rejected, software acceleration\n",
                          index);
            }
            continue;
        }

        if (r_logFile->integer != 0) {
            ri.Printf(R_PRINT_ALL,
                      "ChoosePFD: format %i: %i color %i depth %i stencil\n",
                      index, candidate->colorBits, candidate->depthBits,
                      candidate->stencilBits);
        }

        if (candidate->pixelType != R_PFD_TYPE_RGBA) {
            if (r_logFile->integer != 0)
                ri.Printf(R_PRINT_ALL,
                          "...PFD %d rejected, not RGBA\n", index);
            continue;
        }
        if ((candidate->flags & preferred->flags) != preferred->flags) {
            if (r_logFile->integer != 0) {
                ri.Printf(R_PRINT_ALL,
                          "...PFD %d rejected, improper flags (%x instead of %x)\n",
                          index, candidate->flags, preferred->flags);
            }
            continue;
        }
        if (candidate->depthBits < R_MIN_DEPTH_BITS) {
            if (r_logFile->integer != 0) {
                ri.Printf(R_PRINT_ALL,
                          "...PFD %d rejected, insufficient depth bits (%d instead of %d)\n",
                          index, candidate->depthBits, preferred->depthBits);
            }
            continue;
        }
        if (candidate->stencilBits < R_MIN_STENCIL_BITS &&
            preferred->stencilBits > 0) {
            if (r_logFile->integer != 0) {
                ri.Printf(R_PRINT_ALL,
                          "...PFD %d rejected, insufficient stencil bits (%d instead of %d)\n",
                          index, candidate->stencilBits,
                          preferred->stencilBits);
            }
            continue;
        }

        if (selected == 0) {
            selected = index;
            continue;
        }

        const renderer_pixel_format_descriptor_t *best = &formats[selected];
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (((candidate->flags ^ best->flags) & R_PFD_STEREO) != 0 &&
            (preferred->flags & R_PFD_STEREO) != 0) {
            selected = index;
            continue;
        }
        if (best->colorBits != preferred->colorBits &&
            (candidate->colorBits == preferred->colorBits ||
             candidate->colorBits > best->colorBits)) {
            selected = index;
            continue;
        }
        if (best->depthBits != preferred->depthBits &&
            (candidate->depthBits == preferred->depthBits ||
             candidate->depthBits > best->depthBits)) {
            selected = index;
            continue;
        }
        if (best->stencilBits != preferred->stencilBits &&
            (candidate->stencilBits == preferred->stencilBits ||
             (candidate->stencilBits > best->stencilBits &&
              preferred->stencilBits > 0))) {
            selected = index;
        }
    }

    if (selected == 0)
        return 0;

    const renderer_pixel_format_descriptor_t *best = &formats[selected];
    if ((best->flags & R_PFD_GENERIC_FORMAT) != 0) {
        if (r_allowSoftwareGL->integer == 0) {
            ri.Printf(R_PRINT_ALL,
                      "...no hardware acceleration found\n");
            return 0;
        }
        ri.Printf(R_PRINT_ALL, "...using software emulation\n");
    } else if ((best->flags & R_PFD_GENERIC_ACCELERATED) != 0) {
        ri.Printf(R_PRINT_ALL, "...MCD acceleration found\n");
    } else {
        ri.Printf(R_PRINT_ALL, "...hardware acceleration found\n");
    }

    *preferred = *best;
    return selected;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: WGL/GDI is absent on non-Windows hosts; the
     * native renderer platform implementation supplies its own format path. */
    (void)deviceContext;
    (void)preferred;
    return 0;
#endif
}

/* Source: CoDUOMP.exe 0x004f4110..0x004f427a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4110_004f427b.mcode.
 * Provisional role name: the incoming descriptor is selected and installed
 * before this routine creates and activates the one global WGL context. The
 * three result values are distinguished by both callers at 0x004f4280. */
renderer_wgl_setup_result_t GLW_SetPFD(
    renderer_pixel_format_descriptor_t *descriptor)
{
#if defined(_WIN32)
    if (rendererWin32PixelFormatSet == qfalse) {
        const int32_t pixelFormat =
            GLW_ChoosePFD(rendererWin32DeviceContext, descriptor);
        if (pixelFormat == 0) {
            ri.Printf(R_PRINT_ALL, "...GLW_ChoosePFD failed\n");
            return R_WGL_SETUP_PIXEL_FORMAT_FAILED;
        }

        ri.Printf(R_PRINT_ALL, "...PIXELFORMAT %d selected\n", pixelFormat);
        DescribePixelFormat(
            (HDC)rendererWin32DeviceContext, pixelFormat,
            sizeof(*descriptor), (PIXELFORMATDESCRIPTOR *)descriptor);
        if (SetPixelFormat(
                (HDC)rendererWin32DeviceContext, pixelFormat,
                (PIXELFORMATDESCRIPTOR *)descriptor) == FALSE) {
            ri.Printf(R_PRINT_ALL, "...SetPixelFormat failed\n",
                      rendererWin32DeviceContext);
            return R_WGL_SETUP_PIXEL_FORMAT_FAILED;
        }
        rendererWin32PixelFormatSet = qtrue;
    }

    if (rendererWin32RenderContext != NULL)
        return R_WGL_SETUP_SUCCESS;

    ri.Printf(R_PRINT_ALL, "...creating GL context: ");
    rendererWin32RenderContext =
        qwglCreateContext(rendererWin32DeviceContext);
    if (rendererWin32RenderContext != NULL) {
        ri.Printf(R_PRINT_ALL, "succeeded\n");
        ri.Printf(R_PRINT_ALL, "...making context current: ");
        if (qwglMakeCurrent(
                rendererWin32DeviceContext,
                rendererWin32RenderContext) != 0) {
            ri.Printf(R_PRINT_ALL, "succeeded\n");
            if (GLW_ValidateOpenGLStrings() != qfalse)
                return R_WGL_SETUP_SUCCESS;
            qwglMakeCurrent(NULL, NULL);
        }

        qwglDeleteContext(rendererWin32RenderContext);
        rendererWin32RenderContext = NULL;
    }

    ri.Printf(R_PRINT_ALL, "failed\n");
    return R_WGL_SETUP_CONTEXT_FAILED;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: WGL is absent on non-Windows hosts. */
    (void)descriptor;
    return R_WGL_SETUP_CONTEXT_FAILED;
#endif
}

/* Source: CoDUOMP.exe 0x004f4280..0x004f44b2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4280_004f44b3.mcode.
 * Provisional role name: the diagnostic, device-context acquisition, pixel
 * format negotiation, context setup, and final glConfig bit-depth stores
 * establish this as the Win32 OpenGL driver initializer. */
qboolean GLW_InitDriver(int32_t colorBits)
{
#if defined(_WIN32)
    ri.Printf(R_PRINT_ALL, "Initializing OpenGL driver\n");

    if (rendererWin32DeviceContext == NULL) {
        ri.Printf(R_PRINT_ALL, "...getting DC: ");
        rendererWin32DeviceContext =
            (renderer_wgl_device_context_t)GetDC(win32MainWindow);
        if (rendererWin32DeviceContext == NULL) {
            ri.Printf(R_PRINT_ALL, "failed\n");
            return qfalse;
        }
        ri.Printf(R_PRINT_ALL, "succeeded\n");
    }

    if (colorBits == 0)
        colorBits = rendererWin32DesktopColorBits;

    int32_t depthBits = r_depthbits->integer;
    if (depthBits == 0)
        depthBits = colorBits > R_BASE_COLOR_BITS
                        ? R_HIGH_QUALITY_DEPTH_BITS
                        : R_BASE_COLOR_BITS;

    int32_t stencilBits = r_stencilbits->integer;
    if (depthBits < R_HIGH_QUALITY_DEPTH_BITS)
        stencilBits = 0;

    if (rendererWin32PixelFormatSet == qfalse) {
        GLW_BuildPFD(&rendererWin32PixelFormat, colorBits, depthBits,
                     stencilBits, qfalse);
        renderer_wgl_setup_result_t result =
            GLW_SetPFD(&rendererWin32PixelFormat);
        if (result != R_WGL_SETUP_SUCCESS) {
            if (result == R_WGL_SETUP_CONTEXT_FAILED) {
                ri.Printf(R_PRINT_ALL, "...failed hard\n");
                return qfalse;
            }

            if (r_colorbits->integer == rendererWin32DesktopColorBits &&
                stencilBits == 0) {
                ReleaseDC(win32MainWindow,
                          (HDC)rendererWin32DeviceContext);
                rendererWin32DeviceContext = NULL;
                ri.Printf(R_PRINT_ALL,
                          "...failed to find an appropriate PIXELFORMAT\n");
                return qfalse;
            }

            if (colorBits > rendererWin32DesktopColorBits)
                colorBits = rendererWin32DesktopColorBits;
            GLW_BuildPFD(&rendererWin32PixelFormat, colorBits, depthBits,
                         0, qfalse);
            if (GLW_SetPFD(&rendererWin32PixelFormat) !=
                R_WGL_SETUP_SUCCESS) {
                ReleaseDC(win32MainWindow,
                          (HDC)rendererWin32DeviceContext);
                rendererWin32DeviceContext = NULL;
                ri.Printf(R_PRINT_ALL,
                          "...failed to find an appropriate PIXELFORMAT\n");
                return qfalse;
            }
        }
    }

    int32_t maximumTextureSize;
    qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    if (glConfig.vidWidth <= maximumTextureSize &&
        glConfig.vidHeight <= maximumTextureSize) {
        glConfig.colorBits = rendererWin32PixelFormat.colorBits;
        glConfig.depthBits = rendererWin32PixelFormat.depthBits;
        glConfig.stencilBits = rendererWin32PixelFormat.stencilBits;
        return qtrue;
    }

    if (rendererWin32RenderContext != NULL) {
        qwglMakeCurrent(NULL, NULL);
        qwglDeleteContext(rendererWin32RenderContext);
        rendererWin32RenderContext = NULL;
    }
    if (rendererWin32DeviceContext != NULL) {
        ReleaseDC(win32MainWindow, (HDC)rendererWin32DeviceContext);
        rendererWin32DeviceContext = NULL;
    }
    rendererWin32PixelFormatSet = qfalse;
    ri.Printf(
        R_PRINT_ALL,
        "video card cannot set mode %ix%i because GL_MAX_TEXTURE_SIZE is only %i\n",
        glConfig.vidWidth, glConfig.vidHeight, maximumTextureSize);
    return qfalse;
#else
    int32_t maximumTextureSize;

    (void)colorBits;
    if (qglGetIntegerv == NULL)
        return qfalse;

    qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    if (glConfig.vidWidth > maximumTextureSize ||
        glConfig.vidHeight > maximumTextureSize) {
        ri.Printf(
            R_PRINT_ALL,
            "video card cannot set mode %ix%i because "
            "GL_MAX_TEXTURE_SIZE is only %i\n",
            glConfig.vidWidth, glConfig.vidHeight, maximumTextureSize);
        return qfalse;
    }

    CoduoSDL_GetOpenGLFormat(
        &glConfig.colorBits, &glConfig.depthBits,
        &glConfig.stencilBits);
    return qtrue;
#endif
}

/* Source: CoDUOMP.exe 0x004f44c0..0x004f4770.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f44c0_004f4771.mcode.
 * Provisional role name: callers supply the driver name, mode dimensions,
 * color depth, and fullscreen selection; this routine owns the game-window
 * class and creates or repositions win32MainWindow before GLW_InitDriver. */
qboolean GLW_CreateWindow(const char *driverName, int32_t width,
                          int32_t height, int32_t colorBits,
                          qboolean fullscreen)
{
#if defined(_WIN32)
    static const char windowClassName[] =
        "CoD:United Offensive Multiplayer";
    (void)driverName;

    if (rendererWin32WindowClassRegistered == qfalse) {
        WNDCLASSA windowClass = {0};
        windowClass.lpfnWndProc =
            (WNDPROC)rendererWin32WindowProcedure;
        windowClass.hInstance = (HINSTANCE)sysApplicationInstance;
        windowClass.hIcon = LoadIconA(
            (HINSTANCE)sysApplicationInstance,
            MAKEINTRESOURCEA(R_WINDOW_ICON_RESOURCE));
        windowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
        windowClass.hbrBackground = CreateSolidBrush(0);
        windowClass.lpszClassName = windowClassName;
        if (RegisterClassA(&windowClass) == 0) {
            ri.Error(ERR_FATAL,
                     "EXE_ERR_COULDNT_REGISTER_WINDOW");
        }
        rendererWin32WindowClassRegistered = qtrue;
        ri.Printf(R_PRINT_ALL, "...registered window class\n");
    }

    RECT windowRect = {0, 0, width, height};
    DWORD style;
    DWORD extendedStyle;
    int32_t windowX;
    int32_t windowY;

    if (fullscreen != qfalse) {
        style = WS_POPUP | WS_VISIBLE | WS_SYSMENU;
        extendedStyle = WS_EX_TOPMOST;
        windowX = 0;
        windowY = 0;
    } else {
        style = WS_VISIBLE | WS_CAPTION | WS_SYSMENU;
        extendedStyle = 0;
        AdjustWindowRect(&windowRect, style, FALSE);

        cvar_t *xPosition =
            ri.Cvar_Get("vid_xpos", "0", CVAR_NONE);
        cvar_t *yPosition =
            ri.Cvar_Get("vid_ypos", "0", CVAR_NONE);
        windowX = xPosition->integer;
        windowY = yPosition->integer;
        if (windowX < 0)
            windowX = 0;
        if (windowY < 0)
            windowY = 0;
    }

    const int32_t windowWidth = windowRect.right - windowRect.left;
    const int32_t windowHeight = windowRect.bottom - windowRect.top;
    if (fullscreen == qfalse &&
        windowWidth < rendererWin32DesktopWidth &&
        windowHeight < rendererWin32DesktopHeight) {
        if (windowX + windowWidth > rendererWin32DesktopWidth)
            windowX = rendererWin32DesktopWidth - windowWidth;
        if (windowY + windowHeight > rendererWin32DesktopHeight)
            windowY = rendererWin32DesktopHeight - windowHeight;
    }

    if (win32MainWindow == NULL) {
        win32MainWindow = CreateWindowExA(
            extendedStyle, windowClassName, windowClassName, style,
            windowX, windowY, windowWidth, windowHeight,
            NULL, NULL, (HINSTANCE)sysApplicationInstance, NULL);
        if (win32MainWindow == NULL) {
            ri.Error(ERR_FATAL,
                     "EXE_ERR_COULDNT_CREATE_WINDOW");
        }
        ShowWindow(win32MainWindow, SW_SHOW);
        UpdateWindow(win32MainWindow);
        ri.Printf(R_PRINT_ALL,
                  "...created window@%d,%d (%dx%d)\n",
                  windowX, windowY, windowWidth, windowHeight);
    } else {
        ri.Printf(
            R_PRINT_ALL,
            "...window already present, CreateWindowEx skipped\n");
        MoveWindow(win32MainWindow, windowX, windowY,
                   windowWidth, windowHeight, FALSE);
        ri.Printf(R_PRINT_ALL,
                  "...moved window to %d,%d (%dx%d)\n",
                  windowX, windowY, windowWidth, windowHeight);
    }

    if (GLW_InitDriver(colorBits) == qfalse) {
        ShowWindow(win32MainWindow, SW_HIDE);
        DestroyWindow(win32MainWindow);
        win32MainWindow = NULL;
        return qfalse;
    }

    SetForegroundWindow(win32MainWindow);
    SetFocus(win32MainWindow);
    if (sysSplashWindow != NULL)
        ShowWindow((HWND)sysSplashWindow, SW_HIDE);
    return qtrue;
#else
    (void)driverName;
    return CoduoSDL_CreateOpenGLWindow(
        width, height, colorBits, r_depthbits->integer,
        r_stencilbits->integer, fullscreen);
#endif
}


/* Source: CoDUOMP.exe 0x004f4780..0x004f4806.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4780_004f4807.mcode and the jump
 * table at 0x004f4808. Provisional role name: every recognized value is a
 * ChangeDisplaySettings result and the sole caller passes that API result. */
void GLW_PrintDisplayChangeError(int32_t result)
{
    switch (result) {
    case R_DISPLAY_CHANGE_BAD_PARAMETER:
        ri.Printf(R_PRINT_ALL, "bad param\n");
        break;
    case R_DISPLAY_CHANGE_BAD_FLAGS:
        ri.Printf(R_PRINT_ALL, "bad flags\n");
        break;
    case R_DISPLAY_CHANGE_NOT_UPDATED:
        ri.Printf(R_PRINT_ALL, "not updated\n");
        break;
    case R_DISPLAY_CHANGE_BAD_MODE:
        ri.Printf(R_PRINT_ALL, "bad mode\n");
        break;
    case R_DISPLAY_CHANGE_FAILED:
        ri.Printf(R_PRINT_ALL, "DISP_CHANGE_FAILED\n");
        break;
    case R_DISPLAY_CHANGE_RESTART_REQUIRED:
        ri.Printf(R_PRINT_ALL, "restart required\n");
        break;
    default:
        ri.Printf(R_PRINT_ALL, "unknown error %d\n", result);
        break;
    }
}

/* Source: CoDUOMP.exe 0x004f4830..0x004f4c4e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4830_004f4c4f.mcode.
 * Provisional role name: the four source arguments and all result paths match
 * the Win32 mode-setting stage immediately above GLW_CreateWindow. */
renderer_mode_set_result_t GLW_SetMode(
    const char *driverName, int32_t mode, int32_t colorBits,
    qboolean fullscreen)
{
#if defined(_WIN32)
    ri.Printf(R_PRINT_ALL, "...setting mode %d:", mode);
    if (R_GetModeInfo(&glConfig.vidWidth, &glConfig.vidHeight,
                      &glConfig.windowAspect, mode) == qfalse) {
        ri.Printf(R_PRINT_ALL, " invalid mode\n");
        return R_MODE_SET_INVALID;
    }

    const char *modeKind;
    if (fullscreen != qfalse) {
        if (r_displayRefresh->integer != 0) {
            modeKind = va("FS (%i Hz)", r_displayRefresh->integer);
        } else {
            modeKind = "FS";
        }
    } else {
        modeKind = "W";
    }
    ri.Printf(R_PRINT_ALL, " %d %d %s\n",
              glConfig.vidWidth, glConfig.vidHeight, modeKind);

    HDC desktopDeviceContext = GetDC(GetDesktopWindow());
    rendererWin32DesktopColorBits =
        GetDeviceCaps(desktopDeviceContext, BITSPIXEL);
    rendererWin32DesktopWidth =
        GetDeviceCaps(desktopDeviceContext, HORZRES);
    rendererWin32DesktopHeight =
        GetDeviceCaps(desktopDeviceContext, VERTRES);
    ReleaseDC(GetDesktopWindow(), desktopDeviceContext);

    if (fullscreen == qfalse) {
            while (rendererWin32DesktopWidth < glConfig.vidWidth ||
                   rendererWin32DesktopHeight < glConfig.vidHeight) {
                --mode;
                (void)R_GetModeInfo(
                    &glConfig.vidWidth, &glConfig.vidHeight,
                    &glConfig.windowAspect, mode);
            }
            ri.Cvar_Set("r_mode", va("%d", mode));
    }

    if ((rendererWin32DesktopColorBits <
             R_MIN_ACCEPTABLE_COLOR_BITS ||
         rendererWin32DesktopColorBits ==
             R_DESKTOP_COLOR_WARNING_BITS) &&
        (colorBits == 0 ||
         (fullscreen == qfalse &&
          colorBits >= R_MIN_ACCEPTABLE_COLOR_BITS))) {
        const int32_t choice = MessageBoxA(
            NULL,
            Sys_LocalizeString("WIN_COLORDEPTH_WARN_BODY"),
            Sys_LocalizeString("WIN_COLORDEPTH_WARN_TITLE"),
            MB_OKCANCEL | MB_ICONWARNING);
        if (choice != IDOK)
            return R_MODE_SET_INVALID;
    }

    if (fullscreen != qfalse) {
        DEVMODEA deviceMode = {0};
        deviceMode.dmSize = sizeof(deviceMode);
        deviceMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
        deviceMode.dmPelsWidth = (DWORD)glConfig.vidWidth;
        deviceMode.dmPelsHeight = (DWORD)glConfig.vidHeight;

        if (r_displayRefresh->integer != 0) {
            deviceMode.dmFields |= DM_DISPLAYFREQUENCY;
            deviceMode.dmDisplayFrequency =
                (DWORD)r_displayRefresh->integer;
        }
        if (colorBits != 0) {
            deviceMode.dmFields |= DM_BITSPERPEL;
            deviceMode.dmBitsPerPel = (DWORD)colorBits;
            ri.Printf(R_PRINT_ALL,
                      "...using colorbits of %d\n", colorBits);
        } else {
            ri.Printf(
                R_PRINT_ALL,
                "...using desktop display depth of %d\n",
                rendererWin32DesktopColorBits);
        }

        if (rendererWin32FullscreenModeSet != qfalse) {
            ri.Printf(
                R_PRINT_ALL,
                "...already fullscreen, avoiding redundant CDS\n");
            if (GLW_CreateWindow(
                    driverName, glConfig.vidWidth, glConfig.vidHeight,
                    colorBits, qtrue) == qfalse) {
                ri.Printf(R_PRINT_ALL,
                          "...restoring display settings\n");
                ChangeDisplaySettingsA(NULL, 0);
                return R_MODE_SET_INVALID;
            }
        } else {
            ri.Printf(R_PRINT_ALL, "...calling CDS: ");
            const int32_t displayResult =
                ChangeDisplaySettingsA(&deviceMode, CDS_FULLSCREEN);
            if (displayResult == DISP_CHANGE_SUCCESSFUL) {
                ri.Printf(R_PRINT_ALL, "ok\n");
                if (GLW_CreateWindow(
                        driverName, glConfig.vidWidth,
                        glConfig.vidHeight, colorBits,
                        qtrue) == qfalse) {
                    ri.Printf(R_PRINT_ALL,
                              "...restoring display settings\n");
                    ChangeDisplaySettingsA(NULL, 0);
                    return R_MODE_SET_INVALID;
                }
                rendererWin32FullscreenModeSet = qtrue;
            } else {
                ri.Printf(R_PRINT_ALL, "failed, ");
                ChangeDisplaySettingsA(NULL, 0);
                GLW_PrintDisplayChangeError(displayResult);

                --mode;
                ri.Cvar_Set("r_mode", va("%d", mode));
                ri.Printf(R_PRINT_ALL, "switch Mode to %d", mode);
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                return GLW_SetMode(
                    driverName, mode, colorBits, fullscreen);
            }
        }
    } else {
        if (rendererWin32FullscreenModeSet != qfalse)
            ChangeDisplaySettingsA(NULL, 0);
        rendererWin32FullscreenModeSet = qfalse;
        if (GLW_CreateWindow(
                driverName, glConfig.vidWidth, glConfig.vidHeight,
                colorBits, qfalse) == qfalse) {
            return R_MODE_SET_INVALID;
        }
    }

    DEVMODEA currentMode = {0};
    currentMode.dmSize = sizeof(currentMode);
    if (EnumDisplaySettingsA(
            NULL, ENUM_CURRENT_SETTINGS, &currentMode) != FALSE) {
        glConfig.displayFrequency =
            (int32_t)currentMode.dmDisplayFrequency;
    }
    glConfig.isFullscreen = fullscreen;
    return R_MODE_SET_SUCCESS;
#else
    int32_t refreshRate;

    ri.Printf(R_PRINT_ALL, "...setting mode %d:", mode);
    if (R_GetModeInfo(&glConfig.vidWidth, &glConfig.vidHeight,
                      &glConfig.windowAspect, mode) == qfalse) {
        ri.Printf(R_PRINT_ALL, " invalid mode\n");
        return R_MODE_SET_INVALID;
    }

    CoduoSDL_GetDesktopMode(
        &rendererWin32DesktopWidth, &rendererWin32DesktopHeight,
        &refreshRate);
    if (fullscreen == qfalse) {
            while ((rendererWin32DesktopWidth > 0 &&
                    rendererWin32DesktopWidth < glConfig.vidWidth) ||
                   (rendererWin32DesktopHeight > 0 &&
                    rendererWin32DesktopHeight < glConfig.vidHeight)) {
                --mode;
                if (R_GetModeInfo(
                        &glConfig.vidWidth, &glConfig.vidHeight,
                        &glConfig.windowAspect, mode) == qfalse) {
                    return R_MODE_SET_INVALID;
                }
            }
            ri.Cvar_Set("r_mode", va("%d", mode));
    }

    ri.Printf(
        R_PRINT_ALL, " %d %d %s\n",
        glConfig.vidWidth, glConfig.vidHeight,
        fullscreen != qfalse ? "FS" : "W");
    int32_t outputWindowWidth = glConfig.vidWidth;
    int32_t outputWindowHeight = glConfig.vidHeight;
    if (GLW_CreateWindow(
            driverName, outputWindowWidth, outputWindowHeight,
            colorBits, fullscreen) == qfalse) {
        return R_MODE_SET_INVALID;
    }

    int32_t outputDrawableWidth;
    int32_t outputDrawableHeight;
    CoduoSDL_GetFramebufferSize(
        &outputDrawableWidth, &outputDrawableHeight);
    glConfig.vidWidth = outputDrawableWidth;
    glConfig.vidHeight = outputDrawableHeight;
    glConfig.windowAspect =
        (float)glConfig.vidWidth / (float)glConfig.vidHeight;
    glConfig.displayFrequency = refreshRate;
    glConfig.isFullscreen = fullscreen;
    return R_MODE_SET_SUCCESS;
#endif
}

/* Source: CoDUOMP.exe 0x004f4c50..0x004f4cea.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4c50_004f4ceb.mcode.
 * Role name: parses the major and minor components of glConfig.versionString
 * and performs the lexicographic minimum-version comparison used throughout
 * GLW_InitExtensions. */
qboolean GLW_CheckOpenGLVersion(int32_t requiredMajor,
                                int32_t requiredMinor)
{
    char version[MAX_STRING_CHARS];
    const char *minorText;
    int32_t actualMajor;
    int32_t actualMinor;

    strncpy(version, glConfig.versionString, sizeof(version));
    version[sizeof(version) - 1] = '\0';

    actualMajor = coduo_crt_atoi(version);
    minorText = strchr(version, '.');
    actualMinor = minorText != NULL ? coduo_crt_atoi(minorText + 1) : 0;

    if (actualMajor < requiredMajor)
        return qfalse;
    if (actualMajor == requiredMajor &&
        actualMinor < requiredMinor) {
        return qfalse;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004f4cf0..0x004f4d6b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4cf0_004f4d6c.mcode.
 * Role name: finds a complete whitespace-delimited extension name rather
 * than accepting a substring of another extension. */
qboolean GLW_HasExtension(const char *extensionString,
                          const char *extensionName)
{
    const size_t extensionLength = strlen(extensionName);
    const char *match = strstr(extensionString, extensionName);

    while (match != NULL) {
        const qboolean startsWord =
            match == extensionString ||
            coduo_crt_isspace((unsigned char)match[-1]) != 0;
        const char trailing = match[extensionLength];
        const qboolean endsWord =
            trailing == '\0' ||
            coduo_crt_isspace((unsigned char)trailing) != 0;

        if (startsWord && endsWord)
            return qtrue;
        match = strstr(match + 1, extensionName);
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004f4d70..0x004f4d87, recovered from an exporter
 * function-boundary gap. The exact source name was not retained;
 * GLW_MissingDriverFeatureError describes the proved localized fatal-error
 * wrapper. GLimp_Extensions also contains inlined copies of this operation. */
void GLW_MissingDriverFeatureError(const char *featureErrorKey)
{
    ri.Error(
        ERR_FATAL,
        va("%s\x15\n\n\x14" "EXE_ERR_GET_NEWEST_DRIVERS",
           featureErrorKey));
}

/* Source: CoDUOMP.exe 0x004f4d90..0x004f4df9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4d90_004f4dfa.mcode.
 * Role name: the callers use this common fatal path when a required OpenGL
 * version or extension is absent. The control bytes and localization keys
 * are passed through Com_Error's localization expansion unchanged. */
void GLW_MissingFeatureError(void)
{
    ri.Printf(R_PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendorString);
    ri.Printf(R_PRINT_ALL, "GL_RENDERER: %s\n",
              glConfig.rendererString);
    ri.Printf(R_PRINT_ALL, "GL_VERSION: %s\n", glConfig.versionString);
    ri.Printf(R_PRINT_ALL, "GL_EXTENSIONS: %s\n",
              glConfig.extensionsString);
    ri.Error(
        ERR_FATAL,
        va("%s\x15\n\n\x14" "EXE_ERR_GET_NEWEST_DRIVERS",
           "EXE_ERR_VIDEOCARD_MISSING_FEATURE"));
}

/* Source: CoDUOMP.exe 0x004f5320..0x004f652c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f5320_004f652d.mcode.
 * Name: exact same-module Mac symbol GLimp_Extensions. The Win32 routine
 * selects core entry points where the reported OpenGL version permits them,
 * otherwise selects extension-suffixed entry points, and installs each
 * resolved address in both the driver and public dispatch slots. */
void GLimp_Extensions(void)
{
    int32_t maxGeneralCombiners;

    /* These public slots are the subset explicitly cleared at entry by the
     * original routine. Their driver slots remain owned by QGL lifecycle. */
    qglActiveTextureARB = NULL;
    qglClientActiveTextureARB = NULL;
    qglLockArraysEXT = NULL;
    qglUnlockArraysEXT = NULL;
    qglPNTrianglesiATI = NULL;
    qglPNTrianglesfATI = NULL;
    qglNewObjectBufferATI = NULL;
    qglIsObjectBufferATI = NULL;
    qglUpdateObjectBufferATI = NULL;
    qglGetObjectBufferfvATI = NULL;
    qglGetObjectBufferivATI = NULL;
    qglFreeObjectBufferATI = NULL;
    qglArrayObjectATI = NULL;
    qglGetArrayObjectfvATI = NULL;
    qglGetArrayObjectivATI = NULL;
    qglVariantArrayObjectATI = NULL;
    qglGetVariantArrayObjectfvATI = NULL;
    qglGetVariantArrayObjectivATI = NULL;

    glConfig.textureFilterAnisotropicAvailable = qfalse;
    glConfig.fogDistanceAvailable = qfalse;
    glConfig.NVFogMode = GL_NONE;

    ri.Printf(R_PRINT_ALL, "Initializing OpenGL extensions\n");

    /* The two stores following every GetProcAddress call target the typed
     * driver slot and its public dispatch peer. */
#define GLIMP_LOAD_GL(type_, name_, symbol_) do { \
    rendererGl##name_##Driver = \
        (type_)qwglGetProcAddress((symbol_)); \
    qgl##name_ = rendererGl##name_##Driver; \
} while (0)
#define GLIMP_LOAD_WGL(type_, name_, symbol_) do { \
    rendererWgl##name_##Driver = \
        (type_)qwglGetProcAddress((symbol_)); \
    qwgl##name_ = rendererWgl##name_##Driver; \
} while (0)

    if (GLW_CheckOpenGLVersion(1, 3) != qfalse) {
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_image_3d_func_t,
                      CompressedTexImage3DARB,
                      "glCompressedTexImage3D");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_image_2d_func_t,
                      CompressedTexImage2DARB,
                      "glCompressedTexImage2D");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_image_1d_func_t,
                      CompressedTexImage1DARB,
                      "glCompressedTexImage1D");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_sub_image_3d_func_t,
                      CompressedTexSubImage3DARB,
                      "glCompressedTexSubImage3D");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_sub_image_2d_func_t,
                      CompressedTexSubImage2DARB,
                      "glCompressedTexSubImage2D");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_sub_image_1d_func_t,
                      CompressedTexSubImage1DARB,
                      "glCompressedTexSubImage1D");
        GLIMP_LOAD_GL(renderer_gl_get_compressed_tex_image_func_t,
                      GetCompressedTexImageARB,
                      "glGetCompressedTexImage");
    } else {
        if (GLW_HasExtension(glConfig.extensionsString,
                             "GL_ARB_texture_compression") == qfalse) {
            GLW_MissingFeatureError();
        }
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_image_3d_func_t,
                      CompressedTexImage3DARB,
                      "glCompressedTexImage3DARB");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_image_2d_func_t,
                      CompressedTexImage2DARB,
                      "glCompressedTexImage2DARB");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_image_1d_func_t,
                      CompressedTexImage1DARB,
                      "glCompressedTexImage1DARB");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_sub_image_3d_func_t,
                      CompressedTexSubImage3DARB,
                      "glCompressedTexSubImage3DARB");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_sub_image_2d_func_t,
                      CompressedTexSubImage2DARB,
                      "glCompressedTexSubImage2DARB");
        GLIMP_LOAD_GL(renderer_gl_compressed_tex_sub_image_1d_func_t,
                      CompressedTexSubImage1DARB,
                      "glCompressedTexSubImage1DARB");
        GLIMP_LOAD_GL(renderer_gl_get_compressed_tex_image_func_t,
                      GetCompressedTexImageARB,
                      "glGetCompressedTexImageARB");
    }
    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_EXT_texture_compression_s3tc") == qfalse) {
        GLW_MissingFeatureError();
    }

    qglDrawRangeElementsEXT = NULL;
    if (GLW_CheckOpenGLVersion(1, 2) != qfalse) {
        if (r_ext_draw_range_elements->integer != 0) {
            GLIMP_LOAD_GL(renderer_gl_draw_range_elements_func_t,
                          DrawRangeElementsEXT, "glDrawRangeElements");
            ri.Printf(R_PRINT_ALL,
                      "...using OpenGL 1.2 draw element range\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring OpenGL 1.2 draw element range\n");
        }
    } else if (GLW_HasExtension(glConfig.extensionsString,
                                "GL_EXT_draw_range_elements") != qfalse) {
        if (r_ext_draw_range_elements->integer != 0) {
            GLIMP_LOAD_GL(renderer_gl_draw_range_elements_func_t,
                          DrawRangeElementsEXT,
                          "glDrawRangeElementsEXT");
            ri.Printf(R_PRINT_ALL,
                      "...using GL_EXT_draw_range_elements\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_EXT_draw_range_elements\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_EXT_draw_range_elements not found\n");
    }

    glConfig.textureEnvAddAvailable = qfalse;
    if (GLW_CheckOpenGLVersion(1, 3) != qfalse) {
        if (r_arb_texture_env_add->integer != 0) {
            glConfig.textureEnvAddAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using OpenGL 1.3 texture add environment mode\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring OpenGL 1.3 texture add environment mode\n");
        }
    } else if (GLW_HasExtension(glConfig.extensionsString,
                                "GL_EXT_texture_env_add") != qfalse) {
        if (r_arb_texture_env_add->integer != 0) {
            glConfig.textureEnvAddAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_EXT_texture_env_add\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_EXT_texture_env_add\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_EXT_texture_env_add not found\n");
    }

    glConfig.textureEnvCombineAvailable = qfalse;
    if (GLW_CheckOpenGLVersion(1, 3) != qfalse) {
        if (r_arb_texture_env_combine->integer != 0) {
            glConfig.textureEnvCombineAvailable = qtrue;
            ri.Printf(
                R_PRINT_ALL,
                "...using OpenGL 1.3 texture combine environment mode\n");
        } else {
            ri.Printf(
                R_PRINT_ALL,
                "...ignoring OpenGL 1.3 texture combine environment mode\n");
        }
    } else if (GLW_HasExtension(glConfig.extensionsString,
                                "GL_ARB_texture_env_combine") != qfalse) {
        if (r_arb_texture_env_combine->integer != 0) {
            glConfig.textureEnvCombineAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_ARB_texture_env_combine\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_ARB_texture_env_combine\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_ARB_texture_env_combine not found\n");
    }

    glConfig.textureEnvDot3Available = qfalse;
    if (GLW_CheckOpenGLVersion(1, 3) != qfalse) {
        if (r_arb_texture_env_dot3->integer != 0) {
            glConfig.textureEnvDot3Available = qtrue;
            ri.Printf(
                R_PRINT_ALL,
                "...using OpenGL 1.3 texture dot3 environment mode\n");
        } else {
            ri.Printf(
                R_PRINT_ALL,
                "...ignoring OpenGL 1.3 texture dot3 environment mode\n");
        }
    } else if (GLW_HasExtension(glConfig.extensionsString,
                                "GL_ARB_texture_env_dot3") != qfalse) {
        if (r_arb_texture_env_dot3->integer != 0 &&
            glConfig.textureEnvCombineAvailable != qfalse) {
            glConfig.textureEnvDot3Available = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_ARB_texture_env_dot3\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_ARB_texture_env_dot3\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_ARB_texture_env_dot3 not found\n");
    }

    glConfig.cubeMapAvailable = qfalse;
    if (GLW_CheckOpenGLVersion(1, 3) != qfalse) {
        if (r_arb_texture_cube_map->integer != 0) {
            glConfig.cubeMapAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using OpenGL 1.3 cube map textures\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring OpenGL 1.3 cube map textures\n");
        }
    } else if (GLW_HasExtension(glConfig.extensionsString,
                                "GL_ARB_texture_cube_map") != qfalse) {
        if (r_arb_texture_cube_map->integer != 0) {
            glConfig.cubeMapAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_ARB_texture_cube_map\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_ARB_texture_cube_map\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_ARB_texture_cube_map not found\n");
    }

    glConfig.vertexProgramAvailable = qfalse;
    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_ARB_vertex_program") != qfalse) {
        if (r_arb_vertex_program->integer != 0) {
            glConfig.vertexProgramAvailable = qtrue;
            QGL_LoadARBVertexProgramFunctions();
            ri.Printf(R_PRINT_ALL,
                      "...using GL_ARB_vertex_program\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_ARB_vertex_program\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_ARB_vertex_program not found\n");
    }

    glConfig.textureShaderNVAvailable = qfalse;
    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_NV_texture_shader") != qfalse) {
        if (r_nv_texture_shader->integer != 0) {
            glConfig.textureShaderNVAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_NV_texture_shader\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_NV_texture_shader\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_NV_texture_shader not found\n");
    }

    glConfig.registerCombinerMode = R_REGISTER_COMBINERS_UNAVAILABLE;
    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_NV_register_combiners") != qfalse) {
        if (r_nv_register_combiners->integer != 0) {
            glConfig.registerCombinerMode = R_REGISTER_COMBINERS_NV;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_NV_register_combiners\n");
            GLIMP_LOAD_GL(renderer_gl_combiner_parameterfv_nv_func_t,
                          CombinerParameterfvNV,
                          "glCombinerParameterfvNV");
            GLIMP_LOAD_GL(renderer_gl_combiner_parameterf_nv_func_t,
                          CombinerParameterfNV,
                          "glCombinerParameterfNV");
            GLIMP_LOAD_GL(renderer_gl_combiner_parameteriv_nv_func_t,
                          CombinerParameterivNV,
                          "glCombinerParameterivNV");
            GLIMP_LOAD_GL(renderer_gl_combiner_parameteri_nv_func_t,
                          CombinerParameteriNV,
                          "glCombinerParameteriNV");
            GLIMP_LOAD_GL(renderer_gl_combiner_input_nv_func_t,
                          CombinerInputNV, "glCombinerInputNV");
            GLIMP_LOAD_GL(renderer_gl_combiner_output_nv_func_t,
                          CombinerOutputNV, "glCombinerOutputNV");
            GLIMP_LOAD_GL(renderer_gl_final_combiner_input_nv_func_t,
                          FinalCombinerInputNV,
                          "glFinalCombinerInputNV");
            GLIMP_LOAD_GL(
                renderer_gl_get_combiner_input_parameterfv_nv_func_t,
                GetCombinerInputParameterfvNV,
                "glGetCombinerInputParameterfvNV");
            GLIMP_LOAD_GL(
                renderer_gl_get_combiner_input_parameteriv_nv_func_t,
                GetCombinerInputParameterivNV,
                "glGetCombinerInputParameterivNV");
            GLIMP_LOAD_GL(
                renderer_gl_get_combiner_output_parameterfv_nv_func_t,
                GetCombinerOutputParameterfvNV,
                "glGetCombinerOutputParameterfvNV");
            GLIMP_LOAD_GL(
                renderer_gl_get_combiner_output_parameteriv_nv_func_t,
                GetCombinerOutputParameterivNV,
                "glGetCombinerOutputParameterivNV");
            GLIMP_LOAD_GL(
                renderer_gl_get_final_combiner_input_parameterfv_nv_func_t,
                GetFinalCombinerInputParameterfvNV,
                "glGetFinalCombinerInputParameterfvNV");
            GLIMP_LOAD_GL(
                renderer_gl_get_final_combiner_input_parameteriv_nv_func_t,
                GetFinalCombinerInputParameterivNV,
                "glGetFinalCombinerInputParameterivNV");

            if (GLW_HasExtension(
                    glConfig.extensionsString,
                    "GL_NV_register_combiners2") != qfalse) {
                qglGetIntegerv(GL_MAX_GENERAL_COMBINERS_NV,
                               &maxGeneralCombiners);
                if (maxGeneralCombiners < 8) {
                    ri.Printf(
                        R_PRINT_ALL,
                        "...ignoring GL_NV_register_combiners2 because "
                        "GL_MAX_GENERAL_COMBINERS_NV is %i < 8\n",
                        maxGeneralCombiners);
                } else if (r_nv_register_combiners->integer >= 2) {
                    glConfig.registerCombinerMode =
                        R_REGISTER_COMBINERS_NV2;
                    ri.Printf(R_PRINT_ALL,
                              "...using GL_NV_register_combiners2\n");
                    GLIMP_LOAD_GL(
                        renderer_gl_combiner_stage_parameterfv_nv_func_t,
                        CombinerStageParameterfvNV,
                        "glCombinerStageParameterfvNV");
                    GLIMP_LOAD_GL(
                        renderer_gl_get_combiner_stage_parameterfv_nv_func_t,
                        GetCombinerStageParameterfvNV,
                        "glGetCombinerStageParameterfvNV");
                } else {
                    ri.Printf(
                        R_PRINT_ALL,
                        "...ignoring GL_NV_register_combiners2\n");
                }
            } else {
                ri.Printf(R_PRINT_ALL,
                          "...GL_NV_register_combiners2 not found\n");
            }
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_NV_register_combiners\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_NV_register_combiners not found\n");
    }

#if defined(_WIN32)
    GLIMP_LOAD_WGL(renderer_wgl_swap_interval_ext_func_t,
                   SwapIntervalEXT, "wglSwapIntervalEXT");
    if (qwglSwapIntervalEXT != NULL) {
        ri.Printf(R_PRINT_ALL,
                  "...using WGL_EXT_swap_control\n");
        r_swapInterval->modified = qtrue;
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...WGL_EXT_swap_control not found\n");
    }
#else
    /* NOT_FROM_ORIGINAL_SOURCE: SDL applies the native context swap interval
     * directly. Do not ask GLX for a WGL symbol: some implementations return
     * a non-null dispatch stub for unsupported names. */
    rendererWglSwapIntervalEXTDriver = NULL;
    qwglSwapIntervalEXT = NULL;
    ri.Printf(R_PRINT_ALL, "...using native swap control\n");
    r_swapInterval->modified = qtrue;
#endif

    if (GLW_CheckOpenGLVersion(1, 3) != qfalse) {
        GLIMP_LOAD_GL(renderer_gl_active_texture_arb_func_t,
                      ActiveTextureARB, "glActiveTexture");
        GLIMP_LOAD_GL(renderer_gl_active_texture_arb_func_t,
                      ClientActiveTextureARB, "glClientActiveTexture");
        if (qglActiveTextureARB == NULL ||
            qglClientActiveTextureARB == NULL) {
            ri.Error(
                ERR_FATAL,
                va("%s\x15\n\n\x14" "EXE_ERR_GET_NEWEST_DRIVERS",
                   "EXE_ERR_MULTITEX_INIT_FAIL"));
        }

        qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB,
                       &glConfig.maxActiveTextures);
        if (glConfig.maxActiveTextures <= 1) {
            ri.Error(
                ERR_FATAL,
                va("%s\x15\n\n\x14" "EXE_ERR_GET_NEWEST_DRIVERS",
                   "EXE_ERR_MULTITEX_BAD_MAX"));
        }
        if (glConfig.maxActiveTextures > R_MAX_TEXTURE_UNITS)
            glConfig.maxActiveTextures = R_MAX_TEXTURE_UNITS;
        if (r_maxActiveTextures->integer >= 2 &&
            glConfig.maxActiveTextures > r_maxActiveTextures->integer) {
            glConfig.maxActiveTextures = r_maxActiveTextures->integer;
        }
        ri.Printf(R_PRINT_ALL,
                  "...using OpenGL 1.3 multitexture with %i max textures\n",
                  glConfig.maxActiveTextures);
    } else if (GLW_HasExtension(glConfig.extensionsString,
                                "GL_ARB_multitexture") != qfalse) {
        GLIMP_LOAD_GL(renderer_gl_active_texture_arb_func_t,
                      ActiveTextureARB, "glActiveTextureARB");
        GLIMP_LOAD_GL(renderer_gl_active_texture_arb_func_t,
                      ClientActiveTextureARB,
                      "glClientActiveTextureARB");
        if (qglActiveTextureARB == NULL ||
            qglClientActiveTextureARB == NULL) {
            ri.Error(
                ERR_FATAL,
                va("%s\x15\n\n\x14" "EXE_ERR_GET_NEWEST_DRIVERS",
                   "EXE_ERR_ARB_MULTITEX_INIT_FAILED"));
        }

        qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB,
                       &glConfig.maxActiveTextures);
        if (glConfig.maxActiveTextures <= 1) {
            ri.Error(
                ERR_FATAL,
                va("%s\x15\n\n\x14" "EXE_ERR_GET_NEWEST_DRIVERS",
                   "EXE_ERR_ARB_MULTITEX_BAD_MAX"));
        }
        if (glConfig.maxActiveTextures > R_MAX_TEXTURE_UNITS)
            glConfig.maxActiveTextures = R_MAX_TEXTURE_UNITS;
        if (r_maxActiveTextures->integer >= 2 &&
            glConfig.maxActiveTextures > r_maxActiveTextures->integer) {
            glConfig.maxActiveTextures = r_maxActiveTextures->integer;
        }
        ri.Printf(R_PRINT_ALL,
                  "...using GL_ARB_multitexture with %i max textures\n",
                  glConfig.maxActiveTextures);
    } else {
        GLW_MissingFeatureError();
    }

    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_EXT_compiled_vertex_array") != qfalse) {
        if (r_ext_compiled_vertex_array->integer != 0) {
            ri.Printf(R_PRINT_ALL,
                      "...using GL_EXT_compiled_vertex_array\n");
            GLIMP_LOAD_GL(renderer_gl_lock_arrays_ext_func_t,
                          LockArraysEXT, "glLockArraysEXT");
            GLIMP_LOAD_GL(renderer_gl_void_func_t,
                          UnlockArraysEXT, "glUnlockArraysEXT");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_EXT_compiled_vertex_array\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_EXT_compiled_vertex_array not found\n");
    }

    glConfig.rescaleNormalAvailable = qfalse;
    if (GLW_CheckOpenGLVersion(1, 2) != qfalse) {
        if (r_ext_rescale_normal->integer != 0) {
            glConfig.rescaleNormalAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using OpenGL 1.2 normal rescaling\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring OpenGL 1.2 normal rescaling\n");
        }
    } else if (GLW_HasExtension(glConfig.extensionsString,
                                "GL_EXT_rescale_normal") != qfalse) {
        if (r_ext_rescale_normal->integer != 0) {
            ri.Printf(R_PRINT_ALL,
                      "...using GL_EXT_rescale_normal\n");
            glConfig.rescaleNormalAvailable = qtrue;
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_EXT_rescale_normal\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_EXT_rescale_normal not found\n");
    }

    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_ATI_pn_triangles") != qfalse) {
        if (r_ati_pntriangles->integer != 0) {
            ri.Printf(R_PRINT_ALL,
                      "...using GL_ATI_pn_triangles\n");
            GLIMP_LOAD_GL(renderer_gl_pn_trianglesi_ati_func_t,
                          PNTrianglesiATI, "glPNTrianglesiATI");
            GLIMP_LOAD_GL(renderer_gl_pn_trianglesf_ati_func_t,
                          PNTrianglesfATI, "glPNTrianglesfATI");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_ATI_pn_triangles\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_ATI_pn_triangles not found\n");
        ri.Cvar_Set("r_ati_pntriangles", "0");
    }

    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_ARB_vertex_buffer_object") != qfalse) {
        if (r_arb_vertex_buffer_object->integer != 0) {
            GLIMP_LOAD_GL(renderer_gl_bind_buffer_arb_func_t,
                          BindBufferARB, "glBindBufferARB");
            GLIMP_LOAD_GL(renderer_gl_delete_buffers_arb_func_t,
                          DeleteBuffersARB, "glDeleteBuffersARB");
            GLIMP_LOAD_GL(renderer_gl_gen_buffers_arb_func_t,
                          GenBuffersARB, "glGenBuffersARB");
            GLIMP_LOAD_GL(renderer_gl_is_buffer_arb_func_t,
                          IsBufferARB, "glIsBufferARB");
            GLIMP_LOAD_GL(renderer_gl_buffer_data_arb_func_t,
                          BufferDataARB, "glBufferDataARB");
            GLIMP_LOAD_GL(renderer_gl_buffer_sub_data_arb_func_t,
                          BufferSubDataARB, "glBufferSubDataARB");
            GLIMP_LOAD_GL(renderer_gl_get_buffer_sub_data_arb_func_t,
                          GetBufferSubDataARB,
                          "glGetBufferSubDataARB");
            GLIMP_LOAD_GL(renderer_gl_map_buffer_arb_func_t,
                          MapBufferARB, "glMapBufferARB");
            GLIMP_LOAD_GL(renderer_gl_unmap_buffer_arb_func_t,
                          UnmapBufferARB, "glUnmapBufferARB");
            GLIMP_LOAD_GL(renderer_gl_get_buffer_parameteriv_arb_func_t,
                          GetBufferParameterivARB,
                          "glGetBufferParameterivARB");
            GLIMP_LOAD_GL(renderer_gl_get_buffer_pointerv_arb_func_t,
                          GetBufferPointervARB,
                          "glGetBufferPointervARB");
            glConfig.vertexBufferObjectAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_ARB_vertex_buffer_object\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_ARB_vertex_buffer_object\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_ARB_vertex_buffer_object not found\n");
    }

    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_NV_fog_distance") != qfalse) {
        if (r_nv_fog_dist->integer != 0) {
            glConfig.fogDistanceAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_NV_fog_distance\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_NV_fog_distance\n");
            qglFogi(GL_FOG_DISTANCE_MODE_NV,
                    GL_EYE_PLANE_ABSOLUTE_NV);
        }
        ri.Cvar_Set("r_nv_fog_available", "1");
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_NV_fog_distance not found\n");
        ri.Cvar_Set("r_nv_fog_dist", "0");
        ri.Cvar_Set("r_nv_fog_available", "0");
    }

    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_NV_vertex_array_range2") == qfalse &&
        GLW_HasExtension(glConfig.extensionsString,
                         "GL_NV_vertex_array_range") == qfalse) {
        ri.Printf(R_PRINT_ALL,
                  "...GL_NV_vertex_array_range not found\n");
    } else if (r_nv_vertex_array_range->integer == 0 ||
               glConfig.vertexBufferObjectAvailable != qfalse) {
        ri.Printf(R_PRINT_ALL,
                  "...ignoring GL_NV_vertex_array_range\n");
    } else {
        GLIMP_LOAD_GL(renderer_gl_void_func_t,
                      FlushVertexArrayRangeNV,
                      "glFlushVertexArrayRangeNV");
        GLIMP_LOAD_GL(renderer_gl_vertex_array_range_nv_func_t,
                      VertexArrayRangeNV, "glVertexArrayRangeNV");
        GLIMP_LOAD_GL(renderer_gl_allocate_memory_nv_func_t,
                      AllocateMemoryNV, "wglAllocateMemoryNV");
        GLIMP_LOAD_GL(renderer_gl_free_memory_nv_func_t,
                      FreeMemoryNV, "wglFreeMemoryNV");
        glConfig.vertexArrayRangeMode = R_VERTEX_ARRAY_RANGE_NV;
        ri.Printf(R_PRINT_ALL,
                  "...using GL_NV_vertex_array_range\n");

        if (GLW_HasExtension(glConfig.extensionsString,
                             "GL_NV_vertex_array_range2") != qfalse) {
            if (r_nv_vertex_array_range->integer == 2) {
                glConfig.vertexArrayRangeMode =
                    R_VERTEX_ARRAY_RANGE_NV2;
                ri.Printf(R_PRINT_ALL,
                          "...using GL_NV_vertex_array_range2\n");
            } else {
                ri.Printf(R_PRINT_ALL,
                          "...ignoring GL_NV_vertex_array_range2\n");
            }
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...GL_NV_vertex_array_range2 not found\n");
        }
    }

    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_NV_fence") != qfalse) {
        if (r_nv_fence->integer != 0 &&
            glConfig.vertexBufferObjectAvailable == qfalse) {
            GLIMP_LOAD_GL(renderer_gl_delete_fences_nv_func_t,
                          DeleteFencesNV, "glDeleteFencesNV");
            GLIMP_LOAD_GL(renderer_gl_gen_fences_nv_func_t,
                          GenFencesNV, "glGenFencesNV");
            GLIMP_LOAD_GL(renderer_gl_fence_test_nv_func_t,
                          IsFenceNV, "glIsFenceNV");
            GLIMP_LOAD_GL(renderer_gl_fence_test_nv_func_t,
                          TestFenceNV, "glTestFenceNV");
            GLIMP_LOAD_GL(renderer_gl_get_fenceiv_nv_func_t,
                          GetFenceivNV, "glGetFenceivNV");
            GLIMP_LOAD_GL(renderer_gl_finish_fence_nv_func_t,
                          FinishFenceNV, "glFinishFenceNV");
            GLIMP_LOAD_GL(renderer_gl_set_fence_nv_func_t,
                          SetFenceNV, "glSetFenceNV");
            glConfig.fenceNVAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_NV_fence\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_NV_fence\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_NV_fence not found\n");
    }

    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_ATI_vertex_array_object") != qfalse) {
        if (r_ati_vertex_array_object->integer != 0 &&
            glConfig.vertexBufferObjectAvailable == qfalse &&
            glConfig.vertexArrayRangeMode ==
                R_VERTEX_ARRAY_RANGE_NONE) {
            GLIMP_LOAD_GL(renderer_gl_new_object_buffer_ati_func_t,
                          NewObjectBufferATI,
                          "glNewObjectBufferATI");
            GLIMP_LOAD_GL(renderer_gl_is_object_buffer_ati_func_t,
                          IsObjectBufferATI,
                          "glIsObjectBufferATI");
            GLIMP_LOAD_GL(renderer_gl_update_object_buffer_ati_func_t,
                          UpdateObjectBufferATI,
                          "glUpdateObjectBufferATI");
            GLIMP_LOAD_GL(renderer_gl_get_object_bufferfv_ati_func_t,
                          GetObjectBufferfvATI,
                          "glGetObjectBufferfvATI");
            GLIMP_LOAD_GL(renderer_gl_get_object_bufferiv_ati_func_t,
                          GetObjectBufferivATI,
                          "glGetObjectBufferivATI");
            GLIMP_LOAD_GL(renderer_gl_free_object_buffer_ati_func_t,
                          FreeObjectBufferATI,
                          "glFreeObjectBufferATI");
            GLIMP_LOAD_GL(renderer_gl_array_object_ati_func_t,
                          ArrayObjectATI, "glArrayObjectATI");
            GLIMP_LOAD_GL(renderer_gl_get_array_objectfv_ati_func_t,
                          GetArrayObjectfvATI,
                          "glGetArrayObjectfvATI");
            GLIMP_LOAD_GL(renderer_gl_get_array_objectiv_ati_func_t,
                          GetArrayObjectivATI,
                          "glGetArrayObjectivATI");
            GLIMP_LOAD_GL(renderer_gl_variant_array_object_ati_func_t,
                          VariantArrayObjectATI,
                          "glVariantArrayObjectATI");
            GLIMP_LOAD_GL(
                renderer_gl_get_variant_array_objectfv_ati_func_t,
                GetVariantArrayObjectfvATI,
                "glGetVariantArrayObjectfvATI");
            GLIMP_LOAD_GL(
                renderer_gl_get_variant_array_objectiv_ati_func_t,
                GetVariantArrayObjectivATI,
                "glGetVariantArrayObjectivATI");
            glConfig.vertexArrayObjectATIAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_ATI_vertex_array_object\n");

            if (GLW_HasExtension(glConfig.extensionsString,
                                 "GL_ATI_element_array") != qfalse) {
                if (r_ati_element_array->integer != 0) {
                    GLIMP_LOAD_GL(
                        renderer_gl_element_pointer_ati_func_t,
                        ElementPointerATI, "glElementPointerATI");
                    GLIMP_LOAD_GL(
                        renderer_gl_draw_element_array_ati_func_t,
                        DrawElementArrayATI,
                        "glDrawElementArrayATI");
                    GLIMP_LOAD_GL(
                        renderer_gl_draw_range_element_array_ati_func_t,
                        DrawRangeElementArrayATI,
                        "glDrawRangeElementArrayATI");
                    glConfig.elementArrayATIAvailable = qtrue;
                    ri.Printf(R_PRINT_ALL,
                              "...using GL_ATI_element_array\n");
                } else {
                    ri.Printf(R_PRINT_ALL,
                              "...ignoring GL_ATI_element_array\n");
                }
            } else {
                ri.Printf(R_PRINT_ALL,
                          "...GL_ATI_element_array not found\n");
            }
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_ATI_vertex_array_object\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_ATI_vertex_array_object not found\n");
    }

    glConfig.fragmentShaderATIAvailable = qfalse;
    if (GLW_HasExtension(glConfig.extensionsString,
                         "GL_ATI_fragment_shader") != qfalse) {
        if (r_ati_fragment_shader->integer != 0) {
            GLIMP_LOAD_GL(renderer_gl_gen_fragment_shaders_ati_func_t,
                          GenFragmentShadersATI,
                          "glGenFragmentShadersATI");
            GLIMP_LOAD_GL(renderer_gl_capability_func_t,
                          DeleteFragmentShaderATI,
                          "glDeleteFragmentShaderATI");
            GLIMP_LOAD_GL(renderer_gl_bind_fragment_shader_ati_func_t,
                          BindFragmentShaderATI,
                          "glBindFragmentShaderATI");
            GLIMP_LOAD_GL(renderer_gl_void_func_t,
                          BeginFragmentShaderATI,
                          "glBeginFragmentShaderATI");
            GLIMP_LOAD_GL(renderer_gl_void_func_t,
                          EndFragmentShaderATI,
                          "glEndFragmentShaderATI");
            GLIMP_LOAD_GL(renderer_gl_fragment_shader_texcoord_ati_func_t,
                          PassTexCoordATI, "glPassTexCoordATI");
            GLIMP_LOAD_GL(renderer_gl_fragment_shader_texcoord_ati_func_t,
                          SampleMapATI, "glSampleMapATI");
            GLIMP_LOAD_GL(renderer_gl_color_fragment_op1_ati_func_t,
                          ColorFragmentOp1ATI,
                          "glColorFragmentOp1ATI");
            GLIMP_LOAD_GL(renderer_gl_color_fragment_op2_ati_func_t,
                          ColorFragmentOp2ATI,
                          "glColorFragmentOp2ATI");
            GLIMP_LOAD_GL(renderer_gl_color_fragment_op3_ati_func_t,
                          ColorFragmentOp3ATI,
                          "glColorFragmentOp3ATI");
            GLIMP_LOAD_GL(renderer_gl_alpha_fragment_op1_ati_func_t,
                          AlphaFragmentOp1ATI,
                          "glAlphaFragmentOp1ATI");
            GLIMP_LOAD_GL(renderer_gl_alpha_fragment_op2_ati_func_t,
                          AlphaFragmentOp2ATI,
                          "glAlphaFragmentOp2ATI");
            GLIMP_LOAD_GL(renderer_gl_alpha_fragment_op3_ati_func_t,
                          AlphaFragmentOp3ATI,
                          "glAlphaFragmentOp3ATI");
            GLIMP_LOAD_GL(
                renderer_gl_set_fragment_shader_constant_ati_func_t,
                SetFragmentShaderConstantATI,
                "glSetFragmentShaderConstantATI");
            glConfig.fragmentShaderATIAvailable = qtrue;
            ri.Printf(R_PRINT_ALL,
                      "...using GL_ATI_fragment_shader\n");
        } else {
            ri.Printf(R_PRINT_ALL,
                      "...ignoring GL_ATI_fragment_shader\n");
        }
    } else {
        ri.Printf(R_PRINT_ALL,
                  "...GL_ATI_fragment_shader not found\n");
    }

    /* The shipped executable intentionally forces this extension off even
     * when advertised; it never queries the maximum anisotropy. */
    if (GLW_HasExtension(
            glConfig.extensionsString,
            "GL_EXT_texture_filter_anisotropic") != qfalse) {
        (void)r_ext_texture_filter_anisotropic->integer;
        ri.Printf(
            R_PRINT_ALL,
            "...ignoring GL_EXT_texture_filter_anisotropic\n");
        glConfig.textureFilterAnisotropicAvailable = qfalse;
    }
    ri.Cvar_Set("r_ext_texture_filter_anisotropic", "0");

#undef GLIMP_LOAD_WGL
#undef GLIMP_LOAD_GL
}

/* Source: CoDUOMP.exe 0x004f6530..0x004f65ca.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f6530_004f65cb.mcode.
 * Name: proven by the original failure diagnostic. The accepted Windows
 * versions are NT 5.0 or newer, or the Win9x line beginning with 4.10. */
qboolean GLW_CheckOSVersion(void)
{
#if defined(_WIN32)
    OSVERSIONINFOA version = {0};
    version.dwOSVersionInfoSize = sizeof(version);
    if (GetVersionExA(&version) == FALSE) {
        ri.Printf(
            R_PRINT_ALL,
            "GLW_CheckOSVersion() - GetVersionEx failed\n");
        return qfalse;
    }

    if (version.dwMajorVersion >= 5)
        return qtrue;
    return version.dwMajorVersion == 4 &&
           version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
           version.dwMinorVersion >= 10;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: this probe describes the Win32 host only. */
    return qtrue;
#endif
}

/* Source: CoDUOMP.exe 0x004f65d0..0x004f66b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f65d0_004f66ba.mcode.
 * Role name: initializes the named QGL driver, then applies the current
 * fullscreen, mode, and color-depth cvars. */
qboolean GLW_StartDriverAndSetMode(const char *driverName)
{
    char normalizedDriver[MAX_STRING_CHARS];

    strncpy(normalizedDriver, driverName,
            sizeof(normalizedDriver) - 1);
    normalizedDriver[sizeof(normalizedDriver) - 1] = '\0';
    for (char *character = normalizedDriver;
         *character != '\0'; ++character) {
        *character =
            (char)coduo_crt_tolower((unsigned char)*character);
    }

#if defined(_WIN32)
    if (QGL_Init(normalizedDriver) == qfalse)
        return qfalse;
#endif
    const renderer_mode_set_result_t modeResult =
        GLW_SetMode(driverName, r_mode->integer,
                    r_colorbits->integer,
                    r_fullscreen->integer);
    switch (modeResult) {
    case R_MODE_SET_SUCCESS:
#if !defined(_WIN32)
        if (QGL_Init(normalizedDriver) == qfalse ||
            GLW_InitDriver(r_colorbits->integer) == qfalse) {
            QGL_Shutdown();
            CoduoSDL_DestroyOpenGLWindow();
            return qfalse;
        }
#endif
        return qtrue;
    case R_MODE_SET_FULLSCREEN_UNAVAILABLE:
        ri.Printf(
            R_PRINT_ALL,
            "...WARNING: fullscreen unavailable in this mode\n");
        break;
    case R_MODE_SET_INVALID:
        ri.Printf(
            R_PRINT_ALL,
            "...WARNING: could not set the given mode (%d)\n",
            r_mode->integer);
        break;
    }

#if defined(_WIN32)
    QGL_Shutdown();
#else
    CoduoSDL_DestroyOpenGLWindow();
#endif
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004f67e0..0x004f684a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f67e0_004f684b.mcode.
 * Role name: lowers r_mode to one candidate and retries the stock OpenGL
 * driver. Candidates at or above the current mode are deliberately skipped. */
qboolean GLW_TryFallbackMode(int32_t mode,
                             const char *resolutionName)
{
    char modeText[16];

    if (r_mode->integer <= mode)
        return qfalse;

    (void)snprintf(modeText, sizeof(modeText), "%d", mode);
    ri.Printf(
        R_PRINT_ALL,
        "Forcing %s resolution to allow OpenGL to run in fullscreen\n",
        resolutionName);
    ri.Cvar_Set("r_mode", modeText);
    (void)ri.Cvar_Get("r_mode", "3", CVAR_LATCH);
    return GLW_StartDriverAndSetMode("opengl32");
}

/* Source: CoDUOMP.exe 0x004f6850..0x004f6912.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f6850_004f6913.mcode.
 * Role name: tries the configured display settings, then removes an explicit
 * refresh constraint and walks the original descending resolution fallback
 * list before issuing the fatal load error. */
qboolean GLW_LoadOpenGL(void)
{
    if (GLW_StartDriverAndSetMode("opengl32") != qfalse)
        return qtrue;

    if (r_displayRefresh->integer != 0) {
        ri.Printf(
            R_PRINT_ALL,
            "Forcing default value for r_displayRefresh to allow OpenGL to run in fullscreen\n");
        ri.Cvar_Set("r_displayRefresh", "0");
        (void)ri.Cvar_Get(
            "r_displayRefresh", "0", CVAR_LATCH);
        if (GLW_StartDriverAndSetMode("opengl32") != qfalse)
            return qtrue;
    }

    if (GLW_TryFallbackMode(9, "1600x1200") ||
        GLW_TryFallbackMode(6, "1024x768") ||
        GLW_TryFallbackMode(4, "800x600") ||
        GLW_TryFallbackMode(3, "640x480")) {
        return qtrue;
    }

    ri.Error(ERR_FATAL, "EXE_ERR_COULDNT_LOAD_OPENGL");
}

/* Source: CoDUOMP.exe 0x004f6920..0x004f6c80.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f6920_004f6c81.mcode.
 * Role name: this Windows helper loads gl.csv and applies the row whose
 * renderer column is the longest case-insensitive prefix of GL_RENDERER.
 * A later row replaces the captured values only when its matching prefix is
 * more specific. */
void GLW_ApplyRendererAutoConfig(void)
{
    cvar_t *cvars[R_AUTOCONFIG_MAX_CVARS];
    char values[R_AUTOCONFIG_MAX_CVARS][R_AUTOCONFIG_VALUE_SIZE];
    char *fileBuffer;
    char *parseCursor;
    int32_t cvarCount = 0;
    size_t bestRendererPrefixLength = 0;

    if (r_skip_auto_config->integer != 0)
        return;

    if (ri.FS_ReadFile("gl.csv", (void **)&fileBuffer) < 0)
        return;

    Com_BeginParseSession("gl.csv");
    Com_SetCSV(qtrue);
    parseCursor = fileBuffer;

    char *token = Com_Parse(&parseCursor);
    if (token[0] == '\0' || Q_stricmp(token, "renderer") != 0) {
        ri.Error(
            ERR_FATAL,
            "first column of gl.csv must be called 'renderer'");
    }

    while ((token = Com_ParseOnLine(&parseCursor))[0] != '\0') {
        if (cvarCount >= R_AUTOCONFIG_MAX_CVARS) {
            ri.Error(ERR_FATAL, "more than %i cvars in gl.csv",
                     R_AUTOCONFIG_MAX_CVARS);
        }

        cvars[cvarCount] = ri.Cvar_FindVar(token);
        if (cvars[cvarCount] == NULL) {
            ri.Error(ERR_FATAL,
                     "cvar %s mentioned in gl.csv does not exist", token);
        }
        ++cvarCount;
    }

    while ((token = Com_Parse(&parseCursor))[0] != '\0') {
        const size_t rendererPrefixLength = strlen(token);
        if (rendererPrefixLength > bestRendererPrefixLength &&
            Q_stricmpn(glConfig.rendererString, token,
                       (int32_t)rendererPrefixLength) == 0) {
            bestRendererPrefixLength = rendererPrefixLength;

            for (int32_t cvarIndex = 0; cvarIndex < cvarCount; ++cvarIndex) {
                const char *value = Com_ParseOnLine(&parseCursor);
                const size_t valueLength = strlen(value);
                if (valueLength >= R_AUTOCONFIG_VALUE_SIZE) {
                    ri.Error(
                        ERR_FATAL,
                        "cvar '%s' value '%s' len %i > %i",
                        cvars[cvarIndex]->name, value, (int32_t)valueLength,
                        R_AUTOCONFIG_VALUE_SIZE - 1);
                }
                memcpy(values[cvarIndex], value, valueLength + 1);
            }
        }

        Com_SkipRestOfLine(&parseCursor);
    }

    Com_EndParseSession();
    ri.FS_FreeFile(fileBuffer);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (bestRendererPrefixLength == 0)
        return;

    for (int32_t cvarIndex = 0; cvarIndex < cvarCount; ++cvarIndex) {
        cvar_t *const cvar = cvars[cvarIndex];
        ri.Cvar_Set(cvar->name, values[cvarIndex]);
        (void)ri.Cvar_Get(cvar->name, cvar->resetString, cvar->flags);
    }
}

/* Source: CoDUOMP.exe 0x004f6c90..0x004f6f08.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f6c90_004f6f09.mcode.
 * Name: exact same-module Mac symbol GLimp_Init. The native-width Win32 cvar
 * decoding preserves the original engine-to-renderer handoff without
 * narrowing HINSTANCE or WNDPROC on 64-bit Windows. */
void GLimp_Init(void)
{
    cvar_t *const lastValidRenderer = ri.Cvar_Get(
        "r_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE);

    ri.Printf(R_PRINT_ALL, "Initializing OpenGL subsystem\n");
    if (GLW_CheckOSVersion() == qfalse)
        ri.Error(ERR_FATAL, "EXE_ERR_BAD_WINDOWS_VER");

#if defined(_WIN32)
    cvar_t *hostValue = ri.Cvar_Get(
        "win_hinstance", "", CVAR_NONE);
    uintptr_t hostAddress = 0;
    (void)sscanf(hostValue->string, "%" SCNuPTR, &hostAddress);
    sysApplicationInstance = (void *)hostAddress;

    hostValue = ri.Cvar_Get("win_wndproc", "", CVAR_NONE);
    hostAddress = 0;
    (void)sscanf(hostValue->string, "%" SCNuPTR, &hostAddress);
    _Static_assert(
        sizeof(rendererWin32WindowProcedure) == sizeof(hostAddress),
        "Win32 window procedure must fit in its native address carrier");
    memcpy(&rendererWin32WindowProcedure, &hostAddress,
           sizeof(rendererWin32WindowProcedure));
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native non-Windows window creation does not
     * use the Win32 instance/window-procedure cvar handoff. */
#endif

    r_allowSoftwareGL = ri.Cvar_Get(
        "r_allowSoftwareGL", "0", CVAR_LATCH);
    (void)GLW_LoadOpenGL();

    glConfig.vendorString =
        (const char *)qglGetString(GL_VENDOR);
    glConfig.rendererString =
        (const char *)qglGetString(GL_RENDERER);
    glConfig.versionString =
        (const char *)qglGetString(GL_VERSION);
    glConfig.extensionsString =
        (const char *)qglGetString(GL_EXTENSIONS);

#if defined(_WIN32)
    rendererWglGetExtensionsStringEXTDriver =
        (renderer_wgl_get_extensions_string_ext_func_t)
            qwglGetProcAddress("wglGetExtensionsStringEXT");
    qwglGetExtensionsStringEXT =
        rendererWglGetExtensionsStringEXTDriver;
    if (qwglGetExtensionsStringEXT != NULL)
        glConfig.wglExtensionsString = qwglGetExtensionsStringEXT();
#else
    /* NOT_FROM_ORIGINAL_SOURCE: WGL has no extension string on the native SDL
     * path. GLX may return a callable-looking stub for this unsupported WGL
     * name, so never resolve or invoke it outside Windows. */
    rendererWglGetExtensionsStringEXTDriver = NULL;
    qwglGetExtensionsStringEXT = NULL;
    glConfig.wglExtensionsString = "";
#endif

    const char *optimizeValue = "1";
    if (strstr(glConfig.vendorString, "Matrox") != NULL)
        optimizeValue = "0";

    if (strstr(glConfig.vendorString, "NVIDIA") != NULL) {
        ri.Printf(R_PRINT_ALL, "NVIDIA detected");
        if (strstr(glConfig.rendererString, "GeForce") != NULL &&
            strstr(glConfig.rendererString, "ATI") != NULL) {
            optimizeValue = "0";
        }
    }

    if (strstr(glConfig.vendorString, "RADEON") != NULL) {
        ri.Printf(R_PRINT_ALL, "ATI detected");
        if (strstr(glConfig.rendererString, "ATI") != NULL &&
            strstr(glConfig.rendererString, "RADEON") != NULL) {
            optimizeValue = "0";
        }
    }

    ri.Cvar_Set("r_optimize", optimizeValue);
    GLW_ApplyRendererAutoConfig();

    const char *const rendererString = glConfig.rendererString;
    if (lastValidRenderer->string == NULL || rendererString == NULL ||
        Q_stricmp(lastValidRenderer->string, rendererString) != 0) {
        ri.Cvar_Set("r_textureMode", "GL_LINEAR_MIPMAP_NEAREST");
    }
    ri.Cvar_Set("r_lastValidRenderer", rendererString);

    GLimp_Extensions();
    GLimp_InitGamma();
}

/* Source: CoDUOMP.exe 0x004f6f10..0x004f70c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f6f10_004f70ca.mcode.
 * Name: exact same-module Mac symbol GLimp_Shutdown. The Win32 path restores
 * the captured desktop gamma ramp before releasing the current context,
 * context object, device context, and window in the original order. */
void GLimp_Shutdown(void)
{
#if defined(_WIN32)
    if (qwglMakeCurrent == NULL)
        return;
#else
    if (CoduoSDL_HasOpenGLWindow() == qfalse)
        return;
#endif

    ri.Printf(R_PRINT_ALL, "Shutting down OpenGL subsystem\n");

    GLimp_RestoreGamma();

#if defined(_WIN32)
    ri.Printf(
        R_PRINT_ALL,
        "...wglMakeCurrent( NULL, NULL ): %s\n",
        qwglMakeCurrent(NULL, NULL) != 0 ? "success" : "failed");

    if (rendererWin32RenderContext != NULL) {
        ri.Printf(
            R_PRINT_ALL, "...deleting GL context: %s\n",
            qwglDeleteContext(rendererWin32RenderContext) != 0
                ? "success" : "failed");
        rendererWin32RenderContext = NULL;
    }

    if (rendererWin32DeviceContext != NULL) {
        ri.Printf(
            R_PRINT_ALL, "...releasing DC: %s\n",
            ReleaseDC(win32MainWindow,
                      (HDC)rendererWin32DeviceContext) != 0
                ? "success" : "failed");
        rendererWin32DeviceContext = NULL;
    }

    if (win32MainWindow != NULL) {
        ri.Printf(R_PRINT_ALL, "...destroying window\n");
        ShowWindow(win32MainWindow, SW_HIDE);
        DestroyWindow(win32MainWindow);
        win32MainWindow = NULL;
        rendererWin32PixelFormatSet = qfalse;

        if (dedicated->integer == 0 && sysSplashWindow != NULL) {
            ShowWindow((HWND)sysSplashWindow, SW_SHOW);
            UpdateWindow((HWND)sysSplashWindow);
        }
    }
#else
    CoduoSDL_SetRelativeMouse(qfalse);
#endif

    if (rendererGlLogFile != NULL) {
        fclose(rendererGlLogFile);
        rendererGlLogFile = NULL;
    }

#if defined(_WIN32)
    if (rendererWin32FullscreenModeSet != qfalse) {
        ri.Printf(R_PRINT_ALL, "...resetting display\n");
        ChangeDisplaySettingsA(NULL, 0);
        rendererWin32FullscreenModeSet = qfalse;
    }
#endif

    QGL_Shutdown();
#if !defined(_WIN32)
    CoduoSDL_DestroyOpenGLWindow();
#endif
    memset(&glConfig, 0, sizeof(glConfig));
    memset(&glState, 0, sizeof(glState));
}

/* Source: CoDUOMP.exe 0x004f66c0..0x004f67d3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f66c0_004f67d4.mcode.
 * Name: exact same-module Mac symbol GLimp_EndFrame. In addition to the
 * platform buffer swap, the original applies modified swap-interval state
 * and refreshes the profiling/error/logging dispatch layers every frame. */
void GLimp_EndFrame(void)
{
    if (r_swapInterval->modified != qfalse) {
        r_swapInterval->modified = qfalse;
#if defined(_WIN32)
        if (glConfig.stereoEnabled == qfalse &&
            qwglSwapIntervalEXT != NULL) {
            qwglSwapIntervalEXT(r_swapInterval->integer);
        }
#else
        if (glConfig.stereoEnabled == qfalse)
            CoduoSDL_SetSwapInterval(r_swapInterval->integer);
#endif
    }

    if (Q_stricmp(r_drawBuffer->string, "GL_FRONT") != 0) {
        if (glConfig.vertexArrayRangeMode !=
            R_VERTEX_ARRAY_RANGE_NONE) {
            qglFlushVertexArrayRangeNV();
        }
#if defined(_WIN32)
        SwapBuffers((HDC)rendererWin32DeviceContext);
#else
        CoduoSDL_SwapWindow();
#endif
    }

    const int32_t logLevel = r_logFile->integer;
    const qboolean errorChecking =
        logLevel == 0 && r_debugGLErrors->integer != 0;
    const qboolean drawProfiling =
        logLevel == 0 &&
        r_debugGLErrors->integer == 0 &&
        r_profileDrawElements->integer != 0;

    QGL_EnableDrawProfiling(drawProfiling);
    QGL_EnableErrorChecking(errorChecking);
    QGL_EnableLogging(logLevel);
}

/* Source: CoDUOMP.exe 0x004f4080..0x004f4109.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4080_004f410a.mcode.
 * Provisional role name: the sole caller invokes this immediately after
 * making the new WGL context current. The four GL enum operands and error
 * strings prove that it rejects a context missing any mandatory identity
 * string. No Windows API types cross this helper boundary. */
qboolean GLW_ValidateOpenGLStrings(void)
{
    if (qglGetString(GL_VENDOR) == NULL) {
        ri.Printf(R_PRINT_ALL,
                  "glGetString(GL_VENDOR) returned NULL\n");
        return qfalse;
    }
    if (qglGetString(GL_RENDERER) == NULL) {
        ri.Printf(R_PRINT_ALL,
                  "glGetString(GL_RENDERER) returned NULL\n");
        return qfalse;
    }
    if (qglGetString(GL_VERSION) == NULL) {
        ri.Printf(R_PRINT_ALL,
                  "glGetString(GL_VERSION) returned NULL\n");
        return qfalse;
    }
    if (qglGetString(GL_EXTENSIONS) == NULL) {
        ri.Printf(R_PRINT_ALL,
                  "glGetString(GL_EXTENSIONS) returned NULL\n");
        return qfalse;
    }
    return qtrue;
}
