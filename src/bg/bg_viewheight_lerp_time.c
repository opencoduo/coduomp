// Source: uo_cgame_mp_x86.dll 0x3000a8b0..0x3000a8df
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000a8b0_3000a8df.mcode
//
// PM_GetViewHeightLerpTime - given the player's from-stance viewheight and the
// viewHeightLerpDown flag, return the viewheight (stance) transition duration in
// milliseconds. This is exactly the duration-selection block that
// PM_GetViewHeightLerp (0x3000a970) also performs inline at 0x3000a9df..0x3000aa20
// (see functions/FUN_3000a970_3000aa68.c); here it is factored out as a standalone
// callee, invoked twice from the pmove viewheight-adjust step in
// FUN_3000aa70_3000b003 (0x3000aba1, 0x3000ae3e). Both call sites pass
// ps->viewHeightLerpTarget (+0x100) as the from-viewheight and
// ps->viewHeightLerpDown (+0x104) as the flag, with ECX = ps.
//
// Custom register ABI (proven by the body: CMP EAX,[ECX+..] uses EAX and ECX
// directly, the flag is read from [ESP+4], plain RET): EAX = fromViewheight,
// ECX = ps (playerState_t*), [ESP+4] = viewHeightLerpDown. Result (an int32
// millisecond count) is returned in EAX. Modeled here as ordinary C parameters.
//
// The .mcode size-guess "ClampChar" is REJECTED: this body does no byte clamping.
// It classifies a viewheight value against the playerState stance viewheights
// (proneViewHeight +0x574, crouchViewHeight +0x578) and returns a lerp duration
// code (400 or 200 ms). The Mac PM_GetViewHeightLerpTime performs the same
// stance-duration selection, resolving the source name.
//
// The Windows cgame body at 0x3000a8b0 and Windows game body at 0x2000a670
// are instruction-identical. Linux game.mp.uo.i386.so 0x00061bfe has the same
// comparisons and return values. The Linux ELF cdecl order supplies the shared
// source signature: player state, from-height, transition mode.

#include "bg_pmove.h"

/*
 * Viewheight-transition durations in milliseconds, selected by the lerp's
 * from-stance. Same constants as the inline copy in PM_GetViewHeightLerp; named by
 * their proven role, exact source constant names unresolved.
 */
enum {
    VIEWHEIGHT_LERP_MS_LONG  = 400, /* 0x190: prone-origin lerp, and crouched-origin
                                     *        lerp when viewHeightLerpDown == 0 */
    VIEWHEIGHT_LERP_MS_SHORT = 200  /* 0xc8:  default, and crouched-origin lerp when
                                     *        viewHeightLerpDown != 0 */
};

int32_t PM_GetViewHeightLerpTime(const playerState_t *ps,
                                 int32_t fromViewheight,
                                 int32_t viewHeightLerpDown)
{
    /* 0x3000a8b0: CMP EAX,[ECX+0x574] / JNZ / MOV EAX,0x190 / RET. */
    if (fromViewheight == ps->proneViewHeight) {          /* +0x574 proneViewHeight */
        return VIEWHEIGHT_LERP_MS_LONG;                 /* 400 */
    }

    /* 0x3000a8be: CMP EAX,[ECX+0x578] / JNZ 0x3000a8d9. */
    if (fromViewheight == ps->crouchViewHeight) {          /* +0x578 crouchViewHeight */
        /*
         * 0x3000a8c6: MOV EAX,[ESP+4]; NEG EAX; SBB EAX,EAX; AND EAX,0xffffff38;
         * ADD EAX,0x190. NEG sets CF = (viewHeightLerpDown != 0); SBB EAX,EAX
         * yields 0 or -1; AND with 0xffffff38 (== -200) then ADD 400 gives 200
         * when the flag is nonzero, 400 when it is zero.
         */
        return (viewHeightLerpDown != 0)
                   ? VIEWHEIGHT_LERP_MS_SHORT   /* 400 + (-200) */
                   : VIEWHEIGHT_LERP_MS_LONG;   /* 400 + 0 */
    }

    /* 0x3000a8d9: MOV EAX,0xc8 / RET. */
    return VIEWHEIGHT_LERP_MS_SHORT;                     /* 200 */
}
