#ifndef CODUOMP_Q_SHARED_H
#define CODUOMP_Q_SHARED_H

#ifndef EMULATE_X87
#define EMULATE_X87 0
#endif

#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>

#include "qcommon/huffman.h"
#include "qcommon/msg_delta.h"
#include "client/common/client_common.h"
#include "qcommon/client_state_types.h"
#include "qcommon/collision_map_types.h"
#include "collision/collision_queries.h"
#include "qcommon/com_command_handlers.h"
#include "qcommon/com_config.h"
#include "qcommon/com_frame.h"
#include "qcommon/com_memory.h"
#include "qcommon/com_event_queue.h"
#include "qcommon/com_event_loop.h"
#include "qcommon/com_lifecycle.h"
#include "qcommon/com_parse.h"
#include "qcommon/com_redirect.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/com_startup_commands.h"
#include "qcommon/com_string.h"
#include "qcommon/com_time.h"
#include "qcommon/entity_state_types.h"
#include "filesystem/filesystem.h"
#include "qcommon/filesystem_types.h"
#include "qcommon/game_state_types.h"
#include "qcommon/hunk.h"
#include "qcommon/net_field_types.h"
#include "qcommon/player_state_types.h"
#include "qcommon/q_checksum.h"
#include "qcommon/q_bits.h"
#include "qcommon/q_command.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_cpu.h"
#include "qcommon/q_endian.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_path.h"
#include "qcommon/q_shared_misc.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/qcommon_limits.h"
#include "math/q_math.h"
#include "qcommon/qcommon_runtime_types.h"
#include "qcommon/q_string.h"
#include "qcommon/statmon_types.h"
#include "qcommon/snapshot_types.h"
#include "qcommon/trajectory_types.h"

/* Maintained C headers use the C11 assertion and non-returning spellings.
 * Supply their C++ equivalents when recovered C++ consumes the same headers. */
#ifdef __cplusplus
#define _Static_assert static_assert
#define _Alignof alignof
#define _Noreturn [[noreturn]]
#endif

#if defined(_MSC_VER)
#include <malloc.h>
#define CODUOMP_ALLOCA(size_) _alloca(size_)
#else
#define CODUOMP_ALLOCA(size_) __builtin_alloca(size_)
#endif


typedef enum sysCpuClass_e {
    CPUID_GENERIC           = 0x00,
    CPUID_AXP               = 0x10,
    CPUID_INTEL_UNSUPPORTED = 0x20,
    CPUID_INTEL_PENTIUM     = 0x21,
    CPUID_INTEL_MMX         = 0x22,
    CPUID_INTEL_KATMAI      = 0x23,
    CPUID_AMD_3DNOW         = 0x30
} sysCpuClass_t;

typedef uint8_t byte;

typedef enum cinematic_status_e {
    FMV_IDLE = 0,
    FMV_PLAY = 1,
    FMV_EOF = 2,
    FMV_ID_BLT = 3,
    FMV_ID_IDLE = 4,
    FMV_LOOPED = 5,
    FMV_ID_WAIT = 6
} cinematic_status_t;

/* SEH_LocalizeTextMessage missing-token policy. The names follow the
 * same-family localization interface; the values and behavior are proven by
 * CoDUOMP.exe 0x004706c0..0x00470a90. */
typedef enum msgLocErrType_e {
    LOCMSG_SAFE = 0,
    LOCMSG_NOERR = 1
} msgLocErrType_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Common WinMM-backed millisecond clock. The original compiler inlines this
 * clock in several unrelated subsystems, so its initialization state is not
 * owned by the FX stat-monitor path that first established these globals. */
/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
enum { SYS_PROCESS_LOCK_NAME_CAPACITY = MAX_OSPATH + 2 };

extern qboolean sysMillisecondsInitialized; /* original 0x0389fddc */
extern uint32_t sysMillisecondsBase;         /* original 0x0489bc3c */
extern qboolean sysSseSupported;             /* original 0x0489d460 */
extern double sysCpuFrequencyMHz;             /* original 0x009cf1d8 */
extern int32_t sysPhysicalMemoryMB;           /* original 0x009cf1e0 */
extern int32_t sysVideoMemoryMB;               /* original 0x009cf1e4 */
extern char sysProcessLockFile[SYS_PROCESS_LOCK_NAME_CAPACITY];
                                                /* original 0x009cf1b0 */
