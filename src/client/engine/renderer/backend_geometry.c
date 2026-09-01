#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "renderer_cvars.h"

#include <string.h>

/* These counts reproduce the coordinate entries explicitly initialized before
 * the original multitexture-setup calls: base plus lightmap for optimized
 * world surfaces, and base only for cached static models. They do not imply a
 * matching bound inside either setup routine. */
enum {
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    R_WORLD_BASE_TEXCOORD_COUNT = 2,
    R_CACHED_MODEL_BASE_TEXCOORD_COUNT = 1
};

/* Source: CoDUOMP.exe 0x004ed190..0x004ed4d4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ed190_004ed4d4.mcode.
 * Name: same-module Mac symbol RB_EndSurface_OptimizedGeneric, assigned by
 * the shader-owned 32-byte world-vertex stream and original selector.
 *
 * Optimized world vertices use the CPU-address member of the shader's
 * optimized storage union. The first two texture-coordinate pointers select
 * the base and lightmap pairs in the 32-byte interleaved vertex. */
void RB_EndSurface_OptimizedGeneric(void)
{
    renderer_world_interleaved_vertex_t *vertices =
        (renderer_world_interleaved_vertex_t *)(void *)
            tess.shader->optimizedVertexStorage.address;
    const void *baseTexCoords[R_MAX_TEXTURE_UNITS] = {
        vertices[0].texCoord,
        vertices[0].lightmapCoord,
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord
    };

    if (r_lightmap->integer != 0) {
        if (glState.texEnv[glState.currenttmu] != GL_REPLACE) {
            glState.texEnv[glState.currenttmu] = GL_REPLACE;
            qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
    } else {
        GL_TexEnv(tess.shader->stages[0]->bundle[1].textureEnvMode);
    }

    if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
        qglEnableClientState(GL_COLOR_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
    }
    if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    qglColorPointer(4, GL_UNSIGNED_BYTE,
                    (int32_t)sizeof(vertices[0]), vertices[0].color);
    qglVertexPointer(3, GL_FLOAT,
                     (int32_t)sizeof(vertices[0]), vertices[0].position);

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    for (int32_t stageIndex = 0;
         stageIndex < tess.activeStageCount;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        GL_State(stage->stateBits);
        RB_SetupMultitexture(stage, baseTexCoords,
                             (int32_t)sizeof(vertices[0]));
        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *showTrisStage =
            tr.showTrisShader->stages[0];
        GL_Cull(tr.showTrisShader->cullType);
        GL_State(showTrisStage->stateBits);
        RB_SetupMultitexture(showTrisStage, baseTexCoords,
                             (int32_t)sizeof(vertices[0]));

        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }

        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(tess.renderedIndexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(0.0f, tr.identityLight, 0.0f);
        }

        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
        qglDepthRange(0.0, 1.0);
    }

    GLimp_LogComment("----------\n");
}

/* Source: CoDUOMP.exe 0x004ed4e0..0x004ed862.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ed4e0_004ed862.mcode.
 * Name: same-module Mac symbol RB_EndSurface_OptimizedARB, assigned by the
 * shader-owned 32-byte world-vertex stream and original selector.
 *
 * This is the VBO counterpart of the generic optimized path. All vertex-array
 * pointers are byte offsets into the shader-owned GL_ARRAY_BUFFER_ARB. */
void RB_EndSurface_OptimizedARB(void)
{
    const void *baseTexCoords[R_MAX_TEXTURE_UNITS] = {
        NULL,
        (const void *)(uintptr_t)
            offsetof(renderer_world_interleaved_vertex_t, lightmapCoord),
        NULL, NULL, NULL, NULL, NULL, NULL
    };

    if (r_logFile->integer != 0) {
        GLimp_LogComment(
            va("--- RB_SurfaceOptimizedARB( %s ) ---\n",
               tess.shader->name));
    }

    if (r_lightmap->integer != 0) {
        if (glState.texEnv[glState.currenttmu] != GL_REPLACE) {
            glState.texEnv[glState.currenttmu] = GL_REPLACE;
            qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
    } else {
        GL_TexEnv(tess.shader->stages[0]->bundle[1].textureEnvMode);
    }

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB,
                     tess.shader->optimizedVertexStorage.glBuffer);

    if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
        qglEnableClientState(GL_COLOR_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
    }
    if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    qglColorPointer(
        4, GL_UNSIGNED_BYTE,
        (int32_t)sizeof(renderer_world_interleaved_vertex_t),
        (const void *)(uintptr_t)
            offsetof(renderer_world_interleaved_vertex_t, color));
    qglVertexPointer(
        3, GL_FLOAT,
        (int32_t)sizeof(renderer_world_interleaved_vertex_t),
        (const void *)(uintptr_t)
            offsetof(renderer_world_interleaved_vertex_t, position));

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    for (int32_t stageIndex = 0;
         stageIndex < tess.activeStageCount;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        GL_State(stage->stateBits);
        RB_SetupMultitexture(
            stage, baseTexCoords,
            (int32_t)sizeof(renderer_world_interleaved_vertex_t));
        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *showTrisStage =
            tr.showTrisShader->stages[0];
        GL_Cull(tr.showTrisShader->cullType);
        GL_State(showTrisStage->stateBits);
        RB_SetupMultitexture(
            showTrisStage, baseTexCoords,
            (int32_t)sizeof(renderer_world_interleaved_vertex_t));

        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }

        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(tess.renderedIndexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(0.0f, tr.identityLight, 0.0f);
        }

        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
        qglDepthRange(0.0, 1.0);
    }

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    GLimp_LogComment("----------\n");
}

/* Source: CoDUOMP.exe 0x004ed870..0x004edbab.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ed870_004edbab.mcode.
 * Name: same-module Mac symbol RB_EndSurface_OptimizedATI, assigned by the
 * shader-owned 32-byte world-vertex stream and original selector.
 *
 * ATI vertex-array objects identify their storage with an object-buffer name
 * plus a 32-bit byte offset; no host pointer is narrowed at this boundary. */
