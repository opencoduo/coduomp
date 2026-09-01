#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30018770..0x30018a0a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30018770_30018a0a.mcode
//
// CG_DrawTeamInfo: draw the team-chat scroll ring at the top-left of the HUD.
// Each still-visible ring line (teamChatMsgs[]) is drawn as a dim, fading
// full-width hudColorBar background strip with the wrapped chat text over it.
// This is the id-Tech CG_DrawTeamInfo, drawn every visible frame by
// CG_Draw2D (0x3001bfe0, calls at 0x3001c047 and 0x3001c074).
//
// The .mcode name "Reached_BinaryMover" is a pure size match against a
// game_mp.dll (server mover) function (win size 0x29a ~= 0x29c) and is REJECTED:
// this is a cgame 2D HUD drawer, not a server mover. The prior caller-observed
// name CG_DrawWarmup is likewise wrong. The identity is proven by the exclusive
// team-chat state it touches: cg_chatHeight_vmCvar.integer (clamp), teamChatPos /
// teamChatLastPos (the scroll ring cursors), teamChatMsgTimes[] (per-line
// timestamps), cg_chatTime_vmCvar.integer / cg_chatTime_vmCvar.value (the fade window),
// teamChatMsgs[] (the 8x271-byte line ring), and the cgs.media hudColorBar
// shader (0x3044b6c0) used for the background strip.
//
// Constants (dumped via objdump -s -j .rdata):
//   0x3007bce0 = 1.0f   0x3007bcec = 0.0f    0x3007bda4 = 10.0f (row height)
//   0x3007be0c = 0.6f (bar alpha scale)      0x3007be58 = 0.25f (bar color dim)
//   0x3007bf40 = 0x3ba3d70a = 0.005f (fade ramp, 1/200)
//   0x3007bf44 = 0x43480000 = 200.0f (fade knee)
//   immediates: 0x3e555555 = 0.20833333f = 1/4.8 (text scale),
//               0x41000000 = 8.0f (text x-origin / glyph size)
//
// Traps issued directly through the VM syscall pointer *0x30085e9c:
//   0x5f  CG_CL_LOOKUP_COLOR  : resolve a "^N" color-code digit -> RGBA vector (out ptr)
//   0x34  CG_R_TEXT_WIDTH  : R_TextWidth-style text metric (used to size the strip)
//   0x48  CG_R_SETCOLOR (72): set the 2D draw color for the following draws
//   0x36  CG_R_TEXT_PAINT  : the 2D text/string draw
//   plus CG_R_DRAWSTRETCHPIC via trap_R_DrawStretchPic (0x3003e0f0) for the strip.
//
// Every screen coordinate is scaled from the 480x640 virtual space to real
// pixels via cgs_screenXScale / cgs_screenYScale, exactly like CG_DrawPic.
//
// The dense per-line ESP-relative stack shuffling was resolved by tracking the
// frame pointer through the loop body; the slot roles below are base-relative
// (base = the loop-top ESP), not the raw [ESP+X] literals.
void CG_DrawTeamInfo(void)
{

    // 0x30018770..0x30018810: clamp the visible-line count to [1..8]. A count of
    // 0 (or negative) means the team-chat HUD is off -> nothing to draw.
    int32_t chatHeight = cg_chatHeight_vmCvar.integer; /* 0x30018770 [0x30452d2c] */
    if (chatHeight >= 8) {                   /* 0x30018778/0x30018808 */
        chatHeight = 8;                      /* TEAMCHAT_HEIGHT */
    }
    if (chatHeight <= 0) {                    /* 0x30018784..0x30018786 */
        return;
    }

    int32_t lastPos = teamChatLastPos;       /* 0x3001878c ECX = [0x3044b684] */
    int32_t pos = teamChatPos;               /* 0x30018793 EDI = [0x3044b680] */
    if (lastPos == pos) {                     /* 0x30018799..0x3001879b: ring empty */
        return;
    }

    // 0x300187a1..0x300187c0: scroll the oldest still-visible line out once it has
    // aged past the visible window. teamChatMsgTimes is indexed by (cursor mod
    // chatHeight); cg_chatTime_vmCvar.integer is the integer visible-window (ms).
    if ((int32_t)((uint32_t)cg_time -
                  (uint32_t)teamChatMsgTimes[lastPos % chatHeight]) >
        cg_chatTime_vmCvar.integer) {
        lastPos = coduo_int32_from_bits(
            (uint32_t)lastPos + 1u);          /* 0x300187bf INC ECX */
        teamChatLastPos = lastPos;            /* 0x300187c0 store [0x3044b684] */
    }

    // 0x300187c6..0x300187c9: draw newest-first, from teamChatPos-1 down to
    // teamChatLastPos (id-Tech's `for (i = pos-1; i >= lastPos; i--)`).
    int32_t i = coduo_int32_from_bits((uint32_t)pos - 1u);
    while (i >= lastPos) {
        int32_t ringIndex = i % chatHeight;   /* 0x300187d0..0x300187d3 */

        // 0x300187d5..0x30018827: per-line fade.
        //   age  = cg_time - teamChatMsgTimes[ringIndex]      (ms this line has shown)
        //   left = cg_chatTime_vmCvar.value - age                  (fade budget remaining)
        // While left is still large (> 200 ms) the line is fully opaque (alpha = 1).
        // Once left drops below the 200 ms knee, alpha ramps as left * 0.005f
        // (= left/200) and the line is skipped once that product falls to <= 0.
        // `left` is never stored to a float slot: FILD age / FSUBR cg_chatTime
        // (0x300187e5/e9) leaves it in st0, the 200.0f FCOM (0x300187ef) reads it
        // unrounded, and the opaque path discards it with FSTP ST0 (0x300187fc).
        // `alphaChain` likewise stays 80-bit through the FCOMP: the FST at
        // 0x30018818 keeps st0 (it is FST, not FSTP), so the <= 0.0 test at
        // 0x3001881c runs on the UNROUNDED product while the float copy in
        // [ESP+0xc] is what the later 0.6f multiply reloads (FLD 0x300188d4).
        int32_t age = (int32_t)(
            (uint32_t)cg_time - (uint32_t)teamChatMsgTimes[ringIndex]);
        /* 0x300187e5 FILD age; 0x300187e9 FSUBR cg_chatTime -- age is loaded straight
         * into the FSUBR with no FSTP DWORD, so the implicit int->float conversion
         * stays exact; no (float) cast (it would round age under -std=c11, and age is
         * a live cg_time delta that can exceed 2^24). */
        long double left =
            (long double)cg_chatTime_vmCvar.value -
            (long double)age;  /* 0x300187e5..0x300187e9 */
        float alpha;
        if (left > 200.0f) {                  /* FCOM [0x3007bf44] = 0x43480000 = 200.0f */
            alpha = 1.0f;                     /* 0x300187fe base+0xc = 1.0 */
        } else {
            long double alphaChain = left * 0.005f; /* FMUL [0x3007bf40] = 0.005f (1/200) */
            alpha = (float)alphaChain;        /* 0x30018818 FST [ESP+0xc] (no pop) */
            if (alphaChain <= 0.0f) {         /* 0x3001881c..0x30018827: gone -> skip */
                i = coduo_int32_from_bits((uint32_t)i - 1u);
                continue;                     /* JNP 0x300189ed (fall to DEC/loop test) */
            }
        }

        // 0x3001882d..0x30018871: the line's text color. Default white (1,1,1,1). If
        // the line begins with a "^N" color escape (N a digit '0'..'9', and not
        // "^^"), trap 0x5f writes a complete RGBA vec4. The following FLD/FLD/FXCH chain
        // restores the ordinary r,g order before the component multiplies.
        char *line = teamChatMsgs[ringIndex]; /* 0x3001882d..0x30018839 (stride 0x10f) */
        vec4_t lineColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        if (line != NULL) {
            int8_t colorCode = (int8_t)line[1];
            if ((int8_t)line[0] == '^' && colorCode != '\0' &&
                colorCode != '^' && colorCode >= '0' && colorCode <= '9') { /* 0x3001883d..0x30018853 */
                cgame_syscall(CG_CL_LOOKUP_COLOR, (int32_t)(uint8_t)colorCode,
                              (intptr_t)&lineColor[0]);   /* 0x30018855..0x30018866 */
            }
        }

        // 0x30018887..0x30018906: build the background strip's color/alpha and its
        // size. The strip color is the line color dimmed to 25%, with the fade
        // alpha scaled by 0.6, laid out as a contiguous RGBA vec4 passed to
        // R_SetColor. Its virtual geometry:
        //   yBar   = 10*fromTop + 84   (rows are 10px tall, stacked downward)
        //   width  = R_TextWidth(line) + 24   (text width plus padding, virtual px)
        int32_t fromTop = coduo_int32_from_bits(
            (uint32_t)i - (uint32_t)teamChatPos);             /* 0x30018895..0x30018897 */
        int32_t yBar = coduo_int32_from_bits(
            (uint32_t)fromTop * 10u + 84u);                   /* 0x3001889f/0x300188b5 LEA */
        float barColor[4];                                    /* base+0x28..0x34, RGBA */
        barColor[0] = (float)((long double)lineColor[0] * 0.25L); /* 0x3001888d/0x3001888f */
        barColor[1] = (float)((long double)lineColor[1] * 0.25L); /* 0x300188aa */
        barColor[2] = (float)((long double)lineColor[2] * 0.25L); /* 0x300188c1/0x300188c7 */
        barColor[3] = (float)((long double)alpha *
                              (long double)0.6f);             /* 0x300188d4/0x300188d8 */

        int32_t textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_TEXT_WIDTH, (intptr_t)line,
            0, 0x3e555555, 0));               /* font 0, scale 0.20833, max 0 */
        int32_t barChars = coduo_int32_from_bits(
            (uint32_t)textWidth + 24u);                       /* 0x300188f0 +24 */
        float barWidth = (float)barChars;                     /* 0x300188f7 FILD */

        // 0x300188fb..0x30018906: install the strip color for the draw below.
        cgame_syscall(CG_R_SETCOLOR, (intptr_t)&barColor[0]); /* trap 0x48, &{r,g,b,a} */

        // 0x3001890c..0x30018962: draw the full background strip via
        // trap_R_DrawStretchPic(hudColorBar) with texcoords (0,0)-(1,1). All four
        // rect coords are screen-scaled: x = 0, y = yBar*sys, w = width*sxs,
        // h = 10.0*sys.
        qhandle_t hudColorBar = cgs_media_hudColorBar;        /* 0x3001890c [0x3044b6c0] */
        float barH = (float)((long double)cgs_screenYScale * 10.0L); /* 0x30018912..18 */
        float barW = (float)((long double)cgs_screenXScale *
                             (long double)barWidth);           /* 0x30018937..3d */
        float barY = (float)((long double)cgs_screenYScale *
                             (long double)(float)yBar);        /* 0x30018945..4b */
        float barX = (float)((long double)cgs_screenXScale * 0.0L); /* 0x30018953..59 */
        trap_R_DrawStretchPic(CG_FloatBits(barX), CG_FloatBits(barY),
                              CG_FloatBits(barW), CG_FloatBits(barH),
                              0, 0,                            /* s1=0, t1=0 */
                              CG_FloatBits(1.0f), CG_FloatBits(1.0f), /* s2=1, t2=1 */
                              (int32_t)hudColorBar);           /* 0x30018962 */

        // 0x30018967..0x300189de: draw the line text via trap 0x36. Baseline sits
        // at yText = 10*fromTop + 93 (one row below the strip top). The 2D draw
        // takes the virtual x-origin (8.0), the baseline y, style 0, the 0.20833333f
        // text scale, a whiteColor vec3 {1,1,1}, the line pointer, two zero words,
        // and mode 3.
        int32_t fromTopText = coduo_int32_from_bits(
            (uint32_t)i - (uint32_t)teamChatPos);             /* 0x3001896d..0x30018973 */
        int32_t yText = coduo_int32_from_bits(
            (uint32_t)fromTopText * 10u + 93u);               /* 0x30018977..0x30018984 */
        vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, alpha };
        cgame_syscall(CG_R_TEXT_PAINT,
                      CG_FloatBits(8.0f),                     /* x-origin (0x41000000) */
                      CG_FloatBits((float)yText),             /* baseline y */
                      0,                                      /* style */
                      0x3e555555,                             /* text scale 0.20833333 */
                      (intptr_t)&whiteColor[0],      /* &{1,1,1} */
                      (intptr_t)line,                /* text */
                      0,
                      0,
                      3);                                     /* mode */

        /* 0x300189e4 reloads the shared cursor after the two renderer calls.
         * The expired-line fast path above jumps past this reload and keeps
         * the cursor value already live in ECX. */
        lastPos = teamChatLastPos;
        i = coduo_int32_from_bits((uint32_t)i - 1u);
    }

    // 0x300189f6..0x30018a03: reset the 2D draw color to opaque white so following
    // HUD draws are not tinted by the last line's strip color.
    cgame_syscall(CG_R_SETCOLOR, 0);          /* 0x300189f6..0x300189fa */
}
