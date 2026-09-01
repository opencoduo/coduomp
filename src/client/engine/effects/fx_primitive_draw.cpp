/* Primitive-effect draw virtuals recovered from CoDUOMP.exe. */

#include "fx_classes.hpp"
#include "fx_runtime.h"

static void coduomp_fx_draw_depth_hack_entity(CEffect &effect)
{
    /* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of four byte-identical
     * draw virtuals. */
    if ((effect.flags & FX_EFFECT_FLAG_DEPTH_HACK) != 0U) {
        effect.renderEntity.renderfx |= FX_RENDER_FLAG_DEPTH_HACK;
    }
    SFxHelper_AddFxToScene(&effect.renderEntity);
}

/* 0x004a24d0..0x004a24ed; Mac symbol: COrientedParticle::Draw. */
void COrientedParticle::Draw()
{
    coduomp_fx_draw_depth_hack_entity(*this);
}

/* 0x004a26a0..0x004a26bd; Mac symbol: CLine::Draw. */
void CLine::Draw()
{
    coduomp_fx_draw_depth_hack_entity(*this);
}

/* 0x004a2ad0..0x004a2aed; Mac symbol: CTail::Draw. */
void CTail::Draw()
{
    coduomp_fx_draw_depth_hack_entity(*this);
}

/* 0x004a2f80..0x004a2f9d; Mac symbol: CCylinder::Draw. */
void CCylinder::Draw()
{
    coduomp_fx_draw_depth_hack_entity(*this);
}

/* 0x004a3190..0x004a31a9; Mac symbol: CEmitter::Draw. */
void CEmitter::Draw()
{
    if ((flags & FX_EFFECT_FLAG_RENDER_EFFECT) != 0U) {
        SFxHelper_AddFxToScene(&renderEntity);
    }
}

/* 0x004a3d60..0x004a3d84; Mac symbol: CLight::Draw. */
void CLight::Draw()
{
    SFxHelper_AddLightToScene(renderEntity.origin, renderEntity.radius,
                              renderEntity.lightingOrigin[0],
                              renderEntity.lightingOrigin[1],
                              renderEntity.lightingOrigin[2]);
}

/* Source: CoDUOMP.exe 0x004a41a0..0x004a44f0.
 * Class binding: Windows vtable 0x005a2be8. The four tracked corners are sent
 * as triangles (0,1,3) and (3,2,1). */
void CQuad::Draw()
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    polyVert_t triangle[3];

    for (int triangleIndex = 0; triangleIndex < 2; ++triangleIndex) {
        for (int corner = 0; corner < 3; ++corner) {
            int vertexIndex;
            if (triangleIndex == 0) {
                vertexIndex = corner == 2 ? 3 : corner;
            } else {
                vertexIndex = corner == 0 ? 3 : 3 - corner;
            }
            const fx_quad_vertex_t &source =
                vertices[vertexIndex];
            for (int component = 0; component < 3; ++component) {
                triangle[corner].xyz[component] = source.position[component];
                triangle[corner].modulate[component] =
                    static_cast<uint8_t>(
                        coduo_fp_to_i32_extended(
                            static_cast<long double>(
                                source.color[component]) *
                            static_cast<long double>(source.alpha)));
            }
            for (int component = 0; component < 2; ++component) {
                triangle[corner].st[component] =
                    source.textureCoordinates[component];
            }
            /* Determinized lanes; see ORIGINAL_BINARY_BUG above. */
            triangle[corner].lightmapCoords[0] = 0.0f;
            triangle[corner].lightmapCoords[1] = 0.0f;
            triangle[corner].modulate[3] = 255;
        }
        RE_AddPolyToScene(shaderHandle, 3, triangle);
    }
}

/* Source: CoDUOMP.exe 0x004a46e0..0x004a491f, recovered after repairing the
 * missing Ghidra function boundary. This is the type-15 decal polygon path. */
void CDecal::Draw()
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    polyVert_t vertices[FX_DECAL_POINT_CAPACITY];
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)pointCount > (uint32_t)FX_DECAL_POINT_CAPACITY) {
        SFxHelper_Print("^1CDecal::Draw: invalid point count %i\n", pointCount);
        return;
    }
    for (int32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        for (int component = 0; component < 3; ++component) {
            vertices[pointIndex].xyz[component] =
                origin[component] + points[pointIndex][component];
        }
        for (int component = 0; component < 2; ++component) {
            vertices[pointIndex].st[component] =
                textureCoordinates[pointIndex][component];
        }
        /* Determinized lane; see ORIGINAL_BINARY_BUG above. */
        vertices[pointIndex].lightmapCoords[0] = 0.0f;
        vertices[pointIndex].lightmapCoords[1] = 0.0f;
        for (int component = 0; component < 4; ++component) {
            vertices[pointIndex].modulate[component] =
                renderEntity.shaderRGBA[component];
        }
    }
    RE_AddPolyToScene(renderEntity.spriteShaderHandle,
                      pointCount, vertices);
}

/* Source: CoDUOMP.exe 0x004a4e80..0x004a4fe3.
 * Name and class binding: Windows vtable 0x005a2bcc and same-module Mac symbol
 * CFlash::Draw. The inherited light RGB output is clamped into sprite color,
 * and the flash is submitted eight units in front of the FX camera. */
void CFlash::Draw()
{
    renderEntity.reType = RT_SPRITE;
    for (int component = 0; component < 3; ++component) {
        float &color = renderEntity.lightingOrigin[component];
        if (color > 1.0f) {
            color = 1.0f;
        } else if (color < 0.0f) {
            color = 0.0f;
        }
        renderEntity.shaderRGBA[component] =
            static_cast<uint8_t>(coduo_fp_to_i32_extended(
                static_cast<long double>(color) * 255.0L));
    }
    renderEntity.shaderRGBA[3] = 255;

    for (int component = 0; component < 3; ++component) {
        renderEntity.origin[component] =
            fxViewOrigin[component] +
            fxCullPlanes[0].normal[component] * 8.0f;
    }
    renderEntity.backlerp = 12.0f;
    renderEntity.radius = 12.0f;
    SFxHelper_AddFxToScene(&renderEntity);
}

/* Source: CoDUOMP.exe 0x004a28d0..0x004a28fe.
 * Name: same-module Mac symbol CElectricity::Draw. */
void CElectricity::Draw()
{
    renderEntity.electricity.parameter = electricityParm;
    renderEntity.electricity.lifeTime =
        static_cast<float>(timeEnd - timeStart);
    SFxHelper_AddFxToScene(&renderEntity);
}
