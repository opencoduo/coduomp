#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "renderer_cvars.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef enum rb_static_model_show_color_e {
    RB_STATIC_MODEL_SHOW_COLOR_BLUE,
    RB_STATIC_MODEL_SHOW_COLOR_T2V3
} rb_static_model_show_color_t;

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the color-generator
 * selection repeated by the T2V3 static-model backends. */
static void RB_SetStaticModelStageColor(const shaderStage_t *stage)
{
    if (stage->rgbGen == CGEN_LIGHTING_PRECALC) {
        if (r_fullbright->integer != 0) {
            qglColor3f(tr.identityLight, tr.identityLight, tr.identityLight);
        } else {
            qglColor4ubv(backEnd.currentEntity->e.shaderRGBA);
        }
    } else {
        qglColor4ubv(stage->constantColor);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the two exact
 * diagnostic colors selected by the optimized static-model paths. */
static void RB_SetStaticModelShowColor(int32_t indexCount, rb_static_model_show_color_t colorType)
{
    if (r_showtris->integer >= 5) {
        uint8_t color[4];
        RB_ChooseSurfaceCountColor(indexCount, color);
        qglColor4ubv(color);
        return;
    }

    if (colorType == RB_STATIC_MODEL_SHOW_COLOR_T2V3) {
        qglColor3f(tr.identityLight * 0.5f, tr.identityLight * 0.21875f, tr.identityLight);
    } else {
        qglColor3f(tr.identityLight * 0.25f, tr.identityLight * 0.25f, tr.identityLight);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the literal
 * show-triangle stage image bind used by the ARB and T2N3V3 paths. */
static void RB_BindStaticModelShowStage(shaderStage_t *stage)
{
    GL_State(stage->stateBits);
    GL_Bind(RB_GetAnimatedImage(&stage->bundle[0], glState.currenttmu));
}

/* NOT_FROM_ORIGINAL_SOURCE: the four optimized pointer-backed functions
 * (generic and NV, with or without normals) have one machine-code shape.
 * The booleans retain the two proved source vertex layouts and their distinct
 * color/show-stage behavior. */
static void RB_DrawStaticModelPointer(renderer_static_model_surface_t *surface, qboolean hasNormals, qboolean colorsFromStage,
                                      const char *logMessage)
{
    const uint8_t *vertices = surface->optimized.vertices;
    const int32_t vertexStride =
        hasNormals ? (int32_t)sizeof(renderer_static_model_t2n3v3_vertex_t) : (int32_t)sizeof(renderer_static_model_t2v3_vertex_t);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const void *baseTexCoords[R_MAX_TEXTURE_UNITS] = {vertices, vertices, vertices, vertices, vertices, vertices, vertices, vertices};

    GLimp_LogComment(logMessage);
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);
    GL_ClientState(GLS_CLIENT_TEXCOORD0_ARRAY | (hasNormals ? GLS_CLIENT_NORMAL_ARRAY : 0) | GLS_CLIENT_VERTEX_ARRAY);

    if (hasNormals) {
        qglNormalPointer(GL_FLOAT, vertexStride, vertices + offsetof(renderer_static_model_t2n3v3_vertex_t, normal));
        qglVertexPointer(3, GL_FLOAT, vertexStride, vertices + offsetof(renderer_static_model_t2n3v3_vertex_t, position));
    } else {
        qglVertexPointer(3, GL_FLOAT, vertexStride, vertices + offsetof(renderer_static_model_t2v3_vertex_t, position));
    }

    for (int32_t stageIndex = 0; stageIndex < tess.activeStageCount; ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if (colorsFromStage)
            RB_SetStaticModelStageColor(stage);
        GL_State(stage->stateBits);
        RB_SetupMultitexture(stage, baseTexCoords, vertexStride);
        GL_DrawElements(GL_TRIANGLES, surface->indexCount, GL_UNSIGNED_SHORT, surface->indices);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *stage = tr.showTrisShader->stages[0];
        if (colorsFromStage) {
            GL_State(stage->stateBits);
            RB_SetupMultitexture(stage, baseTexCoords, vertexStride);
        } else {
            RB_BindStaticModelShowStage(stage);
        }

        RB_SetStaticModelShowColor(surface->indexCount,
                                   colorsFromStage ? RB_STATIC_MODEL_SHOW_COLOR_T2V3 : RB_STATIC_MODEL_SHOW_COLOR_BLUE);
        GL_DrawElements(GL_TRIANGLES, surface->indexCount, GL_UNSIGNED_SHORT, surface->indices);
        qglDepthRange(0.0, 1.0);
    }

    GLimp_LogComment("----------\n");
    backEnd.pc.indexCount = (int32_t)((uint32_t)backEnd.pc.indexCount + (uint32_t)surface->indexCount);
    backEnd.pc.vertexCount = (int32_t)((uint32_t)backEnd.pc.vertexCount + (uint32_t)surface->vertexCount);
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the two ARB buffer
 * backends. Both use the same VBO/IBO setup; only the interleaved vertex
 * layout and the T2V3 pass-color update differ. */
static void RB_DrawStaticModelARB(renderer_static_model_surface_t *surface, qboolean hasNormals, const char *logMessage)
{
    const int32_t vertexStride =
        hasNormals ? (int32_t)sizeof(renderer_static_model_t2n3v3_vertex_t) : (int32_t)sizeof(renderer_static_model_t2v3_vertex_t);
    /* Full-size for the same determinized reason as the pointer path above;
     * offsets into the bound VBO, entry [0] repeated. */
    const void *baseTexCoords[R_MAX_TEXTURE_UNITS] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

    GLimp_LogComment(logMessage);
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);
    GL_ClientState(GLS_CLIENT_TEXCOORD0_ARRAY | (hasNormals ? GLS_CLIENT_NORMAL_ARRAY : 0) | GLS_CLIENT_VERTEX_ARRAY);

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, surface->optimized.vertexBuffer);
    qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, surface->backend.arb.indexBuffer);

    if (hasNormals) {
        qglNormalPointer(GL_FLOAT, vertexStride, (const void *)(uintptr_t)offsetof(renderer_static_model_t2n3v3_vertex_t, normal));
        qglVertexPointer(3, GL_FLOAT, vertexStride, (const void *)(uintptr_t)offsetof(renderer_static_model_t2n3v3_vertex_t, position));
    } else {
        qglVertexPointer(3, GL_FLOAT, vertexStride, (const void *)(uintptr_t)offsetof(renderer_static_model_t2v3_vertex_t, position));
    }

    for (int32_t stageIndex = 0; stageIndex < tess.activeStageCount; ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        GL_State(stage->stateBits);
        RB_SetupMultitexture(stage, baseTexCoords, vertexStride);
        if (!hasNormals)
            RB_SetStaticModelStageColor(stage);
        GL_DrawElements(GL_TRIANGLES, surface->indexCount, GL_UNSIGNED_SHORT, NULL);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        RB_BindStaticModelShowStage(tr.showTrisShader->stages[0]);
        RB_SetStaticModelShowColor(surface->indexCount, RB_STATIC_MODEL_SHOW_COLOR_BLUE);
        GL_DrawElements(GL_TRIANGLES, surface->indexCount, GL_UNSIGNED_SHORT, NULL);
        qglDepthRange(0.0, 1.0);
    }

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    GLimp_LogComment("----------\n");
    backEnd.pc.indexCount = (int32_t)((uint32_t)backEnd.pc.indexCount + (uint32_t)surface->indexCount);
    backEnd.pc.vertexCount = (int32_t)((uint32_t)backEnd.pc.vertexCount + (uint32_t)surface->vertexCount);
}

/* Source: CoDUOMP.exe 0x0051a070..0x0051a18c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051a070_0051a18d.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceStaticModel. */
void RB_SurfaceStaticModel(renderer_surface_t *surfaceData)
{
    renderer_static_model_surface_t *surface = (renderer_static_model_surface_t *)surfaceData;
    if (r_logFile->integer != 0) {
        GLimp_LogComment(va("--- RB_SurfaceStaticModel( %s ) ---\n", tess.shader->name));
    }

    const int32_t firstVertex = tess.vertexCount;
    const uint32_t firstComponent = (uint32_t)firstVertex * (uint32_t)tess.vertexComponentCount;
    memcpy(&tess.xyz[firstComponent], surface->vertices, (size_t)surface->vertexCount * sizeof(surface->vertices[0]));
    memcpy(&tess.texCoords[R_TESS_BASE_TEXCOORD_SET][firstVertex], surface->texCoords,
           (size_t)surface->vertexCount * sizeof(surface->texCoords[0]));

    if ((tess.shader->surfaceFlags & (SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_REQUIRES_NORMAL_ARRAY)) != 0) {
        memcpy(&tess.stageNormals[firstVertex], surface->normals, (size_t)surface->vertexCount * sizeof(surface->normals[0]));
        tess.requiresVertexBasis = qtrue;
    }

    for (uint16_t index = 0; index < surface->indexCount; ++index) {
        const uint32_t destinationIndex = (uint32_t)tess.indexCount + (uint32_t)index;
        tess.indexes[destinationIndex] = (uint16_t)((uint32_t)surface->indices[index] + (uint32_t)firstVertex);
    }
    tess.indexCount = (int32_t)((uint32_t)tess.indexCount + (uint32_t)surface->indexCount);
    tess.vertexCount = (int32_t)((uint32_t)tess.vertexCount + (uint32_t)surface->vertexCount);
}

/* Source: CoDUOMP.exe 0x0051a190..0x0051a284.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051a190_0051a285.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceStaticModelCached.
 *
 * The original adds the same 16-bit vertex offset to two packed indices at
 * once. The 32-bit addition (including its inter-lane carry behavior) is
 * preserved with memcpy so the maintained C has no alignment/aliasing UB. */
void RB_SurfaceStaticModelCached(renderer_surface_t *surfaceData)
{
    renderer_cached_static_model_surface_t *cachedSurface = (renderer_cached_static_model_surface_t *)surfaceData;
    if (r_logFile->integer != 0) {
        GLimp_LogComment(va("--- RB_SurfaceStaticModelCached( %s ) ---\n", tess.shader->name));
    }

    renderer_static_model_surface_t *surface = cachedSurface->cached.source;
    const int32_t firstVertex = cachedSurface->cached.vertexOffset;
    const int32_t vertexEnd = (int32_t)((uint32_t)firstVertex + (uint32_t)surface->vertexCount);

    if (tess.optimizedVertexEnd == 0) {
        tess.optimizedFirstVertex = firstVertex;
        tess.optimizedVertexEnd = vertexEnd;
    } else {
        if (firstVertex < tess.optimizedFirstVertex)
            tess.optimizedFirstVertex = firstVertex;
        if (vertexEnd > tess.optimizedVertexEnd)
            tess.optimizedVertexEnd = vertexEnd;
    }

    uint16_t *destination = &tess.optimizedIndexes[tess.renderedIndexCount];
    const uint32_t packedVertexOffset = (uint32_t)firstVertex | ((uint32_t)firstVertex << 16);
    const int32_t sixIndexBlockCount = surface->indexCount / 6;

    tess.renderedIndexCount = (int32_t)((uint32_t)tess.renderedIndexCount + (uint32_t)surface->indexCount);

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    for (uint32_t blockIndex = 0; blockIndex < (uint32_t)sixIndexBlockCount; ++blockIndex) {
        for (int32_t packedWord = 0; packedWord < 3; ++packedWord) {
            const uint32_t packedIndex = blockIndex * 3u + (uint32_t)packedWord;
            uint32_t indices;
            memcpy(&indices, &surface->indices[packedIndex * 2], sizeof(indices));
            indices += packedVertexOffset;
            memcpy(&destination[packedIndex * 2], &indices, sizeof(indices));
        }
    }
}

/* Source: CoDUOMP.exe 0x0051a290..0x0051a522.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051a290_0051a523.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceStaticModelT2V3_ARB. */
void RB_SurfaceStaticModelT2V3_ARB(renderer_surface_t *surfaceData)
{
    renderer_static_model_surface_t *surface = (renderer_static_model_surface_t *)surfaceData;
    RB_DrawStaticModelARB(surface, qfalse, "--- RB_SurfaceStaticModelT2V3_ARB ---\n");
}

/* Source: CoDUOMP.exe 0x0051a530..0x0051a786.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051a530_0051a787.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceStaticModelT2N3V3_ARB. */
void RB_SurfaceStaticModelT2N3V3_ARB(renderer_surface_t *surfaceData)
{
    renderer_static_model_surface_t *surface = (renderer_static_model_surface_t *)surfaceData;
    RB_DrawStaticModelARB(surface, qtrue, "--- RB_SurfaceStaticModelT2N3V3_ARB ---\n");
}

/* Source: CoDUOMP.exe 0x0051a790..0x0051ac5c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051a790_0051ac5d.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceStaticModelATI. The function
 * handles both ATI surface selectors, as proved by the type-18 comparison and
 * the two qglArrayObjectATI layouts. */
void RB_SurfaceStaticModelATI(renderer_surface_t *surfaceData)
{
    renderer_static_model_surface_t *surface = (renderer_static_model_surface_t *)surfaceData;
    const qboolean hasNormals = surface->surfaceType != R_SURFACE_STATIC_MODEL_T2V3_ATI;
    const int32_t vertexStride =
        hasNormals ? (int32_t)sizeof(renderer_static_model_t2n3v3_vertex_t) : (int32_t)sizeof(renderer_static_model_t2v3_vertex_t);
    const uint32_t objectBuffer = surface->optimized.atiObjectBuffer;
    const uint32_t vertexOffset = surface->backend.ati.vertexOffset;
    /* Full-size for the same determinized reason as the pointer path above;
     * ATI object-buffer offsets, entry [0] repeated. */
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS] = {vertexOffset, vertexOffset, vertexOffset, vertexOffset,
                                                           vertexOffset, vertexOffset, vertexOffset, vertexOffset};

    if (r_logFile->integer != 0) {
        GLimp_LogComment(va("--- RB_SurfaceStaticModelATI( %s ) ---\n", tess.shader->name));
    }

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);
    GL_ClientState(GLS_CLIENT_TEXCOORD0_ARRAY | (hasNormals ? GLS_CLIENT_NORMAL_ARRAY : 0) | GLS_CLIENT_VERTEX_ARRAY);

    if (hasNormals) {
        qglArrayObjectATI(GL_NORMAL_ARRAY, 3, GL_FLOAT, vertexStride, objectBuffer,
                          vertexOffset + (uint32_t)offsetof(renderer_static_model_t2n3v3_vertex_t, normal));
        qglArrayObjectATI(GL_VERTEX_ARRAY, 3, GL_FLOAT, vertexStride, objectBuffer,
                          vertexOffset + (uint32_t)offsetof(renderer_static_model_t2n3v3_vertex_t, position));
    } else {
        qglArrayObjectATI(GL_VERTEX_ARRAY, 3, GL_FLOAT, vertexStride, objectBuffer,
                          vertexOffset + (uint32_t)offsetof(renderer_static_model_t2v3_vertex_t, position));
    }

    if (glConfig.elementArrayATIAvailable != qfalse) {
        qglEnable(GL_ELEMENT_ARRAY_ATI);
        qglArrayObjectATI(GL_ELEMENT_ARRAY_ATI, 1, GL_UNSIGNED_SHORT, 0, objectBuffer, surface->backend.ati.indexOffset);
    }

    for (int32_t stageIndex = 0; stageIndex < tess.activeStageCount; ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if (!hasNormals)
            RB_SetStaticModelStageColor(stage);
        GL_State(stage->stateBits);
        RB_SetupMultitextureATI(stage, objectBuffer, texCoordOffsets, vertexStride);

        if (glConfig.elementArrayATIAvailable != qfalse) {
            GL_DrawElementArrayATI(GL_TRIANGLES, surface->indexCount);
        } else {
            GL_DrawElements(GL_TRIANGLES, surface->indexCount, GL_UNSIGNED_SHORT, surface->indices);
        }
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *stage = tr.showTrisShader->stages[0];
        GL_State(stage->stateBits);
        RB_SetupMultitextureATI(stage, objectBuffer, texCoordOffsets, vertexStride);
        RB_SetStaticModelShowColor(surface->indexCount, RB_STATIC_MODEL_SHOW_COLOR_T2V3);

        if (glConfig.elementArrayATIAvailable != qfalse) {
            GL_DrawElementArrayATI(GL_TRIANGLES, surface->indexCount);
        } else {
            GL_DrawElements(GL_TRIANGLES, surface->indexCount, GL_UNSIGNED_SHORT, surface->indices);
        }
        qglDepthRange(0.0, 1.0);
    }

    if (glConfig.elementArrayATIAvailable != qfalse)
        qglDisable(GL_ELEMENT_ARRAY_ATI);

    GLimp_LogComment("----------\n");
    backEnd.pc.indexCount = (int32_t)((uint32_t)backEnd.pc.indexCount + (uint32_t)surface->indexCount);
    backEnd.pc.vertexCount = (int32_t)((uint32_t)backEnd.pc.vertexCount + (uint32_t)surface->vertexCount);
}

