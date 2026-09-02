/*
 * Maintained ABI bridge for script VM callbacks.
 *
 * Each wrapper below is backed by its original one-call machine-code body.
 * Scr_FarHook copies 0x66 host callback words, then publishes the game callback
 * exports. The macros keep that evidenced ABI usable from maintained C without
 * changing the original wrapper interfaces.
 */

#include "game_functions.h"
#include "scr_vm.h"

enum {
    SCRIPT_VM_EXPORT_GET_FUNCTION =
        SCRIPT_IMPORT_CALLBACK_COUNT + SCRIPT_EXPORT_GET_FUNCTION,
    SCRIPT_VM_EXPORT_GET_METHOD =
        SCRIPT_IMPORT_CALLBACK_COUNT + SCRIPT_EXPORT_GET_METHOD,
    SCRIPT_VM_EXPORT_SET_OBJECT_FIELD =
        SCRIPT_IMPORT_CALLBACK_COUNT + SCRIPT_EXPORT_SET_OBJECT_FIELD,
    SCRIPT_VM_EXPORT_GET_OBJECT_FIELD =
        SCRIPT_IMPORT_CALLBACK_COUNT + SCRIPT_EXPORT_GET_OBJECT_FIELD,
    SCRIPT_VM_EXPORT_LOAD_READ =
        SCRIPT_IMPORT_CALLBACK_COUNT + SCRIPT_EXPORT_LOAD_READ,
    SCRIPT_VM_CALLBACK_COUNT =
        SCRIPT_IMPORT_CALLBACK_COUNT + SCRIPT_EXPORT_CALLBACK_COUNT
};

typedef char script_vm_callback_slot_size_must_match[
    sizeof(script_vm_callback_slot_t) == sizeof(script_callback_fn_t) ?
        1 :
        -1];

static script_vm_callback_slot_t scriptVmCallbacks[SCRIPT_VM_CALLBACK_COUNT];

#define SCRIPT_VM_CALL(slot, member) (scriptVmCallbacks[(slot)].member)

#define DEFINE_VOID_0(name, slot, member) \
    void name(void) { SCRIPT_VM_CALL((slot), member)(); }

#define DEFINE_VOID_1(name, slot, member, type1, arg1) \
    void name(type1 arg1) { SCRIPT_VM_CALL((slot), member)(arg1); }

#define DEFINE_VOID_2(name, slot, member, type1, arg1, type2, arg2) \
    void name(type1 arg1, type2 arg2) \
    { \
        SCRIPT_VM_CALL((slot), member)(arg1, arg2); \
    }

#define DEFINE_VOID_3(name, slot, member, type1, arg1, type2, arg2, type3, arg3) \
    void name(type1 arg1, type2 arg2, type3 arg3) \
    { \
        SCRIPT_VM_CALL((slot), member)(arg1, arg2, arg3); \
    }

#define DEFINE_VOID_4(name, slot, member, type1, arg1, type2, arg2, type3, arg3, \
                      type4, arg4) \
    void name(type1 arg1, type2 arg2, type3 arg3, type4 arg4) \
    { \
        SCRIPT_VM_CALL((slot), member)(arg1, arg2, arg3, arg4); \
    }

#define DEFINE_RET_0(ret, name, slot, member) \
    ret name(void) { return SCRIPT_VM_CALL((slot), member)(); }

#define DEFINE_RET_1(ret, name, slot, member, type1, arg1) \
    ret name(type1 arg1) { return SCRIPT_VM_CALL((slot), member)(arg1); }

#define DEFINE_RET_2(ret, name, slot, member, type1, arg1, type2, arg2) \
    ret name(type1 arg1, type2 arg2) \
    { \
        return SCRIPT_VM_CALL((slot), member)(arg1, arg2); \
    }

#define DEFINE_RET_3(ret, name, slot, member, type1, arg1, type2, arg2, type3, \
                     arg3) \
    ret name(type1 arg1, type2 arg2, type3 arg3) \
    { \
        return SCRIPT_VM_CALL((slot), member)(arg1, arg2, arg3); \
    }

