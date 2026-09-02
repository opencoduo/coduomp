// Source: uo_cgame_mp_x86.dll 0x300402b0..0x3004035f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300402b0_3004035f.mcode
//
// CG_CalcViewProjection (provisional name; address-anchored below because the
// exact original cgame symbol is not proven). Role proven from behavior:
//
//   1. Calls CG_CalcFov (0x3003ffc0), which leaves the base horizontal FOV
//      (degrees) on the x87 stack as its float return value.
//   2. Derives the vertical FOV from the horizontal FOV and the current 3D-view
//      aspect ratio (cg_refdef.width / cg_refdef.height) using the exact
//      Quake3 idiom  fovY = atan2(H, W / tan(fovX * pi/360)) * (360/pi).
//   3. Runs two mark-render accumulation passes over the per-frame impact-mark
//      list via CG_PointContents (0x30035420), toggling bit 0x20 of
//      the shared HUD flag word cg_refdef.rdflags (0x30487ac4) per pass depending
//      on whether that pass drew anything, and (on the first pass, when it drew)
//      adding a +/- sine pulse (period 2500 ms) to the two projection terms.
//   4. Caches the resulting projection-scale pair to cg_viewProjScaleA/B and the
//      FOV-normalized sensitivity term to cg_zoomSensitivity, then returns whether the
//      first mark pass drew anything (qboolean, from ESI).
//
// The .mcode size-guess name "VEH_SetupCollmap" is REJECTED: that is a server
// (game_mp.dll, vehicle.c) collision-map setup with no FOV/x87 projection math.
// This body is pure client x87 view-projection setup plus two cgame mark-render
// traps; it matches nothing about a vehicle collision-map builder. Name matched
// by call graph (callee CG_CalcFov, the FOV clamp helper) and behavior, not size.

#include <math.h>
#include "client/cgame/globals.h"          /* screen dims, cg_time, cg_fov_vmCvar.value, view globals */
#include "client/cgame/client_recovered.h" /* CG_CalcFov, CG_PointContents */
#include "compat/coduo_native_x87.h"

/*
 * cg_refdef.rdflags (0x30487ac4): the cg.refdef rdflags bitfield (shared, refs=11;
 * declared in globals.h). This function reads it, sets or clears RDF_DRAW_SKYBOX
 * once per mark-render pass, and writes it back. Only bit 0x20 is touched here;
 * other bits belong to code not reconstructed yet. Was previously aliased locally as
 * cg_hudDrawFlags/CG_HUD_MARKS_DREW (a #define over the mechanical symbol); converged
 * onto the canonical globals.h symbol cg_refdef.rdflags / RDF_DRAW_SKYBOX.
 */

/*
 * CG_PointContents's first argument here is the literal address of
 * cg_refdef.vieworg (0x30487a90), passed as an integer marker/handle argument;
 * the second is the exclude id (-1 = none), the third is the mask (0x20).
 */
#define MARK_VIEWORG_ARG (cg_refdef.vieworg)
#define MARK_EXCLUDE_NONE (-1)
#define MARK_PASS_MASK ((int32_t)RDF_DRAW_SKYBOX)

