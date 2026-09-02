#include "bg_pmove.h"

#include "compat/coduo_x87emu.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

// Sources: uo_cgame_mp_x86.dll 0x300085d0..0x3000868d,
//          uo_game_mp_x86.dll  0x20008380..0x2000843d,
//          game.mp.uo.i386.so  RVA 0x0002379e..0x00023905
//
// Name adjudication: the .mcode bank labels this MatrixTransposeTransformVector43
// with the note "win size 0xbd, matched size 0xbe". That is a pure SIZE match and is
// REJECTED per the no-size-matching rule. This function is not a matrix transform: it
// reads the pmove player-state velocity, projects it onto a wish direction, and
// accumulates a clamped acceleration back into the velocity -- the classic Quake3/CoD
// PM_Accelerate. The body is entirely x87 float math on the pmove context.
//
// Resolved name: PM_Accelerate. Evidence, all from the machine code:
//   * It reads the pmove context pm (0x30539850) and dereferences
//     pm->ps (ECX = [ESI] = move->ps), the player state (playerState_t
//     in this codebase's naming; == server playerState_s). The neighbour at
//     0x30008280 proves 0x30539850 is the pmove_t* (it calls [ECX+0x104] = move->trace
//     and reads [EAX+0x34] = move->traceMask); PmoveSingle (0x3000e050) stores its
//     pmove_t* argument there. pm/playerState_t are the deferred-rename
//     views of move/playerState_t documented in globals.h; used here for consistency.
//   * currentspeed = DotProduct(ps->velocity, wishdir): FLD [ECX+0x28]*[EDX+8] +
//     [ECX+0x24]*[EDX+4] + [EDX]*[ECX+0x20], i.e. vel.z*d.z + vel.y*d.y + vel.x*d.x.
//   * addspeed = wishspeed - currentspeed (FSUBR [ESP+0xc]; FST [ESP+4] keeps it as a
//     temp). If addspeed <= 0 (FCOMP against 0.0 at 0x3007bcec; TEST AH,0x41 / JNP)
//     the function returns immediately, doing nothing.
//   * accelspeed = max(wishspeed, 100.0f) * pml.frametime * accel. The wishspeed used
//     for the acceleration is clamped up to a floor of 100.0f (FCOMP against 100.0 at
//     0x300715ec; TEST AH,0x5 / JP selects wishspeed when wishspeed >= 100.0 else the
//     100.0 constant). pml.frametime is [0x305395a4]; accel is the second stack arg
//     (the caller at 0x3000903b pushes 0x41000000 == 8.0f).
//   * accelspeed is clamped down to addspeed (FCOM [ESP+4]; if accelspeed > addspeed,
//     accelspeed = addspeed).
//   * On the ground (ps->groundEntityNum != ENTITYNUM_NONE, CMP [ECX+0x58],0x3ff / JZ)
//     accelspeed is scaled by 1.0f / ps->friction (FLD 1.0 at 0x3007bce0; FDIV
//     [ECX+0x5c0]; FMULP), then clamped down to addspeed a second time.
//   * ps->velocity[i] += accelspeed * wishdir[i], for i = 0,1,2 (the pmove context
//     pointer is reloaded from ESI/[0x30539850] for the y and z stores, matching the
//     MOV EAX,[ESI] reloads at 0x30008672 and 0x3000867f).
//
// ABI: EDX holds the wishdir pointer (the caller does MOV EDX,ESI = LEA ESI,[ESP+0x10]
// before the CALL); wishspeed and accel are pushed on the stack and cleaned by the
// caller; RET has no immediate. The register-passed wishdir and caller stack cleanup
// are calling-convention details, not source behaviour -- expressed here as plain C
// arguments. The lone PUSH ECX/PUSH ESI is register/scratch preservation; the pushed
// ECX slot ([ESP+0x4]) is reused as the `addspeed` temporary.

