#include <stdint.h>

#include "script_callbacks.h"
#include "script_anim.h"
#include "script_import_fields.h"
#include "script_memory.h"
#include "script_runtime_host.h"
#include "script_runtime_state.h"
#include "script_serialization.h"
#include "script_string.h"
#include "script_value.h"
#include "script_variable.h"

/*
 * The original i386 table stores these core function addresses directly.
 * The game-side wrappers nevertheless forward legacy arguments which the
 * engine implementations do not consume; caller-cleaned i386 cdecl tolerates
 * that machine-code contract. Calling through those incompatible C function
 * types is not valid portable C, and native 64-bit ABIs are not required to
 * preserve it. Use adapters only on wider hosts, while the i386 build keeps
 * the original direct table entries. Scr_AllocString deliberately retains the
 * engine's stock hard-coded string user (1), ignoring the redundant game arg.
 */
#if UINTPTR_MAX > UINT32_MAX
/* NOT_FROM_ORIGINAL_SOURCE: native callback-signature adapter. */
static void coduo_script_callback_save_pre(void *unusedFile)
{
    (void)unusedFile;
    Scr_SavePre();
}

/* NOT_FROM_ORIGINAL_SOURCE: native callback-signature adapter. */
static void coduo_script_callback_save_post(void *unusedFile)
{
    (void)unusedFile;
    Scr_SavePost();
}

/* NOT_FROM_ORIGINAL_SOURCE: native callback-signature adapter. */
static void coduo_script_callback_load_pre(void *unusedFile, int32_t unusedScriptRunning)
{
    (void)unusedFile;
    (void)unusedScriptRunning;
    Scr_LoadPre();
}

/* NOT_FROM_ORIGINAL_SOURCE: native callback-signature adapter. */
static uint16_t coduo_script_callback_alloc_string(const char *text, int32_t unusedUser)
{
    (void)unusedUser;
    return Scr_AllocString(text);
}

/* NOT_FROM_ORIGINAL_SOURCE: native callback-signature adapter. */
static void coduo_script_callback_free_scripts(uint8_t unusedMode)
{
    (void)unusedMode;
    Scr_FreeScripts();
}

#if defined(WINDOWS_BEHAVIOR)
/* NOT_FROM_ORIGINAL_SOURCE: native callback-signature adapter. */
static uint16_t coduo_script_callback_sl_get_string(const char *text, uint8_t user)
{
    return SL_GetString(text, (int32_t)user);
}

/* NOT_FROM_ORIGINAL_SOURCE: native callback-signature adapter. */
static uint16_t coduo_script_callback_sl_get_lowercase_string(const char *text, uint8_t user)
{
    return SL_GetLowercaseString(text, (int32_t)user);
}

/* NOT_FROM_ORIGINAL_SOURCE: native callback-signature adapter. */
static void *coduo_script_callback_mt_alloc(size_t size, int32_t unusedType)
{
    (void)unusedType;
    return MT_Alloc(size);
}
#endif

#define CODUO_SCRIPT_CALLBACK_SAVE_PRE coduo_script_callback_save_pre
#define CODUO_SCRIPT_CALLBACK_SAVE_POST coduo_script_callback_save_post
#define CODUO_SCRIPT_CALLBACK_LOAD_PRE coduo_script_callback_load_pre
#define CODUO_SCRIPT_CALLBACK_ALLOC_STRING coduo_script_callback_alloc_string
#define CODUO_SCRIPT_CALLBACK_FREE_SCRIPTS coduo_script_callback_free_scripts
#if defined(WINDOWS_BEHAVIOR)
#define CODUO_SCRIPT_CALLBACK_SL_GET_STRING coduo_script_callback_sl_get_string
#define CODUO_SCRIPT_CALLBACK_SL_GET_LOWERCASE_STRING coduo_script_callback_sl_get_lowercase_string
#define CODUO_SCRIPT_CALLBACK_MT_ALLOC coduo_script_callback_mt_alloc
#else
#define CODUO_SCRIPT_CALLBACK_SL_GET_STRING SL_GetString
#define CODUO_SCRIPT_CALLBACK_SL_GET_LOWERCASE_STRING SL_GetLowercaseString
#define CODUO_SCRIPT_CALLBACK_MT_ALLOC MT_Alloc
#endif
#else
/* The casts below are the original i386 cdecl contract described above:
 * the game pushes legacy arguments, the engine ignores them, and the caller
 * removes the stack arguments. Keep this incompatibility isolated here. The
 * warning suppression covers only this proved stock-i386 boundary; replacing
 * the functions with adapters here would change its original machine shape. */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#define CODUO_SCRIPT_CALLBACK_SAVE_PRE ((void (*)(void *))Scr_SavePre)
