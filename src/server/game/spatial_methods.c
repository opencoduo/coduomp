/*
 * Source reconstruction for script spatial methods.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>

#include "recovered_game.h"
#include "game_globals.h"
#include "level_locals.h"
#include "scr_vm.h"

qboolean trap_DObjExists(gentity_t *ent);
void trap_DObjDumpInfo(gentity_t *ent);
int G_DObjGetWorldTagMatrix(gentity_t *ent, const char *tagName,
                            DObjSkelMat *matrix);
const char *G_ModelName(int modelIndex);

#define SCRIPT_STANCE_FLAG_PRONE UINT32_C(1)
#define SCRIPT_STANCE_FLAG_CROUCHED UINT32_C(2)

/* 0x68338 ScrCmd_GetOrigin */
/* VERIFIED_DECOMPILER(0x68338, 78338_script_method_scriptbuiltin_getorigin.c, VERIFY-SPATIAL-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ScrCmd_GetOrigin(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    vec3_t origin;

    origin[0] = ent->currentOrigin[0];
    origin[1] = ent->currentOrigin[1];
    origin[2] = ent->currentOrigin[2];

    Scr_AddVector(origin);
}

/* 0x6838d ScriptSpatial_GetTagMatrix */
/* VERIFIED_DECOMPILER(0x6838d, 7838d_FUN_0007838d.c, VERIFY-SPATIAL-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
static DObjSkelMat *ScriptSpatial_GetTagMatrix(uint32_t scriptObject)
{
    uint16_t tagName;
    gentity_t *ent;
    level_locals_t *lvl = &level;

    if ((uint32_t)lvl->cachedTagMatrixObject == scriptObject &&
        lvl->cachedTagMatrixTime == level.time) {
        tagName = Scr_GetConstString(0);
        if (lvl->cachedTagName == tagName) {
            return &lvl->cachedTagMatrix;
        }
    }

    ent = script_object_to_gentity(scriptObject);
    if (!trap_DObjExists(ent)) {
        Scr_ObjectError(va("entity has no model defined (classname '%s')",
                           SL_ConvertToString(ent->scriptClassname)));
    }

    if (G_DObjGetWorldTagMatrix(ent, Scr_GetString(0),
                                &lvl->cachedTagMatrix) == 0) {
        trap_DObjDumpInfo(ent);
        Scr_ParamError(
            0,
            va("tag '%s' does not exist in model '%s' (or any attached submodels)",
               Scr_GetString(0), G_ModelName(ent->modelIndex)));
    }

    lvl->cachedTagMatrixObject = (int32_t)scriptObject;
    lvl->cachedTagMatrixTime = level.time;
    tagName = Scr_GetConstString(0);
    Scr_SetString(&lvl->cachedTagName, tagName);

    return &lvl->cachedTagMatrix;
}

/* 0x68519 script_method_scriptbuiltin_gettagorigin */
/* VERIFIED_DECOMPILER(0x68519, 78519_FUN_00078519.c, VERIFY-SPATIAL-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void script_method_scriptbuiltin_gettagorigin(uint32_t scriptObject)
{
    DObjSkelMat *matrix = ScriptSpatial_GetTagMatrix(scriptObject);

    Scr_AddVector(matrix->origin);
}

/* 0x6854d script_method_scriptbuiltin_gettagangles */
/* VERIFIED_DECOMPILER(0x6854d, 7854d_FUN_0007854d.c, VERIFY-SPATIAL-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void script_method_scriptbuiltin_gettagangles(uint32_t scriptObject)
{
    vec3_t angles;

    Axis4ToAngles(ScriptSpatial_GetTagMatrix(scriptObject), angles);
    Scr_AddVector(angles);
}

/* 0x68635 ScrCmd_GetEye */
/* VERIFIED_DECOMPILER(0x68635, 78635_script_method_scriptbuiltin_geteye.c, VERIFY-SPATIAL-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ScrCmd_GetEye(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    vec3_t eye;

    eye[0] = ent->currentOrigin[0];
    eye[1] = ent->currentOrigin[1];
    eye[2] = ent->currentOrigin[2] + 40.0f;

    Scr_AddVector(eye);
}

/* 0x68590 ScrCmd_GetStance */
/* VERIFIED_DECOMPILER(0x68590, 78590_script_method_scriptbuiltin_getstance.c, VERIFY-SPATIAL-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void ScrCmd_GetStance(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    if (ent->client == 0) {
        Scr_Error("GetStance is only defined for players.");
    } else if ((ent->client->ps.playerStateFlags & SCRIPT_STANCE_FLAG_PRONE) != 0) {
        Scr_AddConstString(scr_const_prone);
    } else if ((ent->client->ps.playerStateFlags & SCRIPT_STANCE_FLAG_CROUCHED) != 0) {
        Scr_AddConstString(scr_const_crouch);
    } else {
        Scr_AddConstString(scr_const_stand);
    }
}
