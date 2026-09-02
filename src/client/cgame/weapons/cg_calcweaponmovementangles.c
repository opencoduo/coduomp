#include "../client_recovered.h"
// Source: uo_cgame_mp_x86.dll 0x30044ce0..0x30045063
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30044ce0_30045063.mcode
//
// Construct stance/speed-driven weapon movement angles, smooth the persistent
// three-axis state toward them, and attenuate the result through the first half
// of the ADS transition. Exact original symbol is unresolved; the role-based
// name follows the sole caller at 0x30045070.

enum {
    CG_WEAPON_STANCE_CROUCH = 0x20,
    CG_WEAPON_STANCE_PRONE = 0x40
};

/* Exact .rdata dword at 0x3007bd90: 0x38d1b718. The shorter 0.0001f
 * spelling rounds one ULP lower (0x38d1b717). */
#define CG_WEAPON_MOVE_MINIMUM_STEP_SCALE 0.000100000005f

void CG_CalcWeaponMovementAngles(vec3_t angles)
{
    const weaponInfo_t *weapon = cg_currentWeaponInfo;
    const playerState_t *ps = &cg_predictedPlayerState;
    const uint8_t stanceFlags = (uint8_t)cg_predictedEventEntity.currentState.eFlags;
    const qboolean sprinting = (ps->playerStateFlags & PMF_SPRINTING) != 0;
    long double thresholdBias = cg_gun_move_minspeed_vmCvar.value;
    float currentSpeed = cg_weaponMoveSpeed;
    long double threshold;
    vec3_t target = {0.0f, 0.0f, 0.0f};

    if (sprinting) {
        threshold = thresholdBias + weapon->moveThresholdAlt;
    } else if ((stanceFlags & CG_WEAPON_STANCE_PRONE) != 0) {
        threshold = thresholdBias + weapon->moveThresholdProne;
    } else if ((stanceFlags & CG_WEAPON_STANCE_CROUCH) != 0) {
        threshold = thresholdBias + weapon->moveThresholdCrouch;
    } else {
        threshold = thresholdBias + weapon->moveThresholdStand;
    }

    if (currentSpeed > threshold && ps->weaponState != WEAPON_STATE_RELOADING) {
        const vec3_t *move;
        /* ps->speed enters via a bare FILD fed straight into FSUB threshold
         * (0x30044d54 FILD; 0x30044d5a FSUB ST0,ST2 — no FSTP DWORD between), so
         * the DLL keeps it exact; an explicit (float) cast would round it (Class 4). */
        long double factor = ((long double)currentSpeed - threshold) / ((long double)ps->speed - threshold);
        vec3_t bias = {cg_gun_move_f_vmCvar.value, cg_gun_move_r_vmCvar.value, cg_gun_move_u_vmCvar.value};

        if (factor > 1.0L) {
            factor = 1.0L;
        } else if (factor < 0.0L) {
            factor = 0.0L;
        }

        if (sprinting) {
            move = &weapon->sprintMove;
        } else if ((stanceFlags & CG_WEAPON_STANCE_PRONE) != 0) {
            move = &weapon->proneMove;
        } else if ((stanceFlags & CG_WEAPON_STANCE_CROUCH) != 0) {
            move = &weapon->duckedMove;
        } else {
            move = &weapon->standMove;
        }

        for (int i = 0; i < 3; ++i) {
            /* 0x30044da1..0x30044e0d forms factor*move and factor*bias
             * separately, adds them while both are still wide, then stores. */
            target[i] = (float)(factor * (long double)(*move)[i] + factor * (long double)bias[i]);
        }
    }

    if (ps->viewHeightTarget == ps->crouchViewHeight || ps->viewHeightTarget == ps->proneViewHeight) {
        vec3_t crouchBias = {cg_gun_ofs_f_vmCvar.value, cg_gun_ofs_r_vmCvar.value, cg_gun_ofs_u_vmCvar.value};
        const vec3_t *stanceOffset = ps->viewHeightTarget == ps->crouchViewHeight ? &weapon->duckedOffset : &weapon->proneOffset;
        long double stance0 = (long double)target[0] + (long double)(*stanceOffset)[0];
        long double stance1 = (long double)target[1] + (long double)(*stanceOffset)[1];

        /* The third target component alone is FSTP'd before its cvar bias is
         * added (0x30044e9a); components zero and one keep the stance sum live. */
        target[2] = (float)((long double)target[2] + (long double)(*stanceOffset)[2]);
        target[0] = (float)(stance0 + (long double)crouchBias[0]);
        target[1] = (float)(stance1 + (long double)crouchBias[1]);
        target[2] = (float)((long double)target[2] + (long double)crouchBias[2]);
    }

    {
        float *state = cg_weaponMoveAngles;
        float smoothBias = cg_gun_move_rate_vmCvar.value;
        const float frameTime = (float)(long double)cg_frametime;
        const float minimumStep = (float)((long double)frameTime * (long double)CG_WEAPON_MOVE_MINIMUM_STEP_SCALE);

        for (int i = 0; i < 3; ++i) {
            if (target[i] != state[i]) {
                /* proneViewHeight enters via a bare FILD compared directly against
                 * ps->viewHeightCurrent (0x30044ef8 FILD; 0x30044efe FLD; 0x30044f04 FUCOMPP
                 * — no FSTP DWORD), so drop the (float) cast (Class 4). */
                const long double smooth =
                    (long double)smoothBias + (long double)((long double)ps->viewHeightCurrent == (long double)ps->proneViewHeight
                                                                ? weapon->moveSmoothProne
                                                                : weapon->moveSmooth);
                long double step = ((long double)target[i] - (long double)state[i]) * smooth * (long double)frameTime * (long double)0.001f;

                if (state[i] < target[i]) {
                    long double newState;
                    if (step < (long double)minimumStep) {
                        step = (long double)minimumStep;
                    }
                    newState = (long double)state[i] + step;
                    state[i] = (float)newState;          /* 0x30044f71 FST */
                    if (newState > (long double)target[i]) {
                        state[i] = target[i];
                    }
                } else {
                    long double newState;
                    if (step > -(long double)minimumStep) {
                        step = -(long double)minimumStep;
                    }
                    newState = (long double)state[i] + step;
                    state[i] = (float)newState;          /* 0x30044fac FST */
                    if (newState < (long double)target[i]) {
                        state[i] = target[i];
                    }
                }
            }
        }

        if (ps->adsFraction == 0.0f) {
            for (int i = 0; i < 3; ++i) {
                angles[i] += state[i];
            }
        } else if (ps->adsFraction < 0.5f) {
            const long double scale = 1.0L - (long double)ps->adsFraction * 2.0L;
            for (int i = 0; i < 3; ++i) {
                angles[i] = (float)((long double)angles[i] + (long double)state[i] * scale);
            }
        }
    }
}
