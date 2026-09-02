#ifndef CODUO_VM_NATIVE_VARARGS_H
#define CODUO_VM_NATIVE_VARARGS_H

/*
 * NOT_FROM_ORIGINAL_SOURCE: i386 cdecl exposes VM varargs as contiguous
 * pointer-sized stack words. Native 64-bit ABIs do not, so the shared VM
 * runtime needs the promoted host type of each argument before copying it
 * into the common intptr_t vector. Each layout string has one promoted-type
 * code per argument:
 * i = signed int, u = unsigned int, p = pointer, and z = size_t. The strings
 * and their lengths are both consumed by that adapter. Game-side boundary
 * calls explicitly cast enum and narrow integer arguments where necessary so
 * these promoted types do not depend on a compiler's enum-compatible type.
 */

#include <stddef.h>

typedef enum coduo_native_vararg_type_e {
    CODUO_NATIVE_VARARG_SIGNED_INT = 'i',
    CODUO_NATIVE_VARARG_UNSIGNED_INT = 'u',
    CODUO_NATIVE_VARARG_POINTER = 'p',
    CODUO_NATIVE_VARARG_SIZE = 'z'
} coduo_native_vararg_type_t;

typedef struct coduo_native_vararg_layout_s {
    const char *argumentTypes;
    size_t argumentCount;
} coduo_native_vararg_layout_t;

#define CODUO_NATIVE_VARARG_TYPE_AT(layout, slot) ((coduo_native_vararg_type_t)((layout).argumentTypes[(slot)]))

