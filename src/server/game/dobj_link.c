/*
 * C-like source reconstruction for game.mp.uo.i386.so DObj/link/attach paths.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 *
 * This is part of the compile-checked recovered source tree. RVAs identify the
 * recovered binary functions. Field names are recovered where evidence is
 * strong; unresolved names are tracked in private recovery records.
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "recovered_game.h"
#include "game_globals.h"
#include "level_locals.h"
#include "scr_vm.h"
#include "g_syscalls.h"
#include "game_functions.h"

/* External/helper names are recovered or descriptive labels. */
void Com_Printf(const char *format, ...);
void G_Error(const char *format, ...);
void G_Printf(const char *format, ...);

int G_ModelIndex(const char *modelName);
uint16_t G_GetGameId(gentity_t *ent);
#if !defined(_WIN32)
int strcasecmp(const char *a, const char *b);
#endif

void G_SafeDObjFree(gentity_t *ent);
void G_UpdateTags(gentity_t *ent, qboolean updateBoneIndex);
void G_UpdateClientInfo(gentity_t *ent);
void G_UpdateVehicleTags(gentity_t *ent);
void G_UpdateTagInfoOfChildren(gentity_t *parent,
                                      qboolean updateBoneIndex);
void G_CalcTagAxis(gentity_t *ent, int useLinkedAngles);

/* NOT_FROM_ORIGINAL_SOURCE: local accessor for attach-ignore bit byte at gentity+0x182. */
static uint8_t *game_compat_g_attach_ignore_collision_byte(gentity_t *ent)
{
    return &ent->attachIgnoreCollision;
}

/* NOT_FROM_ORIGINAL_SOURCE: local accessor for attach-ignore bit byte at gentity+0x182. */
static uint8_t game_compat_g_attach_ignore_collision_value(const gentity_t *ent)
{
    return ent->attachIgnoreCollision;
}

void G_SetOrigin(gentity_t *ent, const float *origin);
void G_SetAngle(gentity_t *ent, const float *angles);
void G_GeneralLink(gentity_t *ent);

#define LINKTO_DEFAULT_TAG ""
#define TRIGGER_MULTIPLE_CLASSNAME "trigger_multiple"
#define DOBJ_ATTACH_SLOT_COUNT 6
#define DOBJ_MODEL_COUNT_MAX (DOBJ_ATTACH_SLOT_COUNT + 1)
#define PICK_TARGET_MAX_ENTITIES 32
#define VTOS_RING_COUNT 8
#define VTOS_BUFFER_SIZE 32
#define VTOSF_BUFFER_SIZE 64

static XModel *s_modelIndexXModels[CS_MODELS_COUNT];
static int s_vtosIndex;
static char s_vtosBuffers[VTOS_RING_COUNT][VTOS_BUFFER_SIZE];
static int s_vtosfIndex;
static char s_vtosfBuffers[VTOS_RING_COUNT][VTOSF_BUFFER_SIZE];

/* VERIFIED_DECOMPILER(0x7799c, 8799c_G_FindConfigstringIndex.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - empty-name return, lookup/create loop, not-precached error, overflow check, configstring write, and return index checked against current decompiler output. */
/* 0x7799c G_FindConfigstringIndex */
int G_FindConfigstringIndex(const char *name, int start, int max,
                            qboolean create, const char *fieldname)
{
    int index;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    for (index = 1; index < max; index++) {
        const char *configstring = trap_GetConfigstringConst(start + index);

        if (configstring[0] == '\0') {
            break;
        }

        if (strcasecmp(configstring, name) == 0) {
            return index;
        }
    }

    if (!create) {
        if (fieldname != NULL) {
            Scr_Error(va("%s \"%s\" not precached", fieldname, name));
        }
        return 0;
    }

    if (index == max) {
        G_Error("G_FindConfigstringIndex: overflow");
    }

    trap_SetConfigstring(start + index, name);
    return index;
}

/* VERIFIED_DECOMPILER(0x77b37, 87b37_G_ModelIndex.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - empty model return, model configstring lookup, spawning-only create path, overflow check, XModel cache store, configstring write, and return index checked against current decompiler output. */
/* 0x77b37 G_ModelIndex */
int G_ModelIndex(const char *modelName)
{
    int index;

    if (modelName[0] == '\0') {
        return 0;
    }

    for (index = 1; index < CS_MODELS_COUNT; index++) {
        const char *configstring =
            trap_GetConfigstringConst(CS_MODELS + index);

        if (configstring[0] == '\0') {
            break;
        }

        if (strcasecmp(configstring, modelName) == 0) {
            return index;
        }
    }

    if (level.spawning == 0) {
        Scr_Error(va("model '%s' not precached", modelName));
    }

    if (index == CS_MODELS_COUNT) {
        G_Error("G_ModelIndex: overflow");
    }

    s_modelIndexXModels[index] = trap_XModelGet(modelName);
    trap_SetConfigstring(CS_MODELS + index, modelName);
    return index;
}

/* VERIFIED_DECOMPILER(0x77c2e, 87c2e_FUN_00087c2e.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - indexed cached XModel load checked against current decompiler output. */
/* 0x77c2e G_GetModel: exact original identity supplied by the UO Mac symbol. */
static XModel *G_GetModel(int modelIndex)
{
    return s_modelIndexXModels[modelIndex];
}

/* VERIFIED_DECOMPILER(0x77c4a, 87c4a_G_ModelName.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - model configstring base addition and returned configstring pointer checked against current decompiler output. */
/* 0x77c4a G_ModelName */
const char *G_ModelName(int modelIndex)
{
    return trap_GetConfigstringConst(CS_MODELS + modelIndex);
}

/* VERIFIED_DECOMPILER(0x77d42, 87d42_G_SoundAliasIndex.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - sound-alias configstring lookup arguments and uint8_t return checked against current decompiler output. */
/* 0x77d42 G_SoundAliasIndex */
uint8_t G_SoundAliasIndex(const char *name)
{
    return (uint8_t)G_FindConfigstringIndex(
        name,
        CS_SOUNDS,
        CS_SOUNDS_COUNT,
        qtrue,
        NULL);
}

/* VERIFIED_DECOMPILER(0x77d88, 87d88_G_GetGameId.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - Scr_GetEntityId arguments and uint16_t return checked against current decompiler output. */
/* 0x77d88 G_GetGameId */
uint16_t G_GetGameId(gentity_t *ent)
{
    return Scr_GetEntityId(ent->s.number, 0);
}

/* VERIFIED_DECOMPILER(0x77db8, 87db8_G_UpdateTags.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - vehicle tag update guard and child tag refresh call checked against current decompiler output. */
/* 0x77db8 G_UpdateTags */
void G_UpdateTags(gentity_t *ent, qboolean updateBoneIndex)
{
    if (ent->vehicle != NULL) {
        G_UpdateVehicleTags(ent);
    }

    G_UpdateTagInfoOfChildren(ent, updateBoneIndex);
}

