#ifndef QCOMMON_SERVER_TYPES_H
#define QCOMMON_SERVER_TYPES_H

/* Complete server-to-client message opcode domain used by the Windows client
 * and Linux server. Values 2..5 are emitted while constructing gamestate;
 * values 6..8 are also used by download and snapshot delivery. */
typedef enum serverMessageCommand_e {
    SERVER_SVC_GAMESTATE = 2,
    SERVER_SVC_CONFIGSTRING = 3,
    SERVER_SVC_BASELINE = 4,
    SERVER_SVC_SERVER_COMMAND = 5,
    SERVER_SVC_DOWNLOAD = 6,
    SERVER_SVC_SNAPSHOT = 7,
    SERVER_SVC_EOF = 8
} serverMessageCommand_t;

typedef enum serverClientState_e {
    CS_FREE = 0,
    CS_ZOMBIE = 1,
    CS_CONNECTED = 2,
    CS_PRIMED = 3,
    CS_ACTIVE = 4
} serverClientState_t;

typedef enum serverState_e {
    SS_DEAD = 0,
    SS_LOADING = 1,
    SS_GAME = 2
} serverState_t;

/* Server entity flags stored in the shared game/engine entity prefix. */
typedef enum serverEntityFlags_e {
    SVF_NOCLIENT = 0x00000001u,
    SVF_DOBJ_USE_DEFAULT_BOUNDS = 0x00000002u,
    SVF_DOBJ_USE_MODEL_BOUNDS = 0x00000004u,
    SVF_LOOPED_FX = 0x00000008u,
    SVF_OBJECTIVE = 0x00000010u,
    SVF_CAPSULE = 0x00000200u,
    SVF_SINGLECLIENT = 0x00000800u,
    SVF_NOTSINGLECLIENT = 0x00002000u,
    SVF_DOBJ_BOUNDS_MASK = SVF_DOBJ_USE_DEFAULT_BOUNDS | SVF_DOBJ_USE_MODEL_BOUNDS,
    SVF_VISIBILITY_BYPASS_MASK = SVF_LOOPED_FX | SVF_OBJECTIVE
} serverEntityFlags_t;

#endif
