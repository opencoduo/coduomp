#include "backend.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_native_x87.h"
#include "../math/vector_math.h"

#include <math.h>
#include <string.h>

#define RB_SPRITE_RADIANS_PER_DEGREE \
    0.01745329238474369f /* 0x3c8efa35 */
#define RB_AXIS_LENGTH        16.0f
#define RB_AXIS_LINE_WIDTH     3.0f
#define RB_DEFAULT_LINE_WIDTH  1.0f
#define RB_RAIL_TEXTURE_SCALE  0.00390625f /* 0x3b800000, 1 / 256 */
#define RB_RAIL_DISC_RADIUS_SCALE 0.25f
#define RB_RAIL_DISC_START_ANGLE_DEGREES 45
#define RB_RAIL_DISC_ANGLE_STEP_DEGREES 90
#define RB_RAIL_DISC_POINT_COUNT 4
#define RB_PI_F 3.1415927410125732f /* 0x40490fdb */
#define RB_INV_180_F \
    0.0055555556900799274f /* 0x3bb60b61, 1 / 180 */
#define RB_LIGHTNING_CORE_WIDTH 8.0f
#define RB_LIGHTNING_PLANE_COUNT 4
#define RB_LIGHTNING_ROTATION_DEGREES 45.0f
#define RB_CYLINDER_MIN_SIDE_COUNT 8
#define RB_CYLINDER_MAX_SIDE_COUNT 32
#define RB_CYLINDER_INV_90 \
    0.011111111380159855f /* 0x3c360b61, 1 / 90 */
#define RB_CYLINDER_INV_1024 0.0009765625f /* 0x3a800000, 1 / 1024 */
#define RB_CYLINDER_TWO_PI \
    6.2831854820251465f /* 0x40c90fdb, 2 * pi as float */
#define RB_BEAM_SEGMENT_COUNT 6
#define RB_BEAM_RADIUS 4.0f
#define RB_BEAM_DEGREES_PER_SEGMENT 60.0f

/* Source: CoDUOMP.exe 0x004f3c00..0x004f3c10.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3c00_004f3c11.mcode and the
 * rb_surfaceTable initializer at 0x005ce970.
 * Name: exact same-module Mac symbol RB_SurfaceBad. Surface-table slot zero
 * calls the renderer fatal-error import with the original diagnostic. */
void RB_SurfaceBad(renderer_surface_t *surface)
{
    (void)surface;
    ri.Error(ERR_FATAL, "Bad surface tesselated.\n");
}

/* Source: CoDUOMP.exe 0x004f3c20.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3c20_004f3c21.mcode and the
 * rb_surfaceTable initializer at 0x005ce970.
 * Name: exact same-module Mac symbol RB_SurfaceSkip. Surface-table slot one
 * deliberately consumes no data and emits no geometry. */
void RB_SurfaceSkip(renderer_surface_t *surface)
{
    (void)surface;
}

/* Source: CoDUOMP.exe 0x004f1400..0x004f1483.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f1400_004f1484.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceSplash. Renderer-entity type 5
 * selects this body in RB_SurfaceEntity. MSVC inlines the separately emitted
 * RB_AddQuadStamp wrapper here and calls RB_AddQuadStampExt directly.
 *
 * The initial negation and optional mirror negation are kept as two operations
 * because they also preserve the original signed-zero result for a zero-radius
 * entity. */
void RB_SurfaceSplash(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    vec3_t left = {-entity->radius, 0.0f, 0.0f};
    vec3_t up = {0.0f, entity->radius, 0.0f};

    if (backEnd.viewParms.isMirror != qfalse) {
        left[0] = -left[0];
    }

    RB_AddQuadStampExt(entity->origin, left, up, entity->shaderRGBA,
                       0.0f, 0.0f, 1.0f, 1.0f);
}

/* Source: CoDUOMP.exe 0x004f1490..0x004f1661.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f1490_004f1662.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceSprite. Renderer-entity type 4
 * selects this body. A zero rotation keeps the camera right/up basis; a
 * nonzero rotation applies the entity's authored clockwise screen rotation.
 * MSVC again inlines RB_AddQuadStamp. */