/* NOT_FROM_ORIGINAL_SOURCE: extracted shared print block from script detach failure path. */
static void game_compat_dobj_print_current_attachments(gentity_t *ent)
{
    Com_Printf("Current attachments:\n");

    for (int slot = 0; slot < 6; slot++) {
        if (ent->attachModelIndex[slot] != 0 && ent->attachTagIndex[slot] != 0) {
            Com_Printf("model: '%s', tag: '%s'\n",
                       G_ModelName(ent->attachModelIndex[slot]),
                       SL_ConvertToString(ent->attachTagIndex[slot]));
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: extracted script parameter validation helper. */
static void game_compat_dobj_require_entity_parameter(uint32_t index)
{
    if (Scr_GetType(index) != SCRIPT_VAR_OBJECT ||
        Scr_GetPointerType(index) != SCRIPT_VAR_ENTITY) {
        Scr_ParamError(index, "not an entity");
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: extracted link failure reporting block from script linkto path. */
static void game_compat_dobj_report_link_failure(gentity_t *parent, const char *tagName)
{
    if (!trap_DObjExists(parent)) {
        if (parent->modelIndex == 0) {
            Scr_Error("failed to link entity since parent has no model");
        }

        Scr_Error(va("failed to link entity since parent model '%s' is invalid",
                     G_ModelName(parent->modelIndex)));
    }

    if (tagName[0] != '\0' && trap_DObjGetBoneIndex(parent, tagName) < 0) {
        trap_DObjDumpInfo(parent);
        Scr_Error(va("failed to link entity since tag '%s' does not exist in parent model '%s'",
                     tagName, G_ModelName(parent->modelIndex)));
    }

    Scr_Error("failed to link entity due to link cycle");
}

/* NOT_FROM_ORIGINAL_SOURCE: extracted enableLinkTo eligibility predicate from script path. */
static qboolean game_compat_dobj_can_enable_link_to(gentity_t *ent)
{
    const char *classname;

    if (ent->s.eType != ET_GENERAL || ent->linkedByte16d != 0) {
        return 0;
    }

    if (ent->nextthink == 0 && ent->think == 0) {
        return 1;
    }

    classname = SL_ConvertToString(ent->scriptClassname);
    return strcasecmp(classname, TRIGGER_MULTIPLE_CLASSNAME) == 0;
}

/* VERIFIED_DECOMPILER(0x78075, 88075_G_SetModel.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - empty-name modelIndex clear and G_ModelIndex store checked against current decompiler output. */
/* 0x78075 G_SetModel */
void G_SetModel(gentity_t *ent, const char *modelName)
{
    if (modelName[0] == '\0') {
        ent->modelIndex = 0;
        return;
    }

    ent->modelIndex = G_ModelIndex(modelName);
}

/* VERIFIED_DECOMPILER(0x77df9, 87df9_G_DObjUpdate.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - client/update path, safe free, empty-model tag update, DObjModel fields without reserved_00a stores, type-copy cases, attach loop, trap_DObjCreate arguments, and tag update checked against current decompiler output and disassembly. */
/* 0x77df9 G_DObjUpdate */
void G_DObjUpdate(gentity_t *ent)
{
    DObjModel models[DOBJ_MODEL_COUNT_MAX];
    uint16_t gameId;
    uint32_t modelCount;

    if (ent->client != 0) {
        G_UpdateClientInfo(ent);
        return;
    }

    G_SafeDObjFree(ent);

    if (ent->modelIndex == 0) {
        G_UpdateTags(ent, 0);
        return;
    }

    gameId = G_GetGameId(ent);

    models[0].model = G_GetModel(ent->modelIndex);
    models[0].tagName = 0;
    models[0].modelIndex = -(int16_t)ent->modelIndex;
    models[0].ignoreCollision = 0;
    modelCount = 1;

    switch (ent->s.eType) {
    case ET_GENERAL:
    case ET_SCRIPTMOVER:
    case ET_VEHICLE:
    case ET_TURRET:
    case ET_VEHICLE_CORPSE:
    case ET_VEHICLE_COLLMAP:
        ent->s.dobjModelIndex = ent->modelIndex;
        break;
    default:
        break;
    }

    for (int slot = 0; slot < DOBJ_ATTACH_SLOT_COUNT; slot++) {
        uint8_t modelIndex = ent->attachModelIndex[slot];

        if (modelIndex == 0) {
            continue;
        }

        models[modelCount].model = G_GetModel(modelIndex);
        models[modelCount].tagName = SL_ConvertToString(ent->attachTagIndex[slot]);
        models[modelCount].modelIndex = -(int16_t)modelIndex;
        models[modelCount].ignoreCollision =
            (game_compat_g_attach_ignore_collision_value(ent) >> slot) & 1;
        modelCount++;
    }

    trap_DObjCreate(models, (uint16_t)modelCount, 0, ent->s.number, gameId);
    G_UpdateTags(ent, 1);
}

/* VERIFIED_DECOMPILER(0x780bc, 880bc_G_EntAttach.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - free slot search, full-slot return, model/tag stores, ignore-collision bit set, DObj update, and return checked against current decompiler output. */
/* 0x780bc G_EntAttach */
qboolean G_EntAttach(gentity_t *ent, const char *modelName,
                     const char *tagName, qboolean ignoreCollision)
{
    int slot;

    for (slot = 0; slot < DOBJ_ATTACH_SLOT_COUNT; slot++) {
        if (ent->attachModelIndex[slot] == 0) {
            break;
        }
    }

    if (slot >= DOBJ_ATTACH_SLOT_COUNT) {
        return 0;
    }

    ent->attachModelIndex[slot] = G_ModelIndex(modelName);
    ent->attachTagIndex[slot] = SL_GetLowercaseString(tagName, 0);

    if (ignoreCollision) {
        (*game_compat_g_attach_ignore_collision_byte(ent)) |= (uint8_t)(1u << slot);
    }

    G_DObjUpdate(ent);
    return 1;
}

/* VERIFIED_DECOMPILER(0x7818a, 8818a_G_EntDetach.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - find-only tag lookup, slot/model match, string clear, compaction, ignore bit propagation, tail clear, DObj update, and return paths checked against current decompiler output. */
/* 0x7818a G_EntDetach */
qboolean G_EntDetach(gentity_t *ent, const char *modelName, const char *tagName)
{
    uint16_t tagStringId = SL_FindLowercaseString(tagName);

    if (tagStringId == 0) {
        return 0;
    }

    for (int slot = 0; slot < DOBJ_ATTACH_SLOT_COUNT; slot++) {
        const char *attachedModelName;

        if (ent->attachTagIndex[slot] != tagStringId) {
            continue;
        }

        attachedModelName = G_ModelName(ent->attachModelIndex[slot]);
        if (strcasecmp(attachedModelName, modelName) != 0) {
            continue;
        }

        ent->attachModelIndex[slot] = 0;
        Scr_SetString(&ent->attachTagIndex[slot], 0);

        for (; slot < DOBJ_ATTACH_SLOT_COUNT - 1; slot++) {
            uint8_t nextIgnoreCollision =
                (game_compat_g_attach_ignore_collision_value(ent) >> (slot + 1)) & 1;

            ent->attachModelIndex[slot] = ent->attachModelIndex[slot + 1];
            ent->attachTagIndex[slot] = ent->attachTagIndex[slot + 1];

            if (nextIgnoreCollision) {
                (*game_compat_g_attach_ignore_collision_byte(ent)) |= (uint8_t)(1u << slot);
            } else {
                (*game_compat_g_attach_ignore_collision_byte(ent)) &= (uint8_t)~(1u << slot);
            }
        }

        ent->attachModelIndex[DOBJ_ATTACH_SLOT_COUNT - 1] = 0;
        ent->attachTagIndex[DOBJ_ATTACH_SLOT_COUNT - 1] = 0;
        (*game_compat_g_attach_ignore_collision_byte(ent)) &=
            (uint8_t)~(1u << (DOBJ_ATTACH_SLOT_COUNT - 1));

        G_DObjUpdate(ent);
        return 1;
    }

    return 0;
}

/* VERIFIED_DECOMPILER(0x78358, 88358_G_EntDetachAll.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - six attach slots, tag string clears, ignore bitset clear, and DObj update checked against current decompiler output. */
/* 0x78358 G_EntDetachAll */
void G_EntDetachAll(gentity_t *ent)
{
    for (int slot = 0; slot < DOBJ_ATTACH_SLOT_COUNT; slot++) {
        ent->attachModelIndex[slot] = 0;
        Scr_SetString(&ent->attachTagIndex[slot], 0);
    }

    (*game_compat_g_attach_ignore_collision_byte(ent)) = 0;
    G_DObjUpdate(ent);
}

/* VERIFIED_DECOMPILER(0x783c7, 883c7_FUN_000883c7.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - pre-unlink, tag validation, parent-chain cycle checks, linkInfo allocation/layout, child insertion, axis zeroing, and return paths checked against current decompiler output. */
/* 0x783c7 G_EntLinkToInternal: exact original identity supplied by the UO Mac symbol. */
static qboolean G_EntLinkToInternal(gentity_t *child, gentity_t *parent,
                                    const char *tagName)
{
    int parentTagIndex;
    gentity_t *ancestor;
    entityLinkInfo_t *linkInfo;

    G_EntUnlink(child);

    if (tagName[0] == '\0') {
        parentTagIndex = -1;
    } else {
        if (!trap_DObjExists(parent)) {
            return qfalse;
        }

        parentTagIndex = trap_DObjGetBoneIndex(parent, tagName);
        if (parentTagIndex < 0) {
            return qfalse;
        }
    }

    for (ancestor = parent; ancestor->linkInfo != NULL;
         ancestor = ancestor->linkInfo->parent) {
        if (ancestor == child) {
            return qfalse;
        }
    }
    if (ancestor == child) {
        return qfalse;
    }

    linkInfo = MT_Alloc(sizeof(*linkInfo), 0x10);
    linkInfo->parent = parent;
    linkInfo->tagStringId =
        tagName[0] == '\0' ? 0 : SL_GetLowercaseString(tagName, 0);
    linkInfo->nextChild = parent->firstChild;
    linkInfo->parentTagIndex = parentTagIndex;
    memset(&linkInfo->relAxis, 0, sizeof(linkInfo->relAxis));
    parent->firstChild = child;
    child->linkInfo = linkInfo;
    memset(&linkInfo->parentRelAxis, 0, sizeof(linkInfo->parentRelAxis));

    return qtrue;
}

/* VERIFIED_DECOMPILER(0x78540, 88540_G_EntLinkTo.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - create-link guard, G_CalcTagAxis argument, and return paths checked against current decompiler output. */
/* 0x78540 G_EntLinkTo */
qboolean G_EntLinkTo(gentity_t *child, gentity_t *parent, const char *tagName)
{
    if (!G_EntLinkToInternal(child, parent, tagName)) {
        return qfalse;
    }

    G_CalcTagAxis(child, 0);
    return qtrue;
}

/* VERIFIED_DECOMPILER(0x7859b, 8859b_G_EntLinkToWithOffset.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - create-link guard, AnglesToAxis target, origin-offset stores, and return paths checked against current decompiler output. */
/* 0x7859b G_EntLinkToWithOffset */
qboolean G_EntLinkToWithOffset(gentity_t *child, gentity_t *parent,
                               const char *tagName,
                               const float *originOffset,
                               const float *anglesOffset)
{
    entityLinkInfo_t *linkInfo;

    if (!G_EntLinkToInternal(child, parent, tagName)) {
        return qfalse;
    }

    linkInfo = child->linkInfo;
    AnglesToAxis(anglesOffset, linkInfo->relAxis.axis);
    linkInfo->relAxis.origin[0] = originOffset[0];
    linkInfo->relAxis.origin[1] = originOffset[1];
    linkInfo->relAxis.origin[2] = originOffset[2];

    return qtrue;
}

/* VERIFIED_DECOMPILER(0x7862b, 8862b_G_EntUnlink.c, VERIFY-P1-DOBJLINK-2026-06-17): DATAFLOW_VERIFIED - null guard, origin/angle stabilization, sibling-list removal, link clear, tag string free, and MT_Free size checked against current decompiler output. */
/* 0x7862b G_EntUnlink */
void G_EntUnlink(gentity_t *ent)
{
    entityLinkInfo_t *linkInfo = ent->linkInfo;
    gentity_t *previousChild;
    gentity_t *child;

    if (linkInfo == NULL) {
        return;
    }

    G_SetOrigin(ent, ent->currentOrigin);
    G_SetAngle(ent, ent->currentAngles);

    previousChild = NULL;
    for (child = linkInfo->parent->firstChild; child != ent;
         child = child->linkInfo->nextChild) {
        previousChild = child;
    }

    if (previousChild == NULL) {
        linkInfo->parent->firstChild = linkInfo->nextChild;
    } else {
        previousChild->linkInfo->nextChild = linkInfo->nextChild;
    }

    ent->linkInfo = NULL;
    Scr_SetString(&linkInfo->tagStringId, 0);
    MT_Free(linkInfo, sizeof(*linkInfo));
}

/* VERIFIED_DECOMPILER(0x78723, 88723_G_EntIsLinkedTo.c, VERIFY-DOBJLINK-PACKET-2026-06-17): DATAFLOW_VERIFIED - direct linkInfo parent test and qboolean return checked against current decompiler output. */
/* 0x78723 G_EntIsLinkedTo */
qboolean G_EntIsLinkedTo(gentity_t *child, gentity_t *parent)
{
    qboolean isLinked = qfalse;

    if (child->linkInfo != NULL && child->linkInfo->parent == parent) {
        isLinked = qtrue;
    }

    return isLinked;
}

/* VERIFIED_DECOMPILER(0x78758, 88758_G_UpdateTagInfo.c, VERIFY-DOBJLINK-PACKET-2026-06-17): DATAFLOW_VERIFIED - empty tag parentTagIndex reset, optional DObj bone refresh, negative-index unlink path, and return behavior checked against current decompiler output. */
/* 0x78758 G_UpdateTagInfo */
void G_UpdateTagInfo(gentity_t *ent, qboolean updateBoneIndex)
{
    entityLinkInfo_t *linkInfo = ent->linkInfo;

    if (linkInfo->tagStringId == 0) {
        linkInfo->parentTagIndex = -1;
    } else {
        if (updateBoneIndex) {
            linkInfo->parentTagIndex =
                trap_DObjGetBoneIndex(linkInfo->parent,
                                      SL_ConvertToString(linkInfo->tagStringId));
            if (linkInfo->parentTagIndex >= 0) {
                return;
            }
        }

        G_EntUnlink(ent);
    }
}

/* VERIFIED_DECOMPILER(0x787d6, 887d6_G_UpdateTagInfoOfChildren.c, VERIFY-DOBJLINK-PACKET-2026-06-17): DATAFLOW_VERIFIED - firstChild traversal, nextChild saved before possible unlink, update call arguments, and loop exit checked against current decompiler output. */
/* 0x787d6 G_UpdateTagInfoOfChildren */
void G_UpdateTagInfoOfChildren(gentity_t *parent, qboolean updateBoneIndex)
{
    gentity_t *child = parent->firstChild;

    while (child != NULL) {
        gentity_t *nextChild = child->linkInfo->nextChild;

        G_UpdateTagInfo(child, updateBoneIndex);
        child = nextChild;
    }
}

/* VERIFIED_DECOMPILER(0x7882b, 8882b_G_CalcTagParentAxis.c, VERIFY-DOBJLINK-PACKET-2026-06-17): DATAFLOW_VERIFIED - untagged parent axis path, tagged parent axis construction, bone calc, matrix-array offset, and DObjSkelMatrixMultiply43 arguments checked against current decompiler output. */
/* 0x7882b G_CalcTagParentAxis */
void G_CalcTagParentAxis(gentity_t *child, matrix43_t *outAxis)
{
    entityLinkInfo_t *linkInfo = child->linkInfo;
    gentity_t *parent = linkInfo->parent;

    if (linkInfo->parentTagIndex < 0) {
        AnglesToAxis(parent->currentAngles, outAxis->axis);
        outAxis->origin[0] = parent->currentOrigin[0];
        outAxis->origin[1] = parent->currentOrigin[1];
        outAxis->origin[2] = parent->currentOrigin[2];
    } else {
        matrix43_t parentAxis;

        AnglesToAxis(parent->currentAngles, parentAxis.axis);
        parentAxis.origin[0] = parent->currentOrigin[0];
        parentAxis.origin[1] = parent->currentOrigin[1];
        parentAxis.origin[2] = parent->currentOrigin[2];

        if ((uint32_t)linkInfo->parentTagIndex >=
            (uint32_t)trap_DObjNumBones(parent)) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            *outAxis = parentAxis;
            return;
        }

        G_DObjCalcBone(parent, linkInfo->parentTagIndex);
        float *matrixArray = trap_DObjGetMatrixArray(parent);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (matrixArray == NULL) {
            *outAxis = parentAxis;
            return;
        }

        DObjSkelMatrixMultiply43(
            (const DObjSkelMat *)(const void *)&matrixArray[
                linkInfo->parentTagIndex * 16],
            &parentAxis, outAxis);
    }
}

/* VERIFIED_DECOMPILER(0x78930, 88930_G_CalcTagParentRelAxis.c, VERIFY-DOBJLINK-PACKET-2026-06-17): DATAFLOW_VERIFIED - parent axis calculation, parentRelAxis offset, and MatrixMultiply43 argument order checked against current decompiler output. */
/* 0x78930 G_CalcTagParentRelAxis */
void G_CalcTagParentRelAxis(gentity_t *child, matrix43_t *outAxis)
{
    matrix43_t parentAxis;
    entityLinkInfo_t *linkInfo = child->linkInfo;

    G_CalcTagParentAxis(child, &parentAxis);
    MatrixMultiply43(&linkInfo->parentRelAxis, &parentAxis, outAxis);
}

/* VERIFIED_DECOMPILER(0x78982, 88982_G_CalcTagAxis.c, VERIFY-DOBJLINK-PACKET-2026-06-17): DATAFLOW_VERIFIED - parent axis, local entity axis, origin stores on affine path, inverse/transpose branch, and relAxis call targets checked against current decompiler output. */
/* 0x78982 G_CalcTagAxis */
void G_CalcTagAxis(gentity_t *ent, int useLinkedAngles)
{
    matrix43_t inverseParentAxis;
    matrix43_t parentAxis;
    matrix43_t localAxis;
    entityLinkInfo_t *linkInfo;

    G_CalcTagParentAxis(ent, &parentAxis);
    AnglesToAxis(ent->currentAngles, localAxis.axis);
    linkInfo = ent->linkInfo;

    if (!useLinkedAngles) {
        MatrixInverseOrthogonal43(&parentAxis, &inverseParentAxis);
        localAxis.origin[0] = ent->currentOrigin[0];
        localAxis.origin[1] = ent->currentOrigin[1];
        localAxis.origin[2] = ent->currentOrigin[2];
        MatrixMultiply43(&localAxis, &inverseParentAxis,
                         &linkInfo->relAxis);
    } else {
        /* C99 cannot add element qualification through the axis_t array
         * typedef implicitly; MatrixTranspose retains a read-only input. */
        MatrixTranspose((const vec_t (*)[3])parentAxis.axis,
                        inverseParentAxis.axis);
        MatrixMultiply(localAxis.axis,
                       inverseParentAxis.axis,
                       linkInfo->relAxis.axis);
    }
}

/* VERIFIED_DECOMPILER(0x78a69, 88a69_G_SetFixedLink.c, VERIFY-DOBJLINK-PACKET-2026-06-17): DATAFLOW_VERIFIED - fixed-link mode branches 1/0/2, relAxis composition, origin/angle/yaw writes, and MatrixTransformVector43 temp-store behavior checked against current decompiler output. */
/* 0x78a69 G_SetFixedLink */
void G_SetFixedLink(gentity_t *ent, int mode)
{
    matrix43_t parentAxis;
    matrix43_t linkedAxis;
    entityLinkInfo_t *linkInfo;

    G_CalcTagParentAxis(ent, &parentAxis);
    linkInfo = ent->linkInfo;

    if (mode == 1) {
        MatrixMultiply43(&linkInfo->relAxis, &parentAxis, &linkedAxis);
        ent->currentOrigin[0] = linkedAxis.origin[0];
        ent->currentOrigin[1] = linkedAxis.origin[1];
        ent->currentOrigin[2] = linkedAxis.origin[2];
        ent->currentAngles[1] = vectoyaw(linkedAxis.axis[0]);
    } else if (mode < 2) {
        if (mode == 0) {
            MatrixMultiply43(&linkInfo->relAxis, &parentAxis, &linkedAxis);
            ent->currentOrigin[0] = linkedAxis.origin[0];
            ent->currentOrigin[1] = linkedAxis.origin[1];
            ent->currentOrigin[2] = linkedAxis.origin[2];
            /* C99 multidimensional-array qualifier bridge; the input axis is
             * not modified by AxisToAngles. */
            AxisToAngles((const vec_t (*)[3])linkedAxis.axis,
                         ent->currentAngles);
        }
    } else if (mode == 2) {
        MatrixTransformVector43(linkInfo->relAxis.origin,
                                &parentAxis, linkedAxis.origin);
        ent->currentOrigin[0] = linkedAxis.origin[0];
        ent->currentOrigin[1] = linkedAxis.origin[1];
        ent->currentOrigin[2] = linkedAxis.origin[2];
    }
}

/* VERIFIED_DECOMPILER(0x78c40, 88c40_Think_GeneralLink.c, VERIFY-DOBJLINK-PACKET-2026-06-17): DATAFLOW_VERIFIED - nextthink level.time+50 store, linkInfo guard, and G_GeneralLink argument checked against current decompiler output. */
/* 0x78c40 Think_GeneralLink */
void Think_GeneralLink(gentity_t *ent)
{
    ent->nextthink = level.time + 50;

    if (ent->linkInfo != NULL) {
        G_GeneralLink(ent);
    }
}

/* VERIFIED_DECOMPILER(0x78c87, 88c87_G_SafeDObjFree.c, VERIFY-DOBJ-POSE-PACKET-2026-06-17): DATAFLOW_VERIFIED - entity number argument, notify false constant, and no side effects beyond trap_SafeDObjFree checked against current decompiler output. */
/* 0x78c87 G_SafeDObjFree */
void G_SafeDObjFree(gentity_t *ent)
{
    trap_SafeDObjFree(ent->s.number, qfalse);
}

/* VERIFIED_DECOMPILER(0x78cb4, 88cb4_G_DObjUpdateServerTime.c, VERIFY-DOBJ-POSE-PACKET-2026-06-17): DATAFLOW_VERIFIED - ent, frameTime*0.001, and notify argument order checked against current decompiler output; int return is retained because the decompiled G_XAnimUpdateEnt caller tests this wrapper result. */
/* 0x78cb4 G_DObjUpdateServerTime */
int G_DObjUpdateServerTime(gentity_t *ent, int notify)
{
    /* 0x78cd3: stock fild keeps frameTime exact in the 80-bit register
     * through the multiply, then rounds once for the float argument;
     * casting frameTime to float before the multiply would add an
     * intermediate rounding under -std=c99. */
    return trap_DObjUpdateServerTime(
        ent,
        (float)((long double)level.frameTime * 0.001f),
        notify);
}

/* VERIFIED_DECOMPILER(0x78cf6, 88cf6_FUN_00088cf6.c, VERIFY-DOBJ-POSE-PACKET-2026-06-17): DATAFLOW_VERIFIED - rot/trans array index scaling, yaw/pitch/roll quaternion order, quaternion multiplies, accumulatedWeight store, and translation[0..2] stores checked against current decompiler output and DObjAnimMat layout. */
/* 0x78cf6 G_DObjSetLocalTagInternal */
static void G_DObjSetLocalTagInternal(gentity_t *ent, const float *origin,
                                      const float *angles, int boneIndex)
{
    float yawQuat[4];
    float pitchQuat[4];
    float rollQuat[4];
    float pitchYawQuat[4];
    DObjAnimMat *rotTrans = &trap_DObjGetRotTransArray(ent)[boneIndex];

    YawToQuaternion(angles[1], yawQuat);
    PitchToQuaternion(angles[0], pitchQuat);
    RollToQuaternion(angles[2], rollQuat);
    QuatMultiply(pitchQuat, yawQuat, pitchYawQuat);
    QuatMultiply(rollQuat, pitchYawQuat, rotTrans->quat);
    rotTrans->accumulatedWeight = 0.0f;
    rotTrans->translation[0] = origin[0];
    rotTrans->translation[1] = origin[1];
    rotTrans->translation[2] = origin[2];
}

/* VERIFIED_DECOMPILER(0x78dcc, 88dcc_G_DObjSetLocalBoneIndex.c, VERIFY-DOBJ-POSE-PACKET-2026-06-17): DATAFLOW_VERIFIED - rot/trans index gate, helper call argument order, and qboolean result checked against current decompiler output. */
/* 0x78dcc G_DObjSetLocalBoneIndex */
qboolean G_DObjSetLocalBoneIndex(gentity_t *ent, uint32_t *partBits,
                                 int boneIndex, const float *origin,
                                 const float *angles)
{
    if (!trap_DObjSetRotTransIndex(ent, partBits, boneIndex)) {
        return qfalse;
    }

    G_DObjSetLocalTagInternal(ent, origin, angles, boneIndex);
    return qtrue;
}

/* VERIFIED_DECOMPILER(0x78e34, 88e34_G_DObjSetLocalTag.c, VERIFY-DOBJ-POSE-PACKET-2026-06-17): DATAFLOW_VERIFIED - tag bone lookup, negative-index false path, rot/trans index gate, helper call argument order, and qboolean result checked against current decompiler output. */
/* 0x78e34 G_DObjSetLocalTag */
qboolean G_DObjSetLocalTag(gentity_t *ent, uint32_t *partBits,
                           const char *tagName, const float *origin,
                           const float *angles)
{
    int boneIndex = trap_DObjGetBoneIndex(ent, tagName);

    if (boneIndex < 0 ||
        !trap_DObjSetRotTransIndex(ent, partBits, boneIndex)) {
        return qfalse;
    }

    G_DObjSetLocalTagInternal(ent, origin, angles, boneIndex);
    return qtrue;
}

/* VERIFIED_DECOMPILER(0x78ec0, 88ec0_G_DObjSetControlTagAngles.c, VERIFY-DOBJ-POSE-PACKET-2026-06-17): DATAFLOW_VERIFIED - tag bone lookup, negative-index false path, control rot/trans gate, vec3_origin helper argument, and qboolean result checked against current decompiler output. */
/* 0x78ec0 G_DObjSetControlTagAngles */
qboolean G_DObjSetControlTagAngles(gentity_t *ent, uint32_t *partBits,
                                   const char *tagName, const float *angles)
{
    int boneIndex = trap_DObjGetBoneIndex(ent, tagName);

    if (boneIndex < 0 ||
        !trap_DObjSetControlRotTransIndex(ent, partBits, boneIndex)) {
        return qfalse;
    }

    G_DObjSetLocalTagInternal(ent, vec3_origin, angles, boneIndex);
    return qtrue;
}

/* VERIFIED_DECOMPILER(0x78f4f, 88f4f_G_DObjCalcPose.c, VERIFY-DOBJ-POSE-PACKET-2026-06-17): DATAFLOW_VERIFIED - 0xff part-bits initialization, create-skeleton false path, calc-anim/controller/calc-skel order, and controller offset 0x230 checked against current decompiler output. */
/* 0x78f4f G_DObjCalcPose */
void G_DObjCalcPose(gentity_t *ent)
{
    uint32_t partBits[4];

    memset(partBits, 0xff, sizeof(partBits));
    if (!trap_DObjCreateSkelForBones(ent, partBits)) {
        trap_DObjCalcAnim(ent, partBits);
        if (ent->controller != NULL) {
            ent->controller(ent, partBits);
        }
        trap_DObjCalcSkel(ent, partBits);
    }
}

/* VERIFIED_DECOMPILER(0x78fe2, 88fe2_G_DObjCalcBone.c, VERIFY-DOBJ-POSE-PACKET-2026-06-17): DATAFLOW_VERIFIED - single-bone create-skeleton false path, hierarchy bit fetch, calc-anim/controller/calc-skel order, and controller offset 0x230 checked against current decompiler output. */
/* 0x78fe2 G_DObjCalcBone */
void G_DObjCalcBone(gentity_t *ent, int boneIndex)
{
    uint32_t partBits[4];

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)boneIndex >= (uint32_t)trap_DObjNumBones(ent)) {
        return;
    }

    if (!trap_DObjCreateSkelForBone(ent, boneIndex)) {
        trap_DObjGetHierarchyBits(ent, boneIndex, partBits);
        trap_DObjCalcAnim(ent, partBits);
        if (ent->controller != NULL) {
            ent->controller(ent, partBits);
        }
        trap_DObjCalcSkel(ent, partBits);
    }
}

/* VERIFIED_DECOMPILER(0x79073, 89073_G_DObjGetLocalBoneIndexMatrix.c, VERIFY-DOBJ-LOOKUP-TARGET-2026-06-17): DATAFLOW_VERIFIED - bone calc call and matrix-array return offset boneIndex*0x40 checked against current decompiler output. */
/* 0x79073 G_DObjGetLocalBoneIndexMatrix */
/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
DObjSkelMat *G_DObjGetLocalBoneIndexMatrix(gentity_t *ent, int boneIndex)
{
    if ((uint32_t)boneIndex >= (uint32_t)trap_DObjNumBones(ent)) {
        return NULL;
    }

    G_DObjCalcBone(ent, boneIndex);
    float *matrixArray = trap_DObjGetMatrixArray(ent);
    if (matrixArray == NULL) {
        return NULL;
    }
    return (DObjSkelMat *)(void *)&matrixArray[boneIndex * 16];
}

/* VERIFIED_DECOMPILER(0x790b4, 890b4_G_DObjGetWorldBoneIndexMatrix.c, VERIFY-DOBJ-LOOKUP-TARGET-2026-06-17): DATAFLOW_VERIFIED - local bone matrix call, currentAngles/currentOrigin axis construction, and DObjSkel2MatrixMultiply43 argument order checked against current decompiler output. */
/* 0x790b4 G_DObjGetWorldBoneIndexMatrix */
void G_DObjGetWorldBoneIndexMatrix(gentity_t *ent, int boneIndex,
                                   DObjSkelMat *outMatrix)
{
    matrix43_t entityAxis;
    const DObjSkelMat *localMatrix =
        G_DObjGetLocalBoneIndexMatrix(ent, boneIndex);

    AnglesToAxis(ent->currentAngles, entityAxis.axis);
    entityAxis.origin[0] = ent->currentOrigin[0];
    entityAxis.origin[1] = ent->currentOrigin[1];
    entityAxis.origin[2] = ent->currentOrigin[2];
    if (localMatrix == NULL) {
        memset(outMatrix, 0, sizeof(*outMatrix));
        for (int32_t row = 0; row < 3; ++row) {
            for (int32_t column = 0; column < 3; ++column) {
                outMatrix->axis[row][column] =
                    entityAxis.axis[row][column];
            }
            outMatrix->origin[row] = entityAxis.origin[row];
        }
        return;
    }
    /* Windows identity: uo_game_mp_x86.dll composes here through the padded
     * body at 0x20017b80 (its only two callers are this function pair) —
     * bone-first, matching the Linux module.  See DObjSkel2MatrixMultiply43. */
    DObjSkel2MatrixMultiply43(localMatrix, &entityAxis, outMatrix);
}

/* VERIFIED_DECOMPILER(0x79135, 89135_G_DObjGetLocalTagMatrix.c, VERIFY-DOBJ-LOOKUP-TARGET-2026-06-17): DATAFLOW_VERIFIED - tag bone lookup, negative-index NULL path, bone calc, and matrix-array return offset boneIndex*0x40 checked against current decompiler output. */
/* 0x79135 G_DObjGetLocalTagMatrix */
DObjSkelMat *G_DObjGetLocalTagMatrix(gentity_t *ent, const char *tagName)
{
    int boneIndex = trap_DObjGetBoneIndex(ent, tagName);

    if ((uint32_t)boneIndex >= (uint32_t)trap_DObjNumBones(ent)) {
        return NULL;
    }

    G_DObjCalcBone(ent, boneIndex);
    float *matrixArray = trap_DObjGetMatrixArray(ent);
    if (matrixArray == NULL) {
        return NULL;
    }
    return (DObjSkelMat *)(void *)&matrixArray[boneIndex * 16];
}

/* VERIFIED_DECOMPILER(0x791a0, 891a0_G_DObjGetWorldTagMatrix.c, VERIFY-DOBJ-LOOKUP-TARGET-2026-06-17): DATAFLOW_VERIFIED - local tag matrix truth return, conditional axis construction, and DObjSkel2MatrixMultiply43 argument order checked against current decompiler output. */
/* 0x791a0 G_DObjGetWorldTagMatrix */
qboolean G_DObjGetWorldTagMatrix(gentity_t *ent, const char *tagName,
                                 DObjSkelMat *outMatrix)
{
    matrix43_t entityAxis;
    const DObjSkelMat *localMatrix = G_DObjGetLocalTagMatrix(ent, tagName);

    if (localMatrix == NULL) {
        return qfalse;
    }

    AnglesToAxis(ent->currentAngles, entityAxis.axis);
    entityAxis.origin[0] = ent->currentOrigin[0];
    entityAxis.origin[1] = ent->currentOrigin[1];
    entityAxis.origin[2] = ent->currentOrigin[2];
    /* Windows identity: uo_game_mp_x86.dll composes here through the padded
     * body at 0x20017b80 (its only two callers are this function pair) —
     * bone-first, matching the Linux module.  See DObjSkel2MatrixMultiply43. */
    DObjSkel2MatrixMultiply43(localMatrix, &entityAxis, outMatrix);
    return qtrue;
}

/* VERIFIED_DECOMPILER(0x7923a, 8923a_G_Find.c, VERIFY-DOBJ-LOOKUP-TARGET-2026-06-17): DATAFLOW_VERIFIED - NULL/start-after-from behavior, entity stride via typed accessor, linked byte gate, 16-bit field load, nonzero match, and NULL/end return checked against current decompiler output. */
/* 0x7923a G_Find */
gentity_t *G_Find(gentity_t *from, size_t fieldOffset, uint16_t match)
{
    int startIndex = 0;

    if (from != NULL) {
        startIndex = (int)(from - g_entities) + 1;
    }

    for (int index = startIndex; index < level.num_entities; index++) {
        gentity_t *ent = &g_entities[index];
        uint16_t value = *(uint16_t *)(void *)&((uint8_t *)ent)[fieldOffset];

        if (ent->linked != 0 && value != 0 && value == match) {
            return ent;
        }
    }

    return NULL;
}

/* VERIFIED_DECOMPILER(0x792db, 892db_G_FindStr.c, VERIFY-DOBJ-LOOKUP-TARGET-2026-06-17): DATAFLOW_VERIFIED - NULL/start-after-from behavior, entity stride via typed accessor, linked byte gate, pointer field load, Q_stricmp arguments, and NULL/end return checked against current decompiler output. */
/* 0x792db G_FindStr */
gentity_t *G_FindStr(gentity_t *from, size_t fieldOffset, const char *match)
{
    int startIndex = 0;

    if (from != NULL) {
        startIndex = (int)(from - g_entities) + 1;
    }

    for (int index = startIndex; index < level.num_entities; index++) {
        gentity_t *ent = &g_entities[index];
        const char *value =
            *(const char **)(void *)&((uint8_t *)ent)[fieldOffset];

        if (ent->linked != 0 && value != NULL && Q_stricmp(value, match) == 0) {
            return ent;
        }
    }

    return NULL;
}

/* VERIFIED_DECOMPILER(0x7937f, 8937f_G_PickTarget.c, VERIFY-DOBJ-LOOKUP-TARGET-2026-06-17): DATAFLOW_VERIFIED - zero-target return, G_Find targetname offset 0x1e6, 32-choice cap, not-found print, SL_ConvertToString argument, and rand modulo choice checked against current decompiler output. */
/* 0x7937f G_PickTarget */
gentity_t *G_PickTarget(uint16_t targetname)
{
    gentity_t *choices[PICK_TARGET_MAX_ENTITIES];
    gentity_t *ent = NULL;
    int count = 0;

    if (targetname == 0) {
        return NULL;
    }

    while (count < PICK_TARGET_MAX_ENTITIES) {
        ent = G_Find(ent, offsetof(gentity_t, targetname), targetname);
        if (ent == NULL) {
            break;
        }

        choices[count++] = ent;
    }

    if (count == 0) {
        G_Printf("G_PickTarget: target %s not found\n",
                 SL_ConvertToString(targetname));
        return NULL;
    }

    return choices[coduo_server_randrange(0, count)];
}

/* VERIFIED_DECOMPILER(0x79467, 89467_vtos.c, VERIFY-DOBJ-LOOKUP-TARGET-2026-06-17): DATAFLOW_VERIFIED - 8-entry/32-byte ring buffer, post-increment mask, Com_sprintf format/size, component order, x87 truncate casts, and returned buffer checked against current decompiler output. */
/* 0x79467 vtos */
const char *vtos(const float *value)
{
    char *buffer = s_vtosBuffers[s_vtosIndex];

    s_vtosIndex = (s_vtosIndex + 1) & (VTOS_RING_COUNT - 1);
    Com_sprintf(buffer, VTOS_BUFFER_SIZE, "(%i %i %i)",
                (int)game_compat_int32_from_float_trunc(value[0]),
                (int)game_compat_int32_from_float_trunc(value[1]),
                (int)game_compat_int32_from_float_trunc(value[2]));
    return buffer;
}

/* VERIFIED_DECOMPILER(0x79507, 89507_vtosf.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - 8-entry/64-byte ring buffer, post-increment mask, Com_sprintf format/size, double-promoted float components, and returned buffer checked against current decompiler output. */
/* 0x79507 vtosf */
const char *vtosf(const float *value)
{
    char *buffer = s_vtosfBuffers[s_vtosfIndex];

    s_vtosfIndex = (s_vtosfIndex + 1) & (VTOS_RING_COUNT - 1);
    Com_sprintf(buffer, VTOSF_BUFFER_SIZE, "(%f %f %f)",
                (double)value[0],
                (double)value[1],
                (double)value[2]);
    return buffer;
}

/* VERIFIED_DECOMPILER(0x79586, 89586_G_SetMovedir.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - special up/down angle sentinels, movedir stores, AngleVectors fallback arguments, and input angle zeroing order checked against current decompiler output. */
/* 0x79586 G_SetMovedir */
void G_SetMovedir(float *angles, float *movedir)
{
    static const vec3_t vecUp = {0.0f, -1.0f, 0.0f};
    static const vec3_t vecDown = {0.0f, -2.0f, 0.0f};

    if (angles[0] == vecUp[0] && angles[1] == vecUp[1] &&
        angles[2] == vecUp[2]) {
        movedir[0] = 0.0f;
        movedir[1] = 0.0f;
        movedir[2] = 1.0f;
    } else if (angles[0] == vecDown[0] && angles[1] == vecDown[1] &&
               angles[2] == vecDown[2]) {
        movedir[0] = 0.0f;
        movedir[1] = 0.0f;
        movedir[2] = -1.0f;
    } else {
        AngleVectors(angles, movedir, NULL, NULL);
    }

    angles[0] = 0.0f;
    angles[1] = 0.0f;
    angles[2] = 0.0f;
}

/* VERIFIED_DECOMPILER(0x67bb6, 77bb6_script_method_scriptbuiltin_attach.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - script object lookup, model/tag/default args, ignore-collision default, pre-detach duplicate error, attach call, and max-attachments error checked against current decompiler output. */
/* 0x67bb6 ScrCmd_attach */
void ScrCmd_attach(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    const char *modelName = Scr_GetString(0);
    const char *tagName = "";
    qboolean ignoreCollision = 0;

    if (Scr_GetNumParam() >= 2) {
        tagName = Scr_GetString(1);
    }

    if (Scr_GetNumParam() >= 3) {
        ignoreCollision = Scr_GetBool(2);
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (G_EntDetach(ent, modelName, tagName)) {
        Scr_Error(va("model '%s' already attached to tag '%s'", modelName, tagName));
    }

    if (!G_EntAttach(ent, modelName, tagName, ignoreCollision)) {
        Scr_Error("maximum attached models exceeded");
    }
}

/* VERIFIED_DECOMPILER(0x67cb0, 77cb0_script_method_scriptbuiltin_detach.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - script object lookup, model/tag/default args, detach failure attachment dump loop, model/tag print argument order, and final Scr_Error checked against current decompiler output. */
/* 0x67cb0 ScrCmd_detach */
void ScrCmd_detach(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    const char *modelName = Scr_GetString(0);
    const char *tagName = "";

    if (Scr_GetNumParam() >= 2) {
        tagName = Scr_GetString(1);
    }

    if (!G_EntDetach(ent, modelName, tagName)) {
        game_compat_dobj_print_current_attachments(ent);
        Scr_Error(va("failed to detach model '%s' from tag '%s'", modelName, tagName));
    }
}

/* VERIFIED_DECOMPILER(0x67de6, 77de6_script_method_scriptbuiltin_detachall.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - script object lookup and G_EntDetachAll argument checked against current decompiler output. */
/* 0x67de6 ScrCmd_detachAll */
void ScrCmd_detachAll(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    G_EntDetachAll(ent);
}

/* VERIFIED_DECOMPILER(0x67e17, 77e17_script_method_scriptbuiltin_getattachsize.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - six-slot count loop, zero model-index terminator, Scr_AddInt argument, and return behavior checked against current decompiler output. */
/* 0x67e17 ScrCmd_GetAttachSize */
void ScrCmd_GetAttachSize(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    int count;

    for (count = 0;
         count < DOBJ_ATTACH_SLOT_COUNT && ent->attachModelIndex[count] != 0;
         count++) {
    }

    Scr_AddInt(count);
}

/* VERIFIED_DECOMPILER(0x67e71, 77e71_script_method_scriptbuiltin_getattachmodelname.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - unsigned index range check, occupied-slot check, G_ModelName argument, and Scr_AddString call checked against current decompiler output. */
/* 0x67e71 ScrCmd_GetAttachModelName */
void ScrCmd_GetAttachModelName(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    uint32_t index = (uint32_t)Scr_GetInt(0);

    if (index >= DOBJ_ATTACH_SLOT_COUNT ||
        ent->attachModelIndex[index] == 0) {
        Scr_ParamError(0, "bad index");
    }

    Scr_AddString(G_ModelName(ent->attachModelIndex[index]));
}

/* VERIFIED_DECOMPILER(0x67ef2, 77ef2_script_method_scriptbuiltin_getattachtagname.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - unsigned index range check, occupied-slot check, attachTagIndex offset, and Scr_AddConstString call checked against current decompiler output. */
/* 0x67ef2 ScrCmd_GetAttachTagName */
void ScrCmd_GetAttachTagName(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    uint32_t index = (uint32_t)Scr_GetInt(0);

    if (index >= DOBJ_ATTACH_SLOT_COUNT ||
        ent->attachModelIndex[index] == 0) {
        Scr_ParamError(0, "bad index");
    }

    Scr_AddConstString(ent->attachTagIndex[index]);
}

/* VERIFIED_DECOMPILER(0x67f6a, 77f6a_script_method_scriptbuiltin_getattachignorecollision.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - unsigned index range check, occupied-slot check, ignore-collision byte load at gentity+0x182, shift/mask, and Scr_AddBool call checked against current decompiler output. */
/* 0x67f6a ScrCmd_GetAttachIgnoreCollision */
void ScrCmd_GetAttachIgnoreCollision(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    uint32_t index = (uint32_t)Scr_GetInt(0);

    if (index >= DOBJ_ATTACH_SLOT_COUNT ||
        ent->attachModelIndex[index] == 0) {
        Scr_ParamError(0, "bad index");
    }

    Scr_AddBool((game_compat_g_attach_ignore_collision_value(ent) >> index) & 1);
}

/* VERIFIED_DECOMPILER(0x67fe7, 77fe7_script_method_scriptbuiltin_linkto.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - entity parameter type checks, linkTo-enabled flag gate, parent/tag/default args, optional origin/angles offset vectors, link call selection, and failure-reporting error paths checked against current decompiler output. */
/* 0x67fe7 ScrCmd_LinkTo */
void ScrCmd_LinkTo(uint32_t scriptObject)
{
    gentity_t *child = script_object_to_gentity(scriptObject);
    gentity_t *parent;
    const char *tagName;
    qboolean linked;
    uint32_t paramCount;

    game_compat_dobj_require_entity_parameter(0);

    if ((child->flags & FL_SUPPORTS_LINKTO) == 0) {
        Scr_ObjectError(va("entity (classname: '%s') does not currently support linkTo",
                           SL_ConvertToString(child->scriptClassname)));
    }

    parent = Scr_GetEntity(0);
    paramCount = Scr_GetNumParam();
    tagName = LINKTO_DEFAULT_TAG;

    if (paramCount >= 2) {
        tagName = Scr_GetString(1);
    }

    if (paramCount < 3) {
        linked = G_EntLinkTo(child, parent, tagName);
    } else {
        vec3_t originOffset;
        vec3_t anglesOffset;

        Scr_GetVector(2, originOffset);
        Scr_GetVector(3, anglesOffset);
        linked = G_EntLinkToWithOffset(child, parent, tagName,
                                       originOffset, anglesOffset);
    }

    if (!linked) {
        game_compat_dobj_report_link_failure(parent, tagName);
    }
}

/* VERIFIED_DECOMPILER(0x681ff, 781ff_script_method_scriptbuiltin_unlink.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - script object lookup and G_EntUnlink argument checked against current decompiler output. */
/* 0x681ff ScrCmd_Unlink */
void ScrCmd_Unlink(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    G_EntUnlink(ent);
}

/* VERIFIED_DECOMPILER(0x68230, 78230_script_method_scriptbuiltin_enablelinkto.c, VERIFY-DOBJLINK-ATTACHLINK-2026-06-17): DATAFLOW_VERIFIED - already-enabled flag error, general/linked-byte/think eligibility, trigger_multiple exception, nextthink/think stores, and linkTo-enabled flag set checked against current decompiler output. */
/* 0x68230 ScrCmd_EnableLinkTo */
void ScrCmd_EnableLinkTo(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    if ((ent->flags & FL_SUPPORTS_LINKTO) != 0) {
        Scr_ObjectError("entity already has linkTo enabled");
    }

    if (!game_compat_dobj_can_enable_link_to(ent)) {
        Scr_ObjectError(va("entity (classname: '%s') does not currently support enableLinkTo",
                           SL_ConvertToString(ent->scriptClassname)));
    }

    ent->nextthink = level.time;
    ent->think = Think_GeneralLink;
    ent->flags |= FL_SUPPORTS_LINKTO;
}
