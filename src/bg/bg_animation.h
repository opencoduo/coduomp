#ifndef BG_ANIMATION_H
#define BG_ANIMATION_H

#include "qcommon/bg_animation_types.h"
#include "qcommon/entity_state_types.h"
#include "qcommon/player_state_types.h"
#include "qcommon/pmove_types.h"
#include "qcommon/script_types.h"

#include <stdint.h>

/* Original animation-loader globals. The Linux ELF retains these spellings;
 * the Windows cgame/game binaries use the same pointer roles and layouts.
 * Windows cgame stores them at 0x30134cc8, 0x300a7820, and 0x300a5108.
 * BG_AnimationIndexForString selects fixed-table lookup versus runtime-array
 * registration from bgRuntimeAnimations and publishes additions through the
 * pointer-valued bgRuntimeAnimationCount. */
extern bg_static_animation_table_t *bgAnimStaticTable;
extern bg_runtime_animation_t *bgRuntimeAnimations;
extern int32_t *bgRuntimeAnimationCount;
extern bg_anim_condition_type_t bgAnimConditionTypes[];
extern bgs_t bgs;

/* Shared animation-script parser tables and scratch state. Each module owns
 * one compiled instance, matching the original cgame/game module boundary. */
extern bg_indexed_string_t weaponStrings[MAX_WEAPONS];
extern bg_indexed_string_t animStateStr[];
extern bg_indexed_string_t bgAnimGroupStrings[];
extern bg_indexed_string_t bgAnimEventStrings[];
extern bg_indexed_string_t animBodyPartsStr[];
extern bg_indexed_string_t animMountedStr[];
extern bg_indexed_string_t animVehicleMotionStr[];
extern bg_indexed_string_t animVehicleStr[];
extern bg_indexed_string_t animWeaponClassStr[];
extern bg_indexed_string_t animWeaponPositionStr[];
extern bg_indexed_string_t animStrafeStateStr[];
extern bg_indexed_string_t bgAnimConditionTypeStrings[];
extern bg_indexed_string_t bgAnimParseSectionStrings[];
extern bg_anim_move_type_t bgAnimParseCurrentAnimGroup;
extern bg_anim_event_t bgAnimParseCurrentEvent;

extern bg_indexed_string_t bgAnimConditionAliases[
    ANIM_COND_COUNT * BG_ANIM_CONDITION_VALUE_COUNT];
extern bg_condition_bits_t bgAnimConditionAliasBits[
    ANIM_COND_COUNT * BG_ANIM_CONDITION_VALUE_COUNT];
extern int32_t bgAnimConditionAliasCounts[ANIM_COND_COUNT];
extern char bgAnimConditionAliasStringBuffer[
    BG_ANIM_CONDITION_ALIAS_STRING_BUFFER_SIZE];
extern int32_t bgAnimConditionAliasStringUsed;
extern char bgAnimScriptFileBuffer[BG_ANIM_SCRIPT_FILE_BUFFER_SIZE];
extern int32_t bgAnimScriptLoaded;
extern const char *bgPlayerAnimScriptPath;

void BG_AnimParseError(const char *format, ...);
int32_t BG_StringHashValue(const char *text);
int32_t BG_IndexForString(const char *name, bg_indexed_string_t *strings,
                          qboolean allowMissing);
char *BG_CopyStringIntoBuffer(const char *text, char *buffer,
                              uint32_t bufferSize, int32_t *used);
int32_t BG_AnimationIndexForString(const char *name);
bg_static_animation_t *BG_AnimationForString(const char *name);
bg_runtime_animation_t *BG_LoadAnimForAnimIndex(uint32_t animIndex);
void BG_SetupAnimNoteTypes(bg_static_animation_table_t *table);
void BG_FinalizePlayerAnims(void);
void BG_LoadAnimTreeInstances(void);
void BG_FindAnims(void);
#if defined(WINDOWS_BEHAVIOR)
XAnim *BG_FindAnimTree(const char *treeName, qboolean errorIfMissing);
#else
script_anim_tree_ref_t BG_FindAnimTree(const char *treeName,
                                       qboolean errorIfMissing);
