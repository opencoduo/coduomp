#include "../client_recovered.h"

#include <stdint.h>
#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x300319a0..0x300319f6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300319a0_300319f6.mcode
//
// One handler of a three-member cgame trap-54 emitter family
//   0x30031940 / 0x300319a0 (this) / 0x30031a00
// dispatched by the command-table trampolines at 0x3003235c/0x3003237e/0x30032391,
// each of which materialises a small context object on its own stack (LEA EAX,
// [ESP+0x18]) and calls the handler with EAX = &ctx and three console/command
// words pushed as (word0, word1, word2). The three siblings are identical in
// shape and differ only in which static 256-byte string buffer they pass as the
// pointer argument (this one passes the buffer at 0x305384e0; the 0x30031940
// sibling passes 0x30538600 and the 0x30031a00 sibling passes 0x30538700).
// Retail UO's CG_AREA_TEAMCHAT owner-draw id and the matching same-module macOS
// symbol establish the original function name CG_DrawAreaTeamChat.
//
// The handler reads two fields from the context object, forms a single-precision
// sum of them, and forwards everything to cgame trap 54 (CG_R_TEXT_PAINT); it returns
// nothing (bare RET, no EAX set). The assigned .mcode name "SP_trigger_lookat" is
// a size-only false match (that is a server trigger_damage.c entity-spawn, absent
// from this cgame trap path) and is rejected.
//
// Register-argument ABI: the context pointer arrives in EAX; word0/word1/word2 are
// the three stack dwords (caller-cleaned, ADD ESP,0xc at the call site).
//
// Instruction map (see .mcode):
//   300319a3 FLD  [EAX+0xc]            st0 = ctx->h           (float, +0xc)
//   300319a6 MOV  EDX,[EAX]            word0 = bits(ctx->x)            (int32, +0)
//   300319a8 FADD [EAX+0x4]            st0 = ctx->h + ctx->y  (+0x4)
//   300319bb FSTP [ESP+0xc]            store the float sum, reload its bit pattern
//   319af/319b1/319c3 PUSH 0/0/0       -> the three trailing zero trap words
//   300319c8 PUSH 0x305384e0           -> the static string buffer pointer arg
//   push (arg2 / arg1 / arg0)          -> the three caller words
//   push (floatSumBits)                -> arg2 of the trap: the float sum, by bits
//   push (word0)                       -> arg1 of the trap: bits(ctx->x)
//   300319ea PUSH 0x36                  -> command id 54 (CG_R_TEXT_PAINT)
//   300319ec CALL *cgame_syscall ; ADD ESP,0x34 ; RET
//
// The trap receives the float sum as a raw 32-bit pattern (FSTP to a stack dword
// that is then PUSHed as an integer word), exactly as the variadic cgame_syscall
// ABI carries floats; we reproduce that by punning the float to its bit pattern.

/*
 * Caller-observed layout of the small context object built by the dispatcher
 * trampolines for the trap-54 handler family. Only the three fields this handler
 * reads are modelled (word0 at +0, coord_lo at +0x4, coord_hi at +0xc); the
 * object is larger (the dispatcher writes several adjacent slots). Exact source
 * type name unresolved.
 */
/* Uses the shared rectDef_t type from client_recovered.h (same layout). */

void CG_DrawAreaTeamChat(rectDef_t *ctx, intptr_t word1, intptr_t word2, intptr_t word3)
{
    /*
     * The trap carries the float sum as a raw 32-bit pattern; the machine code
     * stores it via FSTP and re-pushes the dword. Reproduce that bit-exact
     * transfer by punning the float to its integer representation.
     */
    float coordSum = ctx->h + ctx->y;
    int32_t coordSumBits;
    memcpy(&coordSumBits, &coordSum, sizeof coordSumBits);

    /*
     * cg_hudEmitScratch is a static 256-byte scratch string
     * buffer (0x305384e0..0x305385e0) belonging to this handler; the mechanical
     * "sp_trigger_lookat" owner label is the rejected size-match noted above and
     * its uint32_t typing is the exporter truncating the buffer to its first
     * dword. Its address is forwarded as the pointer argument. Identity as one of
     * the trap-54 family's per-handler buffers is proven; a final buffer name is
     * left to the sibling reconstruction that owns the whole family.
     */
    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(ctx->x), coordSumBits, word1, word2, word3, (intptr_t)cg_hudEmitScratch, 0, 0, 0);
}
