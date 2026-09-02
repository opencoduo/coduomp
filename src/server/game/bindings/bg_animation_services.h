#ifndef GAME_BG_ANIMATION_SERVICES_H
#define GAME_BG_ANIMATION_SERVICES_H

/* Game-side ownership of the dependencies used by shared BG animation code. */
#include "server/game/recovered_game.h"
#include "server/game/game_functions.h"
#include "server/game/scr_vm.h"
#include "server/game/g_syscalls.h"

void Com_Error(errorParm_t code, const char *format, ...);
void G_Error(const char *format, ...);

/* NOT_FROM_ORIGINAL_SOURCE: the game BG_LoadAnimTreeInstances body ends after
 * the common client-info loop and therefore has no module-owned suffix. */
static inline void bg_compat_load_additional_anim_tree_instances(XAnim *masterTree)
{
    (void)masterTree;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the game-owned BG frame clock to the
 * shared original angle routines. */
static inline int32_t bg_compat_animation_frame_time(void)
{
    return bg.frameTime;
}

/* NOT_FROM_ORIGINAL_SOURCE: the controller sine diagnostic belongs only to
 * cgame; both original game-module bodies proceed through the ordinary view-
 * angle path. */
static inline int32_t bg_compat_controller_debug_value(void)
{
    return 0;
}

static inline int32_t bg_compat_controller_debug_time(void)
{
    return 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game entity/DObj helper boundary to
 * the shared controller application contract. */
static inline void bg_compat_controller_set_control_tag_angles(void *dobjOwner, uint32_t *partBits, const char *tagName,
                                                               const vec3_t angles)
{
    (void)G_DObjSetControlTagAngles((gentity_t *)dobjOwner, partBits, tagName, angles);
}

static inline void bg_compat_controller_set_local_tag(void *dobjOwner, uint32_t *partBits, const char *tagName, const vec3_t offset,
                                                      const vec3_t angles)
{
    (void)G_DObjSetLocalTag((gentity_t *)dobjOwner, partBits, tagName, offset, angles);
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the game-owned swing cvar value to the
 * shared original player-angle routine. */
static inline float bg_compat_animation_swing_speed(void)
{
    return bg_swingSpeed.value;
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game-owned BG clocks and diagnostic
 * cvar used by the shared animation-slot runtime. */
static inline int32_t bg_compat_animation_time(void)
{
    return bg.time;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the game-owned animation sampling clock. */
static inline int32_t bg_compat_animation_sample_time(void)
{
    return bg.levelFrameTime;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the game animation diagnostic selector. */
static inline int32_t bg_compat_animation_debug_value(void)
{
    return bg_debugAnim.integer;
}

/* NOT_FROM_ORIGINAL_SOURCE: the game syscall veneers already use packed XAnim
 * identifiers, so these adapters retain their exact public contracts. */
static inline float bg_compat_animation_get_weight(XAnimTree *tree, uint32_t anim)
{
    return trap_XAnimGetWeight(tree, anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game packed XAnim time query. */
static inline float bg_compat_animation_get_time(XAnimTree *tree, uint32_t anim)
{
    return trap_XAnimGetTime(tree, anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game packed primitive query. */
static inline qboolean bg_compat_animation_is_primitive(uint32_t anim)
{
    return trap_XAnimIsPrimitive(anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game animation-length trap. */
static inline int32_t bg_compat_animation_get_length(XAnim *anims, uint16_t animIndex)
{
    return trap_XAnimGetLength(anims, animIndex);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game clear-goal trap. */
static inline void bg_compat_animation_clear_goal_weight(XAnimTree *tree, uint32_t anim, float blendTime)
{
    trap_XAnimClearGoalWeight(tree, anim, blendTime);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game animation-time trap. */
static inline void bg_compat_animation_set_time(XAnimTree *tree, uint32_t anim, float time)
{
    trap_XAnimSetTime(tree, anim, time);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game animation-rate trap. */
static inline void bg_compat_animation_set_rate(XAnimTree *tree, uint32_t anim, float rate)
{
    trap_XAnimSetAnimRate(tree, anim, rate);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the platform-specific slot range error. */
static inline void bg_compat_update_anim_slot_range_error(int32_t entryCount, uint32_t animIndex)
{
#if defined(WINDOWS_BEHAVIOR)
    G_Error("\x15Player animation index out of range (%i): %i", entryCount, animIndex);
#else
    Com_Error(ERR_DROP, "\x15Player animation index out of range (%i): %i", entryCount, animIndex);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the platform-specific death-loop error. */
static inline void bg_compat_death_animation_loop_error(const char *name)
{
#if defined(WINDOWS_BEHAVIOR)
    G_Error("death animation '%s' is looping", name);
#else
    Com_Error(ERR_DROP, "death animation '%s' is looping", name);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the module-specific animation-script
 * filesystem trap boundary used by the shared parser body. */
static inline int32_t bg_compat_anim_script_open(const char *path, int32_t *handle)
{
    return trap_FS_FOpenFile(path, handle, FS_READ);
}

/* NOT_FROM_ORIGINAL_SOURCE: shared-parser adapter for the game read trap. */
static inline void bg_compat_anim_script_read(void *buffer, int32_t length, int32_t handle)
{
    trap_FS_Read(buffer, length, handle);
}

/* NOT_FROM_ORIGINAL_SOURCE: shared-parser adapter for the game close trap. */
static inline void bg_compat_anim_script_close(int32_t handle)
{
    trap_FS_FCloseFile(handle);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game XAnim tree-size veneer. */
static inline int32_t bg_compat_xanim_get_tree_size(XAnim *anims)
{
    return trap_XAnimGetAnimTreeSize(anims);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game packed primitive query. */
static inline qboolean bg_compat_xanim_is_primitive(uint16_t animTree, uint16_t animIndex)
{
    const uint32_t anim = ((uint32_t)animTree << 16) | animIndex;
    return trap_XAnimIsPrimitive(anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game packed animation-name query. */
static inline const char *bg_compat_xanim_get_anim_name(uint16_t animTree, uint16_t animIndex)
{
    const uint32_t anim = ((uint32_t)animTree << 16) | animIndex;
    return trap_XAnimGetAnimName(anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game packed relative-delta query. */
static inline void bg_compat_xanim_get_rel_delta(uint16_t animTree, uint16_t animIndex, vec3_t rotationDelta, vec3_t moveDelta)
{
    const uint32_t anim = ((uint32_t)animTree << 16) | animIndex;
    trap_XAnimGetRelDelta(anim, rotationDelta, moveDelta, 0.0f, 1.0f);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the game packed loop query. */
static inline qboolean bg_compat_xanim_is_looped(uint16_t animTree, uint16_t animIndex)
{
    const uint32_t anim = ((uint32_t)animTree << 16) | animIndex;
    return trap_XAnimIsLooped(anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original game-module diagnostic edge.
 * Windows calls the local format-only G_Error body; Linux calls Com_Error with
 * ERR_DROP. The existing behavior mode selects which server contract to model. */
static inline void bg_compat_anim_index_error(uint32_t animIndex, int32_t entryCount)
{
#if defined(WINDOWS_BEHAVIOR)
    G_Error("\x15Player animation index %i out of 0 to %i range", animIndex, entryCount);
#else
    Com_Error(ERR_DROP, "\x15Player animation index %i out of 0 to %i range", animIndex, entryCount);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the module-specific original error edge.
 * Windows uses G_Error; Linux uses Com_Error(ERR_DROP, ...). */
static inline void bg_compat_get_animation_index_error(void)
{
#if defined(WINDOWS_BEHAVIOR)
    G_Error("\x15"
            "BG_GetAnimationForIndex: index out of bounds");
#else
    Com_Error(ERR_DROP, "\x15"
                        "BG_GetAnimationForIndex: index out of bounds");
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the module-specific fatal edge used by
 * BG_AnimUpdatePlayerStateConditions. */
static inline void bg_compat_player_state_vehicle_type_error(int32_t vehicleType)
{
#if defined(WINDOWS_BEHAVIOR)
    G_Error("\x15"
            "BG_AnimUpdatePlayerStateConditions: Vehicle type unknown: %i",
            vehicleType);
#else
    Com_Error(ERR_DROP,
              "\x15"
              "BG_AnimUpdatePlayerStateConditions: Vehicle type unknown: %i",
              vehicleType);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the same platform split for the entity
 * condition updater's distinct diagnostic text. */
static inline void bg_compat_player_entity_vehicle_type_error(int32_t vehicleType)
{
#if defined(WINDOWS_BEHAVIOR)
    G_Error("\x15"
            "BG_AnimPlayerConditions: Vehicle type unknown: %i",
            vehicleType);
#else
    Com_Error(ERR_DROP,
              "\x15"
              "BG_AnimPlayerConditions: Vehicle type unknown: %i",
              vehicleType);
#endif
}

#endif