extern qboolean fs_fileAccessed;               /* original 0x04935300 */
extern int32_t fs_loadStack;                  /* original 0x0493542c */
extern cvar_t *com_developer;                 /* original 0x04927ea4 */
extern cvar_t *com_developerScript;           /* original 0x0492907c */
extern cvar_t *com_logfile;                   /* original 0x04927ebc */
extern cvar_t *com_statmon;                   /* original 0x04927ea8 */
extern cvar_t *com_viewlog;                   /* original 0x04927eb4 */
extern cvar_t *com_speeds;                    /* original 0x04929064 */
extern cvar_t *com_maxfps;                    /* original 0x0492908c */
extern cvar_t *com_recommendedSet;            /* original 0x04929094 */
extern cvar_t *com_introPlayed;               /* original 0x04929098 */
extern cvar_t *com_animCheck;                 /* original 0x04929078 */
extern cvar_t *com_version;                   /* original 0x04927f64 */
extern cvar_t *com_shortVersion;              /* original 0x04929088 */
extern int32_t com_timeGame;                  /* original 0x0492906c */
extern int32_t com_timeFrontend;              /* original 0x04929070 */
extern int32_t com_timeBackend;               /* original 0x04927ed0 */
extern cvar_t *sv_running;                    /* original 0x04927ed4 */
extern cvar_t *sv_disableClientConsole;       /* original 0x0491cd10 */
extern cvar_t *cl_running;                    /* original 0x04929080 */
extern int32_t com_frameTime;                 /* original 0x04929090 */
extern cvar_t *cl_language;                    /* original 0x04e19994 */
extern qboolean com_errorEntered;              /* original 0x0492909c */
extern errorParm_t com_errorCode;              /* original 0x0098067c */
extern char com_errorMessage[COM_ERROR_MESSAGE_CAPACITY];
                                                /* original 0x04928060 */
extern jmp_buf com_abortFrame;                  /* original 0x00980238 */
extern qboolean scr_updateScreenRecursionGuard; /* original 0x0389fcf4 */
extern int32_t fs_checksumFeed;                 /* original 0x049311e8 */
extern cvar_t *cl_shownet;                    /* original 0x0389fce4 */
extern cvar_t *cl_languagetranslate;           /* original 0x0495819c */
extern cvar_t *cl_languagesavailable;           /* original 0x04dc8830 */
extern cvar_t *cl_languagewarnings;             /* original 0x04e1999c */
extern cvar_t *cl_languagewarningsaserrors;     /* original 0x04958068 */
extern qboolean rendererMultibyteTextEnabled;   /* original 0x009d5fac */
extern statmon_entry_t statmonEntries[STATMON_ENTRY_CAPACITY];
extern int32_t statmonEntryCount;
/* Engine command-token globals are also read directly by the Windows VBO
 * refresh command at 0x004c39e0. Cmd_Argv's bounds behavior is inlined there
 * as an empty-string fallback. */
extern const vec4_t colorBlack;               /* original 0x0058fb98 */
extern const vec4_t colorWhite;               /* original 0x0058fc68 */
uint32_t Sys_Milliseconds(void);
qboolean Com_ConfigureChecksum(void);
void Com_SetRecommended(qboolean restartSound);
char *Sys_DateTimeStamp(void);
int32_t Sys_RoundFloatToInt(float value);
void Sys_SnapVector(vec3_t vector);
qboolean Sys_DetectSSESupport(void);
sysCpuClass_t Sys_DetectCpuClass(void);
int32_t Sys_RoundPositiveFloatToInt(float value);
double Sys_GetCpuFrequencyMHz(void);
int32_t Sys_GetPhysicalMemoryMB(void);
int32_t Sys_GetVideoMemoryMB(void);
char *Sys_GetClipboardData(void);

void Cvar_Set(const char *name, const char *value);
void Cvar_Init(void);
void CL_CharEvent(int32_t character);
void CL_KeyEvent(int32_t key, qboolean down, uint32_t time);
void CL_ClearKeys(void);
int32_t Key_StringToKeynum(const char *string);
const char *Key_KeynumToString(int32_t key, qboolean localized);
qboolean Key_IsDown(int32_t key);
void Key_SetCatcher(int32_t catcher);
int32_t Key_GetCatcher(void);
qboolean Key_GetOverstrikeMode(void);
void Key_SetOverstrikeMode(qboolean enabled);
void Key_SetBinding(int32_t key, const char *binding);
const char *Key_GetBinding(int32_t key);
int32_t PbMaxKeys(void);
void Key_GetBindingBuf(int32_t key, char *buffer, int32_t bufferSize);
void Key_KeynumToStringBuf(int32_t key, char *buffer,
                           int32_t bufferSize);
int32_t Key_GetKey(const char *binding);
void Key_Unbind_f(void);
void Key_Unbindall_f(void);
void Key_Bind_f(void);
void Key_Bindlist_f(void);
void Key_Init(void);
void Key_WriteBindings(int32_t fileHandle);
void Key_ClearStates(void);
void Key_Shutdown(void);
_Noreturn void Com_Error(errorParm_t errorCode, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noreturn))
#endif
    ;