void RB_EndSurface_OptimizedATI(void)
{
    const uint32_t objectBuffer =
        tess.shader->optimizedVertexStorage.atiObjectBuffer;
    const uint32_t vertexOffset =
        (uint32_t)tess.shader->optimizedVertexStorageOffset;
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS] = {
        vertexOffset,
        vertexOffset +
            (uint32_t)offsetof(renderer_world_interleaved_vertex_t,
                               lightmapCoord),
        vertexOffset, vertexOffset, vertexOffset,
        vertexOffset, vertexOffset, vertexOffset
    };

    if (r_lightmap->integer != 0) {
        if (glState.texEnv[glState.currenttmu] != GL_REPLACE) {
            glState.texEnv[glState.currenttmu] = GL_REPLACE;
            qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
    } else {
        GL_TexEnv(tess.shader->stages[0]->bundle[1].textureEnvMode);
    }

    if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
        qglEnableClientState(GL_COLOR_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
    }
    if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    qglArrayObjectATI(
        GL_COLOR_ARRAY, 4, GL_UNSIGNED_BYTE,
        (int32_t)sizeof(renderer_world_interleaved_vertex_t),
        objectBuffer,
        vertexOffset +
            (uint32_t)offsetof(renderer_world_interleaved_vertex_t,
                               color));
    qglArrayObjectATI(
        GL_VERTEX_ARRAY, 3, GL_FLOAT,
        (int32_t)sizeof(renderer_world_interleaved_vertex_t),
        objectBuffer,
        vertexOffset +
            (uint32_t)offsetof(renderer_world_interleaved_vertex_t,
                               position));

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    for (int32_t stageIndex = 0;
         stageIndex < tess.activeStageCount;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        GL_State(stage->stateBits);
        RB_SetupMultitextureATI(
            stage, objectBuffer, texCoordOffsets,
            (int32_t)sizeof(renderer_world_interleaved_vertex_t));
        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *showTrisStage =
            tr.showTrisShader->stages[0];
        GL_Cull(tr.showTrisShader->cullType);
        GL_State(showTrisStage->stateBits);
        RB_SetupMultitextureATI(
            showTrisStage, objectBuffer, texCoordOffsets,
            (int32_t)sizeof(renderer_world_interleaved_vertex_t));

        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }

        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(tess.renderedIndexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(0.0f, tr.identityLight, 0.0f);
        }

        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
        qglDepthRange(0.0, 1.0);
    }

    GLimp_LogComment("----------\n");
}

/* Source: CoDUOMP.exe 0x004edbb0..0x004edf35.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004edbb0_004edf35.mcode.
 * Name: same-module Mac symbol RB_EndSurface_OptimizedNV, assigned by the
 * shader-owned 32-byte world-vertex stream and original selector.
 *
 * The NV vertex-array-range backend exposes optimized vertex memory as a native
 * CPU address, so its array setup matches the generic pointer path. */
void RB_EndSurface_OptimizedNV(void)
{
    renderer_world_interleaved_vertex_t *vertices =
        (renderer_world_interleaved_vertex_t *)(void *)
            tess.shader->optimizedVertexStorage.address;
    const void *baseTexCoords[R_MAX_TEXTURE_UNITS] = {
        vertices[0].texCoord,
        vertices[0].lightmapCoord,
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord
    };

    if (r_logFile->integer != 0) {
        GLimp_LogComment(
            va("--- RB_SurfaceOptimizedNV( %s ) ---\n",
               tess.shader->name));
    }

    if (r_lightmap->integer != 0) {
        if (glState.texEnv[glState.currenttmu] != GL_REPLACE) {
            glState.texEnv[glState.currenttmu] = GL_REPLACE;
            qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
    } else {
        GL_TexEnv(tess.shader->stages[0]->bundle[1].textureEnvMode);
    }

    if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
        qglEnableClientState(GL_COLOR_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
    }
    if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    qglColorPointer(4, GL_UNSIGNED_BYTE,
                    (int32_t)sizeof(vertices[0]), vertices[0].color);
    qglVertexPointer(3, GL_FLOAT,
                     (int32_t)sizeof(vertices[0]), vertices[0].position);

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    for (int32_t stageIndex = 0;
         stageIndex < tess.activeStageCount;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        GL_State(stage->stateBits);
        RB_SetupMultitexture(stage, baseTexCoords,
                             (int32_t)sizeof(vertices[0]));
        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *showTrisStage =
            tr.showTrisShader->stages[0];
        GL_Cull(tr.showTrisShader->cullType);
        GL_State(showTrisStage->stateBits);
        RB_SetupMultitexture(showTrisStage, baseTexCoords,
                             (int32_t)sizeof(vertices[0]));

        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }

        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(tess.renderedIndexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(0.0f, tr.identityLight, 0.0f);
        }

        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
        qglDepthRange(0.0, 1.0);
    }

    GLimp_LogComment("----------\n");
}

/* Source: CoDUOMP.exe 0x004edf40..0x004ee223.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004edf40_004ee223.mcode.
 * Name: same-module Mac symbol RB_EndSurface_CachedStaticModelGeneric,
 * assigned by the shared 24-byte cache stream and original selector.
 *
 * Cached static-model batches address the shared cache stream directly.
 * That stream has one texture-coordinate pair and a 24-byte vertex stride. */
