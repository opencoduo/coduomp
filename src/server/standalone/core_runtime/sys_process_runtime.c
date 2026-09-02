#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#endif

#include "core_runtime_private.h"

enum {
    SYS_CWD_GETCWD_LIMIT = MAX_OSPATH - 1,
    SYS_OPENURL_SCRIPT_NAME_SIZE = 20,
    SYS_ACCESS_EXECUTE = 1,
    SYS_HOME_DIR_MODE = 0777,
    SYS_MAIN_LOOP_SLEEP_USECS = 5000,
    SYS_LISTFILES_MAX_COUNT = 4095,
    SYS_LISTFILES_STACK_CAPACITY = SYS_LISTFILES_MAX_COUNT + 1,
    /* Keep recursive host-directory walking independently stack-bounded even
     * though maintained path scratch storage is larger than retail. */
    SYS_LISTFILES_MAX_RECURSION_DEPTH = 128,
    SYS_PROCESSOR_X86 = 1,
#if defined(_WIN32)
    /* Retail Win32 startup selects x87 double precision (_PC_53). */
    SYS_FRAME_X87_CONTROL_WORD = 0x127f
#else
    /* The recovered Linux frame loop reloads x87 extended precision. */
    SYS_FRAME_X87_CONTROL_WORD = 0x137f
#endif
};

#define SYS_PROCESS_HAS_X87_INLINE_ASM CODUO_ENGINE_HAS_X87_INLINE_ASM

#define SYS_VERSION_BANNER "Linux Quake3 Dedicated Server [%s %s]\n"
#define SYS_VERSION_BUILD_DATE "Feb 10 2005"
#define SYS_VERSION_BUILD_TIME "15:44:04"

static char sys_cwd[MAX_OSPATH];
static char sys_installPath[MAX_OSPATH];
static char sys_basePath[MAX_OSPATH];
static char sys_homePath[MAX_OSPATH];
static const char sys_emptyPath[] = "";
#if !defined(_WIN32)
static uid_t sys_effectiveUid;
#endif
#if SYS_PROCESS_HAS_X87_INLINE_ASM
static const uint16_t sys_frameX87ControlWord = SYS_FRAME_X87_CONTROL_WORD;
#endif

void Sys_SetDefaultCDPath(const char *path);

int32_t Sys_GetProcessorId(void)
{
    return SYS_PROCESSOR_X86;
}

void Sys_DestroySplashWindow(void)
{
}

qboolean Sys_LowPhysicalMemory(void)
{
    return qfalse;
}

void Sys_Print(const char *message)
{
    if (sys_ttyConsoleActive != 0) {
        Sys_TTYHideInputLine();
    }

    fputs(message, stderr);

    if (sys_ttyConsoleActive != 0) {
        Sys_TTYShowInputLine();
    }
}

void Sys_CheckCrashOrRerun(void)
{
#if SYS_PROCESS_HAS_X87_INLINE_ASM
    __asm__ __volatile__("fldcw %0" : : "m"(sys_frameX87ControlWord) : "memory");
#elif defined(CODUO_ENGINE_ALLOW_NON_X87_FLOAT)
#else
#error "Sys_CheckCrashOrRerun requires x87 inline assembly for original FPU control word"
#endif
}

void Sys_PrintBinVersion(const char *localInstall)
{
    const char *separator = "==============================================================";

    fprintf(stdout, "\n\n%s\n", separator);
    fprintf(stdout, SYS_VERSION_BANNER, SYS_VERSION_BUILD_DATE, SYS_VERSION_BUILD_TIME);
    fprintf(stdout, " local install: %s\n", localInstall);
    fprintf(stdout, "%s\n\n", separator);
}

void Sys_Chmod(const char *path, uint32_t mode)
{
    struct stat statbuf;

    if (Sys_Stat(path, &statbuf) == 0) {
        mode_t newMode = statbuf.st_mode | mode;

        if (chmod(path, newMode) != 0) {
            Com_Printf("chmod('%s', %d) failed: errno %d\n", path, newMode, errno);
        }
        Com_DPrintf("chmod +%d '%s'\n", mode, path);
        return;
    }

    Com_Printf("stat('%s')  failed: errno %d\n", path, errno);
}

