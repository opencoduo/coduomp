#include "backend.h"

#include "gl_api.h"

#include <string.h>


enum {
    RB_XMODEL_TEXCOORD_POINTER_COUNT = 4
};

/* NOT_FROM_ORIGINAL_SOURCE: return carrier for the two tessellation-array
 * bases produced by RB_BeginXModelTessRange. */
typedef struct rb_xmodel_tess_range_s {
    vec3_t *positions;
    vec3_t *normals;
} rb_xmodel_tess_range_t;

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical
 * RB_CheckOverflow, triangle expansion, counter updates, and texcoord copy at
 * the head of the four CPU XModel surface backends. */
static rb_xmodel_tess_range_t RB_BeginXModelTessRange(const XSurface *surface)
{
    const int32_t indexCount = surface->triangleCount * 3;

    RB_CheckOverflow(surface->vertexCount, indexCount);
    const int32_t firstVertex = tess.vertexCount;
    const uint32_t firstVertexArrayIndex = (uint16_t)firstVertex;
    const int32_t firstIndex = tess.indexCount;
    tess.vertexCount = (int32_t)((uint32_t)tess.vertexCount + (uint32_t)(int32_t)surface->vertexCount);
    XSurfaceGetTris(surface, (uint16_t(*)[3]) & tess.indexes[firstIndex], (int16_t)firstVertex);
    tess.indexCount = (int32_t)((uint32_t)tess.indexCount + (uint32_t)indexCount);
    memcpy(&tess.texCoords[R_TESS_BASE_TEXCOORD_SET][firstVertexArrayIndex], surface->texCoords,
           (size_t)((uint32_t)(int32_t)surface->vertexCount * (uint32_t)sizeof(surface->texCoords[0])));

    rb_xmodel_tess_range_t range = {
        (vec3_t *)(void *)&tess.xyz[firstVertexArrayIndex * 3u],
        &tess.stageNormals[firstVertexArrayIndex],
    };
    return range;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed expression of the DObj evaluation-storage
 * selection repeated by every RB_SurfaceXModel backend. */
static const DObjSkelMat *RB_GetXModelBasePose(const renderer_entity_surface_t *entitySurface)
{
    const DObj *obj = entitySurface->obj;
    const uint8_t partBaseIndex = obj->modelPartBaseIndices[entitySurface->modelIndex];

    return &obj->evaluationStorage->partSpans[partBaseIndex].basePose;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical logging
 * prefix in the ARB, ATI, and NV optimized rigid-surface backends. */
static void RB_LogOptimizedXModelSurface(const char *format, const renderer_entity_surface_t *entitySurface)
{
    if (r_logFile->integer == 0)
        return;

    GLimp_LogComment(va(format, tess.shader->name, DObjGetModel(entitySurface->obj, entitySurface->modelIndex)->name));
}

/* NOT_FROM_ORIGINAL_SOURCE: the ARB and NV paths contain the same pass loop
 * and show-triangle replay.  `baseTexCoord` is either a VBO byte offset or the
 * native base address of the interleaved NV vertex array. */
static void RB_DrawOptimizedXModelPasses(const XSurface *surface, const void *baseTexCoord, const void *indices)
{
    const void *baseTexCoords[RB_XMODEL_TEXCOORD_POINTER_COUNT] = {baseTexCoord, NULL, NULL, NULL};
    const int32_t indexCount = surface->triangleCount * 3;

    for (int32_t passIndex = 0; passIndex < tess.shader->numUnfoggedPasses; ++passIndex) {
        shaderStage_t *stage = tess.activeStages[passIndex];
        GL_State(stage->stateBits);
        RB_SetupMultitexture(stage, baseTexCoords, (int32_t)sizeof(XSurfaceARBVert));
        GL_DrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indices);
    }

    if (r_showtris->integer == 0)
        return;

    if ((r_showtris->integer & 1) != 0)
        qglDepthRange(0.0, 0.1);

    shaderStage_t *stage = tr.defaultShader->stages[0];
    GL_State(stage->stateBits);
    RB_SetupMultitexture(stage, baseTexCoords, (int32_t)sizeof(XSurfaceARBVert));

    if (r_showtris->integer >= 5) {
        uint8_t color[4];
        RB_ChooseSurfaceCountColor(indexCount, color);
        qglColor4ubv(color);
    } else {
        qglColor3f(tr.identityLight, 0.0f, tr.identityLight);
    }

    GL_DrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indices);
    qglDepthRange(0.0, 1.0);
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the ATI path's repeated
 * state selection, animated-image lookup, and image bind. */
static void RB_BindOptimizedXModelStage(shaderStage_t *stage)
{
    GL_State(stage->stateBits);
    GL_Bind(RB_GetAnimatedImage(&stage->bundle[0], glState.currenttmu));
}

/* Source: CoDUOMP.exe 0x0051f6f0..0x0051f789.
 * The Windows optimizer uses EAX/ECX/EDI/ESI plus one stack argument.  This
 * portable signature names the same matrix, normal, position, and outputs.
 * The component-specific association below follows the x87 stack exactly. */
static void RB_TransformRigidVertex(const DObjSkelMat *matrix, const XSurfaceRigidVert *vertex, vec3_t outPosition, vec3_t outNormal)
{
    outPosition[0] =
        ((vertex->position[1] * matrix->axis[1][0] + vertex->position[0] * matrix->axis[0][0]) + vertex->position[2] * matrix->axis[2][0]) +
        matrix->origin[0];
    outPosition[1] =
        ((vertex->position[1] * matrix->axis[1][1] + vertex->position[2] * matrix->axis[2][1]) + vertex->position[0] * matrix->axis[0][1]) +
        matrix->origin[1];
    outPosition[2] =
        ((vertex->position[1] * matrix->axis[1][2] + vertex->position[2] * matrix->axis[2][2]) + vertex->position[0] * matrix->axis[0][2]) +
        matrix->origin[2];

    outNormal[0] =
        (vertex->normal[1] * matrix->axis[1][0] + vertex->normal[0] * matrix->axis[0][0]) + vertex->normal[2] * matrix->axis[2][0];
    outNormal[1] =
        (vertex->normal[0] * matrix->axis[0][1] + vertex->normal[2] * matrix->axis[2][1]) + vertex->normal[1] * matrix->axis[1][1];
    outNormal[2] =
        (vertex->normal[0] * matrix->axis[0][2] + vertex->normal[2] * matrix->axis[2][2]) + vertex->normal[1] * matrix->axis[1][2];
}

/* NOT_FROM_ORIGINAL_SOURCE: portable scalar spelling of the four-lane SSE
 * arithmetic used by RB_SurfaceXModelRigidSSE and WeightSSE. */
static void RB_TransformVertexSSEOrder(const DObjSkelMat *matrix, const vec3_t normal, const vec3_t position, vec3_t outPosition,
                                       vec3_t outNormal)
{
    for (int32_t component = 0; component < 3; ++component) {
        outPosition[component] = ((position[0] * matrix->axis[0][component] + position[1] * matrix->axis[1][component]) +
                                  position[2] * matrix->axis[2][component]) +
                                 matrix->origin[component];
        outNormal[component] =
            (normal[0] * matrix->axis[0][component] + normal[1] * matrix->axis[1][component]) + normal[2] * matrix->axis[2][component];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: the weighted SSE path adds the matrix-origin lane
 * before the third axis product for its primary point.  That association is
 * distinct from both the rigid SSE path and its own additive-weight loop. */
static void RB_TransformWeightedPrimarySSEOrder(const DObjSkelMat *matrix, const vec3_t normal, const vec3_t position, vec3_t outPosition,
                                                vec3_t outNormal)
{
    for (int32_t component = 0; component < 3; ++component) {
        outPosition[component] =
            ((position[0] * matrix->axis[0][component] + position[1] * matrix->axis[1][component]) + matrix->origin[component]) +
            position[2] * matrix->axis[2][component];
        outNormal[component] =
            (normal[0] * matrix->axis[0][component] + normal[1] * matrix->axis[1][component]) + normal[2] * matrix->axis[2][component];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the inlined x87 loop at
 * CoDUOMP.exe 0x0052116f..0x005211d5. It associates X as axis 1 + axis 2 +
 * axis 0, while Y and Z associate axis 0 + axis 1 + axis 2. */
static void coduomp_accumulate_weighted_point_x87_order(const XSurfaceWeightedPoint *point, const DObjSkelMat *matrix, vec3_t outPosition)
{
    const long double transformedX =
        (((long double)point->blend.position[1] * matrix->axis[1][0] + (long double)point->blend.position[2] * matrix->axis[2][0]) +
         (long double)point->blend.position[0] * matrix->axis[0][0]) +
        (long double)matrix->origin[0];
    outPosition[0] = (float)(transformedX * point->weight + (long double)outPosition[0]);
    for (int32_t component = 1; component < 3; ++component) {
        const long double transformed = (((long double)point->blend.position[0] * matrix->axis[0][component] +
                                          (long double)point->blend.position[1] * matrix->axis[1][component]) +
                                         (long double)point->blend.position[2] * matrix->axis[2][component]) +
                                        (long double)matrix->origin[component];

        outPosition[component] = (float)(transformed * point->weight + (long double)outPosition[component]);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
void RB_SetModelMatrixForRigidSurface(const renderer_entity_surface_t *entitySurface)
{
    const XSurface *surface = entitySurface->surface;
    const DObjSkelMat *basePose = RB_GetXModelBasePose(entitySurface);
    /* 0x005202aa MOVSX preserves the signed 16-bit byte offset before
     * adding it to the model's base-pose span. */
    const DObjSkelMat *boneMatrix = (const DObjSkelMat *)(const void *)((const uint8_t *)basePose + (int32_t)surface->boneIndex);
    const float *modelMatrix = backEnd.orientation.modelMatrix;
    float result[16];

    for (int32_t row = 0; row < 3; ++row) {
        result[row] = (modelMatrix[row] * boneMatrix->axis[0][0] + modelMatrix[8 + row] * boneMatrix->axis[0][2]) +
                      modelMatrix[4 + row] * boneMatrix->axis[0][1];
        result[4 + row] = (modelMatrix[row] * boneMatrix->axis[1][0] + modelMatrix[4 + row] * boneMatrix->axis[1][1]) +
                          modelMatrix[8 + row] * boneMatrix->axis[1][2];
        result[8 + row] = (modelMatrix[row] * boneMatrix->axis[2][0] + modelMatrix[4 + row] * boneMatrix->axis[2][1]) +
                          modelMatrix[8 + row] * boneMatrix->axis[2][2];
        result[12 + row] = ((modelMatrix[row] * boneMatrix->origin[0] + modelMatrix[4 + row] * boneMatrix->origin[1]) +
                            modelMatrix[8 + row] * boneMatrix->origin[2]) +
                           modelMatrix[12 + row];
    }
    result[3] = 0.0f;
    result[7] = 0.0f;
    result[11] = 0.0f;
    result[15] = 1.0f;

    qglLoadMatrixf(result);
}

/* Source: CoDUOMP.exe 0x005204a0..0x00520582.
 * Name: same-module Mac symbol RB_SurfaceXModelRigid. Retail treats a
 * negative vertex count as a nonzero dword countdown and trusts the signed
 * bone-matrix byte offset; loader validation plus the sink guard below remove
 * that malformed input domain. */
void RB_SurfaceXModelRigid(renderer_surface_t *surfaceData)
{
    renderer_entity_surface_t *entitySurface = (renderer_entity_surface_t *)surfaceData;
    const XSurface *surface = entitySurface->surface;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (surface->vertexCount <= 0) {
        return;
    }
    rb_xmodel_tess_range_t range = RB_BeginXModelTessRange(surface);
    const DObjSkelMat *basePose = RB_GetXModelBasePose(entitySurface);
    /* 0x00520533 MOVSX uses the same signed byte offset as the matrix-load
     * path, rather than converting the field to an unsigned matrix index. */
    const DObjSkelMat *boneMatrix = (const DObjSkelMat *)(const void *)((const uint8_t *)basePose + (int32_t)surface->boneIndex);

    uint32_t remainingVertices = (uint32_t)(int32_t)surface->vertexCount;
    if (remainingVertices != 0U) {
        uint32_t vertexIndex = 0;

        do {
            RB_TransformRigidVertex(boneMatrix, &surface->vertexData.rigidVertices[vertexIndex], range.positions[vertexIndex],
                                    range.normals[vertexIndex]);
            ++vertexIndex;
            remainingVertices -= 1u;
        } while (remainingVertices != 0U);
    }
}

/* Source: CoDUOMP.exe 0x00520590..0x005206df.
 * Name: same-module Mac symbol RB_SurfaceXModelRigidSSE.  Portable source
 * preserves the packed SSE add order while storing only the three live lanes
 * instead of the original overlapping four-lane writes. The emitted vertex
 * loop is unconditional and decrements the sign-extended count to zero.
 * Retail therefore walks the input and tessellation arrays for a zero or
 * negative count; the sink guard below excludes that malformed domain. */
void RB_SurfaceXModelRigidSSE(renderer_surface_t *surfaceData)
{
    renderer_entity_surface_t *entitySurface = (renderer_entity_surface_t *)surfaceData;
    const XSurface *surface = entitySurface->surface;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (surface->vertexCount <= 0) {
        return;
    }
    rb_xmodel_tess_range_t range = RB_BeginXModelTessRange(surface);
    const DObjSkelMat *basePose = RB_GetXModelBasePose(entitySurface);
    /* 0x00520636 MOVSX proves that this SSE twin also uses the signed byte
     * offset directly. */
    const DObjSkelMat *boneMatrix = (const DObjSkelMat *)(const void *)((const uint8_t *)basePose + (int32_t)surface->boneIndex);
    uint32_t remainingVertices = (uint32_t)(int32_t)surface->vertexCount;
    uint32_t vertexIndex = 0;

    do {
        const XSurfaceRigidVert *vertex = &surface->vertexData.rigidVertices[vertexIndex];
        RB_TransformVertexSSEOrder(boneMatrix, vertex->normal, vertex->position, range.positions[vertexIndex], range.normals[vertexIndex]);
        ++vertexIndex;
        remainingVertices -= 1u;
    } while (remainingVertices != 0U);
}

/* Source: CoDUOMP.exe 0x005206e0..0x00520971.
 * Name: exact same-module Mac symbol RB_SurfaceXModelRigidARB. */
void RB_SurfaceXModelRigidARB(renderer_surface_t *surfaceData)
{
    renderer_entity_surface_t *entitySurface = (renderer_entity_surface_t *)surfaceData;
    static const char logFormat[] = "--- RB_SurfaceXModelRigidARB( %s ), model: %s ---\n";
    const XSurface *surface = entitySurface->surface;
    const XSurfaceOptimizedDataARB *optimized = surface->optimizedDataARB;

    RB_LogOptimizedXModelSurface(logFormat, entitySurface);
    RB_SetModelMatrixForRigidSurface(entitySurface);
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);
    GL_ClientState(GLS_CLIENT_TEXCOORD0_ARRAY | GLS_CLIENT_NORMAL_ARRAY | GLS_CLIENT_VERTEX_ARRAY);

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, optimized->vertexBuffer);
    qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, optimized->indexBuffer);
    qglNormalPointer(GL_FLOAT, (int32_t)sizeof(XSurfaceARBVert), (const void *)(uintptr_t)offsetof(XSurfaceARBVert, rigidVertex.normal));
    qglVertexPointer(3, GL_FLOAT, (int32_t)sizeof(XSurfaceARBVert),
                     (const void *)(uintptr_t)offsetof(XSurfaceARBVert, rigidVertex.position));

    RB_DrawOptimizedXModelPasses(surface, NULL, NULL);

    qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    qglLoadMatrixf(backEnd.orientation.modelMatrix);
    GLimp_LogComment("----------\n");
    backEnd.pc.indexCount = (int32_t)((uint32_t)backEnd.pc.indexCount + (uint32_t)(int32_t)surface->triangleCount * 3u);
    backEnd.pc.vertexCount = (int32_t)((uint32_t)backEnd.pc.vertexCount + (uint32_t)(int32_t)surface->vertexCount);
}

/* Source: CoDUOMP.exe 0x00520980..0x00520df8.
 * Name: exact same-module Mac symbol RB_SurfaceXModelRigidATI. */
void RB_SurfaceXModelRigidATI(renderer_surface_t *surfaceData)
{
    renderer_entity_surface_t *entitySurface = (renderer_entity_surface_t *)surfaceData;
    static const char logFormat[] = "--- RB_SurfaceXModelRigidATI( %s ), model: %s ---\n";
    const XSurface *surface = entitySurface->surface;
    const XSurfaceOptimizedDataATI *optimized = surface->optimizedDataATI;
    const int32_t indexCount = surface->triangleCount * 3;

    RB_LogOptimizedXModelSurface(logFormat, entitySurface);
    RB_EndMultitexture();
    RB_SetModelMatrixForRigidSurface(entitySurface);
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);
    GL_ClientState(GLS_CLIENT_TEXCOORD0_ARRAY | GLS_CLIENT_NORMAL_ARRAY | GLS_CLIENT_VERTEX_ARRAY);

    qglArrayObjectATI(GL_TEXTURE_COORD_ARRAY, 2, GL_FLOAT, (int32_t)sizeof(XSurfaceARBVert), optimized->objectBuffer,
                      optimized->vertexOffset);
    qglArrayObjectATI(GL_NORMAL_ARRAY, 3, GL_FLOAT, (int32_t)sizeof(XSurfaceARBVert), optimized->objectBuffer,
                      optimized->vertexOffset + (uint32_t)offsetof(XSurfaceARBVert, rigidVertex.normal));
    qglArrayObjectATI(GL_VERTEX_ARRAY, 3, GL_FLOAT, (int32_t)sizeof(XSurfaceARBVert), optimized->objectBuffer,
                      optimized->vertexOffset + (uint32_t)offsetof(XSurfaceARBVert, rigidVertex.position));

    if (glConfig.elementArrayATIAvailable != qfalse) {
        qglEnable(GL_ELEMENT_ARRAY_ATI);
        for (int32_t passIndex = 0; passIndex < tess.shader->numUnfoggedPasses; ++passIndex) {
            RB_BindOptimizedXModelStage(tess.shader->stages[passIndex]);
            qglArrayObjectATI(GL_ELEMENT_ARRAY_ATI, 1, GL_UNSIGNED_SHORT, 0, optimized->objectBuffer, optimized->indexOffset);
            GL_DrawElementArrayATI(GL_TRIANGLES, indexCount);
        }
    } else {
        for (int32_t passIndex = 0; passIndex < tess.shader->numUnfoggedPasses; ++passIndex) {
            RB_BindOptimizedXModelStage(tess.shader->stages[passIndex]);
            GL_DrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, surface->triangles);
        }
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.1);

        RB_BindOptimizedXModelStage(tr.defaultShader->stages[0]);
        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(indexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(tr.identityLight, 0.0f, tr.identityLight);
        }
        GL_DrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, surface->triangles);
        qglDepthRange(0.0, 1.0);
    }

    if (glConfig.elementArrayATIAvailable != qfalse)
        qglDisable(GL_ELEMENT_ARRAY_ATI);

    qglLoadMatrixf(backEnd.orientation.modelMatrix);
    GLimp_LogComment("----------\n");
    backEnd.pc.indexCount = (int32_t)((uint32_t)backEnd.pc.indexCount + (uint32_t)indexCount);
    backEnd.pc.vertexCount = (int32_t)((uint32_t)backEnd.pc.vertexCount + (uint32_t)(int32_t)surface->vertexCount);
}

