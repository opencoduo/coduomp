#include "server.h"
#include "qcommon/server_punkbuster_types.h"

#include "../animation/dobj.h"
#include "../client/debug_lines.h"
#include "../math/vector_math.h"
#include "qcommon/hunk.h"
#include "../physics/cm_trace.h"
#include "../scripting/script_runtime.h"
#include "../system_fatal.h"
#include "../ui/ui_module_loader.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Original server-static storage starts at 0x04907bc0; its clients member is
 * the pointer loaded from 0x04907bd0 by both player-selection helpers. */
serverStatic_t svs;
serverHeader_t sv;
cvar_t *sv_maxclients; /* original 0x0491ccd0 */
cvar_t *g_gametype;    /* original 0x0491ccf4 */
cvar_t *sv_mapname;    /* original 0x0491ccf8 */
cvar_t *sv_mapRotationCurrent; /* original 0x0491cd0c */
cvar_t *scr_allow_tanks;       /* original 0x0491cd14 */
cvar_t *sv_mapRotation;        /* original 0x0491cd44 */
cvar_t *scr_allow_jeeps;       /* original 0x0491cd4c */
cvar_t *dedicated;             /* original 0x049290a4 */
cvar_t *sv_onlyVisibleClients; /* original 0x0491cd24 */
cvar_t *sv_kickBanTime;        /* original 0x0491cce4 */
cvar_t *sv_reconnectlimit;      /* original 0x0491cce0 */
cvar_t *sv_minPing;             /* original 0x0491cd18 */
cvar_t *sv_maxPing;             /* original 0x048a5670 */
cvar_t *sv_privateClients;      /* original 0x0491cccc */
cvar_t *sv_privatePassword;     /* original 0x0491cd20 */
cvar_t *sv_allowDownload;       /* original 0x0491ccec */
cvar_t *sv_maxRate;             /* original 0x0491cd00 */
cvar_t *sv_wwwDownload;         /* original 0x0491cd2c */
cvar_t *sv_wwwBaseURL;          /* original 0x0491cd38 */
cvar_t *sv_wwwDlDisconnected;   /* original 0x0491ccfc */
cvar_t *sv_pure;                /* original 0x04907bac */
cvar_t *sv_showCommands;        /* original 0x0491cd04 */
cvar_t *sv_floodProtect;        /* original 0x0491cce8 */
cvar_t *sv_hostname;            /* original 0x0491ccd4 */
cvar_t *sv_punkbuster;          /* original 0x0491cd48 */
cvar_t *sv_allowAnonymous;      /* original 0x0491ccdc */
cvar_t *sv_disableClientConsole; /* original 0x0491cd10 */
cvar_t *sv_serverid;            /* original 0x0491cd40 */
cvar_t *rconPassword;           /* original 0x04907bb0 */
cvar_t *sv_fps;                 /* original 0x0491cd3c */
cvar_t *sv_timeout;             /* original 0x0491cd30 */
cvar_t *sv_zombietime;          /* original 0x0491cd1c */
cvar_t *sv_showloss;            /* original 0x0491ccd8 */
cvar_t *sv_padPackets;          /* original 0x0491cd34 */
cvar_t *sv_killserver;          /* original 0x0491ccf0 */
cvar_t *sv_packet_info;         /* original 0x0491cd08 */
cvar_t *sv_showAverageBPS;      /* original 0x0491cd28 */
/* Original 0x005cef30..0x005cf097. The PE initializes only magic +0x000 and
 * loadPending +0x138; all module handles and callback slots begin zero. */
serverPbState_t sv_pbServerState = {
    .magic = SERVER_PUNKBUSTER_MAGIC,
    .loadPending = qtrue
}; /* original 0x005cef30 */
char sv_gametypeNormalizeBuffer[MAX_QPATH]; /* original 0x04907b6c */
vm_t *sv_gameVM;       /* original 0x0389fdbc */
int32_t sv_serverId;        /* original 0x0389fdb8 */
int32_t sv_reconnectSequence; /* original 0x0389fdb4 */
sharedEntity_t *sv_gentities; /* original 0x04907a9c */
int32_t sv_gentitySize;       /* original 0x04907aa0 */
int32_t sv_numGentities;      /* original 0x04907aa4 */
playerState_t *sv_gameClients; /* original 0x04907aa8 */
int32_t sv_gameClientSize;     /* original 0x04907aac */
/* Original pointers begin at 0x048a5a98. */
char *sv_configstrings[MAX_CONFIGSTRINGS];
/* Original 0x180-byte svEntity table begins at 0x048a7a98. */
svEntity_t sv_entities[MAX_GENTITIES];

/* Source: CoDUOMP.exe 0x0045a8b0..0x0045a8c4, recovered from an executable
 * gap after repairing the surrounding INT3 boundaries.
 * Name: same-module Mac symbol SV_DelayDropClient. The first pending reason
 * wins until the client lifecycle consumes or clears it. */
void SV_DelayDropClient(client_t *client, const char *dropReason)
{
    if (client->state != CS_ZOMBIE &&
        client->deferredDropReason == NULL) {
        client->deferredDropReason = dropReason;
    }
}

