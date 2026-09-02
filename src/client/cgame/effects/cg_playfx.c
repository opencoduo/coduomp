#include "../client_recovered.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x30022720..0x30022772
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30022720_30022772.mcode
//
// CG_PlayFx: play the effect named by self->currentState.eventParm at self->lerpOrigin.
//
// The .mcode's size-matched "irand" guess is rejected: this function performs no
// randomization. Its out-of-range diagnostic string
//   "ERROR: CG_PlayFx called with invalid effect id %i\n"  (0x30077150)
// names it directly, and its body is the effect-play tail shared with the culling
// variant at 0x30021a30 (same cg_effectDefs table, same 0xe7/0xe8 play traps, same
// error string).
//
// Register-argument ABI (proven at the call sites in 0x30022810): the effect
// object arrives in EDX (`self`) and the optional direction vector in ECX (`dir`);
// callers pass ECX=0 for the origin-only case (0x300234f5) and a stack axis/vector
// pointer for the oriented case (0x30023515). The function takes no stack
// arguments and returns void (each path is a bare RET with no return value set).
//
// Instruction map:
//   30022720 MOV EAX,[EDX+0xa4]      -> fxId = self->currentState.eventParm           (signed load)
//   30022726 TEST EAX,EAX
//   30022728 JLE  0x30022763         -> if (fxId <= 0) goto invalid  (signed)
//   3002272a CMP  EAX,0x50
//   3002272d JGE  0x30022763         -> if (fxId >= 80) goto invalid (signed)
//   3002272f MOV  EAX,[EAX*4+table]  -> handle = cg_effectDefs[fxId]
//   30022736 ADD  EDX,0x208          -> origin = &self->lerpOrigin
//   3002273c TEST ECX,ECX
//   3002273e JZ   0x30022752         -> if (dir == NULL) origin-only path
//   30022740 push ECX/EDX/EAX/0xe8   -> trap(0xe8, handle, origin, dir)
//   30022748 CALL *cgame_syscall     ;  ADD ESP,0x10 ; RET
//   30022752 push EDX/EAX/0xe7       -> trap(0xe7, handle, origin)
//   30022759 CALL *cgame_syscall     ;  ADD ESP,0x0c ; RET
//   30022763 invalid: push EAX(fxId), push str; CALL Com_PrintMessage; ADD ESP,8; RET

void CG_PlayFx(centity_t *self, const vec_t *dir)
{
    int32_t fxId = self->currentState.eventParm;

    if (fxId <= 0 || fxId >= 80) {
        Com_PrintMessage("ERROR: CG_PlayFx called with invalid effect id %i\n", fxId);
        return;
    }

    uint32_t handle = cg_effectDefs[fxId];

    if (dir != NULL) {
        cgame_syscall(CG_PLAY_EFFECT_ORIENTED, coduo_int32_from_bits(handle), (intptr_t)self->lerpOrigin, (intptr_t)dir);
    } else {
        cgame_syscall(CG_PLAY_EFFECT_ORIGIN, coduo_int32_from_bits(handle), (intptr_t)self->lerpOrigin);
    }
}