/* Source: CoDUOMP.exe 0x00520e00..0x0052106a.
 * Name: exact same-module Mac symbol RB_SurfaceXModelRigidNV. */
void RB_SurfaceXModelRigidNV(renderer_surface_t *surfaceData)
{
    renderer_entity_surface_t *entitySurface = (renderer_entity_surface_t *)surfaceData;
    static const char logFormat[] = "--- RB_SurfaceXModelRigidNV( %s ), model: %s ---\n";
    const XSurface *surface = entitySurface->surface;
    const XSurfaceOptimizedDataNV *optimized = surface->optimizedDataNV;
    const XSurfaceARBVert *vertices = (const XSurfaceARBVert *)(const void *)optimized->interleavedVertices;

    RB_LogOptimizedXModelSurface(logFormat, entitySurface);
    RB_SetModelMatrixForRigidSurface(entitySurface);
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);
    GL_ClientState(GLS_CLIENT_TEXCOORD0_ARRAY | GLS_CLIENT_NORMAL_ARRAY | GLS_CLIENT_VERTEX_ARRAY);

    qglNormalPointer(GL_FLOAT, (int32_t)sizeof(vertices[0]), vertices[0].rigidVertex.normal);
    qglVertexPointer(3, GL_FLOAT, (int32_t)sizeof(vertices[0]), vertices[0].rigidVertex.position);

    RB_DrawOptimizedXModelPasses(surface, vertices, surface->triangles);

    qglLoadMatrixf(backEnd.orientation.modelMatrix);
    GLimp_LogComment("----------\n");
    backEnd.pc.indexCount = (int32_t)((uint32_t)backEnd.pc.indexCount + (uint32_t)(int32_t)surface->triangleCount * 3u);
    backEnd.pc.vertexCount = (int32_t)((uint32_t)backEnd.pc.vertexCount + (uint32_t)(int32_t)surface->vertexCount);
}