/* Source: CoDUOMP.exe 0x0045d460..0x0045d7da.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0045d460_0045d7db.mcode.
 * Name and entity argument: exact same-module Mac symbol
 * SV_XModelDebugBoxes. */
void SV_XModelDebugBoxes(int32_t entityNum)
{
    /* Source: CoDUOMP.exe 0x005ca3a8..0x005ca4c7. The original table contains
     * one min/max selector for each axis of both endpoints of all 12 edges. */
    static const int32_t serverBoxCornerSelectors[12][2][3] = { /* 0x005ca3a8 */
        {{0, 0, 0}, {1, 0, 0}}, {{0, 0, 0}, {0, 1, 0}},
        {{1, 1, 0}, {1, 0, 0}}, {{1, 1, 0}, {0, 1, 0}},
        {{0, 0, 1}, {1, 0, 1}}, {{0, 0, 1}, {0, 1, 1}},
        {{1, 1, 1}, {1, 0, 1}}, {{1, 1, 1}, {0, 1, 1}},
        {{0, 0, 0}, {0, 0, 1}}, {{1, 0, 0}, {1, 0, 1}},
        {{0, 1, 0}, {0, 1, 1}}, {{1, 1, 0}, {1, 1, 1}}
    };
    const vec4_t boxColor = {1.0f, 1.0f, 1.0f, 0.0f};
    DObj *obj = Com_GetServerDObj(entityNum);
    XModelPartColl **partCollisions = CODUOMP_ALLOCA(
        (size_t)obj->boneCount * sizeof(*partCollisions));
    DObjSkelMat *boneMatrix = DObjGetMatrixArray(obj, 0);
    sharedEntity_t *gentity = (sharedEntity_t *)(
        (uint8_t *)sv_gentities + (ptrdiff_t)entityNum * sv_gentitySize);
    axis_t entityAxis;
    vec3_t right;

    DObjGetBoneInfo(obj, partCollisions);
    AngleVectors(gentity->currentAngles, entityAxis[0], right,
                 entityAxis[2]);
    for (int32_t component = 0; component < 3; ++component) {
        entityAxis[1][component] = -right[component];
    }

    for (int32_t modelIndex = 0; modelIndex < obj->modelCount;
         ++modelIndex) {
        int32_t partCount;

        if ((obj->collisionSkipModelMask & (1U << modelIndex)) != 0) {
            continue;
        }

        partCount = XModelNumBones(obj->models[modelIndex]);
        for (int32_t partIndex = 0; partIndex < partCount; ++partIndex) {
            const XModelPartColl *collision = *partCollisions++;

            for (int32_t edgeIndex = 0; edgeIndex < 12; ++edgeIndex) {
                vec3_t worldPoints[2];

                for (int32_t endpoint = 0; endpoint < 2; ++endpoint) {
                    vec3_t localPoint;
                    vec3_t modelPoint;
                    vec3_t rotatedPoint;

                    for (int32_t axis = 0; axis < 3; ++axis) {
                        localPoint[axis] =
                            serverBoxCornerSelectors[edgeIndex][endpoint][axis]
                            ? collision->maxs[axis] : collision->mins[axis];
                    }

                    /* These groupings reproduce the inlined x87 transform,
                     * including its component-specific addition order. */
                    modelPoint[0] =
                        ((localPoint[0] * boneMatrix->axis[0][0] +
                          localPoint[2] * boneMatrix->axis[2][0]) +
                         localPoint[1] * boneMatrix->axis[1][0]) +
                        boneMatrix->origin[0];
                    modelPoint[1] =
                        ((localPoint[2] * boneMatrix->axis[2][1] +
                          localPoint[1] * boneMatrix->axis[1][1]) +
                         localPoint[0] * boneMatrix->axis[0][1]) +
                        boneMatrix->origin[1];
                    modelPoint[2] =
                        ((localPoint[0] * boneMatrix->axis[0][2] +
                          localPoint[2] * boneMatrix->axis[2][2]) +
                         localPoint[1] * boneMatrix->axis[1][2]) +
                        boneMatrix->origin[2];

                    rotatedPoint[0] =
                        (modelPoint[2] * entityAxis[2][0] +
                         modelPoint[1] * entityAxis[1][0]) +
                        modelPoint[0] * entityAxis[0][0];
                    rotatedPoint[1] =
                        (modelPoint[2] * entityAxis[2][1] +
                         modelPoint[0] * entityAxis[0][1]) +
                        modelPoint[1] * entityAxis[1][1];
                    rotatedPoint[2] =
                        (modelPoint[2] * entityAxis[2][2] +
                         modelPoint[0] * entityAxis[0][2]) +
                        modelPoint[1] * entityAxis[1][2];
                    worldPoints[endpoint][0] =
                        rotatedPoint[0] + gentity->currentOrigin[0];
                    worldPoints[endpoint][1] =
                        rotatedPoint[1] + gentity->currentOrigin[1];
                    worldPoints[endpoint][2] =
                        rotatedPoint[2] + gentity->currentOrigin[2];
                }

                CL_AddDebugLine(worldPoints[0], worldPoints[1], boxColor,
                                qtrue, 0, qfalse);
            }
            ++boneMatrix;
        }
    }
}