#define CODUO_SCRIPT_CALLBACK_SAVE_POST ((void (*)(void *))Scr_SavePost)
#define CODUO_SCRIPT_CALLBACK_LOAD_PRE ((void (*)(void *, int32_t))Scr_LoadPre)
#define CODUO_SCRIPT_CALLBACK_ALLOC_STRING ((uint16_t (*)(const char *, int32_t))Scr_AllocString)
#define CODUO_SCRIPT_CALLBACK_FREE_SCRIPTS ((void (*)(uint8_t))Scr_FreeScripts)
#if defined(WINDOWS_BEHAVIOR)
#define CODUO_SCRIPT_CALLBACK_SL_GET_STRING ((uint16_t (*)(const char *, uint8_t))SL_GetString)
#define CODUO_SCRIPT_CALLBACK_SL_GET_LOWERCASE_STRING ((uint16_t (*)(const char *, uint8_t))SL_GetLowercaseString)
#define CODUO_SCRIPT_CALLBACK_MT_ALLOC ((void *(*)(size_t, int32_t))MT_Alloc)
#else
#define CODUO_SCRIPT_CALLBACK_SL_GET_STRING SL_GetString
#define CODUO_SCRIPT_CALLBACK_SL_GET_LOWERCASE_STRING SL_GetLowercaseString
#define CODUO_SCRIPT_CALLBACK_MT_ALLOC MT_Alloc
#endif
#endif

#define SCRIPT_IMPORT_ASSIGN(importId, member, callback) script_importCallbacks[(importId)].member = (callback)

/* Source: CoDUOMP.exe 0x0047f950..0x0047f95b and coduo_lnxded
 * 0x080a1aee..0x080a1b00.
 * Name and signature: exact same-module Mac symbol Scr_GetFunction. */
script_function_callback_t Scr_GetFunction(const char **name, int32_t *developerOnly)
{
    return script_exportCallbacks[SCRIPT_EXPORT_GET_FUNCTION].function_lookup(name, developerOnly);
}

/* Source: CoDUOMP.exe 0x0047f960..0x0047f96b and coduo_lnxded
 * 0x080a1b02..0x080a1b14.
 * Name and signature: exact same-module Mac symbol Scr_GetMethod. */
script_method_callback_t Scr_GetMethod(const char **name, int32_t *developerOnly)
{
    return script_exportCallbacks[SCRIPT_EXPORT_GET_METHOD].method_lookup(name, developerOnly);
}

/* Source: CoDUOMP.exe 0x0047f970..0x0047f97c.
 * Name and signature: exact same-module Mac symbol Scr_SetObjectField. */
void Scr_SetObjectField(int32_t classNum, int32_t objectNum, int32_t fieldIndex)
{
    script_exportCallbacks[SCRIPT_EXPORT_SET_OBJECT_FIELD].object_field(classNum, objectNum, fieldIndex);
}

/* Source: CoDUOMP.exe 0x0047f980..0x0047f98c.
 * Name and signature: exact same-module Mac symbol Scr_GetObjectField. */
void Scr_GetObjectField(int32_t classNum, int32_t objectNum, int32_t fieldIndex)
{
    script_exportCallbacks[SCRIPT_EXPORT_GET_OBJECT_FIELD].object_field(classNum, objectNum, fieldIndex);
}

/* Source: CoDUOMP.exe 0x0047f990..0x0047f99a.
 * Name and signature: exact same-module Mac symbol Scr_LoadRead. */
void *Scr_LoadRead(uint32_t size)
{
    return script_exportCallbacks[SCRIPT_EXPORT_LOAD_READ].load_read(size);
}

/* Source: CoDUOMP.exe 0x0047f9a0..0x0047fda8 and coduo_lnxded
 * 0x080a1b3f..0x080a1f4c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f9a0_0047fda9.mcode.
 * Name: exact same-module Mac symbol Scr_NearHook. Both authoritative targets
 * populate the same 102 slots and retain the same four holes. */
script_vm_callback_slot_t *Scr_NearHook(const script_vm_callback_slot_t *gameCallbacks)
{
    if (gameCallbacks != NULL) {
        for (script_export_id_t exportId = SCRIPT_EXPORT_GET_FUNCTION; exportId <= SCRIPT_EXPORT_LOAD_READ; ++exportId) {
            script_exportCallbacks[exportId] = gameCallbacks[exportId];
        }
    }

    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_BOOL, bool_from_u32, Scr_GetBool);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_INT, int_from_u32, Scr_GetInt);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ANIM, anim_ref_from_u32_xanimtree, Scr_GetAnim);
