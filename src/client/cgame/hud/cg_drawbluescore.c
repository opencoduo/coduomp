// Source: uo_cgame_mp_x86.dll 0x300315e0..0x300316ac
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300315e0_300316ac.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_DrawBlueScore (0x300315e0) — HUD-stat display member of the CG_R_TEXT_PAINT
 * emitter family (the 0x30031.. trap-54 / HUD-emit cluster). It is the byte-for-byte
 * structural twin of CG_DrawRedScore (0x30031510); the only differences are
 * (a) it renders the +0xe0 parallel HUD-stat integer cg_hudStat6Value
 * instead of the +0xdc sibling, and (b) the compiler wrapped it in an MSVC /GS
 * stack-cookie prologue/epilogue (the 0x30031510 twin has none). It renders the
 * current stat value into a short text buffer, runs the trap-52 keyed helper on it,
 * and then issues one cgame trap 54 (CG_R_TEXT_PAINT) draw whose two leading float
 * coordinates it computes from the caller's object (obj) plus the trap-52 result.
 *
 * Register + stack ABI (proven from the instruction stream):
 *   EBX (reg)  : obj — a rectDef_t pointer, never saved/restored here (a
 *                caller-provided register argument, like the other register-object
 *                siblings). Its floats f_0/f_4/f_8/f_c are all read.
 *   [E+0x04] arg0  : MOV EBP,[ESP+0x28] at entry (kept in EBP across the body).
 *   [E+0x08] arg1  : trap-52 "b" arg and second trap-54 caller word.
 *   [E+0x0C] arg2  : third trap-54 caller word.
 *   [E+0x10] arg3  : trailing trap-54 word (last argument).
 *   (E = entry ESP. Plain RET, no imm -> the four stack args are caller-cleaned:
 *   cdecl.)
 *
 * MSVC /GS: MOV EAX,[__security_cookie] ; MOV [ESP+0x20],EAX snapshots the cookie
 * into the frame canary on entry (0x300315e3/0x300315ed), and MOV ECX,[canary] ;
 * CALL __security_check_cookie (0x30061639) verifies it on exit (0x30031699/
 * 0x300316a3). These are compiler-generated stack-protector instructions, not
 * source statements, so they are omitted from the body below.
 *
 * Body (matches the 0x30031510 twin exactly except for the stat slot):
 *   1. statValue = cg_hudStat6Value.  If it holds the -9999 "unset"
 *      sentinel (CMP EAX,0xffffd8f1), format "-" into the 16-byte display buffer;
 *      otherwise format "%i" of statValue. The formatter is Com_sprintf with dest in
 *      EDI (a 16-byte stack buffer, LEA EDI,[ESP+0x18]) and size in ESI (MOV ESI,0x10):
 *          Com_sprintf(buf, 16, "-")             when unset
 *          Com_sprintf(buf, 16, "%i", statValue) otherwise
 *      ("-" is cg_hudStatUnsetText; "%i" is an ordinary format literal.) The
 *      Com_sprintf return value is discarded (the twin discards it too).
 *   2. r = cgame_syscall(CG_R_TEXT_WIDTH, buf, arg0, arg1, 0). The keyed trap-52 helper
 *      returns an int32 (the machine code FISUBs it, i.e. treats it as an integer).
 *   3. Compute two float coordinates:
 *          coordA =  obj->h + obj->y               (FLD f_c; FADD f_4)   [stored first]
 *          coordB = (obj->w + obj->x) - (float)r   (FLD f_8; FADD f_0; FISUB r)
 *      and emit them as raw 32-bit float bit patterns (the code FSTPs each to a
 *      dword then forwards the dword to the variadic trap, never promoting to
 *      double), exactly as the other trap-54 emitters do.
 *   4. cgame_syscall(CG_R_TEXT_PAINT, bits(coordB), bits(coordA), arg0, arg1, arg2,
 *                    buf, 0, 0, arg3).  (Argument order proven from the reversed push
 *      trace: id 0x36 last-pushed = arg0; then coordB, coordA, arg0, arg1, arg2, &buf,
 *      0, 0, arg3.  So the trap-54 arg1 slot = coordB and arg2 slot = coordA, matching
 *      the 0x30031510 twin's arg1=(f_8+f_0)-r, arg2=(f_c+f_4).)
 *
 * Name adjudication: the .mcode header's size-matched guess "Script_Orbit" is
 * REJECTED. This function has no orbit/vehicle/script-camera math; it is a fixed-arity
 * HUD-stat display emitter that formats one integer and dispatches cgame trap 54
 * through *0x30085e9c, the direct twin of CG_DrawRedScore (0x30031510). The
 * match was a pure 0xcc size collision with no behavioral basis (the contract forbids
 * size-based naming). The globals.h note on cg_hudStat6Value already documents
 * "the display code at 0x300315f1 prints '-' when it holds the sentinel and otherwise
 * formats the value" — that is this function. Retail UO ui_mp/menudef.h assigns
 * owner-draw id 27 the exact name CG_BLUE_SCORE, and the same-module macOS binary
 * exports CG_DrawBlueScore.
 */

/* 16-byte display-string buffer used as the trap-54 string argument. Proven from
 * MOV ESI,0x10 (the Com_sprintf size) and LEA EDI,[ESP+0x18] (dest). */
enum {
    CG_HUDSTAT_STRING_SIZE = 16
};

void CG_DrawBlueScore(rectDef_t *obj /* EBX */, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
    char displayString[CG_HUDSTAT_STRING_SIZE];

    /* MOV EAX,[0x304480e0] ; CMP EAX,0xffffd8f1 (== -9999). Signed sentinel gate. */
    int32_t statValue = cg_hudStat6Value;

    if (statValue == CG_SCORE_VALUE_UNSET) {
        /* JNZ not taken: PUSH "-" ; Com_sprintf(buf, 16, "-"). */
        Com_sprintf(displayString, CG_HUDSTAT_STRING_SIZE, cg_hudStatUnsetText);
    } else {
        /* PUSH statValue ; PUSH "%i" ; Com_sprintf(buf, 16, "%i", statValue). */
        Com_sprintf(displayString, CG_HUDSTAT_STRING_SIZE, "%i", statValue);
    }

    /*
     * cgame_syscall(CG_R_TEXT_WIDTH, buf, arg0, arg1, 0) -> int32.  (id first, then
     * PUSH &buf ; PUSH EBP(arg0) ; PUSH ECX(arg1) ; PUSH 0.)  The trap-52 helper
     * returns an integer (FISUB below reads it as an integer memory operand).
     */
    int32_t keyedResult = (int32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)displayString, arg0, arg1, 0);

    /* FLD f_c ; FADD f_4 (stored first, to the trap-54 arg2 slot). */
    float coordA = obj->h + obj->y;
    /* FLD f_8 ; FADD f_0 ; FISUB (integer)keyedResult (0x3003166e, the trap-54 arg1
     * slot). keyedResult is subtracted as an INTEGER straight into the 80-bit chain
     * -- no FSTP DWORD rounds it first, so no (float) cast (that would round). */
    float coordB = (float)(((long double)obj->w + (long double)obj->x) - (long double)keyedResult);

    /*
     * cgame_syscall(CG_R_TEXT_PAINT, bits(coordB), bits(coordA), arg0, arg1, arg2,
     *               buf, 0, 0, arg3) — the 10-slot trap-54 draw frame. Both floats
     * are forwarded as their raw 32-bit words (FSTP float -> dword), matching the
     * rest of the CG_R_TEXT_PAINT emitter family.
     */
    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(coordB), CG_FloatBits(coordA), arg0, arg1, arg2, (intptr_t)displayString, 0, 0, arg3);
}
