// Source: uo_cgame_mp_x86.dll 0x30030f10..0x3003101f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30030f10_3003101f.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/* Layout guards proving the machine-code offsets/stride this function relies on
 * (verified at 4-byte i386 pointer width). The 0x4d0 element stride matches
 * `IMUL EAX,EAX,0x4d0` at 0x30030f43; infoValid +0x00 matches `CMP [EAX],0`
 * at 0x30030f4e; health +0x3c matches `MOV EDX,[EAX+0x3c]` at 0x30030f99.
 * The rect fields x/y/w/h at +0x0/+0x4/+0x8/+0xc match the EBX reads
 * (`MOV ECX,[EBX+0xc]`=h, `MOV EDX,[EBX+8]`=w, `MOV EAX,[EBX+4]`=y, `MOV ECX,[EBX]`=x
 * in the icon path; FLD [EBX+0xc]/FADD [EBX+4]/FSUBR [EBX+8]/FADD [EBX] in the value
 * path). */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(clientInfo_t) == 0x4d0, "bgs.clientinfo element stride");
#endif
_Static_assert(offsetof(clientInfo_t, infoValid) == 0x00, "infoValid +0x00");
_Static_assert(offsetof(clientInfo_t, health) == 0x3c, "health +0x3c");
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(rectDef_t, x) == 0x0, "rectDef_t.x +0x0");
_Static_assert(offsetof(rectDef_t, y) == 0x4, "rectDef_t.y +0x4");
_Static_assert(offsetof(rectDef_t, w) == 0x8, "rectDef_t.w +0x8");
_Static_assert(offsetof(rectDef_t, h) == 0xc, "rectDef_t.h +0xc");
#endif

/*
 * CG_HudEmitIconOrValue (0x30030f10) — an ITERATOR-DRIVEN member of the CG_R_TEXT_PAINT
 * HUD emit family that draws ONE per-client HUD element inside a caller-supplied
 * rect. Like its siblings (CG_DrawSelectedPlayerName 0x30031020,
 * CG_DrawSelectedPlayerLocation 0x300311f0) it advances/clamps the shared HUD emit
 * cursor (cg_currentSelectedPlayer_vmCvar.integer), maps it through cg_hudEmitClientTable[] to a
 * per-client bgs.clientinfo[] index, and early-outs if that state is empty.
 * It then renders in one of two modes:
 *
 *   - ICON mode  (hIcon != 0): set the 2D draw color to `color` (trap_R_SetColor),
 *     draw the icon shader stretched to the whole element rect (CG_DrawPic), then
 *     reset the color to opaque white (trap_R_SetColor(NULL)).
 *
 *   - VALUE mode (hIcon == 0): format the iterated client's per-client integer
 *     cached health with "%i" into a 16-byte buffer (Com_sprintf), measure the text
 *     width (trap_R_Text_Width), and draw the text centered horizontally in the rect at
 *     y = rect.h + rect.y (trap_R_Text_Paint).
 *
 * Cursor clamp (identical preamble to the family siblings that share these globals):
 *   idx = cg_currentSelectedPlayer_vmCvar.integer;                     // MOV EAX,[0x3044f60c]
 *   if (idx < 0 || idx >= cg_hudEmitCount)      // TEST/JL ; CMP [0x305385e0]/JL (signed)
 *       cg_currentSelectedPlayer_vmCvar.integer = idx = 0;             // XOR EAX,EAX ; MOV [0x3044f60c],EAX
 *   state = &bgs.clientinfo[cg_hudEmitClientTable[idx]];
 *                                               // MOV EAX,[EAX*4 + 0x305384c0]
 *                                               // IMUL EAX,EAX,0x4d0 ; ADD EAX,0x305e1f34
 *   if (state->infoValid == 0) return;    // CMP [EAX],0 ; JZ epilogue
 * The out-of-range test is signed (JL after TEST EAX,EAX, then JL after
 * CMP EAX,[cg_hudEmitCount]); cg_currentSelectedPlayer_vmCvar.integer/Count and cg_hudEmitClientTable[]
 * are int32.
 *
 * Register-argument ABI (non-default; proven from the call site at 0x30032229):
 *   ECX = hIcon    // MOV ESI,ECX ; the icon shader handle (qhandle_t), 0 selects value mode
 *   EBX = rect     // LEA EBX,[ESP+0x1c] at the call site: a rectDef_t {x,y,w,h}
 * plus four cdecl stack words (caller cleans 0x10 = four dwords):
 *   arg0 (EBP)   = drawParamA   // CG_R_TEXT_WIDTH slot a1 + CG_R_TEXT_PAINT slot a2 (value mode)
 *   arg1         = drawParamB   // CG_R_TEXT_WIDTH slot a2 + CG_R_TEXT_PAINT slot a3 (value mode)
 *   arg2         = drawColor    // trap_R_SetColor(rgba) pointer in ICON mode;
 *                              //   CG_R_TEXT_PAINT slot a4 in value mode
 *   arg3         = drawParamD   // CG_R_TEXT_PAINT slot a8 (value mode)
 * The four cdecl values are forwarded verbatim into the draw traps; only arg2's role
 * as the trap_R_SetColor rgba pointer (icon mode) is proven. The exact draw-parameter
 * meaning of the others (font handle, scale, style flags) is not individually proven
 * here, so they carry role-shaped pass-through names.
 *
 * The /GS stack canary (MOV EAX,[__security_cookie] at entry, __security_check_cookie
 * at both exits) and the EBP/ESI/EDI save/restore + the RET (caller-cleaned cdecl)
 * are i386 calling-convention details with no source-level meaning.
 *
 * Name adjudication: the .mcode header's mechanical guess "script_func_precacheheadicon"
 * is REJECTED — it is a pure win-size==0x10f == matched-size guess. This routine
 * precaches nothing; it advances the HUD emit cursor and issues the 2D draw traps
 * trap_R_SetColor / CG_DrawPic / trap_R_Text_Width / trap_R_Text_Paint, i.e. it is the icon/value
 * member of the trap-54 HUD emit family documented in client_recovered.h. Named
 * CG_HudEmitIconOrValue by proven role; exact original cgame symbol unproven (no
 * cgame syscall-id/symbol table recovered), and trap 54 keeps the honest CG_R_TEXT_PAINT
 * name like the rest of the family.
 */

