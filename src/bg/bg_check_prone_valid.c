#include "bg_pmove.h"
#include "bg_prone_debug.h"
#include "bg_prone_services.h"

#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

// Sources: uo_cgame_mp_x86.dll 0x30006e10..0x3000812a;
//          uo_game_mp_x86.dll  0x20006bf0..0x20007ed7;
//          game.mp.uo.i386.so  RVA 0x2162c..0x22ff2.
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30006e10_3000812a.mcode
//
// BG_CheckProneValid validates a proposed prone body against two caller-supplied
// trace callbacks. The same-module cgame_mp PPC symbol identifies the routine;
// its call graph (PM_VerifyPronePosition, PM_UpdatePronePitch, BG_CheckProne),
// 17-argument shape, three float outputs, and prone constants prove the mapping.
//
// The Windows cgame and game modules retain the same computation and control
// flow apart from their diagnostic-call boundary. Linux retains the same
// decisions with the localized unoptimized spill graphs identified below.
// Target service headers keep the module-owned diagnostic calls and color
// objects outside this shared original body.

#define BG_PRONE_CONTENTS_DEFAULT ((uint32_t)0x00810031u)
#define BG_PRONE_CONTENTS_ALT ((uint32_t)0x00820011u)
#define BG_PRONE_BLOCKING_CONTENTS CONTENTS_WATER

#define BG_PRONE_TRACE_RADIUS 6.0f
#define BG_PRONE_REVERSE_YAW 180.0f
#define BG_PRONE_INITIAL_PROBE_HEIGHT 10.0f
#define BG_PRONE_CLEARANCE_HEIGHT 8.0f
#define BG_PRONE_FORWARD_CLEARANCE 38.0f
#define BG_PRONE_FORWARD_PROBE 24.0f
#define BG_PRONE_CLEARANCE_MARGIN 2.0f
#define BG_PRONE_RAISED_RETRY_HEIGHT 22.0f
#define BG_PRONE_CONNECT_RETRY_HEIGHT 18.0f
#define BG_PRONE_GROUND_TRACE_SCALE 2.5f
#define BG_PRONE_MAX_SLOPE_RATIO (-0.75f)
#define BG_PRONE_FRONT_DROP_SCALE 2.0f
#define BG_PRONE_REAR_TRACE_SCALE 1.5f
#define BG_PRONE_POINT_TRACE_HEIGHT 5.0f
#define BG_PRONE_MIDPOINT_SCALE 0.5f
#define BG_PRONE_TRACE_FULL_FRACTION 1.0f
#define BG_PRONE_GROUND_NORMAL_MIN_Z 0.7f
#define BG_PRONE_MAX_GROUND_DELTA 24.0
#define BG_PRONE_MIN_PITCH_DELTA (-45.0f)
#define BG_PRONE_MAX_PITCH_DELTA 70.0f

/* The original expands every diagnostic-line gate at its use site. The target
 * adapter performs that reload while retaining each module's draw API. */
#define BG_PRONE_DEBUG_LINE(lineStart, lineEnd, lineColor) bg_compat_prone_debug_line((lineStart), (lineEnd), (lineColor))

