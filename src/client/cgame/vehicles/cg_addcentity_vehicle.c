// Source: uo_cgame_mp_x86.dll 0x30021660..0x30021851
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021660_30021851.mcode
//
// CG_AddCEntity_Vehicle — the ET_VEHICLE (currentState.eType 12, shared with 13)
// arm of CG_AddCEntity's dispatch (0x30022170, jump-table slot 12/13 == 0x300221e3
// -> CALL 0x30021660). It does two things for one vehicle client entity:
//   (1) builds and submits its animated DObj render model (an RT_MODEL refEntity_t
//       tagged RF_DOBJ_MODEL|RF_LIGHTING_ORIGIN), and
//   (2) draws a team-gated HUD head icon over the vehicle's occupant.
//
// NAMING: the .mcode header name "Scr_Vehicle_Pain" is a pure SIZE-MATCH guess
// (win size 0x1f1 == a same-size symbol in game_mp_uo, the WRONG DLL — a server GSC
// script method) and is REJECTED per the size-match-name-is-noise policy: this body
// runs no server script, deals no damage, and touches no gentity. It is proven to be
// the client vehicle centity render handler by the call graph: CG_AddCEntity
// (0x30022170) routes eType 12/13 here (EAX=cent, kept in EBX), and the body mirrors
// the eType-0 handler CG_General (0x3001e430) almost line for line — same EF_NODRAW
// gate, same CG_RefreshEntityDObjAnimTree, same CG_DOBJ_GET_HANDLE acquire, same
// zero-fill-and-fill refEntity_t submitted via trap_R_AddRefEntityToScene — then adds
// the vehicle-specific head-icon draw. `CG_AddCEntity_Vehicle` is the caller-observed
// name already recorded in client_recovered.h for 0x30021660; adopted here.
//
// ABI: cent arrives in EBX (register argument; the dispatcher keeps cent there across
// its whole switch and never reloads it before the CALL). The /GS stack cookie
// (__security_cookie @0x30081650 snapshotted into the frame on entry, verified via
// __security_check_cookie @0x30061639 on exit) and the AND ESP,~7 frame alignment are
// i386 calling-convention detail, recorded here and expressed as plain C. Plain RET
// (the register arg needs no caller cleanup).
//
// CONSTANTS (dumped exact, not guessed):
//   0x3007bdd0 = 0x42000000 = 32.0f   (lightingOrigin Z lift; FADD at 0x30021711)
//   0x3007bd94 = 0x3a83126f = 0.001f  (ms->seconds; FMUL at 0x3002173c)
//   0x2080     = RF_DOBJ_MODEL (0x2000) | RF_LIGHTING_ORIGIN (0x80)  (renderfx)
//   0x3ff = ENTITYNUM_NONE-family sentinel; 0x25 = CG_HEADICON_CONFIGSTRING_BASE (37);
//   0x64 = 100 (fixed yaw arg); 0x3f800000 = 1.0f (alphaScale arg); 3 = TEAM_SPECTATOR.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include <string.h>

/* Config-string base added to cent->currentState.iHeadIcon before the CG_ConfigString lookup
 * that yields the head-icon shader name (ADD ECX,0x25 at 0x30021812). Same +37 base
 * used by CG_AddHeadIcon (0x30032ac0) and the sibling builder at 0x300217ef. */
enum {
    CG_HEADICON_CONFIGSTRING_BASE = 37
};

/* Vertical lift (world units) applied to the DObj model's lighting origin: the entity
 * origin plus 32 units in Z. 0x3007bdd0 == 32.0f. */
#define CG_VEHICLE_LIGHTING_ORIGIN_ZLIFT 32.0f

/* ms -> seconds scale for the model's shaderTime (age of the effect). 0x3007bd94
 * == 0.001f. */
#define CG_MS_TO_SECONDS 0.001f

/* Fixed yaw passed to the vehicle head-icon sprite (PUSH 0x64 at 0x30021833). */
enum {
    CG_VEHICLE_HEADICON_YAW = 100
};

/* NOT_FROM_ORIGINAL_SOURCE: rate-limit the two warnings added by the bounded
 * vehicle head-icon compatibility path. A hostile snapshot can otherwise make
 * either invalid index recur every rendered frame. */
static qboolean cgame_compat_reported_invalid_vehicle_occupant;
static qboolean cgame_compat_reported_invalid_vehicle_driver;