#define DEFINE_RET_4(ret, name, slot, member, type1, arg1, type2, arg2, type3, \
                     arg3, type4, arg4) \
    ret name(type1 arg1, type2 arg2, type3 arg3, type4 arg4) \
    { \
        return SCRIPT_VM_CALL((slot), member)(arg1, arg2, arg3, arg4); \
    }

/* VERIFIED_DECOMPILER(0x94a94, a4a94_Scr_GetBool.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot 0 and preserves the callback return as qboolean. */
DEFINE_RET_1(qboolean, Scr_GetBool, SCRIPT_IMPORT_GET_BOOL, bool_from_u32,
             uint32_t, index)
/* VERIFIED_DECOMPILER(0x94aba, a4aba_Scr_GetInt.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(int, Scr_GetInt, SCRIPT_IMPORT_GET_INT, int_from_u32, uint32_t,
             index)
/* VERIFIED_DECOMPILER(0x94ae0, a4ae0_Scr_GetAnim.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot using the original i386 struct-return ABI. */
DEFINE_RET_2(scr_anim_t, Scr_GetAnim, SCRIPT_IMPORT_GET_ANIM,
             anim_ref_from_u32_xanimtree, uint32_t, index, XAnimTree *,
             runtimeTree)
/* VERIFIED_DECOMPILER(0x94b1d, a4b1d_Scr_GetAnimTree.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot using the original i386 struct-return ABI. */
DEFINE_RET_1(script_anim_tree_ref_t, Scr_GetAnimTree,
             SCRIPT_IMPORT_GET_ANIM_TREE, anim_tree_ref_from_u32, uint32_t,
             index)
