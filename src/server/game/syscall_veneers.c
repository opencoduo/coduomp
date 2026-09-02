/*
 * Maintained ABI bridge for engine syscall trap veneers.
 *
 * This file is not a direct decompiler-body reconstruction. It recreates the
 * host syscall boundary from recovered syscall IDs and call-shape evidence so
 * the game module can link as a native shared object.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <string.h>
#include "g_syscalls.h"
#include "game_globals.h"
#include "scr_vm.h"

#if defined(__i386__) && !defined(_WIN32)
#define TRAP_SAVE_EBX() __asm__ volatile("pushl %%ebx" ::: "memory")
#define TRAP_RESTORE_EBX() __asm__ volatile("popl %%ebx" ::: "memory")
#else
#define TRAP_SAVE_EBX() ((void)0)
#define TRAP_RESTORE_EBX() ((void)0)
#endif

/*
 * The Linux engine syscall callback does not provide a useful C prototype to
 * this module and has been observed clobbering i386 PIC's GOT base register.
 * Preserve ebx around the callback so recovered Linux PIC code can safely
 * touch globals, string literals, and PLT entries after any trap. Win32 uses
 * its normal callee-saved ebx convention; pushing here would overlap GCC's
 * outgoing syscall-argument area.
 */
#define TrapCall(...) \
    __extension__({ \
        game_syscall_t gameSyscall = g_syscall; \
        intptr_t result; \
        TRAP_SAVE_EBX(); \
        result = gameSyscall(__VA_ARGS__); \
        TRAP_RESTORE_EBX(); \
        result; \
    })