void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
int32_t Com_VPrintf(const char *format, va_list args);
void Com_PrintMessage(int32_t channel, const char *message);
void Com_Init(char *commandLine);
const char *Com_GetBuildVersion(void);
void Com_Frame(void);
void Com_ErrorCleanup(void);
void Com_Restart(void);
void Com_SetErrorMessage(const char *message);
void Com_CleanupSkeletons(void);
void *Com_GetWeaponInfoMemory(int32_t byteCount,
                              int32_t *previousOwner,
                              int32_t callerOwner);
void Com_FreeWeaponInfoMemory(int32_t callerOwner,
                              qboolean preserveAllocation);
void StatMon_Warning(int32_t entryIndex, int32_t durationMsec,
                     const char *shaderName);
void StatMon_GetStatsArray(statmon_entry_t **entries, int32_t *entryCount);
void StatMon_Reset(void);
void Sys_LoadingKeepAlive(void);
void VM_Clear(void);
void Sys_ListFilteredFiles(const char *directory, const char *subdirectory,
                           const char *filter, char **list,
                           int32_t *numFiles);
char **Sys_ListFiles(const char *directory, const char *extension,
                     const char *filter, int32_t *numFiles,
                     qboolean wantDirectories);
int32_t FS_ReadFile(const char *path, void **buffer);
void FS_ResetFiles(void);
int32_t FS_FOpenFileWrite(const char *path);
qboolean FS_FileExists(const char *path);
char **FS_ListFiles(const char *path, const char *extension,
                    int32_t *fileCount);
int32_t FS_FOpenFileRead(const char *path, int32_t *handle,
                         qboolean uniqueFile);
int32_t FS_FOpenFileRead_Internal(const char *path, int32_t *handle,
                                  qboolean uniqueFile, qboolean quiet);
int32_t FS_SV_FOpenFileRead(const char *path, int32_t *handle);
int32_t FS_SV_FOpenFileWrite(const char *path);
qboolean FS_SV_FileExists(const char *path);
void FS_SV_Rename(const char *from, const char *to);
qboolean FS_idPak(const char *path, const char *mainGame,
                  const char *baseGame);
qboolean FS_serverPak(const char *pakName);
qboolean FS_ComparePaks(char *neededPaks, int32_t neededPaksSize,
                        qboolean includeAlternateNames);
const char *FS_ShiftStr(const char *text, int32_t shift);
int32_t FS_FileIsInPAK(const char *path, int32_t *checksumOut);
qboolean CL_WWWBadChecksum(const char *pakName);
const char *FS_LoadedPakPureChecksums(void);
const char *FS_LoadedPakChecksums(void);
const char *FS_LoadedPakNames(void);
const char *FS_ReferencedPakChecksums(void);
const char *FS_ReferencedPakNames(void);
void FS_Shutdown(qboolean closeFiles);
void FS_ClearPakReferences(qboolean preserveGeneralAndGameReferences);
qboolean FS_ConditionalRestart(int32_t checksumFeed);
void FS_Restart(int32_t checksumFeed);
void FS_RefreshLookupCache(void);
void SEH_UpdateLanguageInfo(void);
void SEH_InitLanguage(void);
int32_t SEH_UpdateCurrentLanguage(int32_t languageIndex);
qboolean SEH_VerifyLanguageSelection(int32_t languageIndex);
char *FS_ShortOSFilePath(const char *filename);
int32_t FS_FOpenFileByMode(const char *filename, int32_t *handle,
                           fsMode_t mode);
int32_t FS_GetFileList(const char *path, const char *extension,
                       char *listBuffer, int32_t bufferSize);
void Cvar_CommandCompletion(name_completion_callback_t callback);
int32_t FS_Read(void *buffer, int32_t byteCount, int32_t handle);
int32_t FS_Write(const void *buffer, int32_t byteCount, int32_t handle);
void FS_Rename(const char *from, const char *to);
void FS_WriteFile(const char *filename, const void *buffer,
                  int32_t byteCount);
void FS_FCloseFile(int32_t handle);
int32_t FS_Seek(int32_t handle, int32_t offset, int32_t origin);
int32_t FS_FTell(int32_t handle);
void FS_Flush(int32_t handle);
void FS_Printf(int32_t fileHandle, const char *format, ...);
void MSS_EndRawSamples(void);
int32_t MSS_RawSamplesTime(void);
void MSS_RawSamples(int32_t sampleFrameCount, int32_t sampleRate,
                    int32_t sampleWidthBytes, int32_t channelCount,
                    const void *sampleData);
enum {
    MSS_STOP_ALL_SOUNDS = 0,
    MSS_STOP_PRESERVE_ROOM_EFFECTS = 1 << 0,
    MSS_STOP_PRESERVE_MUSIC = 1 << 1,
    MSS_STOP_PRESERVE_AMBIENT = 1 << 2,
    MSS_STOP_PRESERVE_2D_AND_3D = 1 << 3
};
void MSS_StopSounds(uint32_t flags);
void MSS_FadeAllSounds(float targetVolume, int32_t durationMsec);
void FS_FreeFile(void *buffer);
char **FS_ListFilteredFiles(const char *path, const char *extension,
                            const char *filter, int32_t *fileCount);
