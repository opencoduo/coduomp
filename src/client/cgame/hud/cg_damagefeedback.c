// Source: uo_cgame_mp_x86.dll 0x30034ac0..0x30034d32
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30034ac0_30034d32.mcode
//
// CG_DamageFeedback(yaw, pitch, damage) — trigger the local player's damage
// feedback when a hit is registered: paint the red directional damage blend, kick
// the screen flash, and register a new damage-direction HUD arrow pointing back at
// the attacker. Called by CG_TransitionPlayerState (0x30034fe0) with the
// playerState_t damage fields: CG_DamageFeedback(ps->damageYaw, ps->damagePitch,
// ps->damageCount). The three arguments are 0..255-quantised BAMS angles (yaw,
// pitch) and the damage magnitude.
//
// Name resolution: the .mcode header's "CG_AddPacketEntities" is a pure win-size
// match (0x272 vs 0x274) and is WRONG — CG_AddPacketEntities is a large dispatcher
// elsewhere. This function is the canonical Quake3/CoD CG_DamageFeedback: it writes
// the cg.damageX / cg.damageValue blend pair, the cg.damageTime+500 flash timer, and
// the cg.damageDir[] indicator ring. The same-module PPC symbol is CG_DamageFeedback,
// and the sole caller (CG_TransitionPlayerState) proves the 3-arg (yaw,pitch,damage)
// shape (see functions/FUN_30034fe0_30035024.c). Header decl already present.
//
// .rdata float constants (dumped via objdump -s -j .rdata):
//   0x3007bce8 = 0.5            (RAND jitter recentre)
//   0x3007bd10 = 32768.0        (rand() / 32768 -> [0,1))
//   0x3007bd54 = 360.0          (BAMS-in-255 -> degrees, numerator)
//   0x3007bd5c = 0.0054931640625 = 360/65536  (16-bit BAMS -> degrees)
//   0x3007bd60 = 182.0444...   = 65536/360    (degrees -> 16-bit BAMS)
//   0x3007bd64 = 255.0         (BAMS-in-255 -> degrees, denominator)
//   0x3007bd70 = 0.01745329238 = M_PI/180     (degrees -> radians)
//   0x3007bde0 = 5.0           (kick clamp minimum)
//   0x3007be04 = 20.0          (rand jitter span: (r-0.5)*20 -> +/-10 deg)
//   0x3007be10 = 0.2           (kick = damage * 0.2)
//   0x3007be8c = 90.0          (kick clamp maximum)
//
// Callees: 0x3005b879 = rand() (CRT, jitters the indicator arrow yaw);
//          0x3006be3c = _ftol2 (CRT, TRUNCATES float->int for the AngleMod wrap).
//
// The x87 FSINCOS pairs and the two DotProducts against cg_refdef.viewaxis[0] /
// cg_refdef.viewaxis[1] build the classic AngleVectors "forward" unit vector from the
// (pitch, yaw) damage direction and project it onto the view basis to get the
// screen-space {X, value} damage blend. The all-directions sentinel (yaw==255 &&
// pitch==255) skips the projection and paints a plain full-screen flash.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#define DEG2RAD 0.01745329238474369f  // 0x3007bd70 = M_PI/180

enum {
    CG_DAMAGE_DIRECTION_ALL = 255,
    CG_DAMAGE_FLASH_DURATION_MS = 500
};

