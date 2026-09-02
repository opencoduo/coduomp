#ifndef QCOMMON_SCRIPT_TYPES_H
#define QCOMMON_SCRIPT_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "asset_type_names.h"
#include "q_shared_types.h"
#include "xanim_types.h"

/* Script VM value/object discriminator shared by the client engine, dedicated
 * engine, and game module. The engine's 19-entry stock type-name table fixes
 * this complete domain; Scr_GetPointerType returns the same discriminator for
 * the object referenced by a value of type SCRIPT_VAR_OBJECT. */
typedef enum script_variable_type_e {
    SCRIPT_VAR_UNDEFINED = 0,
    SCRIPT_VAR_STRING = 1,
    SCRIPT_VAR_LOCALIZED_STRING = 2,
    SCRIPT_VAR_VECTOR = 3,
    SCRIPT_VAR_FLOAT = 4,
    SCRIPT_VAR_INT = 5,
    SCRIPT_VAR_CODEPOS = 6,
    SCRIPT_VAR_OBJECT = 7,
    SCRIPT_VAR_KEY_VALUE = 8,
    SCRIPT_VAR_FUNCTION = 9,
    SCRIPT_VAR_STACK = 10,
    SCRIPT_VAR_ANIMATION = 11,
    SCRIPT_VAR_THREAD = 12,
    SCRIPT_VAR_ENTITY = 13,
    SCRIPT_VAR_STRUCT = 14,
    SCRIPT_VAR_ARRAY = 15,
    SCRIPT_VAR_DEAD_THREAD = 16,
    SCRIPT_VAR_DEAD_ENTITY = 17,
    SCRIPT_VAR_DEAD_OBJECT = 18,
    SCRIPT_VAR_COUNT = 19
} script_variable_type_t;

typedef char script_variable_type_size[
    sizeof(script_variable_type_t) == sizeof(uint32_t) ? 1 : -1];

/* Engine/game script callback-table indices.  CoDUOMP.exe and coduo_lnxded
 * each publish the same 102 import slots, and the Windows and Linux game
 * modules consume those slots in the same order before appending five game
 * exports.  Slots 54, 55, 75, and 78 are retained holes: neither engine
 * writes them. */
enum {
    SCRIPT_IMPORT_CALLBACK_COUNT = 102,
    SCRIPT_EXPORT_CALLBACK_COUNT = 5,
    SCRIPT_VALUE_STACK_COUNT = 2048
};

