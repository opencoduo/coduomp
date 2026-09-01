#include "filesystem_local.h"

/* Source: CoDUOMP.exe 0x049311e8 (.bss). FS_Restart stores its checksum-feed
 * argument here; CL_Disconnect clears it after dropping the connection. */
int32_t fs_checksumFeed;

/* Original Win32 active game-directory buffer at 0x0492d0e0. Empty or null
 * game arguments to FS_BuildOSPath_Internal resolve through this buffer. */
char fs_currentGameDir[FS_PACK_NAME_SIZE];

/* Original Win32 filesystem cvar pointers. FS_Startup at 0x00430800 assigns
 * every slot from the correspondingly named Cvar_Get result. */
cvar_t *fs_copyfiles;       /* 0x049290b0 */
cvar_t *fs_cdpath;          /* 0x049290b4 */
cvar_t *fs_restrict;        /* 0x049311e0 */
cvar_t *fs_debug;
cvar_t *fs_basepath;        /* 0x04935420 */
cvar_t *fs_ignoreLocalized; /* 0x04935424 */
cvar_t *fs_game;            /* 0x04935428 */
cvar_t *fs_homepath;        /* 0x04939560 */
cvar_t *fs_basegame;        /* 0x0492d0c0 */

/* Source: CoDUOMP.exe 0x04935320 and 0x04939460. Startup and restart retain
 * the accepted base/game cvar strings here. The separate fs_gameDirVar at
 * 0x04935200 is cleared after the initial filesystem validation. */
char fs_savedBasePath[FS_PACK_NAME_SIZE];
char fs_savedGame[FS_PACK_NAME_SIZE];

/* Original Win32 filesystem search-path head at 0x0389fcfc and the optional
 * test checksum at 0x04935308. */
searchpath_t *fs_searchpaths;
int32_t fs_fakeChkSum;
int32_t fs_packFiles; /* original 0x0389fd0c */

/* Source: CoDUOMP.exe 0x04935300 (.bss). File-open entry points set this
 * per-frame flag; Com_Frame samples and clears it for the statmon disk-access
 * warning. It is distinct from the outstanding FS_ReadFile allocation count. */
qboolean fs_fileAccessed;

/* Source: CoDUOMP.exe 0x0389fd00. Loose non-pack asset directories are
 * indexed into this linked list by FS_AddNonPackFileDirectory_Internal. */
fs_dir_file_list_t *fs_dirFileLists;

/* Source: CoDUOMP.exe 0x0389fd04 and 0x0389fd08. Filesystem lookup refresh
 * snapshots the searchable pack and loose-file list heads into these globals;
 * FS_GetDataForFile and FS_ClearDataForFiles traverse the snapshots. */
searchpath_t *fs_lookupSearchpaths;
fs_dir_file_list_t *fs_lookupDirFileLists;

/* Source: CoDUOMP.exe 0x0389fd10 and 0x009677c8..0x00967847.
 * PakFileLanguage alternates between these two 64-byte return buffers so the
 * pak comparator can retain both language-name results at once. */
int32_t fs_languageNameBufferIndex;
char fs_languageNameBuffers[2][64];

/* Source: CoDUOMP.exe 0x0389fd14. FS_AddPakFilesForGameDirectory sets this
 * after printing the supported-language list for the first malformed localized
 * pak name, preventing the list from being repeated for later bad names. */
qboolean fs_printedSupportedLanguages;

/* Number of outstanding whole-file temporary allocations returned by
 * FS_ReadFile. FS_ResetFiles clears the counter during error recovery, and
 * the exact Mac-symbol API FS_LoadStack returns it. */
int32_t fs_loadStack; /* original 0x0493542c */

/* Source: CoDUOMP.exe 0x04935200..0x049352ff (.bss). Opening a BSP records
 * the selected pack or directory's game name for later map-path consumers. */
char fs_gameDirVar[FS_PACK_NAME_SIZE];

/* Source: CoDUOMP.exe 0x049311e4 and 0x04935440..0x0493943f (.bss).
 * FS_PureServerSetLoadedPaks installs up to 4096 checksums here;
 * FS_PakIsPure compares each candidate pack against this exact list. */
int32_t fs_numServerPaks;
int32_t fs_serverPaks[FS_MAX_SERVER_PAKS];

/* Source: CoDUOMP.exe 0x049290c0..0x0492d0bf, 0x049311e4,
 * 0x04931200..0x049351ff, and 0x04939440. The two name tables are separately
 * owned companions to the referenced-pak and server-pak checksum lists. */
char *fs_serverPakNames[FS_MAX_SERVER_PAKS];
int32_t fs_numServerReferencedPaks;
/* 0x0492d1e0..0x049311df: checksums paired by index with
 * fs_serverReferencedPakNames; FS_ComparePaks compares these against loaded packs. */
int32_t fs_serverReferencedPaks[FS_MAX_SERVER_PAKS];
char *fs_serverReferencedPakNames[FS_MAX_SERVER_PAKS];

/* Source: CoDUOMP.exe 0x04939580..0x0493dd7f (.bss).
 * FS_FOpenFileRead allocates one of 64 records with a 0x120-byte i386 stride.
 * Sys_LoadDll reads zipArchive from +0x18 to identify the pack containing a
 * native module before enforcing the pure-server disk-copy check. */
fileHandleData_t fs_handleFiles[FS_HANDLE_COUNT];
