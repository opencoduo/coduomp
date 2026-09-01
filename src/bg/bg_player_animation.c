// Source: uo_cgame_mp_x86.dll 0x30005860..0x300058ef
//         uo_game_mp_x86.dll  0x20005690..0x20005723
//         game.mp.uo.i386.so  RVA 0x0001f462..0x0001f556
//
// BG_PlayerAnimation — per-part update for a shared client animation/DObj row.
//
// Naming: the .mcode header guesses "VectorNormalize2D" purely by matching the
// 0x8f window size against a game_mp_uo corpus name. That is rejected: this
// function does no floating-point / vector math at all — it makes six calls and
// only orchestrates sub-updates over a large stateful record (a per-model table
// entry, stride 0x4d0, indexed by DObj model-part handlers). The Mac
// BG_PlayerAnimation body has the identical six-call structure: BG_PlayerAngles,
// BG_AnimPlayerConditions, two BG_PlayerAnimation_VerifyAnim calls, and two
// BG_RunLerpFrameRate calls. That fingerprint resolves the source name.
//
// ABI (register-argument client convention, MSVC-optimized):
//   `self` (the 0x4d0-stride table entry) arrives in EBX and is never reloaded
//   from a stack slot — the two known callers (0x300343e0, 0x300346c0) both set
//   EBX to the table entry before the CALL. `renderEntity` is the single stack
//   argument, read at [ESP+0x8] after the prologue PUSH EBP (0x30005861). The
//   callees likewise take register arguments; the caller cleans the pushed stack
//   arguments (ADD ESP,0x10 at 0x3000589b and ADD ESP,0x8 at 0x300058e8), which
//   is a calling-convention detail, not source-level behavior.
//
// The Windows cgame/game instruction streams differ only in register allocation
// at the final two calls. Linux exposes a three-argument ABI, but never reads
// its first gentity_t pointer; its second and third arguments are exactly the
// entityState_t and clientInfo_t consumed here. The shared signature retains
// only those two behavior-bearing inputs. Every call site is module-internal.

#include "bg_animation.h"

void BG_PlayerAnimation(const entityState_t *renderEntity,
                        clientInfo_t *self)
{
    /*
     * 0x3000586a: CALL 0x30004550 with EDI=self (0x30005868 MOV EDI,EBX) and
     * renderEntity pushed (0x30005867 PUSH EBP). 0x30004550 was reconstructed as
     * BG_PlayerAngles (functions/FUN_30004550_30004859.c): its machine code proves the
     * record is a clientInfo_t and the arg is an entityState_t. The former DObj
     * effect overlay has been removed now that the complete row is shared.
     */
    BG_PlayerAngles(renderEntity, self);

    /*
     * 0x30005872: CALL 0x30004860 with EDI=renderEntity (0x30005870 MOV EDI,EBP)
     * and self pushed (0x3000586f PUSH EBX). This is BG_AnimPlayerConditions: it
     * reads the render entity as an entityState_t (weapon/eFlags/eventParm/legsAnim/
     * clientNum) and self as the clientInfo_t (only viewPitch +0x3e8), and
     * snapshots them into bgs.clientinfo[clientNum].conditionWords[*].
     */
    BG_AnimPlayerConditions(renderEntity, self);

    /*
     * 0x30005877..0x30005890: time out both emitters. EDI = self->animTree
     * ([EBX+0x4c4]); each emitter address is loaded via LEA (self+0x380, then
     * self+0x3b0) into ESI and the time/id pushed as the stack arg.
     */
    BG_PlayerAnimation_VerifyAnim(
        self->animTree, (bg_anim_slot_t *)&self->legsYawAngle);
    BG_PlayerAnimation_VerifyAnim(
        self->animTree, (bg_anim_slot_t *)&self->torsoYawAngle);

    /*
     * 0x30005895..0x300058c0: if the left-hand state is active and no meaningful
     * bit (any bit except 0x200) is set in the torso animation word, clear the
     * hand state and mark the DObj dirty. The dword at [EBX+0x3c0] is the torso
     * slot's animationWord
     * (0x3b0+0x10); the flag pair gunHandLeft/dobjNeedsUpdate lives at +0x400/+0x404.
     *   0x30005895 MOV EAX,[EBX+0x400]      ; gunHandLeft
     *   0x3000589e TEST EAX,EAX / JZ        ; skip when inactive
     *   0x300058a2 TEST [EBX+0x3c0],0xfffffdff / JNZ ; skip when any bit but 0x200 set
     *   0x300058ae MOV [EBX+0x400],0        ; gunHandLeft = 0
     *   0x300058b8 MOV [EBX+0x404],1        ; dobjNeedsUpdate  = 1
    */
    if (self->gunHandLeft != 0 &&
        (self->torsoAnimWord & ~ANIM_TOGGLEBIT) == 0u) {
        self->gunHandLeft = 0;
        self->dobjNeedsUpdate = 1;
    }

    /*
     * 0x300058c2..0x300058e6: advance both emitters from the render entity's
     * per-emitter params. Each call: EAX = emitter address (self+0x380, then
     * self+0x3b0 reused from ESI), ECX = self, EDI = renderEntity (0x300058c9
     * MOV EDI,EBP, preserved across both calls), and renderEntity->legsAnim/
     * torsoAnim
     * pushed as the stack arg.
     *   0x300058c2 MOV EAX,[EBP+0xd0] / PUSH EAX ; emitterParam0
     *   0x300058d8 MOV ECX,[EBP+0xd4] / PUSH ECX ; emitterParam1
     */
    BG_RunLerpFrameRate(self, (bg_anim_slot_t *)&self->legsYawAngle,
                       renderEntity->legsAnimWord, renderEntity);
    BG_RunLerpFrameRate(self, (bg_anim_slot_t *)&self->torsoYawAngle,
                       renderEntity->torsoAnimWord, renderEntity);
}
