// Source: uo_cgame_mp_x86.dll 0x30023b70..0x30023c22
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30023b70_30023c22.mcode
//
// CG_FlameGetSizeRate (0x30023b70) — compute a flame chunk's per-frame
// size/expansion advance rate. The flame chunk `f` arrives in ESI (register-
// argument helper); there are no stack arguments and the float result is returned
// in ST(0). This caller-observed ABI is modeled as a normal cdecl float-returning
// function.
//
// Name adjudication (RESOLVED): the .mcode header's size-matched "PlayerCmd_isAds"
// guess is REJECTED per the no-size-matching rule (that is a server
// script_player.c command returning void). An earlier pass modelled this on a
// provisional cg_adsAnimState_t as CG_ADSAnim_ComputeRate, but the machine code
// operates entirely on flameChunk_t fields: kind (+0x2c), ownerInfoIndex (+0x34,
// compared against cg_snap->ps.psClientNum), driftSpeed (+0x94), soundAmpRate (+0xb8),
// startSpeedBits (+0x5c) and the double life span endTime/spawnTime (+0x50/+0x48).
// Its callers are all flame-subsystem functions (0x30023d3a, 0x30025c73,
// 0x30027825, 0x30027c42 and four sites in CG_FireFlameChunks) passing a flame
// chunk. The struct and this declaration live in client_recovered.h (flameChunk_t
// / CG_FlameGetSizeRate).
//
// Callee 0x3006bb20 is the statically-linked MSVC float-stack CRT helper for
// pow(base, exponent): base arrives in ST(1), exponent in ST(0) (proven from the
// helper storing old-ST1 at [ESP] and old-ST0 at [ESP+8], and from sibling call
// sites that FLD a literal base then FLD a variable exponent, e.g. pow(10.0,x)
// and pow(0.1,x) in 0x3004d060). Expressed here as the source-level pow().

#include "client/cgame/client_recovered.h"
#include <math.h>

// +0xb8 soundAmpRate is clamped to at most 1.0 before use (FCOMP vs 1.0; the
// value is kept only when it is strictly below 1.0, else 1.0 is used).
#define CG_FLAME_RATE_SCALE_MAX 1.0f

// The "different local client" flag is scaled by this coefficient, stored in
// .rdata as 0.0f (0x3007bcec). Because the coefficient is exactly zero the flag
// contributes nothing to the result, but the machine code computes the term, so
// it is preserved verbatim rather than folded away.
#define CG_FLAME_CLIENT_FLAG_COEF 0.0f

// Additive bias for the non-saturated rate term, stored in .rdata as 0.8f
// (0x3007bdf0).
#define CG_FLAME_RATE_BIAS 0.8f

// Scales driftSpeed before it is cubed, stored in .rdata as
// 0.0011111111 (0x3007be00 == 1/900).
#define CG_FLAME_INPUT_SCALE 0.0011111111380159855f

long double CG_FlameGetSizeRate(flameChunk_t *f /* ESI */)
{
    // 30023b70..30023b98: scaleClamped = (soundAmpRate < 1.0f) ? soundAmpRate : 1.0f.
    // FLD soundAmpRate; FCOMP 1.0f; FNSTSW; TEST AH,0x5; JP takes the >= branch
    // (C0 clear => 1.0f), fall-through keeps soundAmpRate when strictly below.
    // The value is held as raw float bits in a stack slot and later subtracted
    // as a 32-bit float (FSUB m32), so it is a float throughout.
    float scaleClamped;
    if (f->soundAmpRate < CG_FLAME_RATE_SCALE_MAX) {
        scaleClamped = f->soundAmpRate;
    } else {
        scaleClamped = CG_FLAME_RATE_SCALE_MAX;
    }

    // 30023b98..30023be9: compute the double denominator term A.
    double A;
    if (f->kind > 1) {
        // 30023b9e: A = 1.0 (double literal at 0x3007bcf8).
        A = 1.0;
    } else {
        // 30023ba6..30023bb8: base = driftSpeed * (1/900); cubed = pow(base, 3.0).
        // The base is FLD float; FMUL float, left raw in st(1) with no store, and
        // _CIpow leaves its result raw in st(0), which FADD ST0,ST0 (30023bbd)
        // then consumes; the sum stays in st through the FADDP at 30023be7 -- the
        // ONLY rounding on this path is the FSTP double [ESP+0xc] at 30023be9
        // (into A). powl keeps both the base and the result 80-bit as the bytes
        // require; a double `pow` would round each. (The powl polynomial still
        // differs from _CIpow -- a pre-existing transcendental relaxation.)
        long double cubed = powl((long double)f->driftSpeed * CG_FLAME_INPUT_SCALE, 3.0L);

        // 30023bbd: FADD ST0,ST0 => 2 * cubed.
        long double doubled = cubed + cubed;

        // 30023bbf..30023bd2: clientMatches =
        // (f->ownerInfoIndex == cg_snap->ps.psClientNum) ? 1 : 0. After SUB,
        // NEG sets CF when the difference is nonzero; SBB self produces -CF and
        // INC therefore leaves 1 only for equality.
        int32_t clientMatches = (f->ownerInfoIndex == cg_snap->ps.psClientNum) ? 1 : 0;

        // 30023bd7..30023be7: A = doubled + (clientMatches * 0.0f + 0.8f).
        // The FILD'd flag is multiplied by the 0.0f coefficient (so it never
        // affects the result) and the 0.8f bias is added; FADDP folds this into
        // doubled. The FILD at 30023bd7 feeds the FMUL at 30023bdb with no
        // intervening store, and nothing narrows the term before the FADDP at
        // 30023be7 -- so neither the (float) nor the (double) cast belongs here.
        // The only rounding on this path is the FSTP double at 30023be9.
        A = (double)(doubled + ((long double)clientMatches * (long double)CG_FLAME_CLIENT_FLAG_COEF + (long double)CG_FLAME_RATE_BIAS));
    }

    // 30023bed..30023bf8: B = startSpeedBits / (endTime - spawnTime).
    // startSpeedBits (+0x5c) holds float bits here and is promoted to the x87
    // stack as a float; endTime (+0x50) and spawnTime (+0x48) are doubles.
    long double lifeSpan = (long double)f->endTime - (long double)f->spawnTime;
    double B = (double)((long double)CG_FloatFromBits(f->startSpeedBits) / lifeSpan);

    // 30023bfc..30023c02: C = 1.0f - scaleClamped. The difference is never
    // stored: FLD 1.0f / FSUB m32 feeds the pow helper's ST(1) directly, so it
    // must stay at register precision (a float local would insert a rounding
    // the DLL does not perform). powl below keeps C 80-bit into the call.
    long double C = (long double)CG_FLAME_RATE_SCALE_MAX - (long double)scaleClamped;

    // 30023c05..30023c1e: result = pow(C, 2.0) * B + B / A.
    // pow(C,2.0): C raw in ST(1), 2.0 (0x3007bde8) in ST(0); _CIpow leaves the
    // result raw in ST(0), FMUL B and FADDP run on it 80-bit, and the RET hands
    // it back unrounded (no store in the tail). powl keeps the base, the result
    // and the return all 80-bit; the return type is long double so no rounding
    // is inserted at `return`. (powl != _CIpow polynomial -- relaxation.)
    return powl(C, 2.0L) * (long double)B + (long double)B / (long double)A;
}
