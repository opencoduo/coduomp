// Source: uo_cgame_mp_x86.dll 0x3001b390..0x3001b414
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b390_3001b414.mcode
//
// CG_EvaluateCameraShakeSource — evaluate one active camera-shake ("earthquake")
// source for the current frame. Returns qtrue and writes the scaled amplitude
// and the raw time falloff back into the source while it is still live; returns
// qfalse when the elapsed time is out of range [0, duration).
//
// The source struct pointer arrives in ESI (client register/thiscall-style ABI;
// the two callers, the shake add path 0x3001b420 and the aggregate 0x3001b550,
// both load ESI with a cg_shakeSource_t and CALL here). RET with no immediate:
// no callee stack cleanup, caller-passed args are register-only.
//
// Naming: the .mcode size-guess "PM_BeginWeaponBreakingdown" is REJECTED. This
// reads cg_time and the view origin (cg_refdef.vieworg) and computes a distance-
// and time-attenuated shake magnitude; it is cgame camera-shake logic, not a
// pmove weapon routine. The name is role-derived from the shake trio call graph
// (see cg_shakeSource_t in client_recovered.h); exact original symbol unproved.

#include <stddef.h>

#include "client/cgame/client_recovered.h"

// Field offsets consumed by this function, proven against the .mcode operands.
_Static_assert(offsetof(cg_shakeSource_t, startMsec) == 0x00, "shake startMsec @+0x00");
_Static_assert(offsetof(cg_shakeSource_t, amplitude) == 0x04, "shake amplitude @+0x04");
_Static_assert(offsetof(cg_shakeSource_t, duration) == 0x08, "shake duration @+0x08");
_Static_assert(offsetof(cg_shakeSource_t, radius) == 0x0c, "shake radius @+0x0c");
_Static_assert(offsetof(cg_shakeSource_t, origin) == 0x10, "shake origin @+0x10");
_Static_assert(offsetof(cg_shakeSource_t, scaledAmplitude) == 0x1c, "shake scaledAmplitude @+0x1c");
_Static_assert(offsetof(cg_shakeSource_t, timeFalloff) == 0x20, "shake timeFalloff @+0x20");

qboolean CG_EvaluateCameraShakeSource(cg_shakeSource_t *source)
{
    // 0x3001b393-0x3001b39d: elapsed = cg_time - source->startMsec (signed int32).
    // MOV EAX,[cg_time]; SUB EAX,[ESI]; MOV [ESP],EAX; JS -> return 0.
    // A negative elapsed (shake in the future) yields qfalse.
    int32_t elapsedMsec = coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)source->startMsec);
    if (elapsedMsec < 0) {
        return qfalse; // 0x3001b40e: XOR EAX,EAX; RET
    }

    // 0x3001b39f-0x3001b3a2: FILD [ESP] then FST [ESP+4].
    // st0 = (double)elapsedMsec; a single-precision copy is stored for later reuse.
    float elapsedFloat = (float)elapsedMsec;

    // 0x3001b3a6-0x3001b3ae: FCOMP source->duration; FNSTSW; TEST AH,1; JZ -> return 0.
    // FCOMP sets C0=1 iff st0 < operand. TEST AH,1 isolates C0; JZ (C0==0) means
    // elapsed >= duration -> the shake has ended -> return 0. Continue while the
    // exact FILD'd integer is below duration or the comparison is unordered. The
    // rounded elapsedFloat copy is reserved for the later time-falloff division.
    /* JZ tests only C0==0. Unordered (NaN duration) has C0==1 and remains
     * active, so spell the rejecting edge as ordered >= rather than !(<). */
    if ((long double)elapsedMsec >= (long double)source->duration) {
        return qfalse; // 0x3001b40e
    }

    // 0x3001b3b0-0x3001b3b8: VectorDistance(cg_refdef.vieworg, source->origin).
    // LEA ECX,[ESI+0x10] (source->origin, the b argument); MOV EAX,cg_refdef.vieworg
    // (the a argument); CALL 0x300495b0. Result on x87 stack.
    float dist = VectorDistance(cg_refdef.vieworg, source->origin);

    // 0x3001b3bd-0x3001b3c6: distFalloff = 1.0f - (dist / source->radius).
    // FDIV [ESI+0xc]; FSUBR [0x3007bce0] (constant 1.0f, mem - st0); FSTP [ESP].
    float distFalloff = (float)(1.0L - (long double)dist / (long double)source->radius);

    // 0x3001b3c9-0x3001b3d6: timeFalloff = (1.0f - elapsedFloat / duration) * amplitude.
    // FLD [ESP+4] (elapsedFloat); FDIV [ESI+8] (duration); FSUBR [0x3007bce0] (1.0f);
    // FMUL [ESI+4] (amplitude). This is source->timeFalloff (st0 = "A").
    // The value NEVER leaves the x87 stack before its consumers: the FMUL ST1 /
    // FDIV ST0,ST1 at 0x3001b3ec/0x3001b3fd and the FSTP to source->timeFalloff
    // all take the unrounded st0, so the local must stay 80-bit (long double).
    long double timeFalloff = (1.0L - (long double)elapsedFloat / (long double)source->duration) * (long double)source->amplitude;

    // 0x3001b3d9-0x3001b3ea: compare distFalloff against 0.0f.
    // FLD [ESP] (distFalloff); FCOMP [0x3007bcec] (0.0f); FLD [ESP] (reload distFalloff);
    // FNSTSW; TEST AH,1 (C0: distFalloff < 0.0f); JNZ -> the FDIV path.
    /* C0 is also set for unordered, so NaN follows the same FDIV edge as a
     * negative distance falloff. */
    if (!(distFalloff >= 0.0f)) {
        // 0x3001b3fd-0x3001b40d: FDIV ST0,ST1 -> distFalloff / timeFalloff.
        source->scaledAmplitude = distFalloff / timeFalloff;
        source->timeFalloff = timeFalloff;
        return qtrue; // MOV EAX,1
    }

    // 0x3001b3ec-0x3001b3fc: FMUL ST1 -> distFalloff * timeFalloff.
    source->scaledAmplitude = distFalloff * timeFalloff;
    source->timeFalloff = timeFalloff;
    return qtrue; // MOV EAX,1
}
