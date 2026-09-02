#include "q_shared.h"

#include "platform/crt_boundary.h"

#include <string.h>


#if defined(_WIN32)
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#endif

#if defined(_WIN32)

typedef struct videoMemoryRendererEstimate_s {
    const char *rendererPrefix;
    int32_t memoryMegabytes;
} videoMemoryRendererEstimate_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(videoMemoryRendererEstimate_t) == 4, "i386 renderer-memory estimate alignment changed");
_Static_assert(offsetof(videoMemoryRendererEstimate_t, rendererPrefix) == 0x00, "i386 renderer-memory prefix moved");
_Static_assert(offsetof(videoMemoryRendererEstimate_t, memoryMegabytes) == 0x04, "i386 renderer-memory estimate moved");
_Static_assert(sizeof(videoMemoryRendererEstimate_t) == 0x08, "i386 renderer-memory estimate stride changed");
#endif

/* Source: CoDUOMP.exe 0x00469410..0x00469768.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469410_00469769.mcode.
 * Role name: the OpenGL fallback passes GL_RENDERER here, and every table
 * result is shifted left by 20 before returning. Ordering is significant:
 * model-specific prefixes precede their generic family prefix. */
static uint32_t Win_EstimateVideoMemoryBytesFromRenderer(const char *renderer)
{
    /* The original constructs all 47 records in its 0x178-byte stack frame at
     * 0x00469427..0x00469714; there is no static estimate table. */
    const videoMemoryRendererEstimate_t estimates[] = {{"Radeon 7200", 64},
                                                       {"Radeon 7500", 64},
                                                       {"Radeon 8500", 64},
                                                       {"Radeon 9000", 64},
                                                       {"Radeon 9100", 64},
                                                       {"Radeon 9200", 64},
                                                       {"Radeon 9500", 64},
                                                       {"Radeon 9600", 128},
                                                       {"Radeon 9700", 128},
                                                       {"Radeon 9800", 128},
                                                       {"Radeon", 32},
                                                       {"GeForce 256", 32},
                                                       {"GeForce2 GTS", 32},
                                                       {"GeForce2 MX", 32},
                                                       {"GeForce2", 32},
                                                       {"GeForce3", 64},
                                                       {"GeForce4 420 Go", 64},
                                                       {"GeForce4 4200 Go", 64},
                                                       {"GeForce4 440 Go", 64},
                                                       {"GeForce4 460 Go", 64},
                                                       {"GeForce4 MX 420", 64},
                                                       {"GeForce4 MX 440", 64},
                                                       {"GeForce4 MX 460", 64},
                                                       {"GeForce4 MX", 64},
                                                       {"GeForce4 Ti 4200", 64},
                                                       {"GeForce4 Ti 4400", 128},
                                                       {"GeForce4 Ti 4600", 128},
                                                       {"GeForce4 Ti 4800", 128},
                                                       {"GeForce4", 64},
                                                       {"GeForce FX 5200", 64},
                                                       {"GeForce FX 5600", 128},
                                                       {"GeForce FX 5800 Ultra", 128},
                                                       {"GeForce FX 5800", 128},
                                                       {"GeForce FX 5900 Ultra", 128},
                                                       {"GeForce FX 5900", 128},
                                                       {"Quadro FX 1000", 128},
                                                       {"Quadro FX", 128},
                                                       {"Quadro4 500", 64},
                                                       {"Quadro4 700", 64},
                                                       {"Quadro4 900", 64},
                                                       {"Quadro4", 64},
                                                       {"Quadro2 Pro", 32},
                                                       {"Quadro2 MXR", 32},
                                                       {"Quadro2", 32},
                                                       {"Quadro DCC", 32},
                                                       {"Quadro", 32},
                                                       {"Matrox ICD for Parhelia", 128}};

    for (size_t index = 0; index < sizeof(estimates) / sizeof(estimates[0]); ++index) {
        const size_t prefixLength = strlen(estimates[index].rendererPrefix);
        if (coduo_crt_strnicmp(renderer, estimates[index].rendererPrefix, prefixLength) == 0) {
            return (uint32_t)estimates[index].memoryMegabytes * 1024u * 1024u;
        }
    }
    return 0;
}

enum {
    WIN_DDRAW_LOCAL_VIDEO_MEMORY = 0x4000,
    WIN_DDRAW_COOPERATIVE_FLAGS = 17,
    WIN_OPENGL_RENDERER_NAME = 0x1f01,
    WIN_OPENGL_PIXEL_FLAGS = 37
};

