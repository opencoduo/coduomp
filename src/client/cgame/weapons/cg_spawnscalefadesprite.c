// Source: uo_cgame_mp_x86.dll 0x30048050..0x300480e5
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30048050_300480e5.mcode

#include "../client_recovered.h"

void CG_SpawnScaleFadeSprite(const vec3_t origin, qhandle_t shader, int32_t radius, int32_t duration)
{
    localEntity_t *le = CG_AllocLocalEntity();
    le->leType = LE_SCALE_FADE;
    le->leFlags = LEF_SCALE_FADE_NO_RADIUS;
    le->endTime = coduo_int32_from_bits((uint32_t)cg_time + (uint32_t)duration);
    /* duration enters via a bare FILD fed straight into FDIVR (0x30048055 FILD;
     * 0x30048061 FDIVR 1.0f) with no FSTP DWORD between, so drop the (float) cast
     * (Class 4). */
    le->lifeRate = 1.0f / duration;

    le->refEntity.origin[0] = origin[0];
    le->refEntity.origin[1] = origin[1];
    le->refEntity.origin[2] = origin[2];
    le->refEntity.reType = RT_SPLASH;
    le->refEntity.spriteShaderHandle = shader;
    le->refEntity.shaderRGBA[0] = 255;
    le->refEntity.shaderRGBA[1] = 255;
    le->refEntity.shaderRGBA[2] = 255;
    le->refEntity.shaderRGBA[3] = 255;
    /* cg_time enters via a bare FILD fed straight into FMUL 0.001f (0x3004809a FILD;
     * 0x300480a9 FMUL) with no FSTP DWORD between, so drop the (float) cast (Class 4). */
    le->refEntity.shaderTime = cg_time * 0.001f;
    le->refEntity.radius = (float)radius;
    le->color[3] = 1.0f;
}