void Sys_StartProcessNow(const char *command)
{
#if defined(_WIN32)
    /* NOT_FROM_ORIGINAL_SOURCE: Win32 process-launch adaptation for MinGW. */
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;
    char commandLine[SYS_DELAYED_PROCESS_COMMAND_SIZE];

    memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    memset(&processInfo, 0, sizeof(processInfo));
    Q_strncpyz(commandLine, command, sizeof(commandLine));

    if (CreateProcessA(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo) != 0) {
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
    }
#else
    pid_t pid = fork();

    if (pid == -1 || pid != 0) {
        return;
    }

    if (strchr(command, ' ') == NULL) {
        execl(command, command, NULL);
    } else {
        system(command);
    }

    _exit(0);
#endif
}

void Sys_StartProcess(const char *command, qboolean delayUntilFinalExit)
{
    if (delayUntilFinalExit == qfalse) {
        Com_DPrintf("Sys_StartProcess %s\n", command);
        Sys_StartProcessNow(command);
        return;
    }

    Com_DPrintf("Sys_StartProcess %s (delaying to final exit)\n", command);
    Q_strncpyz(sys_delayedProcessCommand, command, SYS_DELAYED_PROCESS_COMMAND_SIZE);
    Cbuf_ExecuteText(EXEC_APPEND, "quit\n");
}

const char *Sys_Cwd(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (getcwd(sys_cwd, SYS_CWD_GETCWD_LIMIT) == NULL) {
        sys_cwd[0] = '.';
        sys_cwd[1] = '\0';
    }
    sys_cwd[SYS_CWD_GETCWD_LIMIT] = '\0';
    return sys_cwd;
}