void RB_SurfaceSprite(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    const vec3_t *const viewAxis = backEnd.viewParms.orientation.axis;
    vec3_t left;
    vec3_t up;

    if (entity->rotation == 0.0f) {
        for (int32_t component = 0; component < 3; ++component) {
            left[component] =
                viewAxis[1][component] * entity->radius;
            up[component] =
                viewAxis[2][component] * entity->radius2;
        }
    } else {
        const float radians =
            entity->rotation * RB_SPRITE_RADIANS_PER_DEGREE;
        float cosine;
        float sine;

        coduo_x87_sincosf(radians, &sine, &cosine);
        const long double leftCosineRaw =
            (long double)cosine * (long double)entity->radius;
        const long double leftNegativeSineRaw =
            -(long double)sine * (long double)entity->radius;
        const float leftNegativeSine = (float)leftNegativeSineRaw;
        const long double upCosineRaw =
            (long double)cosine * (long double)entity->radius2;
        const long double upSineRaw =
            (long double)sine * (long double)entity->radius2;
        const float upSine = (float)upSineRaw;

        /* 0x004f1543..0x004f15a8 retains cosine*radius for all three basis
         * products. The negative-sine product remains live only for X after
         * its 0x004f1579 float store; Y/Z reload the rounded copy. */
        left[0] = (float)(
            leftNegativeSineRaw * (long double)viewAxis[2][0] +
            leftCosineRaw * (long double)viewAxis[1][0]);
        left[1] = (float)(
            (long double)leftNegativeSine * (long double)viewAxis[2][1] +
            (long double)(float)(leftCosineRaw *
                                 (long double)viewAxis[1][1]));
        left[2] = (float)(
            (long double)leftNegativeSine * (long double)viewAxis[2][2] +
            (long double)(float)(leftCosineRaw *
                                 (long double)viewAxis[1][2]));

        /* 0x004f15ac..0x004f160e is the matching radius2 chain. */
        up[0] = (float)(
            upSineRaw * (long double)viewAxis[1][0] +
            upCosineRaw * (long double)viewAxis[2][0]);
        up[1] = (float)(
            (long double)upSine * (long double)viewAxis[1][1] +
            (long double)(float)(upCosineRaw *
                                 (long double)viewAxis[2][1]));
        up[2] = (float)(
            (long double)upSine * (long double)viewAxis[1][2] +
            (long double)(float)(upCosineRaw *
                                 (long double)viewAxis[2][2]));
    }

    if (backEnd.viewParms.isMirror != qfalse) {
        for (int32_t component = 0; component < 3; ++component) {
            left[component] = -left[component];
        }
    }

    RB_AddQuadStampExt(entity->origin, left, up, entity->shaderRGBA,
                       0.0f, 0.0f, 1.0f, 1.0f);
}

/* Source: CoDUOMP.exe 0x004f1670..0x004f184f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f1670_004f1850.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceOrientedQuad. Renderer-entity
 * type 12 selects this body. Its forward axis supplies an orthogonal local
 * right/up basis; the remaining rotation, scale, mirror, and stamp operations
 * match RB_SurfaceSprite. */
void RB_SurfaceOrientedQuad(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    vec3_t basisRight;
    vec3_t basisUp;
    vec3_t left;
    vec3_t up;

    MakeNormalVectors(entity->axis[0], basisRight, basisUp);

    if (entity->rotation == 0.0f) {
        for (int32_t component = 0; component < 3; ++component) {
            left[component] =
                basisRight[component] * entity->radius;
            up[component] =
                basisUp[component] * entity->radius2;
        }
    } else {
        const float radians =
            entity->rotation * RB_SPRITE_RADIANS_PER_DEGREE;
        float cosine;
        float sine;

        coduo_x87_sincosf(radians, &sine, &cosine);
        const long double leftCosineRaw =
            (long double)cosine * (long double)entity->radius;
        const long double leftNegativeSineRaw =
            -(long double)sine * (long double)entity->radius;
        const float leftNegativeSine = (float)leftNegativeSineRaw;
        const long double upCosineRaw =
            (long double)cosine * (long double)entity->radius2;
        const long double upSineRaw =
            (long double)sine * (long double)entity->radius2;
        const float upSine = (float)upSineRaw;

        /* 0x004f1732..0x004f178a mirrors the sprite precision chain against
         * the generated local basis. */
        left[0] = (float)(
            leftNegativeSineRaw * (long double)basisUp[0] +
            leftCosineRaw * (long double)basisRight[0]);
        left[1] = (float)(
            (long double)leftNegativeSine * (long double)basisUp[1] +
            (long double)(float)(leftCosineRaw *
                                 (long double)basisRight[1]));
        left[2] = (float)(
            (long double)leftNegativeSine * (long double)basisUp[2] +
            (long double)(float)(leftCosineRaw *
                                 (long double)basisRight[2]));

        /* 0x004f178e..0x004f17ec retains upSine only for X after its
         * 0x004f17bc float store. */
        up[0] = (float)(
            upSineRaw * (long double)basisRight[0] +
            upCosineRaw * (long double)basisUp[0]);
        up[1] = (float)(
            (long double)upSine * (long double)basisRight[1] +
            (long double)(float)(upCosineRaw *
                                 (long double)basisUp[1]));
        up[2] = (float)(
            (long double)upSine * (long double)basisRight[2] +
            (long double)(float)(upCosineRaw *
                                 (long double)basisUp[2]));
    }

    if (backEnd.viewParms.isMirror != qfalse) {
        for (int32_t component = 0; component < 3; ++component) {
            left[component] = -left[component];
        }
    }

    RB_AddQuadStampExt(entity->origin, left, up, entity->shaderRGBA,
                       0.0f, 0.0f, 1.0f, 1.0f);
}

