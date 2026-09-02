// Source: uo_cgame_mp_x86.dll 0x30048260..0x30048451
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30048260_30048451.mcode
//
// CG_SpawnTracerLine — spawn a moving LINE bullet-tracer as an LE_MOVING_TRACER local
// entity that travels from the shot START point toward the shot END point. This is the
// alternate-path twin of CG_SpawnMovingTracer (0x30048a00): where that function builds
// tracer polygon endpoints directly for the current frame, this one allocates a
// self-animating localEntity_t whose TR_LINEAR trajectory carries the tracer along the
// ray over its lifetime (CG_AddMovingTracer renders it each frame until it expires).
//
// NAME ADJUDICATION: the .mcode's mechanical `# name Menus_Open` is a broad-corpus
// SIZE-GUESS (win 0x1f1 ~ matched 0x1f0 from cgame_mp.dll) with zero behavioral basis and
// is REJECTED — this is a cgame tracer / local-entity builder, not a UI menu opener. The
// name CG_SpawnTracerLine is the caller-observed placeholder already carried in
// client_recovered.h for 0x30048260; it is corroborated here by the body: the same
// bg_weaponInfos[weapon]->ammoType in {3,4,5} mode select and the same
// cg_tracerLengthMode{A,B} twins as CG_AddMovingTracer / CG_SpawnMovingTracer, the same
// LEF_TRACER_MODE_A flag, and it hangs a LE_MOVING_TRACER (leType==2) entity on the
// cg_activeLocalEntities list via CG_AllocLocalEntity. Exact CoD symbol unproven; role
// name kept.
//
// ABI (mixed register/stack, proven from this body + the sole caller CG_SpawnTracer's
// non-7 branch at 0x30048e40..0x30048e4b: `MOV EAX,impactOrigin; LEA ECX,&muzzle;
// PUSH weapon; CALL; ADD ESP,4`):
//   register EAX -> endOrigin   (vec3, shot end point;   read as end[0..2] at +0/+4/+8)
//   register ECX -> startOrigin (vec3, shot start point; read as start[0..2] at +0/+4/+8)
//   stack  arg0  -> weapon      (int; index into bg_weaponInfos[] for length/flag select)
// caller-cleaned (bare RET; caller does ADD ESP,4). The register/stack split is an i386
// calling-convention detail; recovered here as ordered C parameters with no attribute
// (the syntax-only build does not require one).

#include "../client_recovered.h"
#include "../globals.h"
#include "../platform/crt_boundary.h"

#include <math.h>    /* sqrtl: models _CIsqrt (CALL 0x3006bee0) on the unrounded st0 arg */
#include <stdint.h>

/*
 * Exact constants read by this function:
 *   float @0x3007bff8 = -1000.0f  (ms/sec sign-flipped scale in the endTime formula)
 *   float @0x304506e8 = cg_tracerlengthlmg_vmCvar.value  (mode-A tracer length; .data)
 *   float @0x30530668 = cg_tracerlength_vmCvar.value  (mode-B tracer length; .data)
 *   float @0x304556c8 = cg_tracerSpeed        (tracer travel speed, units/s; .data)
 *   int32 @0x304831ac = cg_frametime  (current client frame duration, ms)
 *   int32 @0x304831b0 = cg_time       (current client time base, ms)
 * The -1000.0f is a .rdata pool constant (dumped via objdump -s -j .rdata); the rest are
 * mutable .data floats/ints declared in globals.h.
 */

