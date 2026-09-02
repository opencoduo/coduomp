/*
 * Collision-behavior digest (scope harness, NOT part of the reconstruction).
 *
 * Emits a deterministic fingerprint of the loaded world collision so a faithful
 * (x87 / -mfpmath=387) build and an SSE build can be diffed map-by-map to
 * measure how far x87-vs-SSE divergence actually reaches. It fires a fixed grid
 * of box traces through the world model and hashes the exact float bits of the
 * results (fraction / endpos / normal) plus topology counts.
 *
 * Compiled only when CODUO_COLLISION_DIGEST is defined; otherwise this file
 * expands to nothing, so it has zero effect on a normal build. Triggered at
 * runtime by cvar `cd_collisionDigest 1`.
 */
#ifdef CODUO_COLLISION_DIGEST

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compat/crt/random_compat.h"
#include "qcommon/q_command.h"
#include "../core_math/core_math_private.h"
#include "../core_runtime/core_runtime_private.h"
#include "../physics_collision/cm_patch_private.h"
#include "../physics_collision/cm_trace_core_private.h"
#include "../physics_collision/cm_world_sector_private.h"
#include "server/standalone/bindings/coduo_engine_structs.h"

/*
 * CM_LoadMap wall-clock timing (cvar cd_loadTiming 1). Measures the collision
 * load itself — from just after the early-out to just before the digest work —
 * so a build's collision-geometry cost (the part the x87 emulation changes) can
 * be compared across the 32-bit, -mfpmath=387 and EMULATE_X87 builds. Uses
 * CLOCK_MONOTONIC so it is unaffected by wall-clock adjustments.
 */
static struct timespec cd_loadTimingStart;

/* NOT_FROM_ORIGINAL_SOURCE: collision parity timing probe. */
void coduo_engine_collision_load_timing_begin(void)
{
    if (Cvar_VariableIntegerValue("cd_loadTiming") == 0) {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &cd_loadTimingStart);
}

/* NOT_FROM_ORIGINAL_SOURCE: collision parity timing report. */
void coduo_engine_collision_load_timing_end(const char *mapName)
{
    if (Cvar_VariableIntegerValue("cd_loadTiming") == 0) {
        return;
    }

    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsedMsec =
        ((double)(end.tv_sec - cd_loadTimingStart.tv_sec) * 1000.0) +
        ((double)(end.tv_nsec - cd_loadTimingStart.tv_nsec) / 1000000.0);

    /* Report the terrain/curved split: the two collide builders have very
     * different FP cost. Terrain patches go through CM_GenerateTerrainCollide;
     * curved patches through the much heavier CM_GeneratePatchCollide winding/
     * bevel chain, which dominates the emulated build's load time. */
    int32_t curvedCount = 0;
    for (int32_t i = 0; i < cm_numTerrainPatches; ++i) {
        if (cm_terrainPatches[i].terrainCollide == NULL) {
            ++curvedCount;
        }
    }

    Com_Printf("LOADTIMING map=%s msec=%.3f patches=%d curved=%d brushes=%d\n",
               mapName, elapsedMsec, cm_numTerrainPatches, curvedCount,
               cm_numBrushes);
}

/* FNV-1a over raw bytes so identical float bits hash identically and any
 * single-bit FP divergence changes the digest. */
static uint64_t cm_digestAccum;