typedef enum script_import_id_e {
    SCRIPT_IMPORT_GET_BOOL = 0,
    SCRIPT_IMPORT_GET_INT = 1,
    SCRIPT_IMPORT_GET_ANIM = 2,
    SCRIPT_IMPORT_GET_ANIM_TREE = 3,
    SCRIPT_IMPORT_GET_FLOAT = 4,
    SCRIPT_IMPORT_GET_STRING = 5,
    SCRIPT_IMPORT_GET_CONST_STRING = 6,
    SCRIPT_IMPORT_GET_DEBUG_STRING = 7,
    SCRIPT_IMPORT_GET_ISTRING = 8,
    SCRIPT_IMPORT_GET_CONST_ISTRING = 9,
    SCRIPT_IMPORT_GET_VECTOR = 10,
    SCRIPT_IMPORT_GET_FUNC = 11,
    SCRIPT_IMPORT_GET_TYPE = 12,
    SCRIPT_IMPORT_GET_POINTER_TYPE = 13,
    SCRIPT_IMPORT_GET_ENTITY_NUM = 14,
    SCRIPT_IMPORT_GET_NUM_PARAM = 15,
    SCRIPT_IMPORT_ADD_BOOL = 16,
    SCRIPT_IMPORT_ADD_INT = 17,
    SCRIPT_IMPORT_ADD_FLOAT = 18,
    SCRIPT_IMPORT_ADD_ANIM = 19,
    SCRIPT_IMPORT_ADD_UNDEFINED = 20,
    SCRIPT_IMPORT_ADD_ENTITY_NUM = 21,
    SCRIPT_IMPORT_ADD_STRUCT = 22,
    SCRIPT_IMPORT_ADD_STRING = 23,
    SCRIPT_IMPORT_ADD_ISTRING = 24,
    SCRIPT_IMPORT_ADD_CONST_STRING = 25,
    SCRIPT_IMPORT_ADD_VECTOR = 26,
    SCRIPT_IMPORT_ADD_OBJECT = 27,
    SCRIPT_IMPORT_ADD_ARRAY = 28,
    SCRIPT_IMPORT_ADD_ARRAY_STRING_INDEXED = 29,
    SCRIPT_IMPORT_MAKE_ARRAY = 30,
    SCRIPT_IMPORT_BEGIN_LOAD_SCRIPTS = 31,
    SCRIPT_IMPORT_BEGIN_LOAD_ANIM_TREES = 32,
    SCRIPT_IMPORT_END_LOAD_SCRIPTS = 33,
    SCRIPT_IMPORT_END_LOAD_ANIM_TREES = 34,
    SCRIPT_IMPORT_PRECACHE_ANIM_TREES = 35,
    SCRIPT_IMPORT_FREE_SCRIPTS = 36,
    SCRIPT_IMPORT_FREE_GAME_VARIABLE = 37,
    SCRIPT_IMPORT_SHUTDOWN_SYSTEM = 38,
    SCRIPT_IMPORT_IS_SYSTEM_ACTIVE = 39,
    SCRIPT_IMPORT_ADD_EXEC_THREAD = 40,
    SCRIPT_IMPORT_ADD_EXEC_ENT_THREAD_NUM = 41,
    SCRIPT_IMPORT_EXEC_THREAD = 42,
    SCRIPT_IMPORT_EXEC_ENT_THREAD_NUM = 43,
    SCRIPT_IMPORT_IS_THREAD_ALIVE = 44,
    SCRIPT_IMPORT_ERROR = 45,
    SCRIPT_IMPORT_ERROR_WITH_DIALOG_MESSAGE = 46,
    SCRIPT_IMPORT_PARAM_ERROR = 47,
    SCRIPT_IMPORT_OBJECT_ERROR = 48,
    SCRIPT_IMPORT_SET_DYNAMIC_ENTITY_FIELD = 49,
    SCRIPT_IMPORT_FREE_ENTITY_NUM = 50,
    SCRIPT_IMPORT_GET_ENTITY_ID = 51,
    SCRIPT_IMPORT_SET_CLASS_MAP = 52,
    SCRIPT_IMPORT_REMOVE_CLASS_MAP = 53,
    SCRIPT_IMPORT_UNUSED_54 = 54,
    SCRIPT_IMPORT_UNUSED_55 = 55,
    SCRIPT_IMPORT_ADD_CLASS_FIELD = 56,
    SCRIPT_IMPORT_ADD_FIELDS = 57,
    SCRIPT_IMPORT_FIND_FIELD = 58,
    SCRIPT_IMPORT_GET_OFFSET = 59,
    SCRIPT_IMPORT_COPY_ENTITY_NUM = 60,
    SCRIPT_IMPORT_INIT = 61,
    SCRIPT_IMPORT_SHUTDOWN = 62,
    SCRIPT_IMPORT_ABORT = 63,
    SCRIPT_IMPORT_SET_LOADING = 64,
    SCRIPT_IMPORT_INIT_SYSTEM = 65,
    SCRIPT_IMPORT_ALLOC_GAME_VARIABLE = 66,
    SCRIPT_IMPORT_GET_CHECKSUM = 67,
    SCRIPT_IMPORT_HAS_SOURCE_FILES = 68,
    SCRIPT_IMPORT_SAVE_SOURCE = 69,
    SCRIPT_IMPORT_LOAD_SOURCE = 70,
    SCRIPT_IMPORT_SKIP_SOURCE = 71,
    SCRIPT_IMPORT_SAVE_PRE = 72,
    SCRIPT_IMPORT_SAVE_POST = 73,
    SCRIPT_IMPORT_SAVE_SHUTDOWN = 74,
    SCRIPT_IMPORT_UNUSED_75 = 75,
    SCRIPT_IMPORT_LOAD_PRE = 76,
    SCRIPT_IMPORT_LOAD_SHUTDOWN = 77,
    SCRIPT_IMPORT_UNUSED_78 = 78,
    SCRIPT_IMPORT_LOAD_SCRIPT = 79,
    SCRIPT_IMPORT_FIND_ANIM_TREE = 80,
    SCRIPT_IMPORT_FIND_ANIM = 81,
    SCRIPT_IMPORT_GET_FUNCTION_HANDLE = 82,
    SCRIPT_IMPORT_FREE_THREAD = 83,
    SCRIPT_IMPORT_CONVERT_THREAD_TO_SAVE = 84,
    SCRIPT_IMPORT_CONVERT_THREAD_FROM_LOAD = 85,
    SCRIPT_IMPORT_SET_STRING = 86,
    SCRIPT_IMPORT_ALLOC_STRING = 87,
    SCRIPT_IMPORT_NOTIFY_NUM = 88,
    SCRIPT_IMPORT_NOTIFY_ID = 89,
    SCRIPT_IMPORT_SL_CONVERT_TO_STRING = 90,
    SCRIPT_IMPORT_SL_GET_STRING = 91,
    SCRIPT_IMPORT_SL_GET_LOWERCASE_STRING = 92,
    SCRIPT_IMPORT_SL_FIND_LOWERCASE_STRING = 93,
    SCRIPT_IMPORT_CREATE_CANONICAL_FILENAME = 94,
    SCRIPT_IMPORT_SET_TIME = 95,
    SCRIPT_IMPORT_RUN_CURRENT_THREADS = 96,
    SCRIPT_IMPORT_RESET_TIMEOUT = 97,
    SCRIPT_IMPORT_GET_ANIMS_INDEX = 98,
    SCRIPT_IMPORT_GET_ANIMS = 99,
    SCRIPT_IMPORT_MT_ALLOC = 100,
    SCRIPT_IMPORT_MT_FREE = 101
} script_import_id_t;