/* VERIFIED_DECOMPILER(0x94b53, a4b53_Scr_GetFloat.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(float, Scr_GetFloat, SCRIPT_IMPORT_GET_FLOAT, float_from_u32,
             uint32_t, index)
/* VERIFIED_DECOMPILER(0x94b79, a4b79_Scr_GetString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(const char *, Scr_GetString, SCRIPT_IMPORT_GET_STRING,
             cstring_from_u32, uint32_t, index)
/* VERIFIED_DECOMPILER(0x94b9f, a4b9f_Scr_GetConstString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(uint16_t, Scr_GetConstString, SCRIPT_IMPORT_GET_CONST_STRING,
             u16_from_u32, uint32_t, index)
/* VERIFIED_DECOMPILER(0x94bc8, a4bc8_Scr_GetDebugString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(const char *, Scr_GetDebugString, SCRIPT_IMPORT_GET_DEBUG_STRING,
             cstring_from_u32, uint32_t, index)
/* VERIFIED_DECOMPILER(0x94bee, a4bee_Scr_GetIString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(const char *, Scr_GetIString, SCRIPT_IMPORT_GET_ISTRING,
             cstring_from_u32, uint32_t, index)
/* VERIFIED_DECOMPILER(0x94c14, a4c14_Scr_GetConstIString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(uint16_t, Scr_GetConstIString, SCRIPT_IMPORT_GET_CONST_ISTRING,
             u16_from_u32, uint32_t, index)
/* VERIFIED_DECOMPILER(0x94c3d, a4c3d_Scr_GetVector.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_GetVector, SCRIPT_IMPORT_GET_VECTOR, void_from_u32_floatp,
              uint32_t, index, float *, value)
/* VERIFIED_DECOMPILER(0x94c6a, a4c6a_Scr_GetFunc.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(uint32_t, Scr_GetFunc, SCRIPT_IMPORT_GET_FUNC, u32_from_u32,
             uint32_t, index)
/* VERIFIED_DECOMPILER(0x94cbd, a4cbd_Scr_GetType.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(script_variable_type_t, Scr_GetType, SCRIPT_IMPORT_GET_TYPE,
             variable_type_from_u32,
             uint32_t, index)
/* VERIFIED_DECOMPILER(0x94ce3, a4ce3_Scr_GetPointerType.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(script_variable_type_t, Scr_GetPointerType,
             SCRIPT_IMPORT_GET_POINTER_TYPE,
             variable_type_from_u32, uint32_t, index)
/* VERIFIED_DECOMPILER(0x94c90, a4c90_Scr_GetEntityNum.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_2(uint32_t, Scr_GetEntityNum, SCRIPT_IMPORT_GET_ENTITY_NUM,
             u32_from_u32_intp, uint32_t, index, int *, classnum)
/* VERIFIED_DECOMPILER(0x94d09, a4d09_Scr_GetNumParam.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_0(uint32_t, Scr_GetNumParam, SCRIPT_IMPORT_GET_NUM_PARAM,
             u32_from_void)
/* VERIFIED_DECOMPILER(0x94d29, a4d29_Scr_AddBool.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddBool, SCRIPT_IMPORT_ADD_BOOL, void_from_bool, qboolean,
              value)
/* VERIFIED_DECOMPILER(0x94d4f, a4d4f_Scr_AddInt.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddInt, SCRIPT_IMPORT_ADD_INT, void_from_int, int, value)
/* VERIFIED_DECOMPILER(0x94d75, a4d75_Scr_AddFloat.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddFloat, SCRIPT_IMPORT_ADD_FLOAT, void_from_float, float,
              value)
/* VERIFIED_DECOMPILER(0x94d9b, a4d9b_Scr_AddAnim.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddAnim, SCRIPT_IMPORT_ADD_ANIM, void_from_u32, uint32_t,
              anim)
/* VERIFIED_DECOMPILER(0x94dc1, a4dc1_Scr_AddUndefined.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_AddUndefined, SCRIPT_IMPORT_ADD_UNDEFINED, void_from_void)
/* VERIFIED_DECOMPILER(0x94de1, a4de1_Scr_AddEntityNum.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_AddEntityNum, SCRIPT_IMPORT_ADD_ENTITY_NUM,
              void_from_int_int, int, entityNum, int, classnum)
/* VERIFIED_DECOMPILER(0x94e0e, a4e0e_Scr_AddStruct.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_AddStruct, SCRIPT_IMPORT_ADD_STRUCT, void_from_void)
/* VERIFIED_DECOMPILER(0x94e2e, a4e2e_Scr_AddString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddString, SCRIPT_IMPORT_ADD_STRING, void_from_cstring,
              const char *, value)
/* VERIFIED_DECOMPILER(0x94e54, a4e54_Scr_AddIString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddIString, SCRIPT_IMPORT_ADD_ISTRING, void_from_cstring,
              const char *, value)
/* VERIFIED_DECOMPILER(0x94e7a, a4e7a_Scr_AddConstString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddConstString, SCRIPT_IMPORT_ADD_CONST_STRING,
              void_from_u16, uint16_t, value)
/* VERIFIED_DECOMPILER(0x94ea8, a4ea8_Scr_AddVector.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddVector, SCRIPT_IMPORT_ADD_VECTOR, void_from_const_floatp,
              const float *, value)
/* VERIFIED_DECOMPILER(0x94ece, a4ece_Scr_AddObject.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddObject, SCRIPT_IMPORT_ADD_OBJECT, void_from_u16,
              uint16_t, objectId)
/* VERIFIED_DECOMPILER(0x94f1c, a4f1c_Scr_AddArray.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_AddArray, SCRIPT_IMPORT_ADD_ARRAY, void_from_void)
/* VERIFIED_DECOMPILER(0x94f3c, a4f3c_Scr_AddArrayStringIndexed.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_AddArrayStringIndexed, SCRIPT_IMPORT_ADD_ARRAY_STRING_INDEXED,
              void_from_u16, uint16_t, key)
/* VERIFIED_DECOMPILER(0x94efc, a4efc_Scr_MakeArray.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_MakeArray, SCRIPT_IMPORT_MAKE_ARRAY, void_from_void)
/* VERIFIED_DECOMPILER(0x95515, a5515_Scr_BeginLoadScripts.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_BeginLoadScripts, SCRIPT_IMPORT_BEGIN_LOAD_SCRIPTS,
              void_from_void)
/* VERIFIED_DECOMPILER(0x95535, a5535_Scr_BeginLoadAnimTrees.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_BeginLoadAnimTrees, SCRIPT_IMPORT_BEGIN_LOAD_ANIM_TREES,
              void_from_void)
/* VERIFIED_DECOMPILER(0x95555, a5555_Scr_EndLoadScripts.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_EndLoadScripts, SCRIPT_IMPORT_END_LOAD_SCRIPTS,
              void_from_void)
/* VERIFIED_DECOMPILER(0x95575, a5575_Scr_EndLoadAnimTrees.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_EndLoadAnimTrees, SCRIPT_IMPORT_END_LOAD_ANIM_TREES,
              void_from_void)
/* VERIFIED_DECOMPILER(0x95595, a5595_Scr_PrecacheAnimTrees.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_PrecacheAnimTrees, SCRIPT_IMPORT_PRECACHE_ANIM_TREES,
              void_from_anim_tree_alloc, script_anim_tree_alloc_t, alloc)
/* VERIFIED_DECOMPILER(0x955bb, a55bb_Scr_FreeScripts.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_FreeScripts, SCRIPT_IMPORT_FREE_SCRIPTS, void_from_u8,
              uint8_t, mode)
/* VERIFIED_DECOMPILER(0x955e8, a55e8_Scr_FreeGameVariable.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_FreeGameVariable, SCRIPT_IMPORT_FREE_GAME_VARIABLE,
              void_from_bool, qboolean, freeAll)
/* VERIFIED_DECOMPILER(0x9560e, a560e_Scr_ShutdownSystem.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_ShutdownSystem, SCRIPT_IMPORT_SHUTDOWN_SYSTEM,
              void_from_u8, uint8_t, mode)
/* VERIFIED_DECOMPILER(0x9563b, a563b_Scr_IsSystemActive.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; narrows the incoming system argument to one byte before forwarding through script VM callback slot 39, then preserves the full qboolean return. */
DEFINE_RET_1(qboolean, Scr_IsSystemActive, SCRIPT_IMPORT_IS_SYSTEM_ACTIVE,
             bool_from_u8, uint8_t, system)
