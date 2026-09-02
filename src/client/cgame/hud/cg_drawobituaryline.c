#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30031a90..0x30031b5d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031a90_30031b5d.mcode
//
// Trap-52-gated member of the cgame trap-54 emitter family (siblings 0x30031940 /
// 0x30031a00 / 0x300319a0 / 0x30031510 / this). It draws the scoreboard "fragged
// by" obituary line, horizontally centered on the string's measured pixel width.
//
// Behavior:
//   - Early-out if cg_fraggedByName[0] == 0 (no target name set): draw nothing.
//   - Format the line twice with va("Fragged by %s", cg_fraggedByName). Because va()
//     rotates a small ring of buffers, the two calls return two distinct pointers:
//     the first (str1) is drawn; the second (str2) is fed to the width-measuring
//     trap as its key. The machine code re-checks the name-set byte before the
//     second format and retains an empty-string fallback. A concurrent or re-entrant
//     mutation is not expected here, but the second load remains explicit below.
//   - Measure the string via cgame_syscall(CG_R_TEXT_WIDTH, str2, regWord, arg0, 0),
//     whose int32 return is the text width; halve it with the signed /2 idiom
//     (CDQ; SUB EAX,EDX; SAR EAX,1 == width/2 rounding toward zero).
//   - Center point X = obj->x + obj->w * 0.5f; baseline Y = obj->h + obj->y.
//     The drawn left edge is drawX = X - (float)(width/2).
//   - Emit the draw via the family's fixed 10-slot CG_R_TEXT_PAINT vector:
//       cgame_syscall(CG_R_TEXT_PAINT, <drawX bits>, <Y bits>, regWord, arg0, arg1,
//                     str1, 0, 0, arg2)
//
// Name adjudication: the .mcode header's size-matched "BG_GetAnimScriptEvent" guess
// is REJECTED. This function reads no anim-script tables and looks nothing up by
// event; it is a 2D text draw that formats "Fragged by %s", measures the string
// with trap 52, and emits trap 54. The exact original CoD symbol and the engine
// services behind traps 52/54 are unproven (no cgame syscall-id table is recovered,
// matching how CG_R_TEXT_WIDTH/54/58/... are treated in client_recovered.h), so the
// function keeps a behavioral name (CG_DrawObituaryLine) and traps 52/54 keep their
// honest role names.
//
// Register-argument ABI (matches the family): the object pointer arrives in ESI
// (read via MOV [ESI+..], never from a stack slot, never saved/restored). One extra
// word arrives in EBX (forwarded verbatim to BOTH traps; also never saved/restored,
// so it is an incoming register argument, not a preserved callee-save). Three
// forwarded words arrive as ordinary cdecl stack arguments; the function ends in a
// plain RET (no callee cleanup of the incoming slots). EDI is the only genuine
// callee-save here (PUSH EDI / POP EDI) and holds str1. ADD ESP,0x3c unwinds the two
// va() arg pushes plus the two syscall arg vectors that are left on the stack across
// both calls (they are caller-cleaned, deferred to one add); ADD ESP,0x14 unwinds
// the SUB ESP,0x14 frame.
//
// Instruction map (frame base F = ESP after `SUB ESP,0x14`; incoming stack args at
// F+0x18=arg0, F+0x1c=arg1, F+0x20=arg2; locals in F+0x0..F+0x10):
//   30031a90 MOV  AL,[cg_fraggedByName]      gate byte = name[0]
//   30031a98 TEST AL,AL / JZ 0x30031b59      if empty, skip whole body
//   30031aa0 FLD  [ESI+0x8]                  st0 = obj->w
//   30031aa4 FMUL [0x3007bce8]               st0 = obj->w * 0.5f
//   30031ab4 FADD [ESI]                      st0 = obj->w*0.5f + obj->x  (= X)
//   30031ab6 FSTP [F+0x10]                   local X
//   30031aa9..30031aba PUSH name; PUSH "Fragged by %s"; CALL va  -> EAX = str
//   30031aca MOV  EDI,EAX                    str1 = first va()
//   30031acc MOV  EAX,0x30074a0c            EAX = "" (empty-string default, dead here)
//   30031abf..30031ad1 re-test name[0]; JZ skips 2nd format (not taken: name set)
//   30031ad3..30031ae2 PUSH name; PUSH "Fragged by %s"; CALL va -> EAX = str2
//   30031ae5 first syscall (CG_R_TEXT_WIDTH) args pushed:
//            0, arg0, regWord(EBX), str2, 0x34  ->
//            EAX = cgame_syscall(52, str2, EBX, arg0, 0)   (int32 text width)
//   30031afc FLD [ESI+0xc]; FADD [ESI+0x4]    st0 = obj->h + obj->y (= Y)
//   30031b06 CDQ; SUB EAX,EDX; SAR EAX,1      EAX = width/2 (signed, toward zero)
//   30031b0d FSTP [F+0x4]                     local Y
//   30031b16 MOV [F+0x8],EAX                  local half = width/2
//   30031b26 FILD [F+0x8]                     st0 = (float)half
//   30031b2f FSUBR [F+0x10]                   st0 = X - (float)half  (= drawX)
//   30031b40 FSTP [F+0x10]                    local drawX
//   30031b44..30031b4f second syscall (CG_R_TEXT_PAINT) 10-slot vector pushed:
//            arg2, 0, 0, str1(EDI), EBX, arg0, arg1, Y, drawX, 0x36  ->
//            cgame_syscall(54, drawX, Y, EBX, arg0, arg1, str1, 0, 0, arg2)
//   30031b55 ADD ESP,0x3c ; POP EDI ; ADD ESP,0x14 ; RET
//
// Float precision: X, Y and drawX are computed entirely on the x87 stack at single
// precision (FLD/FMUL/FADD of float ptr, FILD of an int, FSUBR), stored via FSTP to
// 4-byte slots, and forwarded to the variadic trap as raw 32-bit bit patterns. They
// are reconstructed as `float` and forwarded through CG_FloatBits so the bit pattern
// is reproduced exactly (no double promotion, matching the family).