/* Source: CoDUOMP.exe 0x0051ac60..0x0051aec7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051ac60_0051aec8.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceStaticModelT2V3_NV. */
void RB_SurfaceStaticModelT2V3_NV(renderer_surface_t *surfaceData)
{
    renderer_static_model_surface_t *surface = (renderer_static_model_surface_t *)surfaceData;
    RB_DrawStaticModelPointer(surface, qfalse, qtrue, "--- RB_SurfaceStaticModelT2V3_NV ---\n");
}

/* Source: CoDUOMP.exe 0x0051aed0..0x0051b104.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051aed0_0051b105.mcode.
 * Name: exact same-module Mac symbol
 * RB_SurfaceStaticModelT2N3V3_Generic. */
void RB_SurfaceStaticModelT2N3V3_Generic(renderer_surface_t *surfaceData)
{
    renderer_static_model_surface_t *surface = (renderer_static_model_surface_t *)surfaceData;
    RB_DrawStaticModelPointer(surface, qtrue, qfalse, "--- RB_SurfaceStaticModelT2N3V3_Generic ---\n");
}

/* Source: CoDUOMP.exe 0x0051b110..0x0051b377.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051b110_0051b378.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceStaticModelT2V3_Generic. */
void RB_SurfaceStaticModelT2V3_Generic(renderer_surface_t *surfaceData)
{
    renderer_static_model_surface_t *surface = (renderer_static_model_surface_t *)surfaceData;
    RB_DrawStaticModelPointer(surface, qfalse, qtrue, "--- RB_SurfaceStaticModelT2V3_Generic ---\n");
}

/* Source: CoDUOMP.exe 0x0051b380..0x0051b5b4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051b380_0051b5b5.mcode.
 * Name: exact same-module Mac symbol RB_SurfaceStaticModelT2N3V3_NV. */
void RB_SurfaceStaticModelT2N3V3_NV(renderer_surface_t *surfaceData)
{
    renderer_static_model_surface_t *surface = (renderer_static_model_surface_t *)surfaceData;
    RB_DrawStaticModelPointer(surface, qtrue, qfalse, "--- RB_SurfaceStaticModelT2N3V3_NV ---\n");
}