typedef enum script_export_id_e {
    SCRIPT_EXPORT_GET_FUNCTION = 0,
    SCRIPT_EXPORT_GET_METHOD = 1,
    SCRIPT_EXPORT_SET_OBJECT_FIELD = 2,
    SCRIPT_EXPORT_GET_OBJECT_FIELD = 3,
    SCRIPT_EXPORT_LOAD_READ = 4
} script_export_id_t;

/* The game supplies these records and Scr_SetClassMap replaces classnum with
 * the engine's class-root handle.  Windows client/game and Linux engine/game
 * all use the same eight-byte i386 record. */
typedef struct script_class_map_entry_s {
    uint16_t classnum;
    uint8_t padding02[2];
    const char *name;
} script_class_map_entry_t;

/* Scr_GetAnimTree and Scr_FindAnimTree return this single-pointer aggregate
 * across the engine/game callback ABI. */
typedef struct script_anim_tree_ref_s {
    XAnim *tree;
} script_anim_tree_ref_t;

typedef void (*script_callback_fn_t)(void);
typedef void (*script_function_callback_t)(void);
typedef void (*script_method_callback_t)(uint32_t scriptObject);
typedef void *(*script_anim_tree_alloc_t)(size_t size);
typedef void (*script_source_io_fn_t)(void *buffer, int32_t byteCount);
typedef script_function_callback_t (*script_get_function_callback_t)(
    const char **name, int32_t *developerOnly);
typedef script_method_callback_t (*script_get_method_callback_t)(
    const char **name, int32_t *developerOnly);
typedef void (*script_object_field_callback_t)(
    int32_t classNum, int32_t objectNum, int32_t fieldIndex);
typedef void *(*script_load_read_callback_t)(uint32_t size);

/* One pointer-sized script callback-table cell.  The import ID or export ID,
 * not the union member, determines the live signature.  The Windows client
 * engine returns XAnim * directly from its slot-80 Scr_FindAnimTree callback;
 * Linux exposes the same one-pointer value through script_anim_tree_ref_t, so
 * both proven source interfaces remain available without changing the cell. */