#if defined(WINDOWS_BEHAVIOR)
void PM_Accelerate(vec3_t wishdir, float wishspeed, float accel)
{
    pmove_t *move = pm; /* ESI is loaded once and retained. */
    playerState_t *ps = move->ps;

    /* currentspeed = DotProduct(ps->velocity, wishdir), evaluated in x87 order:
     * vel.z*wishdir.z + vel.y*wishdir.y + vel.x*wishdir.x. The binary keeps this
     * dot in st0 and consumes it with FSUBR (0x300085ef) without storing it, so
     * it is carried long double (a `float currentspeed` would round where the DLL
     * does not). */
    long double currentspeed = (long double)ps->velocity[2] * (long double)wishdir[2] +
                               (long double)ps->velocity[1] * (long double)wishdir[1] +
                               (long double)wishdir[0] * (long double)ps->velocity[0];

    /* 0x300085ef FSUBR [ESP+0xc]: addspeed = wishspeed - currentspeed, left in
     * st0. 0x300085f3 FST float ptr [ESP+4] stores a ROUNDED copy (kept, reloaded
     * by the clamps below) while 0x300085f7 FCOMP tests the UNROUNDED st0 against
     * 0.0f -- Class 8 FST-keep. addspeedRaw carries the unrounded value for the
     * gate; addspeed is the rounded copy the clamps use. */
    long double addspeedRaw = wishspeed - currentspeed;
    float addspeed = (float)addspeedRaw;
    if (addspeedRaw <= 0.0f)
        return;

    /* Acceleration uses wishspeed with a floor of 100.0f (the FCOMP/JP idiom loads
     * wishspeed when wishspeed >= 100.0f, else the 100.0f constant). accelspeed is
     * kept in st0 across the clamps and the friction divide (0x3000862f FCOM /
     * 0x30008649 FDIV+FMULP -- no store), so it is long double. */
    float accelSpeedBase = wishspeed;
    /* The x87 JP path also keeps an unordered (NaN) wishspeed. Express the
     * opposite, ordered-less-than case so NaN is not replaced by 100. */
    if (wishspeed < 100.0f)
        accelSpeedBase = 100.0f;
    long double accelspeed = (long double)accelSpeedBase * (long double)pml.frametime * (long double)accel;

    if (accelspeed > addspeed)
        accelspeed = addspeed;

    /* On the ground, divide the acceleration by the player's friction. Airborne
     * (groundEntityNum == ENTITYNUM_NONE) skips this. */
    if (ps->groundEntityNum != ENTITYNUM_NONE) {
        accelspeed = accelspeed * (1.0L / (long double)ps->friction);
    }
    /* 0x30008657 performs this second clamp on both the grounded and airborne
     * paths; only the friction divide itself is conditional. */
    if (accelspeed > addspeed)
        accelspeed = addspeed;

    ps->velocity[0] += accelspeed * wishdir[0];
    playerState_t *psY = move->ps; /* 0x30008672 reloads [ESI] */
    psY->velocity[1] += accelspeed * wishdir[1];
    playerState_t *psZ = move->ps; /* 0x3000867f reloads [ESI] */
    psZ->velocity[2] += accelspeed * wishdir[2];
}
#else
/* Linux stores the dot product, add speed, acceleration, grounded-friction
 * result, and each velocity lane as binary32. */
void PM_Accelerate(vec3_t wishdir, float wishspeed, float accel)
{
    float currentSpeed;
    float addSpeed;
    float accelSpeed;
    int32_t lane;

#if EMULATE_X87
    currentSpeed = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(pm->ps->velocity[0]), x87f_load_f32(wishdir[0])),
                                                    x87f_mul(x87f_load_f32(pm->ps->velocity[1]), x87f_load_f32(wishdir[1]))),
                                           x87f_mul(x87f_load_f32(pm->ps->velocity[2]), x87f_load_f32(wishdir[2]))));
#else
    currentSpeed =
        (float)(((long double)pm->ps->velocity[0] * (long double)wishdir[0] + (long double)pm->ps->velocity[1] * (long double)wishdir[1]) +
                (long double)pm->ps->velocity[2] * (long double)wishdir[2]);
#endif
    addSpeed = (float)((long double)wishspeed - (long double)currentSpeed);
    if (addSpeed <= 0.0f) {
        return;
    }

    if (wishspeed < 100.0f) {
        wishspeed = 100.0f;
    }
#if EMULATE_X87
    accelSpeed = x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(accel), x87f_load_f32(pml.frametime)), x87f_load_f32(wishspeed)));
#else
    accelSpeed = (float)((long double)accel * (long double)pml.frametime * (long double)wishspeed);
#endif
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }

    if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {
#if EMULATE_X87
        accelSpeed = x87f_store_f32(x87f_mul(x87f_load_f32(accelSpeed), x87f_div(x87f_load_f32(1.0f), x87f_load_f32(pm->ps->friction))));
#else
        accelSpeed = (float)((long double)accelSpeed * (1.0L / (long double)pm->ps->friction));
#endif
    }
    if (accelSpeed > addSpeed) {
        accelSpeed = addSpeed;
    }

    for (lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
        pm->ps->velocity[lane] = x87f_store_f32(
            x87f_add(x87f_load_f32(pm->ps->velocity[lane]), x87f_mul(x87f_load_f32(accelSpeed), x87f_load_f32(wishdir[lane]))));
#else
        pm->ps->velocity[lane] = (float)((long double)pm->ps->velocity[lane] + (long double)accelSpeed * (long double)wishdir[lane]);
#endif
    }
}
#endif