#if defined(WINDOWS_BEHAVIOR)
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ANIM_TREE, xanim_from_u32, Scr_GetAnimTree);
#else
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ANIM_TREE, anim_tree_ref_from_u32, Scr_GetAnimTree);
#endif
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_FLOAT, float_from_u32, Scr_GetFloat);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_STRING, cstring_from_u32, Scr_GetString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_CONST_STRING, u16_from_u32, Scr_GetConstString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_DEBUG_STRING, cstring_from_u32, Scr_GetDebugString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ISTRING, cstring_from_u32, Scr_GetIString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_CONST_ISTRING, u16_from_u32, Scr_GetConstIString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_VECTOR, void_from_u32_floatp, Scr_GetVector);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_FUNC, u32_from_u32, Scr_GetFunc);
#if defined(WINDOWS_BEHAVIOR)
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ENTITY_NUM, u16_from_u32_intp, Scr_GetEntityNum);
#else
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ENTITY_NUM, u32_from_u32_intp, Scr_GetEntityNum);
#endif
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_TYPE, variable_type_from_u32, Scr_GetType);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_POINTER_TYPE, variable_type_from_u32, Scr_GetPointerType);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_NUM_PARAM, u32_from_void, Scr_GetNumParam);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_BOOL, void_from_bool, Scr_AddBool);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_INT, void_from_int, Scr_AddInt);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_FLOAT, void_from_float, Scr_AddFloat);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_ANIM, void_from_u32, Scr_AddAnim);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_UNDEFINED, void_from_void, Scr_AddUndefined);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_ENTITY_NUM, void_from_int_int, Scr_AddEntityNum);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_STRUCT, void_from_void, Scr_AddStruct);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_STRING, void_from_cstring, Scr_AddString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_ISTRING, void_from_cstring, Scr_AddIString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_CONST_STRING, void_from_u16, Scr_AddConstString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_VECTOR, void_from_const_floatp, Scr_AddVector);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_OBJECT, void_from_u16, Scr_AddObject);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_MAKE_ARRAY, void_from_void, Scr_MakeArray);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_ARRAY, void_from_void, Scr_AddArray);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_ARRAY_STRING_INDEXED, void_from_u16, Scr_AddArrayStringIndexed);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ERROR, void_from_cstring, Scr_Error);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ERROR_WITH_DIALOG_MESSAGE, void_from_cstring_cstring, Scr_ErrorWithDialogMessage);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_PARAM_ERROR, void_from_int_cstring, Scr_ParamError);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_OBJECT_ERROR, void_from_cstring, Scr_ObjectError);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SET_DYNAMIC_ENTITY_FIELD, void_from_int_int_u16, Scr_SetDynamicEntityField);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_FREE_ENTITY_NUM, void_from_int_int, Scr_FreeEntityNum);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ENTITY_ID, u16_from_int_int, Scr_GetEntityId);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SET_CLASS_MAP, void_from_classmap_u32, Scr_SetClassMap);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_REMOVE_CLASS_MAP, void_from_void, Scr_RemoveClassMap);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_CLASS_FIELD, void_from_u16_cstring_u16, Scr_AddClassField);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_FIELDS, void_from_cstring_cstring, Scr_AddFields);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_FIND_FIELD, u16_from_cstring_intp, Scr_FindField);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_OFFSET, u32_from_u16_cstring, Scr_GetOffset);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_COPY_ENTITY_NUM, void_from_int_int_int, Scr_CopyEntityNum);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_INIT, void_from_int_int_int, Scr_Init);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SHUTDOWN, void_from_void, Scr_Shutdown);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ABORT, void_from_void, Scr_Abort);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SET_LOADING, void_from_int, Scr_SetLoading);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_INIT_SYSTEM, void_from_u32_u32, Scr_InitSystem);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ALLOC_GAME_VARIABLE, void_from_void, Scr_AllocGameVariable);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_CHECKSUM, void_from_u32p, Scr_GetChecksum);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_HAS_SOURCE_FILES, bool_from_void, Scr_HasSourceFiles);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SAVE_SOURCE, void_from_source_io, Scr_SaveSource);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_LOAD_SOURCE, void_from_source_io, Scr_LoadSource);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SKIP_SOURCE, void_from_source_io, Scr_SkipSource);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SAVE_PRE, void_from_voidp, CODUO_SCRIPT_CALLBACK_SAVE_PRE);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SAVE_POST, void_from_voidp, CODUO_SCRIPT_CALLBACK_SAVE_POST);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SAVE_SHUTDOWN, void_from_void, Scr_SaveShutdown);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_LOAD_PRE, void_from_voidp_int, CODUO_SCRIPT_CALLBACK_LOAD_PRE);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_LOAD_SHUTDOWN, void_from_void, Scr_LoadShutdown);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_LOAD_SCRIPT, bool_from_cstring, Scr_LoadScript);
#if defined(WINDOWS_BEHAVIOR)
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_FIND_ANIM_TREE, xanim_from_cstring, Scr_FindAnimTree);
#else
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_FIND_ANIM_TREE, anim_tree_ref_from_cstring, Scr_FindAnimTree);
#endif
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_FIND_ANIM, void_from_cstring_cstring_animref, Scr_FindAnim);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_FUNCTION_HANDLE, u32_from_cstring_cstring, Scr_GetFunctionHandle);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_EXEC_THREAD, u16_from_u32_u32, Scr_ExecThread);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_EXEC_ENT_THREAD_NUM, u16_from_int_int_u32_u32, Scr_ExecEntThreadNum);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_IS_THREAD_ALIVE, bool_from_u16, Scr_IsThreadAlive);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_BEGIN_LOAD_SCRIPTS, void_from_void, Scr_BeginLoadScripts);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_BEGIN_LOAD_ANIM_TREES, void_from_void, Scr_BeginLoadAnimTrees);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_END_LOAD_SCRIPTS, void_from_void, Scr_EndLoadScripts);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_END_LOAD_ANIM_TREES, void_from_void, Scr_EndLoadAnimTrees);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_PRECACHE_ANIM_TREES, void_from_anim_tree_alloc, Scr_PrecacheAnimTrees);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_FREE_SCRIPTS, void_from_u8, CODUO_SCRIPT_CALLBACK_FREE_SCRIPTS);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_FREE_GAME_VARIABLE, void_from_bool, Scr_FreeGameVariable);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SHUTDOWN_SYSTEM, void_from_u8, Scr_ShutdownSystem);
#if defined(WINDOWS_BEHAVIOR)
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_IS_SYSTEM_ACTIVE, bool_from_bool, Scr_IsSystemActive);
#else
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_IS_SYSTEM_ACTIVE, bool_from_u8, Scr_IsSystemActive);
#endif
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_EXEC_THREAD, void_from_u32_u32, Scr_AddExecThread);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ADD_EXEC_ENT_THREAD_NUM, void_from_int_int_u32_u32, Scr_AddExecEntThreadNum);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_FREE_THREAD, void_from_u16, Scr_FreeThread);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_CONVERT_THREAD_TO_SAVE, u16_from_u16, Scr_ConvertThreadToSave);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_CONVERT_THREAD_FROM_LOAD, u16_from_u16, Scr_ConvertThreadFromLoad);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SET_STRING, void_from_voidp_u16, Scr_SetString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_ALLOC_STRING, u16_from_cstring_int, CODUO_SCRIPT_CALLBACK_ALLOC_STRING);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_NOTIFY_NUM, void_from_int_int_u16_u32, Scr_NotifyNum);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_NOTIFY_ID, void_from_u16_u16_u32, Scr_NotifyId);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SL_CONVERT_TO_STRING, cstring_from_u16, SL_ConvertToString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SL_GET_STRING, u16_from_cstring_u8, CODUO_SCRIPT_CALLBACK_SL_GET_STRING);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SL_GET_LOWERCASE_STRING, u16_from_cstring_u8, CODUO_SCRIPT_CALLBACK_SL_GET_LOWERCASE_STRING);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SL_FIND_LOWERCASE_STRING, u16_from_cstring, SL_FindLowercaseString);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_CREATE_CANONICAL_FILENAME, u16_from_cstring, Scr_CreateCanonicalFilename);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_SET_TIME, void_from_u32, Scr_SetTime);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_RUN_CURRENT_THREADS, void_from_void, Scr_RunCurrentThreads);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_RESET_TIMEOUT, void_from_void, Scr_ResetTimeout);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ANIMS_INDEX, u32_from_xanim, Scr_GetAnimsIndex);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_GET_ANIMS, xanim_from_u32, Scr_GetAnims);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_MT_ALLOC, voidp_from_size_int, CODUO_SCRIPT_CALLBACK_MT_ALLOC);
    SCRIPT_IMPORT_ASSIGN(SCRIPT_IMPORT_MT_FREE, void_from_voidp_size, MT_Free);

    return script_importCallbacks;
}

#if UINTPTR_MAX == UINT32_MAX
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

#undef SCRIPT_IMPORT_ASSIGN
#undef CODUO_SCRIPT_CALLBACK_SAVE_PRE
#undef CODUO_SCRIPT_CALLBACK_SAVE_POST
#undef CODUO_SCRIPT_CALLBACK_LOAD_PRE
#undef CODUO_SCRIPT_CALLBACK_ALLOC_STRING
#undef CODUO_SCRIPT_CALLBACK_FREE_SCRIPTS
#undef CODUO_SCRIPT_CALLBACK_SL_GET_STRING
#undef CODUO_SCRIPT_CALLBACK_SL_GET_LOWERCASE_STRING
#undef CODUO_SCRIPT_CALLBACK_MT_ALLOC