/* Source: CoDUOMP.exe 0x004f1ab0..0x004f22c6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f1ab0_004f22c7.mcode.
 * Name and source-level vector, GL-state, and immediate-mode calls: exact
 * same-module Mac symbol RB_SurfaceBeam. MSVC inlines the immediate-mode
 * color, begin, vertex, end, and cleanup helpers, which accounts for most of
 * the much larger Windows body.
 *
 * Both rings intentionally remain in beam-local coordinates: one is centered
 * at zero and the other at oldorigin-origin. This body itself applies no
 * entity-origin translation. */
void RB_SurfaceBeam(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    vec3_t beamDelta;
    vec3_t direction;
    vec3_t perpendicular;
    vec3_t startPoints[RB_BEAM_SEGMENT_COUNT];
    vec3_t endPoints[RB_BEAM_SEGMENT_COUNT];

    for (int32_t component = 0; component < 3; ++component) {
        beamDelta[component] =
            entity->oldorigin[component] - entity->origin[component];
        direction[component] = beamDelta[component];
    }
    if (VectorNormalize(direction) == 0.0f)
        return;

    PerpendicularVector(perpendicular, direction);
    for (int32_t component = 0; component < 3; ++component) {
        perpendicular[component] *= RB_BEAM_RADIUS;
    }

    for (int32_t segmentIndex = 0;
         segmentIndex < RB_BEAM_SEGMENT_COUNT; ++segmentIndex) {
        RotatePointAroundVector(
            startPoints[segmentIndex], direction, perpendicular,
            (float)segmentIndex * RB_BEAM_DEGREES_PER_SEGMENT);
        for (int32_t component = 0; component < 3; ++component) {
            endPoints[segmentIndex][component] =
                beamDelta[component] + startPoints[segmentIndex][component];
        }
    }

    GL_Bind(tr.whiteImage);
    GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
    RB_BeginImmediateMode();
    RB_glColor3f(1.0f, 0.0f, 0.0f);
    RB_glBegin(GL_TRIANGLE_STRIP);
    for (int32_t segmentIndex = 0;
         segmentIndex <= RB_BEAM_SEGMENT_COUNT; ++segmentIndex) {
        const int32_t wrappedIndex =
            segmentIndex % RB_BEAM_SEGMENT_COUNT;

        RB_glVertex3fv(startPoints[wrappedIndex]);
        RB_glVertex3fv(endPoints[wrappedIndex]);
    }
    RB_glEnd();
    RB_EndImmediateMode();
}

/* Source: CoDUOMP.exe 0x004f1850..0x004f1aa2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f1850_004f1aa3.mcode and
 * rb_surfaceTable[2] at 0x005ce978.
 * Name and source-level RB_CheckOverflow call: exact same-module Mac symbol
 * RB_SurfacePolychain. The source vertex is the public 32-byte polyVert_t:
 * XYZ, base ST, lightmap ST, then four packed color bytes. */
void RB_SurfacePolychain(renderer_surface_t *surfaceData)
{
    srfPoly_t *surface = (srfPoly_t *)surfaceData;
    const int32_t vertexCount = surface->numVerts;

    RB_CheckOverflow(vertexCount, (vertexCount - 2) * 3);

    /* The original reloads tess.vertexCount at 0x004f188f, after the
     * RB_CheckOverflow flush path returns, so a mid-batch flush rebases this
     * surface at the fresh batch start. A pre-call snapshot writes the poly at
     * stale offsets and emits wrapped fan indexes after a flush. */
    const int32_t baseVertex = tess.vertexCount;

    for (int32_t vertexIndex = 0;
         vertexIndex < vertexCount; ++vertexIndex) {
        const polyVert_t *const source =
            &surface->verts[vertexIndex];
        const int32_t destinationVertex = baseVertex + vertexIndex;
        const int32_t xyzOffset =
            destinationVertex * tess.vertexComponentCount;

        for (int32_t component = 0; component < 3; ++component) {
            tess.xyz[xyzOffset + component] = source->xyz[component];
        }
        for (int32_t component = 0; component < 2; ++component) {
            tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                          [destinationVertex][component] =
                source->st[component];
            tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                          [destinationVertex][component] =
                source->lightmapCoords[component];
        }
        memcpy(&tess.vertexColors[destinationVertex], source->modulate,
               sizeof(tess.vertexColors[destinationVertex]));
    }

    if (tr.refdef.num_dlights > 0 &&
        (tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) != 0) {
        for (int32_t lightIndex = 0;
             lightIndex < tr.refdef.num_dlights; ++lightIndex) {
            const uint32_t lightBit =
                1u << (uint32_t)lightIndex;

            if ((tess.dlightBits & lightBit) != 0)
                continue;

            const renderer_light_t *const light =
                &tr.refdef.dlights[lightIndex];
            const float radiusSquared = light->radius * light->radius;
            for (int32_t vertexIndex = 0;
                 vertexIndex < vertexCount; ++vertexIndex) {
                const vec3_t *const position =
                    &surface->verts[vertexIndex].xyz;
                const float differenceX =
                    (*position)[0] - light->transformedPosition[0];
                const float differenceY =
                    (*position)[1] - light->transformedPosition[1];
                const float differenceZ =
                    (*position)[2] - light->transformedPosition[2];
                const float distanceSquared =
                    (differenceZ * differenceZ +
                     differenceY * differenceY) +
                    differenceX * differenceX;

                if (distanceSquared < radiusSquared) {
                    tess.dlightBits |= lightBit;
                    break;
                }
            }
        }
    }

    for (int32_t triangleIndex = 0;
         triangleIndex < vertexCount - 2; ++triangleIndex) {
        tess.indexes[tess.indexCount + 0] = (uint16_t)baseVertex;
        tess.indexes[tess.indexCount + 1] =
            (uint16_t)(baseVertex + triangleIndex + 1);
        tess.indexes[tess.indexCount + 2] =
            (uint16_t)(baseVertex + triangleIndex + 2);
        tess.indexCount += 3;
    }
    tess.vertexCount = baseVertex + vertexCount;
}