int BG_CheckProneValid(int clientNum, const vec3_t origin, float radius, float height, float yaw, float *groundOffset, float *pitchDown,
                       float *pitchUp, qboolean skipInitialTrace, qboolean allowFallback, const vec3_t groundNormal,
                       pm_trace_fn_t traceFunc, pm_trace_fn_t traceDownFunc, qboolean useAltContentMask, float proneLength,
                       qboolean checkForwardClearance, pm_entity_type_fn_t entityTypeFunc)
{
    trace_t trace;
    vec3_t mins;
    vec3_t maxs;
    vec3_t start;
    vec3_t end;
    vec3_t angles;
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    vec3_t frontHit;
    vec3_t middleHit;
    vec3_t rearHit;
    vec3_t delta;
    float frontClearance;
    qboolean blockedForward = qfalse;
    uint32_t contentMask;

    /* 0x30006e16..0x30006ea1: show the complete proposed prone bounds. */
    if (bg_compat_prone_debug_enabled() != qfalse) {
        vec3_t debugMins;
        vec3_t debugMaxs;

        debugMins[0] = origin[0] - radius;
        debugMins[1] = origin[1] - radius;
        debugMins[2] = origin[2];
        debugMaxs[0] = origin[0] + radius;
        debugMaxs[1] = origin[1] + radius;
        debugMaxs[2] = origin[2] + height;
        bg_compat_prone_debug_box(debugMins, debugMaxs, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }

    /* NEG/SBB/AND/ADD at 0x30006ead..0x30006eb7 selects exactly these masks. */
    contentMask = useAltContentMask ? BG_PRONE_CONTENTS_ALT : BG_PRONE_CONTENTS_DEFAULT;

    /* 0x30006ec3..0x30006fe0: optional initial occupancy/entity probes. */
    if (skipInitialTrace == qfalse) {
        mins[0] = -radius;
        mins[1] = -radius;
        mins[2] = 0.0f;
        maxs[0] = radius;
        maxs[1] = radius;
        maxs[2] = height;
        start[0] = origin[0];
        start[1] = origin[1];
        start[2] = origin[2];
        end[0] = origin[0];
        end[1] = origin[1];
        end[2] = origin[2] + BG_PRONE_INITIAL_PROBE_HEIGHT;
        traceDownFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);

        if ((((uint32_t)trace.contents & BG_PRONE_BLOCKING_CONTENTS) != 0u && trace.startsolid != 0) || trace.allsolid != 0) {
            return 0;
        }

        if (entityTypeFunc != NULL) {
            end[0] = origin[0];
            end[1] = origin[1];
            end[2] = origin[2] - BG_PRONE_INITIAL_PROBE_HEIGHT;
            traceDownFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
            if (entityTypeFunc(trace.entityNum) == ET_VEHICLE) {
                return 0;
            }
        }
    }

    /* 0x30006fe0..0x30007004: an explicitly supplied fallback normal must be a
     * walkable plane. Ordered x87 comparison: NaN does not take this return. */
    if (allowFallback != qfalse && groundNormal != NULL && groundNormal[2] < BG_PRONE_GROUND_NORMAL_MIN_Z) {
        return 0;
    }

    mins[0] = -BG_PRONE_TRACE_RADIUS;
    mins[1] = -BG_PRONE_TRACE_RADIUS;
    mins[2] = -BG_PRONE_TRACE_RADIUS;
    maxs[0] = BG_PRONE_TRACE_RADIUS;
    maxs[1] = BG_PRONE_TRACE_RADIUS;
    maxs[2] = BG_PRONE_TRACE_RADIUS;
    angles[0] = 0.0f;
    angles[1] = yaw - BG_PRONE_REVERSE_YAW;
    angles[2] = 0.0f;
    AngleVectors(angles, forward, right, up);
    height -= BG_PRONE_TRACE_RADIUS;

    /* 0x30007097..0x30007178: optional unobstructed clearance behind the prone
     * facing direction. */
    if (checkForwardClearance != qfalse) {
        start[0] = origin[0];
        start[1] = origin[1];
        start[2] = origin[2] + BG_PRONE_CLEARANCE_HEIGHT;
        end[0] = (float)((long double)start[0] - (long double)forward[0] * BG_PRONE_FORWARD_CLEARANCE);
        end[1] = (float)((long double)start[1] - (long double)forward[1] * BG_PRONE_FORWARD_CLEARANCE);
        end[2] = (float)((long double)start[2] - (long double)forward[2] * BG_PRONE_FORWARD_CLEARANCE);
        traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
        if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION) {
            BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
            return 0;
        }
    }

    /* 0x30007179..0x3000746e: forward body trace and the raised-end retry used
     * when a fallback is allowed. */
    start[0] = origin[0];
    start[1] = origin[1];
    start[2] = origin[2] + height;
    /* Windows 0x300071a1..0x300071ad rounds proneLength - 6 to a binary32
     * slot once. Linux retains the unrounded value in the x87 domain. */
#if defined(WINDOWS_BEHAVIOR)
    const float proneReach = (float)((long double)proneLength - BG_PRONE_TRACE_RADIUS);
#else
    const long double proneReach = (long double)proneLength - BG_PRONE_TRACE_RADIUS;
