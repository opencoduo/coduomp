#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x3002f9d0..0x3002fc9e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002f9d0_3002fc9e.mcode
//
// CG_DrawStatBarWithDecay (provisional role name) — draws a 2D HUD "stat bar"
// whose fill fraction is health/maxHealth taken from the current snapshot's
// embedded playerState, clamped to [0.0,1.0], plus a trailing "ghost" segment
// that lags/decays toward the live value over time so a drop animates.
//
// Two segments are drawn with trap_R_DrawStretchPic (cgame trap 73) bracketed by
// trap_R_SetColor (trap 72): the first is the live filled bar, the second is the
// decaying trailing indicator. The bar rectangle and its base color come in as
// register arguments from the caller's HUD-element dispatcher (0x300324e3):
//   ESI = float color[4]   (base r,g,b,a; the routine overwrites r/g/b in place)
//   EDI = float rect[4]    (x, y, w, h in virtual-640x480 UI space)
//   stack arg0 = the shader handle passed through as trap_R_DrawStretchPic's hShader
// so the C signature reflects that non-cdecl register-argument convention rather
// than inventing stack parameters. (EBP/EBX are the only callee-saved registers the
// function preserves; ESI/EDI are inherited inputs, matching the dispatcher which
// sets them up before `call 0x3002f9d0`.)
//
// NAME ADJUDICATION: the .mcode header's `# name StopFollowing` is a size-match
// guess (win 0x2ce vs matched 0x2c9) and is REJECTED per the naming rules: the
// callee at 0x3003e0f0 is the 2D draw trap trap_R_DrawStretchPic and the routine
// only reads a snapshot playerState and paints two bars, whereas StopFollowing in
// game_mp_uo is a server gentity_t player routine (client_commands.c). This is a
// cgame 2D HUD draw, not StopFollowing. The exact original name is unresolved; the
// role (health bar with a time-decaying trailing indicator) is proven, so a role
// name is used. At 0x3002f9d4 EBX still holds cg_snap when [EBX+0x128] loads the
// numerator, making it snapshot+0x128 == playerState+0x11c (health). Only after
// that load does 0x3002f9e0 advance EBX by 0x0c to the embedded playerState; the
// denominator load [EBX+0x124] is therefore
// playerState.stats[STAT_MAX_HEALTH].
//
// FPU compare idioms (proven from the FNSTSW/TEST/Jcc bytes): after FCOM(P) m the
// x87 status word carries C0=(ST<m) as AH bit0 (0x01), C3=(ST==m) as AH bit6
// (0x40), C2=unordered as AH bit2 (0x04). Hence:
//   TEST AH,0x41; JNZ  -> branch when ST <= m       (C0|C3 set)
//   TEST AH,0x41; JP   -> branch when ST >  m        (neither C0 nor C3, even parity)
//   TEST AH,0x05; JP   -> branch when ST >= m        (C0 clear, ordered, even parity)
// These are the ordinary MSVC x87 lowerings of >=, >, <= float comparisons.

/* 0x3007bea4 (.rdata, refs=1, this function only): the per-frame decay rate of the
 * trailing bar, ~0.0012 fill-fraction per millisecond of cg.frametime. Used below as
 * a float literal in natural source form (bit pattern 0x3a9d4952) rather than the
 * mechanical g_const_float_0_0012000001 symbol. */

_Static_assert(offsetof(snapshot_t, ps.stats[STAT_HEALTH]) == 0x128, "snapshot health at +0x128");
_Static_assert(offsetof(playerState_t, stats[STAT_MAX_HEALTH]) == 0x124, "playerState maxHealth at +0x124");
_Static_assert(offsetof(playerState_t, psClientNum) == 0xd4, "playerState client number at +0xd4");