qboolean CG_CalcViewProjection(void)
{
    /*
     * CG_CalcFov returns the horizontal FOV (degrees) on the x87 stack; keep it
     * in fovX. (Instruction 0x300402b1 CALL 0x3003ffc0, then 0x300402c4 FLD ST2
     * re-reads this returned value after the two FILDs pushed W and H above it.)
     */
    long double fovX = (long double)CG_CalcFov();

    /* Screen (3D-view) dimensions, converted int -> float (FILD). */
    long double viewHeightCurrent = (long double)coduo_int32_from_bits(cg_refdef.height); /* FILD [0x30487a84] */
    long double viewWidth = (long double)coduo_int32_from_bits(cg_refdef.width);  /* FILD [0x30487a80] */

    /*
     * Vertical FOV from horizontal FOV and aspect. Exactly the x87 sequence:
     *   FMUL pi/360   -> halfFovX radians
     *   FPTAN         -> tan(halfFovX)   (and pushes 1.0, discarded by FSTP ST0)
     *   FDIVP         -> viewWidth / tan(halfFovX)
     *   FPATAN        -> atan2(viewHeightCurrent, viewWidth / tan(halfFovX))
     *   FMUL 360/pi   -> * 360/pi  => vertical FOV in degrees
     */
    long double halfFovX = fovX * (long double)DEG_TO_HALF_RAD;
    long double tangent = coduo_x87_tanl(halfFovX);
    long double fovY = coduo_x87_atan2l(viewHeightCurrent, viewWidth / tangent) * (long double)HALF_RAD_TO_DEG;

    /*
     * First mark-render pass. If it drew (nonzero), set the flag bit and pulse
     * the two projection terms by +/- sin(cg_time * 2*pi/2500); otherwise clear
     * the flag bit and leave the terms unpulsed. termA/termB below are the two
     * x87 stack values that persist across the merge point (0x30040315):
     *   drew  -> termA = fovY - pulse,   termB = fovX + pulse
     *   !drew -> termA = fovY,           termB = fovX
     */
    int32_t markFlags = (int32_t)cg_refdef.rdflags;            /* 0x300402eb MOV EAX,[..ac4] */
    int32_t drewFirst;
    long double termA = fovY; /* st0 at 0x30040315 (fovY, possibly pulsed) */
    long double termB = fovX; /* st1 at 0x30040315 (fovX, possibly pulsed) */

    if (CG_PointContents(MARK_VIEWORG_ARG, MARK_EXCLUDE_NONE, MARK_PASS_MASK) != 0) {   /* 0x300402e1 CALL, 0x300402e9 TEST */
        /* 0x300402f2 FILD feeds cg_time straight into the FMUL/FSIN with no
         * intermediate float store, so drop the explicit (float) cast: under
         * -std=c11 it would round cg_time (a ms clock that passes 2^24) where the
         * DLL keeps it exact in 80-bit. */
        long double pulse = coduo_x87_sinl((long double)coduo_int32_from_bits(cg_time) * (long double)PULSE_RATE_2500MS);
        markFlags |= RDF_DRAW_SKYBOX;                       /* 0x300402fd OR EAX,0x20 */
        termB = fovX + pulse;                                 /* 0x3004030a FADDP ST3,ST0 */
        termA = fovY - pulse;                                 /* 0x3004030c FSUBP */
        drewFirst = 1;                                        /* 0x300402f8 MOV ESI,1 */
    } else {
        markFlags &= ~RDF_DRAW_SKYBOX;                      /* 0x30040310 AND EAX,~0x20 */
        drewFirst = 0;                                        /* 0x30040313 XOR ESI,ESI */
    }
    cg_refdef.rdflags = (uint32_t)markFlags;                    /* 0x3004031e MOV [..ac4],EAX */

    /*
     * Second mark-render pass. Same three args; toggles the same flag bit by its
     * own return, but does not affect termA/termB or the returned value.
     */
    if (CG_PointContents(MARK_VIEWORG_ARG, MARK_EXCLUDE_NONE, MARK_PASS_MASK) != 0) {   /* 0x30040323 CALL, 0x3004032b TEST */
        markFlags = (int32_t)cg_refdef.rdflags | RDF_DRAW_SKYBOX;   /* 0x30040334 OR */
    } else {
        markFlags = (int32_t)cg_refdef.rdflags & ~RDF_DRAW_SKYBOX;  /* 0x30040339 AND */
    }
    cg_refdef.rdflags = (uint32_t)markFlags;                    /* 0x3004033e MOV [..ac4],EAX */

    /*
     * Cache the projection pair. Store order (x87 FLD ST1 duplicates termB, then
     * two FSTPs) writes termB first, then termA:
     *   [0x30487a88] = termB
     *   [0x30487a8c] = termA
     *   [0x30489af0] = termB / cg_fov_vmCvar.value
     */
    cg_refdef.fov_x = (float)termB;               /* 0x30040343 FSTP [..a88] */
    cg_refdef.fov_y = (float)termA;               /* 0x3004034c FSTP [..a8c] */
    cg_zoomSensitivity = (float)(termB / (long double)cg_fov_vmCvar.value); /* 0x30040352 FDIV, 0x30040358 FSTP */

    return drewFirst ? qtrue : qfalse;                        /* 0x30040349 MOV EAX,ESI */
}
