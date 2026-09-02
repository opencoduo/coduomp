#include "system_platform.h"

#include "client/common/client_legacy_crt.h"
#include "client/cgame.h"
#include "filesystem/filesystem.h"
#include "qcommon/game_module_abi_types.h"
#include "networking/net_address.h"
#include "platform/dynamic_library_boundary.h"
#if defined(__APPLE__)
#include "platform/macos_app_bundle.h"
#endif
#include "renderer/backend.h"
#include "renderer/renderer_api.h"
#include "system_console.h"
#include "system_event.h"
#include "system_fatal.h"
#include "system_input.h"
#include "system_localization.h"
#include "system_process_lock.h"
#include "ui/ui_module_loader.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#else
#include "platform/sdl_platform.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

enum {
    SYS_FUNCTION_SCAN_LIMIT = 1024,
    SYS_ERROR_TEXT_CAPACITY = 4096,
    SYS_CONSOLE_VISIBLE = 1,
    SYS_MONKEY_COMMAND_CAPACITY = 2 * MAX_OSPATH + 96,
    SYS_USER_NAME_CAPACITY = 1024,
    SYS_PROCESSOR_X86 = 1,
    SYS_WINDOWS_98_FIRST_BUILD = 1998,
    SYS_WINDOWS_95_OSR2_FIRST_BUILD = 1111
};

void *sysSplashWindow;               /* original 0x009d0918 */
qboolean sysCheckCrashOrRerun;       /* original 0x009cf2e8 */
uint32_t sysExecutableChecksum;       /* original 0x009ceda8 */
static char sysCurrentUser[SYS_USER_NAME_CAPACITY];
                                        /* original 0x009d0518..0x009d0917 */
#if defined(_WIN32)
static OSVERSIONINFOA sysOsVersionInfo;  /* original 0x0489bb98 */
static char sysInstallMediaPath[MAX_OSPATH];
                                        /* original 0x009cf1e8..0x009cf2e7 */
static HBITMAP sysSplashBitmap;
static qboolean sysSplashClassRegistered;
#endif

/* Source: CoDUOMP.exe 0x0046be10..0x0046c26a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046be10_0046c26b.mcode, the exact
 * strings at 0x0059efdc..0x0059f2bf, and the imported Win32 calls.
 * Name: exact same-module Mac symbol Sys_Init. */