#endif
void BG_FindAnimTrees(void);
void BG_InitWeaponStrings(void);
void BG_ParseConditionBits(char **parse, bg_indexed_string_t *values,
                           int32_t conditionIndex,
                           bg_condition_bits_t *bits);
qboolean BG_ParseConditions(char **parse, bg_anim_script_t *script);
void BG_ParseCommands(char **parse, bg_anim_script_t *script,
                      bg_static_animation_t *animations);
void BG_AnimParseAnimScript(bg_static_animation_table_t *table,
                            bg_runtime_animation_t *runtimeAnimations,
                            int32_t *runtimeAnimationCount);

qboolean BG_EvaluateConditions(clientInfo_t *clientInfo,
                               const bg_anim_script_t *script);
bg_anim_script_t *BG_FirstValidItem(int32_t clientNum,
                                    const bg_anim_script_list_t *scriptList);
void BG_UpdateConditionValue(int32_t clientNum, int32_t conditionType,
                             int32_t value, qboolean checkConversion);
int32_t BG_GetConditionValue(clientInfo_t *clientInfo,
                             int32_t conditionType,
                             qboolean convertBitset);

int32_t BG_PlayAnim(playerState_t *player, uint32_t animationIndex,
                    uint32_t bodyPart, int32_t duration, qboolean setTimer,
                    qboolean restartSame, qboolean force);
int32_t BG_PlayAnimName(playerState_t *player, const char *animationName,
                        uint32_t bodyPart, qboolean setTimer,
                        qboolean restartSame, qboolean force);
int32_t BG_AnimScriptAnimation(const bg_anim_script_command_t *command,
                               playerState_t *player, qboolean setTimer,
                               qboolean restartSame, qboolean force);
int32_t BG_ExecuteCommand(playerState_t *player, int32_t stateIndex,
                          int32_t animGroup, qboolean restartSame);
int32_t BG_AnimScriptStateChange(playerState_t *player, int32_t fromState,
                                 int32_t toState);
int32_t BG_AnimScriptEvent(playerState_t *player, bg_anim_event_t event,
                           qboolean restartSame, qboolean force);
char *BG_GetAnimString(uint32_t animationIndex);
int32_t BG_GetAnimScriptEvent(playerState_t *player, bg_anim_event_t event);
bg_static_animation_t *BG_GetAnimationForIndex(int32_t clientNum,
                                                uint32_t animationIndex);
qboolean BG_IsCrouchingAnim(const clientInfo_t *clientInfo,
                            uint32_t animationIndex);
qboolean BG_IsProneAnim(const clientInfo_t *clientInfo,
                        uint32_t animationIndex);
void BG_AnimUpdatePlayerStateConditions(pmove_t *pmove);
void BG_AnimPlayerConditions(const entityState_t *entity,
                             clientInfo_t *clientInfo);
void BG_PlayerAnimation(const entityState_t *entity,
                        clientInfo_t *clientInfo);
void BG_LerpAngles(const vec3_t target, float maxStep, vec3_t current);
void BG_LerpOffset(const vec3_t target, float scale, vec3_t current);
void BG_SwingAngles(float target, float deadband, float maxDeviation,
                    float stepScale, float *angle, qboolean *active);
void BG_PlayerAngles(const entityState_t *entity, clientInfo_t *clientInfo);
extern const char *const BG_ControllerTagNames[
    CLIENT_INFO_SPINE_CONTROL_COUNT];
void BG_Player_DoControllersInternal(clientInfo_t *clientInfo,
                                     const entityState_t *entity,
                                     vec3_t output[8]);
void BG_Player_DoControllers(void *dobjOwner, const entityState_t *entity,
                             uint32_t *partBits,
                             clientInfo_t *clientInfo);
void BG_PlayerAnimation_VerifyAnim(XAnimTree *animTree,
                                   bg_anim_slot_t *slot);
void BG_SetNewAnimation(clientInfo_t *clientInfo, bg_anim_slot_t *slot,
                        uint32_t animationWord,
                        qboolean forceDeathRestart);
void BG_RunLerpFrameRate(clientInfo_t *clientInfo, bg_anim_slot_t *slot,
                         uint32_t animationWord,
                         const entityState_t *entity);

#endif
