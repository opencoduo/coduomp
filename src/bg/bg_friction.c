// Sources: uo_cgame_mp_x86.dll 0x30008410..0x300085ce,
//          uo_game_mp_x86.dll  0x200081c0..0x2000837e,
//          game.mp.uo.i386.so  RVA 0x0002342d..0x0002379e
//
// PM_Friction — the CoD:UO pmove velocity-friction step (Quake3
// movement.c PM_Friction lineage). Operates on the global pmove context
// pm (0x30539850): move = pm, ps = move->ps.
//
// NAME ADJUDICATION: the .mcode header guesses "vectoangles" by size
// (win 0x15e == corpus 0x15e). That is REJECTED: this body performs no angle
// math whatsoever — it reads playerState velocity, forms a speed, accumulates a
// friction "drop", and rescales velocity. That is exactly PM_Friction. The
// same-module PPC bank lists a PM_Friction whose *size* (0x288) differs; per the
// worker contract sizes do not correspond across builds and are ignored — the
// name is adopted from proven behavior, not size.
//
// ABI: __cdecl, no source arguments (all state comes from the global pmove
// context). The SUB ESP,8 frame holds two 4-byte float spill locals at [ESP+8]
// (`speed`) and [ESP+0xc] (`control` / scratch). Plain RET; PUSH/POP EBX,ESI are
// register preservation.
//
// x87 note: the whole body is x87 stack arithmetic. Each C expression below is
// written to preserve the exact instruction order, operand widths (all single-
// precision float loads/stores), and the fcom/fnstsw/test-ah/jp compare idiom.
// A `TEST AH,0x5 ; JP taken` after `FCOM/FCOMP mem` means "ST0 >= mem" (parity of
// C0|C2 is even for greater/equal/unordered-none; odd only for the less-than
// case where C0=1); the JP-not-taken fall-through is the `ST0 < mem` branch.

#include "bg_pmove.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>
#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/* Friction tuning constants used by this step. Natural float forms of the
 * .rdata single-precision literals the machine code loads (raw hex kept for
 * traceability). Exact original source names are unresolved; named by role. */
enum {
    PM_STOPSPEED_MS = 100
};            /* speed floor for the "control" value */
#define PM_STOPSPEED 100.0f          /* 0x300715ec */
#define PM_FRICTION 5.5f            /* 0x3007161c: base ground-friction coefficient */
#define PM_LAND_STUN_FRICTION \
    0.3f         /* 0x3007bea0: reduced coefficient when
                                            * PMF_LAND_STUN (0x100) is set */
#define PM_SPECTATORFRICTION 5.0f          /* 0x3007bde0: extra drag for PM_TYPE_SPECTATOR */

/* Recently-jumped window: friction is boosted while ps->pmTime is within this
 * window (and PMF_WALLJUMP is set); past it the flag is dropped. */
enum {
    PM_JUMP_FRICTION_WINDOW_MS = 1800
};  /* compared as [EDX+0x10] > 0x708 */

/* waterlevel values above this (2/3 == waist/head submerged) skip ground friction
 * entirely (MOV AL,[ESI+0xf1]; CMP AL,1; JA). The byte is pm->waterlevel. */
enum {
    PM_FRICTION_WATERLEVEL_MAX = 1
};

#define PM_JUMP_FRICTION_SCALE_MAX 2.5f
#define PM_JUMP_FRICTION_SCALE_SLOPE 0.00088235294f
#define PM_JUMP_FRICTION_SCALE_BASE 1.0f
#define PM_JUMP_FRICTION_TIME_SCALE 0.00058823527f
enum {
    PM_JUMP_FRICTION_SCALE_SATURATE_MS = 1700
};

/* The two source-level helpers are retained as adjacent identical bodies in
 * both Windows DLLs (cgame 0x30008410/0x30008440, game
 * 0x200081c0/0x200081f0).  The optimized Windows jump body inlines the second;
 * Linux calls both at RVAs 0x0002342d and 0x0002348e. */