/* Source: CoDUOMP.exe 0x004f3a00..0x004f3b3f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3a00_004f3b40.mcode.
 * Name and source-level helper calls: exact same-module Mac symbol
 * RB_SurfaceAxis. MSVC inlines RB_glLineWidth, RB_glBegin, RB_glColor3f,
 * RB_glEnd, and RB_EndImmediateMode while retaining RB_glVertex3f calls. */
void RB_SurfaceAxis(void)
{
    RB_BeginImmediateMode();
    GL_Bind(tr.whiteImage);
    RB_glLineWidth(RB_AXIS_LINE_WIDTH);
    RB_glBegin(GL_LINES);

    RB_glColor3f(1.0f, 0.0f, 0.0f);
    RB_glVertex3f(0.0f, 0.0f, 0.0f);
    RB_glVertex3f(RB_AXIS_LENGTH, 0.0f, 0.0f);

    RB_glColor3f(0.0f, 1.0f, 0.0f);
    RB_glVertex3f(0.0f, 0.0f, 0.0f);
    RB_glVertex3f(0.0f, RB_AXIS_LENGTH, 0.0f);

    RB_glColor3f(0.0f, 0.0f, 1.0f);
    RB_glVertex3f(0.0f, 0.0f, 0.0f);
    RB_glVertex3f(0.0f, 0.0f, RB_AXIS_LENGTH);

    RB_glEnd();
    RB_glLineWidth(RB_DEFAULT_LINE_WIDTH);
    RB_EndImmediateMode();
}

/* Source: CoDUOMP.exe 0x004f22d0..0x004f2504.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f22d0_004f2505.mcode.
 * Name and source-level RB_CheckOverflow call: exact same-module Mac symbol
 * DoLine. The side vector is already normalized; radius supplies the signed
 * offset on both sides of the two endpoints. */
static void DoLine(const vec3_t start, const vec3_t end,
                   const vec3_t side, float radius)
{
    uint32_t packedColor;
    int32_t baseVertex;
    int32_t xyzOffset;

    RB_CheckOverflow(4, 6);
    baseVertex = tess.vertexCount;
    xyzOffset = baseVertex * tess.vertexComponentCount;

    for (int32_t component = 0; component < 3; ++component) {
        tess.xyz[xyzOffset + component] =
            start[component] + radius * side[component];
    }
    xyzOffset += tess.vertexComponentCount;
    for (int32_t component = 0; component < 3; ++component) {
        tess.xyz[xyzOffset + component] =
            start[component] - radius * side[component];
    }
    xyzOffset += tess.vertexComponentCount;
    for (int32_t component = 0; component < 3; ++component) {
        tess.xyz[xyzOffset + component] =
            end[component] + radius * side[component];
    }
    xyzOffset += tess.vertexComponentCount;
    for (int32_t component = 0; component < 3; ++component) {
        tess.xyz[xyzOffset + component] =
            end[component] - radius * side[component];
    }

    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][0] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][1] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][0] = 1.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][1] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][0] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][1] = 1.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][0] = 1.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][1] = 1.0f;

    memcpy(&packedColor, backEnd.currentEntity->e.shaderRGBA,
           sizeof(packedColor));
    tess.vertexColors[baseVertex + 0] = packedColor;
    tess.vertexColors[baseVertex + 1] = packedColor;
    tess.vertexColors[baseVertex + 2] = packedColor;
    tess.vertexColors[baseVertex + 3] = packedColor;

    tess.indexes[tess.indexCount + 0] = (uint16_t)(baseVertex + 0);
    tess.indexes[tess.indexCount + 1] = (uint16_t)(baseVertex + 1);
    tess.indexes[tess.indexCount + 2] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 4] = (uint16_t)(baseVertex + 1);
    tess.indexes[tess.indexCount + 5] = (uint16_t)(baseVertex + 3);

    tess.indexCount += 6;
    tess.vertexCount += 4;
}

