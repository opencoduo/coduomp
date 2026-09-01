// Source: uo_cgame_mp_x86.dll 0x30031300..0x3003150e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031300_3003150e.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_HudEmitDigits (0x30031300) — HUD number element of the CG_R_TEXT_PAINT emit family
 * (siblings CG_HudEmitIconOrValue 0x30030f10, CG_DrawPlayerLocation 0x30031280).
 * It draws ONE per-client integer HUD value into a caller-supplied rect, EITHER as a
 * single icon shader stretched to the whole rect (icon mode, hIcon != 0) OR — when no
 * icon is supplied — as a run of bitmap digit glyphs (cg_numberShaders[]) laid out
 * left-to-right, one stretch-pic per character.
 *
 * VALUE SOURCE (0x30031310..0x30031351):
 *   clientNum = cg_snap->ps.psClientNum;                                  // MOV [cg_snap+0xe0]
 *   if (clientNum == cg_clientNum) {                                 // CMP ..,[0x30459144]
 *       value = cg_snap->ps.stats[STAT_HEALTH];          // live local value (+0x128)
 *   } else {
 *       state = &bgs.clientinfo[clientNum];                      // IMUL 0x4d0; ADD 0x305e1f34
 *       if (state->infoValid == 0) return;                     // TEST/JZ -> exit
 *       value = state->health;                                  // cached remote-client health (+0x3c)
 *   }
 *   if (value < 0) value = 0;                                        // TEST EDI,EDI; JGE; XOR EDI,EDI
 * When the drawn element belongs to the local player its value is taken live from the
 * current snapshot; for any other bound client it comes from the cached per-client
 * bgs.clientinfo[] slot (and an empty slot draws nothing).
 *
 * Register-argument ABI (non-default; proven from the sole call site at 0x300322ae):
 *   ECX = hIcon      // -> EBX; icon shader handle, 0 selects digit mode
 *   EDX = color      // trap_R_SetColor(color) in icon mode (raw dword forwarded)
 * plus two cdecl stack words (caller cleans 8 = two dwords, ADD ESP,8 at 0x300322c5):
 *   arg0 = rect      // LEA ECX,[ESP+0x10] at the call site: a rectDef_t {x,y,w,h} (-> ESI)
 *   arg1 = charScale // float; per-digit advance = charScale*20, glyph height = charScale*32
 *
 * The /GS stack canary (MOV EAX,[__security_cookie] at 0x30031303, __security_check_cookie
 * 0x30061639 at both exits), the EBX/ESI/EDI/EBP save-restore, and the plain RET
 * (caller-cleaned cdecl) are i386 calling-convention details with no source meaning.
 *
 * Name adjudication: the .mcode header's mechanical guess "CG_InterpolatePlayerState"
 * (a pure win-size 0x20e ~= matched-size guess) is REJECTED — this routine interpolates
 * no player state; it reads one integer, clamps it, and issues 2D HUD draw traps
 * (trap_R_SetColor / CG_DrawPic in icon mode; trap_R_DrawStretchPic per digit in number
 * mode). Named CG_HudEmitDigits by proven role; exact original cgame symbol unproven, and
 * trap 54/72/73 keep the honest CG_R_TEXT_PAINT / trap_R_* names like the rest of the family.
 */

/* Layout guards proving the machine-code offsets/stride this function relies on
 * (verified at 4-byte i386 pointer width). */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(clientInfo_t) == 0x4d0, "bgs.clientinfo element stride");
#endif
_Static_assert(offsetof(clientInfo_t, infoValid) == 0x00, "infoValid +0x00");
_Static_assert(offsetof(clientInfo_t, health) == 0x3c, "health +0x3c");
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(snapshot_t, ps.psClientNum) == 0xe0, "cg_snap->ps.psClientNum +0xe0");
_Static_assert(offsetof(snapshot_t, ps.stats[STAT_HEALTH]) == 0x128,
               "cg_snap->ps.stats[STAT_HEALTH] +0x128");
_Static_assert(offsetof(rectDef_t, x) == 0x0, "rectDef_t.x +0x0");
_Static_assert(offsetof(rectDef_t, y) == 0x4, "rectDef_t.y +0x4");
_Static_assert(offsetof(rectDef_t, w) == 0x8, "rectDef_t.w +0x8");
_Static_assert(offsetof(rectDef_t, h) == 0xc, "rectDef_t.h +0xc");
#endif

