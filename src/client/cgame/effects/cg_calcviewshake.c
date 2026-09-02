// Source: uo_cgame_mp_x86.dll 0x3001b550..0x3001b720
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b550_3001b720.mcode
//
// CG_CalcViewShake — apply the aggregate camera-shake ("earthquake") to the current
// view for this frame. This is the read/apply end of the camera-shake trio:
//   - CG_AddCameraShake (0x3001b420) registers sources into cg_shakeSources[4];
//   - CG_EvaluateCameraShakeSource (0x3001b390) scores one live source;
//   - this walks all four, takes the strongest surviving source's scaledAmplitude
//     (also merging an external amplitude cg_shakeExternAmplitude), then jitters the
//     view origin and drives three sinusoidal view-sway angles by that magnitude.
//
// The .mcode size-guess name "Item_Slider_Paint" is REJECTED (the real
// Item_Slider_Paint is 0x30056c80). This body does no widget/slider drawing: it reads
// cg_time and cg_shakeSources[], calls CG_EvaluateCameraShakeSource, and accumulates
// onto cg_refdef.vieworg and the effect spin-angle triple — camera-shake view math.
// Name is role-derived from that proven call graph; exact original symbol unproved.
//
// cdecl, no source args (operates on file-scope client globals); plain RET. The two
// callee-saved pushes (ESI/EDI) and the mid-body POP EDI/POP ESI (which restore them
// early, shifting ESP by 8 before the tail) are ABI bookkeeping, folded away here.
//
// x87 FIDELITY: the sway tail is transcribed close to one C op per FLD/FMUL/FADD/FSIN
// so the exact evaluation order is preserved. Constants are dumped from .rdata, never
// guessed.
//
// Callees: 0x3001b390 = CG_EvaluateCameraShakeSource (per-source scorer, ESI = source);
//          0x3005b879 = rand() (returns int in [0, 0x7fff]).

#include "client/cgame/client_recovered.h"
#include "compat/coduo_native_x87.h"
#include "client/cgame/globals.h"

#include <math.h>
#include <stdint.h>

// .rdata float constants consumed here (objdump -s -j .rdata):
//   0x3007bce0 = 1.0f     0x3007bcec = 0.0f
//   0x3007bd88 = PI (3.14159274f)   0x3007bda4 = 10.0f
//   0x3007bdf0 = 0.8f    0x3007bec0 = 1/32768 (3.0517578e-05f, == 1/(RAND_MAX+1))
//   0x3007bf00 = 16.0f   0x3007bf08 = 0x4216cbe4 = 37.69911f (12*PI)
//   0x3007bf0c = 0x423c7edd = 47.12389f (15*PI)
//   0x3007bf10 = 18.0f   0x3007bf14 = 0x41c90fdb = 25.132742f (8*PI)
//   0x3007bf18 = 0x3ada740e = 0.0016666667f (1/600)
#define CG_SHAKE_PI         3.14159274f  // 0x3007bd88
#define CG_SHAKE_RAND_NORM  3.0517578125e-05f  // 0x3007bec0 == 1/32768
#define CG_SHAKE_ORG_SCALE  0.8f         // 0x3007bdf0

