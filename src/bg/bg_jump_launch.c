#include "bg_pmove.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>
#include <stdint.h>

// Sources: uo_cgame_mp_x86.dll 0x30008c70..0x30008cf8,
//          uo_game_mp_x86.dll  0x20008a20..0x20008aa8,
//          game.mp.uo.i386.so  RVA 0x00024380..0x00024482
//
// Name adjudication: the .mcode bank labels this Scr_GetGameTypeNameForScript
// with the note "win size 0x89, matched size 0x89". That is a pure SIZE match and
// is REJECTED per the no-size-matching rule (Scr_GetGameTypeNameForScript is a
// game-type-string lookup returning const char *; this function does x87 physics
// math on the pmove player state and returns void). The behavior is a jump launch.
//
// Resolved name: PM_Jump. Evidence, all from the machine code:
//   * The function operates on the BG pmove context pm (0x30539850) and
//     dereferences pm->ps, the server playerState_s (playerState_t).
//   * It sets ps->groundEntityNum (+0x58) to 1023 == ENTITYNUM_NONE, the canonical
//     "leave the ground" write of a jump.
//   * It computes ps->velocity[2] (+0x28) = sqrt( gravity * 2 * jumpHeight ), the
//     standard launch-speed formula v = sqrt(2*g*h). gravity is ps->gravity (+0x40,
//     FILD as a signed int); jumpHeight is the sole float argument (the caller at
//     0x30008d6d pushes 0x421c0000 == 39.0f). ps->velocity[2] is +0x20+8.
//   * It resets ps->pmTime (+0x10) to 0 and OR-sets the pmove flags (+0x0c) with
//     PMF_WALLJUMP|PMF_JUMP_HELD (0x2000|0x8).
//   * It records the take-off height: ps->jumpOriginZ (+0x6c) = ps->psOrigin[2]
//     (+0x1c == +0x14+8).
// The caller (0x30008d00) is the jump-eligibility gate (checks time-since-ground
// >= 500, ground/lean state, then calls this to perform the jump), consistent with
// a Quake3/CoD PM_CheckJump -> PM_Jump split. Exact source name is highly likely
// PM_Jump; kept with this note rather than an address-shaped symbol.
//
// Fatigue scaling: when PMF_WALLJUMP (0x2000) is already set on entry (the
// player jumped recently), the launch energy (gravity*2*jumpHeight) is divided by a
// fatigue divisor derived from ps->pmTime (+0x10, an int) BEFORE the sqrt, reducing
// jump height. The compiler inlines the adjacent PM_GetJumpFactor body here; its
// piecewise map is:
//   pmTime >  1800        -> no division (full jump)
//   1700 <= pmTime <= 1800 -> divide by 2.5
//   pmTime <  1700        -> divide by (pmTime * 0.00088235294f + 1.0f)
// The 0.00088235294f constant is 1.5/1700 (linear ramp from 1.0 at pmTime==0 up to
// ~2.5 at pmTime==1700). Constants read from .rdata: 2.5f @0x3007be68,
// 0.00088235294f @0x3007be64, 1.0f @0x3007bce0.
//
// x87 stack order (proven against the bytes):
//   FILD  [gravity]           st0 = (float)(int)gravity
//   FLD   [jumpHeight arg]     st1=gravity, st0=jumpHeight
//   FADD  ST0,ST0             st0 = jumpHeight+jumpHeight = 2*jumpHeight
//   FMULP                     st0 = gravity * (2*jumpHeight)
//   (optional) FDIVP by divisor
//   FSQRT                     st0 = sqrt(...)
//   FSTP  [velocity[2]]
//
// Three adjacent pmove globals (0x305395ac/b0/b4) are zeroed together on entry.
// Their exact source names are unresolved: 0x305395ac is used as a pointer (null-
// checked at 0x30008470 and 0x30009661), the other two are scalar pmove state; all
// three are reset by several pmove routines. They are left as their existing
// mechanical symbols (the exporter's owner= labels on them are wrong, as this
// reconstruction proves, but resolving their real identity needs more pmove
// reconstruction than this unit's budget). No alias is introduced.
//
// ABI: __cdecl, single float arg (caller pushes then `add esp,4`); RET with no
// immediate. EBP is not used; the lone PUSH ESI/POP ESI is register preservation.

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#if defined(WINDOWS_BEHAVIOR)
void PM_Jump(float jumpHeight)
{
    pmove_t *move = pm; /* 0x30008c70: EAX is retained throughout. */

    /* Reset the pmove-locals ground state cleared at the top of the jump: the player
     * leaves the ground, so there is no valid ground trace plane and it is no longer
     * walking. This is the same pmove-locals triple PM_GroundTrace (0x3000a2a0) clears
     * on its ground-miss path: pml.groundPlane (0x305395b0, proven by PM_AirMove
     * 0x30009060), pml.groundLiftFlag (0x305395b4), pml.walking (0x305395ac). */
    pml.groundPlane = 0;
    pml.groundLiftFlag = 0;
    pml.walking = 0;

    playerState_t *groundPs = move->ps; /* 0x30008c89 */
    groundPs->groundEntityNum = ENTITYNUM_NONE;

    playerState_t *launchPs = move->ps; /* 0x30008c92 independent reload */
    int32_t launchGravity = launchPs->gravity; /* 0x30008c94 FILD source */
    uint32_t launchFlags = launchPs->playerStateFlags; /* 0x30008c9c */

    /* Launch energy = gravity * 2 * jumpHeight, computed in x87 order. The DLL
     * (0x30008c94 FILD gravity; FLD jumpHeight; FADD ST0,ST0; FMULP) carries this in
     * st0 through the optional FDIVP and the FSQRT with NO intervening float store
     * (FSTP [velocity[2]] @ 0x30008cde is the only rounding), so launchEnergy is
     * long double and gravity feeds a bare FILD (no (float) cast). */
    long double launchEnergy = (long double)launchGravity * ((long double)jumpHeight + (long double)jumpHeight);

    if ((launchFlags & PMF_WALLJUMP) != 0 && launchPs->pmTime <= 1800) {
        /* PM_GetJumpFactor returns live in ST0 on Windows and is consumed by
         * FDIVP (0x30008cda); the source-level call represents the proven
         * adjacent helper that this optimized DLL inlined. */
        launchEnergy = launchEnergy / PM_GetJumpFactor();
    }
    /* pmTime > 1800: no division, full-strength jump. */

    launchPs->velocity[2] = (float)coduo_x87_sqrtl(launchEnergy);

    playerState_t *flagPs = move->ps; /* 0x30008ce1 */
    flagPs->playerStateFlags |= (PMF_WALLJUMP | PMF_JUMP_HELD);
    playerState_t *timerPs = move->ps; /* 0x30008cea */
    timerPs->pmTime = 0;
    playerState_t *originPs = move->ps; /* 0x30008cef */
    originPs->jumpOriginZ = originPs->psOrigin[2];
}
#else
void PM_Jump(float jumpHeight)
{
    playerState_t *const ps = pm->ps;
    float launchEnergy;

    pml.groundPlane = 0;
    pml.groundLiftFlag = 0;
    pml.walking = 0;
    ps->groundEntityNum = ENTITYNUM_NONE;

    /* Linux stores the doubled-height/gravity product as binary32 before the
     * wall-jump gate, and stores the optional division back to the same slot. */
#if EMULATE_X87
    launchEnergy = x87f_store_f32(x87f_mul(x87f_load_i32(ps->gravity), x87f_add(x87f_load_f32(jumpHeight), x87f_load_f32(jumpHeight))));
#else
    launchEnergy = (float)((long double)ps->gravity * ((long double)jumpHeight + (long double)jumpHeight));
#endif

    if ((ps->playerStateFlags & PMF_WALLJUMP) != 0 && ps->pmTime <= 1800) {
#if EMULATE_X87
        launchEnergy = x87f_store_f32(x87f_div(x87f_load_f32(launchEnergy), x87f_load_f32((float)PM_GetJumpFactor())));
#else
        launchEnergy = (float)((long double)launchEnergy / PM_GetJumpFactor());
#endif
    }

    ps->velocity[2] = (float)CoduoLibm_SqrtGlibc((double)launchEnergy);
    ps->playerStateFlags |= PMF_WALLJUMP | PMF_JUMP_HELD;
    ps->pmTime = 0;
    ps->jumpOriginZ = ps->psOrigin[2];
}
#endif
