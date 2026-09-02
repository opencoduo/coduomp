#include "backend.h"

#include "gl_debug.h"
#include "../math/vector_math.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    R_CORONA_ID_LIMIT = 1024,
    R_REPLACEABLE_ENTITY_WARNING_THRESHOLD = 919,
    R_MAX_ACTIVE_SCENE_ENTITIES = 1022,
    R_VISIBLE_MODEL_WARNING_LIMIT = 1021,
    R_VISIBLE_MODEL_STATMON_ENTRY = 9,
    R_VISIBLE_MODEL_STATMON_DURATION_MSEC = 3000
};

static const float rendererTimeToSeconds = 0.0010000000474974513f; /* 0x3a83126f */
static const float rendererDynamicLightIntensityScale = 0.03125f; /* 0x3d000000, 1 / 32 */
static const float rendererDynamicLightConstantAttenuation = 0.0010000000474974513f; /* 0x3a83126f */
static const float rendererHalfDegreesToRadians = 0.0087266461923718452f; /* 0x3c0efa35, pi / 360 */
static const double rendererLodReferenceHalfFovRadians = 0.6981317400932312; /* 0x3fe6571860000000, 40 degrees */

static const char rendererVisibleModelWarningShader[] = "gfx/2d/warning@models.jpg";

/* Original 0x027937d0. The stock RE_AddRefEntityToScene path emits the text
 * warning at most once for a given renderer viewCount. */
int32_t rendererVisibleModelWarningView;

/* Original 0x04884da8. RE_RenderScene derives this persistent frame marker
 * from refdef bit 0x10; the backend clear path consumes it together with a
 * skybox-portal view. */
qboolean rendererSkyboxPortalActive;

/* Source: CoDUOMP.exe 0x004e5c70..0x004e5cb0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5c70_004e5cb1.mcode.
 * Name: exact same-module Mac symbol R_ToggleSmpFrame. The Windows body
 * resets the command cursor and all ten scene-frame counters. */
void R_ToggleSmpFrame(void)
{
    rendererBackendData->commandUsed = 0;
    memset(&rendererSceneFrameState, 0, sizeof(rendererSceneFrameState));
}

/* Source: CoDUOMP.exe 0x004e5cc0..0x004e5cec.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5cc0_004e5ced.mcode.
 * Name: exact same-module Mac symbol RE_ClearScene. This advances the four
 * per-scene list boundaries without discarding data already queued this
 * frame. */
void RE_ClearScene(void)
{
    rendererSceneFrameState.firstDlight = rendererSceneFrameState.dlightCount;
    rendererSceneFrameState.firstCorona = rendererSceneFrameState.coronaCount;
    rendererSceneFrameState.firstEntity = rendererSceneFrameState.entityCount;
    rendererSceneFrameState.firstPoly = rendererSceneFrameState.polyCount;
}

/* Source: CoDUOMP.exe 0x004e5db0..0x004e5e46.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5db0_004e5e47.mcode.
 * Name and signature: exact same-module Mac symbol RE_AddPolyToScene and the
 * renderer export table. */
void RE_AddPolyToScene(int32_t shaderHandle, int32_t vertexCount, const polyVert_t *vertices)
{
    srfPoly_t *poly;
    int32_t nextVertexCount;

    if (tr.registered == qfalse)
        return;

    if (shaderHandle == 0)
        shaderHandle = tr.defaultShader->index;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (vertexCount < 0 || rendererSceneFrameState.polyVertexCount < 0 || rendererMaxPolyVerts < rendererSceneFrameState.polyVertexCount ||
        vertexCount > rendererMaxPolyVerts - rendererSceneFrameState.polyVertexCount ||
        rendererSceneFrameState.polyCount >= rendererMaxPolys) {
        return;
    }
    nextVertexCount = rendererSceneFrameState.polyVertexCount + vertexCount;

    poly = &rendererBackendData->polys[rendererSceneFrameState.polyCount];
    poly->surfaceType = R_SURFACE_POLY;
    poly->hShader = shaderHandle;
    poly->numVerts = vertexCount;
    poly->verts = &rendererBackendData->polyVertices[rendererSceneFrameState.polyVertexCount];
    memcpy(poly->verts, vertices, (size_t)vertexCount * sizeof(*vertices));

    ++rendererSceneFrameState.polyCount;
    rendererSceneFrameState.polyVertexCount = nextVertexCount;
}

/* Source: CoDUOMP.exe 0x004e5e50..0x004e5f7b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5e50_004e5f7c.mcode.
 * Name and signature: exact same-module Mac symbol RE_AddPolysToScene and
 * renderer export-table position 20. */