/* Source: CoDUOMP.exe 0x00469260..0x004692d0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469260_004692d1.mcode.
 * Role name: DirectDrawCreateEx produces IDirectDraw7, whose
 * GetAvailableVidMem method receives DDSCAPS_LOCALVIDMEM. */
static uint32_t Win_GetDirectDrawVideoMemoryBytes(GUID *adapterGuid)
{
    IDirectDraw7 *directDraw = NULL;
    DDSCAPS2 caps;
    DWORD totalBytes = 0;
    DWORD freeBytes = 0;
    HRESULT result;

    result = DirectDrawCreateEx(adapterGuid, (void **)&directDraw, &IID_IDirectDraw7, NULL);
    if (FAILED(result))
        return 0;

    memset(&caps, 0, sizeof(caps));
    caps.dwCaps = WIN_DDRAW_LOCAL_VIDEO_MEMORY;
    result = IDirectDraw7_GetAvailableVidMem(directDraw, &caps, &totalBytes, &freeBytes);
    IDirectDraw7_Release(directDraw);
    return SUCCEEDED(result) ? (uint32_t)totalBytes : 0;
}

/* Source: CoDUOMP.exe 0x004692e0..0x00469302, recovered from the executable
 * gap as the DirectDrawEnumerateExA callback passed at 0x004698f5. */
static BOOL WINAPI Win_CollectDirectDrawVideoMemory(GUID *adapterGuid, LPSTR description, LPSTR name, LPVOID context, HMONITOR monitor)
{
    uint32_t *maximumBytes = (uint32_t *)context;
    (void)description;
    (void)name;

    if (monitor == NULL) {
        const uint32_t bytes = Win_GetDirectDrawVideoMemoryBytes(adapterGuid);
        if (bytes > *maximumBytes)
            *maximumBytes = bytes;
    }
    return TRUE;
}

/* Source: CoDUOMP.exe 0x00469310..0x004693e4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469310_004693e5.mcode.
 * This fallback gives IDirectDraw7 a one-pixel dummy window and exclusive
 * fullscreen cooperative level before querying local video memory. */
