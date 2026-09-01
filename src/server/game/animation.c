/*
 * Source reconstruction for player animation lookup helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "recovered_game.h"
#include "game_functions.h"
#include "qcommon/com_parse.h"
#include "compat/coduo_ctype_compat.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/coduo_native_x87.h"
#include "compat/libm/coduo_libm.h"
#include "qcommon/q_bits.h"
#include "qcommon/q_string.h"

#define BG_WEAPON_STRING_TABLE_STOCK_I386_BYTES 1024
#define BG_WEAPON_STRING_NONE "none"
#define BG_UPDATE_ANIM_SLOT_RANGE_ERROR \
    COM_ERROR_MARKER "Player animation index out of range (%i): %i"
#define BG_ANIM_SCRIPT_FILE_MAX_BYTES 99998
#define BG_ANIM_SCRIPT_FILE_BUFFER_SIZE (BG_ANIM_SCRIPT_FILE_MAX_BYTES + 1)
#define BG_PARSE_TOKEN_BUFFER_SIZE 64
#define BG_PARSE_TOKEN_NONE "none"
#define BG_PARSE_TOKEN_NONE_COMMA "none,"
#define BG_PARSE_TOKEN_COMMA ","
#define BG_PARSE_TOKEN_MINUS_SYMBOL "-"
#define BG_PARSE_TOKEN_MINUS "MINUS"
#define BG_PARSE_TOKEN_AND "AND"
#define BG_PARSE_TOKEN_SPACE " "
#define BG_PARSE_TOKEN_ALL "all"
#define BG_PARSE_TOKEN_DEFAULT "default"
#define BG_PARSE_TOKEN_SET "set"
#define BG_PARSE_TOKEN_STATE "state"
#define BG_PARSE_TOKEN_STATECHANGE "statechange"
#define BG_PARSE_TOKEN_OPEN_BRACE "{"
#define BG_PARSE_TOKEN_CLOSE_BRACE "}"
#define BG_PARSE_TOKEN_EQUALS "="
#define BG_PARSE_TOKEN_DURATION "duration"
#define BG_PARSE_TOKEN_TURRETANIM "turretanim"
#define BG_PARSE_TOKEN_BLENDTIME "blendtime"
#define BG_PARSE_TOKEN_SOUND "sound"
#define BG_PARSE_TOKEN_WAV ".wav"
#define BG_PARSE_CONDITION_END_ERROR \
    "BG_ParseConditionBits: unexpected end of condition"
#define BG_PARSE_CONDITION_TOKEN_ERROR \
    "BG_ParseConditionBits: unexpected '%s'"
#define BG_PARSE_CONDITION_VALUE_END_ERROR \
    "BG_ParseConditions: expected condition value, found end of line"
#define BG_PARSE_NO_CONDITIONS_ERROR "BG_ParseConditions: no conditions found"
#define BG_PARSE_MAX_ANIMS_ERROR \
    "BG_ParseCommands: exceeded maximum number of animations (%i)"
#define BG_PARSE_EXPECTED_ANIM_ERROR "BG_ParseCommands: expected animation"
#define BG_PARSE_EXPECTED_DURATION_ERROR \
    "BG_ParseCommands: expected duration value"
#define BG_PARSE_TURRET_BODY_ERROR \
    "BG_ParseCommands: Turret animations can only be played on the 'both' body part"
#define BG_PARSE_EXPECTED_BLENDTIME_ERROR \
    "BG_ParseCommands: expected blendtime value"
#define BG_PARSE_EXPECTED_SOUND_ERROR "BG_ParseCommands: expected sound"
#define BG_PARSE_WAV_SOUND_ERROR \
    "BG_ParseCommands: wav files not supported, only sound scripts"
#define BG_PARSE_UNKNOWN_PARAMETER_ERROR \
    "BG_ParseCommands: unknown parameter '%s'"
#define BG_PARSE_SCRIPT_LOAD_ERROR \
    COM_ERROR_MARKER "Couldn't load player animation script %s\n"
#define BG_PARSE_UNEXPECTED_EOF_ERROR \
    "BG_AnimParseAnimScript: unexpected end of file: %s"
#define BG_PARSE_UNEXPECTED_TOKEN_ERROR \
    "BG_AnimParseAnimScript: unexpected '%s'"
#define BG_PARSE_EXPECTED_CONDITION_TYPE_ERROR \
    "BG_AnimParseAnimScript: expected condition type string"
#define BG_PARSE_DEFINE_TYPE_ERROR \
    "BG_AnimParseAnimScript: can not make a define of type '%s'"
#define BG_PARSE_EXPECTED_CONDITION_DEFINE_ERROR \
    "BG_AnimParseAnimScript: expected condition define string"
#define BG_PARSE_EXPECTED_EQUALS_EOF_ERROR \
    "BG_AnimParseAnimScript: expected '=', found end of line"
#define BG_PARSE_EXPECTED_EQUALS_TOKEN_ERROR \
    "BG_AnimParseAnimScript: expected '=', found '%s'"
#define BG_PARSE_EXPECTED_STATE_ERROR \
    "BG_AnimParseAnimScript: expected 'state'"
#define BG_PARSE_EXPECTED_STATE_TYPE_ERROR \
    "BG_AnimParseAnimScript: expected state type"
#define BG_PARSE_EXPECTED_OPEN_BRACE_ERROR \
    "BG_AnimParseAnimScript: expected '{'"
#define BG_PARSE_INTERNAL_ERROR "BG_AnimParseAnimScript: internal error"
#define BG_PARSE_MAX_LIST_ITEMS_ERROR \
    "BG_AnimParseAnimScript: exceeded maximum items per script (%i)"
#define BG_PARSE_MAX_GLOBAL_ITEMS_ERROR \
    "BG_AnimParseAnimScript: exceeded maximum global items (%i)"
#define BG_PARSE_EXPECTED_STATECHANGE_ERROR \
    "BG_AnimParseAnimScript: expected 'statechange', got '%s'"
#define BG_PARSE_EXPECTED_STATECHANGE_TYPE_ERROR \
    "BG_AnimParseAnimScript: expected <state type>"
#define BG_ANIM_FIRE_WEAPON_BLEND_TIME_MS 30
#define BG_ANIM_TIMER_PAD_MS 50
#define BG_ANIM_RESTART_MIN_TIMER_MS 50

/* clientInfo_t is now defined in recovered_game.h */

