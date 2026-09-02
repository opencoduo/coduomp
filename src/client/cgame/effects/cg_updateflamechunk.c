// Source: uo_cgame_mp_x86.dll 0x30023c30..0x30023d44
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30023c30_30023d44.mcode
//
// CG_UpdateFlameChunk (0x30023c30) — advance one flame chunk one frame. Given the
// per-frame time delta `dt`, it accumulates the drift timer driftSpeed (+0x94),
// recomputes the size target startSpeedBits (+0x5c) from the chunk's sound/smoke
// rates, clamps the target to the 290.0 maximum, and finally writes the per-frame
// size-advance rate sizeRate (+0x64) either directly (fast path) or via
// CG_FlameGetSizeRate (0x30023b70) once the chunk's start delay has elapsed.
// `this` (the flame chunk) arrives in EAX; `dt` is one caller-cleaned float stack
// arg. No value is returned (POP ESI; RET / RET, no ST(0) result).
//
// Name adjudication (RESOLVED): the .mcode header's mechanical "CG_PlayADSAnim" is
// a size-match guess (win size 0x114 == matched 0x114) and an earlier pass modelled
// this on a provisional cg_adsAnimState_t. The machine code proves it operates on a
// flameChunk_t: driftSpeed (+0x94), kind (+0x2c), deadFlag (+0xa0), smokeDensityRate
// (+0x60), soundAmpRate (+0xb8), startSpeedBits (+0x5c), sizeRate (+0x64), spawnTime
// (+0x48) and birthTime (+0xa4). Its sole caller is CG_MoveFlameChunk
// (0x30025faa), which passes the chunk in EAX and -x as dt, and it shares the flame
// clock cg_flameTime with the rest of the flame subsystem. The role name
// CG_UpdateFlameChunk (per-frame chunk advance) is provisional-by-role.
//
// Float constants are dumped by exact .rdata address (see #defines); adjacent
// pool slots at 0x3007bce0..bcec are 1.0/2.0/0.5/0.0 respectively — this function
// uses 1.0 (bce0) and 0.0 (bcec) but never 2.0/0.5, so each load is taken from its
// own exact address rather than inferred from a neighbor.

#include "client/cgame/client_recovered.h"

// .rdata float pool @0x3007bce0: 1.0f/2.0f/0.5f/0.0f at bce0/bce4/bce8/bcec.
#define CG_FLAME_ONE   1.0f   /* 0x3007bce0 */
#define CG_FLAME_ZERO  0.0f   /* 0x3007bcec */

// Drift-timer floor: once the accumulated timer is nonzero it is held at no
// less than 30.0 (.rdata 0x3007becc == 30.0f, immediate 0x41f00000 == 30.0f).
#define CG_FLAME_TIMER_MIN 30.0f

// Size-target ceiling: 290.0 (.rdata 0x3007bebc == 290.0f, immediate
// 0x43910000 == 290.0f).
#define CG_FLAME_TARGET_MAX 290.0f

// Size-target base bias: 140.0 (.rdata 0x3007bec8 == 140.0f).
#define CG_FLAME_TARGET_BASE 140.0f

// soundAmpRate weighting for the target: rate*0.75 + 0.25 (0x3007be38 == 0.75f,
// 0x3007be58 == 0.25f).
#define CG_FLAME_SCALE_GAIN 0.75f
#define CG_FLAME_SCALE_BIAS 0.25f

// Timer normalization: driftSpeed * (1/900) then subtracted from 1.0
// (0x3007be00 == 0.0011111111f == 1/900).
#define CG_FLAME_TIMER_INV900 0.0011111111380159855f  /* 1/900 */

// Fast-path size rate coefficient: startSpeedBits * (1/1666)
// (0x3007bec4 == 0.00060024f == 1/1666).
#define CG_FLAME_RATE_INV1666 0.0006002400768920779f   /* 1/1666 */

// The flame-clock snapshot (int32 ms) is read via FILD at 0x30023d10. It is the
// shared global cg_flameTime (declared in globals.h, included via
// client_recovered.h), produced by the subsystem tick at 0x30023b0b / 0x30024105.