void FS_FreeFileList(char **files);
void CM_SaveLump(int32_t lumpIndex, const void *buffer,
                 int32_t size, int32_t *checksum);
surfaceType_t Com_SurfaceTypeFromName(const char *name);
const char *Com_SurfaceTypeToName(int32_t surfaceType);
const char *SEH_LocalizeTextMessage(const char *input,
                                    const char *messageType,
                                    msgLocErrType_t errorType);
qboolean SEH_GetLocalizedTokenReference(const char *reference,
                                        char *output,
                                        const char *messageType,
                                        msgLocErrType_t errorType);
const char *SEH_SafeTranslateString(const char *reference);
int32_t SEH_GetCurrentLanguage(void);
const char *SEH_GetLanguageName(int32_t languageIndex);
qboolean SEH_GetLanguageIndexForName(const char *name,
                                     int32_t *languageIndex);
int32_t SEH_PrintStrlen(const char *text);
int32_t SEH_ReadCharFromString(
    const char **text, qboolean *isTrailingPunctuation);
qboolean Language_IsAsian(void);
qboolean Language_UsesSpaces(void);
void Com_NoiseInit(void);
float Com_NoiseGet4f(float x, float y, float z, float t);
qboolean Sys_LowPhysicalMemory(void);
void Sys_FreeFileList(char **list);
#ifdef __cplusplus
}
#endif

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(cvar_t) == 4,
               "i386 cvar alignment changed");
_Static_assert(offsetof(cvar_t, name) == 0x00,
               "i386 cvar name moved");
_Static_assert(offsetof(cvar_t, string) == 0x04,
               "i386 cvar string moved");
_Static_assert(offsetof(cvar_t, resetString) == 0x08,
               "i386 cvar reset string moved");
_Static_assert(offsetof(cvar_t, latchedString) == 0x0c,
               "i386 cvar latched string moved");
_Static_assert(offsetof(cvar_t, flags) == 0x10,
               "i386 cvar flags moved");
_Static_assert(offsetof(cvar_t, modified) == 0x14,
               "i386 cvar modified flag moved");
_Static_assert(offsetof(cvar_t, modificationCount) == 0x18,
               "i386 cvar modification count moved");
_Static_assert(offsetof(cvar_t, value) == 0x1c,
               "i386 cvar float value moved");
_Static_assert(offsetof(cvar_t, integer) == 0x20,
               "i386 cvar integer value moved");
_Static_assert(offsetof(cvar_t, next) == 0x24,
               "i386 cvar sorted link moved");
_Static_assert(offsetof(cvar_t, hashNext) == 0x28,
               "i386 cvar hash link moved");
_Static_assert(sizeof(cvar_t) == 0x2c,
               "i386 cvar size changed");
_Static_assert(_Alignof(cmd_function_t) == 4,
               "i386 command-node alignment changed");
_Static_assert(offsetof(cmd_function_t, next) == 0x00,
               "i386 command-node next link moved");
_Static_assert(offsetof(cmd_function_t, name) == 0x04,
               "i386 command-node name moved");
_Static_assert(offsetof(cmd_function_t, function) == 0x08,
               "i386 command-node callback moved");
_Static_assert(sizeof(cmd_function_t) == 0x0c,
               "i386 command-node size changed");
_Static_assert(_Alignof(qtime_t) == 4,
               "i386 real-time record alignment changed");
_Static_assert(offsetof(qtime_t, tm_sec) == 0x00,
               "i386 real-time seconds moved");
_Static_assert(offsetof(qtime_t, tm_min) == 0x04,
               "i386 real-time minutes moved");
_Static_assert(offsetof(qtime_t, tm_hour) == 0x08,
               "i386 real-time hours moved");
_Static_assert(offsetof(qtime_t, tm_mday) == 0x0c,
               "i386 real-time month day moved");
_Static_assert(offsetof(qtime_t, tm_mon) == 0x10,
               "i386 real-time month moved");
_Static_assert(offsetof(qtime_t, tm_year) == 0x14,
               "i386 real-time year moved");
_Static_assert(offsetof(qtime_t, tm_wday) == 0x18,
               "i386 real-time week day moved");
_Static_assert(offsetof(qtime_t, tm_yday) == 0x1c,
               "i386 real-time year day moved");
_Static_assert(offsetof(qtime_t, tm_isdst) == 0x20,
               "i386 real-time daylight-saving flag moved");
_Static_assert(sizeof(qtime_t) == 0x24,
               "i386 real-time record size changed");
_Static_assert(_Alignof(msg_t) == 4,
               "i386 message cursor alignment changed");
_Static_assert(offsetof(msg_t, overflowed) == 0x00,
               "i386 message overflow flag moved");
_Static_assert(offsetof(msg_t, data) == 0x04,
               "i386 message data pointer moved");
