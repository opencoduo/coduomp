#ifndef CODUOMP_SYSTEM_PLATFORM_H
#define CODUOMP_SYSTEM_PLATFORM_H

#include "q_shared.h"

/* NOT_FROM_ORIGINAL_SOURCE: identify the platform and executable architecture
 * consistently in the client version text and legacy update query. */
#if defined(_WIN32)
#if defined(_M_IX86) || defined(__i386__)
#define CODUOMP_PLATFORM_TAG "win-x86"
#elif defined(_M_X64) || defined(__x86_64__)
#define CODUOMP_PLATFORM_TAG "win-x64"
#else
#error "Unsupported Windows client architecture"
#endif
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
#define CODUOMP_PLATFORM_TAG "macos-arm64"
#elif defined(__x86_64__)
#define CODUOMP_PLATFORM_TAG "macos-amd64"
#else
#error "Unsupported macOS client architecture"
#endif
#elif defined(__linux__)
#if defined(__i386__)
#define CODUOMP_PLATFORM_TAG "linux-x86"
#elif defined(__x86_64__)
#define CODUOMP_PLATFORM_TAG "linux-x64"
#else
#error "Unsupported Linux client architecture"
#endif
#else
#error "Unsupported client platform"
#endif

extern void *sysSplashWindow;
extern qboolean sysCheckCrashOrRerun;
extern uint32_t sysExecutableChecksum;

void Sys_Init(void);
void Sys_CreateCrashMarker(void);
#if defined(_WIN32)
void coduomp_sys_enable_dpi_awareness(void);
#endif
void Sys_CreateSplashWindow(void);
void Sys_ShowSplashWindow(void);
void Sys_HideSplashWindow(void);
void Sys_DestroySplashWindow(void);
qboolean Sys_FunctionsMatch(const uint8_t *leftFunction,
                            const uint8_t *rightFunction);
uint32_t Sys_FunctionChecksum(const uint8_t *function);
qboolean Sys_CheckCrashOrRerun(void);
uint32_t Sys_GetExecutableChecksum(const void *imageBase);
void Sys_Shutdown(void);
void Sys_UnableToLoadDLLError(void);
void Sys_StartProcess(const char *executableName, qboolean doExit);
void Sys_OpenURL(const char *url, qboolean doExit);
void Sys_Warning(const char *text);
qboolean Sys_ScanForInstallMedia(void);
int32_t Sys_MonkeyShouldBeSpanked(void);
int32_t Sys_GetProcessorId(void);
char *Sys_GetCurrentUser(void);
char *Sys_GetClipboardData(void);
int32_t Sys_UnloadDll(void *libraryHandle);
void Sys_Mkdir(const char *path);
const char *Sys_Cwd(void);
const char *Sys_DefaultCDPath(void);
const char *Sys_DefaultBasePath(void);
const char *Sys_DefaultHomePath(void);
const char *Sys_DefaultInstallPath(void);
void coduomp_loading_keepalive(void);
_Noreturn void Sys_Error(const char *format, ...);
_Noreturn void Sys_Quit(void);

#endif