void Sys_Mkdir(char *path)
{
#if defined(_WIN32)
    _mkdir(path);
#else
    mkdir(path, SYS_HOME_DIR_MODE);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: carries recursion depth without changing the
 * original Sys_ListFilteredFiles interface. */
static void coduo_compat_list_filtered_files_recursive(const char *directory, const char *subdirectory, const char *filter, char **list,
                                                       int *numfiles, int32_t recursionDepth)
{
    char fullDirectory[MAX_OSPATH];
    char childPath[MAX_OSPATH];
    char relativePath[MAX_OSPATH];
    struct stat statbuf;

    if (*numfiles >= SYS_LISTFILES_MAX_COUNT) {
        return;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (recursionDepth > SYS_LISTFILES_MAX_RECURSION_DEPTH) {
        Com_Printf("WARNING: Sys_ListFilteredFiles: recursion limit reached below '%s/%s'\n", directory, subdirectory);
        return;
    }

    if (subdirectory[0] == '\0') {
        Com_sprintf(fullDirectory, sizeof(fullDirectory), "%s", directory);
    } else {
        Com_sprintf(fullDirectory, sizeof(fullDirectory), "%s/%s", directory, subdirectory);
    }

    DIR *dir = opendir(fullDirectory);
    if (dir == NULL) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        Com_sprintf(childPath, sizeof(childPath), "%s/%s", fullDirectory, entry->d_name);
        if (Sys_Stat(childPath, &statbuf) == -1) {
            continue;
        }

        if ((statbuf.st_mode & S_IFDIR) != 0 && Q_stricmp(entry->d_name, ".") != 0 && Q_stricmp(entry->d_name, "..") != 0) {
            if (subdirectory[0] == '\0') {
                Com_sprintf(relativePath, sizeof(relativePath), "%s", entry->d_name);
            } else {
                Com_sprintf(relativePath, sizeof(relativePath), "%s/%s", subdirectory, entry->d_name);
            }
            coduo_compat_list_filtered_files_recursive(directory, relativePath, filter, list, numfiles, recursionDepth + 1);
        }

        if (*numfiles >= SYS_LISTFILES_MAX_COUNT) {
            break;
        }

        Com_sprintf(relativePath, sizeof(relativePath), "%s/%s", subdirectory, entry->d_name);
        if (Com_FilterPath(filter, relativePath, qfalse) != qfalse) {
            list[*numfiles] = CopyStringInternal(relativePath);
            ++*numfiles;
        }
    }

    closedir(dir);
}

void Sys_ListFilteredFiles(const char *directory, const char *subdirectory, const char *filter, char **list, int *numfiles)
{
    coduo_compat_list_filtered_files_recursive(directory, subdirectory, filter, list, numfiles, 0);
}

char **Sys_ListFiles(const char *directory, const char *extension, const char *filter, int *numfiles, qboolean wantsubs)
{
    char *list[SYS_LISTFILES_STACK_CAPACITY];
    int count = 0;

    if (filter != NULL) {
        Sys_ListFilteredFiles(directory, "", filter, list, &count);
    } else {
        qboolean listDirectories = wantsubs;

        if (extension == NULL) {
            extension = "";
        }
        if (extension[0] == '/' && extension[1] == '\0') {
            extension = "";
            listDirectories = qtrue;
        }

        DIR *dir = opendir(directory);
        if (dir == NULL) {
            *numfiles = 0;
            return NULL;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            char path[MAX_OSPATH];
            struct stat statbuf;

            Com_sprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
            if (Sys_Stat(path, &statbuf) == -1) {
                continue;
            }

            if ((listDirectories == qfalse && (statbuf.st_mode & S_IFDIR) != 0) ||
                (listDirectories != qfalse && (statbuf.st_mode & S_IFDIR) == 0)) {
                continue;
            }

            if (extension[0] != '\0') {
                size_t nameLength = strlen(entry->d_name);
                size_t extensionLength = strlen(extension);

                if (nameLength < extensionLength || Q_stricmp(entry->d_name + nameLength - extensionLength, extension) != 0) {
                    continue;
                }
            }

            if (count == SYS_LISTFILES_MAX_COUNT) {
                break;
            }
            list[count] = CopyStringInternal(entry->d_name);
            ++count;
        }

        closedir(dir);
    }

    list[count] = NULL;
    *numfiles = count;
    if (count == 0) {
        return NULL;
    }

    char **result = Z_MallocInternal(((size_t)count + 1U) * sizeof(result[0]));
    for (int index = 0; index < count; ++index) {
        result[index] = list[index];
    }
    result[count] = NULL;

    return result;
}

void Sys_OpenURL(const char *url, qboolean delayUntilFinalExit)
{
    char command[MAX_STRING_CHARS];
#if !defined(_WIN32)
    char scriptPath[MAX_OSPATH];
    char scriptName[SYS_OPENURL_SCRIPT_NAME_SIZE];
#endif

    Com_Printf("Sys_OpenURL %s\n", url);
#if defined(_WIN32)
    Com_sprintf(command, sizeof(command), "cmd.exe /c start \"\" \"%s\"", url);
    Sys_StartProcess(command, delayUntilFinalExit);
#else
    Q_strncpyz(scriptName, "openurl.sh", SYS_OPENURL_SCRIPT_NAME_SIZE);

    Com_sprintf(scriptPath, sizeof(scriptPath), "%s/%s", Sys_Cwd(), scriptName);
    if (access(scriptPath, SYS_ACCESS_EXECUTE) == -1) {
        Com_DPrintf("%s not found\n", scriptPath);
        Com_sprintf(scriptPath, sizeof(scriptPath), "%s/%s", Cvar_VariableString("fs_homepath"), scriptName);
        if (access(scriptPath, SYS_ACCESS_EXECUTE) == -1) {
            Com_DPrintf("%s not found\n", scriptPath);
            Com_sprintf(scriptPath, sizeof(scriptPath), "%s/%s", Cvar_VariableString("fs_basepath"), scriptName);
            if (access(scriptPath, SYS_ACCESS_EXECUTE) == -1) {
                Com_DPrintf("%s not found\n", scriptPath);
                Com_Printf("Can't find script '%s' to open requested URL (use +set "
                           "developer 1 for more verbosity)\n",
                           scriptName);
                return;
            }
        }
    }

    Com_DPrintf("URL script: %s\n", scriptPath);
    /* NOT_FROM_ORIGINAL_SOURCE: the URL must be representable by the platform
     * launcher's single-argument quoting grammar. */
    if (strchr(url, '\'') != NULL) {
        Com_Printf("Sys_OpenURL: URL contains an unsafe character\n");
        return;
    }
    Com_sprintf(command, sizeof(command), "%s '%s' &", scriptPath, url);
    Sys_StartProcess(command, delayUntilFinalExit);
#endif
}

void Sys_ParseArgs(int argc, char **argv)
{
    if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        Sys_PrintBinVersion(argv[0]);
        Sys_Exit(0);
    }
}