/* VERIFIED_DECOMPILER(0x956d6, a56d6_Scr_AddExecThread.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_AddExecThread, SCRIPT_IMPORT_ADD_EXEC_THREAD,
              void_from_u32_u32, uint32_t, handle, uint32_t, paramCount)
/* VERIFIED_DECOMPILER(0x95703, a5703_Scr_AddExecEntThreadNum.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_4(Scr_AddExecEntThreadNum, SCRIPT_IMPORT_ADD_EXEC_ENT_THREAD_NUM,
              void_from_int_int_u32_u32, int32_t, entityNum, int32_t, classnum,
              uint32_t, handle, uint32_t, paramCount)
/* VERIFIED_DECOMPILER(0x95668, a5668_Scr_ExecThread.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_2(uint16_t, Scr_ExecThread, SCRIPT_IMPORT_EXEC_THREAD,
             u16_from_u32_u32, uint32_t, handle, uint32_t, paramCount)
/* VERIFIED_DECOMPILER(0x95698, a5698_Scr_ExecEntThreadNum.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_4(uint16_t, Scr_ExecEntThreadNum, SCRIPT_IMPORT_EXEC_ENT_THREAD_NUM,
             u16_from_int_int_u32_u32, int32_t, entityNum, int32_t, classnum,
             uint32_t, handle, uint32_t, paramCount)
/* VERIFIED_DECOMPILER(0x9573e, a573e_Scr_IsThreadAlive.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(qboolean, Scr_IsThreadAlive, SCRIPT_IMPORT_IS_THREAD_ALIVE,
             bool_from_u16, uint16_t, threadId)
/* VERIFIED_DECOMPILER(0x94f6a, a4f6a_Scr_Error.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_Error, SCRIPT_IMPORT_ERROR, void_from_cstring, const char *,
              message)
/* VERIFIED_DECOMPILER(0x94f90, a4f90_Scr_ErrorWithDialogMessage.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_ErrorWithDialogMessage,
              SCRIPT_IMPORT_ERROR_WITH_DIALOG_MESSAGE,
              void_from_cstring_cstring,
              const char *, message, const char *, dialogMessage)
/* VERIFIED_DECOMPILER(0x94fbd, a4fbd_Scr_ParamError.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_ParamError, SCRIPT_IMPORT_PARAM_ERROR,
              void_from_int_cstring,
              int32_t, index, const char *, message)
/* VERIFIED_DECOMPILER(0x94fea, a4fea_Scr_ObjectError.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_ObjectError, SCRIPT_IMPORT_OBJECT_ERROR, void_from_cstring,
              const char *, message)
/* VERIFIED_DECOMPILER(0x95010, a5010_Scr_SetDynamicEntityField.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_3(Scr_SetDynamicEntityField,
              SCRIPT_IMPORT_SET_DYNAMIC_ENTITY_FIELD, void_from_int_int_u16,
              int, entityNum, int, classnum, uint16_t, fieldName)
/* VERIFIED_DECOMPILER(0x9504c, a504c_Scr_FreeEntityNum.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_FreeEntityNum, SCRIPT_IMPORT_FREE_ENTITY_NUM,
              void_from_int_int, int, entityNum, int, classnum)
/* VERIFIED_DECOMPILER(0x95079, a5079_Scr_GetEntityId.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_2(uint16_t, Scr_GetEntityId, SCRIPT_IMPORT_GET_ENTITY_ID,
             u16_from_int_int, int, entityNum, int, classnum)
/* VERIFIED_DECOMPILER(0x950a9, a50a9_Scr_SetClassMap.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_SetClassMap, SCRIPT_IMPORT_SET_CLASS_MAP,
              void_from_classmap_u32, script_class_map_entry_t *, classMap,
              uint32_t, classCount)
/* VERIFIED_DECOMPILER(0x950d6, a50d6_Scr_RemoveClassMap.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_RemoveClassMap, SCRIPT_IMPORT_REMOVE_CLASS_MAP,
              void_from_void)
/* VERIFIED_DECOMPILER(0x950f6, a50f6_Scr_AddClassField.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_3(Scr_AddClassField, SCRIPT_IMPORT_ADD_CLASS_FIELD,
              void_from_u16_cstring_u16, uint16_t, classnum, const char *, name,
              uint16_t, fieldIndex)
/* VERIFIED_DECOMPILER(0x9513a, a513a_Scr_AddFields.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_AddFields, SCRIPT_IMPORT_ADD_FIELDS,
              void_from_cstring_cstring, const char *, path, const char *,
              extension)
/* VERIFIED_DECOMPILER(0x95167, a5167_Scr_FindField.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_2(uint16_t, Scr_FindField, SCRIPT_IMPORT_FIND_FIELD,
             u16_from_cstring_intp, const char *, name, int *, type)
/* VERIFIED_DECOMPILER(0x95197, a5197_Scr_GetOffset.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_2(uint32_t, Scr_GetOffset, SCRIPT_IMPORT_GET_OFFSET,
             u32_from_u16_cstring, uint16_t, classnum, const char *, name)
/* VERIFIED_DECOMPILER(0x951cc, a51cc_Scr_CopyEntityNum.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_3(Scr_CopyEntityNum, SCRIPT_IMPORT_COPY_ENTITY_NUM,
              void_from_int_int_int, int, sourceEntityNum, int, destEntityNum,
              int, classnum)
/* VERIFIED_DECOMPILER(0x95200, a5200_Scr_Init.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_3(Scr_Init, SCRIPT_IMPORT_INIT, void_from_int_int_int, int32_t,
              debugReport, int32_t, developerScript, int32_t, developer)
/* VERIFIED_DECOMPILER(0x95234, a5234_Scr_Shutdown.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_Shutdown, SCRIPT_IMPORT_SHUTDOWN, void_from_void)
/* VERIFIED_DECOMPILER(0x95254, a5254_Scr_Abort.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_Abort, SCRIPT_IMPORT_ABORT, void_from_void)
/* VERIFIED_DECOMPILER(0x95274, a5274_Scr_SetLoading.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_SetLoading, SCRIPT_IMPORT_SET_LOADING, void_from_int, int,
              loading)
/* VERIFIED_DECOMPILER(0x952ba, a52ba_Scr_InitSystem.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_InitSystem, SCRIPT_IMPORT_INIT_SYSTEM, void_from_u32_u32,
              uint32_t, system, uint32_t, time)
/* VERIFIED_DECOMPILER(0x9529a, a529a_Scr_AllocGameVariable.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_AllocGameVariable, SCRIPT_IMPORT_ALLOC_GAME_VARIABLE,
              void_from_void)
/* VERIFIED_DECOMPILER(0x952e7, a52e7_Scr_GetChecksum.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
void Scr_GetChecksum(uint32_t checksum[3])
{
    SCRIPT_VM_CALL(SCRIPT_IMPORT_GET_CHECKSUM, void_from_u32p)(checksum);
}
/* VERIFIED_DECOMPILER(0x9530d, a530d_Scr_HasSourceFiles.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_0(qboolean, Scr_HasSourceFiles, SCRIPT_IMPORT_HAS_SOURCE_FILES,
             bool_from_void)
/* VERIFIED_DECOMPILER(0x9532d, a532d_Scr_SaveSource.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_SaveSource, SCRIPT_IMPORT_SAVE_SOURCE, void_from_source_io,
              script_source_io_fn_t, writeData)
/* VERIFIED_DECOMPILER(0x95353, a5353_Scr_LoadSource.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_LoadSource, SCRIPT_IMPORT_LOAD_SOURCE, void_from_source_io,
              script_source_io_fn_t, readData)
/* VERIFIED_DECOMPILER(0x95379, a5379_Scr_SkipSource.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_SkipSource, SCRIPT_IMPORT_SKIP_SOURCE, void_from_source_io,
              script_source_io_fn_t, readData)
/* VERIFIED_DECOMPILER(0x9539f, a539f_Scr_SavePre.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_SavePre, SCRIPT_IMPORT_SAVE_PRE, void_from_voidp, void *,
              file)
/* VERIFIED_DECOMPILER(0x953c5, a53c5_Scr_SavePost.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_SavePost, SCRIPT_IMPORT_SAVE_POST, void_from_voidp, void *,
              file)
/* VERIFIED_DECOMPILER(0x953eb, a53eb_Scr_SaveShutdown.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_SaveShutdown, SCRIPT_IMPORT_SAVE_SHUTDOWN, void_from_void)
/* VERIFIED_DECOMPILER(0x9540b, a540b_Scr_LoadPre.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_LoadPre, SCRIPT_IMPORT_LOAD_PRE, void_from_voidp_int, void *,
              file, int, scriptRunning)
/* VERIFIED_DECOMPILER(0x95438, a5438_Scr_LoadShutdown.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_LoadShutdown, SCRIPT_IMPORT_LOAD_SHUTDOWN, void_from_void)
/* VERIFIED_DECOMPILER(0x95458, a5458_Scr_LoadScript.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(qboolean, Scr_LoadScript, SCRIPT_IMPORT_LOAD_SCRIPT,
             bool_from_cstring, const char *, scriptName)
#if defined(WINDOWS_BEHAVIOR)
/* uo_game_mp_x86.dll 0x200592b0 pushes the tree-name argument from EAX,
 * calls callback slot 80 at 0x2010ecd8, and returns its pointer in EAX. */
