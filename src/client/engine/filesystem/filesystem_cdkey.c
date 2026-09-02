#include "filesystem_local.h"

#include "../client/cgame.h"
#include "../client/server_browser.h"
#include "qcommon/q_string.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

enum {
    COM_CDKEY_PAYLOAD_BYTES = CL_CDKEY_PART_SIZE + CL_CDKEY_CHECKSUM_SIZE,
    COM_WINDOWS_CDKEY_RECORD_BYTES = COM_CDKEY_PAYLOAD_BYTES + 1
};

static const char comDefaultCdKey[CL_CDKEY_PART_SIZE + 1] = "                ";

#if defined(_WIN32)
static const char comCdKeyRegistryPath[] = "SOFTWARE\\Activision\\Call of Duty United Offensive";
static const char comCdKeyRegistryValue[] = "key";
#endif

/* Source: CoDUOMP.exe 0x0043b020..0x0043b058.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043b020_0043b059.mcode and the exact
 * 17-byte default at 0x00598738. The Windows-only helper has no Mac traceback
 * name; its role name describes the complete body. It deliberately resets
 * only the primary CoD:UO key and leaves the checksum bytes untouched. */
void Com_ResetCDKeyToDefault(void)
{
    memcpy(&cl_cdkey[CL_PRIMARY_CDKEY_OFFSET], comDefaultCdKey, sizeof(comDefaultCdKey));
}

#if defined(_WIN32)

/* Source: CoDUOMP.exe 0x0043b060..0x0043b1c4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043b060_0043b1c5.mcode.
 * Exact same-module Mac symbol: Com_ReadCDKey. The Windows executable reads
 * the 21-byte REG_SZ value from HKLM. */
void Com_ReadCDKey(void)
{
    char record[COM_WINDOWS_CDKEY_RECORD_BYTES];
    qboolean readSucceeded = qfalse;
    HKEY keyHandle;

    if (RegOpenKeyA(HKEY_LOCAL_MACHINE, comCdKeyRegistryPath, &keyHandle) == ERROR_SUCCESS) {
        DWORD valueType = REG_SZ;
        DWORD recordSize = sizeof(record);

        if (RegQueryValueExA(keyHandle, comCdKeyRegistryValue, NULL, &valueType, (BYTE *)record, &recordSize) == ERROR_SUCCESS &&
            recordSize == sizeof(record)) {
            readSucceeded = qtrue;
        }
        RegCloseKey(keyHandle);
    }

    if (readSucceeded == qfalse) {
        Com_ResetCDKeyToDefault();
        return;
    }

    memcpy(&cl_cdkey[CL_PRIMARY_CDKEY_OFFSET], record, CL_CDKEY_PART_SIZE);
    cl_cdkey[CL_UNIQUE_MOD_CDKEY_OFFSET] = '\0';
    memcpy(&cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET], &record[CL_CDKEY_PART_SIZE], CL_CDKEY_CHECKSUM_SIZE);
    cl_cdkeyChecksums[CL_UNIQUE_MOD_CDKEY_CHECKSUM_OFFSET] = '\0';

    if (CL_CDKeyValidate(&cl_cdkey[CL_PRIMARY_CDKEY_OFFSET], &cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET]) == qfalse) {
        Com_ResetCDKeyToDefault();
    }
}

/* Source: CoDUOMP.exe 0x0043b1d0..0x0043b2b6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043b1d0_0043b2b7.mcode.
 * Exact same-module Mac symbol: Com_WriteCDKey. Invalid keys are replaced by
 * the default without writing; valid keys are serialized as 16 key bytes,
 * four checksum bytes, and a final NUL. */