#endif
    end[0] = (float)((long double)start[0] + (long double)proneReach * (long double)forward[0]);
    end[1] = (float)((long double)start[1] + (long double)proneReach * (long double)forward[1]);
    end[2] = (float)((long double)start[2] + (long double)proneReach * (long double)forward[2]);
    traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);

    if (bg_compat_prone_debug_enabled() != qfalse) {
        bg_compat_prone_debug_circle(start, BG_PRONE_TRACE_RADIUS, right, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }
    if (bg_compat_prone_debug_enabled() != qfalse) {
        bg_compat_prone_debug_circle(start, BG_PRONE_TRACE_RADIUS, up, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }

    if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION) {
#if defined(WINDOWS_BEHAVIOR)
        float minimumForwardClearance;
#else
        long double minimumForwardClearance;
#endif

        if (allowFallback == qfalse) {
            BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
            return 0;
        }

        blockedForward = qtrue;
        frontClearance = (float)((long double)proneReach * (long double)trace.fraction + BG_PRONE_TRACE_RADIUS);
        if ((long double)frontClearance < (long double)radius + BG_PRONE_CLEARANCE_MARGIN) {
            BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
            return 0;
        }

        minimumForwardClearance = (long double)height * BG_PRONE_GROUND_NORMAL_MIN_Z + BG_PRONE_FORWARD_PROBE;
        if ((long double)frontClearance < (long double)minimumForwardClearance) {
            float traceDistance;

            BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
            blockedForward = qfalse;
            end[2] = (float)((long double)end[2] + BG_PRONE_RAISED_RETRY_HEIGHT);
            delta[0] = end[0] - start[0];
            delta[1] = end[1] - start[1];
            delta[2] = end[2] - start[2];
            traceDistance = VectorNormalize2(delta, forward);
            traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);

            if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION) {
                blockedForward = qtrue;
                frontClearance = (float)((long double)trace.fraction * (long double)traceDistance + BG_PRONE_TRACE_RADIUS);
                if ((long double)frontClearance < (long double)minimumForwardClearance) {
                    BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
                    return 0;
                }
                BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_YELLOW);
            } else {
                BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_GREEN);
                frontClearance = proneLength;
            }
        } else {
            BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_YELLOW);
        }
    } else {
        BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_GREEN);
        frontClearance = proneLength;
    }

    frontHit[0] = trace.endpos[0];
    frontHit[1] = trace.endpos[1];
    frontHit[2] = trace.endpos[2];

    /* 0x30007472..0x3000767a: middle downward sample, 24 units along the
     * facing direction. */
    end[0] = (float)((long double)origin[0] + (long double)forward[0] * BG_PRONE_FORWARD_PROBE);
    end[1] = (float)((long double)origin[1] + (long double)forward[1] * BG_PRONE_FORWARD_PROBE);
#if defined(LINUX_BEHAVIOR)
    /* Linux 0x21fe6..0x22004 stores origin+forward*24 before adding height. */
    end[2] = (float)((long double)origin[2] + (long double)forward[2] * BG_PRONE_FORWARD_PROBE);
    end[2] = (float)((long double)end[2] + (long double)height);
#else
    end[2] = (float)((long double)origin[2] + (long double)forward[2] * BG_PRONE_FORWARD_PROBE + (long double)height);
#endif
    start[0] = end[0];
    start[1] = end[1];
    start[2] = end[2];
    /* Windows 0x30007513 stores this depth as binary32. Linux retains the same
     * value in the x87 domain through the trace and slope calculation. */
#if defined(WINDOWS_BEHAVIOR)
    const float groundTraceDepth =
        (float)(((long double)radius * BG_PRONE_GROUND_TRACE_SCALE + (long double)height) - BG_PRONE_TRACE_RADIUS);
#else
    const long double groundTraceDepth = ((long double)radius * BG_PRONE_GROUND_TRACE_SCALE + (long double)height) - BG_PRONE_TRACE_RADIUS;