void RB_EndSurface_CachedStaticModelGeneric(void)
{
    renderer_cached_static_model_vertex_t *vertices =
        (renderer_cached_static_model_vertex_t *)(void *)
            tr.cachedStaticModelStorage.address;
    const void *baseTexCoords[R_MAX_TEXTURE_UNITS] = {
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord
    };

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
        qglEnableClientState(GL_COLOR_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
    }
    if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    qglColorPointer(4, GL_UNSIGNED_BYTE,
                    (int32_t)sizeof(vertices[0]), vertices[0].color);
    qglVertexPointer(3, GL_FLOAT,
                     (int32_t)sizeof(vertices[0]), vertices[0].position);

    for (int32_t stageIndex = 0;
         stageIndex < tess.activeStageCount;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        GL_State(stage->stateBits);
        RB_SetupMultitexture(stage, baseTexCoords,
                             (int32_t)sizeof(vertices[0]));
        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *showTrisStage =
            tr.showTrisShader->stages[0];
        GL_Cull(tr.showTrisShader->cullType);
        GL_State(showTrisStage->stateBits);
        RB_SetupMultitexture(showTrisStage, baseTexCoords,
                             (int32_t)sizeof(vertices[0]));

        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }

        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(tess.renderedIndexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(tr.identityLight * 0.5f,
                       tr.identityLight, tr.identityLight);
        }

        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
        qglDepthRange(0.0, 1.0);
    }

    GLimp_LogComment("----------\n");
}

/* Source: CoDUOMP.exe 0x004ee230..0x004ee521.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ee230_004ee521.mcode.
 * Name: same-module Mac symbol RB_EndSurface_CachedStaticModelARB, assigned
 * by the shared 24-byte cache stream and original selector.
 *
 * The shared 24-byte static-model cache stream resides in an ARB vertex
 * buffer for this backend, so each array argument is a byte offset. */
void RB_EndSurface_CachedStaticModelARB(void)
{
    const void *baseTexCoords[R_MAX_TEXTURE_UNITS] = {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);
    qglBindBufferARB(GL_ARRAY_BUFFER_ARB,
                     tr.cachedStaticModelStorage.glBuffer);

    if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
        qglEnableClientState(GL_COLOR_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
    }
    if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    qglColorPointer(
        4, GL_UNSIGNED_BYTE,
        (int32_t)sizeof(renderer_cached_static_model_vertex_t),
        (const void *)(uintptr_t)
            offsetof(renderer_cached_static_model_vertex_t, color));
    qglVertexPointer(
        3, GL_FLOAT,
        (int32_t)sizeof(renderer_cached_static_model_vertex_t),
        (const void *)(uintptr_t)
            offsetof(renderer_cached_static_model_vertex_t, position));

    for (int32_t stageIndex = 0;
         stageIndex < tess.activeStageCount;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        GL_State(stage->stateBits);
        RB_SetupMultitexture(
            stage, baseTexCoords,
            (int32_t)sizeof(renderer_cached_static_model_vertex_t));
        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *showTrisStage =
            tr.showTrisShader->stages[0];
        GL_Cull(tr.showTrisShader->cullType);
        GL_State(showTrisStage->stateBits);
        RB_SetupMultitexture(
            showTrisStage, baseTexCoords,
            (int32_t)sizeof(renderer_cached_static_model_vertex_t));

        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }

        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(tess.renderedIndexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(tr.identityLight * 0.5f,
                       tr.identityLight, tr.identityLight);
        }

        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
        qglDepthRange(0.0, 1.0);
    }

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    GLimp_LogComment("----------\n");
}

/* Source: CoDUOMP.exe 0x004ee530..0x004ee7d8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ee530_004ee7d8.mcode.
 * Name: same-module Mac symbol RB_EndSurface_CachedStaticModelATI, assigned
 * by the shared 24-byte cache stream and original selector.
 *
 * The ATI path preserves two original differences in its show-triangle
 * replay: it neither changes culling nor disables the color array there. */
void RB_EndSurface_CachedStaticModelATI(void)
{
    const uint32_t objectBuffer =
        tr.cachedStaticModelStorage.atiObjectBuffer;
    const uint32_t vertexOffset =
        (uint32_t)tr.cachedStaticModelStorageOffset;
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS] = {
        vertexOffset, vertexOffset, vertexOffset, vertexOffset,
        vertexOffset, vertexOffset, vertexOffset, vertexOffset
    };

    if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
        qglEnableClientState(GL_COLOR_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
    }
    if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    qglArrayObjectATI(
        GL_COLOR_ARRAY, 4, GL_UNSIGNED_BYTE,
        (int32_t)sizeof(renderer_cached_static_model_vertex_t),
        objectBuffer,
        vertexOffset +
            (uint32_t)offsetof(renderer_cached_static_model_vertex_t,
                               color));
    qglArrayObjectATI(
        GL_VERTEX_ARRAY, 3, GL_FLOAT,
        (int32_t)sizeof(renderer_cached_static_model_vertex_t),
        objectBuffer,
        vertexOffset +
            (uint32_t)offsetof(renderer_cached_static_model_vertex_t,
                               position));

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    for (int32_t stageIndex = 0;
         stageIndex < tess.activeStageCount;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        GL_State(stage->stateBits);
        RB_SetupMultitextureATI(
            stage, objectBuffer, texCoordOffsets,
            (int32_t)sizeof(renderer_cached_static_model_vertex_t));
        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *showTrisStage =
            tr.showTrisShader->stages[0];
        GL_State(showTrisStage->stateBits);
        RB_SetupMultitextureATI(
            showTrisStage, objectBuffer, texCoordOffsets,
            (int32_t)sizeof(renderer_cached_static_model_vertex_t));

        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(tess.renderedIndexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(tr.identityLight * 0.5f,
                       tr.identityLight, tr.identityLight);
        }

        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
        qglDepthRange(0.0, 1.0);
    }
}

/* Source: CoDUOMP.exe 0x004ee7e0..0x004eeac3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ee7e0_004eeac3.mcode.
 * Name: same-module Mac symbol RB_EndSurface_CachedStaticModelNV, assigned
 * by the shared 24-byte cache stream and original selector.
 *
 * NV vertex-array-range storage remains directly CPU-addressable, so this
 * path consumes the shared cache stream through its native address member. */