DEFINE_RET_1(XAnim *, Scr_FindAnimTree,
             SCRIPT_IMPORT_FIND_ANIM_TREE, xanim_from_cstring,
             const char *, treeName)
#else
/* VERIFIED_DECOMPILER(0x9547e, a547e_Scr_FindAnimTree.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot using the original i386 struct-return ABI; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(script_anim_tree_ref_t, Scr_FindAnimTree,
             SCRIPT_IMPORT_FIND_ANIM_TREE, anim_tree_ref_from_cstring,
             const char *, treeName)
#endif
/* VERIFIED_DECOMPILER(0x954b4, a54b4_Scr_FindAnim.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_3(Scr_FindAnim, SCRIPT_IMPORT_FIND_ANIM,
              void_from_cstring_cstring_animref, const char *, treeName,
              const char *, animName, scr_anim_t *, outAnim)
/* VERIFIED_DECOMPILER(0x954e8, a54e8_Scr_GetFunctionHandle.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_2(uint32_t, Scr_GetFunctionHandle, SCRIPT_IMPORT_GET_FUNCTION_HANDLE,
             u32_from_cstring_cstring, const char *, scriptName, const char *,
             labelName)
/* VERIFIED_DECOMPILER(0x9576c, a576c_Scr_FreeThread.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_FreeThread, SCRIPT_IMPORT_FREE_THREAD, void_from_u16,
              uint16_t, threadId)
/* VERIFIED_DECOMPILER(0x9579a, a579a_Scr_ConvertThreadToSave.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(uint16_t, Scr_ConvertThreadToSave,
             SCRIPT_IMPORT_CONVERT_THREAD_TO_SAVE, u16_from_u16, uint16_t,
             threadId)
/* VERIFIED_DECOMPILER(0x957cb, a57cb_Scr_ConvertThreadFromLoad.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(uint16_t, Scr_ConvertThreadFromLoad,
             SCRIPT_IMPORT_CONVERT_THREAD_FROM_LOAD, u16_from_u16, uint16_t,
             threadId)
/* VERIFIED_DECOMPILER(0x957fc, a57fc_Scr_SetString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(Scr_SetString, SCRIPT_IMPORT_SET_STRING, void_from_voidp_u16,
              uint16_t *, slot, uint16_t, value)
/* VERIFIED_DECOMPILER(0x95831, a5831_Scr_AllocString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_2(uint16_t, Scr_AllocString, SCRIPT_IMPORT_ALLOC_STRING,
             u16_from_cstring_int, const char *, value, int, user)
/* VERIFIED_DECOMPILER(0x958c7, a58c7_Scr_NotifyNum.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_4(Scr_NotifyNum, SCRIPT_IMPORT_NOTIFY_NUM,
              void_from_int_int_u16_u32, int32_t, entityNum, int32_t, classnum,
              uint16_t, event, uint32_t, paramCount)
/* VERIFIED_DECOMPILER(0x959cf, a59cf_Scr_NotifyId.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_3(Scr_NotifyId, SCRIPT_IMPORT_NOTIFY_ID, void_from_u16_u16_u32,
              uint16_t, objectId, uint16_t, event, uint32_t, paramCount)
/* VERIFIED_DECOMPILER(0x9590a, a590a_SL_ConvertToString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(const char *, SL_ConvertToString, SCRIPT_IMPORT_SL_CONVERT_TO_STRING,
             cstring_from_u16, uint16_t, stringId)
/* VERIFIED_DECOMPILER(0x95938, a5938_SL_GetString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; original machine code narrows user through a byte local, zero-extends it, and forwards through callback slot 91. */
DEFINE_RET_2(uint16_t, SL_GetString, SCRIPT_IMPORT_SL_GET_STRING,
             u16_from_cstring_u8, const char *, value, uint8_t, user)