void CG_UpdateFlameChunk(flameChunk_t *f /* EAX */, float dt /* stack */)
{
    int32_t kind;

    // 30023c30..30023c59: if the drift timer is already 0.0 and there is no time
    // delta, there is nothing to advance. FUCOMPP idioms: TEST AH,0x44;JP branches
    // "not-equal-or-unordered", JNP branches "equal". The timer!=0.0 test JPs to
    // the update; when timer==0.0, dt==0.0 additionally JNPs to the exit.
    if (f->driftSpeed == CG_FLAME_ZERO && dt == CG_FLAME_ZERO) {
        return;
    }

    // 30023c5f..30023c99: advance the timer, then hold it at >= 30.0 while nonzero.
    // FADD/FST writes back driftSpeed = dt + driftSpeed; a duplicate is compared to
    // 0.0 (JNP-equal drops through to the discard) and, when nonzero, to 30.0
    // (FCOMP 30.0; TEST AH,0x5; JP taken for >= 30.0 or unordered, so the
    // fall-through is the strictly-less-than-30.0 case that gets clamped up).
    // The 30023c69 store is FST, not FSTP: it writes the ROUNDED sum to +0x94 and
    // KEEPS the unrounded value in st0. Both compares (30023c77 vs 0.0, 30023c80 vs
    // 30.0) then run on that unrounded st0, not on what landed in memory -- hence
    // the long double live value plus an explicit (float) cast at the store.
    long double newDrift =
        (long double)dt + (long double)f->driftSpeed;
    f->driftSpeed = (float)newDrift;
    if (newDrift != CG_FLAME_ZERO && newDrift < CG_FLAME_TIMER_MIN) {
        f->driftSpeed = CG_FLAME_TIMER_MIN;
    }

    // 30023c9b..30023cef: while the chunk kind (kind) has not reached 2, recompute
    // the size target.
    kind = f->kind; /* 0x30023c9b: retained in ECX through the final kind test. */
    if (kind < 2) {
        // 30023ca3..30023cbb: select 1.0 when deadFlag == 0, else 0.0.
        float phaseSel = (f->deadFlag == 0) ? CG_FLAME_ONE : CG_FLAME_ZERO;

        // 30023cbb..30023cec: one x87 chain, ONE rounding (the FSTP at 30023cec):
        //   startSpeedBits = (140.0 - phaseSel * smokeDensityRate)
        //                  * (soundAmpRate * 0.75 + 0.25)
        //                  + (1.0 - driftSpeed * (1/900))
        // The FMULP product stays in ST0 (no intermediate float store), so this
        // must be a single expression. (+0x5c holds float bits here; the FSTP
        // writes a single into the dword.)
        f->startSpeedBits = (uint32_t)CG_FloatBits((float)(
            ((long double)CG_FLAME_TARGET_BASE -
             (long double)phaseSel * (long double)f->smokeDensityRate) *
            ((long double)f->soundAmpRate *
                 (long double)CG_FLAME_SCALE_GAIN +
             (long double)CG_FLAME_SCALE_BIAS) +
            ((long double)CG_FLAME_ONE -
             (long double)f->driftSpeed *
                 (long double)CG_FLAME_TIMER_INV900)));
    }

    // 30023cef..30023d06: clamp the size target to 290.0. FCOMP 290.0; TEST AH,0x41;
    // JNZ taken when startSpeedBits <= 290.0, so the fall-through (> 290.0) is clamped.
    if (CG_FloatFromBits(f->startSpeedBits) > CG_FLAME_TARGET_MAX) {
        f->startSpeedBits = (uint32_t)CG_FloatBits(CG_FLAME_TARGET_MAX);
    }

    // 30023d06..30023d0e: only touch the advance rate for the active local chunk
    // (deadFlag == 0); otherwise done.
    if (f->deadFlag != 0) {
        return;
    }

    // 30023d10..30023d26: has the chunk's start delay elapsed?
    //   elapsed = (double)cg_flameTime - spawnTime  (spawnTime is a double)
    //   birthTime is an int32 (FILD). FCOMPP compares birthTime vs elapsed.
    //   TEST AH,0x41; JNZ taken when birthTime <= elapsed (delay elapsed), so the
    //   fall-through is birthTime > elapsed (still within the start delay).
    //   The difference is never stored (FILD/FSUB feed FCOMPP directly), so it
    //   stays at register precision — a double local would insert a rounding
    //   the DLL does not perform.
    long double elapsed =
        (long double)coduo_int32_from_bits(cg_flameTime) -
        (long double)f->spawnTime;

    if ((double)f->birthTime > elapsed) {
        // 30023d28..30023d35: still delayed — fast path rate = startSpeedBits / 1666.
        f->sizeRate = CG_FloatFromBits(f->startSpeedBits) *
                      CG_FLAME_RATE_INV1666;
        return;
    }

    // 30023d36..30023d43: delay elapsed. Only in chunk kind 0 (kind == 0) do we
    // recompute the rate from CG_FlameGetSizeRate; otherwise leave it.
    if (kind != 0) {
        return;
    }
    f->sizeRate = CG_FlameGetSizeRate(f);
}