void RB_EndSurface_CachedStaticModelNV(void)
{
    renderer_cached_static_model_vertex_t *vertices =
        (renderer_cached_static_model_vertex_t *)(void *)
            tr.cachedStaticModelStorage.address;
    const void *baseTexCoords[R_MAX_TEXTURE_UNITS] = {
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord,
        vertices[0].texCoord, vertices[0].texCoord
    };

    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
        qglEnableClientState(GL_COLOR_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
    }
    if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    qglColorPointer(4, GL_UNSIGNED_BYTE,
                    (int32_t)sizeof(vertices[0]), vertices[0].color);
    qglVertexPointer(3, GL_FLOAT,
                     (int32_t)sizeof(vertices[0]), vertices[0].position);

    for (int32_t stageIndex = 0;
         stageIndex < tess.activeStageCount;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        GL_State(stage->stateBits);
        RB_SetupMultitexture(stage, baseTexCoords,
                             (int32_t)sizeof(vertices[0]));
        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
    }

    if (r_showtris->integer != 0) {
        if ((r_showtris->integer & 1) != 0)
            qglDepthRange(0.0, 0.10000000149011612);

        shaderStage_t *showTrisStage =
            tr.showTrisShader->stages[0];
        GL_Cull(tr.showTrisShader->cullType);
        GL_State(showTrisStage->stateBits);
        RB_SetupMultitexture(showTrisStage, baseTexCoords,
                             (int32_t)sizeof(vertices[0]));

        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }

        if (r_showtris->integer >= 5) {
            uint8_t color[4];
            RB_ChooseSurfaceCountColor(tess.renderedIndexCount, color);
            qglColor4ubv(color);
        } else {
            qglColor3f(tr.identityLight * 0.5f,
                       tr.identityLight, tr.identityLight);
        }

        GL_DrawRangeElements(
            GL_TRIANGLES, (uint32_t)tess.optimizedFirstVertex,
            (uint32_t)tess.optimizedVertexEnd, tess.renderedIndexCount,
            GL_UNSIGNED_SHORT, tess.optimizedIndexes);
        qglDepthRange(0.0, 1.0);
    }

    GLimp_LogComment("----------\n");
}

/* Source: CoDUOMP.exe 0x004eead0..0x004eeb61.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eead0_004eeb62.mcode and the inline
 * jump/selector tables at 0x004eeb64..0x004eeb98.
 * Name and eight source-level callees: same-module Mac symbols.
 *
 * Values 16..24 intentionally take the default arm. The Windows selector
 * table maps only the four cached-static-model and four optimized backends
 * below, then folds their separately accumulated counts into the ordinary
 * backend totals. */
void RB_EndSurface_Optimized(void)
{
    switch (tess.shader->optimizedBackend) {
    case SHADER_BACKEND_CACHED_STATIC_MODEL_GENERIC:
        RB_EndSurface_CachedStaticModelGeneric();
        break;
    case SHADER_BACKEND_CACHED_STATIC_MODEL_ARB:
        RB_EndSurface_CachedStaticModelARB();
        break;
    case SHADER_BACKEND_CACHED_STATIC_MODEL_ATI:
        RB_EndSurface_CachedStaticModelATI();
        break;
    case SHADER_BACKEND_CACHED_STATIC_MODEL_NV:
        RB_EndSurface_CachedStaticModelNV();
        break;
    case SHADER_BACKEND_OPTIMIZED_GENERIC:
        RB_EndSurface_OptimizedGeneric();
        break;
    case SHADER_BACKEND_OPTIMIZED_ARB:
        RB_EndSurface_OptimizedARB();
        break;
    case SHADER_BACKEND_OPTIMIZED_ATI:
        RB_EndSurface_OptimizedATI();
        break;
    case SHADER_BACKEND_OPTIMIZED_NV:
        RB_EndSurface_OptimizedNV();
        break;
    default:
        break;
    }

    backEnd.pc.indexCount = (int32_t)(
        (uint32_t)backEnd.pc.indexCount +
        (uint32_t)tess.renderedIndexCount);
    backEnd.pc.vertexCount = (int32_t)(
        (uint32_t)backEnd.pc.vertexCount +
        (uint32_t)tess.renderedVertexCount);
    tess.renderedIndexCount = 0;
    tess.renderedVertexCount = 0;
}

/* Source: CoDUOMP.exe 0x004eeba0..0x004eed81.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eeba0_004eed82.mcode.
 * Name: exact same-module Mac symbol RB_EndSurface.
 *
 * The two last-array probes are intentional overflow sentinels. Normal
 * overflow checks prevent live geometry from reaching either location, so a
 * nonzero value proves that an earlier producer overran the tessellation
 * batch. The Windows build inlines the final GLimp_LogComment null-file gate;
 * the maintained source restores that helper boundary. */