static uint32_t Win_GetExclusiveDirectDrawVideoMemoryBytes(GUID *adapterGuid)
{
    IDirectDraw7 *directDraw = NULL;
    DDSCAPS2 caps;
    DWORD totalBytes = 0;
    DWORD freeBytes = 0;
    HWND window;
    HRESULT result;

    result = DirectDrawCreateEx(adapterGuid, (void **)&directDraw, &IID_IDirectDraw7, NULL);
    if (FAILED(result))
        return 0;

    /* 0x0046932f..0x00469355: DirectDraw is created before the dummy window.
     * Every post-window exit destroys the window before releasing DirectDraw. */
    window = CreateWindowExA(0, "static", "dummy", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (window == NULL) {
        IDirectDraw7_Release(directDraw);
        return 0;
    }
    result = IDirectDraw7_SetCooperativeLevel(directDraw, window, WIN_DDRAW_COOPERATIVE_FLAGS);
    if (FAILED(result)) {
        DestroyWindow(window);
        IDirectDraw7_Release(directDraw);
        return 0;
    }

    memset(&caps, 0, sizeof(caps));
    caps.dwCaps = WIN_DDRAW_LOCAL_VIDEO_MEMORY;
    result = IDirectDraw7_GetAvailableVidMem(directDraw, &caps, &totalBytes, &freeBytes);
    DestroyWindow(window);
    IDirectDraw7_Release(directDraw);
    return SUCCEEDED(result) ? (uint32_t)totalBytes : 0;
}

/* Source: CoDUOMP.exe 0x004693f0..0x0046940a, recovered from the executable
 * gap as the second DirectDrawEnumerateExA callback at 0x0046991a. */
static BOOL WINAPI Win_CollectExclusiveDirectDrawVideoMemory(GUID *adapterGuid, LPSTR description, LPSTR name, LPVOID context,
                                                             HMONITOR monitor)
{
    uint32_t *maximumBytes = (uint32_t *)context;
    const uint32_t bytes = Win_GetExclusiveDirectDrawVideoMemoryBytes(adapterGuid);
    (void)description;
    (void)name;
    (void)monitor;

    if (bytes > *maximumBytes)
        *maximumBytes = bytes;
    return TRUE;
}

/* Source: CoDUOMP.exe 0x00469770..0x004698dc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469770_004698dd.mcode.
 * Final Windows fallback: construct a temporary OpenGL context, read
 * GL_RENDERER, and estimate memory through the original 47-entry table. */
static uint32_t Win_GetOpenGLVideoMemoryBytes(void)
{
    typedef HGLRC(WINAPI * wgl_create_context_t)(HDC);
    typedef BOOL(WINAPI * wgl_make_current_t)(HDC, HGLRC);
    typedef BOOL(WINAPI * wgl_delete_context_t)(HGLRC);
    typedef const unsigned char *(WINAPI * gl_get_string_t)(unsigned int);

    HWND window;
    HDC deviceContext;
    HMODULE openGL;
    wgl_create_context_t createContext;
    wgl_make_current_t makeCurrent;
    wgl_delete_context_t deleteContext;
    gl_get_string_t getString;
    PIXELFORMATDESCRIPTOR descriptor;
    HGLRC renderContext;
    int pixelFormat;
    uint32_t memoryBytes = 0;

    window = CreateWindowExA(0, "static", "dummy", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (window == NULL)
        return 0;

    deviceContext = GetDC(window);
    if (deviceContext == NULL) {
        DestroyWindow(window);
        return 0;
    }

    openGL = LoadLibraryA("opengl32.dll");
    if (openGL == NULL) {
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 0;
    }

    createContext = (wgl_create_context_t)(uintptr_t)GetProcAddress(openGL, "wglCreateContext");
    makeCurrent = (wgl_make_current_t)(uintptr_t)GetProcAddress(openGL, "wglMakeCurrent");
    deleteContext = (wgl_delete_context_t)(uintptr_t)GetProcAddress(openGL, "wglDeleteContext");
    getString = (gl_get_string_t)(uintptr_t)GetProcAddress(openGL, "glGetString");
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (createContext == NULL || makeCurrent == NULL || deleteContext == NULL || getString == NULL) {
        FreeLibrary(openGL);
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 0;
    }

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = WIN_OPENGL_PIXEL_FLAGS;
    descriptor.iPixelType = PFD_TYPE_RGBA;

    pixelFormat = ChoosePixelFormat(deviceContext, &descriptor);
    if (pixelFormat != 0) {
        DescribePixelFormat(deviceContext, pixelFormat, sizeof(descriptor), &descriptor);
        if (SetPixelFormat(deviceContext, pixelFormat, &descriptor) != FALSE) {
            renderContext = createContext(deviceContext);
            if (renderContext != NULL) {
                if (makeCurrent(deviceContext, renderContext) != FALSE) {
                    const unsigned char *const renderer = getString(WIN_OPENGL_RENDERER_NAME);
                    if (renderer != NULL) {
                        memoryBytes = Win_EstimateVideoMemoryBytesFromRenderer((const char *)renderer);
                    }
                    makeCurrent(NULL, NULL);
                }
                deleteContext(renderContext);
            }
        }
    }

    FreeLibrary(openGL);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ReleaseDC(window, deviceContext);
    DestroyWindow(window);
    return memoryBytes;
}

#endif

/* Source: CoDUOMP.exe 0x004698e0..0x00469958.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004698e0_00469959.mcode.
 * Role name: the result is stored at 0x009cf1e4, printed as video-card MB,
 * and used as sys_vidMB. DirectDraw is tried both directly and through
 * adapter enumeration before the OpenGL renderer-name fallback.
 *
 * The final value is a power-of-two MB bucket. If rounding upward overshoots
 * the measured ceiling by more than 32 MB, the previous bucket is selected. */
int32_t Sys_GetVideoMemoryMB(void)
{
#if defined(_WIN32)
    uint32_t memoryBytes = Win_GetDirectDrawVideoMemoryBytes(NULL);

    if (memoryBytes == 0) {
        DirectDrawEnumerateExA(Win_CollectDirectDrawVideoMemory, &memoryBytes, 0);
    }
    if (memoryBytes == 0) {
        memoryBytes = Win_GetExclusiveDirectDrawVideoMemoryBytes(NULL);
    }
    if (memoryBytes == 0) {
        DirectDrawEnumerateExA(Win_CollectExclusiveDirectDrawVideoMemory, &memoryBytes, 0);
    }
    if (memoryBytes == 0)
        memoryBytes = Win_GetOpenGLVideoMemoryBytes();
    if (memoryBytes == 0)
        return 0;

    const uint32_t measuredMegabytes = ((memoryBytes - 1u) >> 20) + 1u;
    uint32_t bucketMegabytes = 1;
    while (bucketMegabytes < measuredMegabytes)
        bucketMegabytes <<= 1;
    if (bucketMegabytes - measuredMegabytes > 32u)
        bucketMegabytes >>= 1;
    return (int32_t)bucketMegabytes;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: the recovered probe is entirely Win32
     * DirectDraw/WGL code. Native render backends will supply a platform GPU
     * memory query when the renderer boundary is made operational. */
    return 0;
#endif
}
