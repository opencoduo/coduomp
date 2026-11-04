#include "bg_animation.h"
#include "bg_animation_services.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "math/q_math.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

enum {
    BG_ANIM_RESET_BLEND_TIME_MS = 150,
    BG_ANIM_NO_ANIMATION_BLEND_TIME_MS = 200,
    BG_ANIM_TRANSITION_PAD_MS = 400,
    BG_ANIM_BLEND_MOVING_IN_MS = 120,
    BG_ANIM_BLEND_MOVING_OUT_MS = 250,
    BG_ANIM_BLEND_IDLE_MS = 170,
    BG_ANIM_DEFAULT_CYCLE_MS = 1000,
    BG_ANIM_LENGTH_PAD_MS = 200,
    BG_ANIM_FAST_MOVE_SPEED_MIN = 151,
    BG_ANIM_RATE_SPEED_BASE = 20
};

#define BG_ANIM_MILLISECONDS_TO_SECONDS 0.001f
#define BG_ANIM_RATE_MIN 0.1f
#define BG_ANIM_RATE_ZERO_EPSILON 0.01f
#define BG_ANIM_RATE_FAST_THRESHOLD 2.0f
#define BG_ANIM_RATE_CAP_SLOW_MOVE 3.0f
#define BG_ANIM_RATE_CAP_FAST_MOVE 2.0f
#define BG_ANIM_RATE_CAP_VERTICAL_MOVE 4.0f
#define BG_ANIM_RATE_SPEED_RANGE 130.0f
#define BG_ANIM_RATE_SPEED_RECIPROCAL (1.0f / BG_ANIM_RATE_SPEED_RANGE)

/* The authoritative Windows cgame/game bodies are instruction-identical after
 * module addresses are normalized. Linux retains the canonical symbols at
 * BG_PlayerAnimation_VerifyAnim RVA 0x1d922, BG_SetNewAnimation RVA 0x1ce5f,
 * and BG_RunLerpFrameRate RVA 0x1d56d. Supporting Mac cgame/game preserve the
 * same names and call cluster. */

/* NOT_FROM_ORIGINAL_SOURCE: packed XAnim word used by the shared source. */
static uint32_t bg_compat_make_xanim(uint16_t tree, uint16_t animation)
{
    return ((uint32_t)tree << 16) | animation;
}