void RB_EndSurface(void)
{
    qboolean optimizedSurfaceEnded = qfalse;

    if (tess.renderedIndexCount != 0) {
        RB_EndSurface_Optimized();
        optimizedSurfaceEnded = qtrue;
    }

    if (tess.indexCount == 0)
        return;

    if (tess.indexes[R_MAX_TESS_INDEXES - 1] != 0) {
        ri.Error(ERR_DROP,
                 "\x15RB_EndSurface() - SHADER_MAX_INDEXES hit");
    }
    if (tess.xyz[(R_MAX_TESS_VERTICES - 1) *
                 tess.vertexComponentCount] != 0.0f) {
        ri.Error(ERR_DROP,
                 "\x15RB_EndSurface() - SHADER_MAX_VERTEXES hit");
    }

    if (tess.shader == tr.stencilShadowShader) {
        RB_ShadowTessEnd();
        tess.indexCount = 0;
        return;
    }

    if (r_debugSort->integer != 0 &&
        (float)r_debugSort->integer < tess.shader->sort) {
        tess.indexCount = 0;
        return;
    }

    if (rendererFogCount != 0) {
        if ((backEnd.refdef.rdflags & RDF_SKYBOX_PORTAL) == 0) {
            if (tess.stageIterator == RB_StageIteratorSky) {
                tess.indexCount = 0;
                return;
            }
        } else if (rendererSkyboxPortalActive == qfalse &&
                   tess.stageIterator != RB_StageIteratorSky) {
            tess.indexCount = 0;
            return;
        }
    }

    ++backEnd.pc.shaderCount;
    if (tess.activeStageCount != 0) {
        backEnd.pc.vertexCount += tess.vertexCount;
        backEnd.pc.indexCount += tess.indexCount;
    }

    tess.stageIterator(optimizedSurfaceEnded);

    if (r_showtris->integer != 0 && tess.indexCount != 0)
        DrawTris(&tess);

    if (r_shownormals->string[0] != '\0') {
        if (Q_stricmp(r_shownormals->string, "white") == 0) {
            DrawNormals(&tess);
        } else if (Q_stricmp(r_shownormals->string, "color") == 0) {
            DrawColoredNormals(&tess);
        } else {
            Cvar_Set("r_shownormals", "");
            Com_Printf("Proper Usage: r_shownormals white | color\n");
        }
    }

    tess.indexCount = 0;
    GLimp_LogComment("----------\n");
}

/* Source: CoDUOMP.exe 0x004f0c40..0x004f0ccd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f0c40_004f0cce.mcode.
 * Name: exact same-module Mac symbol RB_CheckOverflow.
 *
 * A batch that cannot accept the requested geometry is submitted and then
 * restarted with the same shader, vertex layout, and entity. The strict
 * single-request checks intentionally permit an exact-capacity request: the
 * next RB_EndSurface sentinel check is the original guard against a producer
 * actually writing the reserved last element. */
void RB_CheckOverflow(int32_t vertexCount, int32_t indexCount)
{
    const int32_t nextVertexCount = (int32_t)(
        (uint32_t)tess.vertexCount + (uint32_t)vertexCount);
    const int32_t nextIndexCount = (int32_t)(
        (uint32_t)tess.indexCount + (uint32_t)indexCount);

    if (nextVertexCount < R_MAX_TESS_VERTICES &&
        nextIndexCount < R_MAX_TESS_INDEXES) {
        return;
    }

    RB_EndSurface();

    if (vertexCount > R_MAX_TESS_VERTICES) {
        ri.Error(ERR_DROP,
                 "\x15RB_CheckOverflow: verts > MAX (%d > %d)",
                 vertexCount, R_MAX_TESS_VERTICES);
    }
    if (indexCount > R_MAX_TESS_INDEXES) {
        ri.Error(ERR_DROP,
                 "\x15RB_CheckOverflow: indices > MAX (%d > %d)",
                 indexCount, R_MAX_TESS_INDEXES);
    }

    backEnd.currentEntity = tess.entity;
    RB_BeginSurface(tess.shader, tess.vertexComponentCount);
}

/* Source: CoDUOMP.exe 0x004f0cd0..0x004f0cf7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f0cd0_004f0cf7.mcode.
 * Name: exact same-module Mac symbol RB_CheckOverflow_Optimized. MSVC also
 * inlines this body into RB_SurfaceOptimized at 0x004f391d..0x004f3940. */
void RB_CheckOverflow_Optimized(void)
{
    RB_EndSurface();
    backEnd.currentEntity = tess.entity;
    RB_BeginSurface(tess.shader, tess.vertexComponentCount);
}

/* Source: CoDUOMP.exe 0x004f3550..0x004f3722.
 * Evidence: repaired boundary
 * coduomp/mcode/CoDUOMP/FUN_004f3550_004f3723.mcode.
 * Name and ordinary one-surface argument: same-module Mac symbol
 * RB_SurfaceTriangles. Both shipped builds copy adjusted indexes backward,
 * then copy the seven independently stored vertex arrays. */
void RB_SurfaceTriangles(renderer_surface_t *surfaceData)
{
    renderer_world_mesh_surface_t *surface =
        (renderer_world_mesh_surface_t *)surfaceData;
    RB_CheckOverflow(surface->vertexCount, surface->indexCount);
    const int32_t baseVertex = tess.vertexCount;
    tess.dlightBits |= surface->dlightBits;

    for (int32_t index = surface->indexCount - 1; index >= 0; --index) {
        tess.indexes[tess.indexCount + index] =
            (uint16_t)(surface->indices[index] + tess.vertexCount);
    }
    tess.indexCount += surface->indexCount;

    if ((tess.shader->surfaceFlags &
         SHADER_SURFACE_REQUIRES_VERTEX_BASIS) != 0 ||
        (r_dlightQuality->integer != 0 && tess.dlightBits != 0)) {
        memcpy(&tess.stageNormals[tess.vertexCount], surface->normals,
               (size_t)surface->vertexCount * sizeof(*surface->normals));
        tess.requiresVertexBasis = qtrue;

        if ((tess.shader->surfaceFlags &
             SHADER_SURFACE_TANGENT_SPACE_INPUT_MASK) != 0) {
            if (surface->tangents != NULL) {
                memcpy(&tess.stageTangents[tess.vertexCount],
                       surface->tangents,
                       (size_t)surface->vertexCount *
                           sizeof(*surface->tangents));
                tess.stageTangentsValid = qtrue;
            }
            if (surface->bitangents != NULL) {
                memcpy(&tess.stageBitangents[tess.vertexCount],
                       surface->bitangents,
                       (size_t)surface->vertexCount *
                           sizeof(*surface->bitangents));
                tess.stageBitangentsValid = qtrue;
            }
        }
    }

    memcpy(&tess.xyz[tess.vertexCount * 3], surface->positions,
           (size_t)surface->vertexCount * sizeof(*surface->positions));
    memcpy(&tess.texCoords[R_TESS_BASE_TEXCOORD_SET][tess.vertexCount],
           surface->texCoords,
           (size_t)surface->vertexCount * sizeof(*surface->texCoords));
    memcpy(&tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET][tess.vertexCount],
           surface->lightmapCoords,
           (size_t)surface->vertexCount * sizeof(*surface->lightmapCoords));
    memcpy(&tess.vertexColors[tess.vertexCount], surface->colors,
           (size_t)surface->vertexCount * sizeof(*surface->colors));

    tess.vertexCount = baseVertex + surface->vertexCount;
}

