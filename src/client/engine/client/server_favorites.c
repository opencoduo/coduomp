#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "server_favorites.h"
#include "../platform/case_sensitive_fs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/* NOT_FROM_ORIGINAL_SOURCE: favorites format 1 is independent of native
 * structs, enum values and array capacities. Header: eight magic bytes,
 * little-endian u32 version and u32 entry count. Each entry: u8 address kind,
 * two network-order port bytes, little-endian u16 name byte count, address
 * bytes (none/4/10 for loopback/IPv4/IPX), then name bytes without a NUL.
 * Names retain the game's byte encoding, including color escapes. Reject
 * unsupported versions, kinds, lengths, excess entries and trailing data
 * without publishing a partial list or enabling an overwrite. */
enum {
    CODUOMP_FAVORITES_VERSION = 1,
    CODUOMP_FAVORITES_HEADER_BYTES = 16,
    CODUOMP_FAVORITES_ENTRY_HEADER_BYTES = 5,
    CODUOMP_FAVORITES_IPV4_BYTES = 4,
    CODUOMP_FAVORITES_IPX_BYTES = 10,
    CODUOMP_FAVORITES_PORT_BYTES = 2,
    CODUOMP_FAVORITE_LOOPBACK = 1,
    CODUOMP_FAVORITE_IPV4 = 2,
    CODUOMP_FAVORITE_IPX = 3,
    CODUOMP_LEGACY_HEADER_BYTES = 16,
    CODUOMP_LEGACY_GLOBAL_CAPACITY = 20000,
    CODUOMP_LEGACY_FAVORITE_CAPACITY = 128,
    CODUOMP_LEGACY_VERSION_1 = 1,
    CODUOMP_LEGACY_VERSION_2 = 2,
    CODUOMP_LEGACY_RECORD_1_BYTES = 152,
    CODUOMP_LEGACY_RECORD_2_BYTES = 248,
    CODUOMP_LEGACY_NAME_OFFSET = 46,
    CODUOMP_LEGACY_NAME_1_BYTES = 32,
    CODUOMP_LEGACY_NAME_2_BYTES = 128,
    CODUOMP_LEGACY_IPV4_OFFSET = 4,
    CODUOMP_LEGACY_IPX_OFFSET = 8,
    CODUOMP_LEGACY_PORT_OFFSET = 18,
    CODUOMP_LEGACY_LOOPBACK = 2,
    CODUOMP_LEGACY_IPV4 = 4,
    CODUOMP_LEGACY_IPX = 5,
    CODUOMP_FAVORITES_TEMP_ATTEMPTS = 64
};

typedef enum coduomp_favorites_open_e {
    CODUOMP_FAVORITES_MISSING,
    CODUOMP_FAVORITES_OPEN,
    CODUOMP_FAVORITES_UNREADABLE
} coduomp_favorites_open_t;

static const unsigned char coduomp_favoritesMagic[8] = {'O', 'C', 'U', 'O', 'F', 'A', 'V', '\0'};
static const char coduomp_favoritesFile[] = "opencoduo-favorites.dat";
static qboolean coduomp_favoritesSaveAllowed;
static char coduomp_favoritesHome[MAX_OSPATH];