int main(int argc, char **argv)
{
#if !defined(_WIN32)
    sys_effectiveUid = geteuid();
    seteuid(getuid());
#endif
    Sys_ParseArgs(argc, argv);
    Sys_SetDefaultCDPath("");
#if defined(_WIN32)
    Sys_CheckCrashOrRerun();
#endif

    int32_t commandLineSize = 1;
    for (int32_t index = 1; index < argc; ++index) {
        commandLineSize += (int32_t)strlen(argv[index]) + 1;
    }

    char commandLine[commandLineSize];
    commandLine[0] = '\0';
    for (int32_t index = 1; index < argc; ++index) {
        if (index > 1) {
            strcat(commandLine, " ");
        }
        strcat(commandLine, argv[index]);
    }

    memset(sys_eventQueue, 0, sizeof(sys_eventQueue));
    memset(sys_packetBuffer, 0, sizeof(sys_packetBuffer));

    Com_Init(commandLine);

    NET_Init();
    Sys_InitTerminalConsole();

#if !defined(_WIN32)
    int flags = fcntl(SYS_STDIN_FILE_DESCRIPTOR, SYS_F_GETFL_COMMAND, SYS_F_GETFL_UNUSED_ARGUMENT);
    fcntl(SYS_STDIN_FILE_DESCRIPTOR, SYS_F_SETFL_COMMAND, flags | SYS_LINUX_O_NONBLOCK);
#endif

    PB_StartServer();
    for (;;) {
        Sys_CheckCrashOrRerun();
#if defined(_WIN32)
        Sleep(SYS_MAIN_LOOP_SLEEP_USECS / 1000);
#else
        usleep(SYS_MAIN_LOOP_SLEEP_USECS);
#endif
        Com_Frame();
        PB_RunServerFrame();
    }

    return 0;
}

void Sys_FreeFileList(char **strings)
{
    if (strings == NULL) {
        return;
    }

    for (int32_t index = 0; strings[index] != NULL; ++index) {
        Z_FreeInternal(strings[index]);
    }
    Z_FreeInternal(strings);
}

void Sys_SetDefaultCDPath(const char *path)
{
    Q_strncpyz(sys_installPath, path, sizeof(sys_installPath));
}

const char *Sys_DefaultCDPath(void)
{
    return sys_installPath;
}

const char *Sys_DefaultBasePath(void)
{
    if (sys_basePath[0] == '\0') {
        return Sys_Cwd();
    }

    return sys_basePath;
}

void Sys_SetDefaultBasePath(const char *path)
{
    Q_strncpyz(sys_basePath, path, sizeof(sys_basePath));
}

const char *Sys_DefaultInstallPath(void)
{
    if (sys_basePath[0] == '\0') {
        return Sys_Cwd();
    }

    return sys_basePath;
}

void Sys_SetDefaultHomePath(const char *path)
{
    Q_strncpyz(sys_homePath, path, sizeof(sys_homePath));
}

const char *Sys_DefaultHomePath(void)
{
    const char *home;

    if (sys_homePath[0] != '\0') {
        return sys_homePath;
    }

#if defined(_WIN32)
    home = getenv("APPDATA");
    if (home == NULL) {
        home = getenv("USERPROFILE");
    }
#else
    home = getenv("HOME");
#endif
    if (home == NULL) {
        return sys_emptyPath;
    }

    Q_strncpyz(sys_homePath, home, sizeof(sys_homePath));
#if defined(_WIN32)
    Q_strcat(sys_homePath, sizeof(sys_homePath), "/Call of Duty");
    if (_mkdir(sys_homePath) != 0 && errno != EEXIST) {
#else
    Q_strcat(sys_homePath, sizeof(sys_homePath), "/.callofduty");
    if (mkdir(sys_homePath, SYS_HOME_DIR_MODE) != 0 && errno != EEXIST) {
#endif
        int *errorLocationForNumber = &errno;
        int32_t errorForText = errno;
        const char *errorText = strerror(errorForText);
        int32_t error = *errorLocationForNumber;

        Sys_Error("Unable to create directory \"%s\", error is %s(%d)\n", sys_homePath, errorText, error);
    }

    return sys_homePath;
}

/*
 * Checked 2026-06-29: these Sys process address-band leaves return 0 in the
 * dedicated binary; no source-level names are proven for the remaining FUN_
 * stubs.
 */
int32_t FUN_080cb21c(void)
{
    return 0;
}

int32_t FUN_080cb226(void)
{
    return 0;
}

/*
 * Mac MP symbols include Sys_ShowConsole; the Linux dedicated build keeps this
 * console visibility hook as a no-op.
 */
void Sys_ShowConsole(int32_t visLevel, qboolean quitOnClose)
{
    (void)visLevel;
    (void)quitOnClose;
}