typedef char bg_static_animation_size_check[
    (sizeof(bg_static_animation_t) == 0x5c) ? 1 : -1];
typedef char bg_static_animation_table_count_check[
    (offsetof(bg_static_animation_table_t, entryCount) == 0xb800) ? 1 : -1];
GAME_I386_LAYOUT_ASSERT(bg_anim_table_anims_check,
                      offsetof(bg_static_animation_table_t, animTreeHandle) == 0xa7ac8);
GAME_I386_LAYOUT_ASSERT(bg_anim_table_size_check,
                      sizeof(bg_static_animation_table_t) == 0xa7acc);
GAME_I386_LAYOUT_ASSERT(bg_anim_table_script_lists_check,
                      offsetof(bg_static_animation_table_t, scriptLists) == 0x20eac);
GAME_I386_LAYOUT_ASSERT(bg_anim_table_canned_list_check,
                      offsetof(bg_static_animation_table_t, canned) == 0x14924);
GAME_I386_LAYOUT_ASSERT(bg_anim_table_script_pool_check,
                      offsetof(bg_static_animation_table_t, globalItems) == 0x21ac4);
GAME_I386_LAYOUT_ASSERT(bg_anim_table_script_pool_count_check,
                      offsetof(bg_static_animation_table_t, globalItemCount) == 0xa7ac4);
typedef char bg_static_animation_hash_check[
    (offsetof(bg_static_animation_t, hash) == 0x4c) ? 1 : -1];
typedef char bg_static_animation_used_check[
    (offsetof(bg_static_animation_t, usedByScript) == 0x58) ? 1 : -1];
#if UINTPTR_MAX == 0xffffffffu
typedef char bg_anim_script_command_size_check[
    (sizeof(bg_anim_script_command_t) == 0x10) ? 1 : -1];
#endif
typedef char bg_anim_condition_size_check[
    (sizeof(bg_anim_condition_t) == 0x0c) ? 1 : -1];
typedef char bg_anim_slot_animation_word_check[
    (offsetof(bg_anim_slot_t, animationWord) == 0x10) ? 1 : -1];
typedef char bg_anim_slot_animation_offset_check[
    (offsetof(bg_anim_slot_t, animationOffset) == 0x14) ? 1 : -1];
typedef char bg_anim_slot_anim_rate_check[
    (offsetof(bg_anim_slot_t, animRate) == 0x28) ? 1 : -1];
typedef char bg_anim_slot_last_update_time_check[
    (offsetof(bg_anim_slot_t, lastUpdateTime) == 0x2c) ? 1 : -1];

bg_static_animation_table_t *bgAnimStaticTable;
bg_runtime_animation_t *bgRuntimeAnimations;
static int32_t bgRuntimeAnimationCountValue;
int32_t *bgRuntimeAnimationCount = &bgRuntimeAnimationCountValue;
/* Animation-script tables and scratch state are defined once in src/bg. */