#define CODUO_VM_NATIVE_GAME_SYSCALL_LAYOUTS(X) \
    X(SYSCALL_PRINTF, "p") \
    X(SYSCALL_ERROR, "p") \
    X(SYSCALL_ERROR_LOCALIZED, "p") \
    X(SYSCALL_MILLISECONDS, "") \
    X(SYSCALL_CVAR_REGISTER, "pppi") \
    X(SYSCALL_CVAR_UPDATE, "p") \
    X(SYSCALL_CVAR_SET, "pp") \
    X(SYSCALL_CVAR_VARIABLE_INTEGER_VALUE, "p") \
    X(SYSCALL_CVAR_VARIABLE_VALUE, "pp") \
    X(SYSCALL_CVAR_VARIABLE_STRING_BUFFER, "ppi") \
    X(SYSCALL_ARGC, "") \
    X(SYSCALL_ARGV, "ipi") \
    X(SYSCALL_HUNK_ALLOC, "z") \
    X(SYSCALL_HUNK_ALLOC_LOW, "z") \
    X(SYSCALL_HUNK_ALLOC_ALIGN, "zi") \
    X(SYSCALL_HUNK_ALLOC_LOW_ALIGN, "zi") \
    X(SYSCALL_HUNK_ALLOC_TEMP, "z") \
    X(SYSCALL_HUNK_FREE_TEMP, "p") \
    X(SYSCALL_FS_FOPEN_FILE, "ppu") \
    X(SYSCALL_FS_READ, "pii") \
    X(SYSCALL_FS_WRITE, "pii") \
    X(SYSCALL_FS_RENAME, "pp") \
    X(SYSCALL_FS_FCLOSE_FILE, "i") \
    X(SYSCALL_SEND_CONSOLE_COMMAND, "ip") \
    X(SYSCALL_LOCATE_GAME_DATA, "piipi") \
    X(SYSCALL_GET_GUID, "i") \
    X(SYSCALL_DROP_CLIENT, "ip") \
    X(SYSCALL_SEND_SERVER_COMMAND, "iip") \
    X(SYSCALL_SET_CONFIGSTRING, "ip") \
    X(SYSCALL_GET_CONFIGSTRING, "ipi") \
    X(SYSCALL_GET_CONFIGSTRING_CONST, "i") \
    X(SYSCALL_IS_LOCAL_CLIENT, "i") \
    X(SYSCALL_GET_CLIENT_PING, "i") \
    X(SYSCALL_GET_USERINFO, "ipi") \
    X(SYSCALL_SET_USERINFO, "ip") \
    X(SYSCALL_GET_SERVERINFO, "pi") \
    X(SYSCALL_SET_BRUSH_MODEL, "p") \
    X(SYSCALL_TRACE, "pppppii") \
    X(SYSCALL_TRACE_CAPSULE, "pppppii") \
    X(SYSCALL_SIGHT_TRACE, "pppppiii") \
    X(SYSCALL_SIGHT_TRACE_CAPSULE, "pppppiii") \
    X(SYSCALL_SIGHT_TRACE_TO_ENTITY, "ppppii") \
    X(SYSCALL_CM_BOX_TRACE, "pppppii") \
    X(SYSCALL_CM_CAPSULE_TRACE, "pppppii") \
    X(SYSCALL_CM_BOX_SIGHT_TRACE, "ppppii") \
    X(SYSCALL_CM_CAPSULE_SIGHT_TRACE, "ppppii") \
    X(SYSCALL_LOCATIONAL_TRACE, "pppiip") \
    X(SYSCALL_POINT_CONTENTS, "pii") \
    X(SYSCALL_IN_PVS, "pp") \
    X(SYSCALL_IN_PVS_IGNORE_PORTALS, "pp") \
    X(SYSCALL_IN_SNAPSHOT, "pi") \
    X(SYSCALL_ADJUST_AREA_PORTAL_STATE, "pi") \
    X(SYSCALL_AREAS_CONNECTED, "ii") \
    X(SYSCALL_LINK_ENTITY, "p") \
    X(SYSCALL_UNLINK_ENTITY, "p") \
    X(SYSCALL_ENTITIES_IN_BOX, "pppii") \
    X(SYSCALL_ENTITY_CONTACT, "ppp") \
    X(SYSCALL_GET_USERCMD, "ip") \
    X(SYSCALL_GET_ENTITY_TOKEN, "pi") \
    X(SYSCALL_FS_GET_FILE_LIST, "pppi") \
    X(SYSCALL_MAP_EXISTS, "p") \
    X(SYSCALL_REAL_TIME, "p") \
    X(SYSCALL_SNAP_VECTOR, "p") \
    X(SYSCALL_ENTITY_CONTACT_CAPSULE, "ppp") \
    X(SYSCALL_COM_SOUND_ALIAS_STRING, "p") \
    X(SYSCALL_COM_PICK_SOUND_ALIAS, "pp") \
    X(SYSCALL_COM_SOUND_ALIAS_INDEX, "p") \
    X(SYSCALL_SURFACE_TYPE_FROM_NAME, "p") \
    X(SYSCALL_SURFACE_TYPE_TO_NAME, "i") \
    X(SYSCALL_ADD_TEST_CLIENT, "") \
    X(SYSCALL_GET_ARCHIVED_CLIENT_INFO, "ippp") \
    X(SYSCALL_ADD_DEBUG_STRING, "ppup") \
    X(SYSCALL_ADD_DEBUG_LINE, "pppii") \
    X(SYSCALL_SET_ARCHIVE, "i") \
    X(SYSCALL_Z_MALLOC, "z") \
    X(SYSCALL_Z_FREE, "p") \
    X(SYSCALL_XANIM_CREATE_TREE, "p") \
    X(SYSCALL_XANIM_CREATE_SMALL_TREE, "p") \
    X(SYSCALL_XANIM_FREE_SMALL_TREE, "p") \
    X(SYSCALL_XMODEL_EXISTS, "p") \
    X(SYSCALL_XMODEL_GET, "p") \
    X(SYSCALL_DOBJ_CREATE, "pupiu") \
    X(SYSCALL_DOBJ_EXISTS, "i") \
    X(SYSCALL_SAFE_DOBJ_FREE, "ii") \
    X(SYSCALL_XANIM_GET_ANIMS, "p") \
    X(SYSCALL_XANIM_CLEAR_TREE_GOAL_WEIGHTS, "piu") \
    X(SYSCALL_XANIM_CLEAR_GOAL_WEIGHT, "piu") \
    X(SYSCALL_XANIM_CLEAR_TREE_GOAL_WEIGHTS_STRICT, "piu") \
    X(SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB, "piuuuii") \
    X(SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL, "piuuuuii") \
    X(SYSCALL_XANIM_SET_ANIM_RATE, "piu") \
    X(SYSCALL_XANIM_SET_TIME, "piu") \
    X(SYSCALL_XANIM_SET_GOAL_WEIGHT_KNOB, "piuuuii") \
    X(SYSCALL_XANIM_CLEAR_TREE, "p") \
    X(SYSCALL_XANIM_HAS_TIME, "ii") \
    X(SYSCALL_XANIM_IS_PRIMITIVE, "ii") \
    X(SYSCALL_XANIM_GET_LENGTH, "pi") \
    X(SYSCALL_XANIM_GET_LENGTH_SECONDS, "ii") \
    X(SYSCALL_XANIM_SET_COMPLETE_GOAL_WEIGHT, "piuuuii") \
    X(SYSCALL_XANIM_SET_GOAL_WEIGHT, "piuuuii") \
    X(SYSCALL_XANIM_CALC_ABS_DELTA, "pipp") \
    X(SYSCALL_XANIM_CALC_DELTA, "pippi") \
    X(SYSCALL_XANIM_GET_REL_DELTA, "iippuu") \
    X(SYSCALL_XANIM_GET_ABS_DELTA, "iippu") \
    X(SYSCALL_XANIM_IS_LOOPED, "ii") \
    X(SYSCALL_XANIM_NOTETRACK_EXISTS, "iii") \
    X(SYSCALL_XANIM_GET_TIME, "pi") \
    X(SYSCALL_XANIM_GET_WEIGHT, "pi") \
    X(SYSCALL_DOBJ_DUMP_INFO, "i") \
    X(SYSCALL_DOBJ_CREATE_SKEL_FOR_BONE, "ii") \
    X(SYSCALL_DOBJ_CREATE_SKEL_FOR_BONES, "ip") \
    X(SYSCALL_DOBJ_UPDATE_SERVER_TIME, "iui") \
    X(SYSCALL_DOBJ_INIT_SERVER_TIME, "iu") \
    X(SYSCALL_DOBJ_GET_HIERARCHY_BITS, "iip") \
    X(SYSCALL_DOBJ_CALC_ANIM, "ip") \
    X(SYSCALL_DOBJ_CALC_SKEL, "ip") \
    X(SYSCALL_XANIM_LOAD_ANIM_TREE, "p") \
    X(SYSCALL_XANIM_SAVE_ANIM_TREE, "p") \
    X(SYSCALL_XANIM_CLONE_ANIM_TREE, "pp") \
    X(SYSCALL_DOBJ_NUM_BONES, "i") \
    X(SYSCALL_DOBJ_GET_BONE_INDEX, "ip") \
    X(SYSCALL_DOBJ_GET_MATRIX_ARRAY, "i") \
    X(SYSCALL_DOBJ_DISPLAY_ANIM, "i") \
    X(SYSCALL_XANIM_HAS_FINISHED, "pi") \
    X(SYSCALL_XANIM_GET_NUM_CHILDREN, "ii") \
    X(SYSCALL_XANIM_GET_CHILD_AT, "iii") \
    X(SYSCALL_XMODEL_NUM_BONES, "p") \
    X(SYSCALL_XMODEL_GET_BONE_NAMES, "p") \
    X(SYSCALL_DOBJ_GET_ROT_TRANS_ARRAY, "i") \
    X(SYSCALL_DOBJ_SET_ROT_TRANS_INDEX, "ipi") \
    X(SYSCALL_DOBJ_SET_CONTROL_ROT_TRANS_INDEX, "ipi") \
    X(SYSCALL_DOBJ_GET_BOUNDS, "ipp") \
    X(SYSCALL_XANIM_GET_ANIM_NAME, "ii") \
    X(SYSCALL_DOBJ_GET_TREE, "i") \
    X(SYSCALL_XANIM_GET_ANIM_TREE_SIZE, "p") \
    X(SYSCALL_XMODEL_DEBUG_BOXES, "i") \
    X(SYSCALL_GET_WEAPON_INFO_MEMORY, "ip") \
    X(SYSCALL_FREE_WEAPON_INFO_MEMORY, "i") \
    X(SYSCALL_FREE_CLIENT_SCRIPT_PERS, "") \
    X(SYSCALL_RESET_ENTITY_PARSE_POINT, "") \
    X(SYSCALL_MEMSET, "piz") \
    X(SYSCALL_MEMCPY, "ppz") \
    X(SYSCALL_STRNCPY, "ppi") \
    X(SYSCALL_SIN, "u") \
    X(SYSCALL_COS, "u") \
    X(SYSCALL_ATAN2, "uu") \
    X(SYSCALL_SQRT, "u") \
    X(SYSCALL_MATRIX_MULTIPLY, "ppp") \
    X(SYSCALL_ANGLE_VECTORS, "pppp") \
    X(SYSCALL_PERPENDICULAR_VECTOR, "pp") \
    X(SYSCALL_FLOOR, "u") \
    X(SYSCALL_CEIL, "u")