#endif
    end[2] = (float)((long double)start[2] - (long double)groundTraceDepth);
    traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
    if (trace.fraction == BG_PRONE_TRACE_FULL_FRACTION) {
        BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
        goto fallback_failure;
    }
    if (trace.normal[2] < BG_PRONE_GROUND_NORMAL_MIN_Z) {
        return 0;
    }

    BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_GREEN);
    middleHit[0] = trace.endpos[0];
    middleHit[1] = trace.endpos[1];
    middleHit[2] = trace.endpos[2];

    /* 0x3000767a..0x30007950: when the forward trace was shortened, reject an
     * excessive drop and try the normalized, raised connecting segment. */
    if (blockedForward != qfalse) {
        float slopeDrop = (float)((long double)groundTraceDepth * (long double)trace.fraction + BG_PRONE_TRACE_RADIUS);

        if ((long double)frontClearance - (long double)slopeDrop < (long double)slopeDrop * BG_PRONE_MAX_SLOPE_RATIO) {
            BG_PRONE_DEBUG_LINE(frontHit, middleHit, BG_PRONE_DEBUG_RED);
            goto fallback_failure;
        }

        BG_PRONE_DEBUG_LINE(frontHit, middleHit, BG_PRONE_DEBUG_MEDIUM_CYAN);
#if defined(LINUX_BEHAVIOR)
        /* Linux 0x22236..0x222ce stores every subtraction and subsequent
         * product-add separately, including the final Z-radius add. */
        delta[0] = (float)((long double)frontHit[0] - middleHit[0]);
        delta[1] = (float)((long double)frontHit[1] - middleHit[1]);
        delta[2] = (float)((long double)frontHit[2] - middleHit[2]);
        delta[0] = (float)((long double)delta[0] + (long double)forward[0] * BG_PRONE_TRACE_RADIUS);
        delta[1] = (float)((long double)delta[1] + (long double)forward[1] * BG_PRONE_TRACE_RADIUS);
        delta[2] = (float)((long double)delta[2] + (long double)forward[2] * BG_PRONE_TRACE_RADIUS);
        delta[2] = (float)((long double)delta[2] + BG_PRONE_TRACE_RADIUS);
#else
        delta[0] = (float)(((long double)frontHit[0] - middleHit[0]) + (long double)forward[0] * BG_PRONE_TRACE_RADIUS);
        delta[1] = (float)(((long double)frontHit[1] - middleHit[1]) + (long double)forward[1] * BG_PRONE_TRACE_RADIUS);
        /* Windows 0x30007735 stores only the Z difference before the two
         * following additions; X and Y stay in x87. */
        const float deltaZ = (float)((long double)frontHit[2] - middleHit[2]);
        delta[2] = (float)((long double)deltaZ + (long double)forward[2] * BG_PRONE_TRACE_RADIUS + BG_PRONE_TRACE_RADIUS);
#endif
        VectorNormalize(delta);

#if defined(LINUX_BEHAVIOR)
        /* Linux 0x222e4..0x2238f stores all three connector coordinates before
         * reloading X and Y for the midpoint averages. */
        end[0] = (float)((long double)start[0] + ((long double)proneReach - BG_PRONE_FORWARD_PROBE) * (long double)delta[0]);
        end[1] = (float)((long double)start[1] + ((long double)proneReach - BG_PRONE_FORWARD_PROBE) * (long double)delta[1]);
        end[2] = (float)((long double)start[2] + ((long double)proneReach - BG_PRONE_FORWARD_PROBE) * (long double)delta[2]);
        end[0] = (float)(((long double)proneReach * forward[0] + (long double)origin[0] + end[0]) * BG_PRONE_MIDPOINT_SCALE);
        end[1] = (float)(((long double)proneReach * forward[1] + (long double)origin[1] + end[1]) * BG_PRONE_MIDPOINT_SCALE);
#else
        /* Windows 0x300077cf stores only the Y connector term before the outer
         * sum; the X connector term remains in x87. */
        const float connectMid1 = (float)(((long double)proneReach - BG_PRONE_FORWARD_PROBE) * delta[1] + start[1]);
        end[2] = (float)(start[2] + ((long double)proneReach - BG_PRONE_FORWARD_PROBE) * delta[2]);
        end[0] = (float)(((long double)proneReach * forward[0] + origin[0] +
                          (((long double)proneReach - BG_PRONE_FORWARD_PROBE) * delta[0] + start[0])) *
                         BG_PRONE_MIDPOINT_SCALE);
        end[1] = (float)(((long double)proneReach * forward[1] + origin[1] + connectMid1) * BG_PRONE_MIDPOINT_SCALE);
#endif

        traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
        if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION) {
            BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
            start[0] = trace.endpos[0];
            start[1] = trace.endpos[1];
            start[2] = (float)((long double)trace.endpos[2] + BG_PRONE_CONNECT_RETRY_HEIGHT);
            end[2] = (float)((long double)end[2] + BG_PRONE_CONNECT_RETRY_HEIGHT);
            traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
            if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION) {
                BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
                goto fallback_failure;
            }
        }

        BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_GREEN);
        frontHit[0] = trace.endpos[0];
        frontHit[1] = trace.endpos[1];
        frontHit[2] = trace.endpos[2];
    }

    /* 0x30007950..0x30007a3d: sample the ground below the forward hit. */
    start[0] = frontHit[0];
    start[1] = frontHit[1];
    start[2] = frontHit[2];
    end[0] = frontHit[0];
    end[1] = frontHit[1];
