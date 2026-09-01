/*
 * Source reconstruction for script objective builtins.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>

#include "recovered_game.h"
#include "game_globals.h"
#include "level_locals.h"
#include "scr_vm.h"

#define OBJECTIVE_ADD_USAGE_ERROR \
    "objective_add needs at least the first two parameters out of its parameter list of: index state [string] [position]\n"
#define OBJECTIVE_INDEX_ERROR \
    "index %i is an illegal objective index. Valid indexes are 0 to %i\n"
#define OBJECTIVE_STATE_ERROR \
    "Illegal objective state \"%s\". Valid states are \"empty\", \"invisible\", \"current\"\n"
#define OBJECTIVE_TEAM_ERROR \
    "Illegal team string '%s'. Must be allies, axis, or none."

enum { OBJECTIVE_ICON_MAX_LEN = MAX_QPATH - 1 };

int G_ShaderIndex(const char *name);

static objective_t *game_compat_objective_for_script_index(int index, uint32_t paramIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: factored objective index validation from callers. */
    if (index < 0 || index >= PLAYERSTATE_OBJECTIVE_COUNT) {
        Scr_ParamError(paramIndex,
                       va(OBJECTIVE_INDEX_ERROR, index,
                          PLAYERSTATE_OBJECTIVE_COUNT - 1));
    }

    return &level.objectives[index];
}

/* VERIFIED_DECOMPILER(0x69dd7, 79dd7_FUN_00079dd7.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - entityNum guard, g_entities stride, linked-byte check, svFlags 0x10 clear, and entityNum reset checked against current decompiler output. */
void ClearObjective_OnEntity(objective_t *objective)
{
    if (objective->entityNum != ENTITYNUM_NONE) {
        gentity_t *ent = &g_entities[objective->entityNum];

        if (ent->linked != 0) {
            ent->svFlags &= ~SVF_OBJECTIVE;
        }

        objective->entityNum = ENTITYNUM_NONE;
    }
}

/* VERIFIED_DECOMPILER(0x69e3f, 79e3f_FUN_00079e3f.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - objective slot state, origin words, entityNum, team, and icon reset stores checked against current decompiler output. */
void ClearObjective(objective_t *objective)
{
    objective->state = OBJECTIVE_STATE_EMPTY;
    objective->origin[2] = 0.0f;
    objective->origin[1] = 0.0f;
    objective->origin[0] = 0.0f;
    objective->entityNum = ENTITYNUM_NONE;
    objective->teamNum = TEAM_FREE;
    objective->icon = 0;
}

/* VERIFIED_DECOMPILER(0x69cf8, 79cf8_G_InitObjectives.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - 16-slot objective loop, 0x1c stride, and ClearObjective call target checked against current decompiler output. */
void G_InitObjectives(void)
{
    int index;
    for (index = 0; index < PLAYERSTATE_OBJECTIVE_COUNT; index++) {
        ClearObjective(&level.objectives[index]);
    }
}

/* VERIFIED_DECOMPILER(0x69d4b, 79d4b_ObjectiveStateIndexFromString.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - empty/invisible/current const-string mapping, failure reset, and boolean returns checked against current decompiler output. */
qboolean ObjectiveStateIndexFromString(int *outState, uint16_t stateName)
{
    if (stateName == scr_const_empty) {
        *outState = OBJECTIVE_STATE_EMPTY;
    } else if (stateName == scr_const_invisible) {
        *outState = OBJECTIVE_STATE_INVISIBLE;
    } else if (stateName == scr_const_current) {
        *outState = OBJECTIVE_STATE_CURRENT;
    } else {
        *outState = OBJECTIVE_STATE_EMPTY;
        return 0;
    }

    return 1;
}

/* VERIFIED_DECOMPILER(0x69e92, 79e92_FUN_00079e92.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - string parameter read, control-character validation, length limit 63, hard-coded ParamError index 3, and shader index store checked against current decompiler output. */
void SetObjectiveIcon(objective_t *objective, uint32_t paramIndex)
{
    const char *name = Scr_GetString(paramIndex);
    int index;

    for (index = 0; name[index] != '\0'; index++) {
        if (name[index] < ' ' || name[index] == '\x7f') {
            Scr_ParamError(
                3,
                va("Illegal character '%c'(ascii %i) in objective icon name: %s\n",
                   name[index], (unsigned char)name[index], name));
        }
    }

    if (index > OBJECTIVE_ICON_MAX_LEN) {
        Scr_ParamError(
            3,
            va("Objective icon name is too long (> %i): %s\n",
               OBJECTIVE_ICON_MAX_LEN, name));
    }

    objective->icon = G_ShaderIndex(name);
}

static void game_compat_objective_set_rounded_origin(objective_t *objective, uint32_t paramIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: factored vector read plus x87 fistp RC=truncate stores. */
    Scr_GetVector(paramIndex, objective->origin);
    objective->origin[0] =
        (float)game_compat_int32_from_float_trunc(objective->origin[0]);
    objective->origin[1] =
        (float)game_compat_int32_from_float_trunc(objective->origin[1]);
    objective->origin[2] =
        (float)game_compat_int32_from_float_trunc(objective->origin[2]);
}

