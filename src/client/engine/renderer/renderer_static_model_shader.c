#include "backend.h"

#include "gl_state.h"

#include <string.h>

enum {
    SHADER_STAGE_STATIC_MODEL_CACHE_STATE = GLS_LIGHTING
};

/* Source: CoDUOMP.exe 0x00503590..0x00503639.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503590_0050363a.mcode.
 * Name and ordinary two-argument source boundary: exact same-module Mac
 * symbol R_CanOptimizeStaticModelStage. The Windows selector bytes at
 * 0x00503644 prove the four accepted color generators; the parser's string
 * comparisons at 0x004ff429..0x004ff676 prove their names. */
qboolean R_CanOptimizeStaticModelStage(
    const shaderStage_t *stage, const shaderStage_t *firstStage)
{
    if (stage->bundle[0].numTexMods != 0)
        return qfalse;

    switch (stage->rgbGen) {
    case CGEN_IDENTITY_LIGHTING:
    case CGEN_IDENTITY:
    case CGEN_LIGHTING_DIFFUSE:   /* rgbGen 10: selector table @0x50364d = 0x00
                                   * (accept -> fall through to the alphaGen check).
                                   * A prior pass omitted this accepted case. */
    case CGEN_LIGHTING_PRECALC:
    case CGEN_CONSTANT:
        break;
    default:
        return qfalse;
    }

    switch (stage->alphaGen) {
    case AGEN_UNSPECIFIED:
    case AGEN_IDENTITY:
    case AGEN_CONSTANT:
        break;
    default:
        return qfalse;
    }

    if (firstStage == NULL)
        return qtrue;

    if (stage->rgbGen != firstStage->rgbGen)
        return qfalse;

    if (stage->rgbGen == CGEN_CONSTANT &&
        memcmp(stage->constantColor, firstStage->constantColor, 3) != 0) {
        return qfalse;
    }

    if (stage->alphaGen != firstStage->alphaGen)
        return qfalse;

    if (stage->alphaGen == AGEN_CONSTANT &&
        stage->constantColor[3] != firstStage->constantColor[3]) {
        return qfalse;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00503650..0x005036b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503650_005036ba.mcode.
 * Name and source-level copy boundary: exact same-module Mac symbol
 * CloneShader. Windows proves the complete 0x198-byte shader copy, clearing
 * all eight 0x688-byte scratch stages, and copying only the live stages. */
void CloneShader(const shader_t *shader)
{
    int32_t stageIndex;

    rendererParsedShader = *shader;
    memset(rendererParsedShaderStages, 0,
           sizeof(rendererParsedShaderStages));

    for (stageIndex = 0;
         stageIndex < shader->numUnfoggedPasses;
         ++stageIndex) {
        rendererParsedShader.stages[stageIndex] =
            &rendererParsedShaderStages[stageIndex];
        rendererParsedShaderStages[stageIndex] = *shader->stages[stageIndex];
    }
}

/* Source: CoDUOMP.exe 0x005036c0..0x0050384e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005036c0_0050384f.mcode.
 * Name and source shape: exact same-module Mac symbol
 * R_CacheableStaticModelShader. Windows behavior is authoritative: a shader
 * with deforms, the high lighting bit, incompatible surface flags, no stages,
 * or an incompatible stage returns NULL. A compatible shader is cached under
 * its '?' prefixed name after removing entity-lighting behavior. */
shader_t *R_CacheableStaticModelShader(const shader_t *shader)
{
    enum {
        SHADER_SURFACE_CACHE_MASK_A = 0x3ff5fd00,
        SHADER_SURFACE_CACHE_MASK_B = 0x3ff7fc00
    };
    char cachedName[MAX_QPATH];
    const char *sourceNameEnd;
    size_t sourceNameLength;
    shader_t *cachedShader;
    int32_t stageIndex;

    if (shader->numDeforms != 0)
        return NULL;
    if ((shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) != 0)
        return NULL;
    if ((shader->surfaceFlags & SHADER_SURFACE_CACHE_MASK_A) != 0 &&
        (shader->surfaceFlags & SHADER_SURFACE_CACHE_MASK_B) != 0) {
        return NULL;
    }
    if (shader->numUnfoggedPasses == 0)
        return NULL;

    if (R_CanOptimizeStaticModelStage(shader->stages[0], NULL) == qfalse)
        return NULL;

    for (stageIndex = 1;
         stageIndex < shader->numUnfoggedPasses;
         ++stageIndex) {
        if (R_CanOptimizeStaticModelStage(
                shader->stages[stageIndex], shader->stages[0]) == qfalse) {
            return NULL;
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    sourceNameEnd = memchr(shader->name, '\0', sizeof(shader->name));
    if (sourceNameEnd == NULL)
        return NULL;
    sourceNameLength = (size_t)(sourceNameEnd - shader->name);
    if (sourceNameLength >= sizeof(cachedName) - 1u)
        return NULL;

    cachedName[0] = '?';
    memcpy(&cachedName[1], shader->name, sourceNameLength + 1u);
    cachedShader = R_FindShaderByName(cachedName);
    if (cachedShader != tr.defaultShader)
        return cachedShader;

    CloneShader(shader);
    /* The preflight above proves prefix + name + NUL fits this field. */
    memcpy(rendererParsedShader.name, cachedName, sourceNameLength + 2u);
    rendererParsedShader.flags |= SHADER_FLAG_ENTITY_MERGABLE;
    rendererParsedShader.lightingFlags &=
        ~SHADER_LIGHTING_ENTITY_MASK;
    rendererParsedShader.optimizedBackend =
        (shader_optimized_backend_t)tr.cachedStaticModelSurfaceType;

    for (stageIndex = 0;
         stageIndex < rendererParsedShader.numUnfoggedPasses;
         ++stageIndex) {
        shaderStage_t *stage = rendererParsedShader.stages[stageIndex];

        stage->flags &= ~SHADER_LIGHTING_ENTITY_MASK;
        stage->stateBits &= ~SHADER_STAGE_STATIC_MODEL_CACHE_STATE;
    }

    return GeneratePermanentShader();
}
