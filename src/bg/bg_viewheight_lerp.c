// Source: uo_cgame_mp_x86.dll 0x3000a970..0x3000aa68
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000a970_3000aa68.mcode
//
// PM_GetViewHeightLerp - return the [0,1] time fraction of the player's current
// viewheight (stance) transition. The Windows game body at 0x2000a730 is
// instruction-identical to this cgame body apart from relocations.
//
// Custom register ABI (proven by the entry: MOV ESI,EAX then ECX used directly,
// no stack args referenced, plain RET): EAX = fromViewheight, ECX = toViewheight;
// the result float is produced in ST0. The SUB ESP,8 / ADD ESP,8 frame holds two
// x87 spill dwords ([ESP+8] = duration, [ESP+0xc] = elapsed), not arguments, so
// the register inputs are modeled here as ordinary C parameters.
//
// The .mcode header name "PM_BeginWeaponReload" is a size guess and is REJECTED:
// this body does no weapon or reload work. It reads the playerState viewheight-
// lerp fields (viewHeightLerpTime +0xfc, viewHeightLerpTarget +0x100,
// viewHeightLerpDown +0x104) and the stance viewheights (proneViewHeight +0x574,
// crouchViewHeight +0x578, standViewHeight +0x57c) and returns a clamped
// (now - startTime) / durationMs fraction. Name adopted from the cgame_mp pmove
// viewheight cluster (PM_GetViewHeightLerp); both Mac modules retain that name.

#include "bg_pmove.h"

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select exactly one PM_GetViewHeightLerp behavior mode"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select exactly one PM_GetViewHeightLerp behavior mode"
#endif

/* -1 sentinel meaning "this viewheight argument is unspecified" (CMP r,-1). */
enum {
    VIEWHEIGHT_NONE = -1
};

/*
 * Viewheight-transition durations in milliseconds, selected by the from-stance of
 * the lerp (ps->viewHeightLerpTarget) at 0x3000a9df..0x3000aa20. Named by their
 * proven role; exact source constant names unresolved.
 */
enum {
    VIEWHEIGHT_LERP_MS_LONG = 400, /* 0x190: prone-origin lerp, and crouched-origin
                                     *        lerp when viewHeightLerpDown == 0 */
    VIEWHEIGHT_LERP_MS_SHORT = 200  /* 0xc8:  default, and crouched-origin lerp when
                                     *        viewHeightLerpDown != 0 */
};

