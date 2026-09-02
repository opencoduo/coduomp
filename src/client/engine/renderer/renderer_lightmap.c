#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    R_LIGHTMAP_SIZE = 512,
    R_LIGHTMAP_SOURCE_COMPONENTS = 3,
    R_LIGHTMAP_DESTINATION_COMPONENTS = 4
};

/* RE_LoadWorldMap owns the loaded BSP byte stream and the one world object
 * beginning at original 0x0388bee0. The name and disk-shader objects formerly
 * declared here as separate globals are fields at +0x00 and +0x84/+0x88. */
uint8_t *rendererWorldFileBase;
world_t rendererWorldData;

/* Source: CoDUOMP.exe 0x0050b270..0x0050b2c7, recovered from an executable
 * gap after repairing the missing Ghidra function boundary.
 * Name and ordinary three-int signature: exact same-module Mac symbol
 * ShaderForShaderNum. The Windows LTCG body narrows shaderNum/lightmapIndex to
 * the 16-bit fields supplied by all callers; Mac retains their source ints. */
shader_t *ShaderForShaderNum(int32_t shaderNum, int32_t lightmapIndex, int32_t shaderUsage)
{
    if (shaderNum < 0 || shaderNum >= rendererWorldData.numShaders) {
        ri.Error(ERR_DROP, "\x15ShaderForShaderNum: bad num %i", shaderNum);
    }

    shader_t *shader = R_FindShader(rendererWorldData.shaders[shaderNum].shader, lightmapIndex, qtrue, shaderUsage);
    if ((shader->flags & SHADER_FLAG_DEFAULTED) != 0)
        return tr.defaultShader;
    return shader;
}

/* Source: CoDUOMP.exe 0x0050ab70..0x0050af03.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ab70_0050af04.mcode.
 * Name and ordinary three-argument signature: exact same-module Mac symbol
 * R_BuildLightmapMergability. Windows 0x0050abc2..0x0050ac2d and both shipped
 * PowerPC clients traverse 16-byte dsurface_t records and read
 * shaderIndex at +0, lightmapIndex at +2, and unsigned vertexCount at +8. The
 * remaining surface fields are consumed by R_LoadSurfaces, not by this pass.
 * The matrix accumulates shared-shader vertex counts for every pair of
 * lightmaps, then greedily groups the strongest pairs into hardware-sized
 * atlases. */
