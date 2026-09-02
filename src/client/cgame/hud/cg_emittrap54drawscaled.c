// Source: uo_cgame_mp_x86.dll 0x3001ce40..0x3001cf06
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001ce40_3001cf06.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_EmitTrap54DrawScaled (0x3001ce40) — a fixed-arity cgame 2D-draw emitter that
 * issues the unresolved-service cgame trap 54 (CG_R_TEXT_PAINT) with the same 10-arg
 * draw frame as the sibling emitters CG_EmitTrap54Draw (0x3001cf10) and
 * CG_Trap54DrawElement (0x3001cff0). Compared with those, this variant adds two
 * caller-controlled behaviors carried in registers: an optional CG_AdjustFrom640
 * screen-space rescale of the x / width / y coordinates (division by
 * cgs.screenXScale / cgs.screenYScale) and an optional default white color.
 *
 * Custom register+stack ABI (proven from the two call sites 0x3001ade1 and
 * 0x3002afc8, and from the entry instruction stream):
 *   EAX (reg)  : mode flag. NEG EAX; SBB EAX,EAX; AND EAX,3 maps it to
 *                (EAX == 0 ? 0 : 3) for the trailing trap argument (arg9).
 *   EDX (reg)  : adjust flag. TEST EDX,EDX selects the coordinate transform:
 *                EDX == 0 -> divide x / width / y by cgs.screenXScale/Yscale
 *                (the CG_AdjustFrom640 rescale, inlined); EDX != 0 -> pass raw.
 *   ECX (reg)  : color. Nonzero -> use ECX as the vec4 color pointer (arg5);
 *                zero -> point arg5 at a local white {1,1,1,1} instead.
 *   [E+0x04] x      : x coordinate (rescaled by 1/screenXScale when EDX == 0).
 *   [E+0x08] y      : y coordinate; forms sum = height*0.8f + y.
 *   [E+0x0C] handle : opaque dword forwarded verbatim (arg6; a text/shader handle).
 *   [E+0x10] width  : width (rescaled by 1/screenXScale when EDX == 0), -> arg7.
 *   [E+0x14] height : height; feeds sum (height*0.8f) and the scale height/48.0f
 *                     (rescaled by 1/screenYScale when EDX == 0), -> arg4.
 *   [E+0x18] extra  : opaque dword forwarded verbatim (arg8).
 * (E = entry ESP; the SUB ESP,0x10 prologue allocates the local white color vec4
 * plus scratch. The trailing ADD ESP,0x38 unwinds the 10 pushed trap dwords and
 * the 0x10 prologue frame together, then a plain RET — no callee cleanup of the
 * caller stack args, matching both callers' own ADD ESP after the CALL.)
 *
 * Outgoing trap frame, proven by simulating the interleaved PUSH/MOV stores
 * (ascending memory = argument order, id first):
 *   arg0 = 54                         (PUSH 0x36)
 *   arg1 = x                          ([E+4], /screenXScale when EDX==0)
 *   arg2 = height*0.8f + y            (FLD [E+0x14]; FMUL 0.8f; FADD [E+8];
 *                                      /screenYScale when EDX==0)   (Z)
 *   arg3 = 5                          (PUSH 5)
 *   arg4 = height/48.0f               (FLD [E+0x14]; FMUL 1/48; /screenYScale
 *                                      when EDX==0)                 (Y)
 *   arg5 = &color                     (ECX, or &local white)
 *   arg6 = handle                     ([E+0xC], verbatim)
 *   arg7 = width                      ([E+0x10], /screenXScale when EDX==0) (X)
 *   arg8 = extra                      ([E+0x18], verbatim)
 *   arg9 = (EAX==0 ? 0 : 3)           (NEG/SBB/AND 3)
 *
 * All float arguments are forwarded to the variadic trap as their raw 32-bit
 * dword bit patterns (the i386 code FSTPs a 4-byte float then PUSHes/MOVes the
 * word, never promoting to double); CG_FloatBits reproduces that exactly.
 *
 * The x87 divide-by-scale idiom (FLD 1.0f; FDIV screenXScale; FMUL coord) computes
 * coord * (1.0f/scale) == coord/scale; expressed here as the division. 1.0f is the
 * shared .rdata constant at 0x3007bce0. The 0.8f and 1/48 multipliers are the
 * .rdata constants at 0x3007bdf0 (0x3f4ccccd) and 0x3007bf04 (0x3caaaaab).
 *
 * Name adjudication: the .mcode header's size-matched guess "G_VehInitPathPos"
 * is REJECTED. That is a server-side vehicle path-init routine; this function has
 * no vehicle/path state, takes screen coordinates in a custom register+stack ABI,
 * and dispatches the cgame VM 2D-draw syscall (trap 54) through *0x30085e9c. The
 * match was a pure size collision (0xc6) with no behavioral basis (the contract
 * forbids size-based naming). The engine service behind trap 54 is unproven (no
 * cgame syscall-id table recovered), so the function and the trap keep honest
 * role/id names, matching the existing CG_EmitTrap54Draw documentation.
 */

/* Fixed CG_R_TEXT_PAINT draw parameters, proven from the pushed immediates. */
enum {
    CG_DRAW54_STYLE     = 5, /* PUSH 5  (arg3) style/font id                    */
    CG_DRAW54_MODE_ON   = 3, /* AND EAX,3 result when the mode flag is nonzero  */
    CG_DRAW54_MODE_OFF  = 0, /* AND EAX,3 result when the mode flag is zero     */
};
#define CG_DRAW54_SUM_SCALE   0.8f          /* 0x3007bdf0 (0x3f4ccccd)          */
#define CG_DRAW54_HEIGHT_SCALE (1.0f/48.0f) /* 0x3007bf04 (0x3caaaaab)          */

void CG_EmitTrap54DrawScaled(int modeFlag, int adjustFlag, const vec_t *color,
                             float x, float y, void *handle,
                             float width, float height, int32_t extra)
{
    qboolean useRawCoordinates = adjustFlag != 0;
    /* height*0.8f + y, then height/48.0f (arg2 / arg4 before any rescale). Both
     * chains stay on the x87 stack UNROUNDED (long double) until the outgoing
     * argument spills at 0x3001cec1..0x3001ced1. */
    long double sum =
        (long double)height * (long double)CG_DRAW54_SUM_SCALE;

    /* 0x3001ce4f..0x3001ce6a: the first three local-white dwords are stored
     * before y is added; the alpha dword follows that addition. */
    vec4_t whiteColor;
    whiteColor[0] = 1.0f;
    whiteColor[1] = 1.0f;
    whiteColor[2] = 1.0f;
    sum += (long double)y;
    whiteColor[3] = 1.0f;

    long double scale =
        (long double)height * (long double)CG_DRAW54_HEIGHT_SCALE;
    long double widthRaw;

    /* EDX == 0 -> CG_AdjustFrom640 rescale of x/width by screenXScale and of
     * y-sum/height-scale by screenYScale. The machine code computes 1.0f/scale
     * ONCE per axis (FDIV 0x3001ce84 / 0x3001ce9a) and MULTIPLIES the coordinates
     * by the shared 80-bit reciprocal (not a per-coordinate divide). Asymmetry
     * proven from the bytes: the y-sum multiplies the UNROUNDED reciprocal
     * (FMULP ST3, 0x3001cea4) while the height-scale multiplies the reciprocal
     * ROUNDED to float (FST 0x3001cea0 / FLD 0x3001ceaa / FMULP ST2). */
    if (!useRawCoordinates) {
        long double invX =
            (long double)1.0f / (long double)cgs_screenXScale;
        x = (float)((long double)x * invX);             /* FSTP 0x3001ce90 */
        {
            long double invY =
                (long double)1.0f / (long double)cgs_screenYScale;
            float invYRounded = (float)invY;               /* FST 0x3001cea0  */
            sum   = sum * invY;                            /* 0x3001cea4      */
            widthRaw = (long double)width * invX;
            scale = scale * invYRounded;                   /* 0x3001ceae      */
        }
    } else {
        widthRaw = (long double)width;
    }

    const vec_t *colorArg = color ? color : whiteColor;
    int32_t xBits = CG_FloatBits(x);
    float widthOut = (float)widthRaw;

    /* NEG EAX; SBB EAX,EAX occurs between the width and scale spills; AND 3
     * follows the scale spill. */
    int mode = (modeFlag != 0) ? CG_DRAW54_MODE_ON : CG_DRAW54_MODE_OFF;
    float scaleOut = (float)scale;
    mode &= 3;
    float sumOut = (float)sum;

    cgame_syscall(CG_R_TEXT_PAINT,
                  xBits,                            /* arg1 */
                  CG_FloatBits(sumOut),             /* arg2 */
                  CG_DRAW54_STYLE,                  /* arg3 */
                  CG_FloatBits(scaleOut),           /* arg4 */
                  (intptr_t)colorArg,      /* arg5 */
                  (intptr_t)handle,        /* arg6 */
                  CG_FloatBits(widthOut),           /* arg7 */
                  extra,                            /* arg8 */
                  mode);                            /* arg9 */
}