/* Source: CoDUOMP.exe 0x00521070..0x005211f2.
 * Name: same-module Mac symbol RB_SurfaceXModelWeight. Retail vertex and
 * additive-weight loops skip only zero before decrementing modulo 2^32, and
 * directly trust matrix offsets. Loader validation and the sink guards bound
 * those retained stream walks. */
void RB_SurfaceXModelWeight(renderer_surface_t *surfaceData)
{
    renderer_entity_surface_t *entitySurface = (renderer_entity_surface_t *)surfaceData;
    const XSurface *surface = entitySurface->surface;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (surface->vertexCount <= 0) {
        return;
    }
    rb_xmodel_tess_range_t range = RB_BeginXModelTessRange(surface);
    const DObjSkelMat *basePose = RB_GetXModelBasePose(entitySurface);
    const uint8_t *primaryCursor = surface->vertexData.blendPrimaryStream;
    const XSurfaceWeightedPoint *additivePoint = surface->weightedPoints;
    uint32_t remainingVertices = (uint32_t)(int32_t)surface->vertexCount;
    uint32_t vertexIndex = 0;

    if (remainingVertices == 0U)
        return;

    do {
        const XSurfaceBlendVertNoWeight *primary = (const XSurfaceBlendVertNoWeight *)(const void *)primaryCursor;
        const DObjSkelMat *primaryMatrix = (const DObjSkelMat *)(const void *)((const uint8_t *)basePose + primary->blend.boneMatrixOffset);
        const XSurfaceRigidVert primaryVertex = {
            {primary->normal[0], primary->normal[1], primary->normal[2]},
            {primary->blend.position[0], primary->blend.position[1], primary->blend.position[2]},
        };

        RB_TransformRigidVertex(primaryMatrix, &primaryVertex, range.positions[vertexIndex], range.normals[vertexIndex]);

        if (primary->additiveWeightCount <= 0) {
            primaryCursor += sizeof(*primary);
            ++vertexIndex;
            remainingVertices -= 1u;
            continue;
        }

        const XSurfaceBlendVert *expandedPrimary = (const XSurfaceBlendVert *)(const void *)primaryCursor;
        range.positions[vertexIndex][0] *= expandedPrimary->primaryWeight;
        range.positions[vertexIndex][1] *= expandedPrimary->primaryWeight;
        range.positions[vertexIndex][2] *= expandedPrimary->primaryWeight;

        uint32_t remainingWeights = (uint32_t)primary->additiveWeightCount;
        do {
            const DObjSkelMat *additiveMatrix =
                (const DObjSkelMat *)(const void *)((const uint8_t *)basePose + additivePoint->blend.boneMatrixOffset);
            coduomp_accumulate_weighted_point_x87_order(additivePoint, additiveMatrix, range.positions[vertexIndex]);
            ++additivePoint;
            remainingWeights -= 1u;
        } while (remainingWeights != 0U);
        primaryCursor += sizeof(*expandedPrimary);
        ++vertexIndex;
        remainingVertices -= 1u;
    } while (remainingVertices != 0U);
}