_Static_assert(offsetof(msg_t, maxsize) == 0x08,
               "i386 message capacity moved");
_Static_assert(offsetof(msg_t, cursize) == 0x0c,
               "i386 message write cursor moved");
_Static_assert(offsetof(msg_t, readcount) == 0x10,
               "i386 message read cursor moved");
_Static_assert(offsetof(msg_t, bit) == 0x14,
               "i386 message bit cursor moved");
_Static_assert(sizeof(msg_t) == 0x18,
               "i386 message cursor size changed");
_Static_assert(_Alignof(usercmd_t) == 4,
               "i386 usercmd alignment changed");
_Static_assert(offsetof(usercmd_t, commandTime) == 0x00,
               "i386 usercmd command time moved");
_Static_assert(offsetof(usercmd_t, buttons) == 0x04,
               "i386 usercmd buttons moved");
_Static_assert(offsetof(usercmd_t, wbuttons) == 0x05,
               "i386 usercmd wbuttons moved");
_Static_assert(offsetof(usercmd_t, weapon) == 0x06,
               "i386 usercmd weapon moved");
_Static_assert(offsetof(usercmd_t, angles) == 0x08,
               "i386 usercmd angles moved");
_Static_assert(sizeof(((usercmd_t *)0)->angles) == 0x0c,
               "i386 usercmd angle extent changed");
_Static_assert(offsetof(usercmd_t, forwardmove) == 0x14,
               "i386 usercmd forward move moved");
_Static_assert(offsetof(usercmd_t, rightmove) == 0x15,
               "i386 usercmd right move moved");
_Static_assert(offsetof(usercmd_t, upmove) == 0x16,
               "i386 usercmd up move moved");
_Static_assert(sizeof(usercmd_t) == 0x18,
               "i386 usercmd size changed");