/* Source: CoDUOMP.exe 0x004f2510..0x004f2601.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f2510_004f2602.mcode.
 * Name and source-level CrossProduct, VectorNormalize, and DoLine calls:
 * exact same-module Mac symbol RB_SurfaceLine. */
void RB_SurfaceLine(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    vec3_t start;
    vec3_t end;
    vec3_t startFromView;
    vec3_t endFromView;
    vec3_t side;

    for (int32_t component = 0; component < 3; ++component) {
        start[component] = entity->origin[component];
        end[component] = entity->oldorigin[component];
        startFromView[component] =
            start[component] -
            backEnd.viewParms.orientation.origin[component];
        endFromView[component] =
            end[component] -
            backEnd.viewParms.orientation.origin[component];
    }

    CrossProduct(startFromView, endFromView, side);
    (void)VectorNormalize(side);
    DoLine(start, end, side, entity->radius);
}

/* Source: CoDUOMP.exe 0x004f2610..0x004f2c9a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f2610_004f2c9b.mcode.
 * Name and source-level vector, trigonometric, and overflow calls: exact
 * same-module Mac symbol RB_SurfaceCylinder.
 *
 * The projected-size expression retains the executable's separate exact
 * float constants and FastRound conversion. The endpoint at entity.origin
 * uses radius2 (+0x64); entity.oldorigin uses radius (+0x7c). */
void RB_SurfaceCylinder(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    vec3_t midpointFromView;
    vec3_t direction;
    vec3_t right;
    vec3_t up;
    vec3_t originRing[RB_CYLINDER_MAX_SIDE_COUNT + 1];
    vec3_t oldOriginRing[RB_CYLINDER_MAX_SIDE_COUNT + 1];
    uint32_t packedColor;
    float distance;
    float sideFraction;
    float angleStep;
    int32_t sideCount;

    for (int32_t component = 0; component < 3; ++component) {
        midpointFromView[component] =
            (entity->origin[component] + entity->oldorigin[component]) *
                0.5f -
            backEnd.viewParms.orientation.origin[component];
    }
    distance = VectorNormalize(midpointFromView);

    sideCount = FastRound(
        (1.0f -
         distance *
             (backEnd.viewParms.fovX * RB_CYLINDER_INV_90) *
             RB_CYLINDER_INV_1024) *
        (float)RB_CYLINDER_MAX_SIDE_COUNT);
    if (sideCount < RB_CYLINDER_MIN_SIDE_COUNT)
        sideCount = RB_CYLINDER_MIN_SIDE_COUNT;
    else if (sideCount > RB_CYLINDER_MAX_SIDE_COUNT)
        sideCount = RB_CYLINDER_MAX_SIDE_COUNT;

    RB_CheckOverflow(sideCount * 2 + 2, sideCount * 6);

    /* The original re-reads tess.vertexCount after the RB_CheckOverflow call
     * (fresh loads of 0x04844d88 at 0x004f28c0..0x004f2c8d for every index
     * word, the vertex loop base, and the final count), so a mid-batch flush
     * rebases this surface at the fresh batch start. */
    const int32_t baseVertex = tess.vertexCount;

    for (int32_t component = 0; component < 3; ++component) {
        direction[component] =
            entity->origin[component] - entity->oldorigin[component];
    }
    (void)VectorNormalize(direction);
    MakeNormalVectors(direction, right, up);

    /* 0x004f2752..0x004f2772 stores the reciprocal as sideFraction but
     * multiplies the retained x87 quotient by 2*pi for angleStep. */
    const long double sideFractionRaw =
        1.0L / (long double)sideCount;
    sideFraction = (float)sideFractionRaw;
    angleStep = (float)(
        sideFractionRaw * (long double)RB_CYLINDER_TWO_PI);
    (void)sideFraction;
    for (int32_t sideIndex = 0; sideIndex < sideCount; ++sideIndex) {
        const float angle = (float)sideIndex * angleStep;
        float sine;
        float cosine;

        coduo_x87_sincosf(angle, &sine, &cosine);

        for (int32_t component = 0; component < 3; ++component) {
            originRing[sideIndex][component] =
                entity->origin[component] +
                up[component] * (cosine * entity->radius2) +
                right[component] * (sine * entity->radius2);
            oldOriginRing[sideIndex][component] =
                entity->oldorigin[component] +
                up[component] * (cosine * entity->radius) +
                right[component] * (sine * entity->radius);
        }
    }
    for (int32_t component = 0; component < 3; ++component) {
        originRing[sideCount][component] = originRing[0][component];
        oldOriginRing[sideCount][component] = oldOriginRing[0][component];
    }

    for (int32_t sideIndex = 0; sideIndex < sideCount; ++sideIndex) {
        const int32_t firstVertex = baseVertex + sideIndex * 2;

        tess.indexes[tess.indexCount + 0] = (uint16_t)(firstVertex + 0);
        tess.indexes[tess.indexCount + 1] = (uint16_t)(firstVertex + 1);
        tess.indexes[tess.indexCount + 2] = (uint16_t)(firstVertex + 3);
        tess.indexes[tess.indexCount + 3] = (uint16_t)(firstVertex + 3);
        tess.indexes[tess.indexCount + 4] = (uint16_t)(firstVertex + 2);
        tess.indexes[tess.indexCount + 5] = (uint16_t)(firstVertex + 0);
        tess.indexCount += 6;
    }

    memcpy(&packedColor, entity->shaderRGBA, sizeof(packedColor));
    for (int32_t sideIndex = 0; sideIndex <= sideCount; ++sideIndex) {
        const int32_t originVertex = baseVertex + sideIndex * 2;
        const int32_t oldOriginVertex = originVertex + 1;
        /* 0x004f29c4..0x004f2c3c multiplies every emitted S coordinate by
         * the stored angular step at stack slot +0x14, not sideFraction. */
        const float textureS = (float)sideIndex * angleStep;
        int32_t xyzOffset =
            originVertex * tess.vertexComponentCount;

        for (int32_t component = 0; component < 3; ++component) {
            tess.xyz[xyzOffset + component] =
                originRing[sideIndex][component];
        }
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][originVertex][0] =
            textureS;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][originVertex][1] = 1.0f;
        tess.vertexColors[originVertex] = packedColor;

        xyzOffset += tess.vertexComponentCount;
        for (int32_t component = 0; component < 3; ++component) {
            tess.xyz[xyzOffset + component] =
                oldOriginRing[sideIndex][component];
        }
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][oldOriginVertex][0] =
            textureS;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][oldOriginVertex][1] = 0.0f;
        tess.vertexColors[oldOriginVertex] = packedColor;
    }

    tess.vertexCount = baseVertex + sideCount * 2 + 2;
}