/* "%i" (0x300769e0 in .rdata): plain signed-decimal health format. */
static const char CG_HUD_EMIT_VALUE_FORMAT[] = "%i";

/* The value-text buffer size (MOV ESI,0x10 -> Com_sprintf size argument). */
enum { CG_HUD_EMIT_VALUE_BUFSIZE = 16 };

void CG_HudEmitIconOrValue(qhandle_t hIcon /*ECX*/,
                           const rectDef_t *rect /*EBX*/,
                           int32_t drawParamA /*stack arg0, EBP*/,
                           int32_t drawParamB /*stack arg1*/,
                           const vec4_t drawColor /*stack arg2*/,
                           int32_t drawParamD /*stack arg3*/)
{
    int32_t idx = cg_currentSelectedPlayer_vmCvar.integer;

    /* Out-of-range (idx < 0 OR idx >= count, signed) -> reset to slot 0 and store
     * back. */
    if (idx < 0 || idx >= cg_hudEmitCount) {
        idx = 0;
        cg_currentSelectedPlayer_vmCvar.integer = 0;
    }

    int32_t clientNum = cgame_compat_read_target_i32_index(
        cg_hudEmitClientTable, idx);
    clientInfo_t *state = cgame_compat_unchecked_clientinfo(
        &bgs.clientinfo[0], clientNum);

    /* No valid per-client state at this table slot -> emit nothing. */
    if (state->infoValid == 0) {
        return;
    }

    if (hIcon != 0) {
        /* ICON mode (0x30030f5b): color modulation, icon fills the whole rect,
         * then reset to opaque white. The color pointer used here is arg2
         * (drawColor, `MOV EAX,[ESP+0x2c]` at 0x30030f5b), i.e. a float[4] rgba
         * pointer; the machine code forwards the raw dword slot unchanged. */
        trap_R_SetColor(drawColor);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, hIcon);
        trap_R_SetColor(NULL);
        return;
    }

    /* VALUE mode (0x30030f99): format the iterated client's integer and draw it
     * centered horizontally in the rect. */
    char valueText[CG_HUD_EMIT_VALUE_BUFSIZE];
    Com_sprintf(valueText, CG_HUD_EMIT_VALUE_BUFSIZE,
               CG_HUD_EMIT_VALUE_FORMAT, state->health);

    /* trap_R_Text_Width (trap id 52) measures the text width; the caller FILDs it below.
     * The four data slots are (text, drawParamA[arg0], drawParamB[arg1], 0),
     * proven from the push order at 0x30030fb5..0x30030fbe (PUSH 0x0; PUSH arg1;
     * PUSH arg0; PUSH buffer; PUSH 0x34). */
    int32_t textWidth = trap_R_Text_Width(valueText, drawParamA, drawParamB, 0);

    /* y = rect.h + rect.y (FLD [EBX+0xc]; FADD [EBX+4]). */
    float drawY = (float)((long double)rect->h +
                          (long double)rect->y);

    /* Centered x = rect.x + 0.5f * (rect.w - textWidth)
     * (FILD width; FSUBR [EBX+8]; FMUL 0.5f (0x3007bce8); FADD [EBX]). textWidth
     * enters via a bare FILD (0x30030ff5) straight into the FSUBR with no FSTP
     * DWORD, so it stays exact in 80-bit -- no (float) cast (that would round). */
    float centerX = (float)(
        (long double)rect->x +
        ((long double)rect->w - (long double)textWidth) * 0.5L);

    /* trap_R_Text_Paint slot mapping (proven from the push order 0x30030fdf..0x30031004):
     *   a0 = centerX (float bits), a1 = drawY (float bits),
     *   a2 = drawParamA (arg0), a3 = drawParamB (arg1), a4 = drawColor (arg2),
     *   a5 = valueText, a6 = 0, a7 = 0, a8 = drawParamD (arg3). */
    trap_R_Text_Paint(CG_FloatBits(centerX), CG_FloatBits(drawY),
              drawParamA, drawParamB, (intptr_t)drawColor,
              (intptr_t)valueText, 0, 0, drawParamD);
}