/* Source: CoDUOMP.exe 0x004f3730..0x004f38f7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3730_004f38f8.mcode.
 * Name and ordinary one-surface argument: same-module Mac symbol
 * RB_DlightFallback. Optimized surface indexes retain their original shared
 * vertex base, so the fallback rebases them by subtracting indices[0] while
 * copying the referenced vertex span into the ordinary tessellation arrays. */
void RB_DlightFallback(renderer_world_mesh_surface_t *surface)
{
    RB_CheckOverflow(surface->vertexCount, surface->indexCount);

    if (r_dlightQuality->integer != 0) {
        memcpy(&tess.stageTangents[tess.vertexCount], surface->tangents,
               (size_t)surface->vertexCount * sizeof(*surface->tangents));
        memcpy(&tess.stageBitangents[tess.vertexCount],
               surface->bitangents,
               (size_t)surface->vertexCount * sizeof(*surface->bitangents));
        memcpy(&tess.stageNormals[tess.vertexCount], surface->normals,
               (size_t)surface->vertexCount * sizeof(*surface->normals));
        tess.stageTangentsValid = qtrue;
        tess.stageBitangentsValid = qtrue;
        tess.requiresVertexBasis = qtrue;
    }

    memcpy(&tess.texCoords[R_TESS_BASE_TEXCOORD_SET][tess.vertexCount],
           surface->texCoords,
           (size_t)surface->vertexCount * sizeof(*surface->texCoords));
    memcpy(&tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET][tess.vertexCount],
           surface->lightmapCoords,
           (size_t)surface->vertexCount * sizeof(*surface->lightmapCoords));
    memcpy(&tess.vertexColors[tess.vertexCount], surface->colors,
           (size_t)surface->vertexCount * sizeof(*surface->colors));
    memcpy(&tess.xyz[tess.vertexCount * 3], surface->positions,
           (size_t)surface->vertexCount * sizeof(*surface->positions));

    const int32_t adjustedVertexBase =
        tess.vertexCount - (int32_t)surface->indices[0];
    for (int32_t index = 0; index < surface->indexCount; ++index) {
        tess.indexes[tess.indexCount + index] =
            (uint16_t)(surface->indices[index] + adjustedVertexBase);
    }

    tess.vertexCount += surface->vertexCount;
    tess.indexCount += surface->indexCount;
    tess.dlightBits |= surface->dlightBits;
}

/* Source: CoDUOMP.exe 0x004f3900..0x004f39f7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f3900_004f39f8.mcode.
 * Name, RB_CheckOverflow_Optimized boundary, and ordinary one-surface
 * argument: same-module Mac symbol RB_SurfaceOptimized. The optimized batch
 * records an exclusive vertex end and retains the surface's shared indexes
 * unchanged; only the dynamic-light fallback expands them into ordinary
 * tessellation storage. */
void RB_SurfaceOptimized(renderer_surface_t *surfaceData)
{
    renderer_world_mesh_surface_t *surface =
        (renderer_world_mesh_surface_t *)surfaceData;
    const int32_t requestedIndexEnd = (int32_t)(
        (uint32_t)tess.renderedIndexCount +
        (uint32_t)surface->indexCount);

    if (requestedIndexEnd >= R_MAX_OPTIMIZED_TESS_INDEXES) {
        RB_CheckOverflow_Optimized();
    }

    const int32_t firstVertex = surface->indices[0];
    const int32_t vertexEnd = (int32_t)(
        (uint32_t)firstVertex + (uint32_t)surface->vertexCount);
    if (tess.optimizedVertexEnd == 0) {
        tess.optimizedFirstVertex = firstVertex;
        tess.optimizedVertexEnd = vertexEnd;
    } else {
        if (firstVertex < tess.optimizedFirstVertex)
            tess.optimizedFirstVertex = firstVertex;
        if (vertexEnd > tess.optimizedVertexEnd)
            tess.optimizedVertexEnd = vertexEnd;
    }

    memcpy(&tess.optimizedIndexes[tess.renderedIndexCount],
           surface->indices,
           (size_t)((uint32_t)surface->indexCount *
                    (uint32_t)sizeof(*surface->indices)));
    tess.renderedIndexCount = (int32_t)(
        (uint32_t)tess.renderedIndexCount +
        (uint32_t)surface->indexCount);
    tess.renderedVertexCount = (int32_t)(
        (uint32_t)tess.renderedVertexCount +
        (uint32_t)surface->vertexCount);

    if (backEnd.refdef.num_dlights != 0 &&
        surface->dlightBits != 0 &&
        (glConfig.vertexArrayRangeMode == R_VERTEX_ARRAY_RANGE_NONE ||
         tr.defaultStorageMode == glState.currentStorageMode)) {
        RB_DlightFallback(surface);
    }
}

/* Source: CoDUOMP.exe 0x004f0d00..0x004f10e4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f0d00_004f10e5.mcode.
 * Exact source name unresolved: the role-proven provisional name describes
 * the five-vertex quad fan, whose supplied center color fades to zero alpha
 * at all four corners. The PE contains no call, branch, relocation, or table
 * reference to this emitted body, and the Mac symbol set has no counterpart;
 * it is retained because the instructions form complete renderer logic rather
 * than linker fill.
 *
 * The Windows optimizer passes origin, left, and up in ESI, EBX, and EDI. The
 * five ordinary stack arguments match RB_AddQuadStampExt. */
