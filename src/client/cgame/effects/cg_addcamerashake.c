// Source: uo_cgame_mp_x86.dll 0x3001b420..0x3001b543
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b420_3001b543.mcode
//
// CG_AddCameraShake — register a new active camera-shake ("earthquake") source.
//
// The .mcode assigned name `script_method_scriptbuiltin_viewkick` (a size-match
// against game_mp_uo coverage, win size 0x123 == matched size 0x123) is REJECTED
// per the "never name by size" rule: this body does no playerState/viewangle
// kick math. It builds a cg_shakeSource_t on the stack and REP MOVSDs it (9 dwords
// == 0x24) into a slot of the fixed 4-slot cg_shakeSources[] table at 0x3048b52c.
// That is the write side of the camera-shake trio whose evaluator
// (CG_EvaluateCameraShakeSource, 0x3001b390) and aggregate walker (0x3001b550)
// are already recovered; the struct/field names come from that proven trio.
//
// Client register/stack ABI (proven at all three call sites — 0x300237ee,
// 0x3002387f, 0x3002b0f4 — each does `add esp,0xc` after the call, i.e. 3 stack
// args + ECX): world origin vec3 in ECX; stack args (amplitude float, duration
// int, radius float). `duration` is FILD'd (int->float) into the shake source.
// The two callee-saved pushes (ESI/EDI) and the RET are ABI, not source.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* cg_shakeSource_t layout is asserted where it is consumed
 * (FUN_3001b390_3001b414.c); not repeated here. */

