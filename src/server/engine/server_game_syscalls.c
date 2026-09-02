#include "server_game_syscalls.h"

#include "collision/collision_area.h"
#include "compat/coduo_x87emu.h"
#include "qcommon/q_bits.h"
#include "server_game_bridge.h"
#include "server_game_lifecycle.h"
#include "server_game_queries.h"
#include "server_game_syscall_services.h"
#include "server_operator_maps.h"
#include "server_xmodel.h"

#include <stddef.h>
#include <float.h>
#include <math.h>
#include <string.h>

enum {
    SERVER_GAME_SYSCALL_WEAPON_MEMORY_OWNER = 1
};

static const float sv_xanimMilliseconds = 1000.0f;

#define SV_ARG(index) (arguments[(index)])
#define SV_PTR(type, index) ((type *)SV_ARG(index))
#define SV_CONST_PTR(type, index) ((const type *)SV_ARG(index))
#define SV_STRING(index) ((const char *)SV_ARG(index))
#define SV_INT(index) ((int32_t)SV_ARG(index))
#define SV_SIZE(index) ((size_t)SV_ARG(index))

/* NOT_FROM_ORIGINAL_SOURCE: explicit source-level expression of the native
 * VM ABI's four-byte float-bit transport. memcpy preserves the bit pattern
 * without depending on a compiler's inactive-union-member extension. */
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "game syscall float transport requires IEEE binary32");
static float server_compat_game_syscall_float_argument(intptr_t argument)
{
    const uint32_t bits = (uint32_t)argument;
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Complete game-module syscall dispatcher:
 *
 *   CoDUOMP.exe   0x0045d820..0x0045ea5b
 *   coduo_lnxded  0x0808eea4..0x08090b56
 *
 * Both original engines implement the same 152 syscall IDs and common
 * argument/result contracts. Target-owned debug presentation, surface-name
 * lookup, and weapon-memory ownership are bound by the target service header.
 * Recovery remains in syscall-id order even though the optimized Windows body
 * shares many return epilogues. The canonical name is retained from the
 * Windows and supporting Mac symbols. */
intptr_t SV_GameSystemCalls(intptr_t *arguments)
{
    const game_syscall_id_t syscall = (game_syscall_id_t)SV_ARG(0);

    switch (syscall) {
    case SYSCALL_PRINTF:
        Com_Printf("%s", SV_STRING(1));
        return 0;
    case SYSCALL_ERROR:
        Com_Error(ERR_DROP, "\x15%s", SV_STRING(1));
        return 0;
    case SYSCALL_ERROR_LOCALIZED:
        Com_Error(ERR_DROP, "%s", SV_STRING(1));
        return 0;
    case SYSCALL_MILLISECONDS:
        return Sys_Milliseconds();
    case SYSCALL_CVAR_REGISTER:
        Cvar_Register(SV_PTR(vmCvar_t, 1), SV_STRING(2), SV_STRING(3), (uint32_t)SV_ARG(4));
        return 0;
    case SYSCALL_CVAR_UPDATE:
        Cvar_Update(SV_PTR(vmCvar_t, 1));
        return 0;
    case SYSCALL_CVAR_SET:
        Cvar_Set(SV_STRING(1), SV_STRING(2));
        return 0;
    case SYSCALL_CVAR_VARIABLE_INTEGER_VALUE:
        return Cvar_VariableIntegerValue(SV_STRING(1));
    case SYSCALL_CVAR_VARIABLE_VALUE:
        *SV_PTR(float, 2) = Cvar_VariableValue(SV_STRING(1));
        return 0;
    case SYSCALL_CVAR_VARIABLE_STRING_BUFFER:
        Cvar_VariableStringBuffer(SV_STRING(1), SV_PTR(char, 2), SV_INT(3));
        return 0;
    case SYSCALL_ARGC:
        return Cmd_Argc();
    case SYSCALL_ARGV:
        Cmd_ArgvBuffer(SV_INT(1), SV_PTR(char, 2), SV_INT(3));
        return 0;
    case SYSCALL_HUNK_ALLOC:
        return (intptr_t)SV_Hunk_AllocInternal(SV_SIZE(1));
    case SYSCALL_HUNK_ALLOC_LOW:
        return (intptr_t)SV_Hunk_AllocLowInternal(SV_SIZE(1));
    case SYSCALL_HUNK_ALLOC_ALIGN:
        return (intptr_t)SV_Hunk_AllocAlignInternal(SV_SIZE(1), SV_SIZE(2));
    case SYSCALL_HUNK_ALLOC_LOW_ALIGN:
        return (intptr_t)SV_Hunk_AllocLowAlignInternal(SV_SIZE(1), SV_SIZE(2));
    case SYSCALL_HUNK_ALLOC_TEMP:
        return (intptr_t)Hunk_AllocateTempMemoryInternal(SV_SIZE(1));
    case SYSCALL_HUNK_FREE_TEMP:
        SV_Hunk_FreeTempMemoryInternal(SV_PTR(void, 1));
        return 0;
    case SYSCALL_FS_FOPEN_FILE:
        return FS_FOpenFileByMode(SV_STRING(1), SV_PTR(int32_t, 2), (fsMode_t)SV_INT(3));
    case SYSCALL_FS_READ:
        FS_Read(SV_PTR(void, 1), SV_INT(2), SV_INT(3));
        return 0;
    case SYSCALL_FS_WRITE:
        return FS_Write(SV_CONST_PTR(void, 1), SV_INT(2), SV_INT(3));
    case SYSCALL_FS_RENAME:
        FS_Rename(SV_STRING(1), SV_STRING(2));
        return 0;
    case SYSCALL_FS_FCLOSE_FILE:
        FS_FCloseFile(SV_INT(1));
        return 0;
    case SYSCALL_SEND_CONSOLE_COMMAND:
        Cbuf_ExecuteText((cbufExec_t)SV_INT(1), SV_STRING(2));
        return 0;
    case SYSCALL_LOCATE_GAME_DATA:
        SV_LocateGameData(SV_PTR(sharedEntity_t, 1), SV_INT(2), SV_INT(3), SV_PTR(playerState_t, 4), SV_INT(5));
        return 0;
    case SYSCALL_GET_GUID:
        if (SV_INT(1) < 0 || SV_INT(1) >= sv_maxclients->integer) {
            return 0;
        }
        return svs.clients[SV_INT(1)].guid;
    case SYSCALL_DROP_CLIENT:
        SV_GameDropClient(SV_INT(1), SV_STRING(2));
        return 0;
    case SYSCALL_SEND_SERVER_COMMAND:
        SV_GameSendServerCommand(SV_INT(1), (qboolean)SV_ARG(2), SV_STRING(3));
        return 0;
    case SYSCALL_SET_CONFIGSTRING:
        SV_SetConfigstring(SV_INT(1), SV_STRING(2));
        return 0;
    case SYSCALL_GET_CONFIGSTRING:
        SV_GetConfigstring(SV_INT(1), SV_PTR(char, 2), SV_INT(3));
        return 0;
    case SYSCALL_GET_CONFIGSTRING_CONST:
        return (intptr_t)SV_GetConfigstringConst(SV_INT(1));
    case SYSCALL_IS_LOCAL_CLIENT: {
        const int32_t clientNum = SV_INT(1);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
            return qfalse;
        }
        return NET_IsLocalAddress(svs.clients[clientNum].netchan.remoteAddress);
    }
    case SYSCALL_GET_CLIENT_PING: {
        const int32_t clientNum = SV_INT(1);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
            return 0;
        }
        return svs.clients[clientNum].ping;
    }
    case SYSCALL_GET_USERINFO:
        SV_GetUserinfo(SV_INT(1), SV_PTR(char, 2), SV_INT(3));
        return 0;
    case SYSCALL_SET_USERINFO:
        SV_SetUserinfo(SV_INT(1), SV_STRING(2));
        return 0;
    case SYSCALL_GET_SERVERINFO:
        SV_GetServerinfo(SV_PTR(char, 1), SV_INT(2));
        return 0;
    case SYSCALL_SET_BRUSH_MODEL:
        SV_SetBrushModel(SV_PTR(sharedEntity_t, 1));
        return 0;
    case SYSCALL_TRACE:
        SV_Trace(SV_PTR(trace_t, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4), SV_CONST_PTR(float, 5),
                 SV_INT(6), SV_INT(7), qfalse, qfalse, NULL, qfalse);
        return 0;
    case SYSCALL_TRACE_CAPSULE:
        SV_Trace(SV_PTR(trace_t, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4), SV_CONST_PTR(float, 5),
                 SV_INT(6), SV_INT(7), qtrue, qfalse, NULL, qfalse);
        return 0;
    case SYSCALL_SIGHT_TRACE:
        SV_SightTrace(SV_PTR(int32_t, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4), SV_CONST_PTR(float, 5),
                      SV_INT(6), SV_INT(7), SV_INT(8), qfalse);
        return 0;
    case SYSCALL_SIGHT_TRACE_CAPSULE:
        SV_SightTrace(SV_PTR(int32_t, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4), SV_CONST_PTR(float, 5),
                      SV_INT(6), SV_INT(7), SV_INT(8), qtrue);
        return 0;
    case SYSCALL_SIGHT_TRACE_TO_ENTITY:
        return SV_SightTraceToEntity(SV_CONST_PTR(float, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4),
                                     SV_INT(5), SV_INT(6), qtrue);
    case SYSCALL_CM_BOX_TRACE:
        CM_BoxTrace(SV_PTR(trace_t, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4), SV_CONST_PTR(float, 5),
                    SV_INT(6), SV_INT(7), qfalse);
        return 0;
    case SYSCALL_CM_CAPSULE_TRACE:
        CM_BoxTrace(SV_PTR(trace_t, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4), SV_CONST_PTR(float, 5),
                    SV_INT(6), SV_INT(7), qtrue);
        return 0;
    case SYSCALL_CM_BOX_SIGHT_TRACE:
        return CM_BoxSightTrace(0, SV_CONST_PTR(float, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4),
                                SV_INT(5), SV_INT(6), qfalse);
    case SYSCALL_CM_CAPSULE_SIGHT_TRACE:
        return CM_BoxSightTrace(0, SV_CONST_PTR(float, 1), SV_CONST_PTR(float, 2), SV_CONST_PTR(float, 3), SV_CONST_PTR(float, 4),
                                SV_INT(5), SV_INT(6), qtrue);
    case SYSCALL_LOCATIONAL_TRACE:
        SV_Trace(SV_PTR(trace_t, 1), SV_CONST_PTR(float, 2), NULL, NULL, SV_CONST_PTR(float, 3), SV_INT(4), SV_INT(5), qfalse, qtrue,
                 SV_CONST_PTR(uint8_t, 6), qtrue);
        return 0;
    case SYSCALL_POINT_CONTENTS:
        return SV_PointContents(SV_CONST_PTR(float, 1), SV_INT(2), SV_INT(3));
    case SYSCALL_IN_PVS:
        return SV_inPVS(SV_CONST_PTR(float, 1), SV_CONST_PTR(float, 2));
    case SYSCALL_IN_PVS_IGNORE_PORTALS:
        return SV_inPVSIgnorePortals(SV_CONST_PTR(float, 1), SV_CONST_PTR(float, 2));
    case SYSCALL_IN_SNAPSHOT:
        return SV_inSnapshot(SV_CONST_PTR(float, 1), SV_INT(2));
    case SYSCALL_ADJUST_AREA_PORTAL_STATE:
        SV_AdjustAreaPortalState(SV_PTR(sharedEntity_t, 1), (qboolean)SV_ARG(2));
        return 0;
    case SYSCALL_AREAS_CONNECTED:
        return CM_AreasConnected(SV_INT(1), SV_INT(2));
    case SYSCALL_LINK_ENTITY:
        SV_LinkEntity(SV_PTR(sharedEntity_t, 1));
        return 0;
    case SYSCALL_UNLINK_ENTITY:
        SV_UnlinkEntity(SV_PTR(sharedEntity_t, 1));
        return 0;
    case SYSCALL_ENTITIES_IN_BOX:
        return CM_AreaEntities(SV_CONST_PTR(float, 1), SV_CONST_PTR(float, 2), SV_PTR(int32_t, 3), SV_INT(4), SV_INT(5));
    case SYSCALL_ENTITY_CONTACT:
        return SV_EntityContact(SV_CONST_PTR(float, 1), SV_CONST_PTR(float, 2), SV_PTR(sharedEntity_t, 3), qfalse);
    case SYSCALL_GET_USERCMD:
        SV_GetUsercmd(SV_INT(1), SV_PTR(usercmd_t, 2));
        return 0;
    case SYSCALL_GET_ENTITY_TOKEN: {
        const char *token = Com_Parse(&sv_entityParsePoint);
        Q_strncpyz(SV_PTR(char, 1), token, SV_INT(2));
        return (sv_entityParsePoint == NULL && token[0] == '\0') ? 0 : 1;
    }
    case SYSCALL_FS_GET_FILE_LIST:
        return FS_GetFileList(SV_STRING(1), SV_STRING(2), SV_PTR(char, 3), SV_INT(4));
    case SYSCALL_MAP_EXISTS:
        return SV_MapExists(SV_STRING(1));
    case SYSCALL_REAL_TIME:
        return (intptr_t)Com_RealTime(SV_PTR(qtime_t, 1));
    case SYSCALL_SNAP_VECTOR:
        Sys_SnapVector(SV_PTR(float, 1));
        return 0;
    case SYSCALL_ENTITY_CONTACT_CAPSULE:
        return SV_EntityContact(SV_CONST_PTR(float, 1), SV_CONST_PTR(float, 2), SV_PTR(sharedEntity_t, 3), qtrue);
    case SYSCALL_COM_SOUND_ALIAS_STRING:
        return (intptr_t)Com_SoundAliasString(SV_STRING(1), SND_ALIAS_BANK_GAME);
    case SYSCALL_COM_PICK_SOUND_ALIAS:
        return (intptr_t)Com_PickSoundAlias(SV_STRING(1), SND_ALIAS_BANK_GAME, SV_CONST_PTR(float, 2));
    case SYSCALL_COM_SOUND_ALIAS_INDEX:
        return Com_SoundAliasIndex(SV_PTR(snd_alias_t, 1), SND_ALIAS_BANK_GAME);
    case SYSCALL_SURFACE_TYPE_FROM_NAME:
        return server_compat_surface_type_from_name(SV_STRING(1));
    case SYSCALL_SURFACE_TYPE_TO_NAME:
        return (intptr_t)server_compat_surface_type_to_name(SV_INT(1));
    case SYSCALL_ADD_TEST_CLIENT:
        return SV_AddTestClient();
    case SYSCALL_GET_ARCHIVED_CLIENT_INFO: {
        const int32_t clientNum = SV_INT(1);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
            return qfalse;
        }
        return SV_GetArchivedClientInfo(clientNum, SV_PTR(int32_t, 2), SV_PTR(playerState_t, 3), SV_PTR(clientState_t, 4));
    }
    case SYSCALL_ADD_DEBUG_STRING:
        server_compat_game_debug_string(arguments);
        return 0;
    case SYSCALL_ADD_DEBUG_LINE:
        server_compat_game_debug_line(arguments);
        return 0;
    case SYSCALL_SET_ARCHIVE:
        SV_EnableArchivedSnapshot((qboolean)SV_ARG(1));
        return 0;
    case SYSCALL_Z_MALLOC:
        return (intptr_t)Z_MallocInternal(SV_SIZE(1));
    case SYSCALL_Z_FREE:
        Z_FreeInternal(SV_PTR(void, 1));
        return 0;
    case SYSCALL_XANIM_CREATE_TREE:
        return (intptr_t)Com_XAnimCreateTree(SV_PTR(XAnim, 1));
    case SYSCALL_XANIM_CREATE_SMALL_TREE:
        return (intptr_t)Com_XAnimCreateSmallTree(SV_PTR(XAnim, 1));
    case SYSCALL_XANIM_FREE_SMALL_TREE:
        Com_XAnimFreeSmallTree(SV_PTR(XAnimTree, 1));
        return 0;
    case SYSCALL_XMODEL_EXISTS:
        return XModelExists(SV_STRING(1));
    case SYSCALL_XMODEL_GET:
        return (intptr_t)SV_XModelGet(SV_STRING(1));
    case SYSCALL_DOBJ_CREATE:
        Com_ServerDObjCreate(SV_PTR(DObjModel, 1), (uint16_t)SV_ARG(2), SV_PTR(XAnimTree, 3), SV_INT(4), (uint16_t)SV_ARG(5));
        return 0;
    case SYSCALL_DOBJ_EXISTS:
        return Com_GetServerDObj(SV_INT(1)) != NULL;
    case SYSCALL_SAFE_DOBJ_FREE:
        Com_SafeServerDObjFree(SV_INT(1), (qboolean)SV_ARG(2));
        return 0;
    case SYSCALL_XANIM_GET_ANIMS:
        return (intptr_t)XAnimRuntimeTreeSourceTree(SV_PTR(XAnimTree, 1));
    case SYSCALL_XANIM_CLEAR_TREE_GOAL_WEIGHTS:
        XAnimClearTreeGoalWeights(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)));
        return 0;
    case SYSCALL_XANIM_CLEAR_GOAL_WEIGHT:
        XAnimClearGoalWeight(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)));
        return 0;
    case SYSCALL_XANIM_CLEAR_TREE_GOAL_WEIGHTS_STRICT:
        XAnimClearTreeGoalWeightsStrict(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)));
        return 0;
    case SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB:
        XAnimSetCompleteGoalWeightKnob(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)),
                                       server_compat_game_syscall_float_argument(SV_ARG(4)),
                                       server_compat_game_syscall_float_argument(SV_ARG(5)), (uint16_t)SV_ARG(6), XANIM_ROOT_NODE_INDEX,
                                       (qboolean)SV_ARG(7));
        return 0;
    case SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL:
        return XAnimSetCompleteGoalWeightKnobAll(
            SV_PTR(XAnimTree, 1), SV_INT(2), SV_INT(3), server_compat_game_syscall_float_argument(SV_ARG(4)),
            server_compat_game_syscall_float_argument(SV_ARG(5)), server_compat_game_syscall_float_argument(SV_ARG(6)), (uint16_t)SV_ARG(7),
            XANIM_ROOT_NODE_INDEX, (qboolean)SV_ARG(8));
    case SYSCALL_XANIM_SET_ANIM_RATE:
        XAnimSetAnimRate(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)));
        return 0;
    case SYSCALL_XANIM_SET_TIME:
        XAnimSetTime(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)));
        return 0;
    case SYSCALL_XANIM_SET_GOAL_WEIGHT_KNOB:
        XAnimSetGoalWeightKnob(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)),
                               server_compat_game_syscall_float_argument(SV_ARG(4)), server_compat_game_syscall_float_argument(SV_ARG(5)),
                               (uint16_t)SV_ARG(6), XANIM_ROOT_NODE_INDEX, (qboolean)SV_ARG(7));
        return 0;
    case SYSCALL_XANIM_CLEAR_TREE:
        XAnimClearTree(SV_PTR(XAnimTree, 1));
        return 0;
    case SYSCALL_XANIM_HAS_TIME: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        return XAnimHasTime(tree, SV_INT(2));
    }
    case SYSCALL_XANIM_IS_PRIMITIVE: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        return XAnimIsPrimitive(tree, SV_INT(2));
    }
    case SYSCALL_XANIM_GET_LENGTH:
#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
        return x87f_store_i32_trunc(
            x87f_mul(x87f_load_f32(sv_xanimMilliseconds), x87f_load_f64(XAnimGetLength(SV_PTR(XAnim, 1), SV_INT(2)))));
#else
        return x87f_store_i32_trunc(x87f_mul(x87f_load_f32(sv_xanimMilliseconds), XAnimGetLength(SV_PTR(XAnim, 1), SV_INT(2))));
#endif
#elif defined(CODUO_X87_TRUNCATE_I32)
        return CODUO_X87_SCALE_F32_TRUNCATE_I32(XAnimGetLength(SV_PTR(XAnim, 1), SV_INT(2)), sv_xanimMilliseconds);
#else
        return (int32_t)((long double)sv_xanimMilliseconds * (long double)XAnimGetLength(SV_PTR(XAnim, 1), SV_INT(2)));
#endif
    case SYSCALL_XANIM_GET_LENGTH_SECONDS: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
        return FloatAsInt(x87f_store_f32(x87f_load_f64(XAnimGetLength(tree, SV_INT(2)))));
#else
        return FloatAsInt(x87f_store_f32(XAnimGetLength(tree, SV_INT(2))));
#endif
#else
        return FloatAsInt((float)XAnimGetLength(tree, SV_INT(2)));