void RB_AddQuadStampExtFade(const vec3_t origin, const vec3_t left,
                            const vec3_t up, const uint8_t color[4],
                            float s1, float t1, float s2, float t2)
{
    const int32_t requestedVertexEnd = (int32_t)(
        (uint32_t)tess.vertexCount + 5u);
    const int32_t requestedIndexEnd = (int32_t)(
        (uint32_t)tess.indexCount + 12u);

    if (requestedVertexEnd >= R_MAX_TESS_VERTICES ||
        requestedIndexEnd >= R_MAX_TESS_INDEXES) {
        RB_EndSurface();
        backEnd.currentEntity = tess.entity;
        RB_BeginSurface(tess.shader, tess.vertexComponentCount);
    }

    const int32_t baseVertex = tess.vertexCount;
    /* The generated-attribute arrays use the zero-extended low half of the
     * tessellation vertex count; index values are also stored as words. */
    const uint32_t firstVertexArrayIndex = (uint16_t)baseVertex;
    const uint32_t centerVertex = firstVertexArrayIndex + 4u;

    tess.indexes[tess.indexCount + 0] = (uint16_t)(baseVertex + 0);
    tess.indexes[tess.indexCount + 1] = (uint16_t)(baseVertex + 1);
    tess.indexes[tess.indexCount + 2] = (uint16_t)centerVertex;
    tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 1);
    tess.indexes[tess.indexCount + 4] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 5] = (uint16_t)centerVertex;
    tess.indexes[tess.indexCount + 6] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 7] = (uint16_t)(baseVertex + 3);
    tess.indexes[tess.indexCount + 8] = (uint16_t)centerVertex;
    tess.indexes[tess.indexCount + 9] = (uint16_t)(baseVertex + 3);
    tess.indexes[tess.indexCount + 10] = (uint16_t)(baseVertex + 0);
    tess.indexes[tess.indexCount + 11] = (uint16_t)centerVertex;

    uint32_t xyzOffset =
        firstVertexArrayIndex * (uint32_t)tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = (left[0] + up[0]) + origin[0];
    tess.xyz[xyzOffset + 1] = (left[1] + up[1]) + origin[1];
    tess.xyz[xyzOffset + 2] = (left[2] + up[2]) + origin[2];

    xyzOffset += tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = (origin[0] - left[0]) + up[0];
    tess.xyz[xyzOffset + 1] = (origin[1] - left[1]) + up[1];
    tess.xyz[xyzOffset + 2] = (origin[2] - left[2]) + up[2];

    xyzOffset += tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = (origin[0] - left[0]) - up[0];
    tess.xyz[xyzOffset + 1] = (origin[1] - left[1]) - up[1];
    tess.xyz[xyzOffset + 2] = (origin[2] - left[2]) - up[2];

    xyzOffset += tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = (left[0] + origin[0]) - up[0];
    tess.xyz[xyzOffset + 1] = (left[1] + origin[1]) - up[1];
    tess.xyz[xyzOffset + 2] = (left[2] + origin[2]) - up[2];

    xyzOffset += tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = origin[0];
    tess.xyz[xyzOffset + 1] = origin[1];
    tess.xyz[xyzOffset + 2] = origin[2];

    for (uint32_t vertexOffset = 0; vertexOffset < 5; ++vertexOffset) {
        const uint32_t vertexIndex =
            firstVertexArrayIndex + vertexOffset;
        tess.stageNormals[vertexIndex][0] =
            -backEnd.viewParms.orientation.axis[0][0];
        tess.stageNormals[vertexIndex][1] =
            -backEnd.viewParms.orientation.axis[0][1];
        tess.stageNormals[vertexIndex][2] =
            -backEnd.viewParms.orientation.axis[0][2];
    }

    const vec2_t cornerTexCoords[4] = {
        {s1, t1},
        {s2, t1},
        {s2, t2},
        {s1, t2}
    };
    for (uint32_t vertexOffset = 0; vertexOffset < 4; ++vertexOffset) {
        const uint32_t vertexIndex =
            firstVertexArrayIndex + vertexOffset;
        memcpy(tess.texCoords[R_TESS_BASE_TEXCOORD_SET][vertexIndex],
               cornerTexCoords[vertexOffset], sizeof(vec2_t));
        memcpy(tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET][vertexIndex],
               cornerTexCoords[vertexOffset], sizeof(vec2_t));
    }

    const vec2_t centerTexCoord = {
        (s1 + s2) * 0.5f,
        (t1 + t2) * 0.5f
    };
    memcpy(tess.texCoords[R_TESS_BASE_TEXCOORD_SET][centerVertex],
           centerTexCoord, sizeof(vec2_t));
    memcpy(tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET][centerVertex],
           centerTexCoord, sizeof(vec2_t));

    uint32_t centerColor;
    uint32_t edgeColor;
    uint8_t edgeColorBytes[4];
    memcpy(&centerColor, color, sizeof(centerColor));
    memcpy(edgeColorBytes, color, sizeof(edgeColorBytes));
    edgeColorBytes[3] = 0;
    memcpy(&edgeColor, edgeColorBytes, sizeof(edgeColor));

    tess.vertexColors[centerVertex] = centerColor;
    tess.vertexColors[firstVertexArrayIndex + 0] = edgeColor;
    tess.vertexColors[firstVertexArrayIndex + 1] = edgeColor;
    tess.vertexColors[firstVertexArrayIndex + 2] = edgeColor;
    tess.vertexColors[firstVertexArrayIndex + 3] = edgeColor;

    tess.vertexCount = (int32_t)(
        (uint32_t)tess.vertexCount + 5u);
    tess.indexCount = (int32_t)(
        (uint32_t)tess.indexCount + 12u);
}

