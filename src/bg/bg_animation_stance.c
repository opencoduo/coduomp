#include "bg_animation.h"

#include <stdint.h>

/*
 * The original Windows cgame/game bodies agree after their module-owned error
 * edge is normalized:
 *
 *   uo_cgame_mp_x86.dll 0x30003a00 and 0x30003a40
 *   uo_game_mp_x86.dll  0x200039e0 and 0x20003a20
 *
 * Linux retains the canonical names and complete two-argument source ABI at
 * RVA 0x0001cdac and 0x0001ce04. Its bodies pass clientInfo->clientNum as the
 * otherwise-unused first argument to BG_GetAnimationForIndex. The Windows
 * optimizer removes that read and carries animationIndex in EAX, which is an
 * optimized realization of the same source contract, not a different API.
 * Supporting Mac cgame/game retain both names with matching 0x3c bodies.
 */
qboolean BG_IsCrouchingAnim(const clientInfo_t *clientInfo,
                            uint32_t animationIndex)
{
    bg_static_animation_t *animation = BG_GetAnimationForIndex(
        clientInfo->clientNum, animationIndex & ~ANIM_TOGGLEBIT);

    return (animation->stateFlagsLowByte & BG_ANIM_CROUCH_STATE_MASK) != 0;
}

qboolean BG_IsProneAnim(const clientInfo_t *clientInfo,
                        uint32_t animationIndex)
{
    bg_static_animation_t *animation = BG_GetAnimationForIndex(
        clientInfo->clientNum, animationIndex & ~ANIM_TOGGLEBIT);

    return (animation->stateFlags & BG_ANIM_PRONE_STATE_MASK) != 0;
}