int32_t R_BuildLightmapMergability(const lump_t *surfaceLump, int32_t atlasDimensions[R_MAX_LIGHTMAPS][2],
                                   int32_t lightmapOrder[R_MAX_LIGHTMAPS])
{
    const dsurface_t *diskSurfaces = (const dsurface_t *)(rendererWorldFileBase + surfaceLump->fileofs);
    const uint32_t lumpLength = (uint32_t)surfaceLump->filelen;

    if (lumpLength % sizeof(*diskSurfaces) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int32_t surfaceCount = (int32_t)(lumpLength / sizeof(*diskSurfaces));
    int32_t originalLightmapCount = 0;
    for (int32_t surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex) {
        const int32_t lightmapIndex = diskSurfaces[surfaceIndex].lightmapNum;
        if (originalLightmapCount <= lightmapIndex) {
            originalLightmapCount = (int32_t)((uint32_t)lightmapIndex + 1u);
        }
    }

    /* Per-shader vertex totals while building mergeCosts; after that pass the
     * same scratch array is cleared and reused as assigned-lightmap markers. */
    int32_t lightmapScratch[R_MAX_LIGHTMAPS];
    uint32_t mergeCosts[R_MAX_LIGHTMAPS][R_MAX_LIGHTMAPS];
    memset(lightmapScratch, 0, sizeof(lightmapScratch));
    memset(mergeCosts, 0, sizeof(mergeCosts));

    for (int32_t shaderIndex = 0; shaderIndex < rendererWorldData.numShaders; ++shaderIndex) {
        for (int32_t surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex) {
            const dsurface_t *diskSurface = &diskSurfaces[surfaceIndex];
            if (diskSurface->shaderNum == shaderIndex && diskSurface->lightmapNum >= 0) {
                const int32_t lightmapIndex = diskSurface->lightmapNum;
                lightmapScratch[lightmapIndex] = (int32_t)((uint32_t)lightmapScratch[lightmapIndex] + (uint32_t)diskSurface->numVerts);
            }
        }

        for (int32_t first = 0; first < originalLightmapCount; ++first) {
            if (lightmapScratch[first] != 0) {
                for (int32_t second = first + 1; second < originalLightmapCount; ++second) {
                    if (lightmapScratch[second] == 0)
                        continue;

                    uint32_t accumulated = mergeCosts[first][second] + (uint32_t)lightmapScratch[first] + (uint32_t)lightmapScratch[second];
                    if ((int32_t)accumulated < 0)
                        accumulated = INT32_MAX;
                    mergeCosts[first][second] = accumulated;
                    mergeCosts[second][first] = mergeCosts[first][second];
                }
                lightmapScratch[first] = 0;
            }
        }
    }

    memset(lightmapScratch, 0, sizeof(lightmapScratch));

    int32_t atlasWidth = glConfig.maxTextureSize / R_LIGHTMAP_SIZE;
    int32_t atlasHeight = atlasWidth;
    int32_t atlasCount = 0;
    int32_t assignedCount = 0;

    while (assignedCount < originalLightmapCount) {
        const int32_t remaining = (int32_t)((uint32_t)originalLightmapCount - (uint32_t)assignedCount);
        int32_t atlasCapacity = (int32_t)((uint32_t)atlasWidth * (uint32_t)atlasHeight);

        while (atlasCapacity > remaining) {
            if (atlasWidth < atlasHeight)
                atlasHeight /= 2;
            else
                atlasWidth /= 2;
            atlasCapacity = (int32_t)((uint32_t)atlasWidth * (uint32_t)atlasHeight);
        }

        if (atlasCapacity >= 2) {
            int32_t pairFirst = -1;
            int32_t pairSecond = -1;

            for (int32_t first = 0; first < originalLightmapCount; ++first) {
                if (lightmapScratch[first] != 0)
                    continue;

                for (int32_t second = first + 1; second < originalLightmapCount; ++second) {
                    if (lightmapScratch[second] != 0)
                        continue;
                    if (pairSecond < 0 || (int32_t)mergeCosts[first][second] > (int32_t)mergeCosts[pairSecond][pairFirst]) {
                        pairFirst = second;
                        pairSecond = first;
                    }
                }
            }

            lightmapOrder[assignedCount++] = pairFirst;
            lightmapOrder[assignedCount++] = pairSecond;
            lightmapScratch[pairFirst] = 1;
            lightmapScratch[pairSecond] = 1;

            for (int32_t remainingSlots = atlasCapacity - 2; remainingSlots != 0; --remainingSlots) {
                for (int32_t lightmapIndex = 0; lightmapIndex < originalLightmapCount; ++lightmapIndex) {
                    const uint32_t combined = mergeCosts[pairSecond][lightmapIndex] + mergeCosts[pairFirst][lightmapIndex];
                    mergeCosts[pairSecond][lightmapIndex] = combined;
                    mergeCosts[lightmapIndex][pairSecond] = combined;
                }

                int32_t bestLightmap = -1;
                for (int32_t lightmapIndex = 0; lightmapIndex < originalLightmapCount; ++lightmapIndex) {
                    if (lightmapScratch[lightmapIndex] != 0)
                        continue;
                    if (bestLightmap < 0 ||
                        (int32_t)mergeCosts[pairSecond][lightmapIndex] > (int32_t)mergeCosts[pairSecond][bestLightmap]) {
                        bestLightmap = lightmapIndex;
                    }
                }

                pairFirst = bestLightmap;
                lightmapOrder[assignedCount++] = bestLightmap;
                lightmapScratch[bestLightmap] = 1;
            }
        } else {
            int32_t lightmapIndex = 0;
            while (lightmapScratch[lightmapIndex] != 0)
                ++lightmapIndex;
            lightmapOrder[assignedCount++] = lightmapIndex;
            lightmapScratch[lightmapIndex] = 1;
        }

        atlasDimensions[atlasCount][0] = atlasWidth;
        atlasDimensions[atlasCount][1] = atlasHeight;
        ++atlasCount;
    }

    ri.Printf(R_PRINT_ALL, "%i merged lightmaps from %i original lightmaps\n", atlasCount, originalLightmapCount);
    return originalLightmapCount;
}

/* Source: CoDUOMP.exe 0x0050af80..0x0050b260.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050af80_0050b261.mcode.
 * Name and three-argument source signature: exact same-module Mac symbol
 * R_LoadLightmaps. Windows inlines the source-level R_CreateImage call, while
 * Mac retains it; the generated atlas dimensions, placement records, flags,
 * and image order agree between both builds. */
void R_LoadLightmaps(const lump_t *lightmapLump, const lump_t *surfaceLump, renderer_lightmap_placement_t placements[R_MAX_LIGHTMAPS])
{
    const uint8_t *lightmapPixels = rendererWorldFileBase + lightmapLump->fileofs;
    int32_t lightmapDataLength = lightmapLump->filelen;

    R_SyncRenderThread();

    int32_t atlasDimensions[R_MAX_LIGHTMAPS][2];
    int32_t lightmapOrder[R_MAX_LIGHTMAPS];
    memset(atlasDimensions, 0, sizeof(atlasDimensions));
    memset(lightmapOrder, 0, sizeof(lightmapOrder));

    const int32_t originalLightmapCount = R_BuildLightmapMergability(surfaceLump, atlasDimensions, lightmapOrder);
    const int32_t sourceLightmapBytes = R_LIGHTMAP_SIZE * R_LIGHTMAP_SIZE * R_LIGHTMAP_SOURCE_COMPONENTS;

    const int32_t expectedLightmapDataLength = (int32_t)((uint32_t)originalLightmapCount * (uint32_t)sourceLightmapBytes);
    if (lightmapDataLength != 0 && lightmapDataLength != expectedLightmapDataLength) {
        ri.Error(ERR_DROP, "\x15R_LoadLightmaps: incorrect lightmap lump size");
    }

    if (r_fullbright->integer != 0)
        lightmapDataLength = 0;

    const uint32_t atlasBufferBytes = (uint32_t)atlasDimensions[0][0] * (uint32_t)atlasDimensions[0][1] * (uint32_t)R_LIGHTMAP_SIZE *
                                      (uint32_t)R_LIGHTMAP_SIZE * (uint32_t)R_LIGHTMAP_DESTINATION_COMPONENTS;
    uint8_t *atlasPixels = (uint8_t *)ri.Hunk_AllocateTempMemory((size_t)atlasBufferBytes);

    if (lightmapDataLength == 0 && (int32_t)atlasBufferBytes > 0) {
        const uint8_t identity = (uint8_t)tr.identityLightByte;
        for (uint32_t offset = 0; offset < atlasBufferBytes; offset += R_LIGHTMAP_DESTINATION_COMPONENTS) {
            atlasPixels[offset + 0] = identity;
            atlasPixels[offset + 1] = identity;
            atlasPixels[offset + 2] = identity;
            atlasPixels[offset + 3] = 255;
        }
    }

    int32_t atlasIndex = 0;
    int32_t consumedLightmaps = 0;
    while (consumedLightmaps < originalLightmapCount) {
        const int32_t atlasArrayIndex = (int16_t)atlasIndex;
        const int32_t widthInLightmaps = atlasDimensions[atlasArrayIndex][0];
        const int32_t heightInLightmaps = atlasDimensions[atlasArrayIndex][1];
        const int32_t atlasTileCount = (int32_t)((uint32_t)widthInLightmaps * (uint32_t)heightInLightmaps);
        const int32_t atlasWidth = (int32_t)((uint32_t)widthInLightmaps * (uint32_t)R_LIGHTMAP_SIZE);
        const int32_t atlasHeight = (int32_t)((uint32_t)heightInLightmaps * (uint32_t)R_LIGHTMAP_SIZE);
        const long double inverseWidthRaw = 1.0L / (long double)widthInLightmaps;
        const long double inverseHeightRaw = 1.0L / (long double)heightInLightmaps;
        const float inverseWidth = (float)inverseWidthRaw;
        const float inverseHeight = (float)inverseHeightRaw;

        uint16_t tileIndexWord = 0;
        int32_t tileIndex = 0;
        while (tileIndex < atlasTileCount) {
            const int32_t originalIndex = lightmapOrder[(int32_t)((uint32_t)consumedLightmaps + (uint32_t)tileIndex)];
            const int32_t tileX = tileIndex % widthInLightmaps;
            const int32_t tileY = tileIndex / widthInLightmaps;

            if (lightmapDataLength != 0) {
                const uint32_t sourceOffset = (uint32_t)originalIndex * (uint32_t)sourceLightmapBytes;
                R_CopyLightmap(atlasPixels, lightmapPixels + sourceOffset, (int32_t)((uint32_t)tileX * (uint32_t)R_LIGHTMAP_SIZE),
                               (int32_t)((uint32_t)tileY * (uint32_t)R_LIGHTMAP_SIZE), atlasWidth);
            }

            renderer_lightmap_placement_t *placement = &placements[originalIndex];
            placement->atlasIndex = (int16_t)atlasArrayIndex;
            placement->sScale = inverseWidth;
            placement->tScale = inverseHeight;
            /* 0x0050b0d6..0x0050b161 keeps both reciprocals on the x87 stack
             * after writing the float scale fields; offsets use the retained
             * values. */
            placement->sOffset = (float)((long double)tileX * inverseWidthRaw);
            placement->tOffset = (float)((long double)tileY * inverseHeightRaw);

            tileIndexWord = (uint16_t)(tileIndexWord + 1u);
            tileIndex = (int16_t)tileIndexWord;
        }

        tr.lightmaps[atlasArrayIndex] =
            R_CreateImage(va("*lightmap%d", atlasArrayIndex), atlasPixels, atlasWidth, atlasHeight, GL_RGBA,
                          IMAGE_FLAG_LIGHTMAP | IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T, R_IMAGE_TRACK_LIGHTMAP, NULL);

        consumedLightmaps = (int32_t)((uint32_t)consumedLightmaps + (uint32_t)atlasTileCount);
        atlasIndex = (int32_t)((uint32_t)atlasIndex + 1u);
    }

    tr.lightmapCount = (int16_t)atlasIndex;
    ri.Hunk_FreeTempMemory(atlasPixels);
}