#endif
    }
    case SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT:
        XAnimSetCompleteGoalWeight(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)),
                                   server_compat_game_syscall_float_argument(SV_ARG(4)),
                                   server_compat_game_syscall_float_argument(SV_ARG(5)), (uint16_t)SV_ARG(6), XANIM_ROOT_NODE_INDEX,
                                   (qboolean)SV_ARG(7));
        return 0;
    case SYSCALL_XANIM_SET_GOAL_WEIGHT:
        /* Both original handlers discard XAnimSetGoalWeight's return value
         * and explicitly return zero from this syscall. */
        XAnimSetGoalWeight(SV_PTR(XAnimTree, 1), SV_INT(2), server_compat_game_syscall_float_argument(SV_ARG(3)),
                           server_compat_game_syscall_float_argument(SV_ARG(4)), server_compat_game_syscall_float_argument(SV_ARG(5)),
                           (uint16_t)SV_ARG(6), XANIM_ROOT_NODE_INDEX, (qboolean)SV_ARG(7));
        return 0;
    case SYSCALL_XANIM_CALC_ABS_DELTA:
        XAnimCalcAbsDelta(SV_PTR(XAnimTree, 1), SV_INT(2), SV_PTR(float, 3), SV_PTR(float, 4));
        return 0;
    case SYSCALL_XANIM_CALC_DELTA:
        XAnimCalcDelta(SV_PTR(XAnimTree, 1), SV_INT(2), SV_PTR(float, 3), SV_PTR(float, 4), SV_INT(5));
        return 0;
    case SYSCALL_XANIM_GET_REL_DELTA: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        XAnimGetRelDelta(tree, SV_INT(2), SV_PTR(float, 3), SV_PTR(float, 4), server_compat_game_syscall_float_argument(SV_ARG(5)),
                         server_compat_game_syscall_float_argument(SV_ARG(6)));
        return 0;
    }
    case SYSCALL_XANIM_GET_ABS_DELTA: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        XAnimGetAbsDelta(tree, SV_INT(2), SV_PTR(float, 3), SV_PTR(float, 4), server_compat_game_syscall_float_argument(SV_ARG(5)));
        return 0;
    }
    case SYSCALL_XANIM_IS_LOOPED: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        return XAnimIsLooped(tree, SV_INT(2));
    }
    case SYSCALL_XANIM_NOTETRACK_EXISTS: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        return XAnimNotetrackExists(tree, SV_INT(2), (uint16_t)SV_ARG(3));
    }
    case SYSCALL_XANIM_GET_TIME:
        return FloatAsInt(XAnimGetTime(SV_PTR(XAnimTree, 1), SV_INT(2)));
    case SYSCALL_XANIM_GET_WEIGHT:
        return FloatAsInt(XAnimGetWeight(SV_PTR(XAnimTree, 1), SV_INT(2)));
    case SYSCALL_DOBJ_DUMP_INFO:
        SV_DObjDumpInfo(SV_INT(1));
        return 0;
    case SYSCALL_DOBJ_CREATE_SKEL_FOR_BONE:
        return SV_DObjCreateSkelForBone(SV_INT(1), SV_INT(2));
    case SYSCALL_DOBJ_CREATE_SKEL_FOR_BONES:
        return SV_DObjCreateSkelForBones(SV_INT(1), SV_CONST_PTR(uint32_t, 2));
    case SYSCALL_DOBJ_UPDATE_SERVER_TIME:
        return SV_DObjUpdateServerTime(SV_INT(1), server_compat_game_syscall_float_argument(SV_ARG(2)), (qboolean)SV_ARG(3));
    case SYSCALL_DOBJ_INIT_SERVER_TIME:
        SV_DObjInitServerTime(SV_INT(1), server_compat_game_syscall_float_argument(SV_ARG(2)));
        return 0;
    case SYSCALL_DOBJ_GET_HIERARCHY_BITS:
        SV_DObjGetHierarchyBits(SV_INT(1), SV_INT(2), SV_PTR(uint32_t, 3));
        return 0;
    case SYSCALL_DOBJ_CALC_ANIM:
        SV_DObjCalcAnim(SV_INT(1), SV_CONST_PTR(uint32_t, 2));
        return 0;
    case SYSCALL_DOBJ_CALC_SKEL:
        SV_DObjCalcSkel(SV_INT(1), SV_CONST_PTR(uint32_t, 2));
        return 0;
    case SYSCALL_XANIM_LOAD_ANIM_TREE:
        XAnimLoadAnimTree(SV_PTR(XAnimTree, 1));
        return 0;
    case SYSCALL_XANIM_SAVE_ANIM_TREE:
        XAnimSaveAnimTree(SV_PTR(XAnimTree, 1));
        return 0;
    case SYSCALL_XANIM_CLONE_ANIM_TREE:
        XAnimCloneAnimTree(SV_PTR(XAnimTree, 1), SV_PTR(XAnimTree, 2));
        return 0;
    case SYSCALL_DOBJ_NUM_BONES:
        return SV_DObjNumBones(SV_INT(1));
    case SYSCALL_DOBJ_GET_BONE_INDEX:
        return SV_DObjGetBoneIndex(SV_INT(1), SV_STRING(2));
    case SYSCALL_DOBJ_GET_MATRIX_ARRAY:
        return (intptr_t)SV_DObjGetMatrixArray(SV_INT(1));
    case SYSCALL_DOBJ_DISPLAY_ANIM:
        SV_DObjDisplayAnim(SV_INT(1));
        return 0;
    case SYSCALL_XANIM_HAS_FINISHED:
        return XAnimHasFinished(SV_PTR(XAnimTree, 1), SV_INT(2));
    case SYSCALL_XANIM_GET_NUM_CHILDREN: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        return XAnimGetNumChildren(tree, SV_INT(2));
    }
    case SYSCALL_XANIM_GET_CHILD_AT: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        return XAnimGetChildAt(tree, SV_INT(2), SV_INT(3));
    }
    case SYSCALL_XMODEL_NUM_BONES:
        return XModelNumBones(SV_PTR(XModel, 1));
    case SYSCALL_XMODEL_GET_BONE_NAMES:
        return (intptr_t)XModelBoneNames(SV_PTR(XModel, 1));
    case SYSCALL_DOBJ_GET_ROT_TRANS_ARRAY:
        return (intptr_t)SV_DObjGetRotTransArray(SV_INT(1));
    case SYSCALL_DOBJ_SET_ROT_TRANS_INDEX:
        return SV_DObjSetRotTransIndex(SV_INT(1), SV_CONST_PTR(uint8_t, 2), SV_INT(3));
    case SYSCALL_DOBJ_SET_CONTROL_ROT_TRANS_INDEX:
        return SV_DObjSetControlRotTransIndex(SV_INT(1), SV_CONST_PTR(uint8_t, 2), SV_INT(3));
    case SYSCALL_DOBJ_GET_BOUNDS:
        SV_DObjGetBounds(SV_INT(1), SV_PTR(float, 2), SV_PTR(float, 3));
        return 0;
    case SYSCALL_XANIM_GET_ANIM_NAME: {
        XAnim *tree = Scr_GetAnims((uint32_t)SV_ARG(1));
        /* CoDUOMP.exe 0x0045e676 loads the complete dword argument before
         * tail-calling XAnimGetAnimName; the former uint16_t cast was a
         * reconstruction-only truncation. */
        return (intptr_t)XAnimGetAnimName(tree, SV_INT(2));
    }
    case SYSCALL_DOBJ_GET_TREE:
        return (intptr_t)SV_DObjGetTree(SV_INT(1));
    case SYSCALL_XANIM_GET_ANIM_TREE_SIZE:
        return XAnimGetAnimTreeSize(SV_PTR(XAnim, 1));
    case SYSCALL_XMODEL_DEBUG_BOXES:
        SV_XModelDebugBoxes(SV_INT(1));
        return 0;
    case SYSCALL_GET_WEAPON_INFO_MEMORY:
        return (intptr_t)server_compat_get_weapon_info_memory(SV_INT(1), SV_PTR(int32_t, 2), SERVER_GAME_SYSCALL_WEAPON_MEMORY_OWNER);
    case SYSCALL_FREE_WEAPON_INFO_MEMORY:
        server_compat_free_weapon_info_memory(SERVER_GAME_SYSCALL_WEAPON_MEMORY_OWNER, (qboolean)SV_ARG(1));
        return 0;
    case SYSCALL_FREE_CLIENT_SCRIPT_PERS:
        SV_FreeClientScriptPers();
        return 0;
    case SYSCALL_RESET_ENTITY_PARSE_POINT:
        SV_ResetEntityParsePoint();
        return 0;
    case SYSCALL_MEMSET:
        memset(SV_PTR(void, 1), SV_INT(2), SV_SIZE(3));
        return 0;
    case SYSCALL_MEMCPY:
        memcpy(SV_PTR(void, 1), SV_CONST_PTR(void, 2), SV_SIZE(3));
        return 0;
    case SYSCALL_STRNCPY:
        /* Preserve the original i386 size_t interpretation of the raw word
         * when the compatibility VM transports it through intptr_t. */
        return (intptr_t)strncpy(SV_PTR(char, 1), SV_STRING(2), (size_t)(uint32_t)SV_ARG(3));
    case SYSCALL_SIN:
        return FloatAsInt((float)sin((double)server_compat_game_syscall_float_argument(SV_ARG(1))));
    case SYSCALL_COS:
        return FloatAsInt((float)cos((double)server_compat_game_syscall_float_argument(SV_ARG(1))));
    case SYSCALL_ATAN2:
        return FloatAsInt((float)atan2((double)server_compat_game_syscall_float_argument(SV_ARG(1)),
                                       (double)server_compat_game_syscall_float_argument(SV_ARG(2))));
    case SYSCALL_SQRT:
        return FloatAsInt((float)sqrt((double)server_compat_game_syscall_float_argument(SV_ARG(1))));
    case SYSCALL_MATRIX_MULTIPLY:
        MatrixMultiply(SV_PTR(vec3_t, 1), SV_PTR(vec3_t, 2), SV_PTR(vec3_t, 3));
        return 0;
    case SYSCALL_ANGLE_VECTORS:
        AngleVectors(SV_CONST_PTR(float, 1), SV_PTR(float, 2), SV_PTR(float, 3), SV_PTR(float, 4));
        return 0;
    case SYSCALL_PERPENDICULAR_VECTOR:
        PerpendicularVector(SV_PTR(float, 1), SV_CONST_PTR(float, 2));
        return 0;
    case SYSCALL_FLOOR:
        return FloatAsInt((float)floor((double)server_compat_game_syscall_float_argument(SV_ARG(1))));
    case SYSCALL_CEIL:
        return FloatAsInt((float)ceil((double)server_compat_game_syscall_float_argument(SV_ARG(1))));
    default:
        Com_Error(ERR_DROP,
                  "\x15"
                  "Bad game system trap: %i",
                  syscall);
        return -1;
    }
}

#undef SV_SIZE
#undef SV_INT
#undef SV_STRING
#undef SV_CONST_PTR
#undef SV_PTR
#undef SV_ARG