void CG_AddCEntity_Vehicle(centity_t *cent /* EBX */)
{
    /* 0x30021678: MOV AL,[EBX+8]; TEST AL,AL; JS -> low byte's sign bit = eFlags bit 7. */
    if (cent->currentState.eFlags & EF_NODRAW)
        return;

    int32_t modelIndex = cent->currentState.modelIndex;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)modelIndex >= (uint32_t)CS_MODELS_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_AddCEntity_Vehicle: invalid model index %i",
                  modelIndex);
        return;
    }
    qhandle_t modelHandle = cg_gameModels[modelIndex];
    int32_t eType = cent->currentState.eType;
    int32_t entityNum = cent->currentState.number;
    CG_RefreshEntityDObjAnimTree(entityNum, eType, modelHandle);

    /* 0x3002169e..0x300216b3: handle = (int32_t)cgame_syscall(CG_DOBJ_GET_HANDLE, cent->currentState.number).
     * A zero handle means no DObj skeleton -> nothing to render, and no head icon. */
    entityNum = cent->currentState.number;
    struct DObj_s *dobj = (struct DObj_s *)cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);
    if (dobj == NULL)
        return;

    /* ---- Build and submit the vehicle DObj render entity (0x300216b9..0x30021771) ----
     * REP STOSD zeroes 0x27 (39) dwords = 0x9c = sizeof(refEntity_t) first, so any field
     * not written stays 0. */
    /* Z is loaded before REP STOSD; X and Y are loaded afterward. The six
     * destination writes are raw dwords in x,z,x,z,y,y order. */
    uint32_t zBits;
    memcpy(&zBits, &cent->lerpOrigin[2], sizeof(zBits));
    refEntity_t re;
    memset(&re, 0, sizeof(re));
    uint32_t xBits;
    uint32_t yBits;
    memcpy(&xBits, &cent->lerpOrigin[0], sizeof(xBits));
    memcpy(&yBits, &cent->lerpOrigin[1], sizeof(yBits));
    memcpy(&re.origin[0], &xBits, sizeof(xBits));
    memcpy(&re.origin[2], &zBits, sizeof(zBits));
    memcpy(&re.oldorigin[0], &xBits, sizeof(xBits));
    memcpy(&re.oldorigin[2], &zBits, sizeof(zBits));
    memcpy(&re.origin[1], &yBits, sizeof(yBits));
    memcpy(&re.oldorigin[1], &yBits, sizeof(yBits));

    /* axis = AnglesToAxisNegRight(re.axis, cent->lerpAngles). EAX = &re.axis,
     * EDX = &cent->lerpAngles. (0x300216e8/0x300216ee/0x300216fa) */
    AnglesToAxisNegRight(re.axis, cent->lerpAngles);

    /* lightingOrigin = entity origin, lifted 32 units in Z (0x300216ff..0x30021742).
     * Instead of CG_General's CG_SetupWeaponLightingOrigin call, the vehicle path fills
     * the lighting origin inline: X/Y from lerpOrigin, Z = lerpOrigin[2] + 32. */
    uint32_t currentTime = cg_time;
    long double lightingZ = (long double)cent->lerpOrigin[2] + (long double)CG_VEHICLE_LIGHTING_ORIGIN_ZLIFT;
    int32_t miscTime = cent->miscTime;
    memcpy(&re.lightingOrigin[0], &cent->lerpOrigin[0], sizeof(re.lightingOrigin[0]));
    memcpy(&re.lightingOrigin[1], &cent->lerpOrigin[1], sizeof(re.lightingOrigin[1]));
    re.lightingOrigin[2] = (float)lightingZ;

    /* shaderTime = model age in seconds = (cg.time - cent->miscTime) * 0.001. The
     * subtraction is done in integer ms (SUB EDX,EDI), then FILD/FMUL to float
     * (0x30021705/0x30021723/0x30021731/0x3002173c/0x30021755). */
    int32_t ageMs = coduo_int32_from_bits(currentTime - (uint32_t)miscTime);
    re.shaderTime = (float)((long double)ageMs * (long double)CG_MS_TO_SECONDS);

    /* DObj handle, owning centity, render kind, and flags (0x30021746..0x30021763). */
    re.renderfx = (int32_t)(RF_DOBJ_MODEL | RF_LIGHTING_ORIGIN); /* 0x2080 */
    re.dobj = dobj;                                                   /* re+0x90 = ESI */
    re.owner = cent;                                           /* re+0x94 = EBX */
    re.reType = RT_MODEL;                                       /* re+0x00 = 1 */

    /* 0x3002176b: trap_R_AddRefEntityToScene(&re) (LEA EAX,&re; PUSH; PUSH 0x3d; call). */
    trap_R_AddRefEntityToScene(&re);

    /* ---- Team-gated vehicle head icon (0x30021771..0x3002183c) ---------------------
     * Draw a HUD head icon over the vehicle's occupant, subject to a chain of guards
     * that all cause an early return (the shared /GS epilogue) when they fail. */

    /* vehicleEntityNum == 0x3ff (no occupant) -> nothing to draw. */
    int32_t vehicleClient = cent->currentState.vehicleEntityNum;
    if (vehicleClient == 0x3ff)
        return;

    /* The occupant is the local player themselves -> suppress (their own icon). */
    int32_t localClientNum = cg_snap->ps.psClientNum;
    if (vehicleClient == localClientNum)
        return;

    /* When the local player is spectating in first person and following an entity, and
     * the followed entity IS this vehicle centity, suppress the icon (you are it). */
    if ((cg_snap->ps.entityStateFlags & EF_IN_VEHICLE) != 0 && cg_snap->ps.viewLockedEntityNum == cent->currentState.number)
        return;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)vehicleClient >= (uint32_t)MAX_CLIENTS) {
        if (cgame_compat_reported_invalid_vehicle_occupant == qfalse) {
            Com_Printf("WARNING: rejected invalid vehicle occupant %i\n", vehicleClient);
            cgame_compat_reported_invalid_vehicle_occupant = qtrue;
        }
        return;
    }

    /* The occupant client's per-client anim/HUD state must be populated (infoValid
     * != 0), indexed clientNum*0x4d0 from bgs.clientinfo. (0x300217b1..0x300217c1) */
    clientInfo_t *vehicleClientInfo = &bgs.clientinfo[vehicleClient];
    if (vehicleClientInfo->infoValid == 0)
        return;

    /* The occupant of *that* entity (cg_entities[vehicleClient].clientNum, i.e. the
     * driver's real client number) must likewise have populated anim state. cg_entities
     * is modeled through the existing centity_t view of the base array at
     * 0x3048c6e0 (IMUL 0x288 + base, then +0x94 == clientNum), matching the established
     * idiom in the 0x30031cb0.. HUD-tag cluster. (0x300217c3..0x300217dd) */
    centity_t *vehicleEnt = &cg_entities[vehicleClient];
    int32_t driverClientNum = vehicleEnt->currentState.clientNum;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)driverClientNum >= (uint32_t)MAX_CLIENTS) {
        if (cgame_compat_reported_invalid_vehicle_driver == qfalse) {
            Com_Printf("WARNING: rejected invalid vehicle driver %i\n", driverClientNum);
            cgame_compat_reported_invalid_vehicle_driver = qtrue;
        }
        return;
    }
    clientInfo_t *driverClientInfo = &bgs.clientinfo[driverClientNum];
    if (driverClientInfo->infoValid == 0)
        return;

    /* The LOCAL player's anim state must be populated too; read its team selector.
     * (0x300217df..0x300217fd) */
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)localClientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_AddCEntity_Vehicle: "
                  "invalid local client number %i",
                  localClientNum);
        return;
    }
    clientInfo_t *localClientInfo = &bgs.clientinfo[localClientNum];
    if (localClientInfo->infoValid == 0)
        return;
    /* 0x300217ef loads this once before the local-team load, then retains ECX
     * through the visibility gates and the final wrapped +37. */
    int32_t headIconCsIndex = cent->currentState.iHeadIcon;
    int32_t localHudTeam = localClientInfo->team;

    /* No head-icon config string -> nothing to draw. (0x300217ef/0x300217fd) */
    if (headIconCsIndex == 0)
        return;

    /* Team visibility: draw only when there is no team restriction (headIconTeam == 0),
     * the local player is spectating (localHudTeam == TEAM_SPECTATOR), or the teams
     * match. (0x300217ff..0x30021810) */
    int32_t iconTeam = cent->currentState.headIconTeam;
    if (iconTeam != 0 && localHudTeam != TEAM_SPECTATOR && iconTeam != localHudTeam)
        return;

    /* Resolve and register the icon shader; draw it over the vehicle if valid.
     * (0x30021812..0x3002183c) */
    int32_t iconConfigStringIndex = coduo_int32_from_bits((uint32_t)headIconCsIndex + (uint32_t)CG_HEADICON_CONFIGSTRING_BASE);
    const char *iconName = CG_ConfigString(iconConfigStringIndex);
    qhandle_t material = CG_RegisterMaterial(iconName, 5);
    if (material != 0) {
        CG_AddHudHeadIconSprite(cent, material, CG_VEHICLE_HEADICON_YAW, 0, 0, 1.0f);
    }
}