/* NOT_FROM_ORIGINAL_SOURCE: explicit little-endian file scalar. */
static uint32_t coduomp_favorites_u32(const unsigned char *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit little-endian file scalar. */
static void coduomp_favorites_put_u32(unsigned char *bytes, uint32_t value)
{
    for (size_t index = 0; index < 4; ++index)
        bytes[index] = (unsigned char)(value >> (index * 8));
}

/* NOT_FROM_ORIGINAL_SOURCE: bounded paths for fixed application-owned names. */
static qboolean coduomp_favorites_path(char *path, const char *root, const char *name)
{
    if (root == NULL || root[0] == '\0')
        return qfalse;
    const int length = snprintf(path, MAX_OSPATH, "%s/%s", root, name);
    if (length < 0 || length >= MAX_OSPATH)
        return qfalse;
    FS_ReplaceSeparators(path);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: match the retail root search order, but do not
 * confuse an inaccessible file with a missing file and import stale data. */
static coduomp_favorites_open_t coduomp_favorites_open(const char *name, FILE **result)
{
    const char *roots[] = {fs_homepath->string, fs_basepath->string, fs_cdpath->string};
    *result = NULL;
    for (size_t index = 0; index < sizeof(roots) / sizeof(roots[0]); ++index) {
        if (roots[index][0] == '\0')
            continue;
        char path[MAX_OSPATH];
        char resolved[MAX_OSPATH];
        if (!coduomp_favorites_path(path, roots[index], name))
            return CODUOMP_FAVORITES_UNREADABLE;
        if (coduomp_resolve_case_path(roots[index], path, resolved, sizeof(resolved)))
            memcpy(path, resolved, strlen(resolved) + 1);
        errno = 0;
        *result = fopen(path, "rb");
        if (*result != NULL)
            return CODUOMP_FAVORITES_OPEN;
        if (errno != ENOENT)
            return CODUOMP_FAVORITES_UNREADABLE;
    }
    return CODUOMP_FAVORITES_MISSING;
}

/* NOT_FROM_ORIGINAL_SOURCE: read a complete stable-format list into staging. */
static qboolean coduomp_favorites_read(FILE *file, lan_server_info_t *servers, int32_t *count)
{
    unsigned char header[CODUOMP_FAVORITES_HEADER_BYTES];
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, coduomp_favoritesMagic, sizeof(coduomp_favoritesMagic)) != 0 ||
        coduomp_favorites_u32(header + 8) != CODUOMP_FAVORITES_VERSION)
        return qfalse;
    const uint32_t entries = coduomp_favorites_u32(header + 12);
    if (entries > LAN_FAVORITE_SERVER_CAPACITY)
        return qfalse;
    for (uint32_t index = 0; index < entries; ++index) {
        unsigned char entry[CODUOMP_FAVORITES_ENTRY_HEADER_BYTES];
        if (fread(entry, 1, sizeof(entry), file) != sizeof(entry))
            return qfalse;
        lan_server_info_t *server = &servers[index];
        memset(server, 0, sizeof(*server));
        size_t addressBytes;
        void *address;
        switch (entry[0]) {
        case CODUOMP_FAVORITE_LOOPBACK:
            server->address.type = NA_LOOPBACK;
            addressBytes = 0;
            address = server->address.ip;
            break;
        case CODUOMP_FAVORITE_IPV4:
            server->address.type = NA_IP;
            addressBytes = CODUOMP_FAVORITES_IPV4_BYTES;
            address = server->address.ip;
            break;
        case CODUOMP_FAVORITE_IPX:
            server->address.type = NA_IPX;
            addressBytes = CODUOMP_FAVORITES_IPX_BYTES;
            address = server->address.ipx;
            break;
        default:
            return qfalse;
        }
        memcpy(&server->address.port, entry + 1, CODUOMP_FAVORITES_PORT_BYTES);
        const size_t nameBytes = (size_t)entry[3] | (size_t)entry[4] << 8;
        if (nameBytes >= sizeof(server->hostName) ||
            fread(address, 1, addressBytes, file) != addressBytes ||
            fread(server->hostName, 1, nameBytes, file) != nameBytes ||
            memchr(server->hostName, '\0', nameBytes) != NULL)
            return qfalse;
        server->ping = -1;
        server->dirty = 1;
    }
    if (fgetc(file) != EOF || ferror(file))
        return qfalse;
    *count = (int32_t)entries;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: old caches are read-only migration sources. Their
 * fixed counts/strides belong to those file formats, not today's arrays. Skip
 * disposable globals and import only active favorite addresses and names;
 * unrelated metadata and unused slots need not be valid to recover favorites. */
static qboolean coduomp_favorites_import(FILE *file, lan_server_info_t *servers, int32_t *count)
{
    unsigned char header[CODUOMP_LEGACY_HEADER_BYTES];
    if (fread(header, 1, sizeof(header), file) != sizeof(header))
        return qfalse;
    const uint32_t version = coduomp_favorites_u32(header);
    const size_t recordBytes = version == CODUOMP_LEGACY_VERSION_1 ? CODUOMP_LEGACY_RECORD_1_BYTES :
        version == CODUOMP_LEGACY_VERSION_2 ? CODUOMP_LEGACY_RECORD_2_BYTES : 0;
    const size_t nameBytes = version == CODUOMP_LEGACY_VERSION_1 ? CODUOMP_LEGACY_NAME_1_BYTES : CODUOMP_LEGACY_NAME_2_BYTES;
    const uint32_t entries = coduomp_favorites_u32(header + 8);
    if (recordBytes == 0 || coduomp_favorites_u32(header + 4) > CODUOMP_LEGACY_GLOBAL_CAPACITY ||
        entries > CODUOMP_LEGACY_FAVORITE_CAPACITY || entries > LAN_FAVORITE_SERVER_CAPACITY ||
        coduomp_favorites_u32(header + 12) !=
            recordBytes * (CODUOMP_LEGACY_GLOBAL_CAPACITY + CODUOMP_LEGACY_FAVORITE_CAPACITY) ||
        fseek(file, 0, SEEK_END) != 0 ||
        ftell(file) < (long)(sizeof(header) + (CODUOMP_LEGACY_GLOBAL_CAPACITY + entries) * recordBytes) ||
        fseek(file, (long)(sizeof(header) + CODUOMP_LEGACY_GLOBAL_CAPACITY * recordBytes), SEEK_SET) != 0)
        return qfalse;
    for (uint32_t index = 0; index < entries; ++index) {
        unsigned char record[CODUOMP_LEGACY_RECORD_2_BYTES];
        if (fread(record, 1, recordBytes, file) != recordBytes)
            return qfalse;
        const unsigned char *name = record + CODUOMP_LEGACY_NAME_OFFSET;
        const unsigned char *end = memchr(name, '\0', nameBytes);
        if (end == NULL || (size_t)(end - name) >= sizeof(servers[index].hostName))
            return qfalse;
        lan_server_info_t *server = &servers[index];
        memset(server, 0, sizeof(*server));
        switch (coduomp_favorites_u32(record)) {
        case CODUOMP_LEGACY_LOOPBACK: server->address.type = NA_LOOPBACK; break;
        case CODUOMP_LEGACY_IPV4: server->address.type = NA_IP; break;
        case CODUOMP_LEGACY_IPX: server->address.type = NA_IPX; break;
        default: return qfalse;
        }
        memcpy(server->address.ip, record + CODUOMP_LEGACY_IPV4_OFFSET, CODUOMP_FAVORITES_IPV4_BYTES);
        memcpy(server->address.ipx, record + CODUOMP_LEGACY_IPX_OFFSET, CODUOMP_FAVORITES_IPX_BYTES);
        memcpy(&server->address.port, record + CODUOMP_LEGACY_PORT_OFFSET, CODUOMP_FAVORITES_PORT_BYTES);
        memcpy(server->hostName, name, (size_t)(end - name));
        server->ping = -1;
        server->dirty = 1;
    }
    *count = (int32_t)entries;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: encode only persistent identity and display name,
 * with checked writes; transient ping/map/player data stays in memory. */
static qboolean coduomp_favorites_write(FILE *file, const lan_server_info_t *servers, int32_t count)
{
    if (count < 0 || count > LAN_FAVORITE_SERVER_CAPACITY)
        return qfalse;
    unsigned char header[CODUOMP_FAVORITES_HEADER_BYTES];
    memcpy(header, coduomp_favoritesMagic, sizeof(coduomp_favoritesMagic));
    coduomp_favorites_put_u32(header + 8, CODUOMP_FAVORITES_VERSION);
    coduomp_favorites_put_u32(header + 12, (uint32_t)count);
    if (fwrite(header, 1, sizeof(header), file) != sizeof(header))
        return qfalse;
    for (int32_t index = 0; index < count; ++index) {
        const lan_server_info_t *server = &servers[index];
        const char *end = memchr(server->hostName, '\0', sizeof(server->hostName));
        if (end == NULL || (size_t)(end - server->hostName) > UINT16_MAX)
            return qfalse;
        const size_t nameBytes = (size_t)(end - server->hostName);
        unsigned char entry[CODUOMP_FAVORITES_ENTRY_HEADER_BYTES];
        const void *address;
        size_t addressBytes;
        switch (server->address.type) {
        case NA_LOOPBACK:
            entry[0] = CODUOMP_FAVORITE_LOOPBACK;
            address = server->address.ip;
            addressBytes = 0;
            break;
        case NA_IP:
            entry[0] = CODUOMP_FAVORITE_IPV4;
            address = server->address.ip;
            addressBytes = CODUOMP_FAVORITES_IPV4_BYTES;
            break;
        case NA_IPX:
            entry[0] = CODUOMP_FAVORITE_IPX;
            address = server->address.ipx;
            addressBytes = CODUOMP_FAVORITES_IPX_BYTES;
            break;
        default:
            return qfalse;
        }
        memcpy(entry + 1, &server->address.port, CODUOMP_FAVORITES_PORT_BYTES);
        entry[3] = (unsigned char)nameBytes;
        entry[4] = (unsigned char)(nameBytes >> 8);
        if (fwrite(entry, 1, sizeof(entry), file) != sizeof(entry) ||
            fwrite(address, 1, addressBytes, file) != addressBytes ||
            fwrite(server->hostName, 1, nameBytes, file) != nameBytes)
            return qfalse;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: same-directory exclusive temporary file, checked
 * write/flush/sync/close, then replacement without a delete/copy fallback.
 * Failure before replacement leaves the previous file intact. POSIX also
 * syncs the parent directory; a post-rename sync failure is reported separately
 * because the new file is already visible. No personal files are used as temps. */
void coduomp_save_favorites(const lan_server_info_t *servers, int32_t count)
{
    if (!coduomp_favoritesSaveAllowed || strcmp(coduomp_favoritesHome, fs_homepath->string) != 0) {
        Com_Printf("Favorites were not saved: existing data could not be safely loaded.\n");
        return;
    }
    char destination[MAX_OSPATH];
    char temporary[MAX_OSPATH];
    if (!coduomp_favorites_path(destination, coduomp_favoritesHome, coduomp_favoritesFile))
        return;
    if (FS_CreatePath(destination) != qfalse) {
        Com_Printf("Favorites were not saved: could not prepare the destination directory.\n");
        return;
    }
    static unsigned int sequence;
    int descriptor = -1;
    for (int attempt = 0; attempt < CODUOMP_FAVORITES_TEMP_ATTEMPTS; ++attempt) {
#ifdef _WIN32
        const int pid = _getpid();
#else
        const int pid = (int)getpid();
#endif
        const int length = snprintf(temporary, sizeof(temporary), "%s.%d.%u.tmp", destination, pid, sequence++);
        if (length < 0 || (size_t)length >= sizeof(temporary))
            break;
#ifdef _WIN32
        descriptor = _open(temporary, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
#else
        descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
#endif
        if (descriptor >= 0 || errno != EEXIST)
            break;
    }
    if (descriptor < 0) {
        Com_Printf("Favorites were not saved: could not create a temporary file.\n");
        return;
    }
#ifdef _WIN32
    FILE *file = _fdopen(descriptor, "wb");
#else
    FILE *file = fdopen(descriptor, "wb");
#endif
    qboolean success = qfalse;
    if (file != NULL) {
        success = coduomp_favorites_write(file, servers, count);
        if (fflush(file) != 0)
            success = qfalse;
#ifdef _WIN32
        if (_commit(descriptor) != 0)
#else
        if (fsync(descriptor) != 0)
#endif
            success = qfalse;
        if (fclose(file) != 0)
            success = qfalse;
    } else {
#ifdef _WIN32
        (void)_close(descriptor);
#else
        (void)close(descriptor);
#endif
    }
#ifdef _WIN32
    if (success && !MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        success = qfalse;
#else
    int directory = -1;
    if (success) {
        directory = open(coduomp_favoritesHome, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory < 0 || rename(temporary, destination) != 0)
            success = qfalse;
    }
    if (directory >= 0) {
        if (success && fsync(directory) != 0)
            Com_Printf("Favorites saved, but directory synchronization failed; power-loss durability is uncertain.\n");
        (void)close(directory);
    }
#endif
    if (!success) {
        Com_Printf("Favorites were not saved; the previous file was retained.\n");
        if (remove(temporary) != 0)
            Com_Printf("Could not remove favorites temporary file: %s\n", temporary);
    } else {
        coduomp_case_path_cache_clear();
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: publish only a fully validated list. A present
 * invalid/newer file blocks migration and saving. An empty valid file is an
 * intentional empty list. Prefer the previous improved cache over retail,
 * and persist an import immediately so future cache removal cannot undo it. */
void coduomp_load_favorites(lan_server_info_t *servers, int32_t *count)
{
    coduomp_favoritesSaveAllowed = qfalse;
    *count = 0;
    if (strlen(fs_homepath->string) >= sizeof(coduomp_favoritesHome))
        return;
    memcpy(coduomp_favoritesHome, fs_homepath->string, strlen(fs_homepath->string) + 1);
    lan_server_info_t staged[LAN_FAVORITE_SERVER_CAPACITY];
    int32_t stagedCount = 0;
    FILE *file;
    coduomp_favorites_open_t status = coduomp_favorites_open(coduomp_favoritesFile, &file);
    qboolean imported = qfalse;
    qboolean valid = qtrue;
    if (status == CODUOMP_FAVORITES_OPEN) {
        valid = coduomp_favorites_read(file, staged, &stagedCount);
    } else if (status == CODUOMP_FAVORITES_MISSING) {
        status = coduomp_favorites_open("opencoduo-servercache.dat", &file);
        if (status == CODUOMP_FAVORITES_MISSING)
            status = coduomp_favorites_open("uoservercache.dat", &file);
        if (status == CODUOMP_FAVORITES_OPEN) {
            valid = coduomp_favorites_import(file, staged, &stagedCount);
            imported = qtrue;
        }
    }
    if (file != NULL && fclose(file) != 0)
        valid = qfalse;
    if (status == CODUOMP_FAVORITES_UNREADABLE || !valid) {
        Com_Printf("Favorites could not be loaded; existing files were preserved and saving is disabled.\n");
        return;
    }
    memset(servers, 0, LAN_FAVORITE_SERVER_CAPACITY * sizeof(*servers));
    memcpy(servers, staged, (size_t)stagedCount * sizeof(*servers));
    *count = stagedCount;
    coduomp_favoritesSaveAllowed = qtrue;
    if (imported)
        coduomp_save_favorites(servers, *count);
}