void Com_Error(errorParm_t code, const char *format, ...);
void Scr_FindAnim(const char *treeName, const char *animName,
                  scr_anim_t *outAnim);
uint32_t Scr_GetAnimsIndex(XAnim *anims);
void Com_Printf(const char *format, ...);
void G_AnimScriptSound(int entityNum, const char *soundAliasName);

#if UINTPTR_MAX == 0xffffffffu
typedef char bg_anim_script_record_size_check[
    (sizeof(bg_anim_script_t) == 0x10c) ? 1 : -1];
typedef char bg_anim_script_command_count_check[
    (offsetof(bg_anim_script_t, commandCount) == 0x88) ? 1 : -1];
typedef char bg_anim_script_commands_check[
    (offsetof(bg_anim_script_t, commands) == 0x8c) ? 1 : -1];
typedef char bg_anim_script_pool_capacity_check[
    (sizeof(((bg_static_animation_table_t *)0)->globalItems) /
         sizeof(((bg_static_animation_table_t *)0)->globalItems[0]) ==
     BG_ANIM_MAX_GLOBAL_SCRIPTS) ? 1 : -1];
#endif
typedef char bg_player_anim_pm_type_check[
    (offsetof(clientInfo_t, moduleState.pmType) == 0x04) ? 1 : -1];
/* The shared declaration fixes clientNum at +0x08 and gunHandLeft at +0x400. */
typedef char bg_anim_player_info_client_num_check[
    (offsetof(clientInfo_t, clientNum) == 0x08) ? 1 : -1];

GAME_I386_LAYOUT_ASSERT(bg_anim_script_list_size_check,
                      sizeof(bg_anim_script_list_t) == 0x204);
GAME_I386_LAYOUT_ASSERT(bg_anim_state_size_check,
                      ANIM_MT_COUNT * sizeof(bg_anim_script_list_t) == 0x2448);
GAME_I386_LAYOUT_ASSERT(bg_anim_statechange_to_state_size_check,
                      ANIM_STATE_COUNT * sizeof(bg_anim_script_list_t) == 0x810);
typedef char bg_anim_animation_lists_size_check[
    (ANIM_STATE_COUNT * ANIM_MT_COUNT * sizeof(bg_anim_script_list_t) ==
     sizeof(((bg_static_animation_table_t *)0)->animations)) ? 1 : -1];
typedef char bg_anim_canned_animation_lists_size_check[
    (ANIM_STATE_COUNT * ANIM_MT_COUNT * sizeof(bg_anim_script_list_t) ==
     sizeof(((bg_static_animation_table_t *)0)->canned)) ? 1 : -1];
typedef char bg_anim_statechange_lists_size_check[
    (ANIM_STATE_COUNT * ANIM_STATE_COUNT * sizeof(bg_anim_script_list_t) ==
     sizeof(((bg_static_animation_table_t *)0)->statechanges)) ? 1 : -1];
/* NOT_FROM_ORIGINAL_SOURCE: callback setter factored out of G_InitGame. */
void game_compat_bg_set_anim_sound_callbacks(bg_anim_sound_alias_fn soundAlias,
                              bg_anim_sound_event_fn soundEvent)
{
    bgs.soundAliasCallback = soundAlias;
    bgs.soundEventCallback = soundEvent;
}

/* The shared finalization and playback clusters are implemented in src/bg. */

/* 0x1e6ec BG_GetSurfIndex
 *
 * RECOVERED(UO-GAME-UNK-0135): Returns model index value for DObj construction.
 * The i386 helper returns constant zero.  Player DObj construction uses model
 * name strings to fetch XModels, and the companion "negative model index" slot
 * is zero in the stock helper; do not replace this with G_ModelIndex, which
 * would add configstring/precache side effects the decompile does not show.
 */
/* VERIFIED_DECOMPILER(0x1e6ec, 2e6ec_FUN_0002e6ec.c, VERIFY-ANIM-PACKET-2026-06-17): DATAFLOW_VERIFIED - ignored model-name argument and constant-zero return checked. */
static int BG_GetSurfIndex(const char *modelName)
{
    (void)modelName;
    return 0;
}

/* 0x1e6f6 BG_GetXModel
 *
 * RECOVERED(UO-GAME-UNK-0135): Returns XModel pointer for DObj construction.
 */