void CG_SpawnTracerLine(const vec3_t endOrigin, const vec3_t startOrigin, int32_t weapon)
{
    /* 30048263..30048279: fetch the weapon's tracer descriptor (may be NULL). */
    const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon]; /* [0x30134cd8][weapon] */

    /* 3004826a..30048298: dir = endOrigin - startOrigin. (start[0..2] held in EBX/EBP/ECX
     * across the body; end[0..2] copied to local slots [ESP+0x14/0x18/0x1c].) */
    vec3_t newEnd; /* [ESP+0x14..0x1c]: end point, later pulled back along dir */
    newEnd[0] = endOrigin[0];
    newEnd[1] = endOrigin[1];
    newEnd[2] = endOrigin[2];

    vec3_t dir; /* [ESP+0x2c..0x34]: unit direction from start toward end */
    dir[0] = endOrigin[0] - startOrigin[0];
    dir[1] = endOrigin[1] - startOrigin[1];
    dir[2] = endOrigin[2] - startOrigin[2];

    /* 300482c4: VectorNormalize(dir) — normalizes dir in place, returns the shot span. */
    float dist = VectorNormalize(dir); /* 0x30049700; parked in [ESP+0x10] */

    /* 300482d1..300482fb: select the tracer length by the weapon's ammo type
     * (LMG/HMG/UMG -> mode A, else mode B). NULL weaponInfo -> mode B. */
    float tracerLength;
    if (weaponInfo != 0 && (weaponInfo->ammoType == WEAPON_AMMO_TYPE_LMG || weaponInfo->ammoType == WEAPON_AMMO_TYPE_HMG ||
                            weaponInfo->ammoType == WEAPON_AMMO_TYPE_UMG)) {
        tracerLength = cg_tracerlengthlmg_vmCvar.value; /* 0x304506e8 */
    } else {
        tracerLength = cg_tracerlength_vmCvar.value; /* 0x30530668 */
    }

    /* 300482ff..30048308: FCOMP dist,tracerLength / TEST AH,0x5 / JNP -> exit. Proceed only
     * when the shot span exceeds the tracer length (a span at least as long as the tracer,
     * so pulling the endpoint back by tracerLength still leaves a positive segment). */
    if (dist < tracerLength) {                 /* 0x30048305 TEST AH,0x5; JNP exit: taken only
                                                * when dist < tracerLength (equality falls through
                                                * and builds the tracer). Prior pass used <=. */
        return;
    }

    /* 3004830e..30048344: newEnd = endOrigin - tracerLength*dir (FCHS makes -tracerLength,
     * then newEnd[i] = (-tracerLength)*dir[i] + end[i]). This is the tail endpoint the
     * tracer will reach at end-of-life. */
    float negLength = -tracerLength;
    newEnd[0] = negLength * dir[0] + newEnd[0];
    newEnd[1] = negLength * dir[1] + newEnd[1];
    newEnd[2] = negLength * dir[2] + newEnd[2];

    /* 30048348..30048396: span = length(newEnd - startOrigin) — the distance the tracer
     * travels from its spawn point to the pulled-back endpoint. */
    vec3_t seg; /* [ESP+0x38..0x40] */
    seg[0] = newEnd[0] - startOrigin[0];
    seg[1] = newEnd[1] - startOrigin[1];
    seg[2] = newEnd[2] - startOrigin[2];
    /* _CIsqrt (CALL 0x30048388 -> 0x3006bee0) consumes the 80-bit sum in st0 with no
     * argument rounding and returns the root in st0; the single rounding is FSTP DWORD
     * [ESP+0x48] @0x3004838d (then copied to span's slot). sqrtl keeps arg+result
     * 80-bit and the store to `float span` is that one rounding -- sqrtf would round
     * the argument to float before the call, a rounding the binary does not perform. */
    float span = (float)sqrtl((long double)seg[2] * (long double)seg[2] + (long double)seg[1] * (long double)seg[1] +
                              (long double)seg[0] * (long double)seg[0]); /* 0x3006bee0 */

    /* 30048399..300483a2: allocate a zeroed local entity, leType = LE_MOVING_TRACER (2). */
    localEntity_t *le = CG_AllocLocalEntity(); /* 0x3002aa70 */
    le->leType = LE_MOVING_TRACER;

    /* 300483a9..300483c7: mode-A ammo types also mark leFlags = LEF_TRACER_MODE_A, which
     * makes CG_AddMovingTracer read the mode-A width/length twins when rendering. */
    if (weaponInfo != 0 && (weaponInfo->ammoType == WEAPON_AMMO_TYPE_LMG || weaponInfo->ammoType == WEAPON_AMMO_TYPE_HMG ||
                            weaponInfo->ammoType == WEAPON_AMMO_TYPE_UMG)) {
        le->leFlags = LEF_TRACER_MODE_A; /* 0x20 */
    }

    /* 300483c7..300483e7: startDither = (rand() % cg.frametime) / 2, or 0 when frametime is
     * 0. The SAR EAX,1 after SUB EAX,EDX;CDQ is a signed /2; the IDIV remainder is already
     * >= 0 for rand() >= 0, so this is a small non-negative sub-frame ms offset used to
     * spread otherwise-coincident tracer spawns across the frame. */
    int32_t startDither;
    if (cg_frametime != 0) { /* 0x304831ac */
        startDither = (coduo_crt_rand() % cg_frametime) / 2; /* rand 0x3005b879 */
    } else {
        startDither = 0;
    }

    /* 300483e7..30048406: startTime = cg.time - startDither; endTime = startTime +
     * (int)(span * 1000 / cg_tracerSpeed). The machine computes span*(-1000)/speed and
     * truncates via _ftol2 (0x3006be3c) into a negative int, then endTime = startTime -
     * that == startTime + span*1000/speed. i.e. the ms at which the TR_LINEAR trajectory
     * (speed units/s) carries the tracer the full `span` from start to newEnd. */
    int32_t startTime = coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)startDither); /* 0x304831b0 */
    int32_t travelMs = coduo_fp_to_i32_extended((long double)span * (long double)-1000.0f /
                                                (long double)cg_tracerSpeed_vmCvar.value); /* 0x3007bff8 / 0x304556c8, trunc 0x3006be3c */
    le->endTime = coduo_int32_from_bits((uint32_t)startTime - (uint32_t)travelMs);

    /* 3004840f..3004841f: TR_LINEAR trajectory based at the shot start point. */
    le->pos.trType = TR_LINEAR;         /* +0x18 = 2 */
    le->pos.trTime = startTime;         /* +0x1c */
    le->pos.trBase[0] = startOrigin[0]; /* +0x24 (EBX) */
    le->pos.trBase[1] = startOrigin[1]; /* +0x28 (EBP) */
    le->pos.trBase[2] = startOrigin[2]; /* +0x2c ([ESP+0x28]) */

    /* 30048422..30048446: trDelta = cg_tracerSpeed * dir — the linear velocity vector that
     * moves trBase to newEnd over (endTime - trTime) ms. */
    le->pos.trDelta[0] = cg_tracerSpeed_vmCvar.value * dir[0]; /* +0x30 */
    le->pos.trDelta[1] = cg_tracerSpeed_vmCvar.value * dir[1]; /* +0x34 */
    le->pos.trDelta[2] = cg_tracerSpeed_vmCvar.value * dir[2]; /* +0x38 */
}
