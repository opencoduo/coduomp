// Source: uo_cgame_mp_x86.dll 0x3003ba10..0x3003c0e7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003ba10_3003c0e7.mcode
// CG_SetShellShockParams — resolve the 27 cg_shock_* vmCvars into one runtime block.

#include "../client_recovered.h"

enum {
    SHOCK_SCREEN_BLEND_TIME,
    SHOCK_SCREEN_BLEND_FADE_TIME,
    SHOCK_VIEW_KICK_PERIOD,
    SHOCK_VIEW_KICK_RADIUS,
    SHOCK_SOUND,
    SHOCK_SOUND_FADE_IN_TIME,
    SHOCK_SOUND_FADE_OUT_TIME,
    SHOCK_SOUND_LOOP_FADE_TIME,
    SHOCK_SOUND_LOOP_END_DELAY,
    SHOCK_SOUND_ROOM_TYPE,
    SHOCK_SOUND_WET_LEVEL,
    SHOCK_SOUND_MOD_END_DELAY,
    SHOCK_VOLUME_AUTO,
    SHOCK_VOLUME_MENU,
    SHOCK_VOLUME_WEAPON,
    SHOCK_VOLUME_VOICE,
    SHOCK_VOLUME_ITEM,
    SHOCK_VOLUME_BODY,
    SHOCK_VOLUME_LOCAL,
    SHOCK_VOLUME_MUSIC,
    SHOCK_VOLUME_ANNOUNCER,
    SHOCK_VOLUME_SHELLSHOCK,
    SHOCK_MOUSE,
    SHOCK_MOUSE_MAX_PITCH_SPEED,
    SHOCK_MOUSE_MAX_YAW_SPEED,
    SHOCK_MOUSE_SENSITIVITY_SCALE,
    SHOCK_MOUSE_FADE_TIME
};

#define SET_SHOCK_MILLISECONDS(field_, index_, minimumOne_) \
    do { \
        int32_t shockMilliseconds_ = Script_RoundToNearestInt(cg_shockParamTargets[(index_)]->value * 1000.0f); \
        (field_) = (minimumOne_) && shockMilliseconds_ < 1 ? 1 : shockMilliseconds_; \
    } while (0)

#define SET_SHOCK_UNIT(field_, index_) \
    do { \
        float shockUnitValue_ = cg_shockParamTargets[(index_)]->value; \
        if (shockUnitValue_ < 0.0f) \
            shockUnitValue_ = 0.0f; \
        if (shockUnitValue_ > 1.0f) \
            shockUnitValue_ = 1.0f; \
        (field_) = shockUnitValue_; \
    } while (0)

void CG_SetShellShockParams(shellshock_t *out)
{
    float period = cg_shockParamTargets[SHOCK_VIEW_KICK_PERIOD]->value;

    SET_SHOCK_MILLISECONDS(out->screenBlendFadeTime, SHOCK_SCREEN_BLEND_FADE_TIME, qtrue);
    SET_SHOCK_MILLISECONDS(out->screenBlendTime, SHOCK_SCREEN_BLEND_TIME, qtrue);
    out->blurDivisor = 3000;
    /* 0x3003bad2..0x3003bb06: the 0.001f constant (0x3007bd94) is BOTH the clamp
     * threshold and the numerator: blurRate = 0.001f / max(period, 0.001f). */
    if (period < 0.001f)
        period = 0.001f;
    out->blurRate = 0.001f / period;
    out->blurScale = cg_shockParamTargets[SHOCK_VIEW_KICK_RADIUS]->value;
    out->soundEnabled = cg_shockParamTargets[SHOCK_SOUND]->integer != 0;
    SET_SHOCK_MILLISECONDS(out->soundFadeInTime, SHOCK_SOUND_FADE_IN_TIME, qtrue);
    SET_SHOCK_MILLISECONDS(out->soundFadeOutTime, SHOCK_SOUND_FADE_OUT_TIME, qtrue);
    SET_SHOCK_MILLISECONDS(out->soundLoopFadeTime, SHOCK_SOUND_LOOP_FADE_TIME, qtrue);
    SET_SHOCK_MILLISECONDS(out->soundLoopEndDelay, SHOCK_SOUND_LOOP_END_DELAY, qfalse);
    Q_strncpyz(out->soundRoomType, cg_shockParamTargets[SHOCK_SOUND_ROOM_TYPE]->string, (int)sizeof(out->soundRoomType));
    SET_SHOCK_UNIT(out->soundWetLevel, SHOCK_SOUND_WET_LEVEL);
    SET_SHOCK_MILLISECONDS(out->soundModEndDelay, SHOCK_SOUND_MOD_END_DELAY, qfalse);
    SET_SHOCK_UNIT(out->soundVolume[0], SHOCK_VOLUME_AUTO);
    SET_SHOCK_UNIT(out->soundVolume[1], SHOCK_VOLUME_MENU);
    SET_SHOCK_UNIT(out->soundVolume[2], SHOCK_VOLUME_BODY);
    SET_SHOCK_UNIT(out->soundVolume[3], SHOCK_VOLUME_ITEM);
    SET_SHOCK_UNIT(out->soundVolume[4], SHOCK_VOLUME_WEAPON);
    SET_SHOCK_UNIT(out->soundVolume[5], SHOCK_VOLUME_VOICE);
    SET_SHOCK_UNIT(out->soundVolume[6], SHOCK_VOLUME_LOCAL);
    SET_SHOCK_UNIT(out->soundVolume[7], SHOCK_VOLUME_MUSIC);
    SET_SHOCK_UNIT(out->soundVolume[8], SHOCK_VOLUME_ANNOUNCER);
    SET_SHOCK_UNIT(out->soundVolume[9], SHOCK_VOLUME_SHELLSHOCK);
    out->mouseEnabled = cg_shockParamTargets[SHOCK_MOUSE]->integer != 0;
    SET_SHOCK_MILLISECONDS(out->mouseFadeTime, SHOCK_MOUSE_FADE_TIME, qtrue);
    out->mouseSensitivityScale = cg_shockParamTargets[SHOCK_MOUSE_SENSITIVITY_SCALE]->value;
    out->mouseMaxPitchSpeed = cg_shockParamTargets[SHOCK_MOUSE_MAX_PITCH_SPEED]->value;
    out->mouseMaxYawSpeed = cg_shockParamTargets[SHOCK_MOUSE_MAX_YAW_SPEED]->value;
}

#undef SET_SHOCK_UNIT
#undef SET_SHOCK_MILLISECONDS
