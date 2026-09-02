#include "backend.h"

#include <math.h>
#include <string.h>

#include "../math/vector_math.h"
#include "compat/coduo_native_x87.h"
#include "gl_api.h"
#include "gl_state.h"

/* Source data: CoDUOMP.exe 0x005ce918..0x005ce958. The only PE reference is
 * R_RotateForViewer at 0x004e4163. This is the fixed id-renderer coordinate
 * conversion multiplied onto the camera model-view matrix. */
static const float rendererFlipMatrix[16] = {0.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, 1.0f, 0.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f};

/* 0x005b9b44 = float32 bits 0x3a83126f. The value is the uniform-axis-scale
 * tolerance used to select GL_RESCALE_NORMAL_EXT instead of GL_NORMALIZE. */
static const float rendererUniformScaleTolerance = 0.0010000000474974513f;

/* Original float32 constants used by the projection setup. The comments give
 * their mathematical roles; the literals preserve the PE values rather than
 * depending on a host M_PI definition.
 *   0x005b9d64 = pi / 360
 *   0x005b9d58 = pi / 2 */
static const float rendererDegreesToHalfRadians = 0.008726646192371845f;
static const float rendererHalfPi = 1.5707963705062866f;

/* Portal-camera constants from the PE. The comments state their source roles;
 * the decimal literals preserve the original float32 values on every host. */
static const float rendererPortalPlaneTolerance = 64.0f;
static const float rendererMillisecondsPerSecond = 1000.0f;
static const float rendererPortalOscillationRate = 0.003000000026077032f; /* cycles argument per renderer millisecond */
static const float rendererPortalOscillationAmplitude = 4.0f;
static const float rendererPortalPlaneEpsilon = 0.0010000000474974513f; /* 0x005b9b44 */
static const float rendererPortalInitialDistanceSquared = 100000000.0f; /* 0x4cbebc20 */

enum renderer_portal_clip_flag_e {
    R_PORTAL_CLIP_X_POSITIVE = 0x01,
    R_PORTAL_CLIP_X_NEGATIVE = 0x02,
    R_PORTAL_CLIP_Y_POSITIVE = 0x04,
    R_PORTAL_CLIP_Y_NEGATIVE = 0x08,
    R_PORTAL_CLIP_Z_POSITIVE = 0x10,
    R_PORTAL_CLIP_Z_NEGATIVE = 0x20,
    R_TRIANGLE_INDEX_COUNT = 3
};

/* Source: CoDUOMP.exe 0x004e3e50..0x004e4068.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3e50_004e4068.mcode.
 * Name: same-module Mac symbol R_RotateForModelEntity. The Windows register
 * inputs and the myGlMultMatrix call prove entity/viewParms/orientation roles.
 * The body keeps all three origin deltas live in x87 across the view-origin
 * dot products, compensates the stored results by the retained axis lengths,
 * and chooses the fixed-function OpenGL normal correction. */
void R_RotateForModelEntity(trRefEntity_t *entity, const viewParms_t *viewParms, orientationr_t *orientation)
{
    float entityMatrix[16];
    float axisLengths[3];
    long double viewDelta[3];

    memcpy(orientation->origin, entity->e.origin, sizeof(orientation->origin));
    memcpy(orientation->axis, entity->e.axis, sizeof(orientation->axis));

    entityMatrix[0] = orientation->axis[0][0];
    entityMatrix[1] = orientation->axis[0][1];
    entityMatrix[2] = orientation->axis[0][2];
    entityMatrix[3] = 0.0f;
    entityMatrix[4] = orientation->axis[1][0];
    entityMatrix[5] = orientation->axis[1][1];
    entityMatrix[6] = orientation->axis[1][2];
    entityMatrix[7] = 0.0f;
    entityMatrix[8] = orientation->axis[2][0];
    entityMatrix[9] = orientation->axis[2][1];
    entityMatrix[10] = orientation->axis[2][2];
    entityMatrix[11] = 0.0f;
    entityMatrix[12] = orientation->origin[0];
    entityMatrix[13] = orientation->origin[1];
    entityMatrix[14] = orientation->origin[2];
    entityMatrix[15] = 1.0f;

    myGlMultMatrix(entityMatrix, viewParms->world.modelMatrix, orientation->modelMatrix);

    viewDelta[0] = (long double)viewParms->orientation.origin[0] - orientation->origin[0];
    viewDelta[1] = (long double)viewParms->orientation.origin[1] - orientation->origin[1];
    viewDelta[2] = (long double)viewParms->orientation.origin[2] - orientation->origin[2];

    orientation->viewOrigin[0] =
        (float)((viewDelta[0] * orientation->axis[0][0] + viewDelta[2] * orientation->axis[0][2]) + viewDelta[1] * orientation->axis[0][1]);
    orientation->viewOrigin[1] =
        (float)((viewDelta[1] * orientation->axis[1][1] + viewDelta[0] * orientation->axis[1][0]) + viewDelta[2] * orientation->axis[1][2]);
    orientation->viewOrigin[2] =
        (float)((viewDelta[2] * orientation->axis[2][2] + viewDelta[1] * orientation->axis[2][1]) + viewDelta[0] * orientation->axis[2][0]);

    entity->normalizationTarget = 0;
    if (entity->e.nonNormalizedAxes == 0.0f)
        return;

    for (int32_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
        const vec3_t *axis = &entity->e.axis[axisIndex];
        const long double axisLengthRaw =
            sqrtl(((long double)(*axis)[2] * (long double)(*axis)[2] + (long double)(*axis)[1] * (long double)(*axis)[1]) +
                  (long double)(*axis)[0] * (long double)(*axis)[0]);

        /* 0x004e3fc6 stores the length for the later uniform-scale checks,
         * while the zero test and view-origin division use the retained x87
         * square root. */
        axisLengths[axisIndex] = (float)axisLengthRaw;
        if (axisLengthRaw != 0.0L) {
            orientation->viewOrigin[axisIndex] = (float)((long double)orientation->viewOrigin[axisIndex] / axisLengthRaw);
        }
    }

    entity->normalizationTarget = GL_NORMALIZE;
    if (glConfig.rescaleNormalAvailable == qfalse)
        return;

    if (fabsf(axisLengths[2] - axisLengths[0]) < rendererUniformScaleTolerance &&
        fabsf(axisLengths[2] - axisLengths[1]) < rendererUniformScaleTolerance) {
        entity->normalizationTarget = GL_RESCALE_NORMAL_EXT;
    }
}