#if defined(WINDOWS_BEHAVIOR)
long double PM_GetViewHeightLerp(int32_t fromViewheight, int32_t toViewheight)
{
    pmove_t *move = pm;      /* [0x30539850] */
    playerState_t *ps = move->ps;          /* EAX = [EDI] */

    /* EDX = ps->viewHeightLerpTime; if it is 0 there is no lerp in flight. */
    int32_t lerpStartTime = ps->viewHeightLerpTime; /* +0xfc */
    if (lerpStartTime == 0) {                    /* TEST EDX,EDX / JNZ */
        return 0.0f;                             /* FLD [0x3007bcec] == 0.0f */
    }

    /*
     * Gate at 0x3000a995..0x3000a9d3: decide whether the (from,to) viewheight
     * pair describes a recognized stance transition. A -1 in either argument
     * skips the check straight to the fraction computation. This mirrors the JZ/
     * JNZ structure exactly; the two "compute" exits fall through to the block at
     * 0x3000a9df, the "reject" exits return 0.0f.
     */
    if (fromViewheight != VIEWHEIGHT_NONE &&      /* CMP ESI,-1 / JZ compute  */
        toViewheight != VIEWHEIGHT_NONE) {      /* CMP ECX,-1 / JZ compute  */
        int32_t activeTarget = ps->viewHeightLerpTarget;
        if (toViewheight != activeTarget) {       /* CMP ECX,[+0x100] / JNZ reject */
            return 0.0f;                          /* FLD 0.0f */
        }
        int32_t crouchHeight = ps->crouchViewHeight;
        if (toViewheight == crouchHeight) {       /* CMP ECX,[+0x578] crouchViewHeight */
            /* +0x574 is proneViewHeight, +0x104 is viewHeightLerpDown. */
            qboolean recognized = qfalse;
            int32_t proneHeight = ps->proneViewHeight;
            if (fromViewheight == proneHeight) {  /* CMP ESI,[+0x574] / JNZ 0x3000a9c1 */
                int32_t lerpDown = ps->viewHeightLerpDown;
                if (lerpDown == 0) {              /* TEST ECX,ECX / JZ compute */
                    recognized = qtrue;           /* prone->crouched: compute. */
                }
            }
            if (!recognized) {
                /* 0x3000a9c1: require the from-stance to be standing. */
                int32_t standHeight = ps->standViewHeight;
                if (fromViewheight != standHeight) { /* CMP ESI,[+0x57c] / JNZ reject */
                    return 0.0f;                  /* FLD 0.0f */
                }
                int32_t lerpDown = ps->viewHeightLerpDown;
                if (lerpDown == 0) {             /* TEST ECX,ECX / JNZ compute */
                    return 0.0f;                  /* fall through to 0x3000a9d3 FLD 0.0f */
                }
                /* standing->crouched with time1 set: active lerp, compute. */
            }
        }
        /* toViewheight != crouchViewHeight: JNZ compute (0x3000a9ad). */
    }

    /*
     * 0x3000a9df: select the transition duration from the from-stance recorded in
     * viewHeightLerpTarget (+0x100).
     */
    int32_t durationMs;
    int32_t activeTarget = ps->viewHeightLerpTarget;
    int32_t proneHeight = ps->proneViewHeight;
    if (activeTarget == proneHeight) {                         /* CMP [+0x100],[+0x574] prone */
        durationMs = VIEWHEIGHT_LERP_MS_LONG;               /* 400 */
    } else {
        int32_t crouchHeight = ps->crouchViewHeight;
        if (activeTarget == crouchHeight) {                    /* CMP [+0x100],[+0x578] crouched */
            /*
             * 0x3000a9ff: NEG EAX; SBB EAX,EAX; AND EAX,0xffffff38; ADD EAX,0x190.
             * NEG sets CF = (viewHeightLerpDown != 0); SBB EAX,EAX yields 0 or -1;
             * AND with 0xffffff38 (== -200) then ADD 400 gives 200 when nonzero, 400
             * when zero.
             */
            int32_t lerpDown = ps->viewHeightLerpDown;
            durationMs = (lerpDown != 0) ? VIEWHEIGHT_LERP_MS_SHORT   /* 400 + (-200) */
                                         : VIEWHEIGHT_LERP_MS_LONG;   /* 400 + 0 */
        } else {
            durationMs = VIEWHEIGHT_LERP_MS_SHORT;    /* 0x3000aa19: 200 (0xc8) */
        }
    }

    /*
     * 0x3000aa21: fraction = (move->command.commandTime - lerpStartTime) / durationMs.
     * ECX = [EDI+0x4] = move->command.commandTime; SUB ECX,EDX; FILD elapsed; FIDIV durationMs
     * (integer divisor loaded from the [ESP+8] slot).
     */
    int32_t elapsed = coduo_int32_from_bits((uint32_t)move->command.commandTime - (uint32_t)lerpStartTime);
    /* FILD elapsed (bare, no float cast) / FIDIV durationMs (integer divisor):
     * the quotient is never stored to a float slot before the clamp/return, so
     * it stays 80-bit (long double). A float frac (or float casts on the
     * operands) would round where the DLL does not. */
    long double frac = (long double)elapsed / durationMs;

    /*
     * 0x3000aa32..: clamp to [0,1] with the x87 FCOM/FNSTSW/TEST-AH idioms.
     *   FCOM 0.0f; TEST AH,0x5; JP  -> frac < 0.0f returns 0.0f (the JP-not-taken
     *                                  path; frac == 0.0f and frac > 0.0f continue)
     *   FCOM 1.0f; TEST AH,0x41; JNZ -> frac > 1.0f returns 1.0f (the JNZ-not-taken
     *                                   path; frac <= 1.0f returns frac)
     * The two stages compose to an ordinary clamp01.
     */
    if (frac < 0.0f) {                            /* FLD [0x3007bcec] == 0.0f */
        return 0.0f;
    }
    if (frac > 1.0f) {                            /* FLD [0x3007bce0] == 1.0f */
        return 1.0f;
    }
    return frac;
}
#else
/* Linux game.mp.uo.i386.so RVA 0x00061912 follows the same transition gates
 * but stores the quotient to binary32 at RVA 0x00061a50 before the clamp and
 * return. The long-double return type is the cross-platform ST0 carrier; this
 * body deliberately retains the Linux store through its `fraction` local. */
long double PM_GetViewHeightLerp(int32_t fromViewheight, int32_t toViewheight)
{
    float fraction;
    int32_t lerpTime;

    if (pm->ps->viewHeightLerpTime == 0) {
        return 0.0f;
    }

    if ((fromViewheight == VIEWHEIGHT_NONE || toViewheight == VIEWHEIGHT_NONE) ||
        (toViewheight == pm->ps->viewHeightLerpTarget &&
         ((toViewheight != pm->ps->crouchViewHeight || (fromViewheight == pm->ps->proneViewHeight && pm->ps->viewHeightLerpDown == 0)) ||
          (fromViewheight == pm->ps->standViewHeight && pm->ps->viewHeightLerpDown != 0)))) {
        int32_t elapsed;

        lerpTime = PM_GetViewHeightLerpTime(pm->ps, pm->ps->viewHeightLerpTarget, pm->ps->viewHeightLerpDown);
        elapsed = coduo_int32_from_bits((uint32_t)pm->command.commandTime - (uint32_t)pm->ps->viewHeightLerpTime);
#if EMULATE_X87
        fraction = x87f_store_f32(x87f_div(x87f_load_i32(elapsed), x87f_load_i32(lerpTime)));
#else
        fraction = (float)((long double)elapsed / (long double)lerpTime);
#endif
        if (fraction < 0.0f) {
            fraction = 0.0f;
        } else if (fraction > 1.0f) {
            fraction = 1.0f;
        }
        return fraction;
    }

    return 0.0f;
}
#endif
