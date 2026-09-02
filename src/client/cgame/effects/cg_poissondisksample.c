#include "../client_recovered.h"
#include "compat/coduo_native_x87.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3003b510..0x3003b5f0
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003b510_3003b5f0.mcode
//
// CG_PoissonDiskSample (role name) — generate ONE random 2D point inside the unit
// disk that lies at least `minDist` away from a given reference point, by polar
// rejection sampling. Returns the point in *out (two floats, x then y). The point
// is (r*cos t, r*sin t) with radius r = rand()/32768 in [0,1) and angle
// t = PI*(2*rand()/32768 - 1) in [-PI, PI); the (radius, angle) pair is redrawn
// until the squared distance from `ref` is >= minDist*minDist.
//
// The caller at 0x3003b5f0 uses this to fill a 128-entry table of {x, y} points
// (each new point is separated by >= 0.5 from the previous one), then prints them
// with Com_Printf("\t{%f, %f},\n", ...). That is the classic blue-noise / Poisson
// disk offset table CoD bakes for soft-edge / jittered filter kernels.
//
// Name adjudication: the .mcode header's size-matched guess "va" (a varargs helper)
// is REJECTED — this body has no variadic access, no va_list walk, no format; it is
// pure x87 float math driven by 15-bit CRT random samples. The exact original CoD symbol is
// not proven (no syscall-id table, no string names it), so a proven-role name is
// used with this uncertainty note. The server bank's Q_random/Q_crandom are related
// PRNG primitives but do not match this two-rand polar-rejection shape, so they are
// not adopted.
//
// ---- Register/stack ABI (frame base E = ESP after `SUB ESP,0x20`) -----------------
//   ESI   = out, float[2] out-parameter (register argument; written at [ESI],[ESI+4]).
//   EDI   = ref, const float[2] reference point (register argument; read [EDI],[EDI+4]).
//   [E+0x24] = arg0 = minDist (float; the incoming stack argument). NOTE the body
//              reuses this stack slot as scratch for cos(angle) after loading minDist
//              (minDist^2 is cached in a local before the reuse), and the caller
//              cleans the single dword arg (`ADD ESP,4`), i.e. plain cdecl with two
//              register pointer params.
//   0x3003b52e `MOV EDI,EDI` is a 2-byte hot-patch NOP (no effect on EDI).
//
// The two `CALL 0x3005b879` are rand() (the statically-linked MSVC CRT PRNG returning
// [0,32767]); portable code calls the explicit coduo_crt_rand boundary shim.
//
// FSINCOS (0x3003b563) leaves ST0=cos(angle), ST1=sin(angle); the two FSTPs then
// store cos to [E+0x24] and sin to [E+0x00].
//
// Loop exit test (0x3003b5dd..0x3003b5e6): `FCOMP distSq(threshold)` sets C0/C2/C3;
// `TEST AH,0x5` isolates C0|C2, `JNP` re-enters the loop while PF=0. For the ordered
// case C2=0, so PF reflects C0: distSq_point < minDist^2 -> C0=1 -> PF=0 -> loop
// (reject, too close); distSq_point >= minDist^2 -> C0=0 -> PF=1 -> fall through
// (accept). i.e. keep sampling while the point is closer than minDist to ref.

void CG_PoissonDiskSample(vec2_t out, const vec2_t ref, float minDist)
{
    /* [ESP+0x14]: cached rejection threshold = minDist*minDist (0x3003b513..0x3003b52a). */
    const float minDistSq = minDist * minDist;

    float angle;   /* [ESP+0x08] after first rand block */
    float cosA;    /* stored into the reused [ESP+0x24] slot by FSINCOS */
    float sinA;    /* stored into [ESP+0x00] by FSINCOS */
    float radius;  /* [ESP+0x04] = rand()/32768 in [0,1) */
    float dx, dy;  /* [ESP+0x18], [ESP+0x1c] */
    long double distSq; /* never stored: FADDP feeds FCOMP directly (0x3003b5db) */

    do {
        /* angle = PI * (2*rand()/32768 - 1), range [-PI, PI). (0x3003b530..0x3003b559)
         * Single expression: the DLL rounds only the (float)rand() cast and the
         * final product (one FSTP at 0x3003b559); the /32768, *2, -1 chain stays
         * in st0. */
        angle = (((float)coduo_crt_rand() / 32768.0f) * 2.0f - 1.0f) * 3.1415927410125732f; /* +PI at 0x3007bd88 */

        /* FSINCOS(angle): cos into cosA, sin into sinA (both from one instruction;
         * ST0=cos, ST1=sin). (0x3003b55f..0x3003b56f) */
        coduo_x87_sincosf(angle, &sinA, &cosA);

        /* radius = rand()/32768, range [0,1). (0x3003b573..0x3003b58e) */
        radius = (float)coduo_crt_rand() / 32768.0f;

        /* out = (radius*cos, radius*sin). (0x3003b592..0x3003b5b7) */
        out[0] = radius * cosA;
        out[1] = radius * sinA;

        /* distSq = |out - ref|^2. (0x3003b5ba..0x3003b5db) The dy term is loaded
         * first (0x3003b5cb) and dx second (0x3003b5d3); term order is kept in
         * that order to match. Value-identical: one FADDP chain, no store. */
        dx = out[0] - ref[0];
        dy = out[1] - ref[1];
        distSq = dy * dy + dx * dx;

        /* Reject (loop) while the point is closer than minDist to ref;
         * accept when distSq >= minDistSq. (0x3003b5dd..0x3003b5e6) */
    } while (distSq < minDistSq);
}