/* Source: CoDUOMP.exe 0x004e4070..0x004e409f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e4070_004e409f.mcode.
 * Name: same-module Mac symbol R_RotateForEntity. Types 0..2 use the model
 * transform; all other render kinds inherit the already-built world
 * orientation at viewParms +0x7c. */
void R_RotateForEntity(trRefEntity_t *entity, const viewParms_t *viewParms, orientationr_t *orientation)
{
    if (entity->e.reType >= RT_BRUSH_MODEL && entity->e.reType <= RT_STATIC_MODEL) {
        R_RotateForModelEntity(entity, viewParms, orientation);
    } else {
        *orientation = viewParms->world;
    }
}

/* Source: CoDUOMP.exe 0x004e40a0..0x004e4200.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e40a0_004e4200.mcode.
 * Name: same-module Mac symbol R_RotateForViewer. The three translation terms
 * retain the PE's x87 multiplication/addition order and 53-bit intermediates
 * through the final negated float store. Plain float expressions make the
 * native arm64 build round the moving-camera dot products in binary32. */
void R_RotateForViewer(void)
{
    orientationr_t *orientation = &tr.orientation;
    const orientationr_t *viewer = &tr.viewParms.orientation;
    float viewerMatrix[16];

    memset(orientation, 0, sizeof(*orientation));
    memcpy(orientation->viewOrigin, viewer->origin, sizeof(orientation->viewOrigin));

    orientation->axis[0][0] = 1.0f;
    orientation->axis[1][1] = 1.0f;
    orientation->axis[2][2] = 1.0f;

    viewerMatrix[0] = viewer->axis[0][0];
    viewerMatrix[1] = viewer->axis[1][0];
    viewerMatrix[2] = viewer->axis[2][0];
    viewerMatrix[3] = 0.0f;
    viewerMatrix[4] = viewer->axis[0][1];
    viewerMatrix[5] = viewer->axis[1][1];
    viewerMatrix[6] = viewer->axis[2][1];
    viewerMatrix[7] = 0.0f;
    viewerMatrix[8] = viewer->axis[0][2];
    viewerMatrix[9] = viewer->axis[1][2];
    viewerMatrix[10] = viewer->axis[2][2];
    viewerMatrix[11] = 0.0f;
    viewerMatrix[12] =
        (float)(-(((long double)viewer->axis[0][2] * viewer->origin[2] + (long double)viewer->axis[0][1] * viewer->origin[1]) +
                  (long double)viewer->axis[0][0] * viewer->origin[0]));
    viewerMatrix[13] =
        (float)(-(((long double)viewer->axis[1][2] * viewer->origin[2] + (long double)viewer->axis[1][1] * viewer->origin[1]) +
                  (long double)viewer->axis[1][0] * viewer->origin[0]));
    viewerMatrix[14] =
        (float)(-(((long double)viewer->axis[2][2] * viewer->origin[2] + (long double)viewer->axis[2][1] * viewer->origin[1]) +
                  (long double)viewer->axis[2][0] * viewer->origin[0]));
    viewerMatrix[15] = 1.0f;

    myGlMultMatrix(viewerMatrix, rendererFlipMatrix, orientation->modelMatrix);
    tr.viewParms.world = *orientation;
}