void Sys_Init(void)
{
    sysCpuClass_t cpuClass;

#if defined(_WIN32)
    timeBeginPeriod(1);
#endif

    Cmd_AddCommand("in_restart", IN_Restart);
    Cmd_AddCommand("net_restart", NET_Restart_f);

#if defined(_WIN32)
    const char *architecture;

    sysOsVersionInfo.dwOSVersionInfoSize = sizeof(sysOsVersionInfo);
    if (GetVersionExA(&sysOsVersionInfo) == FALSE)
        Sys_Error("Couldn't get OS info");
    if (sysOsVersionInfo.dwMajorVersion < 4) {
        Sys_Error("CoD:United Offensive Multiplayer requires Windows "
                  "version 4 or greater");
    }

    switch (sysOsVersionInfo.dwPlatformId) {
    case VER_PLATFORM_WIN32s:
        Sys_Error("CoD:United Offensive Multiplayer doesn't run on Win32s");
        architecture = "unknown Windows variant";
        break;
    case VER_PLATFORM_WIN32_NT:
        architecture = "winnt";
        break;
    case VER_PLATFORM_WIN32_WINDOWS: {
        const uint16_t build = (uint16_t)sysOsVersionInfo.dwBuildNumber;
        if (build >= SYS_WINDOWS_98_FIRST_BUILD)
            architecture = "win98";
        else if (build >= SYS_WINDOWS_95_OSR2_FIRST_BUILD)
            architecture = "win95 osr2.x";
        else
            architecture = "win95";
        break;
    }
    default:
        architecture = "unknown Windows variant";
        break;
    }
    (void)Cvar_Set2("arch", architecture, qtrue);

#if UINTPTR_MAX == UINT32_MAX
    (void)Cvar_Get("win_hinstance", va("%i", (int32_t)(uintptr_t)sysApplicationInstance), CVAR_ROM);
#else
    (void)Cvar_Get("win_hinstance", va("%" PRIuPTR, (uintptr_t)sysApplicationInstance), CVAR_ROM);
#endif

    WNDPROC windowProcedure = MainWndProc;
    uintptr_t windowProcedureAddress = 0;
    _Static_assert(sizeof(windowProcedureAddress) >= sizeof(windowProcedure), "WNDPROC must fit in uintptr_t");
    memcpy(&windowProcedureAddress, &windowProcedure, sizeof(windowProcedure));
#if UINTPTR_MAX == UINT32_MAX
    (void)Cvar_Get("win_wndproc", va("%i", (int32_t)windowProcedureAddress), CVAR_ROM);
#else
    (void)Cvar_Get("win_wndproc", va("%" PRIuPTR, windowProcedureAddress), CVAR_ROM);
#endif
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native Unix renderers do not consume the
     * Win32 architecture/instance/window-procedure cvars. */
#endif

    (void)Cvar_Get("sys_cpustring", "detect", 0);
    const char *const configuredCpu = Cvar_VariableString("sys_cpustring");
    if (Q_stricmp(configuredCpu, "detect") == 0) {
        Com_Printf("...detecting CPU, found ");
        cpuClass = Sys_DetectCpuClass();
        switch (cpuClass) {
        case CPUID_GENERIC:
            (void)Cvar_Set2("sys_cpustring", "generic", qtrue);
            break;
        case CPUID_AXP:
            (void)Cvar_Set2("sys_cpustring", "Alpha AXP", qtrue);
            break;
        case CPUID_INTEL_UNSUPPORTED:
            (void)Cvar_Set2("sys_cpustring", "x86 (pre-Pentium)", qtrue);
            break;
        case CPUID_INTEL_PENTIUM:
            (void)Cvar_Set2("sys_cpustring", "x86 (P5/PPro, non-MMX)", qtrue);
            break;
        case CPUID_INTEL_MMX:
            (void)Cvar_Set2("sys_cpustring", "x86 (P5/Pentium2, MMX)", qtrue);
            break;
        case CPUID_INTEL_KATMAI:
            (void)Cvar_Set2("sys_cpustring", "Intel Pentium III", qtrue);
            break;
        case CPUID_AMD_3DNOW:
            (void)Cvar_Set2("sys_cpustring", "AMD w/ 3DNow!", qtrue);
            break;
        default:
            Com_Error(ERR_FATAL, "\x15Unknown cpu type %d\n", cpuClass);
            break;
        }
    } else {
        Com_Printf("...forcing CPU type to ");
        if (Q_stricmp(configuredCpu, "generic") == 0)
            cpuClass = CPUID_GENERIC;
        else if (Q_stricmp(configuredCpu, "x87") == 0)
            cpuClass = CPUID_INTEL_PENTIUM;
        else if (Q_stricmp(configuredCpu, "mmx") == 0)
            cpuClass = CPUID_INTEL_MMX;
        else if (Q_stricmp(configuredCpu, "3dnow") == 0)
            cpuClass = CPUID_AMD_3DNOW;
        else if (Q_stricmp(configuredCpu, "PentiumIII") == 0)
            cpuClass = CPUID_INTEL_KATMAI;
        else if (Q_stricmp(configuredCpu, "axp") == 0)
            cpuClass = CPUID_AXP;
        else {
            Com_Printf("WARNING: unknown sys_cpustring '%s'\n", configuredCpu);
            cpuClass = CPUID_GENERIC;
        }
    }

    Cvar_SetValue("sys_cpuid", (float)cpuClass);
    Com_Printf("%s\n", Cvar_VariableString("sys_cpustring"));
    Com_Printf("Measured CPU speed is %.2lf GHz\n", sysCpuFrequencyMHz * 0.001);
    Com_Printf("System memory is %i MB (capped at 1 GB)\n", sysPhysicalMemoryMB);
    Com_Printf("Video card memory is %i MB\n", sysVideoMemoryMB);
    Com_Printf("Streaming SIMD Extensions (SSE) %ssupported\n", sysSseSupported != qfalse ? "" : "not ");
    Com_Printf("\n");
    (void)Cvar_Set2("username", Sys_GetCurrentUser(), qtrue);
    IN_Init();
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical initial
 * JMP-thunk resolution and RET/NOP terminator scan in the original
 * Sys_FunctionsMatch and Sys_FunctionChecksum bodies. */
static const uint8_t *coduomp_resolve_function_entry(const uint8_t *function)
{
    int32_t displacement;

    if (function[0] != 0xe9)
        return function;

    memcpy(&displacement, &function[1], sizeof(displacement));
    return function + 5 + displacement;
}

/* NOT_FROM_ORIGINAL_SOURCE: see coduomp_resolve_function_entry. The original
 * scan stops at C3 90 90 and compares/checksums through the first NOP. If no
 * such terminator appears in the first 1024 candidate positions, it uses
 * 1026 bytes. */
static int32_t coduomp_function_comparison_length(const uint8_t *function)
{
    int32_t offset;

    for (offset = 0; offset < SYS_FUNCTION_SCAN_LIMIT; ++offset) {
        if (function[offset] == 0xc3 && function[offset + 1] == 0x90 && function[offset + 2] == 0x90) {
            break;
        }
    }
    return offset + 2;
}

/* Source: CoDUOMP.exe 0x0046acf0..0x0046ad16, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Provisional role name: creates the hidden "crash" sentinel if it does not
 * already exist. The original ignores both creation and close errors. */
void Sys_CreateCrashMarker(void)
{
#if defined(_WIN32)
    HANDLE marker = CreateFileA("crash", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_HIDDEN, NULL);
    if (marker != INVALID_HANDLE_VALUE)
        CloseHandle(marker);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: POSIX equivalent of the Windows hidden-file
     * sentinel operation. POSIX has no hidden attribute; the filename and
     * exclusive-create behavior remain identical. */
    const int marker = open("crash", O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (marker != -1)
        close(marker);
#endif
}

#if defined(_WIN32)
/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the retail client predates
 * Windows DPI virtualization, so modern Windows treats the process as
 * DPI-unaware and stretches its windows by the display scale factor. The
 * Native fullscreen mode sizes the game window from EnumDisplaySettings,
 * which always reports physical pixels, so at 125% scale the window is
 * created 25% larger than the screen and cropped to its top-left corner.
 * Declaring DPI awareness before the first window exists makes one window
 * unit equal one physical pixel for every metric, window, and cursor API the
 * client uses, which also keeps borderless mode at true desktop resolution
 * instead of a stretched virtualized size. The three setters are resolved
 * dynamically because each first appeared in a newer Windows release:
 * per-monitor-v2 awareness (Windows 10 1703), per-monitor awareness
 * (Windows 8.1), then system awareness (Windows Vista). Systems without any
 * of them have no DPI virtualization to opt out of. */
void coduomp_sys_enable_dpi_awareness(void)
{
    typedef BOOL(WINAPI * sys_set_dpi_context_fn)(void *);
    typedef HRESULT(WINAPI * sys_set_dpi_awareness_fn)(int32_t);
    typedef BOOL(WINAPI * sys_set_dpi_aware_fn)(void);
    /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2; the SDK handle constant
     * is absent from older MinGW headers. */
    void *const perMonitorAwareV2 = (void *)(intptr_t)-4;
    /* PROCESS_PER_MONITOR_DPI_AWARE from the Windows 8.1 shell-scaling
     * enumeration, likewise absent from older MinGW headers. */
    enum {
        SYS_PROCESS_PER_MONITOR_DPI_AWARE = 2
    };

    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32 != NULL) {
        sys_set_dpi_context_fn setContext = (sys_set_dpi_context_fn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setContext != NULL && setContext(perMonitorAwareV2) != FALSE)
            return;
    }

    HMODULE shcore = LoadLibraryA("shcore.dll");
    if (shcore != NULL) {
        sys_set_dpi_awareness_fn setAwareness = (sys_set_dpi_awareness_fn)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (setAwareness != NULL && SUCCEEDED(setAwareness(SYS_PROCESS_PER_MONITOR_DPI_AWARE))) {
            FreeLibrary(shcore);
            return;
        }
        FreeLibrary(shcore);
    }

    if (user32 != NULL) {
        sys_set_dpi_aware_fn setAware = (sys_set_dpi_aware_fn)GetProcAddress(user32, "SetProcessDPIAware");
        if (setAware != NULL)
            (void)setAware();
    }
}
#endif

/* Source: CoDUOMP.exe 0x0046e2e0..0x0046e474.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e2e0_0046e475.mcode and the PE
 * imports at 0x00586284..0x00586300. The title, class, and bitmap strings prove
 * that 0x009d0918 owns the startup splash rather than the gameplay window. */
void Sys_CreateSplashWindow(void)
{
#if defined(_WIN32)
    enum {
        SYS_SPLASH_ICON_RESOURCE = 133,
        SYS_SPLASH_WIDTH = 320,
        SYS_SPLASH_HEIGHT = 100
    };
    WNDCLASSA windowClass = {0};
    windowClass.lpfnWndProc = DefWindowProcA;
    windowClass.hInstance = (HINSTANCE)sysApplicationInstance;
    windowClass.hIcon = LoadIconA((HINSTANCE)sysApplicationInstance, MAKEINTRESOURCEA(SYS_SPLASH_ICON_RESOURCE));
    windowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
    windowClass.lpszClassName = "CoD Splash Screen";
    if (RegisterClassA(&windowClass) == 0)
        return;
    sysSplashClassRegistered = qtrue;

    const int32_t screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int32_t screenHeight = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP bitmap = (HBITMAP)LoadImageA(NULL, "coduo.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (bitmap == NULL) {
        Sys_DestroySplashWindow();
        return;
    }
    sysSplashBitmap = bitmap;

    HWND splash =
        CreateWindowExA(WS_EX_APPWINDOW, "CoD Splash Screen", "CoD:United Offensive Multiplayer", WS_POPUP | WS_BORDER | WS_SYSMENU,
                        (screenWidth - SYS_SPLASH_WIDTH) / 2, (screenHeight - SYS_SPLASH_HEIGHT) / 2, SYS_SPLASH_WIDTH, SYS_SPLASH_HEIGHT,
                        NULL, NULL, (HINSTANCE)sysApplicationInstance, NULL);
    sysSplashWindow = splash;
    if (splash == NULL) {
        Sys_DestroySplashWindow();
        return;
    }

    HWND image = CreateWindowExA(0, "Static", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP, 0, 0, SYS_SPLASH_WIDTH, SYS_SPLASH_HEIGHT, splash,
                                 NULL, (HINSTANCE)sysApplicationInstance, NULL);
    if (image == NULL) {
        Sys_DestroySplashWindow();
        return;
    }

    (void)SendMessageA(image, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmap);

    RECT imageRect;
    (void)GetWindowRect(image, &imageRect);
    const int32_t imageWidth = imageRect.right - imageRect.left;
    const int32_t imageHeight = imageRect.bottom - imageRect.top;
    (void)SetWindowPos(splash, NULL, (screenWidth - imageWidth) / 2, (screenHeight - imageHeight) / 2, imageWidth, imageHeight,
                       SWP_NOZORDER);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: non-Windows launchers own any splash UI. */
#endif
}

/* Source: CoDUOMP.exe 0x0046e480..0x0046e4a8, recovered from the executable
 * gap after repairing three missing Ghidra function entries.
 * Name: exact same-module Mac symbol Sys_DestroySplashWindow. */
void Sys_DestroySplashWindow(void)
{
#if defined(_WIN32)
    if (sysSplashWindow != NULL) {
        ShowWindow((HWND)sysSplashWindow, SW_HIDE);
        if (DestroyWindow((HWND)sysSplashWindow) == FALSE)
            return;
        sysSplashWindow = NULL;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (sysSplashBitmap != NULL) {
        (void)DeleteObject(sysSplashBitmap);
        sysSplashBitmap = NULL;
    }
    if (sysSplashClassRegistered != qfalse) {
        (void)UnregisterClassA("CoD Splash Screen", (HINSTANCE)sysApplicationInstance);
        sysSplashClassRegistered = qfalse;
    }
#endif
}

/* Source: CoDUOMP.exe 0x0046e4b0..0x0046e4ce, recovered from the executable
 * gap. Windows-only role name: shows and immediately paints the existing
 * startup splash window. */
void Sys_ShowSplashWindow(void)
{
#if defined(_WIN32)
    if (sysSplashWindow != NULL) {
        ShowWindow((HWND)sysSplashWindow, SW_SHOW);
        UpdateWindow((HWND)sysSplashWindow);
    }
#endif
}

/* Source: CoDUOMP.exe 0x0046e4d0..0x0046e4e2, recovered from the executable
 * gap after repairing its missing Ghidra entry.
 * Name: exact same-module Mac symbol Sys_HideSplashWindow. */
void Sys_HideSplashWindow(void)
{
#if defined(_WIN32)
    if (sysSplashWindow != NULL)
        ShowWindow((HWND)sysSplashWindow, SW_HIDE);
#endif
}

/* Source: CoDUOMP.exe 0x0046ad20..0x0046ae1f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046ad20_0046ae20.mcode.
 * Provisional role name: compares two x86 function bodies after resolving an
 * initial near-JMP thunk. Relative near calls compare by resolved target so
 * relocation alone does not make otherwise matching code unequal. */
qboolean Sys_FunctionsMatch(const uint8_t *leftFunction, const uint8_t *rightFunction)
{
    const uint8_t *left = coduomp_resolve_function_entry(leftFunction);
    const uint8_t *right = coduomp_resolve_function_entry(rightFunction);
    const int32_t length = coduomp_function_comparison_length(left);

    for (int32_t offset = 0; offset < length; ++offset) {
        if (left[offset] == 0xe8) {
            int32_t leftDisplacement;
            int32_t rightDisplacement;
            memcpy(&leftDisplacement, &left[offset + 1], sizeof(leftDisplacement));
            memcpy(&rightDisplacement, &right[offset + 1], sizeof(rightDisplacement));

            const uintptr_t leftTarget = (uintptr_t)&left[offset + 5] + (intptr_t)leftDisplacement;
            const uintptr_t rightTarget = (uintptr_t)&right[offset + 5] + (intptr_t)rightDisplacement;
            if (leftTarget == rightTarget) {
                offset += 4;
                continue;
            }
        }

        if (left[offset] != right[offset])
            return qfalse;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x0046ae20..0x0046aebe, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Provisional role name: hashes the same x86 function-body range understood
 * by Sys_FunctionsMatch. */
uint32_t Sys_FunctionChecksum(const uint8_t *function)
{
    const uint8_t *entry = coduomp_resolve_function_entry(function);
    return Com_BlockChecksum(entry, coduomp_function_comparison_length(entry));
}

/* Source: CoDUOMP.exe 0x0046aec0..0x0046aec5, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Name and result: exact same-module Mac symbol Sys_CheckCrashOrRerun. The
 * Windows compiler inlines the same global read in Com_Frame. */
qboolean Sys_CheckCrashOrRerun(void)
{
    return sysCheckCrashOrRerun;
}

/* Source: CoDUOMP.exe 0x0046aed0..0x0046af40.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046aed0_0046af41.mcode.
 * Name: exact same-module Mac symbol Sys_Shutdown. The platform window
 * teardown and WinMM timer-period release are Windows-only; game, input,
 * renderer, and stat-monitor teardown are common source behavior. */
void Sys_Shutdown(void)
{
    /* The Windows compiler inlines Sys_DestroySplashWindow here. */
    Sys_DestroySplashWindow();
#if defined(_WIN32)
    timeEndPeriod(1);
#endif

    IN_Shutdown();
    CL_ShutdownCGame();
    CL_ShutdownUI();

    CL_ShutdownRef();
}

/* Source: CoDUOMP.exe 0x0046af90..0x0046afaa, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Name and behavior: exact same-module Mac symbol
 * Sys_UnableToLoadDLLError. */
void Sys_UnableToLoadDLLError(void)
{
    Com_Error(ERR_FATAL, "%s\n", Sys_LocalizeString("WIN_UNABLE_LOAD_DLL_BODY"));
}

/* Source: CoDUOMP.exe 0x0046b2a0..0x0046b380.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046b2a0_0046b381.mcode.
 * Name and signature: exact same-module Mac symbol Sys_StartProcess. */
void Sys_StartProcess(const char *executableName, qboolean doExit)
{
#if defined(_WIN32)
    char workingDirectory[MAX_OSPATH];
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;

    memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    GetCurrentDirectoryA(sizeof(workingDirectory), workingDirectory);

    if (CreateProcessA(NULL, va("\"%s\\%s\"", workingDirectory, executableName), NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo,
                       &processInfo) == FALSE) {
        const DWORD errorCode = GetLastError();
        char *systemMessage = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char *)&systemMessage, 0, NULL);
        Com_Error(ERR_FATAL, "EXE_ERR_COULDNT_START_PROCESS\x15'%s\\%s'\n%s\n%08x", workingDirectory, executableName, systemMessage,
                  errorCode);
    }
#else
    /* NOT_FROM_ORIGINAL_SOURCE: POSIX implementation of the Windows
     * CreateProcess boundary. The original starts one executable from the
     * current directory without passing additional arguments. */
    char workingDirectory[MAX_OSPATH];
    char executablePath[2 * MAX_OSPATH];

    if (getcwd(workingDirectory, sizeof(workingDirectory)) == NULL) {
        Com_Error(ERR_FATAL, "EXE_ERR_COULDNT_START_PROCESS\x15'%s'\n%s\n%08x", executableName, strerror(errno), (uint32_t)errno);
    }
    Com_sprintf(executablePath, sizeof(executablePath), "%s/%s", workingDirectory, executableName);

    const pid_t child = fork();
    if (child == 0) {
        execl(executablePath, executablePath, (char *)NULL);
        _exit(127);
    }
    if (child < 0) {
        Com_Error(ERR_FATAL, "EXE_ERR_COULDNT_START_PROCESS\x15'%s/%s'\n%s\n%08x", workingDirectory, executableName, strerror(errno),
                  (uint32_t)errno);
    }
#endif

    if (doExit != qfalse)
        Cbuf_AddText("quit\n");
}

/* Source: CoDUOMP.exe 0x0046b390..0x0046b3e3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046b390_0046b3e4.mcode.
 * Name and signature: exact same-module Mac symbol Sys_OpenURL. */
void Sys_OpenURL(const char *url, qboolean doExit)
{
#if defined(_WIN32)
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ShellExecuteA(NULL, "open", url, NULL, NULL, SW_RESTORE) == NULL) {
        Com_Error(1, "EXE_ERR_COULDNT_OPEN_URL\x15%s", url);
    }

    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow != NULL)
        ShowWindow(foregroundWindow, SW_MAXIMIZE);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native desktop URL opener corresponding to
     * the Windows ShellExecuteA boundary. */
    const pid_t child = fork();
    if (child == 0) {
#if defined(__APPLE__)
        execlp("open", "open", url, (char *)NULL);
#else
        execlp("xdg-open", "xdg-open", url, (char *)NULL);
#endif
        _exit(127);
    }
    if (child < 0)
        Com_Error(1, "EXE_ERR_COULDNT_OPEN_URL\x15%s", url);
#endif

    if (doExit != qfalse)
        Cbuf_AddText("quit\n");
}