void CG_CalcViewShake(void)
{
    // 0x3001b553-0x3001b576: timePhase = cg_time * (1/600). cg_time is FILD'd as
    // a signed int32 (0x3001b553 `db 05`) straight into the FMUL at 0x3001b563
    // with no FSTP DWORD between them, so the integer enters the multiply exact;
    // the sole rounding is the FSTP float [ESP+0x10] at 0x3001b576 that lands in
    // timePhase. An explicit (float) on cg_time would round it first (a real
    // fildl/fstps/flds under -std=c11) and diverge past 2^24 ms, ~4.66 h uptime.
    // The remaining (int32_t) is an integer signedness cast, not a rounding.
    long double timePhaseValue = (long double)coduo_int32_from_bits(cg_time);

    // 0x3001b55b/0x3001b569: the running "strongest source" pair, zero-initialized.
    // maxAmplitude tracks the largest scaledAmplitude (+0x1c) among live sources;
    // maxTimeFalloff is the timeFalloff (+0x20) of that same source.
    float maxTimeFalloff = 0.0f;
    timePhaseValue *= 0.0016666667f;
    float maxAmplitude = 0.0f;
    float timePhase = (float)timePhaseValue;

    // 0x3001b580-0x3001b5af: for each of the four shake sources, score it; if it is
    // still live (returns qtrue) and its scaledAmplitude exceeds the running max, adopt
    // it together with its timeFalloff. The FCOMP/TEST AH,0x41/JNZ skip keeps a strict
    // running maximum (skip when scaledAmplitude <= maxAmplitude).
    for (int i = 0; i < 4; i++) {
        cg_shakeSource_t *source = &cg_shakeSources[i];
        if (CG_EvaluateCameraShakeSource(source)) {
            if (source->scaledAmplitude > maxAmplitude) {
                maxAmplitude = source->scaledAmplitude;
                maxTimeFalloff = source->timeFalloff;
            }
        }
    }

    // 0x3001b5b1-0x3001b5d0: merge the externally-supplied amplitude (written by
    // 0x3001f901) into the max the same way; when it wins it supplies both the amplitude
    // and the falloff (the DLL copies it into both stack slots).
    float externalForCompare = cg_shakeExternAmplitude;
    if (externalForCompare > maxAmplitude) {
        float externalForStore = cg_shakeExternAmplitude;
        maxAmplitude = externalForStore;
        maxTimeFalloff = externalForStore;
    }

    // 0x3001b5d2-0x3001b5e1: ordered values <= 0.0f mean no active shake this
    // frame. JP takes the active path for ordered positive and unordered (NaN)
    // values. In the inactive case, re-roll the sway phase and return.
    if (maxAmplitude <= 0.0f) {
        // 0x3001b5e3-0x3001b60f: cg_shakeSpinPhase = randSigned * PI  (a fresh phase in
        // [-PI, PI) held for the next active frame's sin terms), then RET.
        long double phaseRandom = (long double)coduo_crt_rand();
        phaseRandom *= CG_SHAKE_RAND_NORM;
        phaseRandom = phaseRandom + phaseRandom;
        phaseRandom -= 1.0f;
        phaseRandom *= CG_SHAKE_PI;
        cg_shakeSpinPhase = (float)phaseRandom;
        return;
    }

    // 0x3001b610-0x3001b621: clamp the amplitude to 1.0f (skip when maxAmplitude <= 1.0).
    if (maxAmplitude > 1.0f) {
        maxAmplitude = 1.0f;
    }

    // 0x3001b629-0x3001b635: positional shake scale = clampedAmplitude * 0.8f.
    float orgShakeScale = maxAmplitude * CG_SHAKE_ORG_SCALE;

    // 0x3001b640-0x3001b667: three signed random offsets, each scaled by orgShakeScale,
    // one per view-origin axis. (In the DLL the loop counter is pre-incremented and the
    // stores land in the x/y/z slots the tail reads back.)
    float orgOffset[3];
    for (int i = 0; i < 3; i++) {
        long double offset = (long double)coduo_crt_rand();
        offset *= CG_SHAKE_RAND_NORM;
        offset = offset + offset;
        offset -= 1.0f;
        offset *= (long double)orgShakeScale;
        orgOffset[i] = (float)offset;
    }

    // 0x3001b669-0x3001b695: jitter the view origin by the three random offsets.
    cg_refdef.vieworg[0] = cg_refdef.vieworg[0] + orgOffset[0];
    cg_refdef.vieworg[1] = cg_refdef.vieworg[1] + orgOffset[1];
    cg_refdef.vieworg[2] = cg_refdef.vieworg[2] + orgOffset[2];

    // 0x3001b69b-0x3001b716: advance the three effect spin angles with independent
    // sinusoidal sway. Each: angle += sin(timePhase * freq + cg_shakeSpinPhase)
    //                               * maxAmplitude * maxTimeFalloff * gain.
    // freq/gain per axis (from .rdata): pitch 8*PI / 18.0f, yaw 15*PI / 16.0f,
    // roll 12*PI / 10.0f. Each statement is one unbroken x87 chain
    // (FLD/FMUL/FADD/FSIN/FMUL x3/FADD/FSTP) whose ONLY rounding is the final
    // angle store: the FSIN argument and result both stay in 80-bit registers,
    // so the sine is sinl (sinf would round both the argument and the result).
    long double pitch = (long double)timePhase;
    pitch *= 25.132741928100586f; /* 0x3007bf14, 8*PI */
    pitch += (long double)cg_shakeSpinPhase;
    pitch = coduo_x87_sinl(pitch);
    pitch *= (long double)maxAmplitude;
    pitch *= (long double)maxTimeFalloff;
    pitch *= 18.0f;
    pitch += (long double)cg_refdefViewAngles[0];
    cg_refdefViewAngles[0] = (float)pitch;

    long double yaw = (long double)timePhase;
    yaw *= 47.12389f; /* 0x3007bf0c, 15*PI */
    yaw += (long double)cg_shakeSpinPhase;
    yaw = coduo_x87_sinl(yaw);
    yaw *= (long double)maxAmplitude;
    yaw *= (long double)maxTimeFalloff;
    yaw *= 16.0f;
    yaw += (long double)cg_refdefViewAngles[1];
    cg_refdefViewAngles[1] = (float)yaw;

    long double roll = (long double)timePhase;
    roll *= 37.69911193847656f; /* 0x3007bf08, 12*PI */
    roll += (long double)cg_shakeSpinPhase;
    roll = coduo_x87_sinl(roll);
    roll *= (long double)maxAmplitude;
    roll *= (long double)maxTimeFalloff;
    roll *= 10.0f;
    roll += (long double)cg_refdefViewAngles[2];
    cg_refdefViewAngles[2] = (float)roll;
}