#if defined(WINDOWS_BEHAVIOR)
long double PM_GetSlowdownFriction(void)
{
    const int32_t time = pm->ps->pmTime;

    if (time >= PM_JUMP_FRICTION_SCALE_SATURATE_MS) {
        return PM_JUMP_FRICTION_SCALE_MAX;
    }
    return (long double)time * (long double)PM_JUMP_FRICTION_SCALE_SLOPE + (long double)PM_JUMP_FRICTION_SCALE_BASE;
}

long double PM_GetJumpFactor(void)
{
    const int32_t time = pm->ps->pmTime;

    if (time >= PM_JUMP_FRICTION_SCALE_SATURATE_MS) {
        return PM_JUMP_FRICTION_SCALE_MAX;
    }
    return (long double)time * (long double)PM_JUMP_FRICTION_SCALE_SLOPE + (long double)PM_JUMP_FRICTION_SCALE_BASE;
}
#else
long double PM_GetSlowdownFriction(void)
{
    float value;

    if (pm->ps->pmTime >= PM_JUMP_FRICTION_SCALE_SATURATE_MS) {
        return PM_JUMP_FRICTION_SCALE_MAX;
    }
#if EMULATE_X87
    value = x87f_store_f32(
        x87f_add(x87f_mul(x87f_mul(x87f_load_i32(pm->ps->pmTime), x87f_load_f32(1.5f)), x87f_load_f32(PM_JUMP_FRICTION_TIME_SCALE)),
                 x87f_load_f32(PM_JUMP_FRICTION_SCALE_BASE)));
#else
    value =
        (float)((long double)pm->ps->pmTime * 1.5L * (long double)PM_JUMP_FRICTION_TIME_SCALE + (long double)PM_JUMP_FRICTION_SCALE_BASE);
#endif
    return value;
}

long double PM_GetJumpFactor(void)
{
    float value;

    if (pm->ps->pmTime >= PM_JUMP_FRICTION_SCALE_SATURATE_MS) {
        return PM_JUMP_FRICTION_SCALE_MAX;
    }
#if EMULATE_X87
    value = x87f_store_f32(
        x87f_add(x87f_mul(x87f_mul(x87f_load_i32(pm->ps->pmTime), x87f_load_f32(1.5f)), x87f_load_f32(PM_JUMP_FRICTION_TIME_SCALE)),
                 x87f_load_f32(PM_JUMP_FRICTION_SCALE_BASE)));
#else
    value =
        (float)((long double)pm->ps->pmTime * 1.5L * (long double)PM_JUMP_FRICTION_TIME_SCALE + (long double)PM_JUMP_FRICTION_SCALE_BASE);
#endif
    return value;
}
#endif