/* NOT_FROM_ORIGINAL_SOURCE: maintained syscall ABI veneer. */
static uint32_t game_compat_trap_pass_float(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained syscall ABI veneer. */
static float game_compat_trap_return_float(intptr_t value)
{
    uint32_t bits = (uint32_t)value;
    float result;

    memcpy(&result, &bits, sizeof(result));
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained syscall ABI veneer. */
static uint16_t game_compat_xanim_tree_index(uint32_t anim)
{
    return (uint16_t)(anim >> SCR_ANIM_TREE_INDEX_SHIFT);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained syscall ABI veneer. */
static uint16_t game_compat_xanim_index(uint32_t anim)
{
    return (uint16_t)anim;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained syscall ABI veneer. */
static int game_compat_dobj_entity_number(const gentity_t *ent)
{
    return ent->s.number;
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x17204, 27204_trap_Printf.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 838df_trap_Printf.c passes syscall 0 and message. */
void trap_Printf(const char *message)
{
    TrapCall(SYSCALL_PRINTF, message);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x16924, 26924_trap_Error.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8390d_trap_Error.c passes syscall 1 and message. */
void trap_Error(const char *message)
{
    TrapCall(SYSCALL_ERROR, message);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18c14, 28c14_trap_Error_Localized.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8393b_trap_Error_Localized.c passes syscall 2 and message. */
void trap_Error_Localized(const char *message)
{
    TrapCall(SYSCALL_ERROR_LOCALIZED, message);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x194c4, 294c4_trap_Trace.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 840d8_trap_Trace.c passes syscall 37 and seven arguments in stack order. */
void trap_Trace(trace_t *trace, const float *start, const float *mins, const float *maxs, const float *end, int passEntityNum,
                int contentMask)
{
    TrapCall(SYSCALL_TRACE, trace, start, mins, maxs, end, passEntityNum, contentMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x19464, 29464_trap_TraceCapsule.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84130_trap_TraceCapsule.c passes syscall 38 and seven arguments in stack order. */
void trap_TraceCapsule(trace_t *trace, const float *start, const float *mins, const float *maxs, const float *end, int passEntityNum,
                       int contentMask)
{
    TrapCall(SYSCALL_TRACE_CAPSULE, trace, start, mins, maxs, end, passEntityNum, contentMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x16fe4, 26fe4_trap_SightTrace.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84188_trap_SightTrace.c passes syscall 39 and eight arguments in stack order. */
void trap_SightTrace(int32_t *traceResult, const float *start, const float *mins, const float *maxs, const float *end, int passEntityNum,
                     int passOwnerNum, int contentMask)
{
    TrapCall(SYSCALL_SIGHT_TRACE, traceResult, start, mins, maxs, end, passEntityNum, passOwnerNum, contentMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x741e7, 841e7_trap_SightTraceCapsule.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 40 and eight arguments checked against decompiler and objdump. */
void trap_SightTraceCapsule(int32_t *traceResult, const float *start, const float *mins, const float *maxs, const float *end,
                            int passEntityNum, int passOwnerNum, int contentMask)
{
    TrapCall(SYSCALL_SIGHT_TRACE_CAPSULE, traceResult, start, mins, maxs, end, passEntityNum, passOwnerNum, contentMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74297, 84297_trap_CM_BoxTrace.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 42 and trace/start/end/mins/maxs/model/brushMask order checked. */
void trap_CM_BoxTrace(trace_t *trace, const float *start, const float *end, const float *mins, const float *maxs, int model, int brushMask)
{
    TrapCall(SYSCALL_CM_BOX_TRACE, trace, start, end, mins, maxs, model, brushMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x742ef, 842ef_trap_CM_CapsuleTrace.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 43 and trace/start/end/mins/maxs/model/brushMask order checked. */
void trap_CM_CapsuleTrace(trace_t *trace, const float *start, const float *end, const float *mins, const float *maxs, int model,
                          int brushMask)
{
    TrapCall(SYSCALL_CM_CAPSULE_TRACE, trace, start, end, mins, maxs, model, brushMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74347, 84347_trap_CM_BoxSightTrace.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 44, six argument slots, and eax return preservation checked. */
int trap_CM_BoxSightTrace(const float *start, const float *end, const float *mins, const float *maxs, int model, int brushMask)
{
    return (int)TrapCall(SYSCALL_CM_BOX_SIGHT_TRACE, start, end, mins, maxs, model, brushMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74398, 84398_trap_CM_CapsuleSightTrace.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 45, six argument slots, and eax return preservation checked. */
int trap_CM_CapsuleSightTrace(const float *start, const float *end, const float *mins, const float *maxs, int model, int brushMask)
{
    return (int)TrapCall(SYSCALL_CM_CAPSULE_SIGHT_TRACE, start, end, mins, maxs, model, brushMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x16d44, 26d44_trap_LocationalTrace.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 843e9_trap_LocationalTrace.c passes syscall 46 and six arguments in stack order. */
void trap_LocationalTrace(trace_t *trace, const float *start, const float *end, int passEntityNum, int contentMask, const void *priorityMap)
{
    TrapCall(SYSCALL_LOCATIONAL_TRACE, trace, start, end, passEntityNum, contentMask, priorityMap);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x177a4, 277a4_trap_PointContents.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8443a_trap_PointContents.c passes syscall 47, three arguments, and preserves eax return. */
int trap_PointContents(const float *point, int passEntityNum, int contentMask)
{
    return (int)TrapCall(SYSCALL_POINT_CONTENTS, point, passEntityNum, contentMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18534, 28534_trap_InPVS.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84476_trap_InPVS.c passes syscall 48, two arguments, and preserves eax return. */
qboolean trap_InPVS(const float *p1, const float *p2)
{
    return (qboolean)TrapCall(SYSCALL_IN_PVS, p1, p2);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x744ab, 844ab_trap_InPVSIgnorePortals.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 49, two argument slots, and eax return preservation checked. */
qboolean trap_InPVSIgnorePortals(const float *p1, const float *p2)
{
    return (qboolean)TrapCall(SYSCALL_IN_PVS_IGNORE_PORTALS, p1, p2);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x188c4, 288c4_trap_InSnapshot.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 844e0_trap_InSnapshot.c passes syscall 50, two arguments, and preserves eax return. */
int trap_InSnapshot(const float *origin, int entityNum)
{
    return (int)TrapCall(SYSCALL_IN_SNAPSHOT, origin, entityNum);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x170d4, 270d4_trap_AdjustAreaPortalState.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84515_trap_AdjustAreaPortalState.c passes syscall 51 and two arguments. */
void trap_AdjustAreaPortalState(gentity_t *ent, qboolean open)
{
    TrapCall(SYSCALL_ADJUST_AREA_PORTAL_STATE, ent, (int)open);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x7454a, 8454a_trap_AreasConnected.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 52, two argument slots, and eax return preservation checked. */
qboolean trap_AreasConnected(int area1, int area2)
{
    return (qboolean)TrapCall(SYSCALL_AREAS_CONNECTED, area1, area2);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x173e4, 273e4_trap_EntitiesInBox.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 845db_trap_EntitiesInBox.c passes syscall 55, five arguments, and preserves eax return. */
int trap_EntitiesInBox(const float *mins, const float *maxs, int *entityList, int maxcount, int contentMask)
{
    return (int)TrapCall(SYSCALL_ENTITIES_IN_BOX, mins, maxs, entityList, maxcount, contentMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x16894, 26894_trap_SightTraceToEntity.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84246_trap_SightTraceToEntity.c passes syscall 41, six arguments, and preserves eax return. */
int trap_SightTraceToEntity(const float *start, const float *mins, const float *maxs, const float *end, int entityNum, int contentMask)
{
    return (int)TrapCall(SYSCALL_SIGHT_TRACE_TO_ENTITY, start, mins, maxs, end, entityNum, contentMask);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x174c4, 274c4_trap_EntityContact.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84625_trap_EntityContact.c passes syscall 56, three arguments, and preserves eax return. */
qboolean trap_EntityContact(const float *mins, const float *maxs, gentity_t *ent)
{
    return (qboolean)TrapCall(SYSCALL_ENTITY_CONTACT, mins, maxs, ent);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18434, 28434_trap_EntityContactCapsule.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84661_trap_EntityContactCapsule.c passes syscall 63, three arguments, and preserves eax return. */
qboolean trap_EntityContactCapsule(const float *mins, const float *maxs, gentity_t *ent)
{
    return (qboolean)TrapCall(SYSCALL_ENTITY_CONTACT_CAPSULE, mins, maxs, ent);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x17844, 27844_trap_LinkEntity.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8457f_trap_LinkEntity.c passes syscall 53 and entity pointer. */
void trap_LinkEntity(gentity_t *ent)
{
    TrapCall(SYSCALL_LINK_ENTITY, ent);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18074, 28074_trap_UnlinkEntity.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 845ad_trap_UnlinkEntity.c passes syscall 54 and entity pointer. */
void trap_UnlinkEntity(gentity_t *ent)
{
    TrapCall(SYSCALL_UNLINK_ENTITY, ent);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x16be4, 26be4_trap_DObjCreate.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84aed_trap_DObjCreate.c passes syscall 81, model array, zero-extended uint16 count/gameId, tree, and entity number. */
void trap_DObjCreate(DObjModel *models, uint16_t modelCount, XAnimTree *tree, int entityNum, uint16_t gameId)
{
    TrapCall(SYSCALL_DOBJ_CREATE, models, (unsigned int)modelCount, tree, entityNum, (unsigned int)gameId);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x174e4, 274e4_trap_DObjExists.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84b7c_trap_DObjExists.c passes syscall 82, entity number from ent->s.number, and preserves eax return. */
qboolean trap_DObjExists(gentity_t *ent)
{
    return (qboolean)TrapCall(SYSCALL_DOBJ_EXISTS, game_compat_dobj_entity_number(ent));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x16874, 26874_trap_SafeDObjFree.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84b47_trap_SafeDObjFree.c passes syscall 83, entityNum, and freeAll in stack order. */
void trap_SafeDObjFree(int entityNum, qboolean freeAll)
{
    TrapCall(SYSCALL_SAFE_DOBJ_FREE, entityNum, (int)freeAll);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x755ee, 855ee_trap_DObjNumBones.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 120, entity number from ent->s.number, and eax return preservation checked against decompiler and objdump. */
int trap_DObjNumBones(gentity_t *ent)
{
    return (int)TrapCall(SYSCALL_DOBJ_NUM_BONES, game_compat_dobj_entity_number(ent));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x19774, 29774_trap_DObjGetBoneIndex.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8561e_trap_DObjGetBoneIndex.c passes syscall 121, entity number, tagName, and preserves eax return. */
int trap_DObjGetBoneIndex(gentity_t *ent, const char *tagName)
{
    return (int)TrapCall(SYSCALL_DOBJ_GET_BONE_INDEX, game_compat_dobj_entity_number(ent), tagName);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18584, 28584_trap_DObjGetMatrixArray.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 85655_trap_DObjGetMatrixArray.c passes syscall 122, entity number, and preserves eax pointer return. */
float *trap_DObjGetMatrixArray(gentity_t *ent)
{
    return (float *)TrapCall(SYSCALL_DOBJ_GET_MATRIX_ARRAY, game_compat_dobj_entity_number(ent));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x19044, 29044_trap_DObjDumpInfo.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8538c_trap_DObjDumpInfo.c passes syscall 109 and entity number. */
void trap_DObjDumpInfo(gentity_t *ent)
{
    TrapCall(SYSCALL_DOBJ_DUMP_INFO, game_compat_dobj_entity_number(ent));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x178d4, 278d4_trap_DObjCreateSkelForBone.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 853bc_trap_DObjCreateSkelForBone.c passes syscall 110, entity number, boneIndex, and preserves eax return. */
qboolean trap_DObjCreateSkelForBone(gentity_t *ent, int boneIndex)
{
    return (qboolean)TrapCall(SYSCALL_DOBJ_CREATE_SKEL_FOR_BONE, game_compat_dobj_entity_number(ent), boneIndex);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x16784, 26784_trap_DObjCreateSkelForBones.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 853f3_trap_DObjCreateSkelForBones.c passes syscall 111, entity number, partBits, and preserves eax return. */
qboolean trap_DObjCreateSkelForBones(gentity_t *ent, uint32_t *partBits)
{
    return (qboolean)TrapCall(SYSCALL_DOBJ_CREATE_SKEL_FOR_BONES, game_compat_dobj_entity_number(ent), partBits);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18204, 28204_trap_DObjUpdateServerTime.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8542a_trap_DObjUpdateServerTime.c passes syscall 112, entity number, PASSFLOAT(serverTime), notify, and preserves eax return. */
int trap_DObjUpdateServerTime(gentity_t *ent, float serverTime, qboolean notify)
{
    return (int)TrapCall(SYSCALL_DOBJ_UPDATE_SERVER_TIME, game_compat_dobj_entity_number(ent), game_compat_trap_pass_float(serverTime),
                         (int)notify);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x196f4, 296f4_trap_DObjDisplayAnim.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 85685_trap_DObjDisplayAnim.c passes syscall 123 and entity number. */
void trap_DObjDisplayAnim(gentity_t *ent)
{
    TrapCall(SYSCALL_DOBJ_DISPLAY_ANIM, game_compat_dobj_entity_number(ent));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x17a24, 27a24_trap_DObjInitServerTime.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 85472_trap_DObjInitServerTime.c passes syscall 113, entity number, and PASSFLOAT(serverTime). */
void trap_DObjInitServerTime(gentity_t *ent, float serverTime)
{
    TrapCall(SYSCALL_DOBJ_INIT_SERVER_TIME, game_compat_dobj_entity_number(ent), game_compat_trap_pass_float(serverTime));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x19794, 29794_trap_DObjGetHierarchyBits.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 854b1_trap_DObjGetHierarchyBits.c passes syscall 114, entity number, boneIndex, and partBits. */
void trap_DObjGetHierarchyBits(gentity_t *ent, int boneIndex, uint32_t *partBits)
{
    TrapCall(SYSCALL_DOBJ_GET_HIERARCHY_BITS, game_compat_dobj_entity_number(ent), boneIndex, partBits);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x19784, 29784_trap_DObjCalcAnim.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 854ef_trap_DObjCalcAnim.c passes syscall 115, entity number, and partBits. */
void trap_DObjCalcAnim(gentity_t *ent, uint32_t *partBits)
{
    TrapCall(SYSCALL_DOBJ_CALC_ANIM, game_compat_dobj_entity_number(ent), partBits);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x17a54, 27a54_trap_DObjCalcSkel.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 85526_trap_DObjCalcSkel.c passes syscall 116, entity number, and partBits. */
void trap_DObjCalcSkel(gentity_t *ent, uint32_t *partBits)
{
    TrapCall(SYSCALL_DOBJ_CALC_SKEL, game_compat_dobj_entity_number(ent), partBits);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x184a4, 284a4_trap_DObjGetRotTransArray.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 857ce_trap_DObjGetRotTransArray.c passes syscall 129, entity number, and preserves eax pointer return. */
DObjAnimMat *trap_DObjGetRotTransArray(gentity_t *ent)
{
    return (DObjAnimMat *)TrapCall(SYSCALL_DOBJ_GET_ROT_TRANS_ARRAY, game_compat_dobj_entity_number(ent));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x19324, 29324_trap_DObjSetRotTransIndex.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 857fe_trap_DObjSetRotTransIndex.c passes syscall 130, entity number, partBits, boneIndex, and preserves eax return. */
qboolean trap_DObjSetRotTransIndex(gentity_t *ent, uint32_t *partBits, int boneIndex)
{
    return (qboolean)TrapCall(SYSCALL_DOBJ_SET_ROT_TRANS_INDEX, game_compat_dobj_entity_number(ent), partBits, boneIndex);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x19294, 29294_trap_DObjSetControlRotTransIndex.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8583c_trap_DObjSetControlRotTransIndex.c passes syscall 131, entity number, partBits, boneIndex, and preserves eax return. */
qboolean trap_DObjSetControlRotTransIndex(gentity_t *ent, uint32_t *partBits, int boneIndex)
{
    return (qboolean)TrapCall(SYSCALL_DOBJ_SET_CONTROL_ROT_TRANS_INDEX, game_compat_dobj_entity_number(ent), partBits, boneIndex);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x16b54, 26b54_trap_DObjGetBounds.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8587a_trap_DObjGetBounds.c passes syscall 132, entity number, mins, and maxs. */
void trap_DObjGetBounds(gentity_t *ent, vec3_t mins, vec3_t maxs)
{
    TrapCall(SYSCALL_DOBJ_GET_BOUNDS, game_compat_dobj_entity_number(ent), mins, maxs);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x758ef, 858ef_trap_DObjGetTree.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 134, entity number from ent->s.number, and eax pointer return preservation checked against decompiler and objdump. */
XAnimTree *trap_DObjGetTree(gentity_t *ent)
{
    return (XAnimTree *)TrapCall(SYSCALL_DOBJ_GET_TREE, game_compat_dobj_entity_number(ent));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74a91, 84a91_trap_XModelExists.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 79, modelName argument, and eax return preservation checked against decompiler and objdump. */
qboolean trap_XModelExists(const char *modelName)
{
    return (qboolean)TrapCall(SYSCALL_XMODEL_EXISTS, modelName);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18714, 28714_trap_XModelGet.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84abf_trap_XModelGet.c passes syscall 80, modelName, and preserves eax pointer return. */
XModel *trap_XModelGet(const char *modelName)
{
    return (XModel *)TrapCall(SYSCALL_XMODEL_GET, modelName);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18624, 28624_trap_XModelNumBones.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 85772_trap_XModelNumBones.c passes syscall 127, model, and preserves eax return. */
int trap_XModelNumBones(XModel *model)
{
    return (int)TrapCall(SYSCALL_XMODEL_NUM_BONES, model);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x194f4, 294f4_trap_XModelGetBoneNames.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 857a0_trap_XModelGetBoneNames.c passes syscall 128, model, and preserves eax pointer return. */
const uint16_t *trap_XModelGetBoneNames(XModel *model)
{
    return (const uint16_t *)TrapCall(SYSCALL_XMODEL_GET_BONE_NAMES, model);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x17ba4, 27ba4_trap_XModelDebugBoxes.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8594d_trap_XModelDebugBoxes.c passes syscall 136 and entity number. */
void trap_XModelDebugBoxes(gentity_t *ent)
{
    TrapCall(SYSCALL_XMODEL_DEBUG_BOXES, game_compat_dobj_entity_number(ent));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x18d64, 28d64_trap_XAnimGetAnimTreeSize.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8591f_trap_XAnimGetAnimTreeSize.c passes syscall 135, anims, and preserves eax return. */
int trap_XAnimGetAnimTreeSize(XAnim *anims)
{
    return (int)TrapCall(SYSCALL_XANIM_GET_ANIM_TREE_SIZE, anims);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74f35, 84f35_trap_XAnimHasTime.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 95, packed anim high/low uint16 arguments, and eax/qboolean return preservation checked against decompiler and objdump. */
qboolean trap_XAnimHasTime(uint32_t anim)
{
    return (qboolean)TrapCall(SYSCALL_XANIM_HAS_TIME, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29554, 29554_trap_XAnimIsPrimitive.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84f6c_trap_XAnimIsPrimitive.c passes syscall 96, packed anim high/low uint16 arguments, and preserves eax return. */
qboolean trap_XAnimIsPrimitive(uint32_t anim)
{
    return (qboolean)TrapCall(SYSCALL_XANIM_IS_PRIMITIVE, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x285f4, 285f4_trap_XAnimGetAnimName.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 858b8_trap_XAnimGetAnimName.c passes syscall 133, packed anim high/low uint16 arguments, and preserves eax pointer return. */
const char *trap_XAnimGetAnimName(uint32_t anim)
{
    return (const char *)TrapCall(SYSCALL_XANIM_GET_ANIM_NAME, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27ae4, 27ae4_trap_XAnimGetLength.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84fa3_trap_XAnimGetLength.c passes syscall 97, anims pointer, zero-extended uint16 animIndex, and preserves eax return. */
int trap_XAnimGetLength(XAnim *anims, uint16_t animIndex)
{
    return (int)TrapCall(SYSCALL_XANIM_GET_LENGTH, anims, animIndex);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29244, 29244_trap_XAnimGetLengthSeconds.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84fe0_trap_XAnimGetLengthSeconds.c passes syscall 98, packed anim high/low uint16 arguments, and converts eax float bits through FUN_000838b7. */
float trap_XAnimGetLengthSeconds(uint32_t anim)
{
    return game_compat_trap_return_float(
        TrapCall(SYSCALL_XANIM_GET_LENGTH_SECONDS, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim)));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26ec4, 26ec4_trap_XAnimGetRelDelta.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 851c0_trap_XAnimGetRelDelta.c passes syscall 103, packed anim high/low uint16 arguments, rotation/move pointers, and PASSFLOAT start/end times. */
void trap_XAnimGetRelDelta(uint32_t anim, float *rotationDelta, float *moveDelta, float startTime, float endTime)
{
    TrapCall(SYSCALL_XANIM_GET_REL_DELTA, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim), rotationDelta, moveDelta,
             game_compat_trap_pass_float(startTime), game_compat_trap_pass_float(endTime));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x75227, 85227_trap_XAnimGetAbsDelta.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 104, packed anim high/low uint16 arguments, rotation/move pointers, and PASSFLOAT time checked against decompiler and objdump. */
void trap_XAnimGetAbsDelta(uint32_t anim, float *rotationDelta, float *moveDelta, float time)
{
    TrapCall(SYSCALL_XANIM_GET_ABS_DELTA, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim), rotationDelta, moveDelta,
             game_compat_trap_pass_float(time));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x285d4, 285d4_trap_XAnimIsLooped.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8527b_trap_XAnimIsLooped.c passes syscall 105, packed anim high/low uint16 arguments, and preserves eax return. */
qboolean trap_XAnimIsLooped(uint32_t anim)
{
    return (qboolean)TrapCall(SYSCALL_XANIM_IS_LOOPED, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27e84, 27e84_trap_XAnimGetWeight.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 85342_trap_XAnimGetWeight.c passes syscall 108, tree pointer, zero-extended uint16 anim index, and converts eax float bits through FUN_000838b7. */
float trap_XAnimGetWeight(XAnimTree *tree, uint32_t anim)
{
    return game_compat_trap_return_float(TrapCall(SYSCALL_XANIM_GET_WEIGHT, tree, game_compat_xanim_index(anim)));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29234, 29234_trap_XAnimGetTime.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 852f8_trap_XAnimGetTime.c passes syscall 107, tree pointer, zero-extended uint16 anim index, and converts eax float bits through FUN_000838b7. */
float trap_XAnimGetTime(XAnimTree *tree, uint32_t anim)
{
    return game_compat_trap_return_float(TrapCall(SYSCALL_XANIM_GET_TIME, tree, game_compat_xanim_index(anim)));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28b44, 28b44_trap_XAnimNotetrackExists.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 852b2_trap_XAnimNotetrackExists.c passes syscall 106, packed anim high/low uint16 arguments, zero-extended uint16 notetrack, and preserves eax return. */
qboolean trap_XAnimNotetrackExists(uint32_t anim, uint16_t notetrack)
{
    return (qboolean)TrapCall(SYSCALL_XANIM_NOTETRACK_EXISTS, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim), notetrack);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x756b5, 856b5_trap_XAnimHasFinished.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 124, tree pointer, zero-extended uint16 anim index, and eax/qboolean return preservation checked against decompiler and objdump. */
qboolean trap_XAnimHasFinished(XAnimTree *tree, uint32_t anim)
{
    return (qboolean)TrapCall(SYSCALL_XANIM_HAS_FINISHED, tree, game_compat_xanim_index(anim));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28674, 28674_trap_XAnimGetNumChildren.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 856eb_trap_XAnimGetNumChildren.c passes syscall 125, packed anim high/low uint16 arguments, and preserves eax return. */
int trap_XAnimGetNumChildren(uint32_t anim)
{
    return (int)TrapCall(SYSCALL_XANIM_GET_NUM_CHILDREN, game_compat_xanim_tree_index(anim), game_compat_xanim_index(anim));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28d14, 28d14_trap_XAnimGetChildAt.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 85722_trap_XAnimGetChildAt.c passes syscall 126, packed anim high/low uint16 arguments, childIndex, writes returned child into low half, and returns out. */
uint32_t *trap_XAnimGetChildAt(uint32_t *out, uint32_t anim, int childIndex)
{
    const uint16_t treeIndex = game_compat_xanim_tree_index(anim);
    const uint16_t child = (uint16_t)TrapCall(SYSCALL_XANIM_GET_CHILD_AT, treeIndex, game_compat_xanim_index(anim), childIndex);

    *out = ((uint32_t)treeIndex << SCR_ANIM_TREE_INDEX_SHIFT) | child;
    return out;
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27654, 27654_trap_XAnimCreateTree.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84a07_trap_XAnimCreateTree.c passes syscall 76, anims argument, and preserves eax return. */
XAnimTree *trap_XAnimCreateTree(XAnim *anims)
{
    return (XAnimTree *)TrapCall(SYSCALL_XANIM_CREATE_TREE, anims);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74a35, 84a35_trap_XAnimCreateSmallTree.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 77, anims argument, and eax return preservation checked against decompiler and objdump. */
XAnimTree *trap_XAnimCreateSmallTree(XAnim *anims)
{
    return (XAnimTree *)TrapCall(SYSCALL_XANIM_CREATE_SMALL_TREE, anims);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74a63, 84a63_trap_XAnimFreeSmallTree.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 78 and tree argument checked against decompiler and objdump. */
void trap_XAnimFreeSmallTree(XAnimTree *tree)
{
    TrapCall(SYSCALL_XANIM_FREE_SMALL_TREE, tree);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x755b9, 855b9_trap_XAnimCloneAnimTree.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 119 and fromTree/toTree argument order checked against decompiler and objdump. */
void trap_XAnimCloneAnimTree(XAnimTree *fromTree, XAnimTree *toTree)
{
    TrapCall(SYSCALL_XANIM_CLONE_ANIM_TREE, fromTree, toTree);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26a44, 26a44_trap_XAnimGetAnims.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84bac_trap_XAnimGetAnims.c passes syscall 84, tree argument, and preserves eax pointer return. */
/* Native64 support: this is a runtime-tree pointer in the recovered engine; i386 carried the same value in one 32-bit slot. */
XAnim *trap_XAnimGetAnims(XAnimTree *tree)
{
    return (XAnim *)TrapCall(SYSCALL_XANIM_GET_ANIMS, tree);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74bda, 84bda_trap_XAnimGetRoot.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - trap_XAnimGetAnims/Scr_GetAnimsIndex call order, high-half store, low-half zero, and out return checked against decompiler and objdump. */
uint32_t *trap_XAnimGetRoot(uint32_t *out, XAnimTree *tree)
{
    *out = (uint32_t)Scr_GetAnimsIndex(trap_XAnimGetAnims(tree)) << SCR_ANIM_TREE_INDEX_SHIFT;
    return out;
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74c1d, 84c1d_trap_XAnimClearTreeGoalWeights.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 86, tree, zero-extended uint16 anim index, and PASSFLOAT blendTime checked against decompiler and objdump. */
void trap_XAnimClearTreeGoalWeights(XAnimTree *tree, uint32_t anim, float blendTime)
{
    TrapCall(SYSCALL_XANIM_CLEAR_TREE_GOAL_WEIGHTS, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(blendTime));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x286d4, 286d4_trap_XAnimClearGoalWeight.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84c62_trap_XAnimClearGoalWeight.c passes syscall 87, tree, zero-extended uint16 anim index, and PASSFLOAT blendTime. */
void trap_XAnimClearGoalWeight(XAnimTree *tree, uint32_t anim, float blendTime)
{
    TrapCall(SYSCALL_XANIM_CLEAR_GOAL_WEIGHT, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(blendTime));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27294, 27294_trap_XAnimClearTreeGoalWeightsStrict.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84ca7_trap_XAnimClearTreeGoalWeightsStrict.c passes syscall 88, tree, zero-extended uint16 anim index, and PASSFLOAT blendTime. */
void trap_XAnimClearTreeGoalWeightsStrict(XAnimTree *tree, uint32_t anim, float blendTime)
{
    TrapCall(SYSCALL_XANIM_CLEAR_TREE_GOAL_WEIGHTS_STRICT, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(blendTime));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74cec, 84cec_trap_XAnimSetCompleteGoalWeightKnob.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 89, tree, zero-extended uint16 anim/notify, PASSFLOAT weight/blendTime/rate, and restart order checked against decompiler and objdump. */
void trap_XAnimSetCompleteGoalWeightKnob(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                                         qboolean restart)
{
    TrapCall(SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(weight),
             game_compat_trap_pass_float(blendTime), game_compat_trap_pass_float(rate), notifyName, (int)restart);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26ca4, 26ca4_trap_XAnimSetCompleteGoalWeightKnobAll.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84d6f_trap_XAnimSetCompleteGoalWeightKnobAll.c passes syscall 90, tree, zero-extended uint16 anim/knob/notify, PASSFLOAT weight/blendTime/rate, and restart order. */
void trap_XAnimSetCompleteGoalWeightKnobAll(XAnimTree *tree, uint32_t anim, uint32_t knob, float weight, float blendTime, float rate,
                                            uint16_t notifyName, qboolean restart)
{
    TrapCall(SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL, tree, game_compat_xanim_index(anim), (unsigned int)(uint16_t)knob,
             game_compat_trap_pass_float(weight), game_compat_trap_pass_float(blendTime), game_compat_trap_pass_float(rate), notifyName,
             (int)restart);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28224, 28224_trap_XAnimSetAnimRate.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84dfa_trap_XAnimSetAnimRate.c passes syscall 91, tree, zero-extended uint16 anim index, and PASSFLOAT rate. */
void trap_XAnimSetAnimRate(XAnimTree *tree, uint32_t anim, float rate)
{
    TrapCall(SYSCALL_XANIM_SET_ANIM_RATE, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(rate));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x295b4, 295b4_trap_XAnimSetTime.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84e3f_trap_XAnimSetTime.c passes syscall 92, tree, zero-extended uint16 anim index, and PASSFLOAT time. */
void trap_XAnimSetTime(XAnimTree *tree, uint32_t anim, float time)
{
    TrapCall(SYSCALL_XANIM_SET_TIME, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(time));
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74e84, 84e84_trap_XAnimSetGoalWeightKnob.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 93, tree, zero-extended uint16 anim/notify, PASSFLOAT weight/blendTime/rate, and restart order checked against decompiler and objdump. */
void trap_XAnimSetGoalWeightKnob(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                                 qboolean restart)
{
    TrapCall(SYSCALL_XANIM_SET_GOAL_WEIGHT_KNOB, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(weight),
             game_compat_trap_pass_float(blendTime), game_compat_trap_pass_float(rate), notifyName, (int)restart);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74f07, 84f07_trap_XAnimClearTree.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - syscall 94 and tree argument checked against decompiler and objdump. */
void trap_XAnimClearTree(XAnimTree *tree)
{
    TrapCall(SYSCALL_XANIM_CLEAR_TREE, tree);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x268c4, 268c4_trap_XAnimSetCompleteGoalWeight.c, VERIFY-SYSCALL-VENEERS-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8502b_trap_XAnimSetCompleteGoalWeight.c passes syscall 99, tree, zero-extended uint16 anim/notify, PASSFLOAT weight/blendTime/rate, and restart order. */
void trap_XAnimSetCompleteGoalWeight(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                                     qboolean restart)
{
    TrapCall(SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(weight),
             game_compat_trap_pass_float(blendTime), game_compat_trap_pass_float(rate), notifyName, (int)restart);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x295d4, 295d4_trap_XAnimSetGoalWeight.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 850ae_trap_XAnimSetGoalWeight.c passes syscall 100, tree, zero-extended uint16 anim/notify, PASSFLOAT weight/blendTime/rate, and restart order. */
void trap_XAnimSetGoalWeight(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                             qboolean restart)
{
    TrapCall(SYSCALL_XANIM_SET_GOAL_WEIGHT, tree, game_compat_xanim_index(anim), game_compat_trap_pass_float(weight),
             game_compat_trap_pass_float(blendTime), game_compat_trap_pass_float(rate), notifyName, (int)restart);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28e74, 28e74_trap_XAnimCalcAbsDelta.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 85131_trap_XAnimCalcAbsDelta.c passes syscall 101, tree, zero-extended uint16 anim, rotationDelta, and moveDelta. */
void trap_XAnimCalcAbsDelta(XAnimTree *tree, uint32_t anim, float *rotationDelta, float *moveDelta)
{
    TrapCall(SYSCALL_XANIM_CALC_ABS_DELTA, tree, game_compat_xanim_index(anim), rotationDelta, moveDelta);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x75175, 85175_trap_XAnimCalcDelta.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - syscall 102, tree, zero-extended uint16 anim, rotationDelta, moveDelta, and fifth argument order checked against decompiler and objdump. */
/* Native64 support: the fifth syscall slot is consumed by the recovered engine as an int32 selector, not a host pointer. */
void trap_XAnimCalcDelta(XAnimTree *tree, uint32_t anim, float *rotationDelta, float *moveDelta, int weightSelector)
{
    TrapCall(SYSCALL_XANIM_CALC_DELTA, tree, game_compat_xanim_index(anim), rotationDelta, moveDelta, weightSelector);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x7555d, 8555d_trap_XAnimLoadAnimTree.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - syscall 117 and tree argument checked against decompiler and objdump. */
/* Native64 support: this is a runtime-tree pointer, not the packed 16:16 XAnim ref used by anim arguments. */
void trap_XAnimLoadAnimTree(XAnimTree *tree)
{
    TrapCall(SYSCALL_XANIM_LOAD_ANIM_TREE, tree);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x7558b, 8558b_trap_XAnimSaveAnimTree.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - syscall 118 and tree argument checked against decompiler and objdump. */
/* Native64 support: this is a runtime-tree pointer, not the packed 16:16 XAnim ref used by anim arguments. */
void trap_XAnimSaveAnimTree(XAnimTree *tree)
{
    TrapCall(SYSCALL_XANIM_SAVE_ANIM_TREE, tree);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28a84, 28a84_trap_Cvar_Register.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83cd2_trap_Cvar_Register.c passes syscall 4, cvar, name, value, and flags. */
void trap_Cvar_Register(vmCvar_t *cvar, const char *name, const char *value, int flags)
{
    TrapCall(SYSCALL_CVAR_REGISTER, cvar, name, value, flags);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26e44, 26e44_trap_Cvar_Set.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83d43_trap_Cvar_Set.c passes syscall 6, name, and value. */
void trap_Cvar_Set(const char *name, const char *value)
{
    TrapCall(SYSCALL_CVAR_SET, name, value);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x276e4, 276e4_trap_Cvar_Update.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83d15_trap_Cvar_Update.c passes syscall 5 and cvar. */
void trap_Cvar_Update(vmCvar_t *cvar)
{
    TrapCall(SYSCALL_CVAR_UPDATE, cvar);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26df4, 26df4_trap_Cvar_VariableStringBuffer.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83de4_trap_Cvar_VariableStringBuffer.c passes syscall 9, name, buffer, and size. */
void trap_Cvar_VariableStringBuffer(const char *name, char *buffer, int size)
{
    TrapCall(SYSCALL_CVAR_VARIABLE_STRING_BUFFER, name, buffer, size);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29404, 29404_trap_Cvar_VariableIntegerValue.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83d78_trap_Cvar_VariableIntegerValue.c passes syscall 7 and returns the syscall result in eax. */
int trap_Cvar_VariableIntegerValue(const char *name)
{
    return (int)TrapCall(SYSCALL_CVAR_VARIABLE_INTEGER_VALUE, name);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29174, 29174_trap_Cvar_VariableValue.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83da6_trap_Cvar_VariableValue.c passes syscall 8, name, and local float output pointer, then returns the filled float. */
float trap_Cvar_VariableValue(const char *name)
{
    float value = 0.0f;

    TrapCall(SYSCALL_CVAR_VARIABLE_VALUE, name, &value);
    return value;
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26de4, 26de4_trap_GetConfigstring.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83f3e_trap_GetConfigstring.c passes syscall 29, index, buffer, and bufferLength. */
void trap_GetConfigstring(int index, char *buffer, int bufferLength)
{
    TrapCall(SYSCALL_GET_CONFIGSTRING, index, buffer, bufferLength);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28184, 28184_trap_GetConfigstringConst.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83f7a_trap_GetConfigstringConst.c passes syscall 30 and returns the engine pointer in eax. */
const char *trap_GetConfigstringConst(int index)
{
    return (const char *)TrapCall(SYSCALL_GET_CONFIGSTRING_CONST, index);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27874, 27874_trap_SetConfigstring.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83f09_trap_SetConfigstring.c passes syscall 28, index, and value. */
void trap_SetConfigstring(int index, const char *value)
{
    TrapCall(SYSCALL_SET_CONFIGSTRING, index, value);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29054, 29054_trap_GetUserinfo.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84004_trap_GetUserinfo.c passes syscall 33, clientNum, buffer, and bufferSize. */
void trap_GetUserinfo(int clientNum, char *buffer, int bufferSize)
{
    TrapCall(SYSCALL_GET_USERINFO, clientNum, buffer, bufferSize);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74040, 84040_trap_SetUserinfo.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - syscall 34, clientNum, and info argument order checked against decompiler and objdump. */
void trap_SetUserinfo(int clientNum, const char *info)
{
    TrapCall(SYSCALL_SET_USERINFO, clientNum, info);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28994, 28994_trap_GetUsercmd.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8469d_trap_GetUsercmd.c passes syscall 57, clientNum, and the usercmd output pointer. */
void trap_GetUsercmd(int clientNum, usercmd_t *command)
{
    TrapCall(SYSCALL_GET_USERCMD, clientNum, command);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27764, 27764_trap_GetArchivedClientInfo.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 849c4_trap_GetArchivedClientInfo.c passes syscall 70, clientNum, archiveTime, archivedClientInfo, and archive metadata, returning eax. */
int trap_GetArchivedClientInfo(int clientNum, int32_t *archiveTime, playerState_t *playerState, clientState_t *meta)
{
    return (int)TrapCall(SYSCALL_GET_ARCHIVED_CLIENT_INFO, clientNum, archiveTime, playerState, meta);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28154, 28154_trap_IsLocalClient.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83fa8_trap_IsLocalClient.c passes syscall 31 and returns eax. */
int trap_IsLocalClient(int clientNum)
{
    return (int)TrapCall(SYSCALL_IS_LOCAL_CLIENT, clientNum);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26cd4, 26cd4_trap_GetGuid.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83e6a_trap_GetGuid.c passes syscall 25 and returns eax. */
int trap_GetGuid(int clientNum)
{
    return (int)TrapCall(SYSCALL_GET_GUID, clientNum);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x272a4, 272a4_trap_GetClientPing.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83fd6_trap_GetClientPing.c passes syscall 32 and returns eax. */
int trap_GetClientPing(int clientNum)
{
    return (int)TrapCall(SYSCALL_GET_CLIENT_PING, clientNum);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28144, 28144_trap_SendServerCommand.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83ecd_trap_SendServerCommand.c passes syscall 27, clientNum, reliable, and command. */
void trap_SendServerCommand(int clientNum, int reliable, const char *command)
{
    TrapCall(SYSCALL_SEND_SERVER_COMMAND, clientNum, reliable, command);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28654, 28654_trap_SendConsoleCommand.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83c9d_trap_SendConsoleCommand.c passes syscall 23, executionTime, and text. */
void trap_SendConsoleCommand(int executionTime, const char *text)
{
    TrapCall(SYSCALL_SEND_CONSOLE_COMMAND, executionTime, text);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27e44, 27e44_trap_DropClient.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83e98_trap_DropClient.c passes syscall 26, clientNum, and reason. */
void trap_DropClient(int clientNum, const char *reason)
{
    TrapCall(SYSCALL_DROP_CLIENT, clientNum, reason);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x271d4, 271d4_trap_AddTestClient.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84971_trap_AddTestClient.c returns NULL for negative syscall 69 result, otherwise returns level.gentities[result]. */
gentity_t *trap_AddTestClient(void)
{
    const int entityNum = (int)TrapCall(SYSCALL_ADD_TEST_CLIENT);

    if (entityNum < 0) {
        return NULL;
    }

    return &level.gentities[entityNum];
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28a44, 28a44_trap_SetArchive.c, VERIFY-SYSCALL-VENEERS-CVARCONFIG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8479e_trap_SetArchive.c passes syscall 73 and enabled. */
void trap_SetArchive(qboolean enabled)
{
    TrapCall(SYSCALL_SET_ARCHIVE, (int)enabled);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26e54, 26e54_trap_FS_FOpenFile.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83b15_trap_FS_FOpenFile.c passes syscall 18, path, handle, mode, and preserves eax return. */
int trap_FS_FOpenFile(const char *path, int *handle, fsMode_t mode)
{
    return (int)TrapCall(SYSCALL_FS_FOPEN_FILE, path, handle, mode);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27664, 27664_trap_FS_FCloseFile.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83bfe_trap_FS_FCloseFile.c passes syscall 22 and handle. */
void trap_FS_FCloseFile(int handle)
{
    TrapCall(SYSCALL_FS_FCLOSE_FILE, handle);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x297a4, 297a4_trap_FS_Read.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83b51_trap_FS_Read.c passes syscall 19, buffer, length, handle. */
void trap_FS_Read(void *buffer, int length, int handle)
{
    TrapCall(SYSCALL_FS_READ, buffer, length, handle);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x295e4, 295e4_trap_FS_Write.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83b8d_trap_FS_Write.c passes syscall 20, buffer, length, handle. */
void trap_FS_Write(const void *buffer, int length, int handle)
{
    TrapCall(SYSCALL_FS_WRITE, buffer, length, handle);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x73bc9, 83bc9_trap_FS_Rename.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 21 and from/to argument order checked. */
void trap_FS_Rename(const char *from, const char *to)
{
    TrapCall(SYSCALL_FS_RENAME, from, to);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28504, 28504_trap_FS_GetFileList.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83c2c_trap_FS_GetFileList.c passes syscall 59, path, extension, listBuffer, bufferSize, and preserves eax return. */
int trap_FS_GetFileList(const char *path, const char *extension, char *listBuffer, int bufferSize)
{
    return (int)TrapCall(SYSCALL_FS_GET_FILE_LIST, path, extension, listBuffer, bufferSize);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x285b4, 285b4_trap_MapExists.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83c6f_trap_MapExists.c passes syscall 60, mapName, and preserves eax return. */
int trap_MapExists(const char *mapName)
{
    return (int)TrapCall(SYSCALL_MAP_EXISTS, mapName);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x288a4, 288a4_trap_AddDebugString.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84707_trap_AddDebugString.c passes syscall 71, origin, color, PASSFLOAT(scale), text. */
void trap_AddDebugString(const float *origin, const float *color, float scale, const char *text)
{
    TrapCall(SYSCALL_ADD_DEBUG_STRING, origin, color, game_compat_trap_pass_float(scale), text);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28b54, 28b54_trap_AddDebugLine.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84754_trap_AddDebugLine.c passes syscall 72, start, end, color, depthTest, duration. */
void trap_AddDebugLine(const float *start, const float *end, const float *color, int depthTest, int duration)
{
    TrapCall(SYSCALL_ADD_DEBUG_LINE, start, end, color, depthTest, duration);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x748b9, 848b9_trap_SurfaceTypeFromName.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 67, name argument, and eax return preservation checked. */
int trap_SurfaceTypeFromName(const char *name)
{
    return (int)TrapCall(SYSCALL_SURFACE_TYPE_FROM_NAME, name);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29204, 29204_trap_SurfaceTypeToName.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 848e7_trap_SurfaceTypeToName.c passes syscall 68, surfaceType, and preserves eax pointer return. */
const char *trap_SurfaceTypeToName(int surfaceType)
{
    return (const char *)TrapCall(SYSCALL_SURFACE_TYPE_TO_NAME, surfaceType);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74828, 84828_trap_Com_SoundAliasString.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 64, name argument, and eax pointer return preservation checked. */
const char *trap_Com_SoundAliasString(const char *name)
{
    return (const char *)TrapCall(SYSCALL_COM_SOUND_ALIAS_STRING, name);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74856, 84856_trap_Com_PickSoundAlias.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 65 and name/alias argument order checked. */
void trap_Com_PickSoundAlias(const char *name, void *alias)
{
    TrapCall(SYSCALL_COM_PICK_SOUND_ALIAS, name, alias);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x7488b, 8488b_trap_Com_SoundAliasIndex.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 66, sound-alias record pointer, and eax return preservation checked. */
int trap_Com_SoundAliasIndex(const snd_alias_t *alias)
{
    return (int)TrapCall(SYSCALL_COM_SOUND_ALIAS_INDEX, alias);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x280a4, 280a4_trap_Argc.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83990_trap_Argc.c passes syscall 10 and preserves eax return. */
int trap_Argc(void)
{
    return (int)TrapCall(SYSCALL_ARGC);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27b54, 27b54_trap_Argv.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 839b7_trap_Argv.c passes syscall 11, arg, buffer, bufferLength. */
void trap_Argv(int arg, char *buffer, int bufferLength)
{
    TrapCall(SYSCALL_ARGV, arg, buffer, bufferLength);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27184, 27184_trap_GetEntityToken.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 846d2_trap_GetEntityToken.c passes syscall 58, buffer, bufferSize, and preserves eax return. */
qboolean trap_GetEntityToken(char *buffer, int bufferSize)
{
    return (qboolean)TrapCall(SYSCALL_GET_ENTITY_TOKEN, buffer, bufferSize);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x273c4, 273c4_trap_Milliseconds.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83969_trap_Milliseconds.c passes syscall 3 and preserves eax return. */
int trap_Milliseconds(void)
{
    return (int)TrapCall(SYSCALL_MILLISECONDS);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x747cc, 847cc_trap_RealTime.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 61 and qtime pointer argument checked. */
void trap_RealTime(qtime_t *qtime)
{
    TrapCall(SYSCALL_REAL_TIME, qtime);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29854, 29854_trap_SnapVector.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 847fa_trap_SnapVector.c passes syscall 62 and vector pointer. */
void trap_SnapVector(float *vec)
{
    TrapCall(SYSCALL_SNAP_VECTOR, vec);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x26fa4, 26fa4_trap_GetServerinfo.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 84075_trap_GetServerinfo.c passes syscall 35, buffer, bufferSize. */
void trap_GetServerinfo(char *buffer, int bufferSize)
{
    TrapCall(SYSCALL_GET_SERVERINFO, buffer, bufferSize);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27514, 27514_trap_LocateGameData.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83e20_trap_LocateGameData.c passes syscall 24, gEnts, numGEntities, sizeofGEntity, the first client player-state pointer, and sizeofClient. */
void trap_LocateGameData(gentity_t *gEnts, int numGEntities, int sizeofGEntity, playerState_t *clients, int sizeofClient)
{
    TrapCall(SYSCALL_LOCATE_GAME_DATA, gEnts, numGEntities, sizeofGEntity, clients, sizeofClient);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x281f4, 281f4_trap_SetBrushModel.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 840aa_trap_SetBrushModel.c passes syscall 36 and entity pointer. */
void trap_SetBrushModel(gentity_t *ent)
{
    TrapCall(SYSCALL_SET_BRUSH_MODEL, ent);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28a74, 28a74_trap_FreeClientScriptPers.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 859e0_trap_FreeClientScriptPers.c passes syscall 139 with no arguments. */
void trap_FreeClientScriptPers(void)
{
    TrapCall(SYSCALL_FREE_CLIENT_SCRIPT_PERS);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27c34, 27c34_trap_GetWeaponInfoMemory.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 8597d_trap_GetWeaponInfoMemory.c passes syscall 137, bytes, alreadyLoaded, and preserves eax pointer return. */
weaponInfo_t **trap_GetWeaponInfoMemory(int bytes, int *alreadyLoaded)
{
    return (weaponInfo_t **)TrapCall(SYSCALL_GET_WEAPON_INFO_MEMORY, bytes, alreadyLoaded);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x29524, 29524_trap_FreeWeaponInfoMemory.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 859b2_trap_FreeWeaponInfoMemory.c passes syscall 138 and mode. */
void trap_FreeWeaponInfoMemory(int mode)
{
    TrapCall(SYSCALL_FREE_WEAPON_INFO_MEMORY, mode);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x75a07, 85a07_trap_ResetEntityParsePoint.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 140 with no arguments checked. */
void trap_ResetEntityParsePoint(void)
{
    TrapCall(SYSCALL_RESET_ENTITY_PARSE_POINT);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x739f3, 839f3_trap_Hunk_AllocInternal.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 12, size argument, and eax pointer return preservation checked. */
void *trap_Hunk_AllocInternal(size_t size)
{
    return (void *)TrapCall(SYSCALL_HUNK_ALLOC, size);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27434, 27434_trap_Hunk_AllocLowInternal.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83a21_trap_Hunk_AllocLowInternal.c passes syscall 13, size, and preserves eax pointer return. */
void *trap_Hunk_AllocLowInternal(size_t size)
{
    return (void *)TrapCall(SYSCALL_HUNK_ALLOC_LOW, size);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x28374, 28374_trap_Hunk_AllocAlignInternal.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83a4f_trap_Hunk_AllocAlignInternal.c passes syscall 14, size, alignment, and preserves eax pointer return. */
void *trap_Hunk_AllocAlignInternal(size_t size, int alignment)
{
    return (void *)TrapCall(SYSCALL_HUNK_ALLOC_ALIGN, size, alignment);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x27d34, 27d34_trap_Hunk_AllocLowAlignInternal.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - PLT thunk checked; body 83a84_trap_Hunk_AllocLowAlignInternal.c passes syscall 15, size, alignment, and preserves eax pointer return. */
void *trap_Hunk_AllocLowAlignInternal(size_t size, int alignment)
{
    return (void *)TrapCall(SYSCALL_HUNK_ALLOC_LOW_ALIGN, size, alignment);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x73ab9, 83ab9_trap_Hunk_AllocateTempMemoryInternal.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 16, size argument, and eax pointer return preservation checked. */
void *trap_Hunk_AllocateTempMemoryInternal(size_t size)
{
    return (void *)TrapCall(SYSCALL_HUNK_ALLOC_TEMP, size);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x73ae7, 83ae7_trap_Hunk_FreeTempMemoryInternal.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 17 and pointer argument checked. */
void trap_Hunk_FreeTempMemoryInternal(void *ptr)
{
    TrapCall(SYSCALL_HUNK_FREE_TEMP, ptr);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74915, 84915_trap_Z_MallocInternal.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 74, size argument, and eax pointer return preservation checked. */
void *trap_Z_MallocInternal(size_t size)
{
    return (void *)TrapCall(SYSCALL_Z_MALLOC, size);
}

/* Original trap veneer; host callback adaptation is documented above. */
/* VERIFIED_DECOMPILER(0x74943, 84943_trap_Z_FreeInternal.c, VERIFY-SYSCALL-VENEERS-FSDEBUG-2026-06-17): DATAFLOW_VERIFIED - syscall 75 and pointer argument checked. */
void trap_Z_FreeInternal(void *ptr)
{
    TrapCall(SYSCALL_Z_FREE, ptr);
}