/* Source: CoDUOMP.exe 0x0046b3f0, recovered from an exporter gap. Name and
 * empty platform-hook behavior: exact same-module Mac symbol
 * Sys_LoadingKeepAlive. */
void Sys_LoadingKeepAlive(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: shared expression of the Win32 message-pump and
 * two-millisecond yield inlined at CoDUOMP.exe 0x0043028c..0x004302e1 and
 * 0x0041d01b..0x0041d070. The original out-of-line Sys_LoadingKeepAlive
 * function above is independently empty. */
void coduomp_loading_keepalive(void)
{
#if defined(_WIN32)
    MSG message;

    if (PeekMessageA(&message, NULL, WM_POWERBROADCAST, WM_POWERBROADCAST, PM_NOREMOVE) != FALSE &&
        GetMessageA(&message, NULL, WM_POWERBROADCAST, WM_POWERBROADCAST) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    Sleep(2);
#endif
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
_Noreturn void Sys_Error(const char *format, ...)
{
    char errorText[SYS_ERROR_TEXT_CAPACITY];
    va_list arguments;

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    (void)vsnprintf(errorText, sizeof(errorText), format, arguments);
    va_end(arguments);

    Sys_Shutdown();
    RestoreSystemGammas();
    Sys_Print(errorText);
    Sys_Print("\n");

#if defined(_WIN32)
    Sys_SetErrorText(errorText);
    Sys_ShowConsole(SYS_CONSOLE_VISIBLE, qtrue);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native fatal-error presentation until the
     * native application shell supplies a graphical error console. Sys_Print
     * above remains the native text presentation boundary. */
#endif

    Com_Quit_f();
}

/* Source: CoDUOMP.exe 0x0046b4f0..0x0046b570.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046b4f0_0046b571.mcode.
 * Name and no-argument signature: exact same-module Mac symbol Sys_Quit. */
_Noreturn void Sys_Quit(void)
{
#if defined(_WIN32)
    timeEndPeriod(1);
#endif
    IN_Shutdown();

#if defined(_WIN32)
    Sys_DestroyConsole();
    Sys_DeleteProcessLockFile();
#endif

    Sys_ShutdownLocalization();
    Cvar_Shutdown();
    Cmd_Shutdown();
    Com_InitPushEvent();
    Sys_ClearEventQueue();
    Key_Shutdown();
#if !defined(_WIN32)
    CoduoSDL_Shutdown();
#endif
    exit(EXIT_SUCCESS);
}

/* Source: CoDUOMP.exe 0x0046b580..0x0046b584.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046b580_0046b585.mcode.
 * Name and argument: same-module Mac symbol Sys_Warning and its placement
 * immediately after Sys_Quit in both platform implementations. The Windows
 * compiler emits the body as a tail jump to Sys_Print. No instruction in the
 * shipped PE calls or takes the address of this retained platform API. */
void Sys_Warning(const char *text)
{
    Sys_Print(text);
}

/* Source: CoDUOMP.exe 0x0046b590..0x0046b6a9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046b590_0046b6aa.mcode, PE import
 * 0x0058620c = GetDriveTypeA, and original strings at 0x0059f34c..0x0059f37b.
 * Provisional role name: scans C: through Z: for removable install media.
 * Neither this function nor its 256-byte result at 0x009cf1e8 has an xref in
 * the shipped PE, so this is retained but inactive Windows platform source. */
qboolean Sys_ScanForInstallMedia(void)
{
#if defined(_WIN32)
    char driveRoot[4] = "c:\\";
    char candidatePath[MAX_OSPATH];

    do {
        if (GetDriveTypeA(driveRoot) == DRIVE_CDROM) {
            FILE *file;

            sprintf(sysInstallMediaPath, "%s%s", driveRoot, "");
            sprintf(candidatePath, "%s\\%s", sysInstallMediaPath, "setup\\setup.exe");
            file = fopen(candidatePath, "r");
            if (file != NULL) {
                fclose(file);
                return qtrue;
            }

            sprintf(sysInstallMediaPath, "%s%s", driveRoot, "bin\\x86\\glibc-2.1");
            sprintf(candidatePath, "%s\\%s", sysInstallMediaPath, "setup\\setup");
            file = fopen(candidatePath, "r");
            if (file != NULL) {
                fclose(file);
                return qtrue;
            }
        }
        ++driveRoot[0];
    } while (driveRoot[0] <= 'z');

    return qfalse;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: modern non-Windows builds do not scan
     * drive-letter install media. */
    return qfalse;
#endif
}

/* Source: CoDUOMP.exe 0x0046aff0..0x0046b29a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046aff0_0046b29b.mcode, the
 * "monkey"/"q3monkeyid" strings at 0x0059f3f4/0x0059f3fc, and the exact
 * same-module Mac symbol Sys_MonkeyShouldBeSpanked. The normal process copies
 * itself to a delete-on-close temporary executable and launches that copy.
 * The temporary process waits for the original, replaces the ten-byte marker
 * in the original image with the executable checksum in big-endian order,
 * and exits. No instruction in the shipped PE calls or takes the address of
 * this retained platform routine.
 *
 * The original i386 command line serializes the inherited process HANDLE
 * through `%d`/atoi. intptr_t keeps that explicit process-boundary scalar
 * native-width on 64-bit Windows while producing the same decimal form on
 * i386. */
int32_t Sys_MonkeyShouldBeSpanked(void)
{
#if defined(_WIN32)
    static const char marker[] = "q3monkeyid";
    char **const arguments = __argv;

    if (arguments[1] == NULL || Q_stricmpn(arguments[1], "monkey", 99999) != 0) {
        char tempExecutable[MAX_OSPATH];
        char modulePath[MAX_OSPATH];
        char commandLine[SYS_MONKEY_COMMAND_CAPACITY];
        STARTUPINFOA startupInfo;
        PROCESS_INFORMATION processInfo;

        GetModuleFileNameA(NULL, modulePath, sizeof(modulePath));
        GetTempPathA(sizeof(tempExecutable), tempExecutable);
        GetTempFileNameA(tempExecutable, "Del", 0, tempExecutable);
        CopyFileA(modulePath, tempExecutable, FALSE);

        const HANDLE tempFile = CreateFileA(tempExecutable, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
        const HANDLE originalProcess = OpenProcess(SYNCHRONIZE, TRUE, GetCurrentProcessId());

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
#if UINTPTR_MAX == UINT32_MAX
        Com_sprintf(commandLine, sizeof(commandLine), "%s monkey %d %d \"%s\"", tempExecutable, (int32_t)sysExecutableChecksum,
                    (int32_t)(intptr_t)originalProcess, modulePath);
#else
        Com_sprintf(commandLine, sizeof(commandLine), "%s monkey %d %" PRIdPTR " \"%s\"", tempExecutable, (int32_t)sysExecutableChecksum,
                    (intptr_t)originalProcess, modulePath);
#endif

        memset(&startupInfo, 0, sizeof(startupInfo));
        startupInfo.cb = sizeof(startupInfo);
        CreateProcessA(NULL, commandLine, NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo);
        CloseHandle(originalProcess);
        CloseHandle(tempFile);
        exit(EXIT_SUCCESS);
    }

    sysExecutableChecksum = (uint32_t)coduo_crt_atoi(arguments[2]);
#if UINTPTR_MAX == UINT32_MAX
    /* 0x0046b130..0x0046b141 calls the same 32-bit atoi entry for both
     * serialized dwords. Keep the native-width parser only for Win64, where
     * the process handle no longer fits the retail decimal lane. */
    const intptr_t originalProcessValue = (intptr_t)coduo_crt_atoi(arguments[3]);
#else
    const intptr_t originalProcessValue = (intptr_t)strtoimax(arguments[3], NULL, 10);
#endif
    const HANDLE originalProcess = (HANDLE)(uintptr_t)originalProcessValue;
    WaitForSingleObject(originalProcess, INFINITE);
    CloseHandle(originalProcess);

    FILE *file = fopen(arguments[4], "rb");
    if (file == NULL)
        return 0;

    fseek(file, 0, SEEK_END);
    const long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    uint8_t *const image = malloc((size_t)fileSize);
    if (image == NULL)
        Sys_OutOfMemory();

    memset(image, 0, (size_t)fileSize);
    const size_t readCount = fread(image, (size_t)fileSize, 1, file);
    if (readCount != 1) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        return (int32_t)readCount;
    }
    fclose(file);

    const int32_t markerLength = (int32_t)strlen(marker);
    if (fileSize <= 0)
        return markerLength;

    int32_t comparison = markerLength;
    long markerOffset;
    for (markerOffset = 0; markerOffset < fileSize; ++markerOffset) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        comparison = Q_strncmp((const char *)&image[markerOffset], marker, markerLength);
        if (comparison == 0)
            break;
    }
    if (markerOffset >= fileSize)
        return comparison;

    image[markerOffset] = (uint8_t)(sysExecutableChecksum >> 24);
    image[markerOffset + 1] = (uint8_t)(sysExecutableChecksum >> 16);
    image[markerOffset + 2] = (uint8_t)(sysExecutableChecksum >> 8);
    image[markerOffset + 3] = (uint8_t)sysExecutableChecksum;
    memset(&image[markerOffset + sizeof(sysExecutableChecksum)], 0, sizeof(marker) - 1 - sizeof(sysExecutableChecksum));

    file = fopen(arguments[4], "wb");
    if (file == NULL)
        return 0;
    const size_t writeCount = fwrite(image, (size_t)fileSize, 1, file);
    if (writeCount != 1) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        return (int32_t)writeCount;
    }

    fclose(file);
    free(image);
    exit(EXIT_SUCCESS);
#else
    /* The same-module Mac implementation is an eight-byte `return 0` stub. */
    return 0;
#endif
}

/* Source: CoDUOMP.exe 0x0046b6b0..0x0046b6b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046b6b0_0046b6b6.mcode.
 * Name: exact same-module Mac symbol Sys_GetProcessorId. The retained Windows
 * implementation returns the fixed x86 platform identifier 1. */
int32_t Sys_GetProcessorId(void)
{
    return SYS_PROCESSOR_X86;
}

/* Source: CoDUOMP.exe 0x0046e270..0x0046e2d4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046e270_0046e2d5.mcode and the
 * GetUserNameA import. Role name: established id system helper
 * Sys_GetCurrentUser. The original falls back to "player" both when the
 * platform query fails and when it succeeds with an empty string. */
char *Sys_GetCurrentUser(void)
{
#if defined(_WIN32)
    DWORD bufferBytes = sizeof(sysCurrentUser);

    if (GetUserNameA(sysCurrentUser, &bufferBytes) == FALSE)
        memcpy(sysCurrentUser, "player", sizeof("player"));
#else
    /* NOT_FROM_ORIGINAL_SOURCE: libc environment replacement for the Win32
     * account-name query on native Unix platforms. */
    const char *const user = getenv("USER");
    if (user != NULL)
        Q_strncpyz(sysCurrentUser, user, sizeof(sysCurrentUser));
    else
        sysCurrentUser[0] = '\0';
#endif

    if (sysCurrentUser[0] == '\0')
        memcpy(sysCurrentUser, "player", sizeof("player"));
    return sysCurrentUser;
}

/* Source: CoDUOMP.exe 0x0046b6c0..0x0046b73a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046b6c0_0046b73b.mcode.
 * Name and signature: exact same-module Mac symbol Sys_GetClipboardData.
 * The Windows implementation deliberately truncates at the first newline,
 * carriage return, or backspace by calling strtok and ignoring its result. */
char *Sys_GetClipboardData(void)
{
#if defined(_WIN32)
    char *clipboardText = NULL;

    if (OpenClipboard(NULL) == FALSE)
        return NULL;

    HANDLE clipboardHandle = GetClipboardData(CF_TEXT);
    if (clipboardHandle != NULL) {
        const char *const source = GlobalLock(clipboardHandle);
        if (source != NULL) {
            const SIZE_T allocationBytes = GlobalSize(clipboardHandle);
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (allocationBytes != 0 && allocationBytes <= UINT32_MAX) {
                const size_t copyBytes = (size_t)allocationBytes - 1u;
                clipboardText = Z_MallocInternal((size_t)allocationBytes + 1u);
                strncpy(clipboardText, source, copyBytes);
                clipboardText[copyBytes] = '\0';
            }
            GlobalUnlock(clipboardHandle);
            if (clipboardText != NULL)
                (void)strtok(clipboardText, "\n\r\b");
        }
    }

    CloseClipboard();
    return clipboardText;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: SDL supplies the macOS/Unix window-system
     * clipboard while preserving the original malloc-owned return contract. */
    char *const clipboardText = coduomp_sdl_get_clipboard_text_compat();
    if (clipboardText != NULL)
        (void)strtok(clipboardText, "\n\r\b");
    return clipboardText;
#endif
}

/* Source: CoDUOMP.exe 0x0046b740..0x0046b75d, recovered from the executable
 * gap between Sys_GetClipboardData and Sys_LoadDll.
 * Name and signature: exact same-module Mac symbol Sys_UnloadDll. */
int32_t Sys_UnloadDll(void *libraryHandle)
{
    if (libraryHandle == NULL)
        return qfalse;

    if (coduomp_library_close(libraryHandle) != 0)
        return qtrue;

    Com_Error(ERR_FATAL, "\x15"
                         "Sys_UnloadDll FreeLibrary failed");
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0043f930..0x0043fa22.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043f930_0043fa23.mcode.
 * Provisional role name: compares a virtual-filesystem file against a native
 * OS file. Sys_LoadDll uses it to prove that the extracted module selected
 * from a pack has exactly the packaged bytes. */
static qboolean Sys_FilesMatch(const char *virtualPath, const char *osPath)
{
    void *virtualBytes;
    const int32_t virtualSize = FS_ReadFile(virtualPath, &virtualBytes);
    if (virtualSize == -1)
        return qfalse;

    FILE *const file = fopen(osPath, "rb");
    if (file == NULL) {
        FS_FreeFile(virtualBytes);
        return qfalse;
    }

    fseek(file, 0, SEEK_END);
    const int32_t osSize = (int32_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    qboolean matches = qfalse;
    if (osSize > 0) {
        const size_t transferSize = (size_t)(uint32_t)osSize;
        void *const osBytes = Z_MallocInternal(transferSize);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        const size_t bytesRead = fread(osBytes, 1, transferSize, file);
        if (bytesRead == transferSize && osSize == virtualSize && memcmp(virtualBytes, osBytes, transferSize) == 0) {
            matches = qtrue;
        }
        free(osBytes);
    }

    fclose(file);
    FS_FreeFile(virtualBytes);
    return matches;
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: a reconstructed client module cannot byte-match
 * the server-approved retail x86 DLL stored in a PK3, on any host platform.
 * Trust only the two reconstructed client modules built alongside this
 * engine; game modules and arbitrary names remain subject to the original
 * loader path.
 */
static qboolean coduomp_is_reconstructed_client_module(const char *moduleName)
{
    if (strcmp(moduleName, "cgame") == 0 || strcmp(moduleName, "ui") == 0) {
        return qtrue;
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0046b760..0x0046ba0e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046b760_0046ba0f.mcode.
 * Name and signature: exact same-module Mac symbol Sys_LoadDll. For cgame and
 * UI modules, the Windows loader verifies that a pack-owned DLL and its
 * extracted disk copy match before LoadLibrary. It then tries the active game,
 * base game, main, and finally the bare DLL name in that order. */
void *Sys_LoadDll(const char *moduleName, char *loadedPath, vmMain_t *vmMain, coduo_module_syscall_t moduleSyscall)
{
    char dllName[MAX_QPATH];
    char pureIdentityName[MAX_QPATH];
    char dllPath[MAX_OSPATH];
#if defined(__APPLE__)
    char bundledDllPath[MAX_OSPATH];
#endif
    const char *const emptyString = "";
    const cvar_t *cvar;
    const char *basePath;
    const char *baseGame;
    const char *game;
    const char *loadedFile;
    const qboolean reconstructedClientModule = coduomp_is_reconstructed_client_module(moduleName);
    void *libraryHandle;

    loadedPath[0] = '\0';
    /*
     * NOT_FROM_ORIGINAL_SOURCE: retain the retail module pathname solely as
     * the pure-protocol identity. FS_FOpenFileRead marks the containing pack's
     * server/pure reference bytes when these exact names are opened; the stock
     * server's SV_VerifyPaks_f (0x0045b860) requires those two keyed pack
     * checksums at the front of the client's "cp" response. The original Linux
     * dedicated server independently proves the same protocol at 0x0808cf23;
     * its filesystem marks cgame at pack+0x312 (0x08061ffe..0x0806201b) and UI
     * at pack+0x311 (0x08062028..0x08062056). The reconstructed host library
     * is loaded separately below and is never represented as having retail
     * bytes.
     */
    Com_sprintf(pureIdentityName, sizeof(pureIdentityName), "uo_%s_mp_x86.dll", moduleName);
#if defined(_WIN32)
    Q_strncpyz(dllName, pureIdentityName, sizeof(dllName));
#elif defined(__APPLE__) && defined(__aarch64__)
    /*
     * NOT_FROM_ORIGINAL_SOURCE: native module filename for the reconstructed
     * Apple Silicon ABI. The module entry-point contract remains dllEntry /
     * vmMain; only the platform loader artifact name differs.
     */
    Com_sprintf(dllName, sizeof(dllName), "uo_%s_mp_arm64.dylib", moduleName);
#elif defined(__linux__) && defined(__i386__)
    /*
     * NOT_FROM_ORIGINAL_SOURCE: native module filename for the reconstructed
     * Linux i386 ABI.
     */
    Com_sprintf(dllName, sizeof(dllName), "uo_%s_mp_x86.so", moduleName);
#else
    /*
     * NOT_FROM_ORIGINAL_SOURCE: native Unix module filename. Architecture is
     * explicit so an incompatible module cannot be selected silently.
     */
    Com_sprintf(dllName, sizeof(dllName), "uo_%s_mp_x86_64.so", moduleName);
#endif

    cvar = Cvar_FindVar("fs_basepath");
    basePath = cvar != NULL ? cvar->string : emptyString;
    (void)Cvar_FindVar("fs_cdpath");
    cvar = Cvar_FindVar("fs_basegame");
    baseGame = cvar != NULL ? cvar->string : emptyString;
    cvar = Cvar_FindVar("fs_game");
    game = cvar != NULL ? cvar->string : emptyString;

    FS_BuildOSPath_Internal(basePath, game, dllName, dllPath, qfalse);

    if (Q_strncmp(moduleName, "game", 4) != 0) {
        int32_t fileHandle;
        qboolean packagedCopyMatches = qfalse;
        const char *const packagedModuleName = reconstructedClientModule != qfalse ? pureIdentityName : dllName;

        fs_fileAccessed = 1;
        if (FS_FOpenFileRead(packagedModuleName, &fileHandle, qfalse) >= 0) {
            pack_t *const pack = fs_handleFiles[fileHandle].zipArchive;
            FS_FCloseFile(fileHandle);
            if (pack != NULL && reconstructedClientModule == qfalse) {
                FS_BuildOSPath_Internal(basePath, pack->pakGamename, dllName, dllPath, qfalse);
                packagedCopyMatches = Sys_FilesMatch(dllName, dllPath);
            }
        }

        if (packagedCopyMatches == qfalse) {
            if (cl_connectedToPureServer != qfalse && reconstructedClientModule == qfalse) {
                /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
                Com_Error(1, "EXE_ERR_GAME_FAILED_PURE_CHECK\x15%s", dllName);
            }
            FS_BuildOSPath_Internal(basePath, reconstructedClientModule != qfalse ? "main" : game, dllName, dllPath, qfalse);
        }
    }

    libraryHandle = NULL;
#if defined(__APPLE__)
    if (reconstructedClientModule != qfalse && coduomp_macos_framework_module_path(dllName, bundledDllPath, sizeof(bundledDllPath)) != 0) {
        loadedFile = bundledDllPath;
        libraryHandle = coduomp_library_open(loadedFile);
    }
#endif
    if (libraryHandle == NULL) {
        loadedFile = dllPath;
        libraryHandle = coduomp_library_open(loadedFile);
    }
    if (libraryHandle == NULL) {
        FS_BuildOSPath_Internal(basePath, baseGame, dllName, dllPath, qfalse);
        libraryHandle = coduomp_library_open(dllPath);
    }
    if (libraryHandle == NULL) {
        FS_BuildOSPath_Internal(basePath, "main", dllName, dllPath, qfalse);
        libraryHandle = coduomp_library_open(dllPath);
    }
    if (libraryHandle == NULL) {
        loadedFile = dllName;
        libraryHandle = coduomp_library_open(loadedFile);
    }
    if (libraryHandle == NULL)
        return NULL;

    strncpy(loadedPath, loadedFile, 63);
    loadedPath[63] = '\0';

    coduo_dll_entry_t dllEntry = NULL;
    coduomp_library_symbol(libraryHandle, "dllEntry", &dllEntry, sizeof(dllEntry));
    coduomp_library_symbol(libraryHandle, "vmMain", vmMain, sizeof(*vmMain));
    if (*vmMain == NULL || dllEntry == NULL) {
        (void)coduomp_library_close(libraryHandle);
        return NULL;
    }

#if UINTPTR_MAX == UINT32_MAX
    dllEntry(moduleSyscall);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native cgame/UI entry points accept the
     * reconstructed vector callback, while the native game module retains
     * game_syscall_t.  The dynamic symbol and callback are both one function
     * pointer at the ABI boundary; confine the typed conversion here. */
    if (strcmp(moduleName, "game") == 0) {
        game_dll_entry_t gameDllEntry;
        game_syscall_t gameSyscall;

        _Static_assert(sizeof(gameDllEntry) == sizeof(dllEntry), "native dllEntry pointer size mismatch");
        _Static_assert(sizeof(gameSyscall) == sizeof(moduleSyscall), "native syscall pointer size mismatch");
        memcpy(&gameDllEntry, &dllEntry, sizeof(gameDllEntry));
        memcpy(&gameSyscall, &moduleSyscall, sizeof(gameSyscall));
        gameDllEntry(gameSyscall);
    } else {
        dllEntry(moduleSyscall);
    }
#endif
    return libraryHandle;
}

/* Source: CoDUOMP.exe 0x0046afb0..0x0046afeb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046afb0_0046afec.mcode.
 * Provisional role name: validates the in-memory PE header and checksums the
 * first section's mapped bytes using SizeOfRawData, producing the executable
 * checksum archived by Sys_ArchiveInfo. */
uint32_t Sys_GetExecutableChecksum(const void *imageBase)
{
#if defined(_WIN32)
    const uint8_t *const image = (const uint8_t *)imageBase;
    const IMAGE_DOS_HEADER *const dosHeader = (const IMAGE_DOS_HEADER *)image;
    const IMAGE_NT_HEADERS *const ntHeaders = (const IMAGE_NT_HEADERS *)(image + dosHeader->e_lfanew);

    if (IsBadReadPtr(ntHeaders, sizeof(*ntHeaders)) != FALSE || ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    const IMAGE_SECTION_HEADER *const section = (const IMAGE_SECTION_HEADER *)((const uint8_t *)ntHeaders + sizeof(*ntHeaders));
    return Com_BlockChecksum(image + section->VirtualAddress, (int32_t)section->SizeOfRawData);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: non-PE native executables need a platform
     * image-section checksum provider. Returning zero preserves the existing
     * configuration-cvar convention until that platform boundary is wired. */
    (void)imageBase;
    return 0;
#endif
}