#define CODUOMP_ASSERT_PLAYER_STATE_OFFSET(member_, offset_)                  \
    _Static_assert(offsetof(playerState_t, member_) == (offset_),             \
                   "i386 player-state " #member_ " moved")
#define CODUOMP_ASSERT_PLAYER_STATE_EXTENT(member_, extent_)                  \
    _Static_assert(sizeof(((playerState_t *)0)->member_) == (extent_),        \
                   "i386 player-state " #member_ " extent changed")

_Static_assert(_Alignof(playerState_t) == 4,
               "i386 player-state alignment changed");
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(commandTime, 0x000);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(pmType, 0x004);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(bobCycle, 0x008);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(playerStateFlags, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(pmTime, 0x010);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(psOrigin, 0x014);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(psOrigin, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(velocity, 0x020);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(velocity, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(weaponTime, 0x02c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(weaponDelay, 0x030);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(grenadeTimeLeft, 0x034);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(foliageSoundTime, 0x038);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(fatigueSoundTime, 0x03c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(gravity, 0x040);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(leanFraction, 0x044);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(speed, 0x048);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(deltaAngles, 0x04c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(deltaAngles, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(groundEntityNum, 0x058);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(ladderNormal, 0x05c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(ladderNormal, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(lastJumpCommandTime, 0x068);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(jumpOriginZ, 0x06c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(legsTimer, 0x070);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(legsAnim, 0x074);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(torsoTimer, 0x078);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(torsoAnim, 0x07c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(movementDir, 0x080);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(entityStateFlags, 0x084);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(eventIndex, 0x088);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(events, 0x08c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(events, 0x010);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(eventParms, 0x09c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(eventParms, 0x010);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(lastEventIndex, 0x0ac);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(unused0B0, 0x0b0);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(unused0B0, 0x024);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(psClientNum, 0x0d4);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(currentWeapon, 0x0d8);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(weaponState, 0x0dc);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(adsFraction, 0x0e0);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewModelIndex, 0x0e4);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewAngles, 0x0e8);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(viewAngles, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewHeightTarget, 0x0f4);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewHeightCurrent, 0x0f8);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewHeightLerpTime, 0x0fc);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewHeightLerpTarget, 0x100);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewHeightLerpDown, 0x104);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewHeightLerpPosAdj, 0x108);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(damageEvent, 0x10c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(damageYaw, 0x110);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(damagePitch, 0x114);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(damageCount, 0x118);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(stats, 0x11c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(stats, 0x018);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(stats[STAT_HEALTH], 0x11c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(stats[STAT_DEAD_YAW], 0x120);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(stats[STAT_MAX_HEALTH], 0x124);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(stats[STAT_IDENT_CLIENT_NUM], 0x128);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(stats[STAT_IDENT_CLIENT_HEALTH], 0x12c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(stats[STAT_SPAWN_COUNT], 0x130);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(ammo, 0x134);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(ammo, 0x200);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(clips, 0x334);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(clips, 0x200);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(weaponBits, 0x534);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(weaponBits, 0x010);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(weaponSlots, 0x544);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(weaponSlots, 0x008);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(weaponRechamberBits, 0x54c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(weaponRechamberBits, 0x010);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(playerMins, 0x55c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(playerMins, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(playerMaxs, 0x568);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(playerMaxs, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(proneViewHeight, 0x574);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(crouchViewHeight, 0x578);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(standViewHeight, 0x57c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(deadViewHeight, 0x580);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(walkSpeedScale, 0x584);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(runSpeedScale, 0x588);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(sprintSpeedScale, 0x58c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(proneSpeedScale, 0x590);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(crouchSpeedScale, 0x594);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(strafeSpeedScale, 0x598);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(backSpeedScale, 0x59c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(leanSpeedScale, 0x5a0);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(proneDirection, 0x5a4);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(proneDirectionPitch, 0x5a8);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(proneTorsoPitch, 0x5ac);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(fatigueScale, 0x5b0);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(lastSprintTime, 0x5b4);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewLocked, 0x5b8);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(viewLockedEntityNum, 0x5bc);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(friction, 0x5c0);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(serverCursorHint, 0x5c4);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(serverCursorHintVal, 0x5c8);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(serverCursorHintString, 0x5cc);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(unused5D0, 0x5d0);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(unused5D0, 0x01c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(cursorHintFlags, 0x5ec);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(unused5F0, 0x5f0);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(unused5F0, 0x008);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(cursorHintEntNum, 0x5f8);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(unused5FA, 0x5fa);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(unused5FA, 0x006);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(compassFriendInfo, 0x600);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(compassTankInfo, 0x604);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(torsoHeight, 0x608);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(torsoPitch, 0x60c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(waistPitch, 0x610);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(vehiclePosition, 0x614);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(vehicleType, 0x618);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(vehicleMotion, 0x61c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(oldEventIndex, 0x620);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(weaponAnim, 0x624);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(aimSpreadScale, 0x628);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(motionState, 0x62c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(motionState, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(motionState.externalVelocity, 0x62c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(motionState.externalVelocity, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(motionState.shellshock.index, 0x62c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(motionState.shellshock.time, 0x630);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(motionState.shellshock.duration, 0x634);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(motionState.shellshock, 0x00c);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(objectives, 0x638);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(objectives, 0x1c0);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(hudCurrent, 0x7f8);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(hudCurrent, 0x1e84);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(hudArchival, 0x267c);
CODUOMP_ASSERT_PLAYER_STATE_EXTENT(hudArchival, 0x1e84);
CODUOMP_ASSERT_PLAYER_STATE_OFFSET(deltaTime, 0x4500);
_Static_assert(sizeof(playerState_t) == 0x4504,
               "i386 player-state snapshot size changed");

#undef CODUOMP_ASSERT_PLAYER_STATE_EXTENT
#undef CODUOMP_ASSERT_PLAYER_STATE_OFFSET

_Static_assert(_Alignof(objective_t) == 4,
               "i386 player-state objective alignment changed");
_Static_assert(offsetof(objective_t, state) == 0x00,
               "i386 player-state objective state moved");
_Static_assert(offsetof(objective_t, origin) == 0x04,
               "i386 player-state objective origin moved");
_Static_assert(sizeof(((objective_t *)0)->origin) == 0x0c,
               "i386 player-state objective origin extent changed");
_Static_assert(offsetof(objective_t, entityNum) == 0x10,
               "i386 player-state objective entity moved");
_Static_assert(offsetof(objective_t, teamNum) == 0x14,
               "i386 player-state objective team moved");
_Static_assert(offsetof(objective_t, icon) == 0x18,
               "i386 player-state objective icon moved");
_Static_assert(sizeof(objective_t) == 0x1c,
               "i386 player-state objective size changed");
_Static_assert(_Alignof(entityState_t) == 4,
               "i386 entity-state alignment changed");
_Static_assert(offsetof(entityState_t, number) == 0x00,
               "i386 entity-state number moved");
_Static_assert(offsetof(entityState_t, eType) == 0x04,
               "i386 entity-state type moved");
_Static_assert(offsetof(entityState_t, eFlags) == 0x08,
               "i386 entity-state flags moved");
_Static_assert(offsetof(entityState_t, pos) == 0x0c,
               "i386 entity-state position trajectory moved");
_Static_assert(offsetof(entityState_t, apos) == 0x30,
               "i386 entity-state angular trajectory moved");
_Static_assert(offsetof(entityState_t, time) == 0x54,
               "i386 entity-state time moved");
_Static_assert(offsetof(entityState_t, time2) == 0x58,
               "i386 entity-state secondary time moved");
_Static_assert(offsetof(entityState_t, origin2) == 0x5c,
               "i386 entity-state secondary origin moved");
_Static_assert(sizeof(((entityState_t *)0)->origin2) == 0x0c,
               "i386 entity-state secondary origin extent changed");
_Static_assert(offsetof(entityState_t, angles2) == 0x68,
               "i386 entity-state secondary angles moved");
_Static_assert(sizeof(((entityState_t *)0)->angles2) == 0x0c,
               "i386 entity-state secondary angle extent changed");
_Static_assert(offsetof(entityState_t, otherEntityNum) == 0x74,
               "i386 entity-state other entity moved");
_Static_assert(offsetof(entityState_t, attackerEntityNum) == 0x78,
               "i386 entity-state attacker moved");
_Static_assert(offsetof(entityState_t, groundEntityNum) == 0x7c,
               "i386 entity-state ground entity moved");
_Static_assert(offsetof(entityState_t, constantLight) == 0x80,
               "i386 entity-state constant light moved");
_Static_assert(offsetof(entityState_t, loopSound) == 0x84,
               "i386 entity-state loop sound moved");
_Static_assert(offsetof(entityState_t, surfType) == 0x88,
               "i386 entity-state surface type moved");
_Static_assert(offsetof(entityState_t, index) == 0x8c,
               "i386 entity-state index moved");
_Static_assert(offsetof(entityState_t, xmodel) == 0x90,
               "i386 entity-state model moved");
_Static_assert(offsetof(entityState_t, clientNum) == 0x94,
               "i386 entity-state client number moved");
_Static_assert(offsetof(entityState_t, iHeadIcon) == 0x98,
               "i386 entity-state head icon moved");
_Static_assert(offsetof(entityState_t, iHeadIconTeam) == 0x9c,
               "i386 entity-state head-icon team moved");
_Static_assert(offsetof(entityState_t, solid) == 0xa0,
               "i386 entity-state solid encoding moved");
_Static_assert(offsetof(entityState_t, eventParm) == 0xa4,
               "i386 entity-state event parameter moved");
_Static_assert(offsetof(entityState_t, eventSequence) == 0xa8,
               "i386 entity-state event sequence moved");
_Static_assert(offsetof(entityState_t, events) == 0xac,
               "i386 entity-state events moved");
_Static_assert(sizeof(((entityState_t *)0)->events) == 0x10,
               "i386 entity-state event extent changed");
_Static_assert(offsetof(entityState_t, eventParms) == 0xbc,
               "i386 entity-state event parameters moved");
_Static_assert(sizeof(((entityState_t *)0)->eventParms) == 0x10,
               "i386 entity-state event-parameter extent changed");
_Static_assert(offsetof(entityState_t, weapon) == 0xcc,
               "i386 entity-state weapon moved");
_Static_assert(offsetof(entityState_t, legsAnim) == 0xd0,
               "i386 entity-state legs animation moved");
_Static_assert(offsetof(entityState_t, torsoAnim) == 0xd4,
               "i386 entity-state torso animation moved");
_Static_assert(offsetof(entityState_t, leanf) == 0xd8,
               "i386 entity-state lean moved");
_Static_assert(offsetof(entityState_t, scale) == 0xdc,
               "i386 entity-state scale moved");
_Static_assert(offsetof(entityState_t, dmgFlags) == 0xe0,
               "i386 entity-state damage flags moved");
_Static_assert(offsetof(entityState_t, animMovetype) == 0xe4,
               "i386 entity-state animation move type moved");
_Static_assert(offsetof(entityState_t, fTorsoHeight) == 0xe8,
               "i386 entity-state torso height moved");
_Static_assert(offsetof(entityState_t, fTorsoPitch) == 0xec,
               "i386 entity-state torso pitch moved");
_Static_assert(offsetof(entityState_t, fWaistPitch) == 0xf0,
               "i386 entity-state waist pitch moved");
_Static_assert(sizeof(entityState_t) == 0xf4,
               "i386 entity-state snapshot size changed");
_Static_assert(_Alignof(archivedEntity_t) == 4,
               "i386 archived entity alignment changed");
_Static_assert(offsetof(archivedEntity_t, state) == 0x00,
               "i386 archived entity state moved");
_Static_assert(offsetof(archivedEntity_t, svFlags) == 0xf4,
               "i386 archived entity flags moved");
_Static_assert(offsetof(archivedEntity_t, singleClient) == 0xf8,
               "i386 archived entity client selector moved");
_Static_assert(offsetof(archivedEntity_t, absmin) == 0xfc,
               "i386 archived entity minimum bounds moved");
_Static_assert(sizeof(((archivedEntity_t *)0)->absmin) == 0x0c,
               "i386 archived entity minimum bounds extent changed");
_Static_assert(offsetof(archivedEntity_t, absmax) == 0x108,
               "i386 archived entity maximum bounds moved");
_Static_assert(sizeof(((archivedEntity_t *)0)->absmax) == 0x0c,
               "i386 archived entity maximum bounds extent changed");
_Static_assert(sizeof(archivedEntity_t) == 0x114,
               "i386 archived entity size changed");
_Static_assert(_Alignof(hudElem_t) == 4,
               "i386 HUD-element alignment changed");
_Static_assert(_Alignof(hudelem_color_t) == 4,
               "i386 HUD color alignment changed");
_Static_assert(offsetof(hudelem_color_t, rgba) == 0x00,
               "i386 HUD packed color moved");
_Static_assert(offsetof(hudelem_color_t, components.red) == 0x00,
               "i386 HUD red color lane moved");
_Static_assert(offsetof(hudelem_color_t, components.green) == 0x01,
               "i386 HUD green color lane moved");
_Static_assert(offsetof(hudelem_color_t, components.blue) == 0x02,
               "i386 HUD blue color lane moved");
_Static_assert(offsetof(hudelem_color_t, components.alpha) == 0x03,
               "i386 HUD alpha color lane moved");
_Static_assert(sizeof(hudelem_color_t) == 0x04,
               "i386 HUD packed color size changed");
_Static_assert(offsetof(hudElem_t, type) == 0x00,
               "i386 HUD-element type moved");
_Static_assert(offsetof(hudElem_t, x) == 0x04,
               "i386 HUD-element x coordinate moved");
_Static_assert(offsetof(hudElem_t, y) == 0x08,
               "i386 HUD-element y coordinate moved");
_Static_assert(offsetof(hudElem_t, fontScale) == 0x0c,
               "i386 HUD-element font scale moved");
_Static_assert(offsetof(hudElem_t, font) == 0x10,
               "i386 HUD-element font moved");
_Static_assert(offsetof(hudElem_t, alignX) == 0x14,
               "i386 HUD-element x alignment moved");
_Static_assert(offsetof(hudElem_t, alignY) == 0x18,
               "i386 HUD-element y alignment moved");
_Static_assert(offsetof(hudElem_t, color) == 0x1c,
               "i386 HUD-element color moved");
_Static_assert(offsetof(hudElem_t, fromColor) == 0x20,
               "i386 HUD-element source color moved");
_Static_assert(offsetof(hudElem_t, fadeStartTime) == 0x24,
               "i386 HUD-element fade start moved");
_Static_assert(offsetof(hudElem_t, fadeTime) == 0x28,
               "i386 HUD-element fade duration moved");
_Static_assert(offsetof(hudElem_t, label) == 0x2c,
               "i386 HUD-element label moved");
_Static_assert(offsetof(hudElem_t, width) == 0x30,
               "i386 HUD-element width moved");
_Static_assert(offsetof(hudElem_t, height) == 0x34,
               "i386 HUD-element height moved");
_Static_assert(offsetof(hudElem_t, materialIndex) == 0x38,
               "i386 HUD-element material moved");
_Static_assert(offsetof(hudElem_t, scaleFromWidth) == 0x3c,
               "i386 HUD-element source width moved");
_Static_assert(offsetof(hudElem_t, scaleFromHeight) == 0x40,
               "i386 HUD-element source height moved");
_Static_assert(offsetof(hudElem_t, scaleStartTime) == 0x44,
               "i386 HUD-element scale start moved");
_Static_assert(offsetof(hudElem_t, scaleTime) == 0x48,
               "i386 HUD-element scale duration moved");
_Static_assert(offsetof(hudElem_t, moveFromX) == 0x4c,
               "i386 HUD-element source x moved");
_Static_assert(offsetof(hudElem_t, moveFromY) == 0x50,
               "i386 HUD-element source y moved");
_Static_assert(offsetof(hudElem_t, moveStartTime) == 0x54,
               "i386 HUD-element move start moved");
_Static_assert(offsetof(hudElem_t, moveTime) == 0x58,
               "i386 HUD-element move duration moved");
_Static_assert(offsetof(hudElem_t, timerValue) == 0x5c,
               "i386 HUD-element timer value moved");
_Static_assert(offsetof(hudElem_t, rotationPeriodMs) == 0x60,
               "i386 HUD-element rotation period moved");
_Static_assert(offsetof(hudElem_t, value) == 0x64,
               "i386 HUD-element numeric value moved");
_Static_assert(offsetof(hudElem_t, text) == 0x68,
               "i386 HUD-element text moved");
_Static_assert(offsetof(hudElem_t, sortKey) == 0x6c,
               "i386 HUD-element sort key moved");
_Static_assert(offsetof(hudElem_t, shaderRightTexcoord) == 0x70,
               "i386 HUD-element right texture coordinate moved");
_Static_assert(offsetof(hudElem_t, shaderBottomTexcoord) == 0x74,
               "i386 HUD-element bottom texture coordinate moved");
_Static_assert(offsetof(hudElem_t, unused78) == 0x78,
               "i386 HUD-element unused trailing dword moved");
_Static_assert(sizeof(hudElem_t) == 0x7c,
               "i386 HUD-element snapshot size changed");
#endif

#endif
