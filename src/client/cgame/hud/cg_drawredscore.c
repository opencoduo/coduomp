// Source: uo_cgame_mp_x86.dll 0x30031510..0x300315dc
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031510_300315dc.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_DrawRedScore (0x30031510) — HUD-stat display member of the CG_R_TEXT_PAINT
 * emitter family (the 0x30031.. trap-54 / HUD-emit cluster). It renders the current
 * value of one parallel HUD-stat integer (cg_hudStat5Value, the middle
 * sibling of the d8/dc/e0 array) into a short text buffer, runs the trap-52 keyed
 * helper, and then issues one cgame trap 54 (CG_R_TEXT_PAINT) draw whose two leading
 * float coordinates it computes from the caller's object (obj) plus the trap-52
 * result.
 *
 * Register + stack ABI (proven from the instruction stream):
 *   EBX (reg)  : obj — a rectDef_t pointer, never saved/restored here (a
 *                caller-provided register argument, like the ESI-object siblings).
 *                Its floats f_0/f_4/f_8/f_c are all read.
 *   [E+0x04] arg0  : MOV EBP,[ESP+0x28] at entry (kept in EBP across the body).
 *   [E+0x08] arg1  : trap-52 "b" arg and second trap-54 caller word.
 *   [E+0x0C] arg2  : third trap-54 caller word.
 *   [E+0x10] arg3  : trailing trap-54 word (last argument).
 *   (E = entry ESP. Plain RET, no imm -> the four stack args are caller-cleaned:
 *   cdecl.)
 *
 * Body:
 *   1. statValue = cg_hudStat5Value.  If it holds the -9999 "unset"
 *      sentinel (CMP EAX,0xffffd8f1), format "-" into the 16-byte display buffer;
 *      otherwise format "%i" of statValue. The formatter is Com_sprintf with dest in
 *      EDI (a 16-byte stack buffer) and size in ESI (16), so:
 *          Com_sprintf(buf, 16, "-")            when unset
 *          Com_sprintf(buf, 16, "%i", statValue) otherwise
 *      ("-" is cg_hudStatUnsetText; "%i" is an ordinary format literal.)
 *   2. r = cgame_syscall(CG_R_TEXT_WIDTH, buf, arg0, arg1, 0). The keyed trap-52 helper
 *      returns an int32 (the machine code FISUBs it, i.e. treats it as an integer).
 *   3. Compute two float coordinates:
 *          coordA = (obj->w + obj->x) - (float)r   (FLD f_8; FADD f_0; FISUB r)
 *          coordB =  obj->h + obj->y               (FLD f_c; FADD f_4)
 *      and emit them as raw 32-bit float bit patterns (the code FSTPs each to a
 *      dword then forwards the dword to the variadic trap, never promoting to
 *      double), exactly as the other trap-54 emitters do.
 *   4. cgame_syscall(CG_R_TEXT_PAINT, bits(coordA), bits(coordB), arg0, arg1, arg2,
 *                    buf, 0, 0, arg3).
 *
 * Name adjudication: the .mcode header's size-matched guess "MenuParse_itemDef"
 * is REJECTED. This function parses no menu/itemDef tokens; it is a fixed-arity
 * HUD-stat display emitter that formats one integer and dispatches cgame trap 54
 * through *0x30085e9c, structurally a twin of the 0x304480e0-stat display emitter
 * at the adjacent 0x300315e0 and of CG_Draw1stPlace (0x30031b60), the
 * other reader of this same stat slot. The match was a pure 0xcc size collision
 * with no behavioral basis (the contract forbids size-based naming). The engine
 * service behind trap 52 / trap 54 is unproven (no cgame syscall-id table
 * recovered). Retail UO ui_mp/menudef.h assigns owner-draw id 28 the exact name
 * CG_RED_SCORE, and the same-module macOS binary exports CG_DrawRedScore.
 */

/* 16-byte display-string buffer used as the trap-54 string argument. Proven from
 * MOV ESI,0x10 (the Com_sprintf size) and LEA EDI,[ESP+0x18] (dest). */
enum {
    CG_HUDSTAT_STRING_SIZE = 16
};

void CG_DrawRedScore(rectDef_t *obj /* EBX */, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
    char displayString[CG_HUDSTAT_STRING_SIZE];

    /* MOV EAX,[0x304480dc] ; CMP EAX,0xffffd8f1 (== -9999). Signed sentinel gate. */
    int32_t statValue = cg_hudStat5Value;

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

    /* FLD f_8 ; FADD f_0 ; FISUB (integer)keyedResult (0x3003159e). keyedResult is
     * subtracted as an INTEGER straight into the 80-bit chain -- no FSTP DWORD
     * rounds it first, so no (float) cast here (that would round under -std=c11). */
    float coordA = (float)(((long double)obj->w + (long double)obj->x) - (long double)keyedResult);
    /* FLD f_c ; FADD f_4. */
    float coordB = obj->h + obj->y;

    /*
     * cgame_syscall(CG_R_TEXT_PAINT, bits(coordA), bits(coordB), arg0, arg1, arg2,
     *               buf, 0, 0, arg3) — the 10-slot trap-54 draw frame. Both floats
     * are forwarded as their raw 32-bit words (FSTP float -> dword), matching the
     * rest of the CG_R_TEXT_PAINT emitter family.
     */
    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(coordA), CG_FloatBits(coordB), arg0, arg1, arg2, (intptr_t)displayString, 0, 0, arg3);
}