/* NOT_FROM_ORIGINAL_SOURCE: collision parity byte accumulator. */
static void coduo_engine_collision_digest_bytes(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < length; ++i) {
        cm_digestAccum ^= bytes[i];
        cm_digestAccum *= UINT64_C(0x00000100000001B3);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: same FNV-1a step against a caller-supplied
 * accumulator, so code that owns private collision-build types can
 * fold its own data in. */
void coduo_engine_collision_digest_bytes_external(const void *data, size_t length, uint64_t *accum)
{
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < length; ++i) {
        *accum ^= bytes[i];
        *accum *= UINT64_C(0x00000100000001B3);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE:
 * Probe-ray construction.
 *
 * EVERY float the probe computes MUST come out bit-identical in every build, or
 * the harness feeds each build a different ray and the digests can never match —
 * the divergence would be the probe's, not the engine's. (That exact bug made
 * the trace digest unmatchable until it was found.) This file is not emulated
 * (it is not reconstructed code), so on an EMULATE_X87 build it runs the host's
 * native FP while the reference build runs x87: an expression like
 * `mins + (maxs - mins) * f` then differs by an ULP because x87 keeps the
 * product in an 80-bit register.
 *
 * Rule: derive positions from *integers* and force every float intermediate
 * through a volatile slot, so each step rounds to single precision everywhere.
 * This is an input constructor for the parity probe.
 */
static float coduo_engine_collision_probe_lerp(float lo, float hi, int32_t numerator,
                          int32_t denominator)
{
    volatile float frac = (float)numerator / (float)denominator;
    volatile float span = hi - lo;
    volatile float step = span * frac;
    volatile float result = lo + step;
    return result;
}

/*
 * Flip-rate probe (cvar cd_flipProbe 1). When set, coduo_engine_collision_probe_trace prints one
 * deterministic line per trace with the exact result bits. Two builds that
 * differ only in their x87f_* backend (EMU_X87_SOFTFLOAT vs EMU_X87_DOUBLE)
 * fire the identical trace sequence, so diffing the two dumps line-for-line
 * counts how many collision decisions the approximation flips vs the exact
 * backend. That flip count / total is the approximation's quality metric.
 * (Both builds run the same emulated trace source and the same native FP for
 * any not-yet-emulated helper, so the only variable is the backend.)
 */
static int cm_flipDump;
static int64_t cm_flipSeq;

/* NOT_FROM_ORIGINAL_SOURCE: trace once and fold the result into the digests. */
static void coduo_engine_collision_probe_trace(const vec3_t start, const vec3_t end,
                          const vec3_t mins, const vec3_t maxs,
                          qboolean capsule, uint64_t *hitDigest,
                          int64_t *traceCount, int64_t *hitCount,
                          int64_t *solidCount)
{
    /* Mask -1 = stop on any content, so every solid surface (brush or patch)
     * registers; we only need a deterministic, broadly-hitting probe. */
    trace_t trace;
    memset(&trace, 0, sizeof(trace));
    CM_BoxTrace(&trace, start, end, mins, maxs, 0, -1, capsule);

    if (cm_flipDump) {
        const uint32_t *n = (const uint32_t *)trace.normal;
        Com_Printf("TR %lld f=%08x n=%08x,%08x,%08x s=%d%d\n",
                   (long long)cm_flipSeq++, *(const uint32_t *)&trace.fraction,
                   n[0], n[1], n[2], (int)trace.allsolid, (int)trace.startsolid);
    }

    coduo_engine_collision_digest_bytes(&trace.fraction, sizeof(trace.fraction));
    coduo_engine_collision_digest_bytes(trace.endpos, sizeof(trace.endpos));
    coduo_engine_collision_digest_bytes(trace.normal, sizeof(trace.normal));
    coduo_engine_collision_digest_bytes(&trace.allsolid, sizeof(trace.allsolid));
    coduo_engine_collision_digest_bytes(&trace.startsolid, sizeof(trace.startsolid));

    ++*traceCount;
    if (trace.allsolid != 0 || trace.startsolid != 0) {
        ++*solidCount;
    }
    if (trace.fraction < 1.0f) {
        ++*hitCount;
        /* hits-only digest: fold this trace's bits in so we can tell
         * real-collision divergence from miss/float noise. */
        uint64_t saved = cm_digestAccum;
        cm_digestAccum = *hitDigest;
        coduo_engine_collision_digest_bytes(&trace.fraction, sizeof(trace.fraction));
        coduo_engine_collision_digest_bytes(trace.endpos, sizeof(trace.endpos));
        coduo_engine_collision_digest_bytes(trace.normal, sizeof(trace.normal));
        *hitDigest = cm_digestAccum;
        cm_digestAccum = saved;
    }
}

/*
 * Direct unit harness for CM_TraceSphereThroughTerrainCollide (cvar
 * cd_capsulePatchDigest 1).
 *
 * The whole-map trace digest cannot isolate this one function: a capsule box
 * trace also runs the capsule-vs-brush / bbox helpers, several of which are not
 * yet x87-emulated, so their divergence swamps the signal. This harness instead
 * calls CM_TraceSphereThroughTerrainCollide DIRECTLY, per soup patch, with
 * build-independent synthetic inputs (every float forced through a volatile slot
 * so both the -mfpmath=387 reference and the EMULATE_X87 build feed identical
 * bits), and hashes only that function's output (fraction / normal / startsolid).
 * A match between the two builds proves this function alone is bit-exact.
 *
 * All three hit branches (facet-edge, vertex-sphere, edge-cylinder) match the
 * 32-bit x87 reference bit-for-bit across the map corpus. Reaching that on the
 * cylinder branch also required emulating ProjectPointOnPlane, which builds the
 * edge-cylinder radialAxes at map load (via PerpendicularVector) — an upstream
 * input this function reads. This harness catches regressions in that
 * construction too, since a radialAxes drift shows up as a cylinder-normal
 * mismatch here.
 */
/* NOT_FROM_ORIGINAL_SOURCE: capsule/patch parity probe. */
void coduo_engine_emit_capsule_patch_digest(const char *mapName)
{
    if (Cvar_VariableIntegerValue("cd_capsulePatchDigest") == 0) {
        return;
    }

    uint64_t digest = UINT64_C(0xcbf29ce484222325);
    int64_t calls = 0;
    int64_t hits = 0;

    /* Fixed, exactly-representable capsule dimensions (radius 15, halfheight
     * 40 -> capOffset 25): all powers-of-two-friendly so they are identical
     * in every build. */
    static const float kRadius = 15.0f;
    static const float kHalfheight = 40.0f;

    for (int32_t patchIndex = 0; patchIndex < cm_numTerrainPatches;
         ++patchIndex) {
        const collisionTerrainPatch_t *patch =
            &cm_terrainPatches[patchIndex];
        if (patch->terrainCollide == NULL || patch->curveCollide != NULL) {
            continue;
        }

        /* A small grid of synthetic capsule traces per patch: straight down
         * and diagonally across, from just above the patch to just below, at
         * a few offsets so the facet / vertex-sphere / edge-cylinder branches
         * all get exercised. */
        enum { N = 4 };
        for (int32_t gx = 0; gx < N; ++gx) {
            for (int32_t gy = 0; gy < N; ++gy) {
                traceWork_t tw;
                memset(&tw, 0, sizeof(tw));

                tw.start[0] = coduo_engine_collision_probe_lerp(patch->bounds[0][0], patch->bounds[1][0],
                                           gx * 2 + 1, N * 2);
                tw.start[1] = coduo_engine_collision_probe_lerp(patch->bounds[0][1], patch->bounds[1][1],
                                           gy * 2 + 1, N * 2);
                tw.start[2] = patch->bounds[1][2] + 24.0f;
                /* diagonal end: shifted one cell over so delta is non-axial */
                tw.end[0] = coduo_engine_collision_probe_lerp(patch->bounds[0][0], patch->bounds[1][0],
                                         (N - gx) * 2 - 1, N * 2);
                tw.end[1] = coduo_engine_collision_probe_lerp(patch->bounds[0][1], patch->bounds[1][1],
                                         (N - gy) * 2 - 1, N * 2);
                tw.end[2] = patch->bounds[0][2] - 24.0f;

                /* delta and its length², forced to single precision at EVERY
                 * step so the harness math itself is build-independent. Storing
                 * only the final sum to a volatile is NOT enough: the products
                 * and partial sums would still evaluate in the build's native FP
                 * (x87 80-bit on i386 vs SSE on the emulated build), feeding the
                 * two builds different inputs. Each op therefore rounds through
                 * its own volatile slot. */
                for (int32_t a = 0; a < 3; ++a) {
                    volatile float d = tw.end[a] - tw.start[a];
                    tw.delta[a] = d;
                }
                volatile float p0 = tw.delta[0] * tw.delta[0];
                volatile float p1 = tw.delta[1] * tw.delta[1];
                volatile float p2 = tw.delta[2] * tw.delta[2];
                volatile float s01 = p0 + p1;
                volatile float dls = s01 + p2;
                tw.deltaLengthSquared = dls;

                tw.sphere.use = 1;
                tw.sphere.radius = kRadius;
                tw.sphere.halfheight = kHalfheight;
                tw.isPoint = 0;
                tw.trace.fraction = 1.0f;

                /* Bump the global check-count so the per-vertex/-edge sphere
                 * caches re-evaluate on every call (as a real trace would). */
                ++cm_checkcount;

                CM_TraceSphereThroughTerrainCollide(&tw, patch->terrainCollide);

                cm_digestAccum = digest;
                coduo_engine_collision_digest_bytes(&tw.trace.fraction, sizeof(tw.trace.fraction));
                coduo_engine_collision_digest_bytes(tw.trace.normal, sizeof(tw.trace.normal));
                coduo_engine_collision_digest_bytes(&tw.trace.startsolid,
                               sizeof(tw.trace.startsolid));
                digest = cm_digestAccum;

                ++calls;
                if (tw.trace.fraction < 1.0f || tw.trace.startsolid != 0) {
                    ++hits;
                }
            }
        }
    }

    Com_Printf("CAPSULEPATCHDIGEST map=%s calls=%lld hits=%lld digest=%016llx\n",
               mapName, (long long)calls, (long long)hits,
               (unsigned long long)digest);
}

/* NOT_FROM_ORIGINAL_SOURCE: collision-result parity probe. */
void coduo_engine_emit_collision_digest(const char *mapName)
{
    if (Cvar_VariableIntegerValue("cd_collisionDigest") == 0) {
        return;
    }

    cm_digestAccum = UINT64_C(0xcbf29ce484222325);
    cm_flipDump = Cvar_VariableIntegerValue("cd_flipProbe");
    cm_flipSeq = 0;

    /* Topology counts: cheap first-order divergence signal. */
    coduo_engine_collision_digest_bytes(&cm_numBrushes, sizeof(cm_numBrushes));
    coduo_engine_collision_digest_bytes(&cm_numTerrainPatches, sizeof(cm_numTerrainPatches));

    vec3_t worldMins;
    vec3_t worldMaxs;
    CM_ModelBounds(0, worldMins, worldMaxs);
    coduo_engine_collision_digest_bytes(worldMins, sizeof(worldMins));
    coduo_engine_collision_digest_bytes(worldMaxs, sizeof(worldMaxs));

    /*
     * The probe patterns below exist because a straight-down point-ray grid is
     * a WEAK probe of the trace path, and a weak probe that passes is worse
     * than no probe: vertical point rays resolve almost entirely through AXIAL
     * BSP planes (plane->type < 3), where the distance is `start[type] - dist`
     * — a single float subtract that is exact in x87, SSE and soft-float
     * alike — and they never reach the patch/terrain traces at all. Such a
     * digest cannot detect trace-path FP divergence, however faithful or
     * unfaithful the engine is.
     *
     * Each pattern therefore targets code the previous one cannot reach:
     *   1. vertical point rays  — the original probe; brush + tree walk
     *   2. angled rays          — force NON-AXIAL planes (plane->type >= 3),
     *                             i.e. the 3-term dots and split fractions
     *   3. box traces           — isPoint == 0: the bounds/offset paths and
     *                             the 2048.0f node offset
     *   4. capsule traces       — the sphere paths (radius/offset math)
     *   5. patch-aimed rays     — fired at each terrain patch's own bounds, to
     *                             reach CM_TracePointThroughTerrainCollide, which
     *                             the world-grid rays never call
     *   6. capsule-patch rays   — capsule traces aimed at each SOUP patch, the
     *                             only way to reach
     *                             CM_TraceSphereThroughTerrainCollide
     * Verify any change here with a poison test: break a trace function on
     * purpose and confirm the digest moves. A probe that cannot fail is not a
     * probe.
     */
    enum { GRID = 24 };
    int64_t traceCount = 0;
    int64_t solidCount = 0;
    int64_t hitCount = 0;    /* fraction < 1: ray actually struck geometry */
    uint64_t hitDigest = UINT64_C(0xcbf29ce484222325);

    static const vec3_t pointMins = {0.0f, 0.0f, 0.0f};
    static const vec3_t pointMaxs = {0.0f, 0.0f, 0.0f};
    /* A player-sized box and a capsule-ish box, both build-independent. */
    static const vec3_t boxMins = {-15.0f, -15.0f, -24.0f};
    static const vec3_t boxMaxs = {15.0f, 15.0f, 32.0f};

    /* 1. vertical point rays (the original pattern). */
    for (int32_t gx = 0; gx < GRID; ++gx) {
        for (int32_t gy = 0; gy < GRID; ++gy) {
            vec3_t start;
            vec3_t end;
            start[0] = coduo_engine_collision_probe_lerp(worldMins[0], worldMaxs[0], gx * 2 + 1,
                                    GRID * 2);
            start[1] = coduo_engine_collision_probe_lerp(worldMins[1], worldMaxs[1], gy * 2 + 1,
                                    GRID * 2);
            start[2] = worldMaxs[2] + 64.0f;
            end[0] = start[0];
            end[1] = start[1];
            end[2] = worldMins[2] - 64.0f;

            coduo_engine_collision_probe_trace(start, end, pointMins, pointMaxs, qfalse, &hitDigest,
                          &traceCount, &hitCount, &solidCount);
        }
    }

    /*
     * 2. angled rays. Corner-to-corner and cross-diagonal sweeps: a ray with
     * all three components non-zero is not parallel to any axial plane, so the
     * tree walk must take the plane->type >= 3 path (the 3-term dot, the
     * side tests, and the split-fraction math) instead of the exact axial
     * subtract. This is the pattern the original probe was missing.
     */
    enum { ANGLED = 16 };
    for (int32_t i = 0; i < ANGLED; ++i) {
        for (int32_t j = 0; j < ANGLED; ++j) {
            vec3_t start;
            vec3_t end;

            /* start high on one side, end low on the opposite side: the ray
             * crosses the map diagonally in all three axes. */
            start[0] = coduo_engine_collision_probe_lerp(worldMins[0], worldMaxs[0], i * 2 + 1,
                                    ANGLED * 2);
            start[1] = coduo_engine_collision_probe_lerp(worldMins[1], worldMaxs[1], j * 2 + 1,
                                    ANGLED * 2);
            start[2] = worldMaxs[2];
            end[0] = coduo_engine_collision_probe_lerp(worldMins[0], worldMaxs[0],
                                  (ANGLED - i) * 2 - 1, ANGLED * 2);
            end[1] = coduo_engine_collision_probe_lerp(worldMins[1], worldMaxs[1],
                                  (ANGLED - j) * 2 - 1, ANGLED * 2);
            end[2] = worldMins[2];

            coduo_engine_collision_probe_trace(start, end, pointMins, pointMaxs, qfalse, &hitDigest,
                          &traceCount, &hitCount, &solidCount);

            /* same ray, box swept: isPoint == 0 exercises the bounds/offset
             * paths and the 2048.0f non-axial node offset. */
            coduo_engine_collision_probe_trace(start, end, boxMins, boxMaxs, qfalse, &hitDigest,
                          &traceCount, &hitCount, &solidCount);

            /* and as a capsule: the sphere radius/offset math. */
            coduo_engine_collision_probe_trace(start, end, boxMins, boxMaxs, qtrue, &hitDigest,
                          &traceCount, &hitCount, &solidCount);
        }
    }

    /*
     * 3. patch-aimed rays. The world-grid rays above never reach the patch or
     * terrain traces (a call-counter probe measured zero calls to both), so
     * aim a short ray straight through each terrain patch's own bounds. This
     * is what exercises CM_TraceThroughPatch and, below it,
     * CM_TracePointThroughTerrainCollide / CM_TraceThroughPatchCollide.
     */
    for (int32_t patchIndex = 0; patchIndex < cm_numTerrainPatches;
         ++patchIndex) {
        const collisionTerrainPatch_t *patch =
            &cm_terrainPatches[patchIndex];
        vec3_t start;
        vec3_t end;

        /* down the middle of the patch, from just above it to just below */
        start[0] = coduo_engine_collision_probe_lerp(patch->bounds[0][0], patch->bounds[1][0], 1, 2);
        start[1] = coduo_engine_collision_probe_lerp(patch->bounds[0][1], patch->bounds[1][1], 1, 2);
        start[2] = patch->bounds[1][2] + 16.0f;
        end[0] = start[0];
        end[1] = start[1];
        end[2] = patch->bounds[0][2] - 16.0f;

        coduo_engine_collision_probe_trace(start, end, pointMins, pointMaxs, qfalse, &hitDigest,
                      &traceCount, &hitCount, &solidCount);

        /* an angled ray across the same patch, so the patch facet math sees a
         * non-degenerate direction too */
        start[0] = coduo_engine_collision_probe_lerp(patch->bounds[0][0], patch->bounds[1][0], 1, 4);
        start[1] = coduo_engine_collision_probe_lerp(patch->bounds[0][1], patch->bounds[1][1], 1, 4);
        end[0] = coduo_engine_collision_probe_lerp(patch->bounds[0][0], patch->bounds[1][0], 3, 4);
        end[1] = coduo_engine_collision_probe_lerp(patch->bounds[0][1], patch->bounds[1][1], 3, 4);

        coduo_engine_collision_probe_trace(start, end, pointMins, pointMaxs, qfalse, &hitDigest,
                      &traceCount, &hitCount, &solidCount);
        coduo_engine_collision_probe_trace(start, end, boxMins, boxMaxs, qfalse, &hitDigest,
                      &traceCount, &hitCount, &solidCount);
    }

    /*
     * 4. capsule-through-soup-patch rays. CM_TraceSphereThroughTerrainCollide is
     * reached only by a CAPSULE trace (sphere.use != 0, isPoint == 0) that
     * descends into a terrain-soup patch — one with curveCollide == NULL and
     * terrainCollide != NULL (see CM_TraceThroughPatch's branch and
     * CM_TraceThroughTerrainCollide's sphere dispatch). None of the probes above
     * can reach it: patterns 1/3 fire point rays (isPoint != 0 -> the POINT
     * path), and pattern 2's capsule rays are world-grid rays that resolve in
     * the BSP before ever entering a soup patch's leaf. So aim a capsule trace
     * straight down each soup patch's own bounds, plus one angled across it, so
     * the capsule facet math (the capOffset / capDelta / capTraceOffset chain
     * and the cylinder-edge tests) is actually exercised. Fired with box bounds
     * (boxMaxs[2]=32 > boxMaxs[0]=15 -> radius=15, halfheight=32, capOffset=17:
     * a non-degenerate capsule). Curved patches are skipped here because they
     * route through curveCollide, never terrainCollide.
     *
     * NOTE: this probe path exists to VALIDATE CM_TraceSphereThroughTerrainCollide
     * once its float sites are emulated; that function is not yet emulated, so a
     * faithful-vs-emulated diff of this digest is EXPECTED to differ until the
     * emulation lands. Confirm the path actually runs with a poison test (break
     * the capsule function and watch the digest move) — a probe that cannot fail
     * is not a probe.
     */
    for (int32_t patchIndex = 0; patchIndex < cm_numTerrainPatches;
         ++patchIndex) {
        const collisionTerrainPatch_t *patch =
            &cm_terrainPatches[patchIndex];

        /* soup patches only: patchCollide populated, terrainCollide empty */
        if (patch->terrainCollide == NULL || patch->curveCollide != NULL) {
            continue;
        }

        vec3_t start;
        vec3_t end;

        /* down the middle, top to bottom */
        start[0] = coduo_engine_collision_probe_lerp(patch->bounds[0][0], patch->bounds[1][0], 1, 2);
        start[1] = coduo_engine_collision_probe_lerp(patch->bounds[0][1], patch->bounds[1][1], 1, 2);
        start[2] = patch->bounds[1][2] + 16.0f;
        end[0] = start[0];
        end[1] = start[1];
        end[2] = patch->bounds[0][2] - 16.0f;

        coduo_engine_collision_probe_trace(start, end, boxMins, boxMaxs, qtrue, &hitDigest,
                      &traceCount, &hitCount, &solidCount);

        /* angled across the patch so the capsule sees a non-axial sweep */
        start[0] = coduo_engine_collision_probe_lerp(patch->bounds[0][0], patch->bounds[1][0], 1, 4);
        start[1] = coduo_engine_collision_probe_lerp(patch->bounds[0][1], patch->bounds[1][1], 1, 4);
        end[0] = coduo_engine_collision_probe_lerp(patch->bounds[0][0], patch->bounds[1][0], 3, 4);
        end[1] = coduo_engine_collision_probe_lerp(patch->bounds[0][1], patch->bounds[1][1], 3, 4);

        coduo_engine_collision_probe_trace(start, end, boxMins, boxMaxs, qtrue, &hitDigest,
                      &traceCount, &hitCount, &solidCount);
    }

    Com_Printf("COLLISIONDIGEST map=%s brushes=%d patches=%d traces=%lld "
               "hits=%lld solid=%lld digest=%016llx hitdigest=%016llx\n",
               mapName, cm_numBrushes, cm_numTerrainPatches,
               (long long)traceCount, (long long)hitCount, (long long)solidCount,
               (unsigned long long)cm_digestAccum,
               (unsigned long long)hitDigest);
}

/*
 * Per-patch collision-geometry parity dump — the fine-grained tool that drives
 * the x87-emulation work and defines the "FP sufficiently emulated" milestone.
 *
 * The trace digest above is one whole-map hash (matches / does-not-match, no
 * localization). This dumps a stable per-terrain-patch line — its bounds and a
 * hash of every generated triangle plane and barycentric vector — so a diff
 * between the emulated build and the x87 reference lists EXACTLY which patches
 * still diverge, pointing at the collision-construction functions that still
 * need emulating. Parity is reached when this dump is identical between the two
 * builds for every map in the corpus. Triggered by cvar `cd_collisionParity 1`.
 */
/* NOT_FROM_ORIGINAL_SOURCE: loaded-collision parity report. */
void coduo_engine_emit_collision_parity(const char *mapName)
{
    if (Cvar_VariableIntegerValue("cd_collisionParity") == 0) {
        return;
    }

    uint64_t mapAccum = UINT64_C(0xcbf29ce484222325);

    for (int32_t patchIndex = 0; patchIndex < cm_numTerrainPatches;
         ++patchIndex) {
        const collisionTerrainPatch_t *patch =
            &cm_terrainPatches[patchIndex];

        cm_digestAccum = UINT64_C(0xcbf29ce484222325);
        coduo_engine_collision_digest_bytes(patch->bounds[0], sizeof(patch->bounds[0]));
        coduo_engine_collision_digest_bytes(patch->bounds[1], sizeof(patch->bounds[1]));

        int32_t facetCount = 0;
        const collisionTriangleSoup_t *pc = patch->terrainCollide;
        if (pc != NULL) {
            facetCount = (int32_t)pc->triangleCount;
            for (int32_t facetIndex = 0; facetIndex < facetCount;
                 ++facetIndex) {
                const collisionSoupTriangle_t *facet =
                    &pc->triangles[facetIndex];
                coduo_engine_collision_digest_bytes(&facet->plane,
                                                    sizeof(facet->plane));
                coduo_engine_collision_digest_bytes(facet->svec,
                                                    sizeof(facet->svec));
                coduo_engine_collision_digest_bytes(facet->tvec,
                                                    sizeof(facet->tvec));
            }
        }

        uint64_t patchDigest = cm_digestAccum;

        /* fold the per-patch digest into the map-level accumulator (carry the
         * running mapAccum through coduo_engine_collision_digest_bytes, not the per-patch value) */
        cm_digestAccum = mapAccum;
        coduo_engine_collision_digest_bytes(&patchDigest, sizeof(patchDigest));
        mapAccum = cm_digestAccum;

        Com_Printf("PATCHPARITY %s patch=%d facets=%d "
                   "mins=[%.9g %.9g %.9g] maxs=[%.9g %.9g %.9g] "
                   "digest=%016llx\n",
                   mapName, patchIndex, facetCount,
                   (double)patch->bounds[0][0], (double)patch->bounds[0][1],
                   (double)patch->bounds[0][2], (double)patch->bounds[1][0],
                   (double)patch->bounds[1][1], (double)patch->bounds[1][2],
                   (unsigned long long)patchDigest);

        /* Raw per-facet plane float bits for the first patch, to localize
         * exactly which plane value diverges (cd_collisionParityRaw 1). */
        if (pc != NULL && patchIndex == 0 &&
            Cvar_VariableIntegerValue("cd_collisionParityRaw") != 0) {
            for (int32_t facetIndex = 0; facetIndex < facetCount;
                 ++facetIndex) {
                const collisionSoupTriangle_t *facet =
                    &pc->triangles[facetIndex];
                const float *sp = facet->plane.components.normal;
                Com_Printf("PARITYRAW f%d surf=%08x,%08x,%08x,%08x "
                           "e0=%08x,%08x,%08x,%08x e1=%08x,%08x,%08x,%08x\n",
                           facetIndex,
                           ((const uint32_t *)sp)[0], ((const uint32_t *)sp)[1],
                           ((const uint32_t *)sp)[2],
                           *(const uint32_t *)&facet->plane.components.distance,
                           ((const uint32_t *)facet->svec)[0],
                           ((const uint32_t *)facet->svec)[1],
                           ((const uint32_t *)facet->svec)[2],
                           ((const uint32_t *)facet->svec)[3],
                           ((const uint32_t *)facet->tvec)[0],
                           ((const uint32_t *)facet->tvec)[1],
                           ((const uint32_t *)facet->tvec)[2],
                           ((const uint32_t *)facet->tvec)[3]);
            }
        }
    }

    Com_Printf("PATCHPARITY %s TOTAL patches=%d digest=%016llx\n",
               mapName, cm_numTerrainPatches, (unsigned long long)mapAccum);

    /*
     * Soup-facet vertex-sphere / edge-cylinder data. PATCHPARITY above hashes
     * only plane + svec + tvec, NOT facet->vertices / oppositeEdges
     * — so a divergence in the sphere/cylinder construction (origin, radialAxes,
     * axis, length; see PerpendicularVector / ProjectPointOnPlane) would pass it.
     * Emit those separately so a backend/geometry comparison covers every
     * FP-constructed collision value. Kept off PATCHPARITY so the stock gdb
     * walker (which reproduces PATCHPARITY only) is unaffected.
     */
    uint64_t scAccum = UINT64_C(0xcbf29ce484222325);
    int64_t sphereCount = 0;
    int64_t cylinderCount = 0;
    for (int32_t patchIndex = 0; patchIndex < cm_numTerrainPatches;
         ++patchIndex) {
        const collisionTerrainPatch_t *patch =
            &cm_terrainPatches[patchIndex];
        const collisionTriangleSoup_t *pc = patch->terrainCollide;
        if (pc == NULL) {
            continue;
        }
        cm_digestAccum = scAccum;
        for (int32_t facetIndex = 0; facetIndex < (int32_t)pc->triangleCount;
             ++facetIndex) {
            const collisionSoupTriangle_t *facet = &pc->triangles[facetIndex];
            for (int32_t e = 0; e < 3; ++e) {
                const collisionSoupVertex_t *vs = facet->vertices[e];
                uint8_t present = (vs != NULL);
                coduo_engine_collision_digest_bytes(&present, sizeof(present));
                if (vs != NULL) {
                    coduo_engine_collision_digest_bytes(vs->position,
                                                        sizeof(vs->position));
                    ++sphereCount;
                }
                const collisionSoupEdge_t *ec = facet->oppositeEdges[e];
                present = (ec != NULL);
                coduo_engine_collision_digest_bytes(&present, sizeof(present));
                if (ec != NULL) {
                    coduo_engine_collision_digest_bytes(ec->origin, sizeof(ec->origin));
                    coduo_engine_collision_digest_bytes(ec->radialAxes, sizeof(ec->radialAxes));
                    coduo_engine_collision_digest_bytes(
                        ec->unitDirection, sizeof(ec->unitDirection));
                    coduo_engine_collision_digest_bytes(&ec->length, sizeof(ec->length));
                    ++cylinderCount;
                }
            }
        }
        scAccum = cm_digestAccum;
    }
    Com_Printf("SPHERECYLPARITY %s TOTAL spheres=%lld cylinders=%lld "
               "digest=%016llx\n",
               mapName, (long long)sphereCount, (long long)cylinderCount,
               (unsigned long long)scAccum);

    /*
     * Curved patches populate curveCollide (CM_GeneratePatchCollide) instead
     * of terrainCollide, so the PATCHPARITY dump above reports facets=0 for them
     * and measures nothing. Dump those separately: per-patch, the generated
     * plane floats + facet plane indices/flags produced by the winding/bevel
     * pipeline. Parity requires these to match too.
     */
    uint64_t curvedAccum = UINT64_C(0xcbf29ce484222325);
    int32_t curvedCount = 0;

    for (int32_t patchIndex = 0; patchIndex < cm_numTerrainPatches;
         ++patchIndex) {
        const collisionTerrainPatch_t *patch =
            &cm_terrainPatches[patchIndex];
        if (patch->curveCollide == NULL) {
            continue;
        }

        uint64_t patchAccum = UINT64_C(0xcbf29ce484222325);
        int32_t planeCount = 0;
        int32_t facetCount = 0;
        coduo_engine_digest_curve_collide(patch->curveCollide, &patchAccum,
                                          &planeCount, &facetCount);
        ++curvedCount;

        coduo_engine_collision_digest_bytes_external(&patchAccum, sizeof(patchAccum), &curvedAccum);

        Com_Printf("CURVEDPARITY %s patch=%d planes=%d facets=%d "
                   "digest=%016llx\n",
                   mapName, patchIndex, planeCount, facetCount,
                   (unsigned long long)patchAccum);
    }

    Com_Printf("CURVEDPARITY %s TOTAL curved=%d digest=%016llx\n", mapName,
               curvedCount, (unsigned long long)curvedAccum);
}

/*
 * Core-math cross-build differential digest (cvar cd_coreMathDigest 1).
 *
 * A "direct unit call" harness for the pure leaf functions in src/core_math:
 * runs each converted function over a fixed battery of inputs and folds the
 * float-projected result into a per-function FNV-1a digest. The x87 reference
 * build (-mfpmath=387) and the EMULATE_X87 build must print identical digests;
 * a mismatch means the x87f transcription of that function diverges from x87.
 *
 * Only x87-EMULATED functions belong here. An un-emulated core_math function
 * runs native host FP on the EMULATE build (SSE/NEON) versus x87 on the
 * reference, so it would mismatch for a reason that is not a transcription bug —
 * add each function to the battery as it is converted, not before.
 *
 * Results are projected to float32 before hashing because that is how every
 * caller consumes these returns: the genuinely-80-bit returns (DistanceSquared*)
 * leave an unrounded sum in st0 on the x86 reference but a float-rounded value
 * on the emulated build (see distance_squared.c), and only the caller's float
 * projection — round80->float either way — is the observable value.
 *
 * Inputs are float LITERALS only. Never compute a harness input with native FP:
 * that would feed the x87 reference and the emulated build different values (the
 * same trap documented for the probe rays above).
 */
/* NOT_FROM_ORIGINAL_SOURCE: core-math parity probe. */
void coduo_engine_emit_core_math_digest(void)
{
    if (Cvar_VariableIntegerValue("cd_coreMathDigest") == 0) {
        return;
    }

    static const vec3_t battery[] = {
        {         0.0f,          0.0f,        0.0f },
        {         1.0f,          2.0f,        3.0f },
        {        -4.5f,          0.25f,      16.0f },
        {         0.1f,          0.2f,        0.3f },
        {      1024.5f,      -2048.25f,    4096.125f },
        {  16777217.0f,          3.0f,       -7.0f }, /* 2^24+1: inexact float */
        { -0.0009765625f,  12345.6789f,      -0.5f },
        {    123456.0f,     654321.0f,      0.03125f },
    };
    const int n = (int)(sizeof(battery) / sizeof(battery[0]));

    /* vec4 battery for the quaternion / vec4 helpers, and positive scalars for
     * Q_rsqrt (its input domain is a magnitude). */
    static const vec4_t battery4[] = {
        {  1.0f,   2.0f,   3.0f,   4.0f },
        { -4.5f,   0.25f, 16.0f,  -0.5f },
        {  0.1f,   0.2f,   0.3f,   0.4f },
        {  1024.5f, -2048.25f, 4096.125f, 0.03125f },
        { 16777217.0f, 3.0f, -7.0f, 1.0f },
        {  0.0f,   0.0f,   0.0f,   0.0f },
    };
    const int n4 = (int)(sizeof(battery4) / sizeof(battery4[0]));
    static const float scalars[] = {
        1.0f, 2.0f, 0.25f, 16.0f, 1024.5f, 0.1f, 123456.0f, 0.03125f,
    };
    const int ns = (int)(sizeof(scalars) / sizeof(scalars[0]));

    uint64_t dDistanceSquared = UINT64_C(0xcbf29ce484222325);
    uint64_t dDistanceSquared2D = UINT64_C(0xcbf29ce484222325);
    uint64_t dDistance = UINT64_C(0xcbf29ce484222325);
    uint64_t dDistance2D = UINT64_C(0xcbf29ce484222325);
    uint64_t dVectorLength = UINT64_C(0xcbf29ce484222325);
    uint64_t dDotProduct = UINT64_C(0xcbf29ce484222325);
    uint64_t dVectorMax = UINT64_C(0xcbf29ce484222325);
    uint64_t dVectorNormalize2D = UINT64_C(0xcbf29ce484222325);
    uint64_t dVectorNormalize4 = UINT64_C(0xcbf29ce484222325);
    uint64_t dQuatEigenTrace = UINT64_C(0xcbf29ce484222325);
    uint64_t dQrsqrt = UINT64_C(0xcbf29ce484222325);
    uint64_t dvectoyaw = UINT64_C(0xcbf29ce484222325);
    uint64_t dvectopitch = UINT64_C(0xcbf29ce484222325);
    uint64_t dvectosignedyaw = UINT64_C(0xcbf29ce484222325);
    uint64_t dvectosignedpitch = UINT64_C(0xcbf29ce484222325);
    uint64_t dvectoangles = UINT64_C(0xcbf29ce484222325);
    uint64_t dvectosignedangles = UINT64_C(0xcbf29ce484222325);
    uint64_t dQ_acos = UINT64_C(0xcbf29ce484222325);
    uint64_t dAngleEigenTrace = UINT64_C(0xcbf29ce484222325);
    uint64_t dRotationToYaw = UINT64_C(0xcbf29ce484222325);
    uint64_t dNormalizeColor = UINT64_C(0xcbf29ce484222325);
    uint64_t dColorNormalize = UINT64_C(0xcbf29ce484222325);
    uint64_t dAngleVectors = UINT64_C(0xcbf29ce484222325);

    for (int i = 0; i < n; ++i) {
        float r;
        vec2_t v2i = { battery[i][0], battery[i][1] };
        vec3_t angles;
        vec3_t color;
        vec3_t avF, avR, avU;

        AngleVectors(battery[i], avF, avR, avU);
        coduo_engine_collision_digest_bytes_external(avF, sizeof(avF), &dAngleVectors);
        coduo_engine_collision_digest_bytes_external(avR, sizeof(avR), &dAngleVectors);
        coduo_engine_collision_digest_bytes_external(avU, sizeof(avU), &dAngleVectors);

        r = (float)RotationToYaw(v2i);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dRotationToYaw);

        r = (float)NormalizeColor(battery[i], color);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dNormalizeColor);
        coduo_engine_collision_digest_bytes_external(color, sizeof(color), &dNormalizeColor);
        r = (float)ColorNormalize(battery[i], color);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dColorNormalize);
        coduo_engine_collision_digest_bytes_external(color, sizeof(color), &dColorNormalize);

        r = _VectorLength(battery[i]);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dVectorLength);

        r = VectorMax(battery[i]);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dVectorMax);

        r = vectoyaw(battery[i]);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dvectoyaw);
        r = vectopitch(battery[i]);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dvectopitch);
        r = vectosignedyaw(v2i);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dvectosignedyaw);
        r = vectosignedpitch(battery[i]);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dvectosignedpitch);
        vectoangles(battery[i], angles);
        coduo_engine_collision_digest_bytes_external(angles, sizeof(angles), &dvectoangles);
        vectosignedangles(battery[i], angles);
        coduo_engine_collision_digest_bytes_external(angles, sizeof(angles), &dvectosignedangles);

        for (int j = 0; j < n; ++j) {
            r = VectorDistanceSquared(battery[i], battery[j]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dDistanceSquared);

            r = VectorDistanceSquared2D(battery[i], battery[j]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dDistanceSquared2D);

            r = VectorDistance(battery[i], battery[j]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dDistance);

            r = VectorDistance2D(battery[i], battery[j]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dDistance2D);

            r = _DotProduct(battery[i], battery[j]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dDotProduct);
        }
    }

    /* In-place vec2/vec4 normalizers: hash both the returned length and the
     * mutated vector (copy each input first — the battery is const). */
    for (int i = 0; i < n; ++i) {
        vec2_t v2 = { battery[i][0], battery[i][1] };
        float r = VectorNormalize2D(v2);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dVectorNormalize2D);
        coduo_engine_collision_digest_bytes_external(v2, sizeof(v2), &dVectorNormalize2D);
    }
    for (int i = 0; i < n4; ++i) {
        vec4_t v4 = { battery4[i][0], battery4[i][1], battery4[i][2],
                      battery4[i][3] };
        float r = VectorNormalize4D(v4);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dVectorNormalize4);
        coduo_engine_collision_digest_bytes_external(v4, sizeof(v4), &dVectorNormalize4);

        r = (float)QuatEigenTrace(battery4[i]);
        coduo_engine_collision_digest_bytes_external(
            &r, sizeof(r), &dQuatEigenTrace);
    }
    for (int i = 0; i < ns; ++i) {
        float r = (float)Q_rsqrt(scalars[i]);
        coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dQrsqrt);

        r = AngleEigenTrace((float)(i * 47) - 123.5f);
        coduo_engine_collision_digest_bytes_external(
            &r, sizeof(r), &dAngleEigenTrace);
    }
    {
        /* acos domain is [-1, 1]; include the +-pi clamp edges. */
        static const float acosInputs[] = { -1.0f,  -0.9f, -0.5f, -0.03125f,
                                             0.0f,    0.25f, 0.5f,  0.75f,
                                             0.9999f, 1.0f };
        const int na = (int)(sizeof(acosInputs) / sizeof(acosInputs[0]));
        for (int i = 0; i < na; ++i) {
            float r = (float)Q_acos(acosInputs[i]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dQ_acos);
        }
    }

    Com_Printf("COREMATHDIGEST fn=DistanceSquared digest=%016llx\n",
               (unsigned long long)dDistanceSquared);
    Com_Printf("COREMATHDIGEST fn=DistanceSquared2D digest=%016llx\n",
               (unsigned long long)dDistanceSquared2D);
    Com_Printf("COREMATHDIGEST fn=Distance digest=%016llx\n",
               (unsigned long long)dDistance);
    Com_Printf("COREMATHDIGEST fn=Distance2D digest=%016llx\n",
               (unsigned long long)dDistance2D);
    Com_Printf("COREMATHDIGEST fn=VectorLength digest=%016llx\n",
               (unsigned long long)dVectorLength);
    Com_Printf("COREMATHDIGEST fn=DotProduct digest=%016llx\n",
               (unsigned long long)dDotProduct);
    Com_Printf("COREMATHDIGEST fn=VectorMax digest=%016llx\n",
               (unsigned long long)dVectorMax);
    Com_Printf("COREMATHDIGEST fn=VectorNormalize2D digest=%016llx\n",
               (unsigned long long)dVectorNormalize2D);
    Com_Printf("COREMATHDIGEST fn=VectorNormalize4 digest=%016llx\n",
               (unsigned long long)dVectorNormalize4);
    Com_Printf("COREMATHDIGEST fn=QuatEigenTrace "
               "digest=%016llx\n",
               (unsigned long long)dQuatEigenTrace);
    Com_Printf("COREMATHDIGEST fn=Q_rsqrt digest=%016llx\n",
               (unsigned long long)dQrsqrt);
    Com_Printf("COREMATHDIGEST fn=vectoyaw digest=%016llx\n",
               (unsigned long long)dvectoyaw);
    Com_Printf("COREMATHDIGEST fn=vectopitch digest=%016llx\n",
               (unsigned long long)dvectopitch);
    Com_Printf("COREMATHDIGEST fn=vectosignedyaw digest=%016llx\n",
               (unsigned long long)dvectosignedyaw);
    Com_Printf("COREMATHDIGEST fn=vectosignedpitch digest=%016llx\n",
               (unsigned long long)dvectosignedpitch);
    Com_Printf("COREMATHDIGEST fn=vectoangles digest=%016llx\n",
               (unsigned long long)dvectoangles);
    Com_Printf("COREMATHDIGEST fn=vectosignedangles digest=%016llx\n",
               (unsigned long long)dvectosignedangles);
    Com_Printf("COREMATHDIGEST fn=Q_acos digest=%016llx\n",
               (unsigned long long)dQ_acos);
    Com_Printf("COREMATHDIGEST fn=AngleEigenTrace digest=%016llx\n",
               (unsigned long long)dAngleEigenTrace);
    Com_Printf("COREMATHDIGEST fn=RotationToYaw digest=%016llx\n",
               (unsigned long long)dRotationToYaw);
    Com_Printf("COREMATHDIGEST fn=NormalizeColor digest=%016llx\n",
               (unsigned long long)dNormalizeColor);
    Com_Printf("COREMATHDIGEST fn=ColorNormalize digest=%016llx\n",
               (unsigned long long)dColorNormalize);
    Com_Printf("COREMATHDIGEST fn=AngleVectors digest=%016llx\n",
               (unsigned long long)dAngleVectors);

    /* Matrix / transform helpers (self-contained battery). */
    {
        uint64_t dMatrixTransformVector = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorRotate = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixMultiply = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorAngleMultiply = UINT64_C(0xcbf29ce484222325);
        uint64_t dRotatePointAroundVector = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixTransposeTransformVector =
            UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixMultiplyEquals = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixMultiply34 = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixTransformPoint43Affine = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixTransformPoint43Compact = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixTransformPoint43CompactInPlace =
            UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixMultiply43 = UINT64_C(0xcbf29ce484222325);
        uint64_t dDObjSkelMatrixMultiply43 = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixInverse = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixTransposeTransformVector43 =
            UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixInverseOrthogonal43 = UINT64_C(0xcbf29ce484222325);
        uint64_t dDObjSkel2MatrixMultiply43 = UINT64_C(0xcbf29ce484222325);
        uint64_t dMatrixInverse44 = UINT64_C(0xcbf29ce484222325);
        for (int i = 0; i < n; ++i) {
            vec3_t m[3] = { { battery[i][0], battery[i][1], battery[i][2] },
                            { battery[(i + 1) % n][0], battery[(i + 1) % n][1],
                              battery[(i + 1) % n][2] },
                            { battery[(i + 2) % n][0], battery[(i + 2) % n][1],
                              battery[(i + 2) % n][2] } };
            vec3_t out;
            MatrixTransformVector(battery[(i + 3) % n], m, out);
            coduo_engine_collision_digest_bytes_external(out, sizeof(out), &dMatrixTransformVector);
            VectorRotate(battery[(i + 3) % n], m, out);
            coduo_engine_collision_digest_bytes_external(out, sizeof(out), &dVectorRotate);
            MatrixTransposeTransformVector(battery[(i + 3) % n], m, out);
            coduo_engine_collision_digest_bytes_external(out, sizeof(out),
                                   &dMatrixTransposeTransformVector);

            vec3_t le[3] = { { battery[(i + 1) % n][0], battery[(i + 1) % n][1],
                               battery[(i + 1) % n][2] },
                             { battery[(i + 2) % n][0], battery[(i + 2) % n][1],
                               battery[(i + 2) % n][2] },
                             { battery[(i + 3) % n][0], battery[(i + 3) % n][1],
                               battery[(i + 3) % n][2] } };
            vec3_t re[3] = { { m[0][0], m[0][1], m[0][2] },
                             { m[1][0], m[1][1], m[1][2] },
                             { m[2][0], m[2][1], m[2][2] } };
            MatrixMultiplyEquals(le, re);
            coduo_engine_collision_digest_bytes_external(re, sizeof(re), &dMatrixMultiplyEquals);

            float L43[3][4], R43[3][4], O43[3][4];
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 4; ++c) {
                    L43[r][c] = battery[(i + r) % n][c % 3];
                    R43[r][c] = battery[(i + c) % n][r];
                }
            }
            MatrixMultiply34(L43, R43, O43);
            coduo_engine_collision_digest_bytes_external(O43, sizeof(O43),
                                   &dMatrixMultiply34);

            matrix43_t cm, cm2;
            DObjSkelMat am;
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    cm.axis[r][c] = battery[(i + r) % n][c];
                }
                cm.origin[r] = battery[(i + 3) % n][r];
                for (int c = 0; c < 4; ++c) {
                    am.axis[r][c] = battery[(i + r) % n][c % 3];
                }
            }
            for (int c = 0; c < 4; ++c) {
                am.origin[c] = battery[(i + 3) % n][c % 3];
            }
            vec3_t pOut;
            MatrixTransformPoint43Affine(battery[(i + 4) % n], &am, pOut);
            coduo_engine_collision_digest_bytes_external(pOut, sizeof(pOut),
                                   &dMatrixTransformPoint43Affine);
            MatrixTransformPoint43Compact(battery[(i + 4) % n], &cm, pOut);
            coduo_engine_collision_digest_bytes_external(pOut, sizeof(pOut),
                                   &dMatrixTransformPoint43Compact);
            vec3_t pip = { battery[(i + 4) % n][0], battery[(i + 4) % n][1],
                           battery[(i + 4) % n][2] };
            MatrixTransformPoint43CompactInPlace(pip, &cm);
            coduo_engine_collision_digest_bytes_external(pip, sizeof(pip),
                                   &dMatrixTransformPoint43CompactInPlace);
            /* second compact factor for the 43-compact multiply */
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    cm2.axis[r][c] = battery[(i + c + 1) % n][r];
                }
                cm2.origin[r] = battery[(i + 2) % n][r];
            }
            matrix43_t cmOut;
            MatrixMultiply43(&cm, &cm2, &cmOut);
            coduo_engine_collision_digest_bytes_external(&cmOut, sizeof(cmOut),
                                   &dMatrixMultiply43);

            DObjSkelMatrixMultiply43(&am, &cm2, &cmOut);
            coduo_engine_collision_digest_bytes_external(&cmOut, sizeof(cmOut),
                                   &dDObjSkelMatrixMultiply43);

            vec3_t inv[3], invOut[3];
            for (int r = 0; r < 3; ++r) {
                inv[r][0] = m[r][0];
                inv[r][1] = m[r][1];
                inv[r][2] = m[r][2];
            }
            MatrixInverse(inv, invOut);
            coduo_engine_collision_digest_bytes_external(
                invOut, sizeof(invOut), &dMatrixInverse);

            vec3_t itv;
            MatrixTransposeTransformVector43(battery[(i + 4) % n], &cm, itv);
            coduo_engine_collision_digest_bytes_external(itv, sizeof(itv),
                                   &dMatrixTransposeTransformVector43);

            matrix43_t io;
            MatrixInverseOrthogonal43(&cm, &io);
            coduo_engine_collision_digest_bytes_external(&io, sizeof(io),
                                   &dMatrixInverseOrthogonal43);

            DObjSkelMat am2, amOut;
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 4; ++c) {
                    am2.axis[r][c] = battery[(i + c + 1) % n][r];
                }
                am2.origin[r] = battery[(i + 2) % n][r];
            }
            am2.origin[3] = 1.0f;
            DObjSkel2MatrixMultiply43(&am, &cm2, &amOut);
            coduo_engine_collision_digest_bytes_external(&amOut, sizeof(amOut),
                                   &dDObjSkel2MatrixMultiply43);

            float m44[4][4], inv44[4][4];
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    m44[r][c] = battery[(i + r) % n][c % 3] +
                                (r == c ? 1.0f : 0.0f);
                }
            }
            MatrixInverse44(m44, inv44);
            coduo_engine_collision_digest_bytes_external(inv44, sizeof(inv44), &dMatrixInverse44);
            (void)am2;

            float A[3][3], B[3][3], C[3][3];
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    A[r][c] = battery[(i + r) % n][c];
                    B[r][c] = battery[(i + c) % n][r];
                }
            }
            MatrixMultiply(A, B, C);
            coduo_engine_collision_digest_bytes_external(C, sizeof(C), &dMatrixMultiply);

            vec2_t p = { battery[i][0], battery[i][1] };
            VectorAngleMultiply(p, (float)(i * 37) - 90.0f);
            coduo_engine_collision_digest_bytes_external(p, sizeof(p), &dVectorAngleMultiply);

            /* non-zero dir (battery[0] is the zero vector). */
            vec3_t dst;
            RotatePointAroundVector(dst, m[1], battery[(i + 2) % n],
                                    (float)(i * 53) - 120.0f);
            coduo_engine_collision_digest_bytes_external(dst, sizeof(dst), &dRotatePointAroundVector);
        }
        Com_Printf("COREMATHDIGEST fn=MatrixTransformVector digest=%016llx\n",
                   (unsigned long long)dMatrixTransformVector);
        Com_Printf("COREMATHDIGEST fn=VectorRotate digest=%016llx\n",
                   (unsigned long long)dVectorRotate);
        Com_Printf("COREMATHDIGEST fn=MatrixMultiply digest=%016llx\n",
                   (unsigned long long)dMatrixMultiply);
        Com_Printf("COREMATHDIGEST fn=VectorAngleMultiply digest=%016llx\n",
                   (unsigned long long)dVectorAngleMultiply);
        Com_Printf("COREMATHDIGEST fn=RotatePointAroundVector digest=%016llx\n",
                   (unsigned long long)dRotatePointAroundVector);
        Com_Printf(
            "COREMATHDIGEST fn=MatrixTransposeTransformVector digest=%016llx\n",
            (unsigned long long)dMatrixTransposeTransformVector);
        Com_Printf("COREMATHDIGEST fn=MatrixMultiplyEquals digest=%016llx\n",
                   (unsigned long long)dMatrixMultiplyEquals);
        Com_Printf("COREMATHDIGEST fn=MatrixMultiply34 digest=%016llx\n",
                   (unsigned long long)dMatrixMultiply34);
        Com_Printf(
            "COREMATHDIGEST fn=MatrixTransformPoint43Affine digest=%016llx\n",
            (unsigned long long)dMatrixTransformPoint43Affine);
        Com_Printf(
            "COREMATHDIGEST fn=MatrixTransformPoint43Compact digest=%016llx\n",
            (unsigned long long)dMatrixTransformPoint43Compact);
        Com_Printf("COREMATHDIGEST fn=MatrixTransformPoint43CompactInPlace "
                   "digest=%016llx\n",
                   (unsigned long long)dMatrixTransformPoint43CompactInPlace);
        Com_Printf("COREMATHDIGEST fn=MatrixMultiply43 digest=%016llx\n",
                   (unsigned long long)dMatrixMultiply43);
        Com_Printf("COREMATHDIGEST fn=DObjSkelMatrixMultiply43 digest=%016llx\n",
                   (unsigned long long)dDObjSkelMatrixMultiply43);
        Com_Printf("COREMATHDIGEST fn=MatrixInverse digest=%016llx\n",
                   (unsigned long long)dMatrixInverse);
        Com_Printf(
            "COREMATHDIGEST fn=MatrixTransposeTransformVector43 digest=%016llx\n",
            (unsigned long long)dMatrixTransposeTransformVector43);
        Com_Printf(
            "COREMATHDIGEST fn=MatrixInverseOrthogonal43 digest=%016llx\n",
            (unsigned long long)dMatrixInverseOrthogonal43);
        Com_Printf(
            "COREMATHDIGEST fn=DObjSkel2MatrixMultiply43 digest=%016llx\n",
            (unsigned long long)dDObjSkel2MatrixMultiply43);
        Com_Printf("COREMATHDIGEST fn=MatrixInverse44 digest=%016llx\n",
                   (unsigned long long)dMatrixInverse44);
    }

    /* Quaternion helpers (self-contained battery). */
    {
        uint64_t dQuatMultiply = UINT64_C(0xcbf29ce484222325);
        uint64_t dConvertQuatToMat = UINT64_C(0xcbf29ce484222325);
        uint64_t dQuatRatioEigenTrace = UINT64_C(0xcbf29ce484222325);
        for (int i = 0; i < n4; ++i) {
            vec4_t out;
            QuatMultiply(battery4[i], battery4[(i + 1) % n4], out);
            coduo_engine_collision_digest_bytes_external(out, sizeof(out), &dQuatMultiply);

            float qm[9] = { battery4[i][0], battery4[i][1], battery4[i][2],
                            battery4[i][3], 0, 0, 0, 0, 0 };
            ConvertQuatToMat(qm);
            coduo_engine_collision_digest_bytes_external(
                qm, sizeof(qm), &dConvertQuatToMat);

            float r = (float)QuatRatioEigenTrace(
                battery4[i], battery4[(i + 2) % n4]);
            coduo_engine_collision_digest_bytes_external(
                &r, sizeof(r), &dQuatRatioEigenTrace);
        }
        Com_Printf("COREMATHDIGEST fn=QuatMultiply digest=%016llx\n",
                   (unsigned long long)dQuatMultiply);
        Com_Printf("COREMATHDIGEST fn=ConvertQuatToMat digest=%016llx\n",
                   (unsigned long long)dConvertQuatToMat);
        Com_Printf("COREMATHDIGEST fn=QuatRatioEigenTrace "
                   "digest=%016llx\n",
                   (unsigned long long)dQuatRatioEigenTrace);
    }

    /* Geometry vector helpers (self-contained battery). */
    {
        uint64_t dMakeNormalVectors = UINT64_C(0xcbf29ce484222325);
        uint64_t dProjectPointOntoVector = UINT64_C(0xcbf29ce484222325);
        uint64_t dRotateAroundDirection = UINT64_C(0xcbf29ce484222325);
        uint64_t dPlaneFromPoints = UINT64_C(0xcbf29ce484222325);
        uint64_t dBoxDistSqrdExceeds = UINT64_C(0xcbf29ce484222325);
        uint64_t dTriangleNormal = UINT64_C(0xcbf29ce484222325);
        for (int i = 0; i < n; ++i) {
            vec3_t fwd = { battery[(i + 1) % n][0], battery[(i + 1) % n][1],
                           battery[(i + 1) % n][2] };
            vec3_t rgt, up3, out3;
            MakeNormalVectors(fwd, rgt, up3);
            coduo_engine_collision_digest_bytes_external(rgt, sizeof(rgt), &dMakeNormalVectors);
            coduo_engine_collision_digest_bytes_external(up3, sizeof(up3), &dMakeNormalVectors);

            ProjectPointOntoVector(battery[i], battery[(i + 2) % n],
                                   battery[(i + 3) % n], out3);
            coduo_engine_collision_digest_bytes_external(out3, sizeof(out3),
                                   &dProjectPointOntoVector);

            vec3_t rad[3] = { { fwd[0], fwd[1], fwd[2] },
                              { 0, 0, 0 },
                              { 0, 0, 0 } };
            RotateAroundDirection(rad, (float)(i * 40) - 60.0f);
            coduo_engine_collision_digest_bytes_external(rad, sizeof(rad), &dRotateAroundDirection);

            vec4_t plane;
            qboolean ok = PlaneFromPoints(plane, battery[i],
                                          battery[(i + 1) % n],
                                          battery[(i + 2) % n]);
            coduo_engine_collision_digest_bytes_external(plane, sizeof(plane), &dPlaneFromPoints);
            coduo_engine_collision_digest_bytes_external(&ok, sizeof(ok), &dPlaneFromPoints);

            qboolean ex = BoxDistSqrdExceeds(battery[i], battery[(i + 1) % n],
                                             battery[(i + 2) % n], 0.5f);
            coduo_engine_collision_digest_bytes_external(&ex, sizeof(ex), &dBoxDistSqrdExceeds);

            TriangleNormal(battery[i], battery[(i + 1) % n],
                           battery[(i + 2) % n], out3);
            coduo_engine_collision_digest_bytes_external(out3, sizeof(out3), &dTriangleNormal);
        }
        Com_Printf("COREMATHDIGEST fn=MakeNormalVectors digest=%016llx\n",
                   (unsigned long long)dMakeNormalVectors);
        Com_Printf("COREMATHDIGEST fn=ProjectPointOntoVector digest=%016llx\n",
                   (unsigned long long)dProjectPointOntoVector);
        Com_Printf("COREMATHDIGEST fn=RotateAroundDirection digest=%016llx\n",
                   (unsigned long long)dRotateAroundDirection);
        Com_Printf("COREMATHDIGEST fn=PlaneFromPoints digest=%016llx\n",
                   (unsigned long long)dPlaneFromPoints);
        Com_Printf("COREMATHDIGEST fn=BoxDistSqrdExceeds digest=%016llx\n",
                   (unsigned long long)dBoxDistSqrdExceeds);
        Com_Printf("COREMATHDIGEST fn=TriangleNormal digest=%016llx\n",
                   (unsigned long long)dTriangleNormal);
    }

    /* fsincos-envelope helpers (self-contained battery). The fsincos leaf is
     * the real x87 instruction in both x86 builds, so these verify. */
    {
        static const float degs[] = { 0.0f,   30.0f,  45.0f, 90.0f,
                                      135.0f, 180.0f, 270.0f, -60.0f };
        const int ndg = (int)(sizeof(degs) / sizeof(degs[0]));
        uint64_t dYawVectors = UINT64_C(0xcbf29ce484222325);
        uint64_t dRollToQuaternion = UINT64_C(0xcbf29ce484222325);
        uint64_t dPitchToQuaternion = UINT64_C(0xcbf29ce484222325);
        uint64_t dYawToQuaternion = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorPolar = UINT64_C(0xcbf29ce484222325);
        uint64_t dAnglesToAxis = UINT64_C(0xcbf29ce484222325);
        uint64_t dYawToAxis = UINT64_C(0xcbf29ce484222325);
        for (int i = 0; i < ndg; ++i) {
            vec3_t fwd, rgt, out3;
            vec4_t q;
            axis_t axis;
            YawVectors(degs[i], fwd, rgt);
            coduo_engine_collision_digest_bytes_external(fwd, sizeof(fwd), &dYawVectors);
            coduo_engine_collision_digest_bytes_external(rgt, sizeof(rgt), &dYawVectors);
            RollToQuaternion(degs[i], q);
            coduo_engine_collision_digest_bytes_external(
                q, sizeof(q), &dRollToQuaternion);
            PitchToQuaternion(degs[i], q);
            coduo_engine_collision_digest_bytes_external(
                q, sizeof(q), &dPitchToQuaternion);
            YawToQuaternion(degs[i], q);
            coduo_engine_collision_digest_bytes_external(
                q, sizeof(q), &dYawToQuaternion);
            VectorPolar(out3, (float)(i + 1), degs[i]);
            coduo_engine_collision_digest_bytes_external(out3, sizeof(out3), &dVectorPolar);
            AnglesToAxis(battery[i % n], axis);
            coduo_engine_collision_digest_bytes_external(
                axis, sizeof(axis), &dAnglesToAxis);
            YawToAxis(degs[i], axis);
            coduo_engine_collision_digest_bytes_external(
                axis, sizeof(axis), &dYawToAxis);
        }
        Com_Printf("COREMATHDIGEST fn=YawVectors digest=%016llx\n",
                   (unsigned long long)dYawVectors);
        Com_Printf("COREMATHDIGEST fn=RollToQuaternion digest=%016llx\n",
                   (unsigned long long)dRollToQuaternion);
        Com_Printf("COREMATHDIGEST fn=PitchToQuaternion digest=%016llx\n",
                   (unsigned long long)dPitchToQuaternion);
        Com_Printf("COREMATHDIGEST fn=YawToQuaternion digest=%016llx\n",
                   (unsigned long long)dYawToQuaternion);
        Com_Printf("COREMATHDIGEST fn=VectorPolar digest=%016llx\n",
                   (unsigned long long)dVectorPolar);
        Com_Printf("COREMATHDIGEST fn=AnglesToAxis digest=%016llx\n",
                   (unsigned long long)dAnglesToAxis);
        Com_Printf("COREMATHDIGEST fn=YawToAxis digest=%016llx\n",
                   (unsigned long long)dYawToAxis);
    }

    /* Basic vector ops (VectorMA / VectorCompareEpsilon need the shim; the rest
     * are single-op and verify unchanged). */
    {
        uint64_t dVectorMA = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorCompareEpsilon = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorScale = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorAdd = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorSubtract = UINT64_C(0xcbf29ce484222325);
        uint64_t dVector4Scale = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorInverse = UINT64_C(0xcbf29ce484222325);
        for (int i = 0; i < n; ++i) {
            vec3_t o3;
            _VectorMA(battery[i], battery[(i + 1) % n][0],
                      battery[(i + 2) % n], o3);
            coduo_engine_collision_digest_bytes_external(o3, sizeof(o3), &dVectorMA);
            _VectorScale(battery[i], battery[(i + 1) % n][1], o3);
            coduo_engine_collision_digest_bytes_external(o3, sizeof(o3), &dVectorScale);
            _VectorAdd(battery[i], battery[(i + 1) % n], o3);
            coduo_engine_collision_digest_bytes_external(o3, sizeof(o3), &dVectorAdd);
            _VectorSubtract(battery[i], battery[(i + 1) % n], o3);
            coduo_engine_collision_digest_bytes_external(o3, sizeof(o3), &dVectorSubtract);
            vec3_t inv = { battery[i][0], battery[i][1], battery[i][2] };
            VectorInverse(inv);
            coduo_engine_collision_digest_bytes_external(inv, sizeof(inv), &dVectorInverse);
            vec4_t o4;
            Vector4Scale(battery4[i % n4], battery[(i + 1) % n][2], o4);
            coduo_engine_collision_digest_bytes_external(o4, sizeof(o4), &dVector4Scale);
            for (int j = 0; j < n; ++j) {
                qboolean eq = VectorCompareEpsilon(battery[i], battery[j]);
                coduo_engine_collision_digest_bytes_external(&eq, sizeof(eq), &dVectorCompareEpsilon);
            }
        }
        Com_Printf("COREMATHDIGEST fn=VectorMA digest=%016llx\n",
                   (unsigned long long)dVectorMA);
        Com_Printf("COREMATHDIGEST fn=VectorCompareEpsilon digest=%016llx\n",
                   (unsigned long long)dVectorCompareEpsilon);
        Com_Printf("COREMATHDIGEST fn=VectorScale digest=%016llx\n",
                   (unsigned long long)dVectorScale);
        Com_Printf("COREMATHDIGEST fn=_VectorAdd digest=%016llx\n",
                   (unsigned long long)dVectorAdd);
        Com_Printf("COREMATHDIGEST fn=_VectorSubtract digest=%016llx\n",
                   (unsigned long long)dVectorSubtract);
        Com_Printf("COREMATHDIGEST fn=Vector4Scale digest=%016llx\n",
                   (unsigned long long)dVector4Scale);
        Com_Printf("COREMATHDIGEST fn=VectorInverse digest=%016llx\n",
                   (unsigned long long)dVectorInverse);
    }

    /* Axis<->angle extraction + angle-rotation + sway/gun helpers. */
    {
        uint64_t dAxisToAngles = UINT64_C(0xcbf29ce484222325);
        uint64_t dAxis4ToAngles = UINT64_C(0xcbf29ce484222325);
        uint64_t dAxisToSignedAngles = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorRotateAngles = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorRotateAnglesAroundPoint = UINT64_C(0xcbf29ce484222325);
        uint64_t dQ_SwayRand = UINT64_C(0xcbf29ce484222325);
        uint64_t dgunrandom = UINT64_C(0xcbf29ce484222325);
        uint64_t dPitchForYawOnNormal = UINT64_C(0xcbf29ce484222325);
        for (int i = 0; i < n; ++i) {
            vec3_t ax[3] = { { battery[(i + 1) % n][0], battery[(i + 1) % n][1],
                               battery[(i + 1) % n][2] },
                             { battery[(i + 2) % n][0], battery[(i + 2) % n][1],
                               battery[(i + 2) % n][2] },
                             { battery[(i + 3) % n][0], battery[(i + 3) % n][1],
                               battery[(i + 3) % n][2] } };
            vec3_t angOut, rotOut;
            AxisToAngles(ax, angOut);
            coduo_engine_collision_digest_bytes_external(angOut, sizeof(angOut), &dAxisToAngles);
            AxisToSignedAngles(ax, angOut);
            coduo_engine_collision_digest_bytes_external(angOut, sizeof(angOut),
                                   &dAxisToSignedAngles);
            DObjSkelMat am4;
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 4; ++c) {
                    am4.axis[r][c] = ax[r][c % 3];
                }
            }
            Axis4ToAngles(&am4, angOut);
            coduo_engine_collision_digest_bytes_external(angOut, sizeof(angOut), &dAxis4ToAngles);

            VectorRotateAngles(battery[i], battery[(i + 1) % n], rotOut);
            coduo_engine_collision_digest_bytes_external(rotOut, sizeof(rotOut),
                                   &dVectorRotateAngles);
            VectorRotateAnglesAroundPoint(battery[i], battery[(i + 1) % n],
                                          battery[(i + 2) % n], rotOut);
            coduo_engine_collision_digest_bytes_external(rotOut, sizeof(rotOut),
                                   &dVectorRotateAnglesAroundPoint);

            float sway = (float)Q_SwayRand((float)(i + 1), (float)(i + 2),
                                           (float)(i * 250));
            coduo_engine_collision_digest_bytes_external(&sway, sizeof(sway), &dQ_SwayRand);

            float pfy = (float)PitchForYawOnNormal((float)(i * 30),
                                                   battery[(i + 1) % n]);
            coduo_engine_collision_digest_bytes_external(&pfy, sizeof(pfy), &dPitchForYawOnNormal);
        }
        srand(12345u);
        for (int i = 0; i < 8; ++i) {
            float gx, gy;
            gunrandom(&gx, &gy);
            coduo_engine_collision_digest_bytes_external(&gx, sizeof(gx), &dgunrandom);
            coduo_engine_collision_digest_bytes_external(&gy, sizeof(gy), &dgunrandom);
        }
        Com_Printf("COREMATHDIGEST fn=AxisToAngles digest=%016llx\n",
                   (unsigned long long)dAxisToAngles);
        Com_Printf("COREMATHDIGEST fn=Axis4ToAngles digest=%016llx\n",
                   (unsigned long long)dAxis4ToAngles);
        Com_Printf("COREMATHDIGEST fn=AxisToSignedAngles digest=%016llx\n",
                   (unsigned long long)dAxisToSignedAngles);
        Com_Printf("COREMATHDIGEST fn=VectorRotateAngles digest=%016llx\n",
                   (unsigned long long)dVectorRotateAngles);
        Com_Printf("COREMATHDIGEST fn=VectorRotateAnglesAroundPoint "
                   "digest=%016llx\n",
                   (unsigned long long)dVectorRotateAnglesAroundPoint);
        Com_Printf("COREMATHDIGEST fn=Q_SwayRand digest=%016llx\n",
                   (unsigned long long)dQ_SwayRand);
        Com_Printf("COREMATHDIGEST fn=gunrandom digest=%016llx\n",
                   (unsigned long long)dgunrandom);
        Com_Printf("COREMATHDIGEST fn=PitchForYawOnNormal digest=%016llx\n",
                   (unsigned long long)dPitchForYawOnNormal);
    }

    /* Normalize-fast / perpendicular / centered-RNG helpers. */
    {
        uint64_t dVectorNormalizeFast = UINT64_C(0xcbf29ce484222325);
        uint64_t dPerpendicularVector = UINT64_C(0xcbf29ce484222325);
        uint64_t dCrossProductUp = UINT64_C(0xcbf29ce484222325);
        uint64_t dQ_crandom = UINT64_C(0xcbf29ce484222325);
        for (int i = 0; i < n; ++i) {
            vec3_t nf = { battery[i][0], battery[i][1], battery[i][2] };
            VectorNormalizeFast(nf);
            coduo_engine_collision_digest_bytes_external(nf, sizeof(nf), &dVectorNormalizeFast);
            vec3_t pv;
            PerpendicularVector(pv, battery[(i + 1) % n]);
            coduo_engine_collision_digest_bytes_external(pv, sizeof(pv), &dPerpendicularVector);
            CrossProductUp(battery[i], pv);
            coduo_engine_collision_digest_bytes_external(
                pv, sizeof(pv), &dCrossProductUp);
        }
        int32_t seed = 0x2545f491;
        for (int i = 0; i < 12; ++i) {
            float c = (float)Q_crandom(&seed);
            coduo_engine_collision_digest_bytes_external(&c, sizeof(c), &dQ_crandom);
        }
        Com_Printf("COREMATHDIGEST fn=VectorNormalizeFast digest=%016llx\n",
                   (unsigned long long)dVectorNormalizeFast);
        Com_Printf("COREMATHDIGEST fn=PerpendicularVector digest=%016llx\n",
                   (unsigned long long)dPerpendicularVector);
        Com_Printf("COREMATHDIGEST fn=CrossProductUp digest=%016llx\n",
                   (unsigned long long)dCrossProductUp);
        Com_Printf("COREMATHDIGEST fn=Q_crandom digest=%016llx\n",
                   (unsigned long long)dQ_crandom);
    }

    /* Color-pack / rounding / dir-encode helpers. */
    {
        uint64_t dColorBytes3 = UINT64_C(0xcbf29ce484222325);
        uint64_t dColorBytes4 = UINT64_C(0xcbf29ce484222325);
        uint64_t dQ_rint = UINT64_C(0xcbf29ce484222325);
        uint64_t dDirToByte = UINT64_C(0xcbf29ce484222325);
        static const float cols[] = { 0.0f,  0.25f,     0.5f, 0.75f,
                                      1.0f,  0.333333f, 0.9999f, 0.501960f };
        const int nc = (int)(sizeof(cols) / sizeof(cols[0]));
        for (int i = 0; i < nc; ++i) {
            uint32_t p3 =
                ColorBytes3(cols[i], cols[(i + 1) % nc], cols[(i + 2) % nc]);
            coduo_engine_collision_digest_bytes_external(&p3, sizeof(p3), &dColorBytes3);
            uint32_t p4 = ColorBytes4(cols[i], cols[(i + 1) % nc],
                                      cols[(i + 2) % nc], cols[(i + 3) % nc]);
            coduo_engine_collision_digest_bytes_external(&p4, sizeof(p4), &dColorBytes4);
            float ri = Q_rint((float)(i * 64) - 200.5f);
            coduo_engine_collision_digest_bytes_external(&ri, sizeof(ri), &dQ_rint);
        }
        for (int i = 0; i < n; ++i) {
            uint8_t b = DirToByte(battery[i]);
            coduo_engine_collision_digest_bytes_external(&b, sizeof(b), &dDirToByte);
        }
        Com_Printf("COREMATHDIGEST fn=ColorBytes3 digest=%016llx\n",
                   (unsigned long long)dColorBytes3);
        Com_Printf("COREMATHDIGEST fn=ColorBytes4 digest=%016llx\n",
                   (unsigned long long)dColorBytes4);
        Com_Printf("COREMATHDIGEST fn=Q_rint digest=%016llx\n",
                   (unsigned long long)dQ_rint);
        Com_Printf("COREMATHDIGEST fn=DirToByte digest=%016llx\n",
                   (unsigned long long)dDirToByte);
    }

    /* Geometry angle / round helpers (self-contained battery). */
    {
        static const float angles[] = {
            0.0f,     45.0f,   90.0f,   179.9f,  180.0f,   180.1f,
            270.0f,   359.9f,  360.0f,  400.0f,  -45.0f,   -180.0f,
            -400.0f,  720.5f,  0.03125f, 123.4567f,
        };
        const int na = (int)(sizeof(angles) / sizeof(angles[0]));
        uint64_t dAngleMod = UINT64_C(0xcbf29ce484222325);
        uint64_t dAngleNormalize360 = UINT64_C(0xcbf29ce484222325);
        uint64_t dAngleNormalize180 = UINT64_C(0xcbf29ce484222325);
        uint64_t dAngleNormalize360Accurate = UINT64_C(0xcbf29ce484222325);
        uint64_t dAngleNormalize180Accurate = UINT64_C(0xcbf29ce484222325);
        uint64_t dAngleSubtract = UINT64_C(0xcbf29ce484222325);
        uint64_t dAngleDelta = UINT64_C(0xcbf29ce484222325);
        uint64_t dLerpAngle = UINT64_C(0xcbf29ce484222325);
        uint64_t dAnglesSubtract = UINT64_C(0xcbf29ce484222325);
        uint64_t dRadiusFromBounds = UINT64_C(0xcbf29ce484222325);
        uint64_t dFloatRoundNearest = UINT64_C(0xcbf29ce484222325);
        uint64_t dVectorSnap = UINT64_C(0xcbf29ce484222325);

        for (int i = 0; i < na; ++i) {
            float r;
            r = (float)AngleMod(angles[i]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dAngleMod);
            r = (float)AngleNormalize360(angles[i]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dAngleNormalize360);
            r = (float)AngleNormalize180(angles[i]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dAngleNormalize180);
            r = (float)AngleNormalize360Accurate(angles[i]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dAngleNormalize360Accurate);
            r = (float)AngleNormalize180Accurate(angles[i]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dAngleNormalize180Accurate);
            r = FloatRoundNearest(angles[i]);
            coduo_engine_collision_digest_bytes_external(
                &r, sizeof(r), &dFloatRoundNearest);
            for (int j = 0; j < na; ++j) {
                r = (float)AngleSubtract(angles[i], angles[j]);
                coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dAngleSubtract);
                r = (float)AngleDelta(angles[i], angles[j]);
                coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dAngleDelta);
                r = (float)LerpAngle(angles[i], angles[j], 0.375f);
                coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dLerpAngle);
            }
        }
        for (int i = 0; i < n; ++i) {
            vec3_t a2, out, snap;
            AnglesSubtract(battery[i], battery[(i + 1) % n], out);
            coduo_engine_collision_digest_bytes_external(out, sizeof(out), &dAnglesSubtract);
            float r = RadiusFromBounds(battery[i], battery[(i + 3) % n]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dRadiusFromBounds);
            (void)a2;
            snap[0] = battery[i][0];
            snap[1] = battery[i][1];
            snap[2] = battery[i][2];
            VectorSnap(snap);
            coduo_engine_collision_digest_bytes_external(snap, sizeof(snap), &dVectorSnap);
        }
        Com_Printf("COREMATHDIGEST fn=AngleMod digest=%016llx\n",
                   (unsigned long long)dAngleMod);
        Com_Printf("COREMATHDIGEST fn=AngleNormalize360 digest=%016llx\n",
                   (unsigned long long)dAngleNormalize360);
        Com_Printf("COREMATHDIGEST fn=AngleNormalize180 digest=%016llx\n",
                   (unsigned long long)dAngleNormalize180);
        Com_Printf("COREMATHDIGEST fn=AngleNormalize360Accurate digest=%016llx\n",
                   (unsigned long long)dAngleNormalize360Accurate);
        Com_Printf("COREMATHDIGEST fn=AngleNormalize180Accurate digest=%016llx\n",
                   (unsigned long long)dAngleNormalize180Accurate);
        Com_Printf("COREMATHDIGEST fn=AngleSubtract digest=%016llx\n",
                   (unsigned long long)dAngleSubtract);
        Com_Printf("COREMATHDIGEST fn=AngleDelta digest=%016llx\n",
                   (unsigned long long)dAngleDelta);
        Com_Printf("COREMATHDIGEST fn=LerpAngle digest=%016llx\n",
                   (unsigned long long)dLerpAngle);
        Com_Printf("COREMATHDIGEST fn=AnglesSubtract digest=%016llx\n",
                   (unsigned long long)dAnglesSubtract);
        Com_Printf("COREMATHDIGEST fn=RadiusFromBounds digest=%016llx\n",
                   (unsigned long long)dRadiusFromBounds);
        Com_Printf("COREMATHDIGEST fn=FloatRoundNearest digest=%016llx\n",
                   (unsigned long long)dFloatRoundNearest);
        Com_Printf("COREMATHDIGEST fn=VectorSnap digest=%016llx\n",
                   (unsigned long long)dVectorSnap);
    }

    /* libm-envelope + RNG helpers. flrand advances sharedRandSeed, so seed
     * it deterministically first (both builds then produce the same sequence). */
    {
        uint64_t dflrand = UINT64_C(0xcbf29ce484222325);
        uint64_t dRoundFloat = UINT64_C(0xcbf29ce484222325);
        uint64_t dNormalToLatLong = UINT64_C(0xcbf29ce484222325);
        static const int32_t decimalsList[] = { 0, 1, 2, 3, 4, -1, -2 };
        const int nd = (int)(sizeof(decimalsList) / sizeof(decimalsList[0]));
        /* unit-ish normals so acos(normal[2]) stays in domain. */
        static const vec3_t normals[] = {
            {  0.0f,      0.0f,      1.0f },
            {  0.0f,      0.0f,     -1.0f },
            {  1.0f,      0.0f,      0.0f },
            {  0.6f,      0.8f,      0.0f },
            {  0.5f,     -0.5f,      0.70710677f },
            { -0.267261f, 0.534522f, 0.801784f },
            {  0.0f,      1.0f,      0.0f },
        };
        const int nn = (int)(sizeof(normals) / sizeof(normals[0]));

        Rand_Init(0x1234abcdU);
        for (int i = 0; i < ns; ++i) {
            float r = (float)flrand(-scalars[i], scalars[i]);
            coduo_engine_collision_digest_bytes_external(&r, sizeof(r), &dflrand);
        }
        for (int i = 0; i < ns; ++i) {
            for (int d = 0; d < nd; ++d) {
                float r = RoundFloat(scalars[i], decimalsList[d]);
                coduo_engine_collision_digest_bytes_external(
                    &r, sizeof(r), &dRoundFloat);
            }
        }
        for (int i = 0; i < nn; ++i) {
            uint8_t enc[2];
            NormalToLatLong(normals[i], enc);
            coduo_engine_collision_digest_bytes_external(enc, sizeof(enc), &dNormalToLatLong);
        }
        Com_Printf("COREMATHDIGEST fn=flrand digest=%016llx\n",
                   (unsigned long long)dflrand);
        Com_Printf("COREMATHDIGEST fn=RoundFloat digest=%016llx\n",
                   (unsigned long long)dRoundFloat);
        Com_Printf("COREMATHDIGEST fn=NormalToLatLong digest=%016llx\n",
                   (unsigned long long)dNormalToLatLong);
    }
}

#endif /* CODUO_COLLISION_DIGEST */

/* Keep this a non-empty translation unit when the digest is compiled out
 * (an empty TU is forbidden by -Werror=pedantic). */
typedef int cm_collision_digest_translation_unit_not_empty;