/* VERIFIED_DECOMPILER(0x69f70, 79f70_script_func_objective_add.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - parameter-count usage check, index validation, prior entity clear, state parse/error path, optional rounded origin/icon, entity reset, and team reset checked against current decompiler output. */
void Scr_Objective_Add(void)
{
    uint32_t paramCount = Scr_GetNumParam();
    int state;
    uint16_t stateName;
    objective_t *objective;

    if (paramCount < 2) {
        Scr_Error(OBJECTIVE_ADD_USAGE_ERROR);
    }

    objective = game_compat_objective_for_script_index(Scr_GetInt(0), 0);
    ClearObjective_OnEntity(objective);

    stateName = Scr_GetConstString(1);
    if (!ObjectiveStateIndexFromString(&state, stateName)) {
        Scr_ParamError(
            1,
            va(OBJECTIVE_STATE_ERROR, SL_ConvertToString(stateName)));
    }

    objective->state = state;

    if (paramCount > 2) {
        game_compat_objective_set_rounded_origin(objective, 2);
        objective->entityNum = ENTITYNUM_NONE;

        if (paramCount > 3) {
            SetObjectiveIcon(objective, 3);
        }
    }

    objective->teamNum = TEAM_FREE;
}

/* VERIFIED_DECOMPILER(0x6a132, 7a132_script_func_objective_delete.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - index validation, prior entity clear, and objective slot reset checked against current decompiler output. */
void Scr_Objective_Delete(void)
{
    objective_t *objective = game_compat_objective_for_script_index(Scr_GetInt(0), 0);

    ClearObjective_OnEntity(objective);
    ClearObjective(objective);
}

/* VERIFIED_DECOMPILER(0x6a1de, 7a1de_script_func_objective_state.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - index validation, state const parsing, Scr_GetString error text path, state store, and entity clear for empty/invisible checked against current decompiler output. */
void Scr_Objective_State(void)
{
    int state;
    objective_t *objective = game_compat_objective_for_script_index(Scr_GetInt(0), 0);
    uint16_t stateName = Scr_GetConstString(1);

    if (!ObjectiveStateIndexFromString(&state, stateName)) {
        Scr_ParamError(1, va(OBJECTIVE_STATE_ERROR, Scr_GetString(1)));
    }

    objective->state = state;

    if (state == OBJECTIVE_STATE_EMPTY ||
        state == OBJECTIVE_STATE_INVISIBLE) {
        ClearObjective_OnEntity(objective);
    }
}

/* VERIFIED_DECOMPILER(0x6a2d6, 7a2d6_script_func_objective_icon.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - index validation and SetObjectiveIcon parameter index 1 forwarding checked against current decompiler output. */
void Scr_Objective_Icon(void)
{
    SetObjectiveIcon(game_compat_objective_for_script_index(Scr_GetInt(0), 0), 1);
}

/* VERIFIED_DECOMPILER(0x6a365, 7a365_script_func_objective_position.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - index validation, prior entity clear, vector parameter index 1 read, and three rounded origin stores checked against current decompiler output. */
void Scr_Objective_Position(void)
{
    objective_t *objective = game_compat_objective_for_script_index(Scr_GetInt(0), 0);

    ClearObjective_OnEntity(objective);
    game_compat_objective_set_rounded_origin(objective, 1);
}

/* VERIFIED_DECOMPILER(0x6a477, 7a477_script_func_objective_onentity.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - index validation, prior entity clear, Scr_GetEntity parameter index 1, svFlags 0x10 set, and entity number store checked against current decompiler output. */
void Scr_Objective_OnEntity(void)
{
    objective_t *objective = game_compat_objective_for_script_index(Scr_GetInt(0), 0);
    gentity_t *ent;

    ClearObjective_OnEntity(objective);
    ent = Scr_GetEntity(1);
    ent->svFlags |= SVF_OBJECTIVE;
    objective->entityNum = ent->s.number;
}

/* VERIFIED_DECOMPILER(0x6a533, 7a533_script_func_objective_current.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - selected-index scratch mask, per-argument index validation, current-state promotion, and unselected current-to-active demotion checked against current decompiler output. */
void Scr_Objective_Current(void)
{
    uint32_t paramCount = Scr_GetNumParam();
    int selected[PLAYERSTATE_OBJECTIVE_COUNT] = {0};
    uint32_t paramIndex;
    int objectiveIndex;

    for (paramIndex = 0; paramIndex < paramCount; paramIndex++) {
        objectiveIndex = Scr_GetInt(paramIndex);
        game_compat_objective_for_script_index(objectiveIndex, paramIndex);
        selected[objectiveIndex] = 1;
    }

    for (objectiveIndex = 0; objectiveIndex < PLAYERSTATE_OBJECTIVE_COUNT; objectiveIndex++) {
        objective_t *objective = &level.objectives[objectiveIndex];

        if (selected[objectiveIndex] != 0) {
            objective->state = OBJECTIVE_STATE_CURRENT;
        } else if (objective->state == OBJECTIVE_STATE_CURRENT) {
            objective->state = OBJECTIVE_STATE_ACTIVE;
        }
    }
}

/* VERIFIED_DECOMPILER(0x6a635, 7a635_script_func_objective_team.c, VERIFY-SCRIPT-OBJECTIVES-PACKET-2026-06-17): DATAFLOW_VERIFIED - index validation, allies/axis/none const-string mapping, team stores, and SL_ConvertToString error path checked against current decompiler output. */
void GScr_Objective_Team(void)
{
    objective_t *objective = game_compat_objective_for_script_index(Scr_GetInt(0), 0);
    uint16_t teamName = Scr_GetConstString(1);

    if (teamName == scr_const_allies) {
        objective->teamNum = TEAM_ALLIES;
    } else if (teamName == scr_const_axis) {
        objective->teamNum = TEAM_AXIS;
    } else if (teamName == scr_const_none) {
        objective->teamNum = TEAM_FREE;
    } else {
        Scr_ParamError(1,
                       va(OBJECTIVE_TEAM_ERROR,
                          SL_ConvertToString(teamName)));
    }
}