/* Source: CoDUOMP.exe 0x004f2ca0..0x004f2e86.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f2ca0_004f2e87.mcode.
 * Name: exact same-module Mac symbol DoRailCore. Unlike DoLine, this original
 * helper relies on its callers' surface capacity and does not invoke
 * RB_CheckOverflow. */
static void DoRailCore(const vec3_t start, const vec3_t end,
                       const vec3_t side, float length, float width)
{
    const float textureLength = length * RB_RAIL_TEXTURE_SCALE;
    uint32_t packedColor;
    int32_t baseVertex = tess.vertexCount;
    int32_t xyzOffset = baseVertex * tess.vertexComponentCount;

    for (int32_t component = 0; component < 3; ++component) {
        tess.xyz[xyzOffset + component] =
            start[component] + width * side[component];
    }
    xyzOffset += tess.vertexComponentCount;
    for (int32_t component = 0; component < 3; ++component) {
        tess.xyz[xyzOffset + component] =
            start[component] - width * side[component];
    }
    xyzOffset += tess.vertexComponentCount;
    for (int32_t component = 0; component < 3; ++component) {
        tess.xyz[xyzOffset + component] =
            end[component] + width * side[component];
    }
    xyzOffset += tess.vertexComponentCount;
    for (int32_t component = 0; component < 3; ++component) {
        tess.xyz[xyzOffset + component] =
            end[component] - width * side[component];
    }

    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][0] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][1] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][0] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][1] = 1.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][0] =
        textureLength;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][1] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][0] =
        textureLength;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][1] = 1.0f;

    memcpy(&packedColor, backEnd.currentEntity->e.shaderRGBA,
           sizeof(packedColor));
    tess.vertexColors[baseVertex + 0] = packedColor;
    tess.vertexColors[baseVertex + 1] = packedColor;
    tess.vertexColors[baseVertex + 2] = packedColor;
    tess.vertexColors[baseVertex + 3] = packedColor;

    tess.indexes[tess.indexCount + 0] = (uint16_t)(baseVertex + 0);
    tess.indexes[tess.indexCount + 1] = (uint16_t)(baseVertex + 1);
    tess.indexes[tess.indexCount + 2] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 4] = (uint16_t)(baseVertex + 1);
    tess.indexes[tess.indexCount + 5] = (uint16_t)(baseVertex + 3);

    tess.indexCount += 6;
    tess.vertexCount += 4;
}

/* Source: CoDUOMP.exe 0x004f2e90..0x004f31b0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f2e90_004f31b1.mcode.
 * Name and source-level RB_CheckOverflow calls: exact same-module Mac symbol
 * DoRailDiscs. The Windows FSINCOS consumes one float-rounded radians value;
 * the Mac build exposes the original separate sin/cos source calls. */