#define CODUO_VM_NATIVE_GAME_COMMAND_LAYOUTS(X) \
    X(GAME_GET_API_VERSION, "") \
    X(GAME_INIT, "iiii") \
    X(GAME_SHUTDOWN, "i") \
    X(GAME_CLIENT_CONNECT, "ii") \
    X(GAME_CLIENT_BEGIN, "i") \
    X(GAME_CLIENT_USERINFO_CHANGED, "i") \
    X(GAME_CLIENT_DISCONNECT, "i") \
    X(GAME_CLIENT_COMMAND, "i") \
    X(GAME_CLIENT_THINK, "i") \
    X(GAME_GET_FOLLOW_PLAYER_STATE, "ip") \
    X(GAME_UPDATE_CVARS, "") \
    X(GAME_RUN_FRAME, "i") \
    X(GAME_CONSOLE_COMMAND, "") \
    X(GAME_SCRIPT_FAR_HOOK, "p") \
    X(GAME_DOBJ_CALC_POSE, "i") \
    X(GAME_IS_VALID_WEAPON, "i") \
    X(GAME_SET_MATCH_STATE, "i") \
    X(GAME_GET_MATCH_STATE, "") \
    X(GAME_GET_CLIENT_STATE, "i") \
    X(GAME_GET_CLIENT_ARCHIVE_TIME, "i") \
    X(GAME_SET_CLIENT_ARCHIVE_TIME, "ii") \
    X(GAME_GET_CLIENT_SCORE, "i") \
    X(GAME_GET_FOG_OPAQUE_DIST_SQ_BITS, "")

#endif /* CODUO_VM_NATIVE_VARARGS_H */
