#ifndef QCOMMON_CGAME_MODULE_ABI_TYPES_H
#define QCOMMON_CGAME_MODULE_ABI_TYPES_H

/* vmMain is exported as ordinal 2. The 21 command values and every destination
 * come from the jump table at 0x3002b148 in uo_cgame_mp_x86.dll. Names through
 * command 9 are the established cgame VM interface; extension names describe
 * their machine-code-proven targets. CoDUOMP.exe uses the same values when it
 * calls the cgame module. */
typedef enum cgameVmCommand_e {
    CGVM_GET_API_VERSION = 0,
    CGVM_INIT = 1,
    CGVM_SHUTDOWN = 2,
    CGVM_CONSOLE_COMMAND = 3,
    CGVM_DRAW_ACTIVE_FRAME = 4,
    CGVM_CROSSHAIR_PLAYER = 5,
    CGVM_LAST_ATTACKER = 6,
    CGVM_KEY_EVENT = 7,
    CGVM_MOUSE_EVENT = 8,
    CGVM_EVENT_HANDLING = 9,
    CGVM_GET_MODEL_HANDLE = 10,
    CGVM_DOBJ_CALC_POSE = 11,
    CGVM_DOBJ_CALC_BONE_GENERIC = 12,
    CGVM_GET_ENTITY_ORIGIN_AXIS = 13,
    CGVM_GET_EFFECT_ORIGIN_AXIS = 14,
    CGVM_IMPACT_MARK = 15,
    CGVM_ADD_CAMERA_SHAKE = 16,
    CGVM_DRAW_SCALED = 17,
    CGVM_SCRIPT_FAR_HOOK = 18,
    CGVM_SAVE_STATE = 19,
    CGVM_RESTORE_STATE = 20
} cgVmCommand_t;

enum {
    CGVM_API_VERSION = 2,
    CGVM_COMMAND_COUNT = 21,
    CGVM_LAST_COMMAND = CGVM_RESTORE_STATE
};

#endif