void CG_AddCameraShake(const vec3_t origin, float amplitude,
                       int32_t duration, float radius)
{
    /* 3001b420 FLD [ESP+4] (amplitude) ; 3001b427 FCOMP 0.0f (.rdata 0x3007bcec)
     * 3001b42f TEST AH,0x41 / 3001b432 JNP 0x3001b53f: continue only when
     * amplitude > 0.0f, otherwise return without touching the table. */
    if (amplitude <= 0.0f) {
        return;
    }

    /* Build the new source on the stack, then copy it into the chosen slot.
     * Field assignment proven by tracing the stack writes at 0x3001b443..b470
     * against the cg_shakeSource_t offsets the evaluator reads:
     *   +0x00 startMsec       <- cg_time            (MOV [ESP+8],EDI)
     *   +0x04 amplitude       <- amplitude (arg1)   (MOV [ESP+8],EAX before EDI store at +0x04 slot)
     *   +0x08 duration        <- (float)duration    (FILD [ESP+2c] / FSTP [ESP+0xc])
     *   +0x0c radius          <- radius (arg3)       (MOV [ESP+0x14],EDX)
     *   +0x10 origin.x/y/z    <- *origin[0..2]      (MOV [ESP+0x18/1c/20]) */
    cg_shakeSource_t src;
    uint32_t originXBits;
    uint32_t amplitudeBits;
    uint32_t originYBits;
    uint32_t originZBits;
    uint32_t radiusBits;

    /* These five inputs are integer MOVs in the target, not x87 value copies.
     * Preserve their complete object representations, including NaN payloads. */
    memcpy(&originXBits, &origin[0], 4);
    long double durationValue = (long double)duration; /* FILD dword */
    memcpy(&amplitudeBits, &amplitude, 4);
    memcpy(&src.amplitude, &amplitudeBits, 4);
    src.duration = (float)durationValue;                /* FSTP m32 */
    memcpy(&originYBits, &origin[1], 4);
    memcpy(&originZBits, &origin[2], 4);
    int32_t now = coduo_int32_from_bits(cg_time);
    memcpy(&radiusBits, &radius, 4);
    memcpy(&src.origin[0], &originXBits, 4);
    src.startMsec = now;
    memcpy(&src.origin[1], &originYBits, 4);
    memcpy(&src.origin[2], &originZBits, 4);
    memcpy(&src.radius, &radiusBits, 4);
    /* src.scaledAmplitude (+0x1c) / src.timeFalloff (+0x20) are outputs of the
     * evaluator call immediately below. */

    /* 0x3001b470..0x3001b474: ESI already points at the stack source and the
     * direct call evaluates its time/distance falloff before slot selection.
     * EAX is discarded at 0x3001b479; only the two output fields matter here. */
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (CG_EvaluateCameraShakeSource(&src) == qfalse) {
        return;
    }

    /* 3001b479..3001b4b2: scan for the first free/expired slot.
     *   free  == slot.startMsec > now                (JG 0x3001b528)
     *   or    == now - slot.startMsec >= slot.duration i.e. expired
     *           (FILD startMsec + FADD duration, FCOMPP vs FILD now,
     *            TEST AH,1 / JZ 0x3001b528: take slot when (start+duration) <= now)
     * Both integer clocks are converted directly to retained x87 values. */
    int slot = -1;
    int i;
    for (i = 0; i < 4; i++) {
        int32_t startMsec = cg_shakeSources[i].startMsec;   /* MOV EAX,[ECX] */
        if (startMsec > now) {                              /* CMP EAX,EDI / JG */
            slot = i;
            break;
        }
        /* FILD startMsec / FADD duration / FILD now / FCOMPP; TEST AH,1 / JZ.
         * After the loads ST0 = (float)now (pushed last) and ST1 = start+duration.
         * FCOMPP sets C0 (AH bit0) when ST0 < ST1, i.e. now < start+duration.
         * JZ is taken when C0 == 0, i.e. now >= start+duration => the slot's shake
         * has fully elapsed => reuse it. */
        long double slotEnd = (long double)startMsec;
        slotEnd += (long double)cg_shakeSources[i].duration;
        int32_t comparisonNow = coduo_int32_from_bits(cg_time);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((long double)comparisonNow >= slotEnd) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        /* 3001b4b4..3001b526: all four slots are live. Replace the slot with the
         * smallest scaledAmplitude (weakest current shake). The running minimum
         * is seeded from the evaluated src.scaledAmplitude at stack offset +0x1c
         * (FLD [ESP+0x24] after the callee-saved pushes). A slot replaces only if its
         * scaledAmplitude is strictly below the running minimum; if none is, the
         * loop counter stays 4 (CMP EDX,4 / JZ 0x3001b53d) and the new shake is
         * dropped without touching the table. */
        int minIndex = 4;   /* EDX still 4 from the search loop */
        float minAmplitude = src.scaledAmplitude;   /* FLD [ESP+0x24] */

        /* FLD slot0.scaledAmplitude / FCOMP threshold; TEST AH,5 / JP skip.
         * AH bit0 = C0 (ST0 < mem). JP not taken (fall through) => C0 set =>
         * slot value < current minimum => update. Repeated for slots 0..3. */
        if (cg_shakeSources[0].scaledAmplitude < minAmplitude) {
            minIndex = 0;
            minAmplitude = cg_shakeSources[0].scaledAmplitude;
        }
        if (cg_shakeSources[1].scaledAmplitude < minAmplitude) {
            minIndex = 1;
            minAmplitude = cg_shakeSources[1].scaledAmplitude;
        }
        if (cg_shakeSources[2].scaledAmplitude < minAmplitude) {
            minIndex = 2;
            minAmplitude = cg_shakeSources[2].scaledAmplitude;
        }
        if (cg_shakeSources[3].scaledAmplitude < minAmplitude) {
            minIndex = 3;
            /* minAmplitude no longer used after this last compare */
        }

        if (minIndex == 4) {
            /* 3001b523 CMP EDX,4 / 3001b526 JZ 0x3001b53d: nothing to replace. */
            return;
        }
        slot = minIndex;
    }

    /* 3001b528..3001b53b: LEA EDI,[EDX+EDX*8] / LEA EDI,[EDI*4 + 0x3048b52c] gives
     * &cg_shakeSources[slot] (slot*0x24); REP MOVSD copies the 0x24-byte source. */
    memcpy(&cg_shakeSources[slot], &src, sizeof(src));
}