void CG_DrawStatBarWithDecay(float *color /* ESI */, const rectDef_t *rect /* EDI */, int32_t hShader)
{
    /* 0x3002f9d4 loads cg_snap into EBX. The health load at 0x3002f9da occurs
     * before 0x3002f9e0 adds 0x0c and turns EBX into the playerState base. */
    const snapshot_t *snapshot = cg_snap;
    const playerState_t *ps = &snapshot->ps;

    /* .rdata float pool @0x3007bce0 (bytes dumped from the DLL):
     *   0x3007bce0 = 0x3f800000 = 1.0f
     *   0x3007bce4 = 0x40000000 = 2.0f
     *   0x3007bce8 = 0x3f000000 = 0.5f
     *   0x3007bcec = 0x00000000 = 0.0f
     * This function references only bce0 (1.0), bce8 (0.5) and bcec (0.0); 2.0f
     * at bce4 is NOT read anywhere in 0x3002f9d0..0x3002fc9e. */
    const float zero = 0.0f;      /* 0x3007bcec */
    const float one = 1.0f;      /* 0x3007bce0 / 0x3f800000 */
    const float half = 0.5f;      /* 0x3007bce8 */
    const float decayRatePerMs = 0.0012000001f; /* 0x3007bea4 */

    int32_t health = snapshot->ps.stats[STAT_HEALTH]; /* pre-rebase [EBX+0x128] */
    int32_t clientNum;

    /* [ESP+0x08]: the live fill fraction (also forwarded as the s2 texcoord / EBP).
     * [ESP+0x10]: cached scaled filled-width used by the trailing-segment draw. */
    float frac;            /* [ESP+0x08] */
    float scaledFillW; /* [ESP+0x10]; only written on the first-draw path */
    /* barWidthFrac is the surviving x87 ST(0) across 0x3002fa1f..0x3002fb3a: it is
     * never stored to a float slot (the FCOM at 0x3002fa73 and the FSUBR at
     * 0x3002fa80 both consume it unrounded, and the unused path discards it with
     * FSTP ST0 at 0x3002fb3a), so it is long double, not float. */
    long double barWidthFrac; /* surviving x87 ST(0): the width multiplier of segment 1 */
    int drawFirst;       /* whether the first (live) segment is painted */

    // 0x3002f9da..0x3002fa11: compute frac = health/maxHealth, but only if both are
    // nonzero; the quotient is then clamped into [0.0, 1.0].
    if (health != 0 && ps->stats[STAT_MAX_HEALTH] != 0) {
        /* FST-keep (0x3002fa02 FST, not FSTP): the float copy lands in [ESP+8]
         * while the FCOMP at 0x3002fa06 tests the UNROUNDED 80-bit quotient
         * against 0.0f (0x3007bcec), so the sign test uses the raw chain. */
        /* 0x3002f9fa FILD health; 0x3002f9fe FIDIV maxHealth -- health is FILD'd
         * exact and maxHealth is an integer divisor (FIDIV), neither rounded to
         * float before the divide (no FSTP DWORD). (long double)health keeps the
         * dividend exact and the implicit conversion keeps the divisor exact; a
         * (float) cast on either would round it under -std=c11. */
        long double quotient = (long double)health / ps->stats[STAT_MAX_HEALTH]; /* FILD/FIDIV */
        frac = (float)quotient;      /* 0x3002fa02 FST [ESP+8] */
        /* TEST AH,0x05; JP accepts ordered >= and unordered. The integer
         * numerator/divisor guards make unordered unreachable in normal state,
         * but preserve the exact condition rather than silently changing it. */
        if (!(quotient < zero)) {
            // 0x3002fa11 JP 0x3002fa98: clamp the live fraction into [0.0,1.0].
            // 0x3002fa98: if (frac > 1.0) frac = 1.0;
            if (frac > one) {
                frac = one;              /* [ESP+8] = 1.0f (0x3002faa9) */
                barWidthFrac = frac;     /* reload path 0x3002fa1f */
            } else if (frac < zero) {
                /* 0x3002fab6/0x3002fac7: the clamp's low arm loads 0.0f into ST0
                 * and jumps to 0x3002fa23 WITHOUT rewriting [ESP+8] -- so the
                 * width multiplier is clamped while `frac` keeps its raw value.
                 * (Unreachable in practice: this arm is guarded by the
                 * quotient >= 0.0 test above. The machine code emits the full
                 * two-sided clamp regardless; preserved as-is.) */
                barWidthFrac = zero;     /* 0x3002fac7 FLD 0.0f -> 0x3002fa23 */
            } else {
                // 0x3002fad2: frac in [0.0,1.0]; the general clamp keeps it.
                barWidthFrac = frac;     /* 0x3002fad2 -> 0x3002fa1f: ST0 = [ESP+8] */
            }
        } else {
            frac = zero;                 /* 0x3002fa17 */
            barWidthFrac = frac;
        }
    } else {
        frac = zero;                     /* 0x3002fa17 */
        barWidthFrac = frac;
    }

    /* 0x3002fa23 FLD [ESP+8]; 0x3002fa2b FCOMP 0.0f (0x3007bcec); TEST AH,0x41;
     * JNZ 0x3002fb3a. The FCOMP pops the freshly-loaded [ESP+8] copy, so the test
     * is on `frac` while the SURVIVING st0 is the clamp value (barWidthFrac). The
     * live segment is painted only when frac > 0.0. */
    drawFirst = (frac > zero);

    if (drawFirst) {
        // 0x3002fa3c..0x3002fa6f: build the four scaled coordinates of segment 1.
        float x = cgs_screenXScale * rect->x;      /* [ESP+0x18] */
        float y = cgs_screenYScale * rect->y;      /* [ESP+0x14] */
        scaledFillW = (float)((long double)frac * (long double)rect->w * (long double)cgs_screenXScale); /* [ESP+0x10] */
        float h = cgs_screenYScale * rect->h;      /* [ESP+0x0c] */

        // 0x3002fa73 FCOM 0.5f (0x3007bce8); TEST AH,0x41; JNZ 0x3002faf2 -> the
        // green path runs when barWidthFrac <= 0.5, the white-fade path when it is
        // greater. The compare reads the UNROUNDED surviving st0.
        if (barWidthFrac > half) {
            // 0x3002fa80: fade the low bar toward white by (1.0-frac):
            //   t = 1.0 - barWidthFrac; color[0] = 2*(t*color[0]); color[2] = 2*(t*color[2]);
            // t is never stored (FSUBR leaves it in st0, FLD ST0 duplicates it), so
            // it is long double; each color component rounds once, at its own FSTP.
            long double t = one - barWidthFrac;   /* FSUBR 1.0 : ST0 = 1.0 - ST0 */
            color[0] = (t * color[0]) + (t * color[0]); /* FLD ST0; FMUL [ESI]; FADD ST0,ST0 */
            color[2] = (t * color[2]) + (t * color[2]); /* FMUL [ESI+8]; FADD ST0,ST0 */
        } else {
            // 0x3002faf2: color[1] = (barWidthFrac + 0.2) * color[1] + 0.3;
            color[1] = ((barWidthFrac + 0.2f) * color[1]) + 0.3f; /* 0x3007be10=0.2f, 0x3007bea0=0.3f */
        }

        // 0x3002fb04..0x3002fb35: trap_R_SetColor(color); draw segment 1; the two
        // trap-72 arg dwords stay on the stack and are cleaned with the nine draw
        // args by the single ADD ESP,0x2c.
        trap_R_SetColor(color);
        trap_R_DrawStretchPic(CG_FloatBits(x),                 /* arg1 x */
                              CG_FloatBits(y),                 /* arg2 y */
                              CG_FloatBits(scaledFillW),       /* arg3 w */
                              CG_FloatBits(h),                 /* arg4 h */
                              CG_FloatBits(0.0f),              /* arg5 s1 */
                              CG_FloatBits(0.0f),              /* arg6 t1 */
                              CG_FloatBits(frac),              /* arg7 s2 = live frac (EBP) */
                              CG_FloatBits(1.0f),              /* arg8 t2 */
                              hShader);                        /* arg9 */
    }
    /* else 0x3002fb3a FSTP ST0: discard the unused width value from the x87 stack. */

    // 0x3002fb3c: read the local client number and reset the smoothing state when it
    // changes (a new local player / demo seek invalidates the trailing bar).
    clientNum = ps->psClientNum;                          /* [EBX+0xd4] */
    if (cg_statBarLastClientNum != clientNum) {
        // 0x3002fb4a: snap the trailing value to the live one and re-seed the hold.
        cg_statBarDisplayFrac = frac;                     /* 0x30134d04 = [ESP+8] */
        cg_statBarLastClientNum = clientNum;              /* 0x30085db4 = clientNum */
        cg_statBarHoldTimer = cg_statBarHoldSeed;         /* 0x30085db0 = seed(0x30085dac) */
    } else {
        // 0x3002fb69: same client — advance the trailing value toward the live frac.
        /* TEST AH,0x05; JP takes the snap path for >= and unordered. */
        if (!(frac < cg_statBarDisplayFrac)) {
            // 0x3002fbd7: bar rose (or equal) — snap trailing up to live, re-seed hold.
            cg_statBarDisplayFrac = frac;                 /* 0x30134d04 = [ESP+8] */
            cg_statBarHoldTimer = cg_statBarHoldSeed;     /* 0x30085db0 = seed */
        } else if (cg_statBarHoldTimer != 0) {
            // 0x3002fb7a/0x3002fb83: hold the trailing value while the timer runs;
            // decrement by cg.frametime (ms) and floor at 0.
            int32_t held = coduo_int32_from_bits((uint32_t)cg_statBarHoldTimer - (uint32_t)cg_frametime); /* SUB [0x304831ac] */
            cg_statBarHoldTimer = held;
            if (held < 0) {                                /* JNS: keep when >= 0 */
                cg_statBarHoldTimer = 0;                   /* 0x3002fb90 */
            }
        } else {
            // 0x3002fb9c: hold expired — decay the trailing value toward the live
            //   display = display - (cg.frametime * decayRatePerMs);
            //   if (display <= frac) { display = frac; re-seed hold; }
            // FST-keep (0x3002fbae FST, not FSTP): the store to cg_statBarDisplayFrac
            // rounds a copy to float, but the FCOMP at 0x3002fbb4 tests the
            // UNROUNDED 80-bit chain, so the compare runs on `display` as long double.
            /* 0x3002fb9c FILD cg_frametime; FMUL decayRatePerMs -- cg_frametime is
             * FILD'd straight into the multiply (no FSTP DWORD), so no (float) cast. */
            long double display = (long double)cg_statBarDisplayFrac - (long double)coduo_int32_from_bits((uint32_t)cg_frametime) *
                                                                           (long double)decayRatePerMs; /* FILD;FMUL;FSUBR */
            cg_statBarDisplayFrac = (float)display;        /* 0x3002fbae FST (no pop) */
            if (display <= frac) {                         /* FCOMP [ESP+8]; !(display>frac) */
                cg_statBarDisplayFrac = frac;                 /* 0x3002fbbf */
                cg_statBarHoldTimer = cg_statBarHoldSeed;     /* 0x3002fbcf */
            }
        }
    }

    // 0x3002fbec: draw the trailing (ghost) segment only while it still leads the
    // live fill, i.e. display > frac.
    if (cg_statBarDisplayFrac > frac) {
        float display = cg_statBarDisplayFrac;
        // 0x3002fc01..0x3002fc52: coordinates of the trailing segment.
        float trailX =
            (float)(((long double)frac * (long double)rect->w + (long double)rect->x) * (long double)cgs_screenXScale); /* [ESP+0x20] */
        float trailY = cgs_screenYScale * rect->y; /* [ESP+0x1c] */
        /* 0x3002fc2c executes after two pushes, so [esp+0x10] aliases the
         * original live-frac slot [esp+0x08], not scaledFillW [esp+0x10]. */
        float trailW =
            (float)(((long double)display - (long double)frac) * (long double)rect->w * (long double)cgs_screenXScale); /* [ESP+0x18] */
        float trailH = cgs_screenYScale * rect->h; /* [ESP+0x14] */

        // 0x3002fc46: force the trailing bar RGB to red (1,0,0), retaining
        // the caller's alpha; EAX=0 from XOR sets color[1],color[2].
        color[0] = 1.0f; /* MOV [ESI],0x3f800000 */
        color[1] = 0.0f; /* MOV [ESI+4],EAX(=0) */
        color[2] = 0.0f; /* MOV [ESI+8],EAX(=0) */

        trap_R_SetColor(color);
        trap_R_DrawStretchPic(CG_FloatBits(trailX), /* arg1 x */
                              CG_FloatBits(trailY), /* arg2 y */
                              CG_FloatBits(trailW), /* arg3 w */
                              CG_FloatBits(trailH), /* arg4 h */
                              CG_FloatBits(frac), /* arg5 s1 = live frac */
                              CG_FloatBits(0.0f), /* arg6 t1 */
                              CG_FloatBits(display), /* arg7 s2 = trailing frac */
                              CG_FloatBits(1.0f), /* arg8 t2 */
                              hShader); /* arg9 */
    }

    // 0x3002fc8b: reset the 2D draw color to opaque white (trap_R_SetColor(NULL)).
    trap_R_SetColor(NULL);
}
