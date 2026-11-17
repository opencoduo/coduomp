// Source: uo_cgame_mp_x86.dll 0x3003e860..0x3003e884

#include "client/cgame/client_recovered.h"

/* Blend out the goal weights of an anim's children. The original wrapper
 * forwards tree (stack), animIndex (low word of stack) and blendTime (stack)
 * with syscall 0x8e (CG_XANIM_CLEAR_CHILD_GOAL_WEIGHTS). */
void trap_XAnimClearChildGoalWeights(XAnimTree *tree, uint16_t animIndex, float blendTime)
{
    cgame_syscall(CG_XANIM_CLEAR_CHILD_GOAL_WEIGHTS, (intptr_t)tree, animIndex, CG_FloatBits(blendTime));
}