/* Source: CoDUOMP.exe 0x004e44e0..0x004e460c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e44e0_004e460c.mcode.
 * Name: same-module Mac symbol R_SetupProjection. The Windows function builds
 * the renderer's infinite-far-plane OpenGL matrix. Both FPTAN inputs and
 * results remain live in x87; only the vertical span spills to float before
 * its reciprocal. The matrix is copied for depth-hacked entities, then only
 * that copy's near-plane term is replaced. */
void R_SetupProjection(void)
{
    float *projection = tr.viewParms.projectionMatrix;
    float *depthHackProjection = tr.viewParms.depthHackProjectionMatrix;
    long double yMax;
    long double yMin;
    long double xMax;
    long double xMin;
    long double width;
    long double xScale;
    long double yScale;
    float height;
    float zNear;

    SetFarClip();

    zNear = r_znear->value;
    yMax = coduo_x87_tanl((long double)tr.refdef.fov_y * (long double)rendererDegreesToHalfRadians);
    yMin = -yMax;
    xMax = coduo_x87_tanl((long double)tr.refdef.fov_x * (long double)rendererDegreesToHalfRadians);
    xMin = -xMax;
    width = xMax - xMin;
    height = (float)(yMax - yMin);

    projection[1] = 0.0f;
    projection[2] = 0.0f;
    projection[3] = 0.0f;
    projection[4] = 0.0f;
    projection[6] = 0.0f;
    projection[7] = 0.0f;
    projection[8] = 0.0f;
    projection[9] = 0.0f;
    projection[10] = -1.0f;
    projection[11] = -1.0f;
    projection[12] = 0.0f;
    projection[13] = 0.0f;
    projection[15] = 0.0f;

    xScale = 1.0L / width;
    projection[0] = (float)(xScale + xScale);
    projection[8] = (float)((xMin + xMax) * xScale);
    yScale = 1.0L / (long double)height;
    projection[5] = (float)(yScale + yScale);
    projection[9] = (float)((yMin + yMax) * yScale);
    projection[14] = -2.0f * zNear;

    memcpy(depthHackProjection, projection, sizeof(tr.viewParms.projectionMatrix));
    depthHackProjection[14] = -2.0f * r_znear_depthhack->value;
}

/* Source: CoDUOMP.exe 0x004e4610..0x004e48c2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e4610_004e48c2.mcode.
 * Name: same-module Mac symbol R_SetupFrustum. Plane order is left, right,
 * bottom, top. FSINCOS stores cosine at stack +0x00 and sine at +0x04;
 * 0x004e464e..0x004e4739 then multiplies the forward axis by sine and the
 * signed side axis by cosine (and 0x004e476b..0x004e484c does the same for
 * the up axis). The angle divide/multiply narrows immediately before FSINCOS;
 * each plane-distance dot narrows only at its destination. Each plane then
 * receives the original type and negative-component sign mask. */
void R_SetupFrustum(void)
{
    orientationr_t *viewer = &tr.viewParms.orientation;
    renderer_frustum_plane_t *frustum = tr.viewParms.frustum;
    const float horizontalHalfAngle = (float)(((long double)tr.viewParms.fovX / 180.0L) * (long double)rendererHalfPi);
    float horizontalSin;
    float horizontalCos;

    coduo_x87_sincosf(horizontalHalfAngle, &horizontalSin, &horizontalCos);

    for (int32_t component = 0; component < 3; ++component) {
        frustum[0].normal[component] = viewer->axis[0][component] * horizontalSin;
        frustum[0].normal[component] = (float)((long double)viewer->axis[1][component] * horizontalCos + frustum[0].normal[component]);

        frustum[1].normal[component] = viewer->axis[0][component] * horizontalSin;
        frustum[1].normal[component] = (float)((long double)viewer->axis[1][component] * -horizontalCos + frustum[1].normal[component]);
    }

    const float verticalHalfAngle = (float)(((long double)tr.viewParms.fovY / 180.0L) * (long double)rendererHalfPi);
    float verticalSin;
    float verticalCos;

    coduo_x87_sincosf(verticalHalfAngle, &verticalSin, &verticalCos);

    for (int32_t component = 0; component < 3; ++component) {
        frustum[2].normal[component] = viewer->axis[0][component] * verticalSin;
        frustum[2].normal[component] = (float)((long double)viewer->axis[2][component] * verticalCos + frustum[2].normal[component]);

        frustum[3].normal[component] = viewer->axis[0][component] * verticalSin;
        frustum[3].normal[component] = (float)((long double)viewer->axis[2][component] * -verticalCos + frustum[3].normal[component]);
    }

    for (int32_t planeIndex = 0; planeIndex < R_FRUSTUM_PLANE_COUNT; ++planeIndex) {
        renderer_frustum_plane_t *plane = &frustum[planeIndex];
        uint8_t signBits = 0;

        plane->type = R_PLANE_NON_AXIAL;
        plane->distance = (float)(((long double)viewer->origin[2] * plane->normal[2] + (long double)viewer->origin[1] * plane->normal[1]) +
                                  (long double)viewer->origin[0] * plane->normal[0]);

        if (plane->normal[0] < 0.0f)
            signBits |= 1;
        if (plane->normal[1] < 0.0f)
            signBits |= 2;
        if (plane->normal[2] < 0.0f)
            signBits |= 4;
        plane->signBits = signBits;
    }
}

