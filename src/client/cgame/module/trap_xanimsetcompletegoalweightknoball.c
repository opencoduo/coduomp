#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3003e800..0x3003e852
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e800_3003e852.mcode
//
// trap_XAnimSetCompleteGoalWeightKnobAll: cgame system-call wrapper for trap id
// 0x8d (141). It forwards an XAnimTree pointer, animation and knob indices,
// three raw float payloads, a notify name, and the restart flag. The Windows
// wrapper zero-extends the low words of anim, knob, and notifyName before the
// call; the other five payloads remain full dwords. The recovered engine
// dispatcher independently proves this exact service and argument order.

int32_t trap_XAnimSetCompleteGoalWeightKnobAll(XAnimTree *tree, uint32_t anim, uint32_t knob, float weight, float blendTime, float rate,
                                               uint16_t notifyName, qboolean restart)
{
    return (int32_t)cgame_syscall(CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL, (intptr_t)tree, (uint16_t)anim, (uint16_t)knob,
                                  CG_FloatBits(weight), CG_FloatBits(blendTime), CG_FloatBits(rate), notifyName, restart);
}