#if defined(WINDOWS_BEHAVIOR)
void PM_Friction(void)
{
    pmove_t *move = pm;                 /* ESI = [0x30539850] */
    playerState_t *ps = move->ps;                     /* EDX = [ESI] */
    /* pml.walking on-ground gate (0x305395ac). Only READ here (compared != 0);
     * loaded once into ECX at entry (0x30008470). Resolved to pml.walking (see
     * globals.h) — the pmove locals flag that the player is on a walkable ground
     * plane this frame. */
    int32_t walking = pml.walking;

    /* speed = length of {vx, vy, vc}, where vc is velocity[2] normally, but is
     * substituted by 0.0f while pml.walking is set (0x30008490..0x30008494 FSTP
     * ST0; FLD 0.0f), i.e. the Q3 PM_Friction "vec[2]=0 on ground" idiom.
     * The three velocity components are loaded once (FLD +0x20/+0x24/+0x28). */
    float vx = ps->velocity[0];
    float vy = ps->velocity[1];
    float vc = (walking != 0) ? 0.0f : ps->velocity[2];       /* 0.0f @ 0x3007bcec */

    /* FLD ST0; FMUL ST1; FLD ST2; FMUL ST3; FADDP; FLD ST3; FMUL ST4; FADDP;
     * FSQRT — vc*vc + vy*vy + vx*vx, then sqrt. FSQRT (0x300084aa) leaves an
     * 53-bit result that FST-keep (0x300084b2) rounds to float for [ESP+8] while
     * the FCOMP below tests the unrounded chain still in ST0. A long-double
     * carrier keeps that value register-wide; the target x87 control word still
     * supplies the retail arithmetic precision, with a separate float copy. */
    long double speedRaw =
        coduo_x87_sqrtl((long double)vc * (long double)vc + (long double)vy * (long double)vy + (long double)vx * (long double)vx);
    float speed = (float)speedRaw;                            /* FST float [ESP+8] */

    /* if (speed < 1.0f) { VectorClear(velocity); return; }  (0x300084b6 FCOMP
     * against 1.0f @ 0x3007bce0; JP-not-taken == speed < 1.0f). The FCOMP consumes
     * the unrounded 53-bit sqrt result (speedRaw), not the rounded [ESP+8] copy. */
    if (speedRaw < 1.0f) {
        ps->velocity[2] = 0.0f;                              /* MOV [EDX+0x28],0 */
        ps->velocity[1] = 0.0f;                              /* MOV [EDX+0x24],0 */
        ps->velocity[0] = 0.0f;                              /* MOV [EDX+0x20],0 */
        return;
    }

    /* drop starts at 0.0f (FLD 0.0 @ 0x3007bcec). Ground-friction accumulation is
     * only performed when the player is eligible; each disqualifier below leaves
     * drop == 0 and jumps to the hasDObj/spectator terms at 0x3000855a.
     * drop uses a live x87 carrier: the DLL carries it in ST0 across the entire body
     * (branch factor, *pml.frametime*5.5, +=water, +=spectator, up to the FSUBR at
     * 0x30008595) with NO intervening float store, so a float local's per-step
     * rounding would be spurious. */
    long double drop = 0.0f;

    qboolean skipGroundFriction = ((uint8_t)move->waterlevel > PM_FRICTION_WATERLEVEL_MAX) || /* CMP AL,1; JA */
                                  (walking == 0) ||                                     /* CMP ECX,EBX; JZ */
                                  ((pml.groundTrace.surfaceFlags & SURF_SLICK) != 0) /* TEST [..],0x2; JNZ */
        ;

    if (!skipGroundFriction) {
        /* 0x300084ef performs this load only after all three earlier ground-
         * friction gates pass. Keep it inside the gate rather than speculatively
         * reading the state word on paths that jump directly to water drag. */
        uint32_t flags = ps->playerStateFlags;               /* MOV ECX,[EDX+0xc] */
        if ((flags & PMF_NO_GROUNDFRICTION) == 0) {  /* TEST CH,0x2; JNZ */

            /* control = max(speed, PM_STOPSPEED) (0x300084f9 FCOMP 100.0f; the
             * JP-taken path == speed >= 100 keeps speed, else loads 100.0f). */
            float control = (speed < PM_STOPSPEED) ? PM_STOPSPEED : speed;

            /* Select the friction coefficient by playerState flags. */
            if (flags & PMF_LAND_STUN) {            /* TEST CH,0x1; JZ */
                /* reduced-traction friction */
                drop = (long double)control * (long double)PM_LAND_STUN_FRICTION;   /* FMUL 0.3f */
            } else if (flags & PMF_WALLJUMP) {        /* TEST CH,0x20; JZ */
                if (ps->pmTime > PM_JUMP_FRICTION_WINDOW_MS) { /* CMP [EDX+0x10],0x708; JG */
                    /* Recently-jumped window has elapsed: drop the jumped flag and
                     * clear the take-off-height latch; friction reverts to base. */
                    ps->playerStateFlags &= ~(uint32_t)PMF_WALLJUMP; /* AND [EDX+0xc],0xffffdfff */
                    move->ps->jumpOriginZ = 0;               /* MOV [EAX+0x6c],0 (EAX=[ESI]) */
                    drop = control;
                } else {
                    /* Within the window: scale friction up by the jump-time ramp. */
                    drop = PM_GetSlowdownFriction() * control; /* CALL 0x30008410; FMUL [ESP+0xc] */
                }
            } else {
                drop = control;
            }

            /* drop *= pml.frametime * PM_FRICTION (0x3000854e/0x30008554). */
            drop = drop * pml.frametime * PM_FRICTION;
        }
    }

    /* Additional drag proportional to the water depth (swim drag grows with how
     * deep the player is submerged). drop += (float)waterlevel * pml.frametime *
     * speed. Only applied when waterlevel != 0 (0x3000855a CMP AL,BL; JZ). */
    uint8_t waterlevel = move->waterlevel;                     /* MOV AL,[ESI+0xf1] */
    if (waterlevel != 0) {
        drop += (long double)(int32_t)waterlevel * (long double)pml.frametime * (long double)speed;
    }

    /* Spectator movement gets extra friction: drop += pml.frametime*speed*5.0. */
    playerState_t *spectatorPs = move->ps;                     /* 0x3000857b reload [ESI] */
    if (spectatorPs->pmType == PM_TYPE_SPECTATOR) {          /* CMP [EAX+0x4],4; JNZ */
        drop += (long double)pml.frametime * (long double)speed * (long double)PM_SPECTATORFRICTION;
    }

    /* newspeed = speed - drop; if (newspeed < 0) newspeed = 0; (0x30008595 FSUBR
     * mem - ST0, then FCOM against 0.0f). newspeed stays live in x87: the DLL keeps it
     * in st0 through the FCOM, the max-with-0, the FDIV [ESP+8] and into the three
     * velocity FMULs — no float store until each velocity component is written. */
    long double newspeed = (long double)speed - drop;
    if (newspeed < 0.0f) {
        newspeed = 0.0f;                                     /* FLD 0.0f @ 0x3007bcec */
    }

    /* newspeed /= speed; VectorScale(velocity, newspeed, velocity). */
    newspeed = newspeed / speed;                             /* FDIV [ESP+8] */
    ps->velocity[0] = newspeed * ps->velocity[0];            /* FLD ST0; FMUL [EDX+0x20]; FSTP */
    ps->velocity[1] = newspeed * ps->velocity[1];            /* FLD ST0; FMUL [EDX+0x24]; FSTP */
    ps->velocity[2] = newspeed * ps->velocity[2];            /* FMUL [EDX+0x28]; FSTP */
}
#else
void PM_Friction(void)
{
    playerState_t *const ps = pm->ps;
    float verticalVelocity = ps->velocity[2];
    float speed;
    float drop = 0.0f;
    float newSpeed;
    int32_t lane;

    if (pml.walking != 0) {
        verticalVelocity = 0.0f;
    }

    /* Linux stores the squared sum as binary64 for the glibc sqrt call and
     * stores the returned speed as binary32 before the threshold comparison. */
#if EMULATE_X87
    speed = (float)CoduoLibm_SqrtGlibc(
        x87f_store_f64(x87f_add(x87f_add(x87f_mul(x87f_load_f32(ps->velocity[0]), x87f_load_f32(ps->velocity[0])),
                                         x87f_mul(x87f_load_f32(ps->velocity[1]), x87f_load_f32(ps->velocity[1]))),
                                x87f_mul(x87f_load_f32(verticalVelocity), x87f_load_f32(verticalVelocity)))));
#else
    speed = (float)CoduoLibm_SqrtGlibc((double)(((long double)ps->velocity[0] * (long double)ps->velocity[0] +
                                                 (long double)ps->velocity[1] * (long double)ps->velocity[1]) +
                                                (long double)verticalVelocity * (long double)verticalVelocity));
#endif

    if (speed < 1.0f) {
        ps->velocity[2] = 0.0f;
        ps->velocity[1] = 0.0f;
        ps->velocity[0] = 0.0f;
        return;
    }

    if (pm->waterlevel <= PM_FRICTION_WATERLEVEL_MAX && pml.walking != 0 && (pml.groundTrace.surfaceFlags & SURF_SLICK) == 0 &&
        (ps->playerStateFlags & PMF_NO_GROUNDFRICTION) == 0) {
        float control = speed;
        float multiplier;

        if (control < PM_STOPSPEED) {
            control = PM_STOPSPEED;
        }
        multiplier = control;

        if ((ps->playerStateFlags & PMF_LAND_STUN) != 0) {
#if EMULATE_X87
            multiplier = x87f_store_f32(x87f_mul(x87f_load_f32(control), x87f_load_f32(PM_LAND_STUN_FRICTION)));
#else
            multiplier = (float)((long double)control * (long double)PM_LAND_STUN_FRICTION);
#endif
        } else if ((ps->playerStateFlags & PMF_WALLJUMP) != 0) {
            if (ps->pmTime <= PM_JUMP_FRICTION_WINDOW_MS) {
#if EMULATE_X87
                multiplier = x87f_store_f32(x87f_mul(x87f_load_f32(control), x87f_load_f32((float)PM_GetSlowdownFriction())));
#else
                multiplier = (float)((long double)control * PM_GetSlowdownFriction());
#endif
            } else {
                ps->playerStateFlags &= ~(uint32_t)PMF_WALLJUMP;
                ps->jumpOriginZ = 0.0f;
            }
        }

        /* The Linux body stores the ground-friction contribution in `drop`
         * after the complete multiply chain. */
#if EMULATE_X87
        drop = x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(multiplier), x87f_load_f32(PM_FRICTION)), x87f_load_f32(pml.frametime)));