void Com_WriteCDKey(void)
{
    if (CL_CDKeyValidate(&cl_cdkey[CL_PRIMARY_CDKEY_OFFSET], &cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET]) == qfalse) {
        Com_ResetCDKeyToDefault();
        return;
    }

    char record[COM_WINDOWS_CDKEY_RECORD_BYTES];
    memcpy(record, &cl_cdkey[CL_PRIMARY_CDKEY_OFFSET], CL_CDKEY_PART_SIZE);
    memcpy(&record[CL_CDKEY_PART_SIZE], &cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET], CL_CDKEY_CHECKSUM_SIZE);
    record[COM_WINDOWS_CDKEY_RECORD_BYTES - 1] = '\0';

    HKEY keyHandle;
    if (RegCreateKeyA(HKEY_LOCAL_MACHINE, comCdKeyRegistryPath, &keyHandle) != ERROR_SUCCESS) {
        return;
    }
    (void)RegSetValueExA(keyHandle, comCdKeyRegistryValue, 0, REG_SZ, (const BYTE *)record, sizeof(record));
    RegCloseKey(keyHandle);
}

#else

/*
 * NOT_FROM_ORIGINAL_SOURCE: native environment override for the primary UO
 * key. UO_CODKEY uses the same 20-character key-plus-checksum payload as the
 * leading bytes of codkey. The value is never logged or written to disk.
 */
static qboolean coduomp_load_uo_codkey_environment(char *keyDestination, char *checksumDestination)
{
    const char *const value = getenv("UO_CODKEY");
    if (value == NULL || value[0] == '\0')
        return qfalse;

    if (strlen(value) != COM_CDKEY_PAYLOAD_BYTES) {
        Com_Printf("WARNING: ignoring invalid UO_CODKEY environment value\n");
        return qfalse;
    }

    char key[CL_CDKEY_PART_SIZE + 1] = {0};
    char checksum[CL_CDKEY_CHECKSUM_SIZE + 1] = {0};
    memcpy(key, value, CL_CDKEY_PART_SIZE);
    memcpy(checksum, &value[CL_CDKEY_PART_SIZE], CL_CDKEY_CHECKSUM_SIZE);
    Q_strupr(key);
    Q_strupr(checksum);

    if (CL_CDKeyValidate(key, checksum) == qfalse) {
        Com_Printf("WARNING: ignoring invalid UO_CODKEY environment value\n");
        return qfalse;
    }

    Q_strncpyz(keyDestination, key, CL_CDKEY_PART_SIZE + 1);
    Q_strncpyz(checksumDestination, checksum, CL_CDKEY_CHECKSUM_SIZE + 1);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring shared by the same-version
 * Mac Com_ReadCDKey and Com_AppendCDKey bodies. Each original function builds
 * "<gameDirectory>/codkey", reads exactly the leading 16 key bytes and four
 * checksum bytes, validates them, and installs them into its destination.
 * The modern native call uses an empty gameDirectory for a root-level codkey. */
static qboolean Com_LoadCDKeyFile(const char *gameDirectory, char *keyDestination, char *checksumDestination)
{
    char qpath[MAX_OSPATH];
    char key[CL_CDKEY_PART_SIZE + 1] = {0};
    char checksum[CL_CDKEY_CHECKSUM_SIZE + 1] = {0};
    int32_t fileHandle = 0;

    if (gameDirectory[0] != '\0') {
        Com_sprintf(qpath, sizeof(qpath), "%s/%s", gameDirectory, "codkey");
    } else {
        Q_strncpyz(qpath, "codkey", sizeof(qpath));
    }
    const int32_t fileSize = FS_SV_FOpenFileRead(qpath, &fileHandle);
    if (fileHandle == 0 || fileSize < COM_CDKEY_PAYLOAD_BYTES) {
        if (fileHandle != 0)
            FS_FCloseFile(fileHandle);
        return qfalse;
    }

    const qboolean readSucceeded = FS_Read(key, CL_CDKEY_PART_SIZE, fileHandle) == CL_CDKEY_PART_SIZE &&
                                   FS_Read(checksum, CL_CDKEY_CHECKSUM_SIZE, fileHandle) == CL_CDKEY_CHECKSUM_SIZE;
    FS_FCloseFile(fileHandle);

    if (readSucceeded == qfalse || CL_CDKeyValidate(key, checksum) == qfalse) {
        return qfalse;
    }

    Q_strncpyz(keyDestination, key, CL_CDKEY_PART_SIZE + 1);
    Q_strncpyz(checksumDestination, checksum, CL_CDKEY_CHECKSUM_SIZE + 1);
    return qtrue;
}

/* Source: same-version Mac PEF 0x10045540..0x10045693.
 * Name and signature: exact symbol Com_ReadCDKey. The shipped Mac engine
 * reads the primary CoD:UO key from "<gameDirectory>/codkey"; unlike the
 * Windows REG_SZ record, the file may contain explanatory text after its
 * 20-byte payload. The modern native caller passes an empty directory so the
 * primary key resides directly below fs_homepath. */
void Com_ReadCDKey(const char *gameDirectory)
{
    if (gameDirectory[0] == '\0' && coduomp_load_uo_codkey_environment(&cl_cdkey[CL_PRIMARY_CDKEY_OFFSET],
                                                                       &cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET]) != qfalse) {
        return;
    }

    if (Com_LoadCDKeyFile(gameDirectory, &cl_cdkey[CL_PRIMARY_CDKEY_OFFSET], &cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET]) ==
        qfalse) {
        Q_strncpyz(&cl_cdkey[CL_PRIMARY_CDKEY_OFFSET], comDefaultCdKey, CL_CDKEY_PART_SIZE + 1);
        Q_strncpyz(&cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET], "    ", CL_CDKEY_CHECKSUM_SIZE + 1);
    }
}

