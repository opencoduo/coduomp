// Source: uo_cgame_mp_x86.dll 0x300430a0..0x3004318c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300430a0_3004318c.mcode
//
// CG_SpawnRailCoreSegment — allocate and initialize one rail-trail "core" segment
// as a fading local entity. This is NOT Pmove: the mechanical .mcode name "Pmove"
// was a size guess (function size 0xec matched a Pmove of the same size) and is
// rejected. The body allocates a localEntity_t via CG_AllocLocalEntity (the
// 236-byte / 0xec local-render object) and fills an embedded refEntity_t with the
// railCore shader as a two-point (origin/oldorigin) render entity.
//
// Register/stack ABI proven at the call sites (0x30043511, 0x3004351f, ... which
// set EBX/EDI via LEA and PUSH one pointer before each CALL, then ADD ESP,4):
//   - EBX          = `start`   : vec3 pointer -> refEntity.origin    (+0x94 of le)
//   - EDI          = `end`     : vec3 pointer -> refEntity.oldorigin (+0xa4 of le)
//   - [ESP+0xc]    = `colorRGB`: vec3 pointer (the single pushed cdecl arg) -> le->color
// The function does a plain RET (caller cleans the one stack arg). The EBX/EDI
// register-argument convention is not expressible in portable C without a
// non-portable attribute the build target does not require yet, so it is recorded
// here and the parameters are ordered (start, end, colorRGB).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_SpawnRailCoreSegment(const vec3_t start, const vec3_t end,
                             const vec3_t colorRGB)
{
    localEntity_t *le;

    /* 300430a1 MOV EAX,[cg_railTrailTime_vmCvar.integer]; 300430ae CMP EAX,ESI(=0);
     * 300430b0 JLE 0x30043188 — skip entirely when the lifetime is <= 0.
     * Read as a signed int; ESI is xor'd to 0 at 0x300430ac and reused below. */
    if ((int32_t)cg_railTrailTime_vmCvar.integer <= 0) {
        return;
    }

    /* 300430b6 CALL CG_AllocLocalEntity — returns a freshly zeroed localEntity_t
     * in EAX; all subsequent stores are relative to it. */
    le = CG_AllocLocalEntity();

    /* 300430bb MOV [EAX+0x8],ESI(=0): le->leType = LE_FADE_RGB. The fade-RGB
     * handler (CG_AddFadeRGB) ages this segment out by its remaining-life fraction. */
    le->leType = LE_FADE_RGB;

    /* 300430be MOV ECX,[cg_railTrailTime_vmCvar.integer]; 300430c4 MOV EDX,[cg.time];
     * 300430ca ADD EDX,ECX; 300430cc MOV [EAX+0x10],EDX:
     * le->endTime = cg.time + cg_railTrailTime_vmCvar.integer. */
    le->endTime = coduo_int32_from_bits(
        cg_time + (uint32_t)cg_railTrailTime_vmCvar.integer);

    /* 300430cf FILD [cg_railTrailTime_vmCvar.integer]; 300430d5 FSTP [ESP+8] (spill to float);
     * 300430d9 FLD [0x3007bce0](=1.0f); 300430df FDIV [ESP+8];
     * 300430e3 FSTP [EAX+0x14]: le->lifeRate = 1.0f / (float)cg_railTrailTime_vmCvar.integer.
     * The x87 divide operates on the float-rounded lifetime (FILD -> FSTP dword). */
    float lifetime = (float)coduo_int32_from_bits(
        (uint32_t)cg_railTrailTime_vmCvar.integer);
    le->lifeRate = (float)(1.0L / (long double)lifetime);

    /* 300430ec MOV [EAX+0x50],0x7: le->refEntity.reType = RT_RAIL_CORE. */
    le->refEntity.reType = RT_RAIL_CORE;

    /* 300430e6 FILD [cg.time]; 300430f3 FSTP [ESP+8]; 300430f7 FLD [ESP+8];
     * 300430fb FDIV [0x3007be88](=1000.0f); 30043101 FSTP [EAX+0xc8]:
     * le->refEntity.shaderTime = (float)cg.time / 1000.0f (ms -> seconds). */
    float timeMilliseconds = (float)coduo_int32_from_bits(cg_time);
    le->refEntity.shaderTime = (float)(
        (long double)timeMilliseconds / 1000.0L);

    /* 30043107 MOV ECX,[cg_railCoreShader]; 3004310d MOV [EAX+0xb8],ECX:
     * le->refEntity.spriteShaderHandle = cg_railCoreShader (registered "railCore"). */
    le->refEntity.spriteShaderHandle = cg_railCoreShader;

    /* 30043113..30043127 copy [EBX+0/4/8] into [EAX+0x94/0x98/0x9c]:
     * le->refEntity.origin = start. */
    le->refEntity.origin[0] = start[0];
    le->refEntity.origin[1] = start[1];
    le->refEntity.origin[2] = start[2];

    /* 3004312d..30043141 copy [EDI+0/4/8] into [EAX+0xa4/0xa8/0xac]:
     * le->refEntity.oldorigin = end. */
    le->refEntity.oldorigin[0] = end[0];
    le->refEntity.oldorigin[1] = end[1];
    le->refEntity.oldorigin[2] = end[2];

    /* 30043147..30043156 copy [EBP+0/4/8] into [EAX+0x3c/0x40/0x44]:
     * le->color[0..2] = colorRGB (RGB fade multipliers). */
    le->color[0] = colorRGB[0];
    le->color[1] = colorRGB[1];
    le->color[2] = colorRGB[2];

    /* 30043159 MOV ECX,0x3f800000(=1.0f); 3004315e MOV [EAX+0x48],ECX:
     * le->color[3] = 1.0f (opaque alpha). ECX holds the 1.0f bit pattern and is
     * reused for every 1.0f store below; ESI holds 0 for every zero store. */
    le->color[3] = 1.0f;

    /* Identity 3x3 orientation basis in le->refEntity.axis (le +0x6c..+0x8c,
     * i.e. refEntity +0x1c..+0x3c). ECX still holds 1.0f, ESI still holds 0:
     *   30043161 [EAX+0x6c]=1.0f  -> axis[0][0]
     *   30043164 [EAX+0x70]=0     -> axis[0][1]
     *   30043167 [EAX+0x74]=0     -> axis[0][2]
     *   3004316a [EAX+0x78]=0     -> axis[1][0]
     *   3004316d [EAX+0x7c]=1.0f  -> axis[1][1]
     *   30043170 [EAX+0x80]=0     -> axis[1][2]
     *   30043176 [EAX+0x84]=0     -> axis[2][0]
     *   3004317c [EAX+0x88]=0     -> axis[2][1]
     *   30043182 [EAX+0x8c]=1.0f  -> axis[2][2] */
    le->refEntity.axis[0][0] = 1.0f;
    le->refEntity.axis[0][1] = 0.0f;
    le->refEntity.axis[0][2] = 0.0f;
    le->refEntity.axis[1][0] = 0.0f;
    le->refEntity.axis[1][1] = 1.0f;
    le->refEntity.axis[1][2] = 0.0f;
    le->refEntity.axis[2][0] = 0.0f;
    le->refEntity.axis[2][1] = 0.0f;
    le->refEntity.axis[2][2] = 1.0f;
}