#if defined(LINUX_BEHAVIOR)
    /* Linux 0x226d8..0x22722 forms the doubled drop as difference+difference;
     * Windows uses one multiply by 2.0f. */
    end[2] = (float)((long double)frontHit[2] -
                     (((long double)frontHit[2] - middleHit[2]) + ((long double)frontHit[2] - middleHit[2]) + radius));
#else
    end[2] = (float)((long double)frontHit[2] - (((long double)frontHit[2] - middleHit[2]) * BG_PRONE_FRONT_DROP_SCALE + radius));
#endif
    traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
    if (trace.fraction == BG_PRONE_TRACE_FULL_FRACTION) {
        BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
        goto fallback_failure;
    }
    if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION && (((uint32_t)trace.contents & BG_PRONE_BLOCKING_CONTENTS) != 0u)) {
        goto fallback_failure;
    }
    if (trace.normal[2] < BG_PRONE_GROUND_NORMAL_MIN_Z) {
        return 0;
    }
    BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_GREEN);
    frontHit[0] = trace.endpos[0];
    frontHit[1] = trace.endpos[1];
    frontHit[2] = trace.endpos[2];

    /* 0x30007a3d..0x30007bce: rear/center ground sample and height-delta gate. */
    start[0] = origin[0];
    start[1] = origin[1];
    start[2] = origin[2] + height;
    end[0] = origin[0];
    end[1] = origin[1];
    end[2] = (float)((long double)origin[2] - (long double)radius * BG_PRONE_REAR_TRACE_SCALE);
    traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
    if (trace.fraction == BG_PRONE_TRACE_FULL_FRACTION) {
        BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_RED);
        goto fallback_failure;
    }
    if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION && (((uint32_t)trace.contents & BG_PRONE_BLOCKING_CONTENTS) != 0u)) {
        goto fallback_failure;
    }
    if (trace.normal[2] < BG_PRONE_GROUND_NORMAL_MIN_Z) {
        return 0;
    }
    BG_PRONE_DEBUG_LINE(start, trace.endpos, BG_PRONE_DEBUG_GREEN);
    rearHit[0] = trace.endpos[0];
    rearHit[1] = trace.endpos[1];
    rearHit[2] = trace.endpos[2];

    if (fabsl(((long double)rearHit[2] - (long double)origin[2]) - BG_PRONE_TRACE_RADIUS) > (long double)BG_PRONE_MAX_GROUND_DELTA) {
        BG_PRONE_DEBUG_LINE(origin, rearHit, BG_PRONE_DEBUG_RED);
        goto fallback_failure;
    }

    /* 0x30007bce..0x30007c64: reject excessive pitch change between the two
     * ground segments. The machine code checks the lower bound first. */
    delta[0] = middleHit[0] - rearHit[0];
    delta[1] = middleHit[1] - rearHit[1];
    delta[2] = middleHit[2] - rearHit[2];
    {
        float rearPitch = vectopitch(delta);
        float frontPitch;
        float pitchDelta;

        delta[0] = frontHit[0] - middleHit[0];
        delta[1] = frontHit[1] - middleHit[1];
        delta[2] = frontHit[2] - middleHit[2];
        frontPitch = vectopitch(delta);
        pitchDelta = AngleSubtract(frontPitch, rearPitch);
        if (pitchDelta < BG_PRONE_MIN_PITCH_DELTA || pitchDelta > BG_PRONE_MAX_PITCH_DELTA) {
            BG_PRONE_DEBUG_LINE(rearHit, middleHit, BG_PRONE_DEBUG_MAGENTA);
            BG_PRONE_DEBUG_LINE(middleHit, frontHit, BG_PRONE_DEBUG_MAGENTA);
            goto fallback_failure;
        }
    }

    /* 0x30007c64..0x30007e04: point traces between the three samples, five
     * units above the ground. */
#if defined(LINUX_BEHAVIOR)
    /* Linux 0x229ef..0x22a08 materializes the three point-trace minima by
     * toggling the sign bit of positive zero. Windows stores positive zero. */
    mins[0] = -0.0f;
    mins[1] = -0.0f;
    mins[2] = -0.0f;
#else
    mins[0] = 0.0f;
    mins[1] = 0.0f;
    mins[2] = 0.0f;