typedef union script_vm_callback_slot_u {
    script_callback_fn_t generic;
    qboolean (*bool_from_u32)(uint32_t);
    int (*int_from_u32)(uint32_t);
    int (*int_from_int)(int);
    int (*int_from_cstring)(const char *);
    int (*int_from_u16)(uint16_t);
    int (*int_from_void)(void);
    qboolean (*bool_from_void)(void);
    qboolean (*bool_from_cstring)(const char *);
    qboolean (*bool_from_u16)(uint16_t);
    qboolean (*bool_from_u8)(uint8_t);
    qboolean (*bool_from_bool)(qboolean);
    uint16_t (*u16_from_u32)(uint32_t);
    uint16_t (*u16_from_u16)(uint16_t);
    uint16_t (*u16_from_cstring)(const char *);
    uint16_t (*u16_from_cstring_int)(const char *, int);
    uint16_t (*u16_from_cstring_intp)(const char *, int *);
    uint16_t (*u16_from_int_int)(int, int);
    uint16_t (*u16_from_u16_cstring)(uint16_t, const char *);
    uint16_t (*u16_from_u32_u32)(uint32_t, uint32_t);
    uint16_t (*u16_from_int_int_u32_u32)(int, int, uint32_t, uint32_t);
    uint16_t (*u16_from_voidp)(void *);
    uint32_t (*u32_from_void)(void);
    uint32_t (*u32_from_u32)(uint32_t);
    uint32_t (*u32_from_u16_cstring)(uint16_t, const char *);
    uint32_t (*u32_from_xanim)(XAnim *);
    uint32_t (*u32_from_u32_intp)(uint32_t, int *);
    uint16_t (*u16_from_u32_intp)(uint32_t, int *);
    uint32_t (*u32_from_cstring_cstring)(const char *, const char *);
    script_variable_type_t (*variable_type_from_u32)(uint32_t);
    scr_anim_t (*anim_ref_from_u32_xanimtree)(uint32_t, XAnimTree *);
    script_anim_tree_ref_t (*anim_tree_ref_from_u32)(uint32_t);
    script_anim_tree_ref_t (*anim_tree_ref_from_cstring)(const char *);
    XAnim *(*xanim_from_cstring)(const char *);
    float (*float_from_u32)(uint32_t);
    const char *(*cstring_from_u16)(uint16_t);
    const char *(*cstring_from_u32)(uint32_t);
    void *(*voidp_from_u16)(uint16_t);
    XAnim *(*xanim_from_u32)(uint32_t);
    void *(*voidp_from_size_int)(size_t, int);
    void (*void_from_void)(void);
    void (*void_from_bool)(qboolean);
    void (*void_from_int)(int);
    void (*void_from_u16)(uint16_t);
    void (*void_from_u32)(uint32_t);
    void (*void_from_float)(float);
    void (*void_from_cstring)(const char *);
    void (*void_from_cstring_cstring)(const char *, const char *);
    void (*void_from_int_cstring)(int32_t, const char *);
    void (*void_from_const_floatp)(const float *);
    void (*void_from_anim_tree_alloc)(script_anim_tree_alloc_t);
    void (*void_from_voidp)(void *);
    void (*void_from_u32p)(uint32_t *);
    void (*void_from_source_io)(script_source_io_fn_t);
    void (*void_from_voidp_size)(void *, size_t);
    void (*void_from_u32_floatp)(uint32_t, float *);
    void (*void_from_u8)(uint8_t);
    int (*int_from_u8)(uint8_t);
    uint16_t (*u16_from_cstring_u8)(const char *, uint8_t);
    void (*void_from_intp_cstring)(int *, const char *);
    void (*void_from_int_int)(int, int);
    void (*void_from_u32_u32)(uint32_t, uint32_t);
    void (*void_from_size_int)(size_t, int);
    void (*void_from_int_int_int)(int, int, int);
    void (*void_from_int_int_u16)(int, int, uint16_t);
    void (*void_from_int_int_u16_u32)(int, int, uint16_t, uint32_t);
    void (*void_from_int_int_u32_u32)(int, int, uint32_t, uint32_t);
    void (*void_from_u16_u16_u32)(uint16_t, uint16_t, uint32_t);
    void (*void_from_u16_cstring_u16)(uint16_t, const char *, uint16_t);
    void (*void_from_classmap_u32)(script_class_map_entry_t *, uint32_t);
    void (*void_from_cstring_voidp)(const char *, const void *);
    void (*void_from_cstring_cstring_voidp)(
        const char *, const char *, void *);
    void (*void_from_cstring_cstring_animref)(
        const char *, const char *, scr_anim_t *);
    void (*void_from_voidp_voidp_voidp)(void *, void *, void *);
    void (*void_from_voidp_u16)(uint16_t *, uint16_t);
    void (*void_from_voidp_int)(void *, int);
    void (*void_from_voidp_int_uint32)(void *, int, uint32_t);
    void (*void_from_voidp_int_int)(void *, int, int);
    script_get_function_callback_t function_lookup;
    script_get_method_callback_t method_lookup;
    script_object_field_callback_t object_field;
    script_load_read_callback_t load_read;
} script_vm_callback_slot_t;

#if UINTPTR_MAX == UINT32_MAX
typedef char script_class_map_entry_size[
    sizeof(script_class_map_entry_t) == 0x08 ? 1 : -1];
typedef char script_class_map_entry_name_offset[
    offsetof(script_class_map_entry_t, name) == 0x04 ? 1 : -1];
#endif
typedef char script_vm_callback_slot_size[
    sizeof(script_vm_callback_slot_t) == sizeof(script_callback_fn_t) ? 1 : -1];

#if defined(__cplusplus)
#define SCRIPT_TYPES_ALIGNOF(type_) alignof(type_)
#elif defined(_MSC_VER)
#define SCRIPT_TYPES_ALIGNOF(type_) __alignof(type_)
#elif defined(__GNUC__) || defined(__clang__)
/* Both game-module makefiles intentionally use C99.  GCC and Clang expose
 * their alignment operator in that language mode without the C11-pedantic
 * diagnostic produced by _Alignof. */
#define SCRIPT_TYPES_ALIGNOF(type_) __alignof__(type_)
#else
#define SCRIPT_TYPES_ALIGNOF(type_) _Alignof(type_)
#endif

#if UINTPTR_MAX == UINT32_MAX
typedef char script_class_map_entry_alignment[
    SCRIPT_TYPES_ALIGNOF(script_class_map_entry_t) == 0x04 ? 1 : -1];
typedef char script_vm_callback_slot_alignment[
    SCRIPT_TYPES_ALIGNOF(script_vm_callback_slot_t) == 0x04 ? 1 : -1];
#endif

#undef SCRIPT_TYPES_ALIGNOF

#endif