/* Source: same-version Mac PEF 0x100453c0..0x1004550b.
 * Name and signature: exact symbol Com_AppendCDKey. An fs_game module with a
 * unique key stores its independent key/checksum pair in that module's codkey
 * file. This second slot is not the retail CoD1 key. */
void Com_AppendCDKey(const char *gameDirectory)
{
    if (Com_LoadCDKeyFile(gameDirectory, &cl_cdkey[CL_UNIQUE_MOD_CDKEY_OFFSET], &cl_cdkeyChecksums[CL_UNIQUE_MOD_CDKEY_CHECKSUM_OFFSET]) ==
        qfalse) {
        Q_strncpyz(&cl_cdkey[CL_UNIQUE_MOD_CDKEY_OFFSET], comDefaultCdKey, CL_CDKEY_PART_SIZE + 1);
        Q_strncpyz(&cl_cdkeyChecksums[CL_UNIQUE_MOD_CDKEY_CHECKSUM_OFFSET], "    ", CL_CDKEY_CHECKSUM_SIZE + 1);
    }
}

/* Source: same-version Mac PEF 0x10045280..0x1004539b.
 * Name and signature: exact symbol Com_WriteCDKey. The original uppercases
 * and validates the supplied pair, writes the 20-byte payload to
 * "<gameDirectory>/codkey", then appends human-readable warnings. */
void Com_WriteCDKey(const char *gameDirectory, const char *key, const char *checksum)
{
    char qpath[MAX_OSPATH];
    char normalizedKey[CL_CDKEY_PART_SIZE + 1];
    char normalizedChecksum[CL_CDKEY_CHECKSUM_SIZE + 1];

    Q_strncpyz(normalizedKey, key, sizeof(normalizedKey));
    Q_strncpyz(normalizedChecksum, checksum, sizeof(normalizedChecksum));
    Q_strupr(normalizedKey);
    Q_strupr(normalizedChecksum);

    if (CL_CDKeyValidate(normalizedKey, normalizedChecksum) == qfalse) {
        return;
    }

    if (gameDirectory[0] != '\0') {
        Com_sprintf(qpath, sizeof(qpath), "%s/%s", gameDirectory, "codkey");
    } else {
        Q_strncpyz(qpath, "codkey", sizeof(qpath));
    }
    const int32_t fileHandle = FS_SV_FOpenFileWrite(qpath);
    if (fileHandle == 0) {
        Com_Printf("Couldn't write %s.\n", gameDirectory);
        return;
    }

    (void)FS_Write(normalizedKey, CL_CDKEY_PART_SIZE, fileHandle);
    (void)FS_Write(normalizedChecksum, CL_CDKEY_CHECKSUM_SIZE, fileHandle);
    FS_Printf(fileHandle, "\n// generated by CoD, do not modify\n");
    FS_Printf(fileHandle, "// Do not give this file to ANYONE.\n");
    FS_Printf(fileHandle, "// Aspyr will NOT ask you to send this file to them.\n");
    FS_FCloseFile(fileHandle);
}

#endif
