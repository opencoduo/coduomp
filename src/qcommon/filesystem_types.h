#ifndef QCOMMON_FILESYSTEM_TYPES_H
#define QCOMMON_FILESYSTEM_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "file_data.h"
#include "q_shared_types.h"
#include "qcommon_limits.h"

/* Build-time policy knobs for mounted PK3 content. The defaults leave broad
 * headroom above the retail data set while preventing one downloaded mod from
 * forcing multi-gigabyte catalog or entry allocations. */
#ifndef FS_MAX_PAK_ARCHIVE_BYTES
#define FS_MAX_PAK_ARCHIVE_BYTES UINT64_C(536870912)
#endif
#ifndef FS_MAX_PAK_ENTRY_BYTES
#define FS_MAX_PAK_ENTRY_BYTES UINT64_C(268435456)
#endif
#ifndef FS_MAX_PAK_ENTRIES
#define FS_MAX_PAK_ENTRIES UINT32_C(32768)
#endif

#if FS_MAX_PAK_ARCHIVE_BYTES > INT32_MAX
#error "FS_MAX_PAK_ARCHIVE_BYTES must fit the signed filesystem position ABI"
#endif
#if FS_MAX_PAK_ENTRY_BYTES > INT32_MAX
#error "FS_MAX_PAK_ENTRY_BYTES must fit the signed filesystem length ABI"
#endif
#if FS_MAX_PAK_ENTRIES > UINT16_MAX
#error "FS_MAX_PAK_ENTRIES must fit the classic ZIP entry-count field"
#endif

enum {
    FS_HANDLE_COUNT = 64,
    FS_HANDLE_NAME_SIZE = 256,
    FS_PACK_NAME_SIZE = 256,
    FS_LANGUAGE_NAME_BUFFER_COUNT = 2,
    FS_LANGUAGE_NAME_BUFFER_SIZE = 64,
    FS_MAX_SERVER_PAKS = 4096,
    FS_SEEK_ORIGIN_CURRENT = 0,
    FS_SEEK_ORIGIN_END = 1,
    FS_SEEK_ORIGIN_SET = 2
};

/* Complete stock file-open mode domain passed through the engine/module ABI.
 * Windows client and Linux server dispatch values 0..3 identically. */
typedef enum fsMode_e {
    FS_READ = 0,
    FS_WRITE = 1,
    FS_APPEND = 2,
    FS_APPEND_SYNC = 3
} fsMode_t;

typedef struct fileInPack_s fileInPack_t;
typedef struct directory_s directory_t;
typedef struct fs_dir_file_s fs_dir_file_t;
typedef struct fs_dir_file_list_s fs_dir_file_list_t;
typedef struct pack_s pack_t;
typedef struct searchpath_s searchpath_t;
typedef struct fileHandleData_s fileHandleData_t;

/* The Windows client and Linux server use the same three-word fileData_t view
 * at +0x04.  The older Linux recovery names basename/payload/clearCallback
 * described those same fields rather than a distinct record. */
struct fileInPack_s {
    int32_t zipPosition;
    fileData_t data;
    char *name;
    fileInPack_t *next;
};

struct directory_s {
    char path[FS_PACK_NAME_SIZE];
    char gamedir[FS_PACK_NAME_SIZE];
};

struct fs_dir_file_s {
    fileData_t data;
    fs_dir_file_t *next;
};

struct fs_dir_file_list_s {
    char path[FS_PACK_NAME_SIZE];
    int32_t numFiles;
    int32_t hashSize;
    fs_dir_file_t **hashTable;
    fs_dir_file_t *fileList;
    fs_dir_file_list_t *next;
};

struct pack_s {
    char pakFilename[FS_PACK_NAME_SIZE];
    char pakBasename[FS_PACK_NAME_SIZE];
    char pakGamename[FS_PACK_NAME_SIZE];
    /* Opaque minizip archive handle.  Windows defines unzFile as void *;
     * Linux recovers the concrete private implementation behind this slot. */
    void *zipFile;
    int32_t checksum;
    int32_t pureChecksum;
    int32_t numFiles;
    uint8_t generalReference;
    uint8_t uiModuleReference;
    uint8_t cgameModuleReference;
    uint8_t gameModuleReference;
    int32_t hashSize;
    fileInPack_t **hashTable;
    fileInPack_t *fileList;
};

struct searchpath_s {
    searchpath_t *next;
    pack_t *pack;
    directory_t *dir;
    qboolean localized;
    int32_t language;
};

/* FS_FileForHandle in both authoritative engines indexes this exact 0x120-byte
 * i386 slot.  The fixed-width position and size preserve the original 32-bit
 * long representation when the recovered source is built on a 64-bit host. */