void RE_AddPolysToScene(int32_t shaderHandle, int32_t vertexCount, const polyVert_t *vertices, int32_t polyCount)
{
    int32_t nextVertexCount;
    int32_t polyIndex;

    if (tr.registered == qfalse)
        return;

    if (shaderHandle == 0) {
        ri.Printf(R_PRINT_WARNING, "WARNING: RE_AddPolysToScene: NULL poly shader\n");
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    for (polyIndex = 0; polyIndex < polyCount; ++polyIndex) {
        srfPoly_t *poly;

        if (vertexCount < 0 || rendererSceneFrameState.polyVertexCount < 0 ||
            rendererMaxPolyVerts < rendererSceneFrameState.polyVertexCount ||
            vertexCount > rendererMaxPolyVerts - rendererSceneFrameState.polyVertexCount ||
            rendererSceneFrameState.polyCount >= rendererMaxPolys) {
            break;
        }
        nextVertexCount = rendererSceneFrameState.polyVertexCount + vertexCount;

        poly = &rendererBackendData->polys[rendererSceneFrameState.polyCount];
        poly->surfaceType = R_SURFACE_POLY;
        poly->hShader = shaderHandle;
        poly->numVerts = vertexCount;
        poly->verts = &rendererBackendData->polyVertices[rendererSceneFrameState.polyVertexCount];
        memcpy(poly->verts, vertices, (size_t)vertexCount * sizeof(*vertices));

        ++rendererSceneFrameState.polyCount;
        rendererSceneFrameState.polyVertexCount = nextVertexCount;
        vertices += vertexCount;
    }
}

/* Source: CoDUOMP.exe 0x004e61b0..0x004e62b4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e61b0_004e62b5.mcode.
 * Name and signature: exact same-module Mac symbol RE_AddLightToScene and the
 * renderer export table. Fields not written by the Windows body deliberately
 * retain their previous frame-storage bytes. */
void RE_AddLightToScene(const vec3_t origin, float radius, float red, float green, float blue)
{
    renderer_light_t *light;
    long double intensity;

    if (tr.registered == qfalse || rendererSceneFrameState.dlightCount >= R_MAX_DLIGHTS || radius <= 0.0f) {
        return;
    }

    light = &rendererBackendData->dlights[rendererSceneFrameState.dlightCount++];
    intensity = (long double)radius * radius * rendererDynamicLightIntensityScale;

    light->type = R_LIGHT_TYPE_POINT;
    light->color[0] = red;
    light->color[1] = green;
    light->color[2] = blue;
    light->intensity = (float)intensity;
    light->ambient[0] = 0.0f;
    light->ambient[1] = 0.0f;
    light->ambient[2] = 0.0f;
    light->ambient[3] = 1.0f;
    light->diffuse[0] = (float)((long double)tr.identityLight * intensity * red);
    light->diffuse[1] = (float)((long double)tr.identityLight * intensity * green);
    light->diffuse[2] = (float)((long double)tr.identityLight * intensity * blue);
    light->diffuse[3] = 1.0f;
    light->specular[0] = 0.0f;
    light->specular[1] = 0.0f;
    light->specular[2] = 0.0f;
    light->specular[3] = 1.0f;
    light->position[0] = origin[0];
    light->position[1] = origin[1];
    light->position[2] = origin[2];
    light->position[3] = 1.0f;
    light->constantAttenuation = rendererDynamicLightConstantAttenuation;
    light->linearAttenuation = 0.0f;
    light->quadraticAttenuation = 1.0f;
    light->spotExponent = 0.0f;
    light->spotCutoff = 180.0f;
    light->radius = radius;
}

/* Source: CoDUOMP.exe 0x004e62c0..0x004e6355.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e62c0_004e6356.mcode.
 * Name and signature: exact same-module Mac symbol RE_AddCoronaToScene and
 * renderer export-table position 23. */
void RE_AddCoronaToScene(const vec3_t origin, float red, float green, float blue, float scale, int32_t id, int32_t flags)
{
    renderer_corona_t *corona;

    if (tr.registered == qfalse || rendererSceneFrameState.coronaCount >= R_MAX_CORONAS) {
        return;
    }

    if (id < 0 || id >= R_CORONA_ID_LIMIT) {
        ri.Printf(R_PRINT_DEVELOPER,
                  "^3added corona with invalid id %i "
                  "(should be >= 0 and < %i)\n",
                  id, R_CORONA_ID_LIMIT);
        return;
    }

    corona = &rendererBackendData->coronas[rendererSceneFrameState.coronaCount++];
    corona->origin[0] = origin[0];
    corona->origin[1] = origin[1];
    corona->origin[2] = origin[2];
    corona->color[0] = red;
    corona->color[1] = green;
    corona->color[2] = blue;
    corona->scale = scale;
    corona->id = id;
    corona->flags = flags;
}

/* Source: CoDUOMP.exe 0x004e6360..0x004e679d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e6360_004e679e.mcode.
 * Name and signature: exact same-module Mac symbol RE_RenderScene and
 * renderer export-table position 24. Windows machine code proves the complete
 * 0x50-byte input copy, scene-list slices, dynamic-light mode handling,
 * viewport and LOD setup, R_RenderView call, and front-end timing update. */
void RE_RenderScene(const refdef_t *refdef)
{
    viewParms_t viewParms;
    int32_t startTime;
    int32_t sceneDlightCount;
    float lodScale;
    float lodBias;
    float minimumFov;
    double lodRatio;

    if (tr.registered == qfalse)
        return;

    if (rendererGlLogFile != NULL)
        fprintf(rendererGlLogFile, "%s", "====== RE_RenderScene =====\n");

    if (r_norefresh->integer != 0)
        return;

    startTime = ri.Milliseconds();
    if (tr.world == NULL && (refdef->rdflags & RDF_NOWORLDMODEL) == 0) {
        ri.Error(ERR_DROP, "\x15R_RenderScene: NULL worldmodel");
    }

    memcpy(&tr.refdef, refdef, sizeof(*refdef));
    if ((refdef->rdflags & RDF_SKYBOX_PORTAL) != 0)
        rendererFogCount = 1;
    rendererSkyboxPortalActive = (refdef->rdflags & RDF_SKYBOX_PORTAL_ACTIVE) != 0;

    tr.refdef.floatTime = (float)((double)tr.refdef.time * (double)rendererTimeToSeconds);

    tr.refdef.num_entities = rendererSceneFrameState.entityCount - rendererSceneFrameState.firstEntity;
    tr.refdef.entities = &rendererBackendData->sceneEntities[rendererSceneFrameState.firstEntity];

    sceneDlightCount = rendererSceneFrameState.dlightCount - rendererSceneFrameState.firstDlight;
    tr.refdef.num_dlights = sceneDlightCount;
    tr.refdef.entityDlightCount = sceneDlightCount;
    tr.refdef.dlights = &rendererBackendData->dlights[rendererSceneFrameState.firstDlight];

    tr.refdef.coronaCount = rendererSceneFrameState.coronaCount - rendererSceneFrameState.firstCorona;
    tr.refdef.coronas = &rendererBackendData->coronas[rendererSceneFrameState.firstCorona];

    tr.refdef.numPolys = rendererSceneFrameState.polyCount - rendererSceneFrameState.firstPoly;
    tr.refdef.polys = &rendererBackendData->polys[rendererSceneFrameState.firstPoly];

    tr.refdef.numDrawSurfs = rendererSceneFrameState.drawSurfCount;
    tr.refdef.drawSurfs = rendererBackendData->drawSurfs;
    tr.refdef.entitySurfaceCount = 0;
    tr.refdef.entitySurfaces = rendererBackendData->entitySurfaces;

    if (r_dynamiclight->integer == 0)
        tr.refdef.entityDlightCount = 0;
    if (r_dynamiclight->integer != 1)
        tr.refdef.num_dlights = 0;
    if (r_dynamiclight->integer == 3) {
        ri.Printf(R_PRINT_ALL, "%i dynamic lights in scene\n", 0);
    }

    memset(&viewParms, 0, sizeof(viewParms));
    viewParms.viewportX = tr.refdef.x;
    viewParms.viewportY = glConfig.vidHeight - refdef->height - refdef->y;
    viewParms.viewportWidth = refdef->width;
    viewParms.viewportHeight = refdef->height;
    viewParms.fovX = refdef->fov_x;
    viewParms.fovY = refdef->fov_y;
    viewParms.isPortal = qfalse;

    lodScale = r_lodscale->value;
    if (lodScale > 4.0f)
        lodScale = 4.0f;
    lodBias = r_lodbias->value;
    if (lodBias > 0.0f)
        lodBias = 0.0f;

    minimumFov = refdef->fov_x;
    if (!(refdef->fov_x <= refdef->fov_y))
        minimumFov = refdef->fov_y;
    lodRatio = tan((double)minimumFov * (double)rendererHalfDegreesToRadians) / tan(rendererLodReferenceHalfFovRadians);
    viewParms.lodScale = (float)(lodRatio * (double)lodScale);
    viewParms.lodBias = (float)(lodRatio * (double)lodBias);

    memcpy(viewParms.orientation.origin, refdef->vieworg, sizeof(viewParms.orientation.origin));
    memcpy(viewParms.orientation.axis, refdef->viewaxis, sizeof(viewParms.orientation.axis));
    memcpy(viewParms.pvsOrigin, refdef->vieworg, sizeof(viewParms.pvsOrigin));

    R_RenderView(&viewParms);

    rendererSceneFrameState.drawSurfCount = tr.refdef.numDrawSurfs;
    rendererSceneFrameState.firstEntity = rendererSceneFrameState.entityCount;
    rendererSceneFrameState.firstDlight = rendererSceneFrameState.dlightCount;
    rendererSceneFrameState.firstPoly = rendererSceneFrameState.polyCount;
    tr.frontEndMsec += ri.Milliseconds() - startTime;
}

/* Source: CoDUOMP.exe 0x004e5f80..0x004e6050.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e5f80_004e6051.mcode.
 * Name and VectorDistance source boundary: exact same-module Mac symbol
 * R_FurthestReplaceableRefEntity and its call graph. Windows proves the scene
 * range, the two non-replaceable model types, and the strict furthest-distance
 * selection below. */
int32_t R_FurthestReplaceableRefEntity(void)
{
    float furthestDistance = 0.0f;
    int32_t furthestEntity = -1;
    int32_t entityIndex;

    for (entityIndex = rendererSceneFrameState.firstEntity; entityIndex < rendererSceneFrameState.entityCount; ++entityIndex) {
        const trRefEntity_t *sceneEntity = &rendererBackendData->sceneEntities[entityIndex];
        vec3_t difference;
        float distance;

        if (sceneEntity->e.reType == RT_BRUSH_MODEL || sceneEntity->e.reType == RT_MODEL) {
            continue;
        }

        difference[0] = tr.refdef.vieworg[0] - sceneEntity->e.origin[0];
        difference[1] = tr.refdef.vieworg[1] - sceneEntity->e.origin[1];
        difference[2] = tr.refdef.vieworg[2] - sceneEntity->e.origin[2];
        distance = sqrtf(difference[2] * difference[2] + difference[1] * difference[1] + difference[0] * difference[0]);

        if (distance > furthestDistance) {
            furthestDistance = distance;
            furthestEntity = entityIndex;
        }
    }

    return furthestEntity;
}

/* Source: CoDUOMP.exe 0x004e6060..0x004e609d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e6060_004e609e.mcode.
 * Name: exact same-module Mac symbol R_SetSceneRefEntity. The 39-dword copy is
 * exactly one original refEntity_t; the four following stores prove the
 * renderer-owned lighting, cull, and static-lighting initialization. */
void R_SetSceneRefEntity(int32_t entityIndex, const refEntity_t *entity, renderer_static_model_t *staticLighting)
{
    trRefEntity_t *sceneEntity = &rendererBackendData->sceneEntities[entityIndex];

    memcpy(&sceneEntity->e, entity, sizeof(sceneEntity->e));
    sceneEntity->lightingCalculated = 0;
    sceneEntity->cullState = CULL_IN;
    sceneEntity->staticModelLighting = staticLighting;
    sceneEntity->lightCount = 0;
}

/* Source: CoDUOMP.exe 0x004e60a0..0x004e61ac.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e60a0_004e61ad.mcode.
 * Name, arguments, and calls: exact same-module Mac symbol
 * RE_AddRefEntityToScene and its calls to StatMon_Warning,
 * R_FurthestReplaceableRefEntity, R_SetSceneRefEntity, and the retail
 * once-per-view console warning. The second argument is a static-lighting
 * pointer, not the previously inferred scene number: Windows stores it at
 * trRefEntity_t +0x2b0, and static-model callers pass the owning lighting
 * record while FX callers pass NULL. */
void RE_AddRefEntityToScene(const refEntity_t *entity, renderer_static_model_t *staticLighting)
{
    int32_t entityIndex;

    if (tr.registered == qfalse)
        return;

    if (com_statmon->integer != 0 && rendererSceneFrameState.entityCount >= R_REPLACEABLE_ENTITY_WARNING_THRESHOLD) {
        StatMon_Warning(R_VISIBLE_MODEL_STATMON_ENTRY, R_VISIBLE_MODEL_STATMON_DURATION_MSEC, rendererVisibleModelWarningShader);
    }

    if (rendererSceneFrameState.entityCount >= R_MAX_ACTIVE_SCENE_ENTITIES) {
        if (rendererVisibleModelWarningView != tr.viewCount) {
            rendererVisibleModelWarningView = tr.viewCount;
            ri.Printf(R_PRINT_WARNING, "too many visible models (more than %i)\n", R_VISIBLE_MODEL_WARNING_LIMIT);
        }

        entityIndex = R_FurthestReplaceableRefEntity();
        if (entityIndex < 0)
            return;

        R_SetSceneRefEntity(entityIndex, entity, staticLighting);
        return;
    }

    if (entity->reType < RT_BRUSH_MODEL || entity->reType >= RT_MAX_REF_ENTITY_TYPE) {
        ri.Error(ERR_DROP, "\x15RE_AddRefEntityToScene: bad reType %i", entity->reType);
    }

    entityIndex = rendererSceneFrameState.entityCount;
    R_SetSceneRefEntity(entityIndex, entity, staticLighting);
    ++rendererSceneFrameState.entityCount;
    ++tr.refdef.num_entities;
}