static void DoRailDiscs(int32_t segmentCount, const vec3_t start,
                        const vec3_t step, const vec3_t right,
                        const vec3_t up)
{
    const float railRadius =
        (float)r_railWidth->integer * RB_RAIL_DISC_RADIUS_SCALE;
    vec3_t positions[RB_RAIL_DISC_POINT_COUNT];
    uint32_t packedColor;

    if (segmentCount > 1)
        --segmentCount;
    if (segmentCount == 0)
        return;

    for (int32_t pointIndex = 0,
                 angleDegrees = RB_RAIL_DISC_START_ANGLE_DEGREES;
         pointIndex < RB_RAIL_DISC_POINT_COUNT;
         ++pointIndex, angleDegrees += RB_RAIL_DISC_ANGLE_STEP_DEGREES) {
        const float radians =
            (float)angleDegrees * RB_PI_F * RB_INV_180_F;
        float sine;
        float cosine;

        coduo_x87_sincosf(radians, &sine, &cosine);

        for (int32_t component = 0; component < 3; ++component) {
            positions[pointIndex][component] =
                (cosine * right[component] + sine * up[component]) *
                    railRadius +
                start[component];
        }

        if (segmentCount > 1) {
            for (int32_t component = 0; component < 3; ++component) {
                positions[pointIndex][component] += step[component];
            }
        }
    }

    memcpy(&packedColor, backEnd.currentEntity->e.shaderRGBA,
           sizeof(packedColor));

    while (segmentCount > 0) {
        int32_t baseVertex;
        int32_t xyzOffset;

        RB_CheckOverflow(4, 6);
        baseVertex = tess.vertexCount;
        xyzOffset = baseVertex * tess.vertexComponentCount;

        for (int32_t pointIndex = 0;
             pointIndex < RB_RAIL_DISC_POINT_COUNT; ++pointIndex) {
            for (int32_t component = 0; component < 3; ++component) {
                tess.xyz[xyzOffset + component] =
                    positions[pointIndex][component];
                positions[pointIndex][component] += step[component];
            }
            xyzOffset += tess.vertexComponentCount;

            tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                          [baseVertex + pointIndex][0] =
                pointIndex < 2 ? 1.0f : 0.0f;
            tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                          [baseVertex + pointIndex][1] =
                pointIndex == 0 || pointIndex == 3 ? 0.0f : 1.0f;
            tess.vertexColors[baseVertex + pointIndex] = packedColor;
        }

        tess.indexes[tess.indexCount + 0] = (uint16_t)(baseVertex + 0);
        tess.indexes[tess.indexCount + 1] = (uint16_t)(baseVertex + 1);
        tess.indexes[tess.indexCount + 2] = (uint16_t)(baseVertex + 3);
        tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 3);
        tess.indexes[tess.indexCount + 4] = (uint16_t)(baseVertex + 1);
        tess.indexes[tess.indexCount + 5] = (uint16_t)(baseVertex + 2);

        tess.indexCount += 6;
        tess.vertexCount += 4;
        --segmentCount;
    }
}

/* Source: CoDUOMP.exe 0x004f31c0..0x004f3283.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f31c0_004f3284.mcode.
 * Name and source-level VectorNormalize, MakeNormalVectors, and DoRailDiscs
 * calls: exact same-module Mac symbol RB_SurfaceRailRings. The segment ratio
 * remains retained through the original _ftol2 conversion; only its low
 * dword becomes the signed segment count. */
void RB_SurfaceRailRings(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    vec3_t start;
    vec3_t direction;
    vec3_t right;
    vec3_t up;
    vec3_t step;
    float length;
    int32_t segmentCount;

    for (int32_t component = 0; component < 3; ++component) {
        start[component] = entity->oldorigin[component];
        direction[component] =
            entity->origin[component] - start[component];
    }

    length = VectorNormalize(direction);
    MakeNormalVectors(direction, right, up);
    const long double rawSegmentCount =
        (long double)length / r_railSegmentLength->value;
    segmentCount = coduo_fp_to_i32_extended(rawSegmentCount);
    if (segmentCount <= 0)
        segmentCount = 1;

    for (int32_t component = 0; component < 3; ++component) {
        step[component] =
            direction[component] * r_railSegmentLength->value;
    }

    DoRailDiscs(segmentCount, start, step, right, up);
}

/* Source: CoDUOMP.exe 0x004f3290..0x004f33cf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3290_004f33d0.mcode.
 * Name and source-level vector/helper calls: exact same-module Mac symbol
 * RB_SurfaceRailCore. */
void RB_SurfaceRailCore(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    vec3_t start;
    vec3_t end;
    vec3_t direction;
    vec3_t startFromView;
    vec3_t endFromView;
    vec3_t side;
    float length;

    for (int32_t component = 0; component < 3; ++component) {
        start[component] = entity->oldorigin[component];
        end[component] = entity->origin[component];
        direction[component] = end[component] - start[component];
    }
    length = VectorNormalize(direction);

    for (int32_t component = 0; component < 3; ++component) {
        startFromView[component] =
            start[component] -
            backEnd.viewParms.orientation.origin[component];
        endFromView[component] =
            end[component] -
            backEnd.viewParms.orientation.origin[component];
    }
    (void)VectorNormalize(startFromView);
    (void)VectorNormalize(endFromView);
    CrossProduct(startFromView, endFromView, side);
    (void)VectorNormalize(side);

    DoRailCore(start, end, side, length, r_railCoreWidth->value);
}