#endif
    maxs[0] = 0.0f;
    maxs[1] = 0.0f;
    maxs[2] = 0.0f;
    start[0] = rearHit[0];
    start[1] = rearHit[1];
    start[2] = rearHit[2] + BG_PRONE_POINT_TRACE_HEIGHT;
    end[0] = middleHit[0];
    end[1] = middleHit[1];
    end[2] = middleHit[2] + BG_PRONE_POINT_TRACE_HEIGHT;
    traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
    if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION) {
        BG_PRONE_DEBUG_LINE(start, end, BG_PRONE_DEBUG_RED);
        goto fallback_failure;
    }

    BG_PRONE_DEBUG_LINE(start, end, BG_PRONE_DEBUG_GREEN);
    start[0] = end[0];
    start[1] = end[1];
    start[2] = end[2];
    end[0] = frontHit[0];
    end[1] = frontHit[1];
    end[2] = frontHit[2] + BG_PRONE_POINT_TRACE_HEIGHT;
    traceFunc(&trace, start, mins, maxs, end, clientNum, (int32_t)contentMask);
    if (trace.fraction < BG_PRONE_TRACE_FULL_FRACTION) {
        BG_PRONE_DEBUG_LINE(start, end, BG_PRONE_DEBUG_RED);
        goto fallback_failure;
    }

    BG_PRONE_DEBUG_LINE(start, end, BG_PRONE_DEBUG_GREEN);

    /* 0x30007e04..0x30007fb8: successful diagnostic geometry. Each circle gate
     * reloads the module-owned debug cvar independently in the original. */
    if (bg_compat_prone_debug_enabled() != qfalse) {
        bg_compat_prone_debug_circle(rearHit, BG_PRONE_TRACE_RADIUS, right, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }
    if (bg_compat_prone_debug_enabled() != qfalse) {
        bg_compat_prone_debug_circle(rearHit, BG_PRONE_TRACE_RADIUS, up, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }
    if (bg_compat_prone_debug_enabled() != qfalse) {
        bg_compat_prone_debug_circle(middleHit, BG_PRONE_TRACE_RADIUS, right, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }
    if (bg_compat_prone_debug_enabled() != qfalse) {
        bg_compat_prone_debug_circle(middleHit, BG_PRONE_TRACE_RADIUS, up, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }
    if (bg_compat_prone_debug_enabled() != qfalse) {
        bg_compat_prone_debug_circle(frontHit, BG_PRONE_TRACE_RADIUS, right, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }
    if (bg_compat_prone_debug_enabled() != qfalse) {
        bg_compat_prone_debug_circle(frontHit, BG_PRONE_TRACE_RADIUS, up, BG_PRONE_DEBUG_MEDIUM_CYAN);
    }
    BG_PRONE_DEBUG_LINE(rearHit, middleHit, BG_PRONE_DEBUG_CYAN);
    BG_PRONE_DEBUG_LINE(middleHit, frontHit, BG_PRONE_DEBUG_CYAN);

    /* 0x30007fb8..0x30008079: optional successful outputs. */
    if (groundOffset != NULL) {
        *groundOffset = (float)(((long double)rearHit[2] - (long double)origin[2]) - BG_PRONE_TRACE_RADIUS);
    }
    if (pitchDown != NULL) {
        delta[0] = rearHit[0] - middleHit[0];
        delta[1] = rearHit[1] - middleHit[1];
        delta[2] = rearHit[2] - middleHit[2];
        *pitchDown = AngleNormalize180(vectopitch(delta));
    }
    if (pitchUp != NULL) {
        delta[0] = middleHit[0] - frontHit[0];
        delta[1] = middleHit[1] - frontHit[1];
        delta[2] = middleHit[2] - frontHit[2];
        *pitchUp = AngleNormalize180(vectopitch(delta));
    }
    return 1;

fallback_failure:
    /* 0x300080d8..0x30008129: allowFallback returns 0 without touching outputs;
     * otherwise the failure is reported as 1 after zeroing each non-NULL output. */
    if (allowFallback != qfalse) {
        return 0;
    }
    if (groundOffset != NULL) {
        *groundOffset = 0.0f;
    }
    if (pitchDown != NULL) {
        *pitchDown = 0.0f;
    }
    if (pitchUp != NULL) {
        *pitchUp = 0.0f;
    }
    return 1;
}

#undef BG_PRONE_DEBUG_LINE