/* Source: CoDUOMP.exe 0x004f10f0..0x004f13d8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f10f0_004f13d9.mcode.
 * Name: exact same-module Mac symbol RB_AddQuadStampExt.
 *
 * The Windows stores prove that the caller's half-width vector is `left`, the
 * half-height vector is `up`, and both the base and lightmap coordinate sets
 * receive the same four corners. Positions use the full 32-bit vertex base,
 * while normals, texture coordinates, and colors use its zero-extended low
 * half. The generated quad faces opposite the current view-forward axis in
 * local space. */
void RB_AddQuadStampExt(const vec3_t origin, const vec3_t left,
                        const vec3_t up, const uint8_t color[4],
                        float s1, float t1, float s2, float t2)
{
    int32_t baseVertex;
    uint32_t xyzOffset;
    uint32_t packedColor;

    const int32_t requestedVertexEnd = (int32_t)(
        (uint32_t)tess.vertexCount + 4u);
    const int32_t requestedIndexEnd = (int32_t)(
        (uint32_t)tess.indexCount + 6u);

    if (requestedVertexEnd >= R_MAX_TESS_VERTICES ||
        requestedIndexEnd >= R_MAX_TESS_INDEXES) {
        RB_EndSurface();
        backEnd.currentEntity = tess.entity;
        RB_BeginSurface(tess.shader, tess.vertexComponentCount);
    }

    baseVertex = tess.vertexCount;
    const uint32_t firstVertexArrayIndex = (uint16_t)baseVertex;

    tess.indexes[tess.indexCount + 0] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 1] = (uint16_t)(baseVertex + 1);
    tess.indexes[tess.indexCount + 2] = (uint16_t)(baseVertex + 3);
    tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 3);
    tess.indexes[tess.indexCount + 4] = (uint16_t)(baseVertex + 1);
    tess.indexes[tess.indexCount + 5] = (uint16_t)(baseVertex + 2);

    xyzOffset =
        (uint32_t)baseVertex *
        (uint32_t)tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = (left[0] + up[0]) + origin[0];
    tess.xyz[xyzOffset + 1] = (left[1] + origin[1]) + up[1];
    tess.xyz[xyzOffset + 2] = (left[2] + origin[2]) + up[2];

    xyzOffset += tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = (origin[0] - left[0]) + up[0];
    tess.xyz[xyzOffset + 1] = (origin[1] - left[1]) + up[1];
    tess.xyz[xyzOffset + 2] = (origin[2] - left[2]) + up[2];

    xyzOffset += tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = (origin[0] - left[0]) - up[0];
    tess.xyz[xyzOffset + 1] = (origin[1] - left[1]) - up[1];
    tess.xyz[xyzOffset + 2] = (origin[2] - left[2]) - up[2];

    xyzOffset += tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = (left[0] + origin[0]) - up[0];
    tess.xyz[xyzOffset + 1] = (left[1] + origin[1]) - up[1];
    tess.xyz[xyzOffset + 2] = (left[2] + origin[2]) - up[2];

    for (uint32_t vertexOffset = 0; vertexOffset < 4; ++vertexOffset) {
        const uint32_t vertexIndex =
            firstVertexArrayIndex + vertexOffset;

        tess.stageNormals[vertexIndex][0] =
            -backEnd.viewParms.orientation.axis[0][0];
        tess.stageNormals[vertexIndex][1] =
            -backEnd.viewParms.orientation.axis[0][1];
        tess.stageNormals[vertexIndex][2] =
            -backEnd.viewParms.orientation.axis[0][2];
    }

    tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                  [firstVertexArrayIndex + 0][0] = s1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                  [firstVertexArrayIndex + 0][1] = t1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                  [firstVertexArrayIndex + 1][0] = s2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                  [firstVertexArrayIndex + 1][1] = t1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                  [firstVertexArrayIndex + 2][0] = s2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                  [firstVertexArrayIndex + 2][1] = t2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                  [firstVertexArrayIndex + 3][0] = s1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                  [firstVertexArrayIndex + 3][1] = t2;

    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                  [firstVertexArrayIndex + 0][0] = s1;
    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                  [firstVertexArrayIndex + 0][1] = t1;
    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                  [firstVertexArrayIndex + 1][0] = s2;
    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                  [firstVertexArrayIndex + 1][1] = t1;
    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                  [firstVertexArrayIndex + 2][0] = s2;
    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                  [firstVertexArrayIndex + 2][1] = t2;
    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                  [firstVertexArrayIndex + 3][0] = s1;
    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET]
                  [firstVertexArrayIndex + 3][1] = t2;

    memcpy(&packedColor, color, sizeof(packedColor));
    tess.vertexColors[firstVertexArrayIndex + 0] = packedColor;
    tess.vertexColors[firstVertexArrayIndex + 1] = packedColor;
    tess.vertexColors[firstVertexArrayIndex + 2] = packedColor;
    tess.vertexColors[firstVertexArrayIndex + 3] = packedColor;

    tess.vertexCount = (int32_t)(
        (uint32_t)tess.vertexCount + 4u);
    tess.indexCount = (int32_t)(
        (uint32_t)tess.indexCount + 6u);
}

/* Source: CoDUOMP.exe 0x004f13e0..0x004f13ff.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f13e0_004f13ff.mcode.
 * Name: exact same-module Mac symbol RB_AddQuadStamp. The wrapper supplies
 * the complete [0,1] texture rectangle; several Windows callers inline it. */
void RB_AddQuadStamp(const vec3_t origin, const vec3_t left,
                     const vec3_t up, const uint8_t color[4])
{
    RB_AddQuadStampExt(origin, left, up, color,
                       0.0f, 0.0f, 1.0f, 1.0f);
}