/* VERIFIED_DECOMPILER(0x9596f, a596f_SL_GetLowercaseString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; original machine code narrows user through a byte local, zero-extends it, and forwards through callback slot 92. */
DEFINE_RET_2(uint16_t, SL_GetLowercaseString,
             SCRIPT_IMPORT_SL_GET_LOWERCASE_STRING, u16_from_cstring_u8,
             const char *, value, uint8_t, user)
/* VERIFIED_DECOMPILER(0x959a6, a59a6_SL_FindLowercaseString.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(uint16_t, SL_FindLowercaseString,
             SCRIPT_IMPORT_SL_FIND_LOWERCASE_STRING, u16_from_cstring,
             const char *, value)
/* VERIFIED_DECOMPILER(0x95a13, a5a13_Scr_CreateCanonicalFilename.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(uint16_t, Scr_CreateCanonicalFilename,
             SCRIPT_IMPORT_CREATE_CANONICAL_FILENAME, u16_from_cstring,
             const char *, filename)
/* VERIFIED_DECOMPILER(0x95861, a5861_Scr_SetTime.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_1(Scr_SetTime, SCRIPT_IMPORT_SET_TIME, void_from_u32, uint32_t,
              time)
/* VERIFIED_DECOMPILER(0x95887, a5887_Scr_RunCurrentThreads.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_RunCurrentThreads, SCRIPT_IMPORT_RUN_CURRENT_THREADS,
              void_from_void)
/* VERIFIED_DECOMPILER(0x958a7, a58a7_Scr_ResetTimeout.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_0(Scr_ResetTimeout, SCRIPT_IMPORT_RESET_TIMEOUT, void_from_void)
/* VERIFIED_DECOMPILER(0x95a3c, a5a3c_Scr_GetAnimsIndex.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(uint32_t, Scr_GetAnimsIndex, SCRIPT_IMPORT_GET_ANIMS_INDEX,
             u32_from_xanim, XAnim *, anims)
/* VERIFIED_DECOMPILER(0x95a62, a5a62_Scr_GetAnims.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_1(XAnim *, Scr_GetAnims, SCRIPT_IMPORT_GET_ANIMS, xanim_from_u32,
             uint32_t, animsIndex)
/* VERIFIED_DECOMPILER(0x95a88, a5a88_MT_Alloc.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_RET_2(void *, MT_Alloc, SCRIPT_IMPORT_MT_ALLOC, voidp_from_size_int,
             size_t, size, int, tag)
/* VERIFIED_DECOMPILER(0x95ab5, a5ab5_MT_Free.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; forwards through script VM callback slot; slot address and argument arity mechanically checked against decompiler. */
DEFINE_VOID_2(MT_Free, SCRIPT_IMPORT_MT_FREE, void_from_voidp_size, void *,
              ptr, size_t, size)