/* "%i" (0x300769e0 in .rdata): plain signed-decimal format for the HUD integer. */
static const char CG_HUD_EMIT_DIGIT_FORMAT[] = "%i";

/* MOV ESI,0x10 -> Com_sprintf size argument (the value-text buffer size). */
enum { CG_HUD_EMIT_DIGIT_BUFSIZE = 16 };

/* Value clamp range for digit mode (CMP EDI,0x3e7 / CMP EDI,-0x63): the number is
 * clamped to at most three characters worth of magnitude before formatting. */
enum {
    CG_HUD_EMIT_VALUE_MAX = 999,   /* 0x3e7 */
    CG_HUD_EMIT_VALUE_MIN = -99,   /* 0xffffff9d */
};

/* Maximum glyphs drawn (CMP EBX,3 / MOV EBX,3): at most three characters. */
enum { CG_HUD_EMIT_MAX_GLYPHS = 3 };

/* The minus sign occupies the last cg_numberShaders[] slot (MOV ESI,0xa on '-'). */
enum { CG_NUMBER_SHADER_MINUS = 10 };

/* cgs_screenXScale (0x30447aa4) / cgs_screenYScale (0x30447aa8): virtual-640x480 ->
 * real-pixel scale factors read as floats by every 2D-draw path (FMUL [0x30447aa4] /
 * FMUL [0x30447aa8]); declared in globals.h. */

/* charScale*20 (0x3007be04 == 20.0f) and charScale*32 (0x3007bdd0 == 32.0f): the
 * per-glyph horizontal advance and the glyph cell height, in virtual pixels. */
#define CG_HUD_DIGIT_ADVANCE_SCALE 20.0f /* .rdata 0x3007be04 */
#define CG_HUD_DIGIT_HEIGHT_SCALE  32.0f /* .rdata 0x3007bdd0 */

