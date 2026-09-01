#ifndef CGAME_BG_ANIMATION_SERVICES_H
#define CGAME_BG_ANIMATION_SERVICES_H

/* Client-side ownership of the dependencies used by shared BG animation
 * code. Scr_FindAnim is the original import-table wrapper, while diagnostics
 * remain implemented by this module. */
#include "client/cgame/client_recovered.h"

/* NOT_FROM_ORIGINAL_SOURCE: retain the cgame-only corpse-row suffix of
 * BG_LoadAnimTreeInstances outside the shared BG implementation. */
static inline void bg_compat_load_additional_anim_tree_instances(
    XAnim *masterTree)
{
    for (int32_t i = 0; i < PLAYER_CLONE_COUNT; ++i) {
        cg_corpseInfo[i].animTree = trap_XAnimCreateTree(masterTree);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the cgame-owned animation frame clock to
 * the shared original BG angle routines. */
static inline int32_t bg_compat_animation_frame_time(void)
{
    return coduo_int32_from_bits(cg_effectFrameTime);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the cgame-only controller diagnostic cvar
 * and clock outside the shared BG controller implementation. */
static inline int32_t bg_compat_controller_debug_value(void)
{
    return cg_debugProneCheck.integer;
}

static inline int32_t bg_compat_controller_debug_time(void)
{
    return coduo_int32_from_bits(cg_effectTime);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize cgame's DObj-handle trap boundary to
 * the shared controller application contract. */
static inline void bg_compat_controller_set_control_tag_angles(
    void *dobjOwner, uint32_t *partBits, const char *tagName,
    const vec3_t angles)
{
    const int32_t boneIndex = (int32_t)cgame_syscall(
        CG_DOBJ_GET_BONE_INDEX, (intptr_t)dobjOwner, tagName);

    if (boneIndex < 0) {
        return;
    }
    if (cgame_syscall(CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX,
                      (intptr_t)dobjOwner, (intptr_t)partBits,
                      boneIndex) == 0) {
        return;
    }
    CG_DObjSetLocalTagInternal(dobjOwner, boneIndex, angles, vec3_origin);
}

/* NOT_FROM_ORIGINAL_SOURCE: tag_origin companion for the cgame trap ABI. */
static inline void bg_compat_controller_set_local_tag(
    void *dobjOwner, uint32_t *partBits, const char *tagName,
    const vec3_t offset, const vec3_t angles)
{
    const int32_t boneIndex = (int32_t)cgame_syscall(
        CG_DOBJ_GET_BONE_INDEX, (intptr_t)dobjOwner, tagName);

    if (boneIndex < 0) {
        return;
    }
    if (cgame_syscall(CG_DOBJ_SET_ROT_TRANS_INDEX,
                      (intptr_t)dobjOwner, (intptr_t)partBits,
                      boneIndex) == 0) {
        return;
    }
    CG_DObjSetLocalTagInternal(dobjOwner, boneIndex, angles, offset);
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the cgame-owned swing cvar value to the
 * shared original BG player-angle routine. */
static inline float bg_compat_animation_swing_speed(void)
{
    return bg_swingSpeed_vmCvar.value;
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the two module-owned BG clocks used by
 * the animation-slot runtime. */
static inline int32_t bg_compat_animation_time(void)
{
    return coduo_int32_from_bits(cg_effectTime);
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the cgame-owned animation sampling clock. */
static inline int32_t bg_compat_animation_sample_time(void)
{
    return cg_effectAnimTime;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the cgame animation diagnostic selector. */
static inline int32_t bg_compat_animation_debug_value(void)
{
    return cg_debuganim_vmCvar.integer;
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize packed game XAnim identifiers to the
 * cgame trap ABI, whose wrappers consume the low animation word. */
static inline float bg_compat_animation_get_weight(XAnimTree *tree,
                                                    uint32_t anim)
{
    return trap_XAnimGetWeight(tree, (uint16_t)anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame low-word XAnim time trap. */
static inline float bg_compat_animation_get_time(XAnimTree *tree,
                                                  uint32_t anim)
{
    return trap_XAnimGetTime(tree, (uint16_t)anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame packed primitive query. */
static inline qboolean bg_compat_animation_is_primitive(uint32_t anim)
{
    return (qboolean)cgame_syscall(CG_XANIM_IS_PRIMITIVE,
                                   (uint16_t)(anim >> 16),
                                   (uint16_t)anim);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame animation-length trap. */
static inline int32_t bg_compat_animation_get_length(XAnim *anims,
                                                      uint16_t animIndex)
{
    return (int32_t)cgame_syscall(CG_XANIM_GET_LENGTH, (intptr_t)anims,
                                  animIndex);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame clear-goal trap. */
static inline void bg_compat_animation_clear_goal_weight(
    XAnimTree *tree, uint32_t anim, float blendTime)
{
    (void)cgame_syscall(CG_XANIM_CLEAR_GOAL_WEIGHT, (intptr_t)tree,
                        (uint16_t)anim, CG_FloatBits(blendTime));
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame animation-time trap. */
static inline void bg_compat_animation_set_time(XAnimTree *tree,
                                                 uint32_t anim, float time)
{
    (void)cgame_syscall(CG_XANIM_SET_TIME, (intptr_t)tree, (uint16_t)anim,
                        CG_FloatBits(time));
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame animation-rate trap. */
static inline void bg_compat_animation_set_rate(XAnimTree *tree,
                                                 uint32_t anim, float rate)
{
    (void)cgame_syscall(CG_XANIM_SET_ANIM_RATE, (intptr_t)tree,
                        (uint16_t)anim, CG_FloatBits(rate));
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the cgame diagnostic edges of the shared
 * animation-slot runtime. */
static inline void bg_compat_update_anim_slot_range_error(
    int32_t entryCount, uint32_t animIndex)
{
    Com_Error(ERR_DROP, "\x15Player animation index out of range (%i): %i",
              entryCount, animIndex);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the cgame death-animation fatal edge. */
static inline void bg_compat_death_animation_loop_error(const char *name)
{
    Com_Error(ERR_DROP, "death animation '%s' is looping", name);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the module-specific animation-script
 * filesystem trap boundary used by the shared parser body. */
static inline int32_t bg_compat_anim_script_open(const char *path,
                                                 int32_t *handle)
{
    return (int32_t)cgame_syscall(CG_FS_FOPEN_FILE, path, handle, FS_READ);
}

/* NOT_FROM_ORIGINAL_SOURCE: shared-parser adapter for the client read trap. */
static inline void bg_compat_anim_script_read(void *buffer, int32_t length,
                                               int32_t handle)
{
    (void)cgame_syscall(CG_FS_READ, buffer, length, handle);
}

/* NOT_FROM_ORIGINAL_SOURCE: shared-parser adapter for the client close trap. */
static inline void bg_compat_anim_script_close(int32_t handle)
{
    (void)cgame_syscall(CG_FS_FCLOSE_FILE, handle);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame XAnim tree-size trap used by
 * the shared original finalizer. */
static inline int32_t bg_compat_xanim_get_tree_size(XAnim *anims)
{
    return (int32_t)cgame_syscall(CG_XANIM_GET_ANIM_TREE_SIZE, anims);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame two-word primitive query. */
static inline qboolean bg_compat_xanim_is_primitive(uint16_t animTree,
                                                    uint16_t animIndex)
{
    return (qboolean)cgame_syscall(CG_XANIM_IS_PRIMITIVE,
                                   (int32_t)animTree,
                                   (uint32_t)animIndex);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame two-word animation-name query. */
static inline const char *bg_compat_xanim_get_anim_name(uint16_t animTree,
                                                        uint16_t animIndex)
{
    return (const char *)(intptr_t)cgame_syscall(
        CG_XANIM_GET_ANIM_NAME, (int32_t)animTree, (uint32_t)animIndex);
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame relative-delta trap. */
static inline void bg_compat_xanim_get_rel_delta(
    uint16_t animTree, uint16_t animIndex, vec3_t rotationDelta,
    vec3_t moveDelta)
{
    (void)cgame_syscall(CG_XANIM_GET_REL_DELTA, (int32_t)animTree,
                        (uint32_t)animIndex, (intptr_t)rotationDelta,
                        (intptr_t)moveDelta, 0, CG_FloatBits(1.0f));
}

/* NOT_FROM_ORIGINAL_SOURCE: normalize the cgame two-word loop query. */
static inline qboolean bg_compat_xanim_is_looped(uint16_t animTree,
                                                 uint16_t animIndex)
{
    return (qboolean)cgame_syscall(CG_XANIM_IS_LOOPED_BY_TREE_INDEX,
                                   (int32_t)animTree,
                                   (uint32_t)animIndex);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the client finalizer's direct
 * Com_Error(ERR_DROP, ...) dependency edge. */
static inline void bg_compat_anim_index_error(uint32_t animIndex,
                                              int32_t entryCount)
{
    Com_Error(ERR_DROP, "\x15Player animation index %i out of 0 to %i range",
              animIndex, entryCount);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the client accessor's direct
 * Com_Error(ERR_DROP, ...) edge. */
static inline void bg_compat_get_animation_index_error(void)
{
    Com_Error(ERR_DROP,
              "\x15" "BG_GetAnimationForIndex: index out of bounds");
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the client player-state condition
 * updater's direct Com_Error edge. */
static inline void bg_compat_player_state_vehicle_type_error(
    int32_t vehicleType)
{
    Com_Error(
        ERR_DROP,
        "\x15" "BG_AnimUpdatePlayerStateConditions: Vehicle type unknown: %i",
        vehicleType);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the client entity-condition updater's
 * direct Com_Error edge. */
static inline void bg_compat_player_entity_vehicle_type_error(
    int32_t vehicleType)
{
    Com_Error(ERR_DROP,
              "\x15" "BG_AnimPlayerConditions: Vehicle type unknown: %i",
              vehicleType);
}

#endif