/* VERIFIED_DECOMPILER(0x95ae2, a5ae2_Scr_FarHook.c, VERIFY-SCRIPT-VM-BRIDGE-WRAPPERS-2026-06-17): DATAFLOW_VERIFIED; copies 0x66 import callback words, publishes five game callback exports, and returns export table base. */
script_callback_fn_t *
Scr_FarHook(script_callback_fn_t *engineCallbacks)
{
    if (engineCallbacks != NULL) {
        for (size_t index = 0; index < SCRIPT_IMPORT_CALLBACK_COUNT;
             index++) {
            scriptVmCallbacks[index].generic = engineCallbacks[index];
        }
    }

    scriptVmCallbacks[SCRIPT_VM_EXPORT_GET_FUNCTION].function_lookup =
        Scr_GetFunction;
    scriptVmCallbacks[SCRIPT_VM_EXPORT_GET_METHOD].method_lookup =
        Scr_GetMethod;
    scriptVmCallbacks[SCRIPT_VM_EXPORT_SET_OBJECT_FIELD].object_field =
        Scr_SetObjectField;
    scriptVmCallbacks[SCRIPT_VM_EXPORT_GET_OBJECT_FIELD].object_field =
        Scr_GetObjectField;
    scriptVmCallbacks[SCRIPT_VM_EXPORT_LOAD_READ].load_read = Scr_LoadRead;

    return &scriptVmCallbacks[SCRIPT_VM_EXPORT_GET_FUNCTION].generic;
}