/* Source: CoDUOMP.exe 0x004e48d0..0x004e499a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e48d0_004e499a.mcode.
 * Name: same-module Mac symbol R_MirrorPoint. The Windows ESI/ECX/EAX/EDX
 * inputs prove point/surface/camera/output roles. The point is converted to
 * surface-local coordinates and then rebuilt in the camera orientation. */
void R_MirrorPoint(const vec3_t point, const orientationr_t *surface, const orientationr_t *camera, vec3_t mirroredPoint)
{
    const long double localX = (long double)point[0] - surface->origin[0];
    const long double localY = (long double)point[1] - surface->origin[1];
    const long double localZ = (long double)point[2] - surface->origin[2];
    const long double surface0 = localZ * surface->axis[0][2] + localY * surface->axis[0][1] + localX * surface->axis[0][0];

    long double xTerm = surface0 * camera->axis[0][0];
    float yTerm = (float)(surface0 * camera->axis[0][1]);
    float zTerm = (float)(surface0 * camera->axis[0][2]);

    const long double surface1 = localZ * surface->axis[1][2] + localY * surface->axis[1][1] + localX * surface->axis[1][0];
    const float storedSurface1 = (float)surface1;
    float storedX = (float)(xTerm + surface1 * camera->axis[1][0]);
    yTerm = (float)((long double)storedSurface1 * camera->axis[1][1] + yTerm);
    zTerm = (float)((long double)storedSurface1 * camera->axis[1][2] + zTerm);

    const long double surface2 = localZ * surface->axis[2][2] + localY * surface->axis[2][1] + localX * surface->axis[2][0];
    xTerm = surface2 * camera->axis[2][0] + storedX;
    float storedY = (float)(surface2 * camera->axis[2][1] + yTerm);
    float storedZ = (float)(surface2 * camera->axis[2][2] + zTerm);

    mirroredPoint[0] = (float)(xTerm + camera->origin[0]);
    mirroredPoint[1] = (float)((long double)storedY + camera->origin[1]);
    mirroredPoint[2] = (float)((long double)storedZ + camera->origin[2]);
}

/* Source: CoDUOMP.exe 0x004e49a0..0x004e4a43.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e49a0_004e4a43.mcode.
 * Name: same-module Mac symbol R_MirrorVector. Unlike R_MirrorPoint, vectors
 * have no origin translation; only their surface-local axis weights are
 * rebuilt against the camera axes. */