/* Source: CoDUOMP.exe 0x00521200..0x005213d7.
 * Name: same-module Mac symbol RB_SurfaceXModelWeightSSE. Its vertex loop is
 * unconditional; a nonzero additive count likewise decrements modulo 2^32
 * until zero. Retail therefore walks the serialized streams and pose storage
 * for zero or negative counts; loader validation and the guards below exclude
 * that malformed domain. */
void RB_SurfaceXModelWeightSSE(renderer_surface_t *surfaceData)
{
    renderer_entity_surface_t *entitySurface = (renderer_entity_surface_t *)surfaceData;
    const XSurface *surface = entitySurface->surface;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (surface->vertexCount <= 0) {
        return;
    }
    rb_xmodel_tess_range_t range = RB_BeginXModelTessRange(surface);
    const DObjSkelMat *basePose = RB_GetXModelBasePose(entitySurface);
    const uint8_t *primaryCursor = surface->vertexData.blendPrimaryStream;
    const XSurfaceWeightedPoint *additivePoint = surface->weightedPoints;
    uint32_t remainingVertices = (uint32_t)(int32_t)surface->vertexCount;
    uint32_t vertexIndex = 0;

    do {
        const XSurfaceBlendVertNoWeight *primary = (const XSurfaceBlendVertNoWeight *)(const void *)primaryCursor;
        const DObjSkelMat *primaryMatrix = (const DObjSkelMat *)(const void *)((const uint8_t *)basePose + primary->blend.boneMatrixOffset);

        RB_TransformWeightedPrimarySSEOrder(primaryMatrix, primary->normal, primary->blend.position, range.positions[vertexIndex],
                                            range.normals[vertexIndex]);

        if (primary->additiveWeightCount <= 0) {
            primaryCursor += sizeof(*primary);
            ++vertexIndex;
            remainingVertices -= 1u;
            continue;
        }

        const XSurfaceBlendVert *expandedPrimary = (const XSurfaceBlendVert *)(const void *)primaryCursor;
        for (int32_t component = 0; component < 3; ++component) {
            range.positions[vertexIndex][component] *= expandedPrimary->primaryWeight;
        }

        uint32_t remainingWeights = (uint32_t)primary->additiveWeightCount;
        do {
            const DObjSkelMat *additiveMatrix =
                (const DObjSkelMat *)(const void *)((const uint8_t *)basePose + additivePoint->blend.boneMatrixOffset);
            vec3_t transformed;
            vec3_t unusedNormal;
            /* The original inlined path has no addressable zero-vector
             * object; its unused normal calculation is eliminated. Keep the
             * source-factoring input automatic so it does not invent one. */
            const vec3_t zeroNormal = {0.0f, 0.0f, 0.0f};

            RB_TransformVertexSSEOrder(additiveMatrix, zeroNormal, additivePoint->blend.position, transformed, unusedNormal);
            for (int32_t component = 0; component < 3; ++component) {
                range.positions[vertexIndex][component] += transformed[component] * additivePoint->weight;
            }
            ++additivePoint;
            remainingWeights -= 1u;
        } while (remainingWeights != 0U);
        primaryCursor += sizeof(*expandedPrimary);
        ++vertexIndex;
        remainingVertices -= 1u;
    } while (remainingVertices != 0U);
}