void CG_DamageFeedback(int32_t yaw, int32_t pitch, int32_t damage)
{
    /* 0x30034ac3..0x30034ad1: FILD damage remains live across the cg.time load and
     * feedback-time store, then is committed to a binary32 stack slot. */
    long double damageRaw = (long double)damage;
    int32_t feedbackTime = coduo_int32_from_bits(cg_time);
    cg_damageFeedbackTime = feedbackTime;
    float damageFloat = (float)damageRaw;

    // 0x30034ad6..0x30034b19: view-kick magnitude = clamp(damage * 0.2, 5, 90).
    float kick = (float)(
        (long double)damageFloat * (long double)0.2f);
    if (kick < 5.0f) {                    // 0x3007bde0
        kick = 5.0f;
    } else if (kick > 90.0f) {            // 0x3007be8c
        kick = 90.0f;
    }

    if (yaw == CG_DAMAGE_DIRECTION_ALL &&
        pitch == CG_DAMAGE_DIRECTION_ALL) {
        // 0x30034b26..0x30034b42: attacker direction unknown / all-around damage —
        // no directional arrow, just the plain screen flash: X=0, value=-kick.
        cg_damageFlashX = 0.0f;           // 0x3048af14
        cg_damageFlashScale = -kick;      // 0x3048af10 (FCHS then store)
    } else {
        // 0x30034b47..0x30034b99: decode the two quantised angles.
        // The wire yaw/pitch are 0..255 fractions of a full turn:
        //   deg = value / 255.0 * 360.0
        float pitchInput = (float)pitch;
        float pitchDeg = (float)(
            ((long double)pitchInput / (long double)255.0f)
            * (long double)360.0f);
        float yawInput = (float)yaw;
        float yawDeg = (float)(
            ((long double)yawInput / (long double)255.0f)
            * (long double)360.0f);

        /* 0x30034b8f..0x30034be3: each radians product is rounded to binary32,
         * then one hardware FSINCOS stores cosine first and sine second. */
        float yawRadians = (float)(
            (long double)yawDeg * (long double)DEG2RAD);
        float sinYaw;
        float cosYaw;
        coduo_x87_sincosf(yawRadians, &sinYaw, &cosYaw);

        float pitchRadians = (float)(
            (long double)pitchDeg * (long double)DEG2RAD);
        float sinPitch;
        float cosPitch;
        coduo_x87_sincosf(pitchRadians, &sinPitch, &cosPitch);

        // 0x30034be7..0x30034c13: AngleVectors "forward" for (pitch, yaw).
        //   dir.x =  cos(pitch)*cos(yaw)
        //   dir.y =  cos(pitch)*sin(yaw)
        //   dir.z = -sin(pitch)
        vec3_t damageDir;
        damageDir[0] = (float)(
            (long double)cosPitch * (long double)cosYaw);
        damageDir[1] = (float)(
            (long double)cosPitch * (long double)sinYaw);
        damageDir[2] = -sinPitch;

        // 0x30034c17..0x30034c6b: project the damage direction onto the view basis
        // to get the screen-space blend pair, scaled by the kick magnitude.
        //   X     = -DotProduct(cg_refdef.viewaxis[1], dir) * kick   (horizontal)
        //   value =  DotProduct(cg_refdef.viewaxis[0], dir) * kick (toward/away)
        // x87 FADDP chain order is z-first: (v[2]*d[2] + v[1]*d[1]) + v[0]*d[0]
        // (FLD [..ab0]; FMUL d2; FLD [..aac]; FMUL d1; FADDP; FLD [..aa8];
        //  FMUL d0; FADDP; FMUL kick; [FCHS;] FSTP) -- term order preserved here.
        long double horizontalRaw =
            ((long double)cg_refdef.viewaxis[1][2]
                 * (long double)damageDir[2]
             + (long double)cg_refdef.viewaxis[1][1]
                 * (long double)damageDir[1])
            + (long double)cg_refdef.viewaxis[1][0]
                 * (long double)damageDir[0];
        horizontalRaw *= (long double)kick;
        cg_damageFlashX = (float)-horizontalRaw;

        long double forwardRaw =
            ((long double)cg_refdef.viewaxis[0][2]
                 * (long double)damageDir[2]
             + (long double)cg_refdef.viewaxis[0][1]
                 * (long double)damageDir[1])
            + (long double)cg_refdef.viewaxis[0][0]
                 * (long double)damageDir[0];
        forwardRaw *= (long double)kick;
        cg_damageFlashScale = (float)forwardRaw;

        // 0x30034c71..0x30034c8a: find the least-recently-used indicator slot
        // (minimum serverTime) among the 8-slot damage-direction ring.
        int32_t best = 0;
        for (int32_t i = 1; i < CG_DAMAGE_DIRECTION_SLOT_COUNT; i++) {
            int32_t candidateTime = cg_damageDirIndicators[i].serverTime;
            int32_t bestTime = cg_damageDirIndicators[best].serverTime;
            if (candidateTime < bestTime) {
                best = i;
            }
        }

        /* 0x30034c8c..0x30034ca7: the duration global is read before the snapshot
         * pointer and serverTime, then the two dwords are stored in that order. */
        int32_t duration = cg_hudDamageIconTime_vmCvar.integer;
        snapshot_t *snap = cg_snap;
        int32_t serverTime = snap->serverTime;
        cg_damageDirIndicator_t *indicator = &cg_damageDirIndicators[best];
        indicator->serverTime = serverTime;
        indicator->duration = duration;

        // 0x30034cad..0x30034d06: the arrow yaw = AngleMod(yawDeg + jitter), where
        // jitter = (rand()/32768 - 0.5) * 20  (a +/-10 degree wobble). AngleMod is
        // the classic 16-bit BAMS wrap: (int)(deg * 65536/360) & 0xffff back to deg.
        // 0x3006be3c is _ftol2 (truncation), matching the (int) cast.
        int32_t randomValue = coduo_crt_rand();
        float randomFloat = (float)randomValue;
        float jitteredYaw = (float)(
            ((((long double)randomFloat / (long double)32768.0f)
               - (long double)0.5f)
              * (long double)20.0f)
            + (long double)yawDeg);
        int32_t bams = (int32_t)(
            (uint32_t)coduo_fp_to_i32_extended(
                (long double)jitteredYaw
                * (long double)182.04444885253906f)
            & 65535u);
        float bamsFloat = (float)bams;
        indicator->yaw = (float)(
            (long double)bamsFloat * (long double)0.0054931640625f);
    }

    /* 0x30034d0c..0x30034d28: snapshot both globals before the target-width ADD,
     * store the wrapped deadline, then dereference the retained snapshot. */
    int32_t now = coduo_int32_from_bits(cg_time);
    snapshot_t *snap = cg_snap;
    int32_t flashEndTime = coduo_int32_from_bits(
        (uint32_t)now + (uint32_t)CG_DAMAGE_FLASH_DURATION_MS);
    cg_damageFlashEndTime = flashEndTime;
    int32_t latestServerTime = snap->serverTime;
    cg_damageDirLatestServerTime = latestServerTime;
}