/*
 * The FMUL constant at .rdata 0x3007bce8 is the single-precision float 0x3f000000
 * (= 0.5f); the va() format string at 0x30079750 is "Fragged by %s"
 * (g_string_fragged_by_pcts); the dead empty-string default loaded into
 * EAX at 0x30031acc is 0x30074a0c ("" in .rdata).
 */

void CG_DrawObituaryLine(rectDef_t *obj, intptr_t regWord, intptr_t arg0, intptr_t arg1, intptr_t arg2)
{
    /* 0x30031a90 MOV AL,[cg_fraggedByName]; TEST AL,AL; JZ epilogue */
    if (cg_fraggedByName[0] == 0)
        return;

    /*
     * Center point X = left + width*0.5. FLD f_8; FMUL 0.5f; FADD f_0.
     * (obj->w * 0.5f + obj->x.)
     */
    float centerX = obj->w * 0.5f + obj->x;

    /* Two va() calls: str1 is drawn, str2 keys the width measurement. va() rotates
     * buffers, so the two pointers differ. */
    const char *str1 = va("Fragged by %s", cg_fraggedByName);
    const char *str2 = "";
    /* 0x30031abf..0x30031ae2 re-reads the global byte after the first va().
     * It is normally still nonzero, but the retail load and empty-string
     * fallback are part of the observable operation graph. */
    if (cg_fraggedByName[0] != '\0') {
        str2 = va("Fragged by %s", cg_fraggedByName);
    }

    /* CG_R_TEXT_WIDTH measures the string's pixel width (int32 return). */
    int32_t textWidth = (int32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)str2, regWord, arg0, 0);

    /* Y baseline = obj->h + obj->y (FLD f_c; FADD f_4). */
    float baselineY = obj->h + obj->y;

    /* Signed width/2 rounding toward zero: CDQ; SUB EAX,EDX; SAR EAX,1. */
    int32_t halfWidth = textWidth / 2;

    /* drawX = centerX - halfWidth: 0x30031b26 FILD [halfWidth]; 0x30031b2f FSUBR
     * centerX. halfWidth is FILDed straight into the subtract (no float store), so it
     * stays exact in 80-bit -- no (float) cast. */
    float drawX = centerX - halfWidth;

    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(drawX),      /* word0: left edge, centered */
                  CG_FloatBits(baselineY),  /* word4: baseline Y */
                  regWord, arg0, arg1, (intptr_t)str1,  /* drawn "Fragged by %s" string */
                  0, 0, arg2);
}
