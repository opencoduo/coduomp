#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x3003dfa0..0x3003dfe5
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003dfa0_3003dfe5.mcode
//
// trap_R_AddLightToScene: the cgame trap-0x42 wrapper for the engine's
// "add a dynamic light to the current scene" service. It forwards its five
// 32-bit stack arguments unchanged to cgame_syscall with command id
// CG_R_ADD_LIGHT_TO_SCENE (0x42), then cleans six dwords (id + 5 args) via
// ADD ESP,0x18 and returns.
//
// The trap-id-to-service binding is proven from the sole reconstructed caller,
// CG_EntityEffects (0x3001e7f0): it unpacks currentState.constantLight
// (0xAABBGGRR) into intensity = (light>>24)*4 and r/g/b = (channel byte)/255,
// then pushes (id=0x42, &cent lerpOrigin, intensity, r, g, b) and cleans 0x18
// bytes — i.e. this exact wrapper's ABI. The classic Q3/CoD signature is
// trap_R_AddLightToScene(const vec3_t org, float intensity, float r, float g,
// float b). The same-module PPC bank lists trap_R_AddLightToScene; adopted as
// the role name (no cgame syscall-id table was recovered to bind 0x42 to the
// engine name absolutely). The mechanical .mcode name "trap_syscall_66" is a
// size-shaped placeholder and is superseded by this behavior-proven identity.
//
// Machine-code note on the argument shuffle (0x3003dfa0..0x3003dfd9): the body
// performs a long chain of MOV [ESP+k] <-> reg moves interleaved with the five
// PUSHes. Tracing it slot-by-slot (accounting for each PUSH lowering ESP by 4),
// the net effect is a straight bit-exact forward of the five incoming dwords in
// their original order: the call stack becomes (0x42, org, intensity, r, g, b)
// with the first user argument deepest after the id. It is register-allocation
// noise, not a reordering. The floats/pointer are opaque 32-bit words to the
// wrapper, so they are forwarded through CG_FloatBits to reproduce the exact
// dwords the i386 code pushes (org, a pointer, is already a 32-bit word).
//
// Caller-cleaned cdecl (ADD ESP,0x18 after the call, RET with no imm). The
// service returns void in practice; cgame_syscall's declared int32_t result is
// simply not consumed here.

void trap_R_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
    cgame_syscall(CG_R_ADD_LIGHT_TO_SCENE, (intptr_t)org, CG_FloatBits(intensity), CG_FloatBits(r), CG_FloatBits(g), CG_FloatBits(b));
}