/* NOT_FROM_ORIGINAL_SOURCE: host-safe animation-offset accessor. */
static bg_static_animation_t *bg_compat_slot_animation(
    const bg_anim_slot_t *slot)
{
    if (slot->animationOffset == 0) {
        return NULL;
    }
#if UINTPTR_MAX == UINT32_MAX
    return (bg_static_animation_t *)(uintptr_t)slot->animationOffset;
#else
    return (bg_static_animation_t *)(void *)(
        (uint8_t *)bgAnimStaticTable + slot->animationOffset);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: host-safe animation-offset mutator. */
static void bg_compat_set_slot_animation(bg_anim_slot_t *slot,
                                          bg_static_animation_t *animation)
{
    if (animation == NULL) {
        slot->animationOffset = 0;
    } else {
#if UINTPTR_MAX == UINT32_MAX
        slot->animationOffset = (uint32_t)(uintptr_t)animation;
#else
        slot->animationOffset = (uint32_t)(
            (uint8_t *)animation - (uint8_t *)bgAnimStaticTable);
#endif
    }
}

/* 0x1d922 BG_PlayerAnimation_VerifyAnim */
/* VERIFIED_DECOMPILER(0x1d922, 2d922_FUN_0002d922.c, VERIFY-ANIM-PACKET-2026-06-17): DATAFLOW_VERIFIED - anims index packing with slot low word, toggle-bit mask, zero-weight compare, and slot word/offset/blend-time stores checked. */
void BG_PlayerAnimation_VerifyAnim(XAnimTree *animTree,
                                   bg_anim_slot_t *slot)
{
    const uint16_t animsIndex =
        Scr_GetAnimsIndex(bgAnimStaticTable->animTreeHandle);

    if (slot->animationWord != 0) {
        const uint32_t anim =
            bg_compat_make_xanim(animsIndex, (uint16_t)slot->animationWord) &
            ~ANIM_TOGGLEBIT;

        if (bg_compat_animation_get_weight(animTree, anim) == 0.0f) {
            slot->animationWord = 0;
            bg_compat_set_slot_animation(slot, NULL);
            slot->blendTime = BG_ANIM_RESET_BLEND_TIME_MS;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original BG_SetNewAnimation (0x1ce5f); no standalone original body. */
static qboolean bg_compat_anim_slot_is_legs(const clientInfo_t *ci,
                                  const bg_anim_slot_t *slot)
{
    return slot == (const bg_anim_slot_t *)&ci->legsYawAngle;
}

/* NOT_FROM_ORIGINAL_SOURCE: slot XAnim word helper extracted from 0x1ce5f/0x1d56d. */
/* VERIFIED_DECOMPILER(0x1ce5f, 2ce5f_FUN_0002ce5f.c, VERIFY-ANIM-CONTROLLERS-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against CONCAT22(animTreeIndex, animWord low16) and 0xfffffdff toggle-bit mask. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original BG_SetNewAnimation (0x1ce5f); no standalone original body. */
static uint32_t bg_compat_anim_slot_xanim(uint16_t animTreeIndex,
                                          uint32_t animationWord)
{
    return bg_compat_make_xanim(animTreeIndex, (uint16_t)animationWord) &
           ~ANIM_TOGGLEBIT;
}

/* NOT_FROM_ORIGINAL_SOURCE: last-origin copy helper extracted from 0x1d56d. */
/* VERIFIED_DECOMPILER(0x1d56d, 2d56d_FUN_0002d56d.c, VERIFY-ANIM-CONTROLLERS-2026-06-17): DATAFLOW_VERIFIED - source-only helper checked against three stores from fourth argument +0x18/+0x1c/+0x20, i.e. ent->s.pos.trBase. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original BG_RunLerpFrameRate (0x1d56d); no standalone original body. */
static void bg_compat_copy_anim_slot_origin(bg_anim_slot_t *slot,
                                             const entityState_t *entity)
{
    slot->lastOrigin[0] = entity->pos.trBase[0];
    slot->lastOrigin[1] = entity->pos.trBase[1];
    slot->lastOrigin[2] = entity->pos.trBase[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: loop phase helper extracted from 0x1ce5f. */
/* VERIFIED_DECOMPILER(0x1ce5f, 2ce5f_FUN_0002ce5f.c, VERIFY-ANIM-CONTROLLERS-2026-06-23): DATAFLOW_VERIFIED - source-only helper checked against clientNum * 0.36 plus bg.time % duration / duration minus x87 fistp RC=truncate integral phase. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of original BG_SetNewAnimation (0x1ce5f); no standalone original body. */
static float bg_compat_initial_loop_time(const clientInfo_t *ci, int duration)
{
    /* 0x1d15e..0x1d17a: one 80-bit x87 chain with a single float rounding at
     * the phase store; separate framePhase/cyclePhase temporaries would round
     * three times. All operands feed the chain via fild (int->80, exact). */
    const double phase =
        (double)(bg_compat_animation_time() % duration) / duration +
        (double)ci->clientNum * 0.36f;
    const int32_t phaseInteger = coduo_fp_to_i32_extended((long double)phase);
    return (float)(phase - phaseInteger);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve each compiler's proved order for the
 * stance queries whose results decide whether the transition timer restarts. */
static qboolean bg_compat_animation_stance_changed(
    const clientInfo_t *ci, uint32_t oldAnimation, uint32_t newAnimation)
{
    qboolean oldValue;
    qboolean newValue;

    oldValue = BG_IsCrouchingAnim(ci, oldAnimation);
    newValue = BG_IsCrouchingAnim(ci, newAnimation);
    if (oldValue != newValue) {
        return qtrue;
    }
    oldValue = BG_IsProneAnim(ci, oldAnimation);
    newValue = BG_IsProneAnim(ci, newAnimation);
    return oldValue != newValue;
}

/* 0x1ce5f BG_SetNewAnimation */
/* VERIFIED_DECOMPILER(0x1ce5f, 2ce5f_FUN_0002ce5f.c, VERIFY-ANIM-CONTROLLERS-2026-06-17): DATAFLOW_VERIFIED - old slot state capture, legs-slot identity, masked index/range error, slot pointer/offset cache, blend minima, crouch/prone transition timer, moving loop start phase, old-weight clear, zero-animation torso blend handles, torso dirty state, death/non-death weight paths, blend-handle weights, and debug print checked. */
void BG_SetNewAnimation(clientInfo_t *ci, bg_anim_slot_t *slot,
                        uint32_t animationWord,
                        qboolean forceDeathRestart)
{
    bg_static_animation_t *oldAnimation = bg_compat_slot_animation(slot);
    const uint32_t oldAnimationWord = slot->animationWord;
    const qboolean isLegsSlot = bg_compat_anim_slot_is_legs(ci, slot);
    const uint32_t animationIndex = animationWord & ~ANIM_TOGGLEBIT;
    bg_static_animation_t *currentAnimation = NULL;
    float startTime = 0.0f;

    slot->animationWord = animationWord;

    if ((uint32_t)bgAnimStaticTable->entryCount <= animationIndex) {
        bg_compat_update_anim_slot_range_error(bgAnimStaticTable->entryCount,
                                                animationIndex);
    }

    XAnimTree *animTree = ci->animTree;
    XAnim *anims = bgAnimStaticTable->animTreeHandle;
    const uint16_t animTreeIndex = Scr_GetAnimsIndex(anims);
    if (animationIndex == 0) {
        bg_compat_set_slot_animation(slot, NULL);
        slot->blendTime = BG_ANIM_NO_ANIMATION_BLEND_TIME_MS;
    } else {
        currentAnimation = &bgAnimStaticTable->entries[animationIndex];
        bg_compat_set_slot_animation(slot, currentAnimation);
        slot->blendTime = currentAnimation->blendTime;

        if (isLegsSlot && bg_compat_animation_stance_changed(
                              ci, oldAnimationWord, animationIndex)) {
            ci->animTransitionTime =
                coduo_int32_from_bits((uint32_t)bg_compat_animation_time() + BG_ANIM_TRANSITION_PAD_MS);
        }
    }

    if (oldAnimation == NULL && isLegsSlot) {
        slot->blendTime = 0;
    } else {
        int minimumBlendTime = -1;

        if (currentAnimation == NULL || slot->blendTime < 1) {
            if (currentAnimation == NULL || currentAnimation->moveSpeed == 0) {
                minimumBlendTime =
                    (oldAnimation == NULL || oldAnimation->moveSpeed == 0) ?
                        BG_ANIM_BLEND_IDLE_MS :
                        BG_ANIM_BLEND_MOVING_OUT_MS;
            } else {
                minimumBlendTime = BG_ANIM_BLEND_MOVING_IN_MS;
            }
        }

        const int transitionBlendTime =
            coduo_int32_from_bits((uint32_t)ci->animTransitionTime - (uint32_t)bg_compat_animation_time());
        if (minimumBlendTime < transitionBlendTime) {
            minimumBlendTime = transitionBlendTime;
        }

        if (slot->blendTime < minimumBlendTime) {
            slot->blendTime = minimumBlendTime;
        }
    }

    if (currentAnimation != NULL && currentAnimation->moveSpeed != 0) {
        const uint32_t newAnim =
            bg_compat_anim_slot_xanim(animTreeIndex, animationWord);

        if (trap_XAnimIsLooped(newAnim)) {
            const uint32_t oldAnim =
                bg_compat_anim_slot_xanim(animTreeIndex, oldAnimationWord);

            if (oldAnimation == NULL || oldAnimation->moveSpeed == 0 ||
                !trap_XAnimIsLooped(oldAnim)) {
                int duration = BG_ANIM_DEFAULT_CYCLE_MS;

                if (bg_compat_animation_is_primitive(oldAnim)) {
                    duration = coduo_int32_from_bits((uint32_t)bg_compat_animation_get_length(
                                   anims,
                                   (uint16_t)(oldAnimationWord &
                                              ~ANIM_TOGGLEBIT)) + BG_ANIM_LENGTH_PAD_MS);
                }

                startTime = bg_compat_initial_loop_time(ci, duration);
            } else {
                startTime = bg_compat_animation_get_time(animTree, oldAnim);
            }
        }
    }

    /* Convert the full signed millisecond value before scaling. Each call
     * receives a float rounded after the product, including large blend times. */
    if (oldAnimation != NULL) {
        const uint32_t oldAnim =
            bg_compat_anim_slot_xanim(animTreeIndex, oldAnimationWord);

        bg_compat_animation_clear_goal_weight(
            animTree, oldAnim,
            coduo_round_f32((WIDE_FLOAT)slot->blendTime * BG_ANIM_MILLISECONDS_TO_SECONDS));
    }

    if (animationIndex == 0) {
        if (!isLegsSlot) {
            float blendSeconds =
                coduo_round_f32((WIDE_FLOAT)slot->blendTime * BG_ANIM_MILLISECONDS_TO_SECONDS);

            trap_XAnimSetCompleteGoalWeight(
                animTree,
                bg_compat_make_xanim(
                    bgs.resolvedTorsoAnimHandle.treeIndex,
                    bgs.resolvedTorsoAnimHandle.animIndex),
                0.0f, blendSeconds,
                1.0f, 0, 0);
            blendSeconds =
                coduo_round_f32((WIDE_FLOAT)slot->blendTime * BG_ANIM_MILLISECONDS_TO_SECONDS);
            trap_XAnimSetCompleteGoalWeight(
                animTree,
                bg_compat_make_xanim(
                    bgs.resolvedLegsAnimHandle.treeIndex,
                    bgs.resolvedLegsAnimHandle.animIndex),
                1.0f, blendSeconds,
                1.0f, 0, 0);
        }

        /* Stock falls through to the common debug print even for index zero. */
        if (bg_compat_animation_debug_value() == 1) {
            Com_Printf("Anim-%s: %i, %s, (blend time) %i\n",
                       isLegsSlot ? "legs " : "torso", animationIndex,
                       bgAnimStaticTable->entries[animationIndex].name,
                       slot->blendTime);
        }
        return;
    }

    if (!isLegsSlot) {
        ci->gunHandLeft = 0;
        ci->dobjNeedsUpdate = 1;
    }

    const uint32_t newAnim =
        bg_compat_anim_slot_xanim(animTreeIndex, animationWord);

    if ((currentAnimation->flags & BG_ANIM_ENTRY_DEATH) == 0) {
        const qboolean setStartTime =
            currentAnimation->moveSpeed != 0 &&
            bg_compat_animation_get_weight(animTree, newAnim) == 0.0f;
        const float blendSeconds =
            coduo_round_f32((WIDE_FLOAT)slot->blendTime * BG_ANIM_MILLISECONDS_TO_SECONDS);

        trap_XAnimSetCompleteGoalWeight(
            animTree, newAnim, 1.0f, blendSeconds, 1.0f,
            (uint16_t)currentAnimation->usedByScript, !isLegsSlot);
        if (setStartTime) {
            bg_compat_animation_set_time(animTree, newAnim, startTime);
        }
    } else {
        if (trap_XAnimIsLooped(newAnim)) {
            bg_compat_death_animation_loop_error(currentAnimation->name);
        }

        if (!forceDeathRestart) {
            (void)trap_XAnimSetCompleteGoalWeightKnobAll(
                animTree, newAnim,
                bg_compat_make_xanim(bgs.rootAnimHandle.treeIndex,
                                     bgs.rootAnimHandle.animIndex),
                1.0f,
                0.0f, 1.0f, 0, 0);
            bg_compat_animation_set_time(animTree, newAnim, 1.0f);
        } else {
            const float blendSeconds =
                coduo_round_f32((WIDE_FLOAT)slot->blendTime * BG_ANIM_MILLISECONDS_TO_SECONDS);
            trap_XAnimSetCompleteGoalWeight(
                animTree, newAnim, 1.0f, blendSeconds, 1.0f, 0, 0);
        }
    }

    if (!isLegsSlot) {
        float blendSeconds =
            coduo_round_f32((WIDE_FLOAT)slot->blendTime * BG_ANIM_MILLISECONDS_TO_SECONDS);
        trap_XAnimSetCompleteGoalWeight(
            animTree,
            bg_compat_make_xanim(
                bgs.resolvedTorsoAnimHandle.treeIndex,
                bgs.resolvedTorsoAnimHandle.animIndex),
            1.0f, blendSeconds, 1.0f,
            (uint16_t)currentAnimation->usedByScript, 0);
        blendSeconds =
            coduo_round_f32((WIDE_FLOAT)slot->blendTime * BG_ANIM_MILLISECONDS_TO_SECONDS);
        trap_XAnimSetCompleteGoalWeight(
            animTree,
            bg_compat_make_xanim(
                bgs.resolvedLegsAnimHandle.treeIndex,
                bgs.resolvedLegsAnimHandle.animIndex),
            0.01f, blendSeconds,
            1.0f, (uint16_t)currentAnimation->usedByScript, 0);
    }

    if (bg_compat_animation_debug_value() == 1) {
        Com_Printf("Anim-%s: %i, %s, (blend time) %i\n",
                   isLegsSlot ? "legs " : "torso", animationIndex,
                   currentAnimation->name, slot->blendTime);
    }
}

/* 0x1d56d BG_RunLerpFrameRate */
/* VERIFIED_DECOMPILER(0x1d56d, 2d56d_FUN_0002d56d.c, VERIFY-ANIM-CONTROLLERS-2026-06-17): DATAFLOW_VERIFIED - old-slot vertical-motion flag capture, anim tree/index lookup, update-slot gate and death-restart flag, masked zero return, reset path, level-frame delta guard, vertical vs VectorDistance movement, animRate calculation, min/zero/fast caps, debug print arguments, and trap_XAnimSetAnimRate checked. */
void BG_RunLerpFrameRate(clientInfo_t *ci, bg_anim_slot_t *slot,
                         uint32_t animationWord,
                         const entityState_t *entity)
{
    bg_static_animation_t *slotAnimation = bg_compat_slot_animation(slot);
    const qboolean usesVerticalDelta =
        slotAnimation != NULL &&
        (slotAnimation->flags & BG_ANIM_ENTRY_VERTICAL_MOTION) != 0;
    XAnimTree *animTree = ci->animTree;
    const uint16_t animTreeIndex =
        Scr_GetAnimsIndex(bgAnimStaticTable->animTreeHandle);

    if (animationWord != slot->animationWord ||
        (bg_compat_slot_animation(slot) == NULL &&
         (animationWord & ~ANIM_TOGGLEBIT) != 0)) {
        BG_SetNewAnimation(ci, slot, animationWord,
                          (entity->eFlags >> 10) & 1u);
    }

    if ((animationWord & ~ANIM_TOGGLEBIT) == 0) {
        return;
    }

    bg_static_animation_t *animation = bg_compat_slot_animation(slot);
    const int32_t animationMoveSpeed = animation->moveSpeed;
    if (animationMoveSpeed == 0 || slot->lastUpdateTime == 0) {
        slot->animRate = 1.0f;
        slot->lastUpdateTime = bg_compat_animation_sample_time();
        bg_compat_copy_anim_slot_origin(slot, entity);
    } else if (bg_compat_animation_sample_time() != slot->lastUpdateTime) {
        WIDE_FLOAT moveDistance;

        if (usesVerticalDelta) {
            moveDistance =
                fabsl((WIDE_FLOAT)slot->lastOrigin[2] - entity->pos.trBase[2]);
        } else {
            moveDistance =
                VectorDistance(slot->lastOrigin, entity->pos.trBase);
        }

        /* Windows keeps the vertical difference, elapsed-seconds product and
         * speed quotient live in x87. VectorDistance has its own float return
         * store; the rate rounds to float only after the second division. */
        const int32_t elapsed = coduo_int32_from_bits(
            (uint32_t)bg_compat_animation_sample_time() - (uint32_t)slot->lastUpdateTime);
        const WIDE_FLOAT moveSpeed = moveDistance / ((WIDE_FLOAT)elapsed * BG_ANIM_MILLISECONDS_TO_SECONDS);

        slot->animRate = coduo_round_f32(moveSpeed / animationMoveSpeed);
        slot->lastUpdateTime = bg_compat_animation_sample_time();
        bg_compat_copy_anim_slot_origin(slot, entity);

        if (slot->animRate < BG_ANIM_RATE_MIN) {
            if (slot->animRate >= BG_ANIM_RATE_ZERO_EPSILON ||
                !usesVerticalDelta) {
                slot->animRate = BG_ANIM_RATE_MIN;
            } else {
                slot->animRate = 0.0f;
            }
        } else if (slot->animRate > BG_ANIM_RATE_FAST_THRESHOLD) {
            if ((animation->flags & BG_ANIM_ENTRY_VERTICAL_MOTION) == 0) {
                if (animation->moveSpeed >= BG_ANIM_FAST_MOVE_SPEED_MIN) {
                    slot->animRate = BG_ANIM_RATE_CAP_FAST_MOVE;
                } else if (animation->moveSpeed < BG_ANIM_RATE_SPEED_BASE) {
                    if (slot->animRate > BG_ANIM_RATE_CAP_SLOW_MOVE) {
                        slot->animRate = BG_ANIM_RATE_CAP_SLOW_MOVE;
                    }
                } else {
                    /* The scale is the binary32 reciprocal (0x3bfc0fc1).
                     * Keep the product and cap live through the comparison. */
                    const WIDE_FLOAT maxRate =
                        BG_ANIM_RATE_CAP_SLOW_MOVE -
                        (WIDE_FLOAT)(animation->moveSpeed - BG_ANIM_RATE_SPEED_BASE) *
                            BG_ANIM_RATE_SPEED_RECIPROCAL;

                    if (slot->animRate > maxRate) {
                        slot->animRate = maxRate;
                    }
                }
            } else if (slot->animRate > BG_ANIM_RATE_CAP_VERTICAL_MOVE) {
                slot->animRate = BG_ANIM_RATE_CAP_VERTICAL_MOVE;
            }
        }

        if (bg_compat_animation_debug_value() == 2) {
            Com_Printf("MoveSpeed: %s, %i, %4.4f : %1.4f\n",
                       bgAnimStaticTable->entries[animationWord &
                                             ~ANIM_TOGGLEBIT].name,
                       bgAnimStaticTable->entries[animationWord &
                                             ~ANIM_TOGGLEBIT].moveSpeed,
                       (double)moveSpeed, slot->animRate);
        }
    }

    if (slot->animationWord != 0) {
        const uint32_t anim =
            bg_compat_anim_slot_xanim(animTreeIndex, slot->animationWord);

        bg_compat_animation_set_rate(animTree, anim, slot->animRate);
    }
}