struct fileHandleData_s {
    void *ioObject;
    qboolean uniqueObject;
    qboolean sync;
    int32_t position;
    int32_t size;
    int32_t zipRewindOffset;
    pack_t *zipArchive;
    qboolean seekCallbackGuard;
    char name[FS_HANDLE_NAME_SIZE];
};

#if UINTPTR_MAX == UINT32_MAX
#define FS_LAYOUT_ASSERT(name, expression) \
    typedef char name[(expression) ? 1 : -1]

FS_LAYOUT_ASSERT(fs_file_in_pack_data_offset,
                 offsetof(fileInPack_t, data) == 0x04);
FS_LAYOUT_ASSERT(fs_file_in_pack_name_offset,
                 offsetof(fileInPack_t, name) == 0x10);
FS_LAYOUT_ASSERT(fs_file_in_pack_next_offset,
                 offsetof(fileInPack_t, next) == 0x14);
FS_LAYOUT_ASSERT(fs_file_in_pack_size, sizeof(fileInPack_t) == 0x18);
FS_LAYOUT_ASSERT(fs_directory_gamedir_offset,
                 offsetof(directory_t, gamedir) == 0x100);
FS_LAYOUT_ASSERT(fs_directory_size, sizeof(directory_t) == 0x200);
FS_LAYOUT_ASSERT(fs_dir_file_next_offset,
                 offsetof(fs_dir_file_t, next) == 0x0c);
FS_LAYOUT_ASSERT(fs_dir_file_size, sizeof(fs_dir_file_t) == 0x10);
FS_LAYOUT_ASSERT(fs_dir_file_list_num_files_offset,
                 offsetof(fs_dir_file_list_t, numFiles) == 0x100);
FS_LAYOUT_ASSERT(fs_dir_file_list_hash_size_offset,
                 offsetof(fs_dir_file_list_t, hashSize) == 0x104);
FS_LAYOUT_ASSERT(fs_dir_file_list_hash_table_offset,
                 offsetof(fs_dir_file_list_t, hashTable) == 0x108);
FS_LAYOUT_ASSERT(fs_dir_file_list_file_list_offset,
                 offsetof(fs_dir_file_list_t, fileList) == 0x10c);
FS_LAYOUT_ASSERT(fs_dir_file_list_next_offset,
                 offsetof(fs_dir_file_list_t, next) == 0x110);
FS_LAYOUT_ASSERT(fs_dir_file_list_size,
                 sizeof(fs_dir_file_list_t) == 0x114);
FS_LAYOUT_ASSERT(fs_pack_zip_file_offset,
                 offsetof(pack_t, zipFile) == 0x300);
FS_LAYOUT_ASSERT(fs_pack_checksum_offset,
                 offsetof(pack_t, checksum) == 0x304);
FS_LAYOUT_ASSERT(fs_pack_reference_flags_offset,
                 offsetof(pack_t, generalReference) == 0x310);
FS_LAYOUT_ASSERT(fs_pack_hash_size_offset,
                 offsetof(pack_t, hashSize) == 0x314);
FS_LAYOUT_ASSERT(fs_pack_hash_table_offset,
                 offsetof(pack_t, hashTable) == 0x318);
FS_LAYOUT_ASSERT(fs_pack_file_list_offset,
                 offsetof(pack_t, fileList) == 0x31c);
FS_LAYOUT_ASSERT(fs_pack_size, sizeof(pack_t) == 0x320);
FS_LAYOUT_ASSERT(fs_searchpath_localized_offset,
                 offsetof(searchpath_t, localized) == 0x0c);
FS_LAYOUT_ASSERT(fs_searchpath_language_offset,
                 offsetof(searchpath_t, language) == 0x10);
FS_LAYOUT_ASSERT(fs_searchpath_size, sizeof(searchpath_t) == 0x14);
FS_LAYOUT_ASSERT(fs_handle_position_offset,
                 offsetof(fileHandleData_t, position) == 0x0c);
FS_LAYOUT_ASSERT(fs_handle_size_offset,
                 offsetof(fileHandleData_t, size) == 0x10);
FS_LAYOUT_ASSERT(fs_handle_zip_rewind_offset,
                 offsetof(fileHandleData_t, zipRewindOffset) == 0x14);
FS_LAYOUT_ASSERT(fs_handle_zip_archive_offset,
                 offsetof(fileHandleData_t, zipArchive) == 0x18);
FS_LAYOUT_ASSERT(fs_handle_seek_guard_offset,
                 offsetof(fileHandleData_t, seekCallbackGuard) == 0x1c);
FS_LAYOUT_ASSERT(fs_handle_name_offset,
                 offsetof(fileHandleData_t, name) == 0x20);
FS_LAYOUT_ASSERT(fs_handle_size, sizeof(fileHandleData_t) == 0x120);

#undef FS_LAYOUT_ASSERT
#endif

#endif