#else
        drop = (float)((long double)multiplier * (long double)PM_FRICTION * (long double)pml.frametime);
#endif
    }

    if (pm->waterlevel != 0) {
#if EMULATE_X87
        drop = x87f_store_f32(x87f_add(x87f_load_f32(drop), x87f_mul(x87f_mul(x87f_load_i32((int32_t)pm->waterlevel), x87f_load_f32(speed)),
                                                                     x87f_load_f32(pml.frametime))));
#else
        drop = (float)((long double)drop + (long double)(int32_t)pm->waterlevel * (long double)speed * (long double)pml.frametime);
#endif
    }

    if (ps->pmType == PM_TYPE_SPECTATOR) {
#if EMULATE_X87
        drop = x87f_store_f32(x87f_add(x87f_load_f32(drop), x87f_mul(x87f_mul(x87f_load_f32(speed), x87f_load_f32(PM_SPECTATORFRICTION)),
                                                                     x87f_load_f32(pml.frametime))));
#else
        drop = (float)((long double)drop + (long double)speed * (long double)PM_SPECTATORFRICTION * (long double)pml.frametime);
#endif
    }

    newSpeed = (float)((long double)speed - (long double)drop);
    if (newSpeed < 0.0f) {
        newSpeed = 0.0f;
    }
#if EMULATE_X87
    newSpeed = x87f_store_f32(x87f_div(x87f_load_f32(newSpeed), x87f_load_f32(speed)));
#else
    newSpeed = (float)((long double)newSpeed / (long double)speed);
#endif

    for (lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
        ps->velocity[lane] = x87f_store_f32(x87f_mul(x87f_load_f32(ps->velocity[lane]), x87f_load_f32(newSpeed)));
#else
        ps->velocity[lane] = (float)((long double)ps->velocity[lane] * (long double)newSpeed);
#endif
    }
}
#endif