void CG_HudEmitDigits(qhandle_t hIcon /*ECX*/,
                      const vec4_t color /*EDX*/,
                      const rectDef_t *rect /*stack arg0, ESI*/,
                      float charScale /*stack arg1*/)
{
    /* --- Pick the integer to display (0x30031310..0x30031351). --- */
    int32_t value;
    int32_t clientNum = cg_snap->ps.psClientNum;
    if (clientNum == cg_clientNum) {
        /* Element bound to the local player: use the live snapshot value. */
        value = cg_snap->ps.stats[STAT_HEALTH];
    } else {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_HudEmitDigits: invalid client number %i",
                      clientNum);
            return;
        }
        clientInfo_t *state = &bgs.clientinfo[clientNum];
        if (state->infoValid == 0) {
            /* No valid cached per-client state -> draw nothing. */
            return;
        }
        value = state->health;
    }

    /* value < 0 -> 0 (JGE skips the XOR; a negative field draws as 0 before clamp). */
    if (value < 0) {
        value = 0;
    }

    if (hIcon != 0) {
        /* --- ICON mode (0x30031355): color-modulate, stretch the icon shader over
         * the whole rect, then reset the draw color. --- */
        trap_R_SetColor(color);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, hIcon);
        trap_R_SetColor((const float *)0);
        return;
    }

    /* --- DIGIT mode (0x30031390 onward). --- */

    /* 0x30031391 push edx; push 0x48; call syscall = trap_R_SetColor(color) --
     * same push/push/call form as the icon-mode SetColor at 0x30031355. The
     * digit path sets the backend color modulate for every glyph and does NOT
     * reset it afterward. A prior pass omitted this, so digits rendered with the
     * previously-active (stale) draw color. */
    trap_R_SetColor(color);

    /* Per-glyph horizontal advance and the running left edge, truncated to whole pixels
     * (FMUL 20.0; then CALL 0x3006be3c for the advance, and of rect.y and rect.x).
     * 0x3006be3c is MSVC _ftol2 -- it FISTPs round-to-nearest then applies a sign-aware
     * +-1 residue correction, i.e. it TRUNCATES toward zero (C (int) cast semantics), NOT
     * round-to-nearest; coduo_fp_to_i32_extended models that truncation. */
    int32_t charAdvance = coduo_fp_to_i32_extended(
        (long double)charScale * (long double)CG_HUD_DIGIT_ADVANCE_SCALE);
    int32_t drawY = coduo_fp_to_i32_extended((long double)rect->y);
    int32_t cursorX = coduo_fp_to_i32_extended((long double)rect->x);

    /* Clamp the magnitude to [-99, 999] before formatting (0x300313c5..0x300313e0). */
    if (value > CG_HUD_EMIT_VALUE_MAX) {
        value = CG_HUD_EMIT_VALUE_MAX;
    } else if (value < CG_HUD_EMIT_VALUE_MIN) {
        value = CG_HUD_EMIT_VALUE_MIN;
    }

    /* Format the clamped integer (0x300313e2: Com_sprintf(buf, 16, "%i", value)). */
    char text[CG_HUD_EMIT_DIGIT_BUFSIZE];
    Com_sprintf(text, CG_HUD_EMIT_DIGIT_BUFSIZE, CG_HUD_EMIT_DIGIT_FORMAT, value);

    /* strlen(text), then clamp the glyph count to at most three (0x300313fb..0x30031415). */
    int32_t glyphs = 0;
    {
        const char *p = text;
        while (*p != '\0') {
            ++p;
        }
        glyphs = (int32_t)(p - text);
    }
    if (glyphs > CG_HUD_EMIT_MAX_GLYPHS) {
        glyphs = CG_HUD_EMIT_MAX_GLYPHS;
    }

    /* Empty string -> nothing to draw (0x30031415: TEST buf[0]; JZ end). */
    const char *cp = text;
    if (*cp == '\0') {
        return;
    }

    /* --- Draw each character as its own stretched digit/sign glyph (loop 0x30031425). --- */
    while (glyphs != 0) {
        int32_t glyphIndex;
        char c = *cp;
        if (c == '-') {
            glyphIndex = CG_NUMBER_SHADER_MINUS;
        } else {
            /* MOVSX ESI,AL; SUB ESI,'0' — signed digit-to-index. */
            glyphIndex = (int32_t)(int8_t)(uint8_t)c - '0';
        }

        /* Glyph cell height in whole pixels: trunc(charScale*32) via _ftol2
         * (0x30031440..0x3003146a; coduo_fp_to_i32_extended truncates toward zero). */
        int32_t charHeight = coduo_fp_to_i32_extended(
            (long double)charScale *
            (long double)CG_HUD_DIGIT_HEIGHT_SCALE);

        /* Convert the integer virtual-screen rect into real device pixels; the texture
         * spans the whole glyph (s1=0, t1=0, s2=1, t2=1). Each integer coordinate enters
         * via a bare FILD (0x3003147a/9b/b0/c3) straight into the FMUL by the matching
         * axis screen scale (0x3003148b/a5/b9/cc) -- no FSTP DWORD rounds the int first,
         * so no (float) casts (they would round under -std=c11); the sole rounding per
         * coordinate is the FSTP DWORD to its arg slot (0x30031496/ac/bf/d2). */
        float px = (float)((long double)cursorX *
                           (long double)cgs_screenXScale);
        float py = (float)((long double)drawY *
                           (long double)cgs_screenYScale);
        float pw = (float)((long double)charAdvance *
                           (long double)cgs_screenXScale);
        float ph = (float)((long double)charHeight *
                           (long double)cgs_screenYScale);

        trap_R_DrawStretchPic(CG_FloatBits(px), CG_FloatBits(py),
                              CG_FloatBits(pw), CG_FloatBits(ph),
                              CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                              CG_FloatBits(1.0f), CG_FloatBits(1.0f),
                              cg_numberShaders[glyphIndex]);

        /* Advance the cursor and step to the next character (0x300314e3..0x300314f7);
         * the loop stops on NUL or when the three-glyph budget is exhausted. */
        cursorX = coduo_int32_from_bits(
            (uint32_t)cursorX + (uint32_t)charAdvance);
        char next = cp[1];
        ++cp;
        --glyphs;
        if (next == '\0') {
            break;
        }
    }
}