void R_MirrorVector(const vec3_t vector, const orientationr_t *surface, const orientationr_t *camera, vec3_t mirroredVector)
{
    mirroredVector[0] = 0.0f;
    mirroredVector[1] = 0.0f;
    mirroredVector[2] = 0.0f;

    for (int32_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
        /* 0x004e49b4..0x004e4a3f retains each complete surface-axis dot in
         * x87 across the three camera-axis products. */
        const long double axisWeight = ((long double)vector[2] * (long double)surface->axis[axisIndex][2] +
                                        (long double)vector[1] * (long double)surface->axis[axisIndex][1]) +
                                       (long double)vector[0] * (long double)surface->axis[axisIndex][0];

        for (int32_t coordinate = 0; coordinate < 3; ++coordinate) {
            const long double contribution = axisWeight * (long double)camera->axis[axisIndex][coordinate];

            if (axisIndex == 0) {
                mirroredVector[coordinate] = (float)contribution;
            } else {
                mirroredVector[coordinate] = (float)(contribution + (long double)mirroredVector[coordinate]);
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x004e4a50..0x004e4b17.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e4a50_004e4b17.mcode.
 * Name: same-module Mac symbol R_PlaneForSurface. Type 2 takes the first three
 * 32-byte-stride polygon vertices; types 24 and above use the indexed packed
 * position array. Computed paths copy only normal/distance, exactly matching
 * the PE's four-dword copy; the fallback initializes the complete classified
 * plane and uses the X-normal default. */
void R_PlaneForSurface(const renderer_surface_t *surface, renderer_frustum_plane_t *plane)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    plane_t computedPlane = {.equation = {0.0f, 0.0f, 0.0f, 0.0f}};

    if (surface == NULL) {
        memset(plane, 0, sizeof(*plane));
        plane->normal[0] = 1.0f;
        return;
    }

    if (surface->surfaceType >= R_SURFACE_INDEXED_POSITION_FIRST) {
        const renderer_world_mesh_surface_t *indexedSurface = (const renderer_world_mesh_surface_t *)surface;
        const uint16_t baseIndex = indexedSurface->indices[0];
        const vec3_t *point0 = &indexedSurface->positions[0];
        const vec3_t *point1 = &indexedSurface->positions[indexedSurface->indices[1] - baseIndex];
        const vec3_t *point2 = &indexedSurface->positions[indexedSurface->indices[2] - baseIndex];

        (void)PlaneFromPoints(computedPlane.equation, *point0, *point1, *point2);
        memcpy(plane, &computedPlane, sizeof(computedPlane));
        return;
    }

    if (surface->surfaceType == R_SURFACE_POLY) {
        const srfPoly_t *polySurface = (const srfPoly_t *)surface;

        (void)PlaneFromPoints(computedPlane.equation, polySurface->verts[0].xyz, polySurface->verts[1].xyz, polySurface->verts[2].xyz);
        memcpy(plane, &computedPlane, sizeof(computedPlane));
        return;
    }

    memset(plane, 0, sizeof(*plane));
    plane->normal[0] = 1.0f;
}

/* Source: CoDUOMP.exe 0x004e4b20..0x004e5061.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e4b20_004e5061.mcode and direct PE
 * disassembly. Name: same-module Mac symbol R_GetPortalOrientations. The caller
 * at 0x004e5539 proves the five stack arguments, while entityNumber arrives in
 * ECX. The first portal-surface entity within 64 units supplies the PVS origin
 * and either a mirror camera or a separately positioned portal camera. Dot,
 * cross-product, translated-origin, and rotating-portal graphs retain their
 * x87 intermediates through the explicit float stores. */
qboolean R_GetPortalOrientations(int32_t entityNumber, const drawSurf_t *drawSurf, orientationr_t *surface, orientationr_t *camera,
                                 vec3_t pvsOrigin, qboolean *mirror)
{
    renderer_frustum_plane_t originalPlane;
    plane_t transformedPlane;

    R_PlaneForSurface(drawSurf->surface, &originalPlane);

    if (entityNumber != R_WORLD_ENTITY_NUMBER) {
        trRefEntity_t *entity = &tr.refdef.entities[entityNumber];

        tr.currentEntityNumber = entityNumber;
        tr.currentEntity = entity;
        R_RotateForEntity(entity, &tr.viewParms, &tr.orientation);
        R_LocalNormalToWorld(originalPlane.normal, transformedPlane.components.normal);

        transformedPlane.components.distance = (float)((((long double)tr.orientation.origin[2] * transformedPlane.components.normal[2] +
                                                         (long double)tr.orientation.origin[1] * transformedPlane.components.normal[1]) +
                                                        (long double)tr.orientation.origin[0] * transformedPlane.components.normal[0]) +
                                                       originalPlane.distance);
        originalPlane.distance = (float)((((long double)tr.orientation.origin[2] * originalPlane.normal[2] +
                                           (long double)tr.orientation.origin[1] * originalPlane.normal[1]) +
                                          (long double)tr.orientation.origin[0] * originalPlane.normal[0]) +
                                         originalPlane.distance);
    } else {
        memcpy(transformedPlane.components.normal, originalPlane.normal, sizeof(transformedPlane.components.normal));
        transformedPlane.components.distance = originalPlane.distance;
    }

    memcpy(surface->axis[0], transformedPlane.components.normal, sizeof(surface->axis[0]));
    PerpendicularVector(surface->axis[1], surface->axis[0]);
    surface->axis[2][0] =
        (float)((long double)surface->axis[0][1] * surface->axis[1][2] - (long double)surface->axis[1][1] * surface->axis[0][2]);
    surface->axis[2][1] =
        (float)((long double)surface->axis[0][2] * surface->axis[1][0] - (long double)surface->axis[0][0] * surface->axis[1][2]);
    surface->axis[2][2] =
        (float)((long double)surface->axis[1][1] * surface->axis[0][0] - (long double)surface->axis[0][1] * surface->axis[1][0]);

    for (int32_t portalIndex = 0; portalIndex < tr.refdef.num_entities; ++portalIndex) {
        const trRefEntity_t *portal = &tr.refdef.entities[portalIndex];
        float planeSeparation;

        if (portal->e.reType != RT_PORTALSURFACE)
            continue;

        planeSeparation = (float)(((long double)originalPlane.normal[2] * portal->e.origin[2] +
                                   (long double)originalPlane.normal[0] * portal->e.origin[0]) +
                                  (long double)originalPlane.normal[1] * portal->e.origin[1] - originalPlane.distance);
        if (planeSeparation > rendererPortalPlaneTolerance || planeSeparation < -rendererPortalPlaneTolerance) {
            continue;
        }

        memcpy(pvsOrigin, portal->e.oldorigin, sizeof(vec3_t));

        if (portal->e.origin[0] == portal->e.oldorigin[0] && portal->e.origin[1] == portal->e.oldorigin[1] &&
            portal->e.origin[2] == portal->e.oldorigin[2]) {
            surface->origin[0] = transformedPlane.components.distance * transformedPlane.components.normal[0];
            surface->origin[1] = transformedPlane.components.distance * transformedPlane.components.normal[1];
            surface->origin[2] = transformedPlane.components.distance * transformedPlane.components.normal[2];
            memcpy(camera->origin, surface->origin, sizeof(camera->origin));

            camera->axis[0][0] = -surface->axis[0][0];
            camera->axis[0][1] = -surface->axis[0][1];
            camera->axis[0][2] = -surface->axis[0][2];
            memcpy(camera->axis[1], surface->axis[1], sizeof(camera->axis[1]));
            memcpy(camera->axis[2], surface->axis[2], sizeof(camera->axis[2]));

            *mirror = qtrue;
            return qtrue;
        }

        planeSeparation =
            (float)(((long double)transformedPlane.components.normal[2] * portal->e.origin[2] +
                     (long double)transformedPlane.components.normal[1] * portal->e.origin[1]) +
                    (long double)transformedPlane.components.normal[0] * portal->e.origin[0] - transformedPlane.components.distance);
        planeSeparation = -planeSeparation;
        surface->origin[0] = (float)((long double)planeSeparation * surface->axis[0][0] + portal->e.origin[0]);
        surface->origin[1] = (float)((long double)planeSeparation * surface->axis[0][1] + portal->e.origin[1]);
        surface->origin[2] = (float)((long double)planeSeparation * surface->axis[0][2] + portal->e.origin[2]);

        memcpy(camera->origin, portal->e.oldorigin, sizeof(camera->origin));
        memcpy(camera->axis, portal->e.axis, sizeof(camera->axis));
        camera->axis[0][0] = -camera->axis[0][0];
        camera->axis[0][1] = -camera->axis[0][1];
        camera->axis[0][2] = -camera->axis[0][2];
        camera->axis[1][0] = -camera->axis[1][0];
        camera->axis[1][1] = -camera->axis[1][1];
        camera->axis[1][2] = -camera->axis[1][2];

        if (portal->e.oldframe != 0) {
            float rotationDegrees;
            vec3_t originalAxis;
            const float timeAsFloat = (float)tr.refdef.time;

            if (portal->e.frame != 0) {
                const float frameAsFloat = (float)portal->e.frame;

                rotationDegrees = (float)(((long double)timeAsFloat / rendererMillisecondsPerSecond) * frameAsFloat);
            } else {
                const float oscillation = (float)coduo_x87_sinl((long double)timeAsFloat * rendererPortalOscillationRate);

                rotationDegrees = (float)((long double)oscillation * rendererPortalOscillationAmplitude + portal->e.rotation);
            }

            memcpy(originalAxis, camera->axis[1], sizeof(originalAxis));
            RotatePointAroundVector(camera->axis[1], camera->axis[0], originalAxis, rotationDegrees);
            camera->axis[2][0] =
                (float)((long double)camera->axis[0][1] * camera->axis[1][2] - (long double)camera->axis[1][1] * camera->axis[0][2]);
            camera->axis[2][1] =
                (float)((long double)camera->axis[0][2] * camera->axis[1][0] - (long double)camera->axis[0][0] * camera->axis[1][2]);
            camera->axis[2][2] =
                (float)((long double)camera->axis[1][1] * camera->axis[0][0] - (long double)camera->axis[0][1] * camera->axis[1][0]);
        } else if (portal->e.rotation != 0.0f) {
            vec3_t originalAxis;

            memcpy(originalAxis, camera->axis[1], sizeof(originalAxis));
            RotatePointAroundVector(camera->axis[1], camera->axis[0], originalAxis, portal->e.rotation);
            camera->axis[2][0] =
                (float)((long double)camera->axis[0][1] * camera->axis[1][2] - (long double)camera->axis[1][1] * camera->axis[0][2]);
            camera->axis[2][1] =
                (float)((long double)camera->axis[0][2] * camera->axis[1][0] - (long double)camera->axis[0][0] * camera->axis[1][2]);
            camera->axis[2][2] =
                (float)((long double)camera->axis[1][1] * camera->axis[0][0] - (long double)camera->axis[0][1] * camera->axis[1][0]);
        }

        *mirror = qfalse;
        return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x0050ee20..0x0050ee62.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ee20_0050ee62.mcode.
 * Name: same-module Mac symbol R_SetPlaneSidesDPVS. The three output bytes are
 * byte offsets into paired bounds values, not a conventional sign-bit mask. */
void R_SetPlaneSidesDPVS(renderer_dpvs_plane_t *plane)
{
    uint32_t normalBits[3];

    plane->distance -= rendererPortalPlaneEpsilon;
    memcpy(normalBits, plane->normal, sizeof(normalBits));
    plane->sideOffsets[0] =
        normalBits[0] != 0U && (normalBits[0] & 0x80000000U) == 0U ? R_DPVS_SIDE_X_POSITIVE_OFFSET : R_DPVS_SIDE_X_NEGATIVE_OFFSET;
    plane->sideOffsets[1] =
        normalBits[1] != 0U && (normalBits[1] & 0x80000000U) == 0U ? R_DPVS_SIDE_Y_POSITIVE_OFFSET : R_DPVS_SIDE_Y_NEGATIVE_OFFSET;
    plane->sideOffsets[2] =
        normalBits[2] != 0U && (normalBits[2] & 0x80000000U) == 0U ? R_DPVS_SIDE_Z_POSITIVE_OFFSET : R_DPVS_SIDE_Z_NEGATIVE_OFFSET;
}

/* Source: CoDUOMP.exe 0x004e51d0..0x004e5476.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e51d0_004e5476.mcode and direct PE
 * disassembly. Name: same-module Mac symbol SurfIsOffscreen. The surface is
 * tessellated without drawing, clipped as a complete set, culled for back-facing
 * triangles, and finally rejected beyond its shader's portal range. Vertex
 * deltas, squared distances, facing dots, and the portal-range square remain
 * unspilled x87 values through their comparisons. */
qboolean SurfIsOffscreen(const drawSurf_t *drawSurf)
{
    const uint32_t sort = drawSurf->sort;
    const int32_t shaderIndex = (int32_t)((sort >> R_SORT_SHADER_SHIFT) & R_SORT_SHADER_MASK);
    const renderer_static_vertex_memory_source_t storageMode =
        (renderer_static_vertex_memory_source_t)((sort >> R_SORT_STORAGE_SHIFT) & R_SORT_STORAGE_MASK);
    int32_t entityNumber = (int32_t)((sort >> R_SORT_ENTITY_SHIFT) & R_SORT_ENTITY_MASK);
    shader_t *shader = tr.sortedShaders[shaderIndex];
    uint32_t pointAnd = UINT32_MAX;
    float minimumDistanceSquared = rendererPortalInitialDistanceSquared;

    R_RotateForViewer();

    if ((sort & R_SORT_WORLD_ENTITY) != 0)
        entityNumber = R_WORLD_ENTITY_NUMBER;
    if (entityNumber == R_WORLD_ENTITY_NUMBER)
        backEnd.currentEntity = &tr.worldEntity;
    else
        backEnd.currentEntity = &backEnd.refdef.entities[entityNumber];

    if (storageMode != glState.currentStorageMode) {
        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE)
            RB_SelectStorageNV(storageMode);
        else if (glConfig.vertexArrayObjectATIAvailable != qfalse)
            RB_SelectStorageATI(storageMode);
        glState.currentStorageMode = storageMode;
    }

    RB_BeginSurface(shader, R_DYNAMIC_TESS_STORAGE);
    rb_surfaceTable[drawSurf->surface->surfaceType](drawSurf->surface);

    if (tess.vertexCount <= 0)
        return qtrue;

    for (int32_t vertexIndex = 0; vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position = &tess.xyz[vertexIndex * tess.vertexComponentCount];
        vec4_t eye;
        vec4_t clip;
        uint32_t pointFlags = 0;

        R_TransformModelToClip(position, tr.orientation.modelMatrix, tr.viewParms.projectionMatrix, eye, clip);

        if (clip[0] >= clip[3])
            pointFlags = R_PORTAL_CLIP_X_POSITIVE;
        else if (clip[0] <= -clip[3])
            pointFlags = R_PORTAL_CLIP_X_NEGATIVE;

        if (clip[1] >= clip[3])
            pointFlags |= R_PORTAL_CLIP_Y_POSITIVE;
        else if (clip[1] <= -clip[3])
            pointFlags |= R_PORTAL_CLIP_Y_NEGATIVE;

        if (clip[2] >= clip[3])
            pointFlags |= R_PORTAL_CLIP_Z_POSITIVE;
        else if (clip[2] <= -clip[3])
            pointFlags |= R_PORTAL_CLIP_Z_NEGATIVE;

        pointAnd &= pointFlags;
    }

    if (pointAnd != 0)
        return qtrue;

    {
        int32_t visibleTriangleCount = tess.indexCount / R_TRIANGLE_INDEX_COUNT;

        for (int32_t indexOffset = 0; indexOffset < tess.indexCount; indexOffset += R_TRIANGLE_INDEX_COUNT) {
            const uint16_t vertexIndex = tess.indexes[indexOffset];
            const float *position = &tess.xyz[(int32_t)vertexIndex * tess.vertexComponentCount];
            long double viewDirection[3];
            long double distanceSquared;
            long double facing;

            viewDirection[0] = (long double)position[0] - tr.viewParms.orientation.origin[0];
            viewDirection[1] = (long double)position[1] - tr.viewParms.orientation.origin[1];
            viewDirection[2] = (long double)position[2] - tr.viewParms.orientation.origin[2];

            distanceSquared =
                (viewDirection[2] * viewDirection[2] + viewDirection[1] * viewDirection[1]) + viewDirection[0] * viewDirection[0];
            if (distanceSquared < minimumDistanceSquared)
                minimumDistanceSquared = (float)distanceSquared;

            facing = (viewDirection[2] * tess.stageNormals[vertexIndex][2] + viewDirection[1] * tess.stageNormals[vertexIndex][1]) +
                     viewDirection[0] * tess.stageNormals[vertexIndex][0];
            if (facing >= 0.0f)
                --visibleTriangleCount;
        }

        if (visibleTriangleCount == 0)
            return qtrue;
    }

    if (IsMirror(entityNumber, drawSurf))
        return qfalse;

    return (long double)minimumDistanceSquared > (long double)tess.shader->portalRange * tess.shader->portalRange ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x004e5480..0x004e5635.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5480_004e5635.mcode and direct PE
 * disassembly. Name: same-module Mac symbol R_MirrorViewBySurface. The current
 * view is copied twice so the derived portal view can be rendered and the
 * original front-end state restored exactly afterward. The portal-plane dot
 * narrows only at the destination float store. */
qboolean R_MirrorViewBySurface(const drawSurf_t *drawSurf, int32_t entityNumber)
{
    viewParms_t oldViewParms;
    viewParms_t newViewParms;
    orientationr_t surface;
    orientationr_t camera;

    if (tr.viewParms.isPortal != qfalse) {
        ri.Printf(R_PRINT_DEVELOPER, "WARNING: recursive mirror/portal found\n");
        return qfalse;
    }
    if (r_noportals->integer != 0 || r_fastsky->integer != 0)
        return qfalse;
    if (SurfIsOffscreen(drawSurf))
        return qfalse;

    oldViewParms = tr.viewParms;
    newViewParms = tr.viewParms;
    newViewParms.isPortal = qtrue;

    if (!R_GetPortalOrientations(entityNumber, drawSurf, &surface, &camera, newViewParms.pvsOrigin, &newViewParms.isMirror)) {
        return qfalse;
    }

    R_MirrorPoint(oldViewParms.orientation.origin, &surface, &camera, newViewParms.orientation.origin);

    newViewParms.portalPlane.normal[0] = -camera.axis[0][0];
    newViewParms.portalPlane.normal[1] = -camera.axis[0][1];
    newViewParms.portalPlane.normal[2] = -camera.axis[0][2];
    newViewParms.portalPlane.distance = (float)(((long double)camera.origin[2] * newViewParms.portalPlane.normal[2] +
                                                 (long double)camera.origin[1] * newViewParms.portalPlane.normal[1]) +
                                                (long double)camera.origin[0] * newViewParms.portalPlane.normal[0]);
    R_SetPlaneSidesDPVS(&newViewParms.portalPlane);

    R_MirrorVector(oldViewParms.orientation.axis[0], &surface, &camera, newViewParms.orientation.axis[0]);
    R_MirrorVector(oldViewParms.orientation.axis[1], &surface, &camera, newViewParms.orientation.axis[1]);
    R_MirrorVector(oldViewParms.orientation.axis[2], &surface, &camera, newViewParms.orientation.axis[2]);

    R_RenderView(&newViewParms);
    tr.viewParms = oldViewParms;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004e5070..0x004e51cd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5070_004e51cd.mcode.
 * Name: same-module Mac symbol IsMirror. The draw surface supplies the portal
 * plane; non-world entity planes have their distance translated by the model
 * orientation. A type-11 portal entity within 64 units is a mirror exactly
 * when its origin and oldorigin are identical. The first near portal decides
 * the result, matching the PE's immediate false path on inequality. The portal
 * separation remains live in x87 across both range comparisons. */
qboolean IsMirror(int32_t entityNumber, const drawSurf_t *drawSurf)
{
    renderer_frustum_plane_t plane;

    R_PlaneForSurface(drawSurf->surface, &plane);

    if (entityNumber != R_WORLD_ENTITY_NUMBER) {
        trRefEntity_t *entity = &tr.refdef.entities[entityNumber];

        tr.currentEntityNumber = entityNumber;
        tr.currentEntity = entity;
        R_RotateForEntity(entity, &tr.viewParms, &tr.orientation);

        plane.distance =
            (float)(((long double)tr.orientation.origin[2] * plane.normal[2] + (long double)tr.orientation.origin[1] * plane.normal[1]) +
                    (long double)tr.orientation.origin[0] * plane.normal[0] + plane.distance);
    }

    for (int32_t portalIndex = 0; portalIndex < tr.refdef.num_entities; ++portalIndex) {
        const trRefEntity_t *portal = &tr.refdef.entities[portalIndex];
        long double planeSeparation;

        if (portal->e.reType != RT_PORTALSURFACE)
            continue;

        planeSeparation = ((long double)plane.normal[2] * portal->e.origin[2] + (long double)plane.normal[0] * portal->e.origin[0]) +
                          (long double)plane.normal[1] * portal->e.origin[1] - plane.distance;
        if (planeSeparation > rendererPortalPlaneTolerance || planeSeparation < -rendererPortalPlaneTolerance)
            continue;

        return portal->e.origin[0] == portal->e.oldorigin[0] && portal->e.origin[1] == portal->e.oldorigin[1] &&
                       portal->e.origin[2] == portal->e.oldorigin[2]
                   ? qtrue
                   : qfalse;
    }

    return qfalse;
}