/* VERIFIED_DECOMPILER(0x1e6f6, 2e6f6_FUN_0002e6f6.c, VERIFY-ANIM-CONTROLLERS-2026-06-17): DATAFLOW_VERIFIED - sole trap_XModelGet call and wrapper return register preservation checked against decompiler and objdump 0x1e6f6..0x1e718. */
static XModel *BG_GetXModel(const char *modelName)
{
    return trap_XModelGet(modelName);
}

/* 0x1e719 BG_DObjCreate
 *
 * RECOVERED(UO-GAME-UNK-0135): Creates DObj from models array.
 */
/* VERIFIED_DECOMPILER(0x1e719, 2e719_FUN_0002e719.c, VERIFY-ANIM-CONTROLLERS-2026-06-17): DATAFLOW_VERIFIED - trap_DObjCreate argument order, uint16 model count widening, entity number, and final zero flags checked. */
static void BG_DObjCreate(DObjModel *models, uint16_t modelCount,
                          XAnimTree *animTree, int entityNum)
{
    trap_DObjCreate(models, modelCount, animTree, entityNum, 0);
}

/* 0x1f556 BG_UpdatePlayerDObj
 *
 * Updates the player's DObj (Dynamic Object) model. Builds a DObjModel array
 * from clientInfo and creates/updates the DObj. Handles vehicle weapon state
 * and attached models.
 */
/* VERIFIED_DECOMPILER(0x1f556, 2f556_BG_UpdatePlayerDObj.c, VERIFY-WAVE2-ANIMATION-STANCE-2026-06-17): DATAFLOW_VERIFIED
 * Evidence: matched DObj existence/update gates, vehicle weapon suppression,
 * model/tag array population, ignore-collision bits, create call, and version bump.
 */
void BG_UpdatePlayerDObj(gentity_t *ent, const gentity_t *entState,
                         clientInfo_t *ci,
                         uint8_t *dObjVersion)
{
    qboolean dobjExists = trap_DObjExists(ent);
    int modelIndex = entState->s.weapon;
    if ((entState->s.eFlags & EF_RESTRICTED_MASK) != 0) {
        const uint32_t vehicleAnimState = (uint32_t)entState->s.vehicleAnimState;
        const int vehicleType =
            (vehicleAnimState & VEHICLE_ANIM_STATE_TYPE_MASK) >>
            VEHICLE_ANIM_STATE_TYPE_SHIFT;
        const int vehiclePos =
            (vehicleAnimState & VEHICLE_ANIM_STATE_POS_MASK) >>
            VEHICLE_ANIM_STATE_POS_SHIFT;

        if (BG_AllowPlayerWeaponAtVehiclePos(vehicleType, vehiclePos) == 0) {
            modelIndex = 0;
        }
    }

    if (ci->infoValid == 0 || ci->modelName[0] == '\0') {
        /* DATAFLOW_VERIFIED: original frees DObj for argument 2's entity number. */
        trap_SafeDObjFree(entState->s.number, 1);
        return;
    }

    if (dobjExists) {
        if (ci->dobjSavedModel == modelIndex &&
            ci->dobjNeedsUpdate == 0 &&
            ci->dobjVersion == *dObjVersion) {
            return;
        }
        trap_SafeDObjFree(entState->s.number, 0);
    }

    XAnimTree *animTree = ci->animTree;

    /* Build DObjModel array */
    DObjModel models[7];
    int modelCount = 0;

    /* Base model */
    /* FUN_0002e6ec is constant-zero in the i386 binary. */
    models[modelCount].modelIndex =
        (int16_t)BG_GetSurfIndex(ci->modelName);
    models[modelCount].model = (XModel *)BG_GetXModel(ci->modelName);
    models[modelCount].tagName = NULL;
    models[modelCount].ignoreCollision = 0;
    modelCount++;

    /* Attached models */
    for (int slot = 0; slot < 6; slot++) {
        if (ci->attachModelNames[slot][0] == '\0') {
            continue;
        }

        /* FUN_0002e6ec is constant-zero in the i386 binary. */
        models[modelCount].modelIndex =
            (int16_t)BG_GetSurfIndex(ci->attachModelNames[slot]);
        models[modelCount].model =
            (XModel *)BG_GetXModel(ci->attachModelNames[slot]);
        models[modelCount].tagName = ci->attachTagNames[slot];
        models[modelCount].ignoreCollision =
            (ent->attachIgnoreCollision >> slot) & 1;
        modelCount++;
    }

    BG_DObjCreate(models, (uint16_t)modelCount, animTree,
                        entState->s.number);

    ci->dobjSavedModel = modelIndex;
    if (ci->dobjNeedsUpdate != 0) {
        ci->dobjNeedsUpdate = 0;
        ci->dobjVersion++;
    }
    *dObjVersion = ci->dobjVersion;
}