/* Source: CoDUOMP.exe 0x004f33d0..0x004f3548.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f33d0_004f3549.mcode.
 * Name and source-level vector/helper calls: exact same-module Mac symbol
 * RB_SurfaceLightningBolt. The four ribbon planes use the original fixed
 * eight-unit width and cumulative 45-degree rotation. */
void RB_SurfaceLightningBolt(void)
{
    const refEntity_t *const entity = &backEnd.currentEntity->e;
    vec3_t start;
    vec3_t end;
    vec3_t direction;
    vec3_t startFromView;
    vec3_t endFromView;
    vec3_t side;
    vec3_t rotatedSide;
    float length;

    for (int32_t component = 0; component < 3; ++component) {
        start[component] = entity->origin[component];
        end[component] = entity->oldorigin[component];
        direction[component] = end[component] - start[component];
    }
    length = VectorNormalize(direction);

    for (int32_t component = 0; component < 3; ++component) {
        startFromView[component] =
            start[component] -
            backEnd.viewParms.orientation.origin[component];
        endFromView[component] =
            end[component] -
            backEnd.viewParms.orientation.origin[component];
    }
    (void)VectorNormalize(startFromView);
    (void)VectorNormalize(endFromView);
    CrossProduct(startFromView, endFromView, side);
    (void)VectorNormalize(side);

    for (int32_t planeIndex = 0;
         planeIndex < RB_LIGHTNING_PLANE_COUNT; ++planeIndex) {
        DoRailCore(start, end, side, length, RB_LIGHTNING_CORE_WIDTH);
        RotatePointAroundVector(rotatedSide, direction, side,
                                RB_LIGHTNING_ROTATION_DEGREES);
        for (int32_t component = 0; component < 3; ++component) {
            side[component] = rotatedSide[component];
        }
    }
}

/* Source: CoDUOMP.exe 0x004f3b40..0x004f3bc9 plus selector table
 * 0x004f3bcc..0x004f3bfb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3b40_004f3bca.mcode.
 * Name and target roles: exact same-module Mac symbols. Types 8 and 11 share
 * the ordinary default arm in the Windows selector table; type 14 retains the
 * shipped FX-port diagnostic instead of attempting to draw electricity. */
void RB_SurfaceEntity(renderer_surface_t *surface)
{
    (void)surface;
    switch (backEnd.currentEntity->e.reType) {
    case RT_SPRITE:
        RB_SurfaceSprite();
        break;
    case RT_SPLASH:
        RB_SurfaceSplash();
        break;
    case RT_BEAM:
        RB_SurfaceBeam();
        break;
    case RT_RAIL_CORE:
        RB_SurfaceRailCore();
        break;
    case RT_RAIL_RINGS:
        RB_SurfaceRailRings();
        break;
    case RT_LIGHTNING:
        RB_SurfaceLightningBolt();
        break;
    case RT_ORIENTED_QUAD:
        RB_SurfaceOrientedQuad();
        break;
    case RT_LINE:
        RB_SurfaceLine();
        break;
    case RT_ELECTRICITY:
        ri.Printf(R_PRINT_ALL, "FXPORT RT_ELECTRICITY TDB\n");
        break;
    case RT_CYLINDER:
        RB_SurfaceCylinder();
        break;
    default:
        RB_SurfaceAxis();
        break;
    }
}

#undef RB_DEFAULT_LINE_WIDTH
#undef RB_AXIS_LINE_WIDTH
#undef RB_AXIS_LENGTH
#undef RB_SPRITE_RADIANS_PER_DEGREE
#undef RB_LIGHTNING_ROTATION_DEGREES
#undef RB_LIGHTNING_PLANE_COUNT
#undef RB_LIGHTNING_CORE_WIDTH
#undef RB_INV_180_F
#undef RB_PI_F
#undef RB_RAIL_DISC_POINT_COUNT
#undef RB_RAIL_DISC_ANGLE_STEP_DEGREES
#undef RB_RAIL_DISC_START_ANGLE_DEGREES
#undef RB_RAIL_DISC_RADIUS_SCALE
#undef RB_RAIL_TEXTURE_SCALE
#undef RB_CYLINDER_TWO_PI
#undef RB_CYLINDER_INV_1024
#undef RB_CYLINDER_INV_90
#undef RB_CYLINDER_MAX_SIDE_COUNT
#undef RB_CYLINDER_MIN_SIDE_COUNT
#undef RB_BEAM_DEGREES_PER_SEGMENT
#undef RB_BEAM_RADIUS
#undef RB_BEAM_SEGMENT_COUNT
