#include "backend.h"

#include "compat/coduo_native_x87.h"
#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"
#include "gl_api.h"
#include "gl_state.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    RB_ANIMATION_FRAME_FRACTION_BITS = 10,
    RB_ANIMATION_FRAME_SCALE = 1 << RB_ANIMATION_FRAME_FRACTION_BITS,
    RB_X86_SHIFT_COUNT_MASK = 31
};

/* Original 0x0387be98. RB_StageIteratorGeneric sets this while the client
 * arrays point at the generated tessellation storage before stage processing;
 * RB_ComputeTexCoords must therefore copy even an otherwise unmodified base
 * coordinate set. */
qboolean rbSetArraysOnce;

/* Source: CoDUOMP.exe 0x005ced78..0x005ced83 (.data). The only consumer is
 * RB_CalcSpecularAlpha, matching the same-module Mac routine's fixed light
 * origin. */
static const vec3_t rbSpecularLightOrigin = {-960.0f, 1980.0f, 96.0f};

/* Source: CoDUOMP.exe 0x004ea9a0..0x004eaa0d.
 * Name: exact same-module Mac symbol RB_EnableClientTmu. */
void RB_EnableClientTmu(int32_t textureUnit,
                        const textureBundle_t *bundle,
                        const void *texCoords, int32_t vertexStride)
{
    /* D3 E2/D3 E0 at 0x004ea9c9/0x004ea9e5 mask CL to five bits. */
    const uint32_t texCoordArrayBit =
        1u << ((uint32_t)textureUnit & RB_X86_SHIFT_COUNT_MASK);

    if (glState.currentClientTmu != textureUnit) {
        qglClientActiveTextureARB(GL_TEXTURE0_ARB +
                                  (uint32_t)textureUnit);
        glState.currentClientTmu = textureUnit;
    }
    if ((glState.clientStateBits & texCoordArrayBit) == 0) {
        qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glState.clientStateBits |= texCoordArrayBit;
    }
    qglTexCoordPointer(bundle->texCoordComponentCount, GL_FLOAT,
                       vertexStride, texCoords);
}

/* Source: CoDUOMP.exe 0x004eaaa0..0x004eab17.
 * Name: exact same-module Mac symbol RB_EnableClientTmuATI. Unlike the client
 * pointer path, ATI array objects receive the object-buffer handle and a byte
 * offset into that buffer as separate scalar arguments. */
void RB_EnableClientTmuATI(int32_t textureUnit,
                           const textureBundle_t *bundle,
                           uint32_t objectBuffer, uint32_t texCoordOffset,
                           int32_t vertexStride)
{
    /* D3 E2/D3 E0 at 0x004eaac9/0x004eaae5 mask CL to five bits. */
    const uint32_t texCoordArrayBit =
        1u << ((uint32_t)textureUnit & RB_X86_SHIFT_COUNT_MASK);

    if (glState.currentClientTmu != textureUnit) {
        qglClientActiveTextureARB(GL_TEXTURE0_ARB +
                                  (uint32_t)textureUnit);
        glState.currentClientTmu = textureUnit;
    }
    if ((glState.clientStateBits & texCoordArrayBit) == 0) {
        qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glState.clientStateBits |= texCoordArrayBit;
    }
    qglArrayObjectATI(GL_TEXTURE_COORD_ARRAY,
                      bundle->texCoordComponentCount, GL_FLOAT,
                      vertexStride, objectBuffer, texCoordOffset);
}

/* Source: CoDUOMP.exe 0x004eaa10..0x004eaa58.
 * Name: exact same-module Mac symbol RB_DisableClientTmu. */
void RB_DisableClientTmu(int32_t textureUnit)
{
    /* The original D3 E6 shift masks CL to five bits. */
    const uint32_t texCoordArrayBit =
        1u << ((uint32_t)textureUnit & RB_X86_SHIFT_COUNT_MASK);

    if ((glState.clientStateBits & texCoordArrayBit) == 0)
        return;

    if (glState.currentClientTmu != textureUnit) {
        qglClientActiveTextureARB(GL_TEXTURE0_ARB +
                                  (uint32_t)textureUnit);
        glState.currentClientTmu = textureUnit;
    }
    qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glState.clientStateBits &= ~texCoordArrayBit;
}

/* Source: CoDUOMP.exe 0x004eaa60..0x004eaa96.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eaa60_004eaa97.mcode.
 * Name and argument order: exact same-module Mac symbol RB_SetupClientTmu. */
void RB_SetupClientTmu(
    int32_t textureUnit, shaderStage_t *stage,
    const void *const baseTexCoords[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride)
{
    textureBundle_t *bundle = &stage->bundle[textureUnit];

    if (bundle->textureEnvMode != 0) {
        RB_EnableClientTmu(textureUnit, bundle,
                           baseTexCoords[textureUnit], vertexStride);
    } else {
        RB_DisableClientTmu(textureUnit);
    }
}

/* Source: CoDUOMP.exe 0x004eab20..0x004eab5b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eab20_004eab5c.mcode.
 * Name and argument order: exact same-module Mac symbol
 * RB_SetupClientTmuATI. */
void RB_SetupClientTmuATI(
    int32_t textureUnit, shaderStage_t *stage, uint32_t objectBuffer,
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride)
{
    textureBundle_t *bundle = &stage->bundle[textureUnit];

    if (bundle->textureEnvMode != 0) {
        RB_EnableClientTmuATI(textureUnit, bundle, objectBuffer,
                              texCoordOffsets[textureUnit], vertexStride);
    } else {
        RB_DisableClientTmu(textureUnit);
    }
}

/* Source: CoDUOMP.exe 0x004ea500..0x004ea8ce.
 * Name: exact same-module Mac symbol RB_EnableTMU. Besides binding the stage's
 * selected image, this installs ARB texture-combine and NV texture-shader
 * state. The per-unit caches are updated exactly where the Windows body
 * updates them; merely inspecting a different unit does not change the active
 * server TMU. */
void RB_EnableTMU(textureBundle_t *bundle, int32_t textureUnit)
{
    uint32_t textureEnvMode = bundle->textureEnvMode;
    image_t *image;

    if (bundle->isLightmap != 0 && r_lightmap->integer != 0)
        textureEnvMode = GL_REPLACE;

    image = RB_GetAnimatedImage(bundle, textureUnit);
    if (glState.currentTextureTargets[textureUnit] != image->target ||
        glState.currenttextures[textureUnit] != image->texnum ||
        glState.texEnv[textureUnit] != (int32_t)textureEnvMode) {
        if (glState.currenttmu != textureUnit) {
            qglActiveTextureARB(GL_TEXTURE0_ARB +
                                (uint32_t)textureUnit);
            glState.currenttmu = textureUnit;
        }

        if (glState.currentTextureTargets[textureUnit] != image->target) {
            if (glState.currentTextureTargets[textureUnit] != 0)
                qglDisable(glState.currentTextureTargets[textureUnit]);
            qglEnable(image->target);
            glState.currentTextureTargets[textureUnit] = image->target;
        }
        if (glState.currenttextures[textureUnit] != image->texnum) {
            image->frameUsed = tr.frameCount;
            qglBindTexture(image->target, image->texnum);
            glState.currenttextures[textureUnit] = image->texnum;
        }
        if (glState.texEnv[textureUnit] != (int32_t)textureEnvMode) {
            qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                       (int32_t)textureEnvMode);
            glState.texEnv[textureUnit] = (int32_t)textureEnvMode;
        }
    }

    if (textureEnvMode == GL_COMBINE_ARB) {
        const shader_texture_combine_t *combine = bundle->textureCombine;

        if (glState.currenttmu != textureUnit) {
            qglActiveTextureARB(GL_TEXTURE0_ARB +
                                (uint32_t)textureUnit);
            glState.currenttmu = textureUnit;
        }
        qglTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
                    combine->environmentColor);
        qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,
                   (int32_t)combine->rgb.operation);
        qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, combine->rgb.scale);
        qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB,
                   (int32_t)combine->rgb.sources[0]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB,
                   (int32_t)combine->rgb.sources[1]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB,
                   (int32_t)combine->rgb.sources[2]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB,
                   (int32_t)combine->rgb.operands[0]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB,
                   (int32_t)combine->rgb.operands[1]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB,
                   (int32_t)combine->rgb.operands[2]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB,
                   (int32_t)combine->alpha.operation);
        qglTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, combine->alpha.scale);
        qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB,
                   (int32_t)combine->alpha.sources[0]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB,
                   (int32_t)combine->alpha.sources[1]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB,
                   (int32_t)combine->alpha.sources[2]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB,
                   (int32_t)combine->alpha.operands[0]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB,
                   (int32_t)combine->alpha.operands[1]);
        qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB,
                   (int32_t)combine->alpha.operands[2]);
    }

    const shader_texture_shader_t *textureShader = bundle->textureShader;
    if (textureShader == NULL ||
        (textureShader->operation == 0 &&
         glState.textureShaderEnabled[textureUnit] == qfalse)) {
        return;
    }

    if (glState.currenttmu != textureUnit) {
        qglActiveTextureARB(GL_TEXTURE0_ARB + (uint32_t)textureUnit);
        glState.currenttmu = textureUnit;
    }
    qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV,
               (int32_t)textureShader->operation);

    switch (textureShader->operation) {
    case GL_CULL_FRAGMENT_NV:
        qglTexEnvfv(GL_TEXTURE_SHADER_NV, GL_CULL_MODES_NV,
                    textureShader->parameters.floats);
        break;
    case GL_DEPENDENT_AR_TEXTURE_2D_NV:
    case GL_DEPENDENT_GB_TEXTURE_2D_NV:
        qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV,
                   (int32_t)textureShader->previousTextureInput);
        break;
    case GL_DOT_PRODUCT_NV:
    case GL_DOT_PRODUCT_TEXTURE_2D_NV:
    case GL_DOT_PRODUCT_TEXTURE_CUBE_MAP_NV:
    case GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV:
    case GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV:
        qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV,
                   (int32_t)textureShader->previousTextureInput);
        qglTexEnvi(GL_TEXTURE_SHADER_NV,
                   GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV,
                   (int32_t)textureShader->dotProductMapping);
        break;
    case GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV:
        qglTexEnvfv(GL_TEXTURE_SHADER_NV, GL_CONST_EYE_NV,
                    textureShader->parameters.floats);
        qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV,
                   (int32_t)textureShader->previousTextureInput);
        qglTexEnvi(GL_TEXTURE_SHADER_NV,
                   GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV,
                   (int32_t)textureShader->dotProductMapping);
        break;
    default:
        break;
    }

    glState.textureShaderEnabled[textureUnit] =
        textureShader->operation != 0 ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x004ea970..0x004ea996.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ea970_004ea997.mcode.
 * Name: exact same-module Mac symbol RB_SetupTmu. Ghidra omitted this complete
 * wrapper because the Windows linker retained it without a direct caller. */
void RB_SetupTmu(int32_t textureUnit, shaderStage_t *stage)
{
    textureBundle_t *bundle = &stage->bundle[textureUnit];

    if (bundle->textureEnvMode != 0)
        RB_EnableTMU(bundle, textureUnit);
    else
        RB_DisableTMU(textureUnit);
}

/* Source: CoDUOMP.exe 0x004eab60..0x004ead54.
 * Name: exact same-module Mac symbol RB_SetupRegisterCombiners. */
void RB_SetupRegisterCombiners(
    renderer_register_combiners_t *registerCombiners)
{
    qglCombinerParameterfvNV(GL_CONSTANT_COLOR0_NV,
                             registerCombiners->constantColors[0]);
    qglCombinerParameterfvNV(GL_CONSTANT_COLOR1_NV,
                             registerCombiners->constantColors[1]);
    qglCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV,
                            registerCombiners->generalCombinerCount);

    const qboolean perStageConstantsAvailable =
        glConfig.registerCombinerMode >= R_REGISTER_COMBINERS_NV2;
    if (perStageConstantsAvailable != qfalse) {
        if (registerCombiners->perStageConstants != qfalse)
            qglEnable(GL_PER_STAGE_CONSTANTS_NV);
        else
            qglDisable(GL_PER_STAGE_CONSTANTS_NV);
    }

    for (int32_t stageIndex = 0;
         stageIndex < registerCombiners->generalCombinerCount;
         ++stageIndex) {
        const uint32_t stage = GL_COMBINER0_NV + (uint32_t)stageIndex;
        const renderer_general_combiner_t *combiner =
            &registerCombiners->general[stageIndex];

        for (int32_t inputIndex = 0; inputIndex < 4; ++inputIndex) {
            const uint32_t variable =
                GL_VARIABLE_A_NV + (uint32_t)inputIndex;
            const renderer_combiner_input_t *rgb =
                &combiner->rgb.inputs[inputIndex];
            const renderer_combiner_input_t *alpha =
                &combiner->alpha.inputs[inputIndex];

            qglCombinerInputNV(stage, GL_RGB, variable, rgb->input,
                               rgb->mapping, rgb->componentUsage);
            qglCombinerInputNV(stage, GL_ALPHA, variable, alpha->input,
                               alpha->mapping, alpha->componentUsage);
        }

        const renderer_combiner_output_t *rgbOutput =
            &combiner->rgb.output;
        qglCombinerOutputNV(
            stage, GL_RGB, rgbOutput->abOutput, rgbOutput->cdOutput,
            rgbOutput->sumOutput, rgbOutput->scale, rgbOutput->bias,
            rgbOutput->abDotProduct, rgbOutput->cdDotProduct,
            rgbOutput->muxSum);

        const renderer_combiner_output_t *alphaOutput =
            &combiner->alpha.output;
        qglCombinerOutputNV(
            stage, GL_ALPHA, alphaOutput->abOutput, alphaOutput->cdOutput,
            alphaOutput->sumOutput, alphaOutput->scale,
            alphaOutput->bias, alphaOutput->abDotProduct,
            alphaOutput->cdDotProduct, alphaOutput->muxSum);

        if (registerCombiners->perStageConstants != qfalse) {
            qglCombinerStageParameterfvNV(
                stage, GL_CONSTANT_COLOR0_NV,
                combiner->constantColors[0]);
            qglCombinerStageParameterfvNV(
                stage, GL_CONSTANT_COLOR1_NV,
                combiner->constantColors[1]);
        }
    }

    for (int32_t inputIndex = 0; inputIndex < 7; ++inputIndex) {
        const renderer_combiner_input_t *input =
            &registerCombiners->final.inputs[inputIndex];
        qglFinalCombinerInputNV(
            GL_VARIABLE_A_NV + (uint32_t)inputIndex, input->input,
            input->mapping, input->componentUsage);
    }

    GL_CheckErrors("RB_SetupRegisterCombiners");
}

/* Source: CoDUOMP.exe 0x00521c20..0x00521fb0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00521c20_00521fb1.mcode.
 * Name: exact same-module Mac symbol DeformText. The original replaces the
 * input quad with one atlas-mapped quad per non-space byte. Its center and
 * height come from the first four input vertices; the first staged normal
 * supplies the horizontal direction. */
void DeformText(const char *text)
{
    enum {
        DEFORM_TEXT_SOURCE_VERTEX_COUNT = 4,
        DEFORM_TEXT_ATLAS_GRID_SIZE = 16
    };
    const float atlasStep = 0.0625f;
    const uint8_t color[4] = {
        UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX
    };
    const vec3_t *normal = &tess.stageNormals[0];
    vec3_t center = {0.0f, 0.0f, 0.0f};
    /* 0x00521c23..0x00521c8d is the expanded cross product with the global
     * up axis. Preserve its multiply/subtract graph rather than replacing the
     * mathematically-zero terms with a literal. */
    const float normalZTimesZero =
        (float)((long double)(*normal)[2] * 0.0L);
    vec3_t left = {
        (float)((long double)(*normal)[1] * -1.0L -
                (long double)normalZTimesZero),
        (float)((long double)normalZTimesZero -
                (long double)(*normal)[0] * -1.0L),
        (float)((long double)(*normal)[0] * 0.0L -
                (long double)(*normal)[1] * 0.0L)
    };
    vec3_t up;
    vec3_t advance;
    /* Exact .rdata bounds at 0x005b9f64/0x005b9f60. */
    float minimumZ = 999999.0f;
    float maximumZ = -999999.0f;

    for (int32_t vertexIndex = 0;
         vertexIndex < DEFORM_TEXT_SOURCE_VERTEX_COUNT; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];

        center[0] += position[0];
        center[1] += position[1];
        center[2] += position[2];
        if (position[2] < minimumZ)
            minimumZ = position[2];
        if (position[2] > maximumZ)
            maximumZ = position[2];
    }

    center[0] *= 0.25f;
    center[1] *= 0.25f;
    center[2] *= 0.25f;

    up[0] = 0.0f;
    up[1] = 0.0f;
    up[2] = (maximumZ - minimumZ) * 0.5f;

    const float leftScale = up[2] * -0.75f;
    left[0] *= leftScale;
    left[1] *= leftScale;
    left[2] *= leftScale;

    const int32_t textLength = (int32_t)strlen(text);
    const float centerOffset = (float)(textLength - 1);

    center[0] += centerOffset * left[0];
    center[1] += centerOffset * left[1];
    center[2] += centerOffset * left[2];
    advance[0] = left[0] + left[0];
    advance[1] = left[1] + left[1];
    advance[2] = left[2] + left[2];

    tess.indexCount = 0;
    tess.vertexCount = 0;

    for (int32_t characterIndex = 0;
         characterIndex < textLength; ++characterIndex) {
        const uint8_t character = (uint8_t)text[characterIndex];

        if (character != ' ') {
            const float s1 =
                (float)(character &
                        (DEFORM_TEXT_ATLAS_GRID_SIZE - 1)) * atlasStep;
            const float t1 =
                (float)(character >> 4) * atlasStep;

            RB_AddQuadStampExt(center, left, up, color,
                               s1, t1, s1 + atlasStep, t1 + atlasStep);
        }

        center[0] -= advance[0];
        center[1] -= advance[1];
        center[2] -= advance[2];
    }
}

/* Source: CoDUOMP.exe 0x00521fc0..0x0052203c.
 * Name: exact same-module Mac symbol GlobalPositionToLocal. The expression
 * ordering follows the original x87 stack so each component retains its two
 * successive additions. */
void GlobalPositionToLocal(const vec3_t world, vec3_t local)
{
    /* 0x00521fc0..0x00522033 retains all three x87 subtraction results
     * across the complete unrolled transform and rounds only each output. */
    const long double translatedX =
        (long double)world[0] - backEnd.orientation.origin[0];
    const long double translatedY =
        (long double)world[1] - backEnd.orientation.origin[1];
    const long double translatedZ =
        (long double)world[2] - backEnd.orientation.origin[2];

    for (int32_t component = 0; component < 3; ++component) {
        local[component] = (float)(
            (translatedZ * backEnd.orientation.axis[component][2] +
             translatedY * backEnd.orientation.axis[component][1]) +
            translatedX * backEnd.orientation.axis[component][0]);
    }
}

/* Source: CoDUOMP.exe 0x00522040..0x005220a2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522040_005220a3.mcode.
 * Name: exact same-module Mac symbol GlobalVectorToLocal. Unlike the position
 * variant this applies only the current model orientation axes. Each sum is
 * grouped in the order emitted by the original x87 instructions. */
void GlobalVectorToLocal(const vec3_t world, vec3_t local)
{
    for (int32_t component = 0; component < 3; ++component) {
        local[component] =
            (backEnd.orientation.axis[component][0] * world[0] +
             backEnd.orientation.axis[component][2] * world[2]) +
            backEnd.orientation.axis[component][1] * world[1];
    }
}

/* Source: CoDUOMP.exe 0x005220b0..0x00522350.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005220b0_00522351.mcode.
 * Name: exact same-module Mac symbol AutospriteDeform. Each source group of
 * four vertices contributes its center and half-diagonal radius; the output
 * quad faces the current view and retains the first source vertex's color. */
void AutospriteDeform(void)
{
    enum {
        AUTOSPRITE_SOURCE_VERTEX_COUNT = 4,
        AUTOSPRITE_OUTPUT_INDEX_COUNT = 6
    };
    const float diagonalToHalfSide = 0.7070000171661377f;
    const int32_t sourceVertexCount = tess.vertexCount;

    if ((sourceVertexCount &
         (AUTOSPRITE_SOURCE_VERTEX_COUNT - 1)) != 0) {
        ri.Printf(R_PRINT_WARNING,
                  "Autosprite shader %s had odd vertex count\n",
                  tess.shader->name);
    }
    if (tess.indexCount !=
        (sourceVertexCount / AUTOSPRITE_SOURCE_VERTEX_COUNT) *
            AUTOSPRITE_OUTPUT_INDEX_COUNT) {
        ri.Printf(R_PRINT_WARNING,
                  "Autosprite shader %s had odd index count\n",
                  tess.shader->name);
    }

    vec3_t viewLeft;
    vec3_t viewUp;

    if (backEnd.currentEntity != &tr.worldEntity) {
        GlobalVectorToLocal(backEnd.viewParms.orientation.axis[1], viewLeft);
        GlobalVectorToLocal(backEnd.viewParms.orientation.axis[2], viewUp);
    } else {
        viewLeft[0] = backEnd.viewParms.orientation.axis[1][0];
        viewLeft[1] = backEnd.viewParms.orientation.axis[1][1];
        viewLeft[2] = backEnd.viewParms.orientation.axis[1][2];
        viewUp[0] = backEnd.viewParms.orientation.axis[2][0];
        viewUp[1] = backEnd.viewParms.orientation.axis[2][1];
        viewUp[2] = backEnd.viewParms.orientation.axis[2][2];
    }

    tess.vertexCount = 0;
    tess.indexCount = 0;
    if (sourceVertexCount <= 0)
        return;

    const int32_t quadCount =
        ((sourceVertexCount - 1) /
         AUTOSPRITE_SOURCE_VERTEX_COUNT) + 1;

    for (int32_t quadIndex = 0; quadIndex < quadCount; ++quadIndex) {
        const int32_t firstVertex =
            quadIndex * AUTOSPRITE_SOURCE_VERTEX_COUNT;
        const float *vertices =
            &tess.xyz[firstVertex * tess.vertexComponentCount];
        const int32_t stride = tess.vertexComponentCount;
        vec3_t center;
        vec3_t left;
        vec3_t up;

        center[0] =
            (((vertices[stride * 2] + vertices[stride]) + vertices[0]) +
             vertices[stride * 3]) * 0.25f;
        center[1] =
            (((vertices[stride * 2 + 1] + vertices[stride + 1]) +
              vertices[1]) + vertices[stride * 3 + 1]) * 0.25f;
        center[2] =
            (((vertices[stride * 2 + 2] + vertices[stride + 2]) +
              vertices[2]) + vertices[stride * 3 + 2]) * 0.25f;

        /* 0x005221f3..0x0052225b retains all three center deltas, the square
         * root, and the 0.707 scale in x87 registers until each left/up
         * component is stored. */
        const long double deltaX =
            (long double)vertices[0] - (long double)center[0];
        const long double deltaY =
            (long double)vertices[1] - (long double)center[1];
        const long double deltaZ =
            (long double)vertices[2] - (long double)center[2];
        const long double halfSide =
            sqrtl((deltaZ * deltaZ + deltaY * deltaY) +
                  deltaX * deltaX) *
            (long double)diagonalToHalfSide;

        left[0] = (float)((long double)viewLeft[0] * halfSide);
        left[1] = (float)((long double)viewLeft[1] * halfSide);
        left[2] = (float)((long double)viewLeft[2] * halfSide);
        up[0] = (float)((long double)viewUp[0] * halfSide);
        up[1] = (float)((long double)viewUp[1] * halfSide);
        up[2] = (float)((long double)viewUp[2] * halfSide);

        if (backEnd.viewParms.isMirror != qfalse) {
            left[0] = -left[0];
            left[1] = -left[1];
            left[2] = -left[2];
        }

        if (backEnd.currentEntity->e.nonNormalizedAxes != 0.0f) {
            const vec3_t *entityAxis =
                &backEnd.currentEntity->e.axis[0];
            /* 0x00522291..0x00522309 likewise retains the axis length and its
             * reciprocal through all six scale-and-store operations. */
            const long double axisX = (long double)(*entityAxis)[0];
            const long double axisY = (long double)(*entityAxis)[1];
            const long double axisZ = (long double)(*entityAxis)[2];
            const long double axisLength =
                sqrtl((axisZ * axisZ + axisY * axisY) + axisX * axisX);
            const long double inverseAxisLength =
                axisLength == 0.0L ? 0.0L : 1.0L / axisLength;

            left[0] = (float)((long double)left[0] * inverseAxisLength);
            left[1] = (float)((long double)left[1] * inverseAxisLength);
            left[2] = (float)((long double)left[2] * inverseAxisLength);
            up[0] = (float)((long double)up[0] * inverseAxisLength);
            up[1] = (float)((long double)up[1] * inverseAxisLength);
            up[2] = (float)((long double)up[2] * inverseAxisLength);
        }

        RB_AddQuadStampExt(center, left, up,
                           (const uint8_t *)&tess.vertexColors[firstVertex],
                           0.0f, 0.0f, 1.0f, 1.0f);
    }
}

/* Source: CoDUOMP.exe 0x00522360..0x005229cf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522360_005229d0.mcode.
 * Name: exact same-module Mac symbol Autosprite2Deform. For each pair of
 * indexed triangles, the original selects the two shortest directed edges,
 * treats their midpoints as the billboard's major axis, and rotates those
 * edges around that axis to face the view. */
void Autosprite2Deform(void)
{
    enum {
        AUTOSPRITE2_VERTICES_PER_QUAD = 4,
        AUTOSPRITE2_INDEXES_PER_QUAD = 6,
        AUTOSPRITE2_EDGE_COUNT = 6,
        AUTOSPRITE2_TRIANGLE_INDEX_COUNT = 3
    };

    if ((tess.vertexCount & (AUTOSPRITE2_VERTICES_PER_QUAD - 1)) != 0) {
        ri.Printf(R_PRINT_WARNING,
                  "Autosprite2 shader %s had odd vertex count\n",
                  tess.shader->name);
    }
    if (tess.indexCount !=
        (tess.vertexCount / AUTOSPRITE2_VERTICES_PER_QUAD) *
            AUTOSPRITE2_INDEXES_PER_QUAD) {
        ri.Printf(R_PRINT_WARNING,
                  "Autosprite2 shader %s had odd index count\n",
                  tess.shader->name);
    }

    vec3_t viewForward;

    if (backEnd.currentEntity != &tr.worldEntity) {
        GlobalVectorToLocal(backEnd.viewParms.orientation.axis[0],
                            viewForward);
    } else {
        viewForward[0] = backEnd.viewParms.orientation.axis[0][0];
        viewForward[1] = backEnd.viewParms.orientation.axis[0][1];
        viewForward[2] = backEnd.viewParms.orientation.axis[0][2];
    }

    for (int32_t firstIndex = 0;
         firstIndex < tess.indexCount;
         firstIndex += AUTOSPRITE2_INDEXES_PER_QUAD) {
        float shortestSquared = FLT_MAX;
        float secondShortestSquared = FLT_MAX;
        float *shortestMinus = NULL;
        float *shortestPlus = NULL;
        float *secondMinus = NULL;
        float *secondPlus = NULL;

        for (int32_t edgeIndex = 0;
             edgeIndex < AUTOSPRITE2_EDGE_COUNT; ++edgeIndex) {
            const int32_t triangleBase =
                (edgeIndex / AUTOSPRITE2_TRIANGLE_INDEX_COUNT) *
                AUTOSPRITE2_TRIANGLE_INDEX_COUNT;
            const int32_t triangleEdge =
                edgeIndex % AUTOSPRITE2_TRIANGLE_INDEX_COUNT;
            const uint16_t minusIndex =
                tess.indexes[firstIndex + triangleBase + triangleEdge];
            const uint16_t plusIndex =
                tess.indexes[
                    firstIndex + triangleBase +
                    (triangleEdge + 1) % AUTOSPRITE2_TRIANGLE_INDEX_COUNT];
            float *candidateMinus =
                &tess.xyz[(int32_t)minusIndex * tess.vertexComponentCount];
            float *candidatePlus =
                &tess.xyz[(int32_t)plusIndex * tess.vertexComponentCount];
            const float squaredLength =
                VectorDistanceSquared(candidateMinus, candidatePlus);

            if (squaredLength < shortestSquared) {
                secondShortestSquared = shortestSquared;
                secondMinus = shortestPlus;
                secondPlus = shortestMinus;
                shortestSquared = squaredLength;
                shortestMinus = candidateMinus;
                shortestPlus = candidatePlus;
            } else if (squaredLength < secondShortestSquared) {
                secondShortestSquared = squaredLength;
                secondMinus = candidatePlus;
                secondPlus = candidateMinus;
            }
        }

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (shortestMinus == NULL || shortestPlus == NULL ||
            secondMinus == NULL || secondPlus == NULL) {
            ri.Error(ERR_DROP, "\x15" "Autosprite2 shader %s has invalid geometry", tess.shader->name);
            return;
        }

        vec3_t shortestCenter = {
            (shortestMinus[0] + shortestPlus[0]) * 0.5f,
            (shortestMinus[1] + shortestPlus[1]) * 0.5f,
            (shortestMinus[2] + shortestPlus[2]) * 0.5f
        };
        vec3_t secondCenter = {
            (secondMinus[0] + secondPlus[0]) * 0.5f,
            (secondMinus[1] + secondPlus[1]) * 0.5f,
            (secondMinus[2] + secondPlus[2]) * 0.5f
        };
        const vec3_t centerDelta = {
            secondCenter[0] - shortestCenter[0],
            secondCenter[1] - shortestCenter[1],
            secondCenter[2] - shortestCenter[2]
        };
        vec3_t perpendicular = {
            centerDelta[1] * viewForward[2] -
                centerDelta[2] * viewForward[1],
            centerDelta[2] * viewForward[0] -
                centerDelta[0] * viewForward[2],
            centerDelta[0] * viewForward[1] -
                centerDelta[1] * viewForward[0]
        };

        (void)VectorNormalize(perpendicular);

        const float shortestHalfLength =
            sqrtf(shortestSquared) * 0.5f;
        const float secondHalfLength =
            sqrtf(secondShortestSquared) * 0.5f;

        shortestMinus[0] =
            shortestCenter[0] - perpendicular[0] * shortestHalfLength;
        shortestMinus[1] =
            shortestCenter[1] - perpendicular[1] * shortestHalfLength;
        shortestMinus[2] =
            shortestCenter[2] - perpendicular[2] * shortestHalfLength;
        shortestPlus[0] =
            shortestCenter[0] + perpendicular[0] * shortestHalfLength;
        shortestPlus[1] =
            shortestCenter[1] + perpendicular[1] * shortestHalfLength;
        shortestPlus[2] =
            shortestCenter[2] + perpendicular[2] * shortestHalfLength;

        secondPlus[0] =
            secondCenter[0] + perpendicular[0] * secondHalfLength;
        secondPlus[1] =
            secondCenter[1] + perpendicular[1] * secondHalfLength;
        secondPlus[2] =
            secondCenter[2] + perpendicular[2] * secondHalfLength;
        secondMinus[0] =
            secondCenter[0] - perpendicular[0] * secondHalfLength;
        secondMinus[1] =
            secondCenter[1] - perpendicular[1] * secondHalfLength;
        secondMinus[2] =
            secondCenter[2] - perpendicular[2] * secondHalfLength;
    }
}

/* Source: CoDUOMP.exe 0x00523990..0x005239a0.
 * The Windows compiler retained this out-of-line copy of the renderer's
 * inline x87 FISTP conversion while expanding the same operation at its live
 * call sites. No Mac traceback name survives; this role name distinguishes
 * its active-rounding-mode behavior from the CRT `_ftol2` helper. */
int32_t RB_X87RoundFloatToInt(float value)
{
    return coduo_x87_fistp_f32_i32(value);
}

/* Source: CoDUOMP.exe 0x00523b20..0x00523c75.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523b20_00523c76.mcode.
 * Name: exact same-module Mac symbol RB_CalcDiffuseColor. Nonpositive light
 * incidence uses the entity's prepacked ambient color; positive incidence
 * adds the scaled directed-light components and clamps only the upper byte
 * bound, exactly as the Windows loop does. */
void RB_CalcDiffuseColor(uint8_t colors[][4])
{
    const trRefEntity_t *entity = backEnd.currentEntity;

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const vec3_t *normal = &tess.stageNormals[vertexIndex];
        const long double incidenceWide =
            ((long double)entity->lightDir[2] *
                 (long double)(*normal)[2] +
             (long double)entity->lightDir[0] *
                 (long double)(*normal)[0]) +
            (long double)entity->lightDir[1] *
                (long double)(*normal)[1];
        const float incidence = (float)incidenceWide;

        if (incidenceWide <= (long double)0.0f) {
            memcpy(colors[vertexIndex], &entity->ambientLightInt,
                   sizeof(entity->ambientLightInt));
            continue;
        }

        for (int32_t component = 0; component < 3; ++component) {
            const float colorValue = (float)(
                (long double)incidence *
                    (long double)entity->directedLight[component] +
                (long double)entity->ambientLight[component]);
            int32_t colorComponent = (int32_t)lrintf(colorValue);

            if (colorComponent > UINT8_MAX)
                colorComponent = UINT8_MAX;
            colors[vertexIndex][component] = (uint8_t)colorComponent;
        }
        colors[vertexIndex][3] = UINT8_MAX;
    }
}

/* Source: CoDUOMP.exe 0x005239a0..0x00523b1d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005239a0_00523b1e.mcode.
 * Name: exact same-module Mac symbol RB_CalcSpecularAlpha. The fixed light
 * direction is reflected around each staged normal, then dotted with the
 * view direction normalized by the original 0x5f3759df estimate and one
 * Newton step. The positive result is raised to the fourth power. */
void RB_CalcSpecularAlpha(uint8_t colors[][4])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        const vec3_t *normal = &tess.stageNormals[vertexIndex];
        vec3_t incident = {
            rbSpecularLightOrigin[0] - position[0],
            rbSpecularLightOrigin[1] - position[1],
            rbSpecularLightOrigin[2] - position[2]
        };

        VectorNormalizeFast(incident);

        const long double incidenceRaw =
            ((long double)incident[2] * (*normal)[2] +
             (long double)incident[1] * (*normal)[1]) +
            (long double)incident[0] * (*normal)[0];
        const long double reflectedXRaw =
            incidenceRaw * (long double)(*normal)[0] * 2.0L -
            (long double)incident[0];
        const long double reflectedYRaw =
            incidenceRaw * (long double)(*normal)[1] * 2.0L -
            (long double)incident[1];
        const long double reflectedZRaw =
            incidenceRaw * (long double)(*normal)[2] * 2.0L -
            (long double)incident[2];

        const long double viewXRaw =
            (long double)backEnd.orientation.viewOrigin[0] -
            (long double)position[0];
        const float viewY =
            backEnd.orientation.viewOrigin[1] - position[1];
        const long double viewZRaw =
            (long double)backEnd.orientation.viewOrigin[2] -
            (long double)position[2];
        const float viewZ = (float)viewZRaw;
        const float viewLengthSquared = (float)(
            (viewZRaw * (long double)viewZ +
             (long double)viewY * (long double)viewY) +
            viewXRaw * viewXRaw);
        uint32_t inverseBits;
        float inverseEstimate;

        memcpy(&inverseBits, &viewLengthSquared, sizeof(inverseBits));
        uint32_t shiftedBits = inverseBits >> 1;
        if ((inverseBits & 0x80000000U) != 0U)
            shiftedBits |= 0x80000000U;
        inverseBits = 0x5f3759dfU - shiftedBits;
        memcpy(&inverseEstimate, &inverseBits, sizeof(inverseEstimate));

        const long double inverseViewLengthRaw =
            (long double)inverseEstimate *
            (1.5L - (long double)viewLengthSquared * 0.5L *
                        (long double)inverseEstimate *
                        (long double)inverseEstimate);
        const float specularDot = (float)(
            (((long double)viewZ * reflectedZRaw +
              (long double)viewY * reflectedYRaw) +
             viewXRaw * reflectedXRaw) * inverseViewLengthRaw);
        int32_t alpha;

        if (specularDot < 0.0f) {
            alpha = 0;
        } else {
            const long double squaredRaw =
                (long double)specularDot * (long double)specularDot;
            alpha = (int32_t)(squaredRaw * squaredRaw * 255.0L);
            if (alpha > UINT8_MAX)
                alpha = UINT8_MAX;
        }
        colors[vertexIndex][3] = (uint8_t)alpha;
    }
}

/* Source: CoDUOMP.exe 0x004ead60..0x004eae46.
 * Name: exact same-module Mac symbol RB_SetupVertexProgram. Environment
 * parameters 0 and 1 are positions in the current model space; parameter 2
 * is the current light's scaled diffuse color and parameter 3 contains its
 * attenuation coefficients. */
void RB_SetupVertexProgram(renderer_vertex_program_t *vertexProgram)
{
    vec4_t parameter;

    if (rendererCurrentVertexProgram != vertexProgram) {
        qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, vertexProgram->glProgramName);
        rendererCurrentVertexProgram = vertexProgram;
    }

    GlobalPositionToLocal(
        backEnd.viewParms.orientation.origin, parameter);
    parameter[3] = 1.0f;
    qglProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 0, parameter);

    if (backEnd.currentLight == NULL)
        return;

    GlobalPositionToLocal(backEnd.currentLight->position, parameter);
    parameter[3] = 1.0f;
    qglProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 1, parameter);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    parameter[0] = backEnd.currentLightScale *
                   backEnd.currentLight->diffuse[0];
    parameter[1] = backEnd.currentLightScale *
                   backEnd.currentLight->diffuse[1];
    parameter[2] = backEnd.currentLightScale *
                   backEnd.currentLight->diffuse[2];
    parameter[3] = backEnd.currentLightScale *
                   backEnd.currentLight->diffuse[3];
    qglProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, 2,
                                 backEnd.currentLight->diffuse);

    qglProgramEnvParameter4fARB(
        GL_VERTEX_PROGRAM_ARB, 3,
        backEnd.currentLight->constantAttenuation,
        backEnd.currentLight->linearAttenuation,
        backEnd.currentLight->quadraticAttenuation, 1.0f);
}

/* Source: CoDUOMP.exe 0x004eae70..0x004eafe6.
 * Name: exact same-module Mac symbol RB_SetupMultitexture. This routine keeps
 * both the client texture-coordinate arrays and the server texture units in
 * sync for every hardware unit before installing the stage's extension
 * programs. */
void RB_SetupMultitexture(
    shaderStage_t *stage,
    const void *const baseTexCoords[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride)
{
    const int32_t originalClientTmu = glState.currentClientTmu;
    const int32_t originalTmu = glState.currenttmu;
    textureBundle_t *bundle =
        &stage->bundle[originalClientTmu];

    if (bundle->textureEnvMode != 0) {
        RB_EnableClientTmu(originalClientTmu, bundle,
                           baseTexCoords[originalClientTmu],
                           vertexStride);
    } else {
        RB_DisableClientTmu(originalClientTmu);
    }

    bundle = &stage->bundle[originalTmu];
    if (bundle->textureEnvMode != 0)
        RB_EnableTMU(bundle, originalTmu);
    else
        RB_DisableTMU(originalTmu);

    for (int32_t textureUnit = 0;
         textureUnit < glConfig.maxActiveTextures;
         ++textureUnit) {
        bundle = &stage->bundle[textureUnit];

        if (textureUnit != originalClientTmu) {
            if (bundle->textureEnvMode != 0) {
                RB_EnableClientTmu(textureUnit, bundle,
                                   baseTexCoords[textureUnit],
                                   vertexStride);
            } else {
                RB_DisableClientTmu(textureUnit);
            }
        }

        if (textureUnit != originalTmu) {
            if (bundle->textureEnvMode != 0)
                RB_EnableTMU(bundle, textureUnit);
            else
                RB_DisableTMU(textureUnit);
        }
    }

    if (stage->registerCombiners != NULL)
        RB_SetupRegisterCombiners(stage->registerCombiners);
    if (stage->vertexProgram != NULL)
        RB_SetupVertexProgram(stage->vertexProgram);
    GL_BindFragmentShaderATI(stage->fragmentShaderATI);
}

/* Source: CoDUOMP.exe 0x004eaff0..0x004eb171.
 * Name: exact same-module Mac symbol RB_SetupMultitextureATI. It has the same
 * texture-unit and shader
 * extension coordination, but supplies object-buffer offsets through the
 * exact Mac RB_EnableClientTmuATI operation. */
void RB_SetupMultitextureATI(
    shaderStage_t *stage, uint32_t objectBuffer,
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride)
{
    const int32_t originalClientTmu = glState.currentClientTmu;
    const int32_t originalTmu = glState.currenttmu;
    textureBundle_t *bundle =
        &stage->bundle[originalClientTmu];

    if (bundle->textureEnvMode != 0) {
        RB_EnableClientTmuATI(originalClientTmu, bundle, objectBuffer,
                              texCoordOffsets[originalClientTmu],
                              vertexStride);
    } else {
        RB_DisableClientTmu(originalClientTmu);
    }

    bundle = &stage->bundle[originalTmu];
    if (bundle->textureEnvMode != 0)
        RB_EnableTMU(bundle, originalTmu);
    else
        RB_DisableTMU(originalTmu);

    for (int32_t textureUnit = 0;
         textureUnit < glConfig.maxActiveTextures;
         ++textureUnit) {
        bundle = &stage->bundle[textureUnit];

        if (textureUnit != originalClientTmu) {
            if (bundle->textureEnvMode != 0) {
                RB_EnableClientTmuATI(
                    textureUnit, bundle, objectBuffer,
                    texCoordOffsets[textureUnit], vertexStride);
            } else {
                RB_DisableClientTmu(textureUnit);
            }
        }

        if (textureUnit != originalTmu) {
            if (bundle->textureEnvMode != 0)
                RB_EnableTMU(bundle, textureUnit);
            else
                RB_DisableTMU(textureUnit);
        }
    }

    if (stage->registerCombiners != NULL)
        RB_SetupRegisterCombiners(stage->registerCombiners);
    if (stage->vertexProgram != NULL)
        RB_SetupVertexProgram(stage->vertexProgram);
    GL_BindFragmentShaderATI(stage->fragmentShaderATI);
}

/* Source: CoDUOMP.exe 0x004eb260..0x004eb27e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eb260_004eb27f.mcode.
 * Name and source-level argument order: exact same-module Mac symbol
 * RB_SetupStage. */
void RB_SetupStage(
    shaderStage_t *stage,
    const void *const baseTexCoords[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride)
{
    GL_State(stage->stateBits);
    RB_SetupMultitexture(stage, baseTexCoords, vertexStride);
}

/* Source: CoDUOMP.exe 0x004eb280..0x004eb2a3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eb280_004eb2a4.mcode.
 * Name and source-level argument order: exact same-module Mac symbol
 * RB_SetupStageATI. */
void RB_SetupStageATI(
    shaderStage_t *stage, uint32_t objectBuffer,
    const uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS],
    int32_t vertexStride)
{
    GL_State(stage->stateBits);
    RB_SetupMultitextureATI(stage, objectBuffer, texCoordOffsets,
                            vertexStride);
}

/* Source: CoDUOMP.exe 0x004e9f70..0x004ea173.
 * Name: exact same-module Mac symbol RB_ChooseSurfaceCountColor. Values above
 * showtris level 6 use the Windows-domain random provider, seeded from this
 * surface's total vertex/index workload and restored to the saved rand output
 * afterwards. Lower levels use fixed workload bands. */
void RB_ChooseSurfaceCountColor(int32_t indexCount, uint8_t color[4])
{
    const uint8_t identityLightByte = (uint8_t)tr.identityLightByte;

    if (r_showtris->integer > 6) {
        const int32_t savedRandomValue = coduo_crt_rand();
        const uint32_t surfaceSeed =
            ((uint32_t)tess.vertexCount + (uint32_t)indexCount) * 31337u;
        vec3_t randomColor;

        srand(surfaceSeed);
        randomColor[0] = (float)coduo_crt_rand() *
                         0.000030517578125f; /* exact 1/32768 */
        randomColor[1] = (float)coduo_crt_rand() *
                         0.000030517578125f;
        randomColor[2] = (float)coduo_crt_rand() *
                         0.000030517578125f;
        (void)VectorNormalize(randomColor);

        for (int32_t component = 0; component < 3; ++component) {
            const float scaledComponent = randomColor[component] * 255.0f;
            color[component] = (uint8_t)lrint(
                (double)scaledComponent +
                0.00000000093132257461547852); /* exact 2^-30 */
        }
        srand((uint32_t)savedRandomValue);
    } else if (indexCount <= 30) {
        color[0] = identityLightByte;
        color[1] = identityLightByte;
        color[2] = identityLightByte;
    } else if (indexCount <= 150) {
        color[0] = identityLightByte;
        color[1] = (uint8_t)(tr.identityLightByte >> 1);
        color[2] = (uint8_t)(tr.identityLightByte >> 1);
    } else if (indexCount <= 450) {
        color[0] = identityLightByte;
        color[1] = 0;
        color[2] = 0;
    } else if (indexCount <= 1200) {
        color[0] = identityLightByte;
        color[1] = identityLightByte;
        color[2] = 0;
    } else if (indexCount <= 3000) {
        color[0] = (uint8_t)(tr.identityLightByte >> 2);
        color[1] = (uint8_t)(tr.identityLightByte >> 2);
        color[2] = identityLightByte;
    } else {
        color[0] = 0;
        color[1] = identityLightByte;
        color[2] = 0;
    }

    color[3] = 255;
}

/* Source: CoDUOMP.exe 0x004ea1a0..0x004ea267.
 * Name: exact same-module Mac symbol RB_MakeNormalVectors. The Windows caller
 * at 0x004ebabb identifies the input as tess.stageNormals and the two outputs
 * as the tangent-space arrays at original offsets +0x580000 and +0x4c0000. */
void RB_MakeNormalVectors(const vec3_t normal, vec3_t tangent,
                          vec3_t bitangent)
{
    float inverse;
    float shared;

    if (normal[0] > -1.0f) {
        inverse = (float)(
            (long double)1.0f /
            ((long double)normal[0] + 1.0f));
        shared = (float)(
            (long double)normal[1] * normal[2] * inverse);

        tangent[0] = normal[1];
        tangent[1] = (float)(
            (long double)normal[1] * normal[1] * inverse - 1.0f);
        tangent[2] = shared;
        bitangent[0] = normal[2];
        bitangent[1] = shared;
        bitangent[2] = (float)(
            (long double)normal[2] * normal[2] * inverse - 1.0f);
        return;
    }

    inverse = (float)(
        (long double)1.0f /
        ((long double)normal[2] + 1.0f));
    shared = (float)(
        (long double)normal[0] * normal[1] * inverse);

    tangent[0] = -shared;
    tangent[1] = (float)(
        (long double)1.0f -
        (long double)normal[1] * normal[1] * inverse);
    tangent[2] = -normal[1];
    bitangent[0] = (float)(
        (long double)normal[0] * normal[0] * inverse - 1.0f);
    bitangent[1] = shared;
    bitangent[2] = normal[0];
}

/* Source: CoDUOMP.exe 0x005213e0..0x00521426.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005213e0_00521427.mcode.
 * Name: exact same-module Mac symbol TableForFunc. The Windows jump table
 * proves values 1..5 and their exact renderer waveform tables. */
const float *TableForFunc(shader_wave_func_t function)
{
    switch (function) {
    case SHADER_WAVE_SIN:
        return tr.sinTable;
    case SHADER_WAVE_SQUARE:
        return tr.squareTable;
    case SHADER_WAVE_TRIANGLE:
        return tr.triangleTable;
    case SHADER_WAVE_SAWTOOTH:
        return tr.sawToothTable;
    case SHADER_WAVE_INVERSE_SAWTOOTH:
        return tr.inverseSawToothTable;
    default:
        ri.Error(ERR_DROP,
                 "\x15" "TableForFunc called with invalid function '%d' "
                 "in shader '%s'\n",
                 function, tess.shader->name);
        return NULL;
    }
}

/* Source: CoDUOMP.exe 0x00521440..0x0052147c.
 * Name: exact same-module Mac symbol EvalWaveForm. The table coordinate is
 * stored as float before the original current-rounding-mode x87 FISTP. */
long double EvalWaveForm(const waveForm_t *waveform)
{
    const float *waveTable = TableForFunc(waveform->func);
    const float tableCoordinate = (float)(
        ((long double)tess.shaderTime * waveform->frequency +
         waveform->phase) * 1024.0f);
    const int32_t tableIndex =
        RB_X87RoundFloatToInt(tableCoordinate) & 1023;
    return (long double)waveTable[tableIndex] * waveform->amplitude +
           waveform->base;
}

/* Source: CoDUOMP.exe 0x00521480..0x005214e8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00521480_005214e9.mcode.
 * Name: exact same-module Mac symbol EvalWaveFormClamped. The Windows x87
 * comparisons leave NaN unchanged while clamping ordinary values to [0, 1]. */
long double EvalWaveFormClamped(const waveForm_t *waveform)
{
    const float *waveTable = TableForFunc(waveform->func);
    const float tableCoordinate = (float)(
        ((long double)tess.shaderTime * waveform->frequency +
         waveform->phase) * 1024.0f);
    const int32_t tableIndex =
        RB_X87RoundFloatToInt(tableCoordinate) & 1023;
    const long double value =
        (long double)waveTable[tableIndex] * waveform->amplitude +
        waveform->base;

    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

/* Source: CoDUOMP.exe 0x00522ae0..0x00522b11.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522ae0_00522b12.mcode.
 * Name: exact same-module Mac symbol RB_CalcColorFromEntity. */
void RB_CalcColorFromEntity(uint8_t colors[][4])
{
    if (backEnd.currentEntity == NULL)
        return;

    uint32_t packedColor;
    memcpy(&packedColor, backEnd.currentEntity->e.shaderRGBA,
           sizeof(packedColor));
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        memcpy(colors[vertexIndex], &packedColor, sizeof(packedColor));
    }
}

/* Source: CoDUOMP.exe 0x00522b20..0x00522b82.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522b20_00522b83.mcode.
 * Name: exact same-module Mac symbol RB_CalcColorFromOneMinusEntity. */
void RB_CalcColorFromOneMinusEntity(uint8_t colors[][4])
{
    if (backEnd.currentEntity == NULL)
        return;

    uint8_t invertedColor[4];
    for (int32_t component = 0; component < 4; ++component) {
        invertedColor[component] =
            (uint8_t)(UINT8_MAX -
                      backEnd.currentEntity->e.shaderRGBA[component]);
    }
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        memcpy(colors[vertexIndex], invertedColor, sizeof(invertedColor));
    }
}

/* Source: CoDUOMP.exe 0x00522b90..0x00522bc9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522b90_00522bca.mcode.
 * Name: exact same-module Mac symbol RB_CalcAlphaFromEntity. */
void RB_CalcAlphaFromEntity(uint8_t colors[][4])
{
    if (backEnd.currentEntity == NULL)
        return;

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        colors[vertexIndex][3] =
            backEnd.currentEntity->e.shaderRGBA[3];
    }
}

/* Source: CoDUOMP.exe 0x00522bd0..0x00522c10.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522bd0_00522c11.mcode.
 * Name: exact same-module Mac symbol RB_CalcAlphaFromOneMinusEntity. */
void RB_CalcAlphaFromOneMinusEntity(uint8_t colors[][4])
{
    if (backEnd.currentEntity == NULL)
        return;

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        colors[vertexIndex][3] =
            (uint8_t)(UINT8_MAX -
                      backEnd.currentEntity->e.shaderRGBA[3]);
    }
}

/* Source: CoDUOMP.exe 0x00522c20..0x00522d14.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522c20_00522d15.mcode.
 * Name: exact same-module Mac symbol RB_CalcWaveColor. Noise waves use
 * Com_NoiseGet4f(0,0,0,(shaderTime+phase)*frequency); table waves apply the
 * renderer identity-light scale after evaluating the waveform. */
void RB_CalcWaveColor(const waveForm_t *waveform,
                      uint8_t colors[][4])
{
    float value;

    if (waveform->func == SHADER_WAVE_NOISE) {
        value = Com_NoiseGet4f(
                    0.0f, 0.0f, 0.0f,
                    (tess.shaderTime + waveform->phase) * waveform->frequency) *
                    waveform->amplitude +
                waveform->base;
    } else {
        const float *waveTable = TableForFunc(waveform->func);
        const float tableCoordinate =
            (tess.shaderTime * waveform->frequency + waveform->phase) * 1024.0f;
        const int32_t tableIndex = (int32_t)lrintf(tableCoordinate) & 1023;

        value = (waveTable[tableIndex] * waveform->amplitude +
                 waveform->base) * tr.identityLight;
    }

    if (value < 0.0f)
        value = 0.0f;
    else if (value > 1.0f)
        value = 1.0f;

    const uint8_t colorByte = (uint8_t)(int32_t)lrintf(value * 255.0f);
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        colors[vertexIndex][0] = colorByte;
        colors[vertexIndex][1] = colorByte;
        colors[vertexIndex][2] = colorByte;
        colors[vertexIndex][3] = 255;
    }
}

/* Source: CoDUOMP.exe 0x00522d20..0x00522d57.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522d20_00522d58.mcode.
 * Name: exact same-module Mac symbol RB_CalcWaveAlpha. */
void RB_CalcWaveAlpha(const waveForm_t *waveform,
                      uint8_t colors[][4])
{
    const uint8_t alpha =
        (uint8_t)coduo_fp_to_u32_extended(
            EvalWaveFormClamped(waveform) * 255.0L);

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        colors[vertexIndex][3] = alpha;
    }
}

/* Source: CoDUOMP.exe 0x004ebdc0..0x004ec6f5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ebdc0_004ec6f6.mcode.
 * Name: exact same-module Mac symbol RB_ComputeColors. The shader parser at
 * 0x004ff3c8..0x004ff927 proves every generator value and its source spelling.
 * The color-wave branch passes stage+0x650 at 0x004ebfce, while the alpha-wave
 * branch passes the distinct stage+0x668 record at 0x004ec125.
 * The four dot modes share the same normalized local eye vector but retain
 * their distinct clamp formulas: dot, oneMinusDot, onePlusDot, negativeDot. */
void RB_ComputeColors(const shaderStage_t *stage)
{
    uint8_t (*colors)[4] = (uint8_t (*)[4])tess.stageVertexColors;
    const uint8_t (*vertexColors)[4] =
        (const uint8_t (*)[4])tess.vertexColors;

    switch (stage->rgbGen) {
    case CGEN_IDENTITY:
        memset(colors, UINT8_MAX,
               (size_t)((uint32_t)tess.vertexCount *
                        (uint32_t)sizeof(colors[0])));
        break;

    case CGEN_ENTITY:
        RB_CalcColorFromEntity(colors);
        break;

    case CGEN_ONE_MINUS_ENTITY:
        RB_CalcColorFromOneMinusEntity(colors);
        break;

    case CGEN_EXACT_VERTEX:
        memcpy(colors, vertexColors,
               (size_t)((uint32_t)tess.vertexCount *
                        (uint32_t)sizeof(colors[0])));
        break;

    case CGEN_VERTEX:
        if (tr.overbrightBits == 0) {
            memcpy(colors, vertexColors,
                   (size_t)((uint32_t)tess.vertexCount *
                            (uint32_t)sizeof(colors[0])));
        } else {
            for (int32_t vertexIndex = 0;
                 vertexIndex < tess.vertexCount; ++vertexIndex) {
                colors[vertexIndex][0] =
                    vertexColors[vertexIndex][0] >> tr.overbrightBits;
                colors[vertexIndex][1] =
                    vertexColors[vertexIndex][1] >> tr.overbrightBits;
                colors[vertexIndex][2] =
                    vertexColors[vertexIndex][2] >> tr.overbrightBits;
                colors[vertexIndex][3] = vertexColors[vertexIndex][3];
            }
        }
        break;

    case CGEN_ONE_MINUS_VERTEX:
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            for (int32_t component = 0; component < 3; ++component) {
                const int32_t inverse =
                    UINT8_MAX - vertexColors[vertexIndex][component];

                if (tr.identityLight == 1.0f) {
                    colors[vertexIndex][component] = (uint8_t)inverse;
                } else {
                    colors[vertexIndex][component] = (uint8_t)
                        coduo_fp_to_u32_extended(
                            (long double)inverse *
                            (long double)tr.identityLight);
                }
            }
        }
        break;

    case CGEN_WAVEFORM:
        RB_CalcWaveColor(&stage->rgbWave, colors);
        break;

    case CGEN_LIGHTING_AMBIENT:
    case CGEN_LIGHTING_DIFFUSE:
        if (tr.world->lightIndexCount != 0) {
            memset(colors, UINT8_MAX,
                   (size_t)((uint32_t)tess.vertexCount *
                            (uint32_t)sizeof(colors[0])));
        } else {
            RB_CalcDiffuseColor(colors);
        }
        break;

    case CGEN_LIGHTING_PRECALC:
        memset(colors, (uint8_t)tr.identityLightByte,
               (size_t)((uint32_t)tess.vertexCount *
                        (uint32_t)sizeof(colors[0])));
        break;

    case CGEN_CONSTANT:
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            memcpy(colors[vertexIndex], stage->constantColor,
                   sizeof(colors[vertexIndex]));
        }
        break;

    case CGEN_DEBUG_SURFACE_COUNT: {
        if (r_showtris->integer >= 5) {
            RB_ChooseSurfaceCountColor(tess.indexCount, colors[0]);
        } else {
            memcpy(colors[0], stage->constantColor, sizeof(colors[0]));
        }
        for (int32_t vertexIndex = 1;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            memcpy(colors[vertexIndex], colors[0], sizeof(colors[0]));
        }
        break;
    }

    case CGEN_BAD:
    case CGEN_IDENTITY_LIGHTING:
    default:
        memset(colors, (uint8_t)tr.identityLightByte,
               (size_t)((uint32_t)tess.vertexCount *
                        (uint32_t)sizeof(colors[0])));
        break;
    }

    switch (stage->alphaGen) {
    case AGEN_UNSPECIFIED:
        if (stage->rgbGen == CGEN_IDENTITY ||
            (stage->rgbGen == CGEN_VERTEX &&
             tr.identityLight == 1.0f)) {
            return;
        }
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            colors[vertexIndex][3] = UINT8_MAX;
        }
        return;

    case AGEN_IDENTITY:
        return;

    case AGEN_ENTITY:
        RB_CalcAlphaFromEntity(colors);
        return;

    case AGEN_ONE_MINUS_ENTITY:
        RB_CalcAlphaFromOneMinusEntity(colors);
        return;

    case AGEN_VERTEX:
        if (stage->rgbGen != CGEN_VERTEX) {
            for (int32_t vertexIndex = 0;
                 vertexIndex < tess.vertexCount; ++vertexIndex) {
                colors[vertexIndex][3] = vertexColors[vertexIndex][3];
            }
        }
        return;

    case AGEN_ONE_MINUS_VERTEX:
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            colors[vertexIndex][3] =
                (uint8_t)(UINT8_MAX - vertexColors[vertexIndex][3]);
        }
        return;

    case AGEN_LIGHTING_SPECULAR:
        RB_CalcSpecularAlpha(colors);
        return;

    case AGEN_WAVEFORM:
        RB_CalcWaveAlpha(&stage->alphaWave, colors);
        return;

    case AGEN_PORTAL:
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            const uint32_t componentOffset =
                (uint32_t)vertexIndex *
                (uint32_t)tess.vertexComponentCount;
            const float *position = &tess.xyz[componentOffset];
            const long double viewDeltaRaw[3] = {
                (long double)position[0] -
                    (long double)backEnd.viewParms.orientation.origin[0],
                (long double)position[1] -
                    (long double)backEnd.viewParms.orientation.origin[1],
                (long double)position[2] -
                    (long double)backEnd.viewParms.orientation.origin[2]
            };
            /* 0x004ec200..0x004ec248 carries all three subtractions, the
             * square root, and the quotient in x87 precision through the
             * first clamp comparison, while also storing the quotient as
             * float for the remaining path. */
            const long double alphaRaw = sqrtl(
                (viewDeltaRaw[2] * viewDeltaRaw[2] +
                 viewDeltaRaw[1] * viewDeltaRaw[1]) +
                viewDeltaRaw[0] * viewDeltaRaw[0]) /
                (long double)tess.shader->portalRange;
            float alpha = (float)alphaRaw;

            if (alphaRaw < 0.0L)
                alpha = 0.0f;
            else if (alpha > 1.0f)
                alpha = 1.0f;
            colors[vertexIndex][3] =
                (uint8_t)(int32_t)(alpha * 255.0f);
        }
        return;

    case AGEN_CONSTANT:
        if (stage->rgbGen != CGEN_CONSTANT) {
            for (int32_t vertexIndex = 0;
                 vertexIndex < tess.vertexCount; ++vertexIndex) {
                colors[vertexIndex][3] = stage->constantColor[3];
            }
        }
        return;

    case AGEN_DOT:
    case AGEN_ONE_MINUS_DOT:
    case AGEN_ONE_PLUS_DOT:
    case AGEN_NEGATIVE_DOT: {
        vec3_t localViewOrigin;

        if (backEnd.currentEntity == &tr.worldEntity) {
            localViewOrigin[0] =
                backEnd.viewParms.orientation.origin[0];
            localViewOrigin[1] =
                backEnd.viewParms.orientation.origin[1];
            localViewOrigin[2] =
                backEnd.viewParms.orientation.origin[2];
        } else {
            vec3_t translated = {
                backEnd.viewParms.orientation.origin[0] -
                    backEnd.currentEntity->e.origin[0],
                backEnd.viewParms.orientation.origin[1] -
                    backEnd.currentEntity->e.origin[1],
                backEnd.viewParms.orientation.origin[2] -
                    backEnd.currentEntity->e.origin[2]
            };

            VectorRotate(translated, backEnd.currentEntity->e.axis,
                         localViewOrigin);
        }

        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            const uint32_t componentOffset =
                (uint32_t)vertexIndex *
                (uint32_t)tess.vertexComponentCount;
            const float *position = &tess.xyz[componentOffset];
            vec3_t direction = {
                localViewOrigin[0] - position[0],
                localViewOrigin[1] - position[1],
                localViewOrigin[2] - position[2]
            };
            float alpha;

            VectorNormalizeFast(direction);
            alpha =
                (direction[2] * tess.stageNormals[vertexIndex][2] +
                 direction[0] * tess.stageNormals[vertexIndex][0]) +
                direction[1] * tess.stageNormals[vertexIndex][1];

            switch (stage->alphaGen) {
            case AGEN_DOT: {
                uint32_t alphaBits;
                memcpy(&alphaBits, &alpha, sizeof(alphaBits));
                alphaBits &=
                    ~(0u - (alphaBits >> 31));
                memcpy(&alpha, &alphaBits, sizeof(alpha));
                break;
            }
            case AGEN_ONE_MINUS_DOT: {
                uint32_t alphaBits;
                memcpy(&alphaBits, &alpha, sizeof(alphaBits));
                alphaBits &=
                    ~(0u - (alphaBits >> 31));
                memcpy(&alpha, &alphaBits, sizeof(alpha));
                alpha = 1.0f - alpha;
                break;
            }
            case AGEN_ONE_PLUS_DOT: {
                uint32_t alphaBits;
                memcpy(&alphaBits, &alpha, sizeof(alphaBits));
                alphaBits &=
                    0u - (alphaBits >> 31);
                memcpy(&alpha, &alphaBits, sizeof(alpha));
                alpha = 1.0f + alpha;
                break;
            }
            case AGEN_NEGATIVE_DOT: {
                uint32_t alphaBits;
                alpha = -alpha;
                memcpy(&alphaBits, &alpha, sizeof(alphaBits));
                alphaBits &=
                    ~(0u - (alphaBits >> 31));
                memcpy(&alpha, &alphaBits, sizeof(alpha));
                break;
            }
            default:
                break;
            }

            colors[vertexIndex][3] =
                (uint8_t)(int32_t)(alpha * 255.0f + 0.5f);
        }
        return;
    }
    }
}

/* Source: CoDUOMP.exe 0x00521580..0x005216ec.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00521580_005216ed.mcode.
 * Name: exact same-module Mac symbol RB_CalcDeformVertexes. Windows evaluates
 * one synchronized displacement when frequency is zero; otherwise it adds the
 * position sum times deformationSpread to each vertex's waveform phase. */
void RB_CalcDeformVertexes(const deformStage_t *deform)
{
    const float *waveTable = TableForFunc(deform->deformationWave.func);

    if (deform->deformationWave.frequency == 0.0f) {
        const float tableCoordinate = (float)(
            ((double)tess.shaderTime *
                 (double)deform->deformationWave.frequency +
             (double)deform->deformationWave.phase) *
            1024.0);
        const int32_t tableIndex = (int32_t)lrintf(tableCoordinate) & 1023;
        const float displacement =
            waveTable[tableIndex] * deform->deformationWave.amplitude +
            deform->deformationWave.base;

        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            float *position =
                &tess.xyz[vertexIndex * tess.vertexComponentCount];

            position[0] +=
                displacement * tess.stageNormals[vertexIndex][0];
            position[1] +=
                displacement * tess.stageNormals[vertexIndex][1];
            position[2] +=
                displacement * tess.stageNormals[vertexIndex][2];
        }
        return;
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        const float tableCoordinate = (float)(
            (((double)position[2] + (double)position[1] +
              (double)position[0]) *
                 (double)deform->deformationSpread +
             (double)deform->deformationWave.phase +
             (double)tess.shaderTime *
                 (double)deform->deformationWave.frequency) *
            1024.0);
        const int32_t tableIndex = (int32_t)lrintf(tableCoordinate) & 1023;
        const float displacement =
            waveTable[tableIndex] * deform->deformationWave.amplitude +
            deform->deformationWave.base;

        position[0] += displacement * tess.stageNormals[vertexIndex][0];
        position[1] += displacement * tess.stageNormals[vertexIndex][1];
        position[2] += displacement * tess.stageNormals[vertexIndex][2];
    }
}

/* Source: CoDUOMP.exe 0x005216f0..0x00521874.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005216f0_00521875.mcode.
 * Name: exact same-module Mac symbol RB_CalcFlapVertexes. This is the same
 * wave displacement as RB_CalcDeformVertexes, additionally scaled by either
 * the base S or T coordinate selected by the caller. */
void RB_CalcFlapVertexes(const deformStage_t *deform,
                         int32_t textureCoordinateAxis)
{
    const float *waveTable = TableForFunc(deform->deformationWave.func);

    if (deform->deformationWave.frequency == 0.0f) {
        const float tableCoordinate = (float)(
            ((double)tess.shaderTime *
                 (double)deform->deformationWave.frequency +
             (double)deform->deformationWave.phase) *
            1024.0);
        const int32_t tableIndex = (int32_t)lrintf(tableCoordinate) & 1023;
        const float displacement =
            waveTable[tableIndex] * deform->deformationWave.amplitude +
            deform->deformationWave.base;

        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            const float scaledDisplacement =
                displacement *
                tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                              [vertexIndex][textureCoordinateAxis];
            float *position =
                &tess.xyz[vertexIndex * tess.vertexComponentCount];

            position[0] +=
                scaledDisplacement * tess.stageNormals[vertexIndex][0];
            position[1] +=
                scaledDisplacement * tess.stageNormals[vertexIndex][1];
            position[2] +=
                scaledDisplacement * tess.stageNormals[vertexIndex][2];
        }
        return;
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        const float tableCoordinate = (float)(
            (((double)position[0] + (double)position[2] +
              (double)position[1]) *
                 (double)deform->deformationSpread +
             (double)deform->deformationWave.phase +
             (double)tess.shaderTime *
                 (double)deform->deformationWave.frequency) *
            1024.0);
        const int32_t tableIndex = (int32_t)lrintf(tableCoordinate) & 1023;
        const float displacement =
            (waveTable[tableIndex] * deform->deformationWave.amplitude +
             deform->deformationWave.base) *
            tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                          [vertexIndex][textureCoordinateAxis];

        position[0] += displacement * tess.stageNormals[vertexIndex][0];
        position[1] += displacement * tess.stageNormals[vertexIndex][1];
        position[2] += displacement * tess.stageNormals[vertexIndex][2];
    }
}

/* Source: CoDUOMP.exe 0x00521880..0x00521993.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00521880_00521994.mcode.
 * Name: exact same-module Mac symbol RB_CalcDeformSyncNormals. The adjacent
 * table-sample difference supplies the wave slope used to rotate each normal
 * consistently with position-synchronized wave deformation. */
void RB_CalcDeformSyncNormals(const deformStage_t *deform)
{
    const float *waveTable = TableForFunc(deform->deformationWave.func);

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        vec3_t *normal = &tess.stageNormals[vertexIndex];
        /* 0x005218b0..0x005218d9 sums Z+X+Y and retains that x87 value
         * through all three slope-factor calculations. */
        const long double normalSum =
            ((long double)(*normal)[2] + (long double)(*normal)[0]) +
            (long double)(*normal)[1];
        const vec3_t slopeFactors = {
            (float)(1.0L - normalSum * (long double)(*normal)[0]),
            (float)(1.0L - normalSum * (long double)(*normal)[1]),
            (float)(1.0L - normalSum * (long double)(*normal)[2])
        };
        const float tableCoordinate = (float)(
            (((double)position[2] + (double)position[1] +
              (double)position[0]) *
                 (double)deform->deformationSpread +
             (double)deform->deformationWave.phase +
             (double)tess.shaderTime *
                 (double)deform->deformationWave.frequency) *
            1024.0);
        const int32_t tableIndex = (int32_t)lrintf(tableCoordinate);
        /* 0x0052191e..0x00521957 retains the table difference and complete
         * scaled slope through the three component updates. */
        const long double slope =
            -(((long double)waveTable[(tableIndex + 1) & 1023] -
               (long double)waveTable[tableIndex & 1023]) *
              (long double)deform->deformationWave.amplitude * 1024.0L *
              (long double)deform->deformationSpread);

        (*normal)[0] = (float)(
            (long double)slopeFactors[0] * slope +
            (long double)(*normal)[0]);
        (*normal)[1] = (float)(
            (long double)slopeFactors[1] * slope +
            (long double)(*normal)[1]);
        (*normal)[2] = (float)(
            (long double)slopeFactors[2] * slope +
            (long double)(*normal)[2]);
        VectorNormalizeFast(*normal);
    }

    tess.stageTangentsValid = qfalse;
    tess.stageBitangentsValid = qfalse;
}

/* Source: CoDUOMP.exe 0x005219a0..0x00521ad3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005219a0_00521ad4.mcode.
 * Name: exact same-module Mac symbol RB_CalcDeformNormals. Windows samples
 * four-dimensional noise at three X offsets (0, 100, 200), adds the scaled
 * results to the normal components, and fast-normalizes the result. */
void RB_CalcDeformNormals(const deformStage_t *deform)
{
    const float positionScale = 0.98000001907348633f;
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        vec3_t *normal = &tess.stageNormals[vertexIndex];
        /* 0x005219c4..0x00521a88 reloads and rounds all four arguments before
         * each noise call rather than hoisting them across the calls. */
        (*normal)[0] +=
            Com_NoiseGet4f(
                position[0] * positionScale,
                position[1] * positionScale,
                position[2] * positionScale,
                (float)((double)tess.shaderTime *
                        (double)deform->deformationWave.frequency)) *
            deform->deformationWave.amplitude;
        (*normal)[1] +=
            Com_NoiseGet4f(
                position[0] * positionScale + 100.0f,
                position[1] * positionScale,
                position[2] * positionScale,
                (float)((double)tess.shaderTime *
                        (double)deform->deformationWave.frequency)) *
            deform->deformationWave.amplitude;
        (*normal)[2] +=
            Com_NoiseGet4f(
                position[0] * positionScale + 200.0f,
                position[1] * positionScale,
                position[2] * positionScale,
                (float)((double)tess.shaderTime *
                        (double)deform->deformationWave.frequency)) *
            deform->deformationWave.amplitude;
        VectorNormalizeFast(*normal);
    }

    tess.stageTangentsValid = qfalse;
    tess.stageBitangentsValid = qfalse;
}

/* Source: CoDUOMP.exe 0x00521ae0..0x00521b7b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00521ae0_00521b7c.mcode.
 * Name: exact same-module Mac symbol RB_CalcBulgeVertexes. The Windows table
 * phase uses milliseconds*0.001 and 1024/(2*pi), whose original float bit
 * patterns are retained below. */
void RB_CalcBulgeVertexes(const deformStage_t *deform)
{
    const float millisecondsToSeconds = 0.0010000000474974513f;
    const float waveTableStepsPerRadian = 162.97465515136719f;
    const float timePhase = (float)(
        (long double)backEnd.refdef.time *
        (long double)deform->bulgeSpeed *
        (long double)millisecondsToSeconds);

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const long double tableCoordinate =
            ((long double)deform->bulgeWidth *
                 (long double)tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                                            [vertexIndex][0] +
             (long double)timePhase) *
            (long double)waveTableStepsPerRadian;
        const uint32_t tableIndex =
            coduo_fp_to_u32_extended(tableCoordinate) & 1023u;
        const long double deformation =
            (long double)tr.sinTable[tableIndex] *
            (long double)deform->bulgeHeight;
        float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];

        position[0] = (float)(
            deformation * tess.stageNormals[vertexIndex][0] +
            (long double)position[0]);
        position[1] = (float)(
            deformation * tess.stageNormals[vertexIndex][1] +
            (long double)position[1]);
        position[2] = (float)(
            deformation * tess.stageNormals[vertexIndex][2] +
            (long double)position[2]);
    }
}

/* Source: CoDUOMP.exe 0x00521b80..0x00521c19.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00521b80_00521c1a.mcode.
 * Name: exact same-module Mac symbol RB_CalcMoveVertexes. */
void RB_CalcMoveVertexes(const deformStage_t *deform)
{
    const float *waveTable = TableForFunc(deform->deformationWave.func);
    const float tableCoordinate = (float)(
        ((long double)tess.shaderTime *
             (long double)deform->deformationWave.frequency +
         (long double)deform->deformationWave.phase) * 1024.0L);
    const int32_t tableIndex =
        RB_X87RoundFloatToInt(tableCoordinate) & 1023;
    const long double deformation =
        (long double)waveTable[tableIndex] *
            (long double)deform->deformationWave.amplitude +
        (long double)deform->deformationWave.base;
    const vec3_t move = {
        (float)(deformation * deform->moveVector[0]),
        (float)(deformation * deform->moveVector[1]),
        (float)(deformation * deform->moveVector[2])
    };

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];

        position[0] += move[0];
        position[1] += move[1];
        position[2] += move[2];
    }
}

/* Source: CoDUOMP.exe 0x005229d0..0x00522a94.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005229d0_00522a95.mcode and its jump
 * table at 0x00522a98. Name: exact same-module Mac symbol
 * RB_DeformTessGeometry. The jump table proves both the deform enum ordering
 * and the mapping from text modes 0..7 to the refdef's 32-byte text slots. */
void RB_DeformTessGeometry(void)
{
    shader_t *shader = tess.shader;

    for (int32_t deformIndex = 0;
         deformIndex < shader->numDeforms; ++deformIndex) {
        const deformStage_t *deform = &shader->deforms[deformIndex];

        switch (deform->deformation) {
        case DEFORM_WAVE:
            RB_CalcDeformVertexes(deform);
            break;
        case DEFORM_FLAP_S:
            RB_CalcFlapVertexes(deform, 0);
            break;
        case DEFORM_FLAP_T:
            RB_CalcFlapVertexes(deform, 1);
            break;
        case DEFORM_NORMALS:
            RB_CalcDeformNormals(deform);
            break;
        case DEFORM_SYNC_NORMALS:
            RB_CalcDeformSyncNormals(deform);
            break;
        case DEFORM_BULGE:
            RB_CalcBulgeVertexes(deform);
            break;
        case DEFORM_MOVE:
            RB_CalcMoveVertexes(deform);
            break;
        case DEFORM_PROJECTION_SHADOW:
            RB_ProjectionShadowDeform();
            break;
        case DEFORM_AUTOSPRITE:
            AutospriteDeform();
            break;
        case DEFORM_AUTOSPRITE2:
            Autosprite2Deform();
            break;
        case DEFORM_TEXT0:
        case DEFORM_TEXT1:
        case DEFORM_TEXT2:
        case DEFORM_TEXT3:
        case DEFORM_TEXT4:
        case DEFORM_TEXT5:
        case DEFORM_TEXT6:
        case DEFORM_TEXT7:
            DeformText(backEnd.refdef.text[
                deform->deformation - DEFORM_TEXT0]);
            break;
        }
    }
}

/* Source: CoDUOMP.exe 0x00523340..0x00523372.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523340_00523373.mcode.
 * Name: exact same-module Mac symbol RB_CalcSwapTexCoords. */
void RB_CalcSwapTexCoords(vec2_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float oldS = textureCoords[vertexIndex][0];

        textureCoords[vertexIndex][0] = textureCoords[vertexIndex][1];
        textureCoords[vertexIndex][1] = 1.0f - oldS;
    }
}

/* Source: CoDUOMP.exe 0x00523430..0x0052345f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523430_00523460.mcode.
 * Name: exact same-module Mac symbol RB_CalcScaleTexCoords. */
void RB_CalcScaleTexCoords(const vec2_t scale, vec2_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        textureCoords[vertexIndex][0] *= scale[0];
        textureCoords[vertexIndex][1] *= scale[1];
    }
}

/* Source: CoDUOMP.exe 0x00523460..0x005234da.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523460_005234db.mcode.
 * Name: exact same-module Mac symbol RB_CalcScrollTexCoords. */
void RB_CalcScrollTexCoords(const vec2_t scroll, vec2_t textureCoords[])
{
    float sOffset = tess.shaderTime * scroll[0];
    float tOffset = tess.shaderTime * scroll[1];

    sOffset = (float)((double)sOffset - floor((double)sOffset));
    tOffset = (float)((double)tOffset - floor((double)tOffset));

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        textureCoords[vertexIndex][0] += sOffset;
        textureCoords[vertexIndex][1] += tOffset;
    }
}

/* Source: CoDUOMP.exe 0x005234e0..0x00523526.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005234e0_00523527.mcode.
 * Name: exact same-module Mac symbol RB_CalcTransformTexCoords. */
void RB_CalcTransformTexCoords(const texModInfo_t *texMod,
                               vec2_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const long double oldS = textureCoords[vertexIndex][0];
        const long double oldT = textureCoords[vertexIndex][1];

        textureCoords[vertexIndex][0] = (float)(
            oldS * texMod->matrix[0][0] +
            oldT * texMod->matrix[1][0] + texMod->translate[0]);
        textureCoords[vertexIndex][1] = (float)(
            oldS * texMod->matrix[0][1] +
            oldT * texMod->matrix[1][1] + texMod->translate[1]);
    }
}

/* Source: CoDUOMP.exe 0x005235d0..0x0052362d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005235d0_0052362e.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapNegateTexCoords. */
void RB_CalcCubeMapNegateTexCoords(vec3_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        textureCoords[vertexIndex][0] = -textureCoords[vertexIndex][0];
        textureCoords[vertexIndex][1] = -textureCoords[vertexIndex][1];
        textureCoords[vertexIndex][2] = -textureCoords[vertexIndex][2];
    }
}

/* Source: CoDUOMP.exe 0x005214f0..0x00521573.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005214f0_00521574.mcode.
 * Name: exact same-module Mac symbol RB_CalcStretchTexCoords. The waveform
 * table coordinate is stored to float before the original x87 FISTP; lrintf
 * retains that current-rounding-mode conversion on portable hosts. */
void RB_CalcStretchTexCoords(const waveForm_t *waveform,
                             vec2_t textureCoords[])
{
    const float *waveTable = TableForFunc(waveform->func);
    /* 0x005214fa..0x0052150e retains the x87 multiply/add/multiply result
     * until the single binary32 store used by FISTP. */
    const float tableCoordinate = (float)(
        ((long double)tess.shaderTime * waveform->frequency +
         waveform->phase) * 1024.0f);
    const int32_t tableIndex = (int32_t)lrintf(tableCoordinate) & 1023;
    const long double stretch =
        (long double)1.0f /
        ((long double)waveTable[tableIndex] * waveform->amplitude +
         waveform->base);
    const long double translate =
        (long double)0.5f - stretch * 0.5f;
    texModInfo_t transform = {0};

    transform.matrix[0][0] = (float)stretch;
    transform.matrix[1][1] = (float)stretch;
    transform.translate[0] = (float)translate;
    transform.translate[1] = (float)translate;
    RB_CalcTransformTexCoords(&transform, textureCoords);
}

/* Source: CoDUOMP.exe 0x00523380..0x00523422.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523380_00523423.mcode.
 * Name: exact same-module Mac symbol RB_CalcTurbulentTexCoords. The table
 * conversion uses the original _ftol2 truncation semantics. */
void RB_CalcTurbulentTexCoords(const waveForm_t *waveform,
                               vec2_t textureCoords[])
{
    const float phase = (float)(
        (long double)tess.shaderTime * (long double)waveform->frequency +
        (long double)waveform->phase);

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        const long double sTableCoordinate =
            (((long double)position[2] + (long double)position[0]) *
                 0x1p-10L + (long double)phase) * 1024.0L;
        const long double tTableCoordinate =
            ((long double)position[1] * 0x1p-10L +
             (long double)phase) * 1024.0L;
        const uint32_t sTableIndex =
            coduo_fp_to_u32_extended(sTableCoordinate) & 1023u;
        const uint32_t tTableIndex =
            coduo_fp_to_u32_extended(tTableCoordinate) & 1023u;

        textureCoords[vertexIndex][0] = (float)(
            (long double)tr.sinTable[sTableIndex] *
                (long double)waveform->amplitude +
            (long double)textureCoords[vertexIndex][0]);
        textureCoords[vertexIndex][1] = (float)(
            (long double)tr.sinTable[tTableIndex] *
                (long double)waveform->amplitude +
            (long double)textureCoords[vertexIndex][1]);
    }
}

/* Source: CoDUOMP.exe 0x00523530..0x005235c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523530_005235ca.mcode.
 * Name: exact same-module Mac symbol RB_CalcRotateTexCoords. */
void RB_CalcRotateTexCoords(float rotateSpeed, vec2_t textureCoords[])
{
    const float tableStepsPerDegree = 2.8444445133209229f;
    const uint32_t angle =
        coduo_fp_to_u32_extended(
            -(long double)tess.shaderTime *
            (long double)rotateSpeed *
            (long double)tableStepsPerDegree);
    const float sine = tr.sinTable[angle & 1023u];
    /* ADD EAX,0x100 at 0x00523557 wraps in 32 bits before the table mask. */
    const float cosine =
        tr.sinTable[(angle + 256u) & 1023u];
    const float cosineHalf =
        (float)((long double)cosine * 0.5L);
    const float sineHalf =
        (float)((long double)sine * 0.5L);
    texModInfo_t transform = {0};

    transform.matrix[0][0] = cosine;
    transform.matrix[0][1] = sine;
    transform.matrix[1][0] = -sine;
    transform.matrix[1][1] = cosine;
    transform.translate[0] = (float)(
        (0.5L - (long double)cosineHalf) + (long double)sineHalf);
    transform.translate[1] = (float)(
        (0.5L - (long double)sineHalf) - (long double)cosineHalf);
    RB_CalcTransformTexCoords(&transform, textureCoords);
}

/* Source: CoDUOMP.exe 0x00522d60..0x00522e28.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522d60_00522e29.mcode.
 * Name: exact same-module Mac symbol RB_CalcEnvironmentTexCoords. The Windows
 * code normalizes the vertex-to-eye delta, reflects its Y/Z components about
 * the vertex normal, and maps those two components into the 2D texture range. */
void RB_CalcEnvironmentTexCoords(vec2_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        vec3_t vertexToEye = {
            backEnd.orientation.viewOrigin[0] - position[0],
            backEnd.orientation.viewOrigin[1] - position[1],
            backEnd.orientation.viewOrigin[2] - position[2]
        };

        VectorNormalizeFast(vertexToEye);

        const long double dot =
            (((long double)vertexToEye[0] *
                  tess.stageNormals[vertexIndex][0] +
              (long double)vertexToEye[1] *
                  tess.stageNormals[vertexIndex][1]) +
             (long double)vertexToEye[2] *
                 tess.stageNormals[vertexIndex][2]);
        const long double reflectedY =
            (dot * tess.stageNormals[vertexIndex][1]) * 2.0L -
            vertexToEye[1];
        const float reflectedZ = (float)(
            (dot * tess.stageNormals[vertexIndex][2]) * 2.0L -
            vertexToEye[2]);

        textureCoords[vertexIndex][0] =
            (float)((reflectedY + 1.0L) * 0.5L);
        textureCoords[vertexIndex][1] =
            (float)(0.5L - (long double)reflectedZ * 0.5L);
    }
}

/* Source: CoDUOMP.exe 0x00522e30..0x00522e7b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522e30_00522e7c.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapEyeToVertexTexCoords. */
void RB_CalcCubeMapEyeToVertexTexCoords(vec3_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];

        textureCoords[vertexIndex][0] =
            position[0] - backEnd.orientation.viewOrigin[0];
        textureCoords[vertexIndex][1] =
            position[1] - backEnd.orientation.viewOrigin[1];
        textureCoords[vertexIndex][2] =
            position[2] - backEnd.orientation.viewOrigin[2];
    }
}

/* Source: CoDUOMP.exe 0x00522e80..0x00522ecb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522e80_00522ecc.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapVertexToEyeTexCoords. */
void RB_CalcCubeMapVertexToEyeTexCoords(vec3_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];

        textureCoords[vertexIndex][0] =
            backEnd.orientation.viewOrigin[0] - position[0];
        textureCoords[vertexIndex][1] =
            backEnd.orientation.viewOrigin[1] - position[1];
        textureCoords[vertexIndex][2] =
            backEnd.orientation.viewOrigin[2] - position[2];
    }
}

/* Source: CoDUOMP.exe 0x00522ed0..0x00522f7a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522ed0_00522f7b.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapReflectionTexCoords. The
 * Windows helper intentionally reflects the unnormalized vertex-to-eye vector. */
void RB_CalcCubeMapReflectionTexCoords(vec3_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        const long double vertexToEyeX =
            (long double)backEnd.orientation.viewOrigin[0] - position[0];
        const long double vertexToEyeY =
            (long double)backEnd.orientation.viewOrigin[1] - position[1];
        const float vertexToEyeZ = (float)(
            (long double)backEnd.orientation.viewOrigin[2] - position[2]);
        const long double dot =
            ((vertexToEyeY * tess.stageNormals[vertexIndex][1] +
              vertexToEyeX * tess.stageNormals[vertexIndex][0]) +
             (long double)vertexToEyeZ *
                 tess.stageNormals[vertexIndex][2]);
        const long double doubledDot = dot + dot;
        const long double reflectedX =
            doubledDot * tess.stageNormals[vertexIndex][0];
        const float reflectedY = (float)(
            doubledDot * tess.stageNormals[vertexIndex][1]);
        const float reflectedZ = (float)(
            doubledDot * tess.stageNormals[vertexIndex][2]);

        textureCoords[vertexIndex][0] =
            (float)(reflectedX - vertexToEyeX);
        textureCoords[vertexIndex][1] =
            (float)((long double)reflectedY - vertexToEyeY);
        textureCoords[vertexIndex][2] =
            (float)((long double)reflectedZ - vertexToEyeZ);
    }
}

/* Source: CoDUOMP.exe 0x00522f80..0x0052306c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00522f80_0052306d.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapLightVectorTexCoords. The
 * light's homogeneous position is transformed into the current surface space
 * before the per-vertex subtraction. */
void RB_CalcCubeMapLightVectorTexCoords(vec3_t textureCoords[])
{
    const renderer_light_t *light = backEnd.currentLight;
    long double localLightPosition[3];
    vec3_t transformedLightPosition;

    if (light->position[3] == 0.0f) {
        localLightPosition[0] = light->position[0];
        localLightPosition[1] = light->position[1];
        localLightPosition[2] = light->position[2];
    } else {
        localLightPosition[0] =
            light->position[0] - backEnd.orientation.origin[0];
        localLightPosition[1] =
            light->position[1] - backEnd.orientation.origin[1];
        localLightPosition[2] =
            light->position[2] - backEnd.orientation.origin[2];
    }

    for (int32_t axis = 0; axis < 3; ++axis) {
        /* 0x00522f94..0x0052302a retains all three source-position
         * components in x87 across the unrolled transform, grouping each
         * result as (Z*axisZ + Y*axisY) + X*axisX before its float store. */
        transformedLightPosition[axis] = (float)(
            (localLightPosition[2] *
                 (long double)backEnd.orientation.axis[axis][2] +
             localLightPosition[1] *
                 (long double)backEnd.orientation.axis[axis][1]) +
            localLightPosition[0] *
                (long double)backEnd.orientation.axis[axis][0]);
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];

        textureCoords[vertexIndex][0] =
            transformedLightPosition[0] - position[0];
        textureCoords[vertexIndex][1] =
            transformedLightPosition[1] - position[1];
        textureCoords[vertexIndex][2] =
            transformedLightPosition[2] - position[2];
    }
}

/* Source: CoDUOMP.exe 0x00523070..0x005231c7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523070_005231c8.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapLightHalfAngleTexCoords.
 * Windows normalizes the vertex-to-eye and light-to-vertex vectors separately,
 * adds them, and deliberately does not normalize the resulting half vector. */
void RB_CalcCubeMapLightHalfAngleTexCoords(vec3_t textureCoords[])
{
    const renderer_light_t *light = backEnd.currentLight;
    long double localLightPosition[3];
    vec3_t transformedLightPosition;

    if (light->position[3] == 0.0f) {
        localLightPosition[0] = light->position[0];
        localLightPosition[1] = light->position[1];
        localLightPosition[2] = light->position[2];
    } else {
        localLightPosition[0] =
            light->position[0] - backEnd.orientation.origin[0];
        localLightPosition[1] =
            light->position[1] - backEnd.orientation.origin[1];
        localLightPosition[2] =
            light->position[2] - backEnd.orientation.origin[2];
    }

    for (int32_t axis = 0; axis < 3; ++axis) {
        /* 0x00523085..0x0052311d retains the same three-component x87
         * carrier and (Z*axisZ + Y*axisY) + X*axisX grouping as the light
         * vector generator before rounding each transformed component. */
        transformedLightPosition[axis] = (float)(
            (localLightPosition[2] *
                 (long double)backEnd.orientation.axis[axis][2] +
             localLightPosition[1] *
                 (long double)backEnd.orientation.axis[axis][1]) +
            localLightPosition[0] *
                (long double)backEnd.orientation.axis[axis][0]);
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        vec3_t vertexToEye = {
            backEnd.orientation.viewOrigin[0] - position[0],
            backEnd.orientation.viewOrigin[1] - position[1],
            backEnd.orientation.viewOrigin[2] - position[2]
        };
        vec3_t vertexToLight = {
            transformedLightPosition[0] - position[0],
            transformedLightPosition[1] - position[1],
            transformedLightPosition[2] - position[2]
        };

        VectorNormalizeFast(vertexToEye);
        VectorNormalizeFast(vertexToLight);
        textureCoords[vertexIndex][0] = vertexToLight[0] + vertexToEye[0];
        textureCoords[vertexIndex][1] = vertexToLight[1] + vertexToEye[1];
        textureCoords[vertexIndex][2] = vertexToLight[2] + vertexToEye[2];
    }
}

/* Source: CoDUOMP.exe 0x005231d0..0x00523260.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005231d0_00523261.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapSunHalfAngleTexCoords. */
void RB_CalcCubeMapSunHalfAngleTexCoords(vec3_t textureCoords[])
{
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const float *position =
            &tess.xyz[vertexIndex * tess.vertexComponentCount];
        vec3_t vertexToEye = {
            backEnd.orientation.viewOrigin[0] - position[0],
            backEnd.orientation.viewOrigin[1] - position[1],
            backEnd.orientation.viewOrigin[2] - position[2]
        };

        VectorNormalizeFast(vertexToEye);
        textureCoords[vertexIndex][0] = vertexToEye[0] + tr.sunDirection[0];
        textureCoords[vertexIndex][1] = vertexToEye[1] + tr.sunDirection[1];
        textureCoords[vertexIndex][2] = vertexToEye[2] + tr.sunDirection[2];
    }
}

/* Source: CoDUOMP.exe 0x00523270..0x005232c8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523270_005232c9.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapTbnTexCoords. */
void RB_CalcCubeMapTbnTexCoords(vec3_t textureCoords[], int32_t axis)
{
    if (tess.stageTangentsValid == qfalse ||
        tess.stageBitangentsValid == qfalse) {
        RB_CalcTangentSpace();
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        textureCoords[vertexIndex][0] = tess.stageTangents[vertexIndex][axis];
        textureCoords[vertexIndex][1] = tess.stageBitangents[vertexIndex][axis];
        textureCoords[vertexIndex][2] = tess.stageNormals[vertexIndex][axis];
    }
}

/* Source: CoDUOMP.exe 0x005232d0..0x0052333a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005232d0_0052333b.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapDot3ReflectTexCoords. The
 * fourth coordinate uses the selected world-view-origin and packed XYZ lanes;
 * unlike the other generators, this machine loop has a fixed vec3 position
 * stride rather than tess.vertexComponentCount. */
void RB_CalcCubeMapDot3ReflectTexCoords(vec4_t textureCoords[], int32_t axis)
{
    if (tess.stageTangentsValid == qfalse ||
        tess.stageBitangentsValid == qfalse) {
        RB_CalcTangentSpace();
    }

    const vec3_t *positions = (const vec3_t *)tess.xyz;
    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        textureCoords[vertexIndex][0] = tess.stageTangents[vertexIndex][axis];
        textureCoords[vertexIndex][1] = tess.stageBitangents[vertexIndex][axis];
        textureCoords[vertexIndex][2] = tess.stageNormals[vertexIndex][axis];
        textureCoords[vertexIndex][3] =
            backEnd.viewParms.orientation.origin[axis] -
            positions[vertexIndex][axis];
    }
}

/* Source: CoDUOMP.exe 0x00523630..0x005236d8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00523630_005236d9.mcode.
 * The same-module Mac compiler inlines this Windows helper, so its descriptive
 * name is inferred from its proven role in RB_CalcTangentSpace. */
static void RB_CalcTriangleTangent(uint16_t index0, uint16_t index1,
                                   uint16_t index2, vec3_t tangent)
{
    const float *position0 =
        &tess.xyz[(int32_t)index0 * tess.vertexComponentCount];
    const float *position1 =
        &tess.xyz[(int32_t)index1 * tess.vertexComponentCount];
    const float *position2 =
        &tess.xyz[(int32_t)index2 * tess.vertexComponentCount];
    const long double t0 =
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][index0][1];
    const long double t1 =
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][index1][1];
    const long double t2 =
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][index2][1];
    const long double weight0 = t1 - t2;
    const long double weight1 = t2 - t0;
    const long double weight2 = t0 - t1;

    for (int32_t axis = 0; axis < 3; ++axis) {
        tangent[axis] = (float)(
            weight0 * (long double)position0[axis] +
            weight1 * (long double)position1[axis] +
            weight2 * (long double)position2[axis]);
    }
    VectorNormalizeFast(tangent);
}

/* Source: CoDUOMP.exe 0x005236e0..0x005238e7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005236e0_005238e8.mcode.
 * Name: exact same-module Mac symbol RB_CalcTangentSpace. Windows accumulates
 * one fast-normalized tangent per indexed triangle, normalizes shared tangent
 * sums precisely, then derives and precisely normalizes normal-cross-tangent
 * bitangents. */
void RB_CalcTangentSpace(void)
{
    int32_t vertexTriangleCounts[R_MAX_TESS_VERTICES];

    memset(tess.stageTangents, 0,
           (size_t)((uint32_t)tess.vertexCount *
                    (uint32_t)sizeof(tess.stageTangents[0])));
    memset(tess.stageBitangents, 0,
           (size_t)((uint32_t)tess.vertexCount *
                    (uint32_t)sizeof(tess.stageBitangents[0])));
    memset(vertexTriangleCounts, 0,
           (size_t)((uint32_t)tess.vertexCount *
                    (uint32_t)sizeof(vertexTriangleCounts[0])));

    for (int32_t indexOffset = 0;
         indexOffset < tess.indexCount; indexOffset += 3) {
        const uint16_t index0 = tess.indexes[indexOffset];
        const uint16_t index1 = tess.indexes[indexOffset + 1];
        const uint16_t index2 = tess.indexes[indexOffset + 2];
        vec3_t triangleTangent;

        RB_CalcTriangleTangent(index0, index1, index2, triangleTangent);
        ++vertexTriangleCounts[index0];
        ++vertexTriangleCounts[index1];
        ++vertexTriangleCounts[index2];

        for (int32_t axis = 0; axis < 3; ++axis) {
            tess.stageTangents[index0][axis] += triangleTangent[axis];
            tess.stageTangents[index1][axis] += triangleTangent[axis];
            tess.stageTangents[index2][axis] += triangleTangent[axis];
        }
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        if (vertexTriangleCounts[vertexIndex] > 1)
            (void)VectorNormalize(tess.stageTangents[vertexIndex]);

        tess.stageBitangents[vertexIndex][0] =
            tess.stageNormals[vertexIndex][1] *
                tess.stageTangents[vertexIndex][2] -
            tess.stageNormals[vertexIndex][2] *
                tess.stageTangents[vertexIndex][1];
        tess.stageBitangents[vertexIndex][1] =
            tess.stageNormals[vertexIndex][2] *
                tess.stageTangents[vertexIndex][0] -
            tess.stageNormals[vertexIndex][0] *
                tess.stageTangents[vertexIndex][2];
        tess.stageBitangents[vertexIndex][2] =
            tess.stageNormals[vertexIndex][0] *
                tess.stageTangents[vertexIndex][1] -
            tess.stageNormals[vertexIndex][1] *
                tess.stageTangents[vertexIndex][0];
        (void)VectorNormalize(tess.stageBitangents[vertexIndex]);
    }

    tess.stageTangentsValid = qtrue;
    tess.stageBitangentsValid = qtrue;
}

/* Source: CoDUOMP.exe 0x005238f0..0x0052398c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005238f0_0052398d.mcode.
 * Name: exact same-module Mac symbol RB_CalcCubeMapBumpmapFrameTexCoords. */
void RB_CalcCubeMapBumpmapFrameTexCoords(vec3_t textureCoords[])
{
    if (tess.stageTangentsValid == qfalse ||
        tess.stageBitangentsValid == qfalse) {
        RB_CalcTangentSpace();
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const long double source[3] = {
            textureCoords[vertexIndex][0],
            textureCoords[vertexIndex][1],
            textureCoords[vertexIndex][2]
        };

        textureCoords[vertexIndex][0] = (float)(
            (source[2] *
                 (long double)tess.stageTangents[vertexIndex][2] +
             source[0] *
                 (long double)tess.stageTangents[vertexIndex][0]) +
            source[1] *
                (long double)tess.stageTangents[vertexIndex][1]);
        textureCoords[vertexIndex][1] = (float)(
            (source[2] *
                 (long double)tess.stageBitangents[vertexIndex][2] +
             source[1] *
                 (long double)tess.stageBitangents[vertexIndex][1]) +
            source[0] *
                (long double)tess.stageBitangents[vertexIndex][0]);
        textureCoords[vertexIndex][2] = (float)(
            (source[1] *
                 (long double)tess.stageNormals[vertexIndex][1] +
             source[2] *
                 (long double)tess.stageNormals[vertexIndex][2]) +
            source[0] *
                (long double)tess.stageNormals[vertexIndex][0]);
    }
}

/* Source: CoDUOMP.exe 0x004ec760..0x004ecace.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ec760_004ecacf.mcode.
 * Name and dispatch roles: same-module Mac RB_ComputeTexCoords and its named
 * callees. The Windows body proves the eight 0xc8-byte bundle iterations,
 * all generator/modifier values, copy widths, array choices, and the special
 * set-arrays-once copy rule. */
void RB_ComputeTexCoords(shaderStage_t *stage)
{
    for (int32_t textureUnit = 0;
         textureUnit < R_MAX_TEXTURE_UNITS; ++textureUnit) {
        textureBundle_t *bundle = &stage->bundle[textureUnit];
        void *generated = tess.generatedTexCoords[textureUnit];

        tess.activeTexCoords[textureUnit] = generated;

        switch (bundle->tcGen) {
        case TCGEN_BAD:
            return;

        case TCGEN_IDENTITY:
            memset(generated, 0,
                   (size_t)((uint32_t)tess.vertexCount *
                            (uint32_t)sizeof(vec2_t)));
            break;

        case TCGEN_LIGHTMAP:
            if (rbSetArraysOnce == qfalse && bundle->numTexMods == 0) {
                tess.activeTexCoords[textureUnit] =
                    tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET];
            } else {
                memcpy(generated,
                       tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET],
                       (size_t)((uint32_t)tess.vertexCount *
                                (uint32_t)sizeof(vec2_t)));
            }
            break;

        case TCGEN_TEXTURE:
            if (rbSetArraysOnce == qfalse && bundle->numTexMods == 0) {
                tess.activeTexCoords[textureUnit] =
                    tess.texCoords[R_TESS_BASE_TEXCOORD_SET];
            } else {
                memcpy(generated,
                       tess.texCoords[R_TESS_BASE_TEXCOORD_SET],
                       (size_t)((uint32_t)tess.vertexCount *
                                (uint32_t)sizeof(vec2_t)));
            }
            break;

        case TCGEN_ENVIRONMENT_MAPPED:
            RB_CalcEnvironmentTexCoords((float (*)[2])generated);
            break;

        case TCGEN_FOG:
            break;

        case TCGEN_VECTOR: {
            vec2_t *textureCoords = generated;

            for (int32_t vertexIndex = 0;
                 vertexIndex < tess.vertexCount; ++vertexIndex) {
                const uint32_t componentOffset =
                    (uint32_t)vertexIndex *
                    (uint32_t)tess.vertexComponentCount;
                const float *position = &tess.xyz[componentOffset];

                textureCoords[vertexIndex][0] =
                    position[0] * bundle->tcGenVectors[0][0] +
                    position[1] * bundle->tcGenVectors[0][1] +
                    position[2] * bundle->tcGenVectors[0][2];
                textureCoords[vertexIndex][1] =
                    position[0] * bundle->tcGenVectors[1][0] +
                    position[1] * bundle->tcGenVectors[1][1] +
                    position[2] * bundle->tcGenVectors[1][2];
            }
            break;
        }

        case TCGEN_NORMAL:
            if (bundle->numTexMods == 0) {
                tess.activeTexCoords[textureUnit] = tess.stageNormals;
            } else {
                memcpy(generated, tess.stageNormals,
                       (size_t)((uint32_t)tess.vertexCount *
                                (uint32_t)sizeof(vec3_t)));
            }
            break;

        case TCGEN_TANGENT:
            if (tess.stageTangentsValid == qfalse)
                RB_CalcTangentSpace();
            if (bundle->numTexMods == 0) {
                tess.activeTexCoords[textureUnit] = tess.stageTangents;
            } else {
                memcpy(generated, tess.stageTangents,
                       (size_t)((uint32_t)tess.vertexCount *
                                (uint32_t)sizeof(vec3_t)));
            }
            break;

        case TCGEN_BITANGENT:
            if (tess.stageBitangentsValid == qfalse)
                RB_CalcTangentSpace();
            if (bundle->numTexMods == 0) {
                tess.activeTexCoords[textureUnit] = tess.stageBitangents;
            } else {
                memcpy(generated, tess.stageBitangents,
                       (size_t)((uint32_t)tess.vertexCount *
                                (uint32_t)sizeof(vec3_t)));
            }
            break;

        case TCGEN_TBN_S:
        case TCGEN_TBN_T:
        case TCGEN_TBN_R:
            RB_CalcCubeMapTbnTexCoords(
                (float (*)[3])generated,
                (int32_t)bundle->tcGen - (int32_t)TCGEN_TBN_S);
            break;

        case TCGEN_CUBEMAP_VERTEX_TO_EYE:
            RB_CalcCubeMapVertexToEyeTexCoords((float (*)[3])generated);
            break;

        case TCGEN_CUBEMAP_EYE_TO_VERTEX:
            RB_CalcCubeMapEyeToVertexTexCoords((float (*)[3])generated);
            break;

        case TCGEN_CUBEMAP_REFLECTION:
            RB_CalcCubeMapReflectionTexCoords((float (*)[3])generated);
            break;

        case TCGEN_CUBEMAP_LIGHT_VECTOR:
            RB_CalcCubeMapLightVectorTexCoords((float (*)[3])generated);
            break;

        case TCGEN_CUBEMAP_LIGHT_HALF_ANGLE:
            RB_CalcCubeMapLightHalfAngleTexCoords((float (*)[3])generated);
            break;

        case TCGEN_CUBEMAP_SUN_HALF_ANGLE:
            RB_CalcCubeMapSunHalfAngleTexCoords((float (*)[3])generated);
            break;

        case TCGEN_CUBEMAP_DOT3_REFLECT_S:
        case TCGEN_CUBEMAP_DOT3_REFLECT_T:
        case TCGEN_CUBEMAP_DOT3_REFLECT_R:
            RB_CalcCubeMapDot3ReflectTexCoords(
                (float (*)[4])generated,
                (int32_t)bundle->tcGen -
                    (int32_t)TCGEN_CUBEMAP_DOT3_REFLECT_S);
            break;
        }

        for (int32_t modifierIndex = 0;
             modifierIndex < bundle->numTexMods; ++modifierIndex) {
            const texModInfo_t *texMod = &bundle->texMods[modifierIndex];
            vec2_t *textureCoords =
                (vec2_t *)tess.activeTexCoords[textureUnit];

            switch (texMod->type) {
            case TMOD_NONE:
                modifierIndex = R_MAX_SHADER_TEXMODS;
                break;

            case TMOD_TRANSFORM:
                RB_CalcTransformTexCoords(texMod, textureCoords);
                break;

            case TMOD_TURBULENT:
                RB_CalcTurbulentTexCoords(&texMod->wave, textureCoords);
                break;

            case TMOD_SCROLL:
                RB_CalcScrollTexCoords(texMod->scroll, textureCoords);
                break;

            case TMOD_SCALE:
                RB_CalcScaleTexCoords(texMod->scale, textureCoords);
                break;

            case TMOD_STRETCH:
                RB_CalcStretchTexCoords(&texMod->wave, textureCoords);
                break;

            case TMOD_ROTATE:
                RB_CalcRotateTexCoords(texMod->rotateSpeed, textureCoords);
                break;

            case TMOD_ENTITY_TRANSLATE:
                RB_CalcScrollTexCoords(
                    backEnd.currentEntity->e.shaderTexCoord,
                    textureCoords);
                break;

            case TMOD_SWAP:
                RB_CalcSwapTexCoords(textureCoords);
                break;

            case TMOD_CUBEMAP_NEGATE:
                RB_CalcCubeMapNegateTexCoords((float (*)[3])textureCoords);
                break;

            case TMOD_CUBEMAP_BUMPMAP_FRAME:
                RB_CalcCubeMapBumpmapFrameTexCoords(
                    (float (*)[3])textureCoords);
                break;

            default:
                ri.Error(ERR_DROP,
                         "\x15" "ERROR: unknown texmod '%d' in shader '%s'\n",
                         texMod->type, tess.shader->name);
                break;
            }
        }
    }
}

typedef enum renderer_dlight_clip_bits_e {
    R_DLIGHT_CLIP_S_LOW  = 0x01,
    R_DLIGHT_CLIP_S_HIGH = 0x02,
    R_DLIGHT_CLIP_T_LOW  = 0x04,
    R_DLIGHT_CLIP_T_HIGH = 0x08,
    R_DLIGHT_CLIP_Z_HIGH = 0x10,
    R_DLIGHT_CLIP_Z_LOW  = 0x20
} renderer_dlight_clip_bits_t;

/* Source: CoDUOMP.exe 0x004eba10..0x004ebdbe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eba10_004ebdbf.mcode.
 * Name and source-level argument order: exact same-module Mac symbol
 * RB_BuildDlightArrays. The Windows body keeps one clip byte per possible
 * tessellation vertex on the stack, generates projected light coordinates
 * and colors, then copies only triangles that are not wholly outside one
 * common projection plane. */
int32_t RB_BuildDlightArrays(
    const renderer_light_t *light,
    vec2_t textureCoords[R_MAX_TESS_VERTICES],
    uint8_t colors[R_MAX_TESS_VERTICES][4],
    uint16_t filteredIndexes[R_MAX_TESS_INDEXES])
{
    uint8_t clipBits[R_MAX_TESS_VERTICES];
    const float inverseRadius = 1.0f / light->radius;
    const float scaledColor[3] = {
        light->color[0] * 255.0f,
        light->color[1] * 255.0f,
        light->color[2] * 255.0f
    };

    if (r_dlightQuality->integer != 0 &&
        tess.requiresVertexBasis != qfalse &&
        (tess.stageTangentsValid == qfalse ||
         tess.stageBitangentsValid == qfalse)) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            RB_MakeNormalVectors(tess.stageNormals[vertexIndex],
                                 tess.stageTangents[vertexIndex],
                                 tess.stageBitangents[vertexIndex]);
        }
        tess.stageTangentsValid = qtrue;
        tess.stageBitangentsValid = qtrue;
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount; ++vertexIndex) {
        const uint32_t componentOffset =
            (uint32_t)vertexIndex *
            (uint32_t)tess.vertexComponentCount;
        const float *position = &tess.xyz[componentOffset];
        const long double lightDeltaRaw[3] = {
            (long double)light->transformedPosition[0] -
                (long double)position[0],
            (long double)light->transformedPosition[1] -
                (long double)position[1],
            (long double)light->transformedPosition[2] -
                (long double)position[2]
        };
        long double projectedSRaw;
        long double projectedTRaw;
        float projectedDepth;
        uint8_t clip = 0;
        float intensity;

        if (r_dlightQuality->integer != 0 &&
            tess.requiresVertexBasis != qfalse) {
            const float lightDeltaX = (float)lightDeltaRaw[0];
            const float lightDeltaY = (float)lightDeltaRaw[1];
            const float lightDeltaZ = (float)lightDeltaRaw[2];

            /* 0x004ebb2a..0x004ebba1 rounds X/Y first. Z is stored without
             * popping, so only the tangent projection consumes retained Z;
             * the bitangent and normal projections reload rounded Z. */
            projectedSRaw =
                (lightDeltaRaw[2] *
                     (long double)tess.stageTangents[vertexIndex][2] +
                 (long double)lightDeltaX *
                     (long double)tess.stageTangents[vertexIndex][0]) +
                (long double)lightDeltaY *
                    (long double)tess.stageTangents[vertexIndex][1];
            projectedTRaw =
                ((long double)lightDeltaZ *
                     (long double)tess.stageBitangents[vertexIndex][2] +
                 (long double)lightDeltaY *
                     (long double)tess.stageBitangents[vertexIndex][1]) +
                (long double)lightDeltaX *
                    (long double)tess.stageBitangents[vertexIndex][0];
            projectedDepth = (float)(
                ((long double)lightDeltaY *
                     (long double)tess.stageNormals[vertexIndex][1] +
                 (long double)lightDeltaZ *
                     (long double)tess.stageNormals[vertexIndex][2]) +
                (long double)lightDeltaX *
                    (long double)tess.stageNormals[vertexIndex][0]);
        } else {
            /* 0x004ebba5..0x004ebbbe retains the X/Y subtractions through
             * texture projection and rounds only the stored depth. */
            projectedSRaw = lightDeltaRaw[0];
            projectedTRaw = lightDeltaRaw[1];
            projectedDepth = (float)lightDeltaRaw[2];
        }

        const long double textureSRaw =
            projectedSRaw * (long double)inverseRadius + 0.5L;
        const long double textureTRaw =
            projectedTRaw * (long double)inverseRadius + 0.5L;
        textureCoords[vertexIndex][0] = (float)textureSRaw;
        textureCoords[vertexIndex][1] = (float)textureTRaw;
        backEnd.pc.dlightVertexCount = (int32_t)(
            (uint32_t)backEnd.pc.dlightVertexCount + 1u);

        if (textureCoords[vertexIndex][0] < 0.0f)
            clip |= R_DLIGHT_CLIP_S_LOW;
        else if (textureCoords[vertexIndex][0] > 1.0f)
            clip |= R_DLIGHT_CLIP_S_HIGH;

        /* 0x004ebbe6 stores T without popping; both T clip comparisons use
         * that retained value, while S is reloaded from its float local. */
        if (textureTRaw < 0.0L)
            clip |= R_DLIGHT_CLIP_T_LOW;
        else if (textureTRaw > 1.0L)
            clip |= R_DLIGHT_CLIP_T_HIGH;

        if (projectedDepth > light->radius) {
            clip |= R_DLIGHT_CLIP_Z_HIGH;
            intensity = 0.0f;
        } else if (projectedDepth < -light->radius) {
            clip |= R_DLIGHT_CLIP_Z_LOW;
            intensity = 0.0f;
        } else {
            const float absoluteDepth = fabsf(projectedDepth);

            if (absoluteDepth < light->radius * 0.5f) {
                intensity = 1.0f;
            } else {
                intensity = (light->radius - absoluteDepth) *
                            inverseRadius * 2.0f;
            }
        }

        clipBits[vertexIndex] = clip;
        colors[vertexIndex][0] =
            (uint8_t)lrintf(scaledColor[0] * intensity);
        colors[vertexIndex][1] =
            (uint8_t)lrintf(scaledColor[1] * intensity);
        colors[vertexIndex][2] =
            (uint8_t)lrintf(scaledColor[2] * intensity);
        colors[vertexIndex][3] = 255;
    }

    int32_t filteredIndexCount = 0;
    for (int32_t index = 0; index < tess.indexCount; index += 3) {
        const uint16_t index0 = tess.indexes[index];
        const uint16_t index1 = tess.indexes[index + 1];
        const uint16_t index2 = tess.indexes[index + 2];

        if ((clipBits[index0] & clipBits[index1] & clipBits[index2]) != 0)
            continue;

        filteredIndexes[filteredIndexCount++] = index0;
        filteredIndexes[filteredIndexCount++] = index1;
        filteredIndexes[filteredIndexCount++] = index2;
    }

    return filteredIndexCount;
}

/* Source: CoDUOMP.exe 0x004ea180..0x004ea19a.
 * Name: exact same-module Mac symbol RB_SetSurfaceCountColor. */
void RB_SetSurfaceCountColor(int32_t indexCount)
{
    uint8_t color[4];

    RB_ChooseSurfaceCountColor(indexCount, color);
    qglColor4ubv(color);
}

/* Original renderer counters at CoDUOMP.exe 0x0387be9c and 0x0387bea0. No
 * executable range outside R_DrawStripElements reads either counter. */
static int32_t r_stripElementCount;
static int32_t r_stripBeginCount;

/* Source: CoDUOMP.exe 0x004ea270..0x004ea3a6.
 * Name: exact same-module Mac symbol R_DrawStripElements. The callback is the
 * active qglArrayElement dispatch entry supplied by R_DrawElements. */
static void R_DrawStripElements(int32_t indexCount, const uint16_t *indexes,
                                renderer_gl_array_element_func_t emitElement)
{
    uint16_t previousA;
    uint16_t previousB;
    uint16_t previousC;
    qboolean oddTriangle;

    r_stripBeginCount = (int32_t)(
        (uint32_t)r_stripBeginCount + 1u);
    if (indexCount <= 0)
        return;

    qglBegin(GL_TRIANGLE_STRIP);
    emitElement(indexes[0]);
    emitElement(indexes[1]);
    emitElement(indexes[2]);
    r_stripElementCount = (int32_t)(
        (uint32_t)r_stripElementCount + 3u);

    previousA = indexes[0];
    previousB = indexes[1];
    previousC = indexes[2];
    oddTriangle = qfalse;

    for (int32_t index = 3; index < indexCount; index += 3) {
        const uint16_t currentA = indexes[index];
        const uint16_t currentB = indexes[index + 1];
        const uint16_t currentC = indexes[index + 2];
        qboolean continuesStrip;

        if (oddTriangle) {
            continuesStrip =
                currentB == previousC && currentA == previousA;
        } else {
            continuesStrip =
                currentA == previousC && currentB == previousB;
        }

        if (continuesStrip) {
            emitElement(currentC);
            r_stripElementCount = (int32_t)(
                (uint32_t)r_stripElementCount + 1u);
            oddTriangle = oddTriangle ? qfalse : qtrue;
        } else {
            qglEnd();
            qglBegin(GL_TRIANGLE_STRIP);
            r_stripBeginCount = (int32_t)(
                (uint32_t)r_stripBeginCount + 1u);
            emitElement(currentA);
            emitElement(currentB);
            emitElement(currentC);
            r_stripElementCount = (int32_t)(
                (uint32_t)r_stripElementCount + 3u);
            oddTriangle = qfalse;
        }

        previousA = currentA;
        previousB = currentB;
        previousC = currentC;
    }

    qglEnd();
}

/* Source: CoDUOMP.exe 0x004ea3b0..0x004ea41f.
 * Name: exact same-module Mac symbol R_DrawElements. The Mac body preserves
 * the source-level call to GL_DrawElements that MSVC inlined in the Windows
 * direct-draw path; both binaries select the same automatic fallback. The
 * marked Apple Silicon branch below is a native-port adaptation; all stock
 * targets retain the proved selector. */
void R_DrawElements(int32_t indexCount, const uint16_t *indexes)
{
    renderer_primitive_mode_t primitiveMode =
        (renderer_primitive_mode_t)r_primitives->integer;

    if (primitiveMode == R_PRIMITIVES_AUTOMATIC) {
        primitiveMode = qglLockArraysEXT != NULL
                            ? R_PRIMITIVES_DRAW_ELEMENTS
                            : R_PRIMITIVES_ARRAY_ELEMENTS;
    }

    if (primitiveMode == R_PRIMITIVES_DRAW_ELEMENTS) {
        GL_DrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexes);
    } else if (primitiveMode == R_PRIMITIVES_ARRAY_ELEMENTS) {
        R_DrawStripElements(indexCount, indexes, qglArrayElement);
    }
}

/* Source: CoDUOMP.exe 0x004eb8f0..0x004eb9b2.
 * Name: exact same-module Mac symbol RB_BeginSurface. The validity words at
 * tess +0x28/+0x2c independently gate the recovered tangent and bitangent
 * arrays. */
void RB_BeginSurface(shader_t *shader, int32_t vertexComponentCount)
{
    tess.indexCount = 0;
    tess.vertexCount = 0;
    tess.dlightBits = 0;
    tess.shader = shader;
    tess.activeStages = shader->stages;
    tess.activeStageCount = shader->numUnfoggedPasses;
    tess.stageIterator = shader->optimalStageIteratorFunc;
    tess.entity = backEnd.currentEntity;
    tess.requiresVertexBasis =
        (shader->surfaceFlags & SHADER_SURFACE_REQUIRES_VERTEX_BASIS) != 0
            ? qtrue
            : qfalse;
    tess.stageTangentsValid = qfalse;
    tess.stageBitangentsValid = qfalse;
    tess.optimizedFirstVertex = 0;
    tess.optimizedVertexEnd = 0;
    tess.renderedIndexCount = 0;
    tess.renderedVertexCount = 0;
    tess.vertexComponentCount = vertexComponentCount;

    tess.shaderTime = backEnd.refdef.floatTime - shader->timeOffset;
    if (shader->clampTime != 0.0f &&
        tess.shaderTime >= shader->clampTime) {
        tess.shaderTime = shader->clampTime;
    }
}

/* Source: CoDUOMP.exe 0x004ea420..0x004ea4d7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ea420_004ea4d8.mcode.
 * Name: exact same-module Mac symbol RB_GetAnimatedImage. The two x87
 * products are retained until one binary32 store; FISTP then uses the active
 * floating-point rounding mode before the ten fixed-point fraction bits are
 * discarded, so lrintf preserves that conversion on native targets. */
image_t *RB_GetAnimatedImage(textureBundle_t *bundle,
                             int32_t textureUnit)
{
    if (bundle->isVideoMap != 0) {
        ri.CIN_RunCinematic(bundle->videoMapHandle);
        ri.CIN_UploadCinematic(bundle->videoMapHandle);
        return tr.scratchImages[bundle->videoMapHandle];
    }

    if (bundle->waterMap != NULL) {
        RB_UploadWaterTexture(bundle->waterMap, tr.refdef.time,
                              textureUnit);
        return bundle->image[0];
    }

    if (bundle->numImageAnimations <= 1)
        return bundle->image[0];

    const float scaledFrame = (float)(
        ((long double)tess.shaderTime * bundle->imageAnimationSpeed) *
        (long double)(float)RB_ANIMATION_FRAME_SCALE);
    int32_t imageIndex =
        (int32_t)lrintf(scaledFrame) >> RB_ANIMATION_FRAME_FRACTION_BITS;
    if (imageIndex < 0)
        imageIndex = 0;

    if (bundle->clampAnimation != 0) {
        if (imageIndex >= bundle->numImageAnimations)
            imageIndex = bundle->numImageAnimations - 1;
    } else {
        imageIndex %= bundle->numImageAnimations;
    }

    return bundle->image[imageIndex];
}

/* Source: CoDUOMP.exe 0x004ea4e0..0x004ea4f9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ea4e0_004ea4fa.mcode.
 * Name: exact same-module Mac symbol RB_BindAnimatedImage. */
void RB_BindAnimatedImage(textureBundle_t *bundle)
{
    image_t *image = RB_GetAnimatedImage(bundle, glState.currenttmu);

    GL_Bind(image);
}

/* Source: CoDUOMP.exe 0x004eb9c0..0x004eba00.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eb9c0_004eba01.mcode.
 * Name and argument order: exact same-module Mac symbol DrawMultitextured. */
void DrawMultitextured(shaderStage_t *stage, int32_t indexCount,
                       const uint16_t *indexes)
{
    GL_State(stage->stateBits);
    if (backEnd.viewParms.isPortal != qfalse)
        qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    RB_SetupMultitexture(stage, tess.activeTexCoords, 0);
    R_DrawElements(indexCount, indexes);
}

/* Source: CoDUOMP.exe 0x004ecc10..0x004ecd70.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ecc10_004ecd71.mcode.
 * Name and source-level parameter order: exact same-module Mac symbol
 * RB_SingleStageGeneric. A second populated image bundle selects the existing
 * DrawMultitextured source path; the single-bundle path installs only texture
 * unit zero and then applies the stage's optional extension programs. */
void RB_SingleStageGeneric(shaderStage_t *stage, int32_t indexCount,
                           const uint16_t *indexes)
{
    RB_ComputeColors(stage);
    RB_ComputeTexCoords(stage);

    if (rbSetArraysOnce == qfalse) {
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
            qglEnableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
        }
        qglColorPointer(4, GL_UNSIGNED_BYTE, 0,
                        tess.stageVertexColors);
    }

    if (stage->bundle[1].image[0] != NULL) {
        DrawMultitextured(stage, indexCount, indexes);
        return;
    }

    if (rbSetArraysOnce == qfalse) {
        qglTexCoordPointer(stage->bundle[0].texCoordComponentCount,
                           GL_FLOAT, 0, tess.activeTexCoords[0]);
    }

    image_t *image;
    if ((tess.shader->stages[0]->stateBits & GLS_LIGHTING) != 0 &&
        r_lightmap->integer != 0) {
        image = tr.whiteImage;
    } else {
        image = RB_GetAnimatedImage(&stage->bundle[0],
                                    glState.currenttmu);
    }

    GL_Bind(image);
    GL_State(stage->stateBits);

    if (stage->registerCombiners != NULL)
        RB_SetupRegisterCombiners(stage->registerCombiners);
    if (stage->vertexProgram != NULL)
        RB_SetupVertexProgram(stage->vertexProgram);
    GL_BindFragmentShaderATI(stage->fragmentShaderATI);

    R_DrawElements(indexCount, indexes);
}

/* Source: CoDUOMP.exe 0x004ecd80..0x004ece88.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ecd80_004ece89.mcode.
 * Name: exact same-module Mac symbol ProjectDlightTexture. The 393,216-entry
 * stack array is the original projected-light index scratch span; the stack
 * probe emitted ahead of the body is compiler support, not a source call.
 * The default projected-light arm reads tr.dlightShader at original address
 * 0x048850f4, then its first stage at shader +0x154. */
void ProjectDlightTexture(void)
{
    uint16_t filteredIndexes[R_MAX_TESS_INDEXES];

    if (backEnd.refdef.num_dlights == 0)
        return;

    RB_EndMultitexture();
    for (int32_t lightIndex = 0;
         lightIndex < backEnd.refdef.num_dlights;
         ++lightIndex) {
        if ((tess.dlightBits &
             (1u << (uint32_t)lightIndex)) == 0) {
            continue;
        }

        renderer_light_t *light = &backEnd.refdef.dlights[lightIndex];
        const int32_t filteredIndexCount = RB_BuildDlightArrays(
            light,
            tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET],
            (uint8_t (*)[4])tess.vertexColors,
            filteredIndexes);

        if (filteredIndexCount == 0)
            continue;

        if ((tess.shader->lightingFlags &
             SHADER_LIGHTING_PER_ENTITY) != 0) {
            backEnd.currentLight = light;
            for (int32_t stageIndex = 0;
                 stageIndex < R_MAX_SHADER_STAGES;
                 ++stageIndex) {
                shaderStage_t *stage = tess.activeStages[stageIndex];

                if (stage == NULL)
                    break;
                if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0) {
                    RB_SingleStageGeneric(stage, filteredIndexCount,
                                          filteredIndexes);
                }
            }
        } else if (tr.dlightShader->stages[0] != NULL) {
            RB_SingleStageGeneric(tr.dlightShader->stages[0],
                                  filteredIndexCount,
                                  filteredIndexes);
        }
    }
}

/* Source: CoDUOMP.exe 0x004ece90..0x004ecef3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ece90_004ecef4.mcode.
 * Name: exact same-module Mac symbol RB_IterateStagesGeneric. Per-light stages
 * are deferred to the light passes, while lightmap-debug mode stops after the
 * first stage that consumes a lightmap bundle. */
void RB_IterateStagesGeneric(void)
{
    RB_EndMultitexture();
    for (int32_t stageIndex = 0;
         stageIndex < R_MAX_SHADER_STAGES;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if (stage == NULL)
            return;
        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        RB_SingleStageGeneric(stage, tess.indexCount, tess.indexes);
        if (r_lightmap->integer != 0 &&
            (stage->bundle[0].isLightmap != 0 ||
             stage->bundle[1].isLightmap != 0)) {
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x004ecf00..0x004ed184.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ecf00_004ed185.mcode.
 * Name: exact same-module Mac symbol RB_StageIteratorGeneric. The one-stage
 * fixed-function path locks stable vertex storage once and points texture
 * unit zero at generatedTexCoords[0]; RB_ComputeTexCoords observes
 * rbSetArraysOnce and fills that storage rather than changing the pointer.
 * Projected dynamic lights use the original exact 5.0f sort threshold. */
void RB_StageIteratorGeneric(qboolean portalPass)
{
    const float projectedDlightSortLimit = 5.0f;

    if (tess.activeStageCount <= 0)
        return;
    if (portalPass != qfalse && tess.dlightBits == 0)
        return;

    RB_DeformTessGeometry();

    const qboolean usesNormalArray =
        (tess.shader->surfaceFlags &
         SHADER_SURFACE_REQUIRES_NORMAL_ARRAY) != 0
            ? qtrue
            : qfalse;
    if (r_logFile->integer != 0) {
        GLimp_LogComment(va("--- RB_StageIteratorGeneric( %s ) ---\n",
                            tess.shader->name));
    }

    RB_EndMultitexture();
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    const uint32_t lockedArrayState =
        GLS_CLIENT_VERTEX_ARRAY |
        (usesNormalArray != qfalse ? GLS_CLIENT_NORMAL_ARRAY : 0);
    const uint32_t stageArrayState =
        lockedArrayState |
        GLS_CLIENT_TEXCOORD0_ARRAY |
        GLS_CLIENT_COLOR_ARRAY;

    if (tess.activeStageCount == 1 &&
        tess.shader->stages[0]->bundle[1].textureEnvMode == 0) {
        rbSetArraysOnce = qtrue;
        GL_ClientState(stageArrayState);
        qglColorPointer(4, GL_UNSIGNED_BYTE, 0,
                        tess.stageVertexColors);
        qglTexCoordPointer(
            tess.shader->stages[0]->bundle[0].texCoordComponentCount,
            GL_FLOAT, 0, tess.generatedTexCoords[0]);
    } else {
        GL_ClientState(lockedArrayState);
    }

    if (usesNormalArray != qfalse)
        qglNormalPointer(GL_FLOAT, (int32_t)sizeof(vec3_t),
                         tess.stageNormals);
    qglVertexPointer(tess.vertexComponentCount, GL_FLOAT, 0, tess.xyz);

    if (qglLockArraysEXT != NULL)
        qglLockArraysEXT(0, tess.vertexCount);

    if (rbSetArraysOnce == qfalse)
        GL_ClientState(stageArrayState);

    if (portalPass == qfalse)
        RB_IterateStagesGeneric();

    if (tess.dlightBits != 0 &&
        tess.shader->sort <= projectedDlightSortLimit &&
        (tess.shader->surfaceParmFlags &
         SHADER_DLIGHT_PROJECTION_BLOCK_MASK) == 0) {
        ProjectDlightTexture();
        goto cleanup;
    }

    if ((tess.shader->lightingFlags &
         SHADER_LIGHTING_PER_ENTITY) == 0) {
        goto cleanup;
    }
    if (backEnd.currentEntity == NULL ||
        backEnd.currentEntity->lightCount == 0) {
        goto cleanup;
    }

    for (int32_t lightIndex = 0;
         lightIndex < backEnd.currentEntity->lightCount;
         ++lightIndex) {
        renderer_entity_light_t *entityLight =
            &backEnd.currentEntity->lights[lightIndex];
        backEnd.currentLight = entityLight->light;

        if (backEnd.currentLight->type == R_LIGHT_TYPE_DIFFUSE_SUN)
            continue;

        backEnd.currentLightScale = entityLight->scale;
        for (int32_t stageIndex = 0;
             stageIndex < R_MAX_SHADER_STAGES;
             ++stageIndex) {
            shaderStage_t *stage = tess.activeStages[stageIndex];

            if (stage == NULL)
                break;
            if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0) {
                RB_SingleStageGeneric(stage, tess.indexCount,
                                      tess.indexes);
            }
        }
    }
    backEnd.currentLight = NULL;

cleanup:
    if (qglUnlockArraysEXT != NULL)
        qglUnlockArraysEXT();
    rbSetArraysOnce = qfalse;
}

/* Source: CoDUOMP.exe 0x0051d000..0x0051d015 and exact same-module Mac symbol
 * RB_PickBufferOffsetATI. Evidence:
 * coduomp/mcode/CoDUOMP/FUN_0051d000_0051d016.mcode. The Windows compiler
 * also inlined this source helper at 0x0051d07b, 0x0051d0f0, 0x0051d1bb, and
 * 0x0051d24c inside RB_SingleStageGenericATI. Its signed comparison wraps the
 * next allocation to byte offset zero only when the allocation end would
 * exceed the object-buffer capacity. */
int32_t RB_PickBufferOffsetATI(int32_t *currentOffset, int32_t size,
                               int32_t capacity)
{
    int32_t offset = *currentOffset;
    const int32_t endOffset =
        (int32_t)((uint32_t)offset + (uint32_t)size);

    if (endOffset > capacity)
        offset = 0;
    *currentOffset =
        (int32_t)((uint32_t)offset + (uint32_t)size);
    return offset;
}

/* Source: CoDUOMP.exe 0x0051d020..0x0051d2fd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051d020_0051d2fe.mcode.
 * Name and the RB_PickBufferOffsetATI source boundary: exact same-module Mac
 * symbols. The Windows compiler inlined the offset picker for all four upload
 * classes while retaining the final current-offset store after GL_State. */
void RB_SingleStageGenericATI(shaderStage_t *stage, int32_t indexCount,
                              const uint16_t *indexes)
{
    const uint32_t objectBuffer =
        backEnd.dynamicBuffer.storage.atiObjectBuffer;
    const int32_t capacity = backEnd.dynamicBuffer.capacity;
    int32_t currentOffset = backEnd.dynamicBuffer.currentOffset;
    uint32_t texCoordOffsets[R_MAX_TEXTURE_UNITS] = {0};

    qglEnable(GL_ELEMENT_ARRAY_ATI);
    RB_ComputeTexCoords(stage);

    for (int32_t textureUnit = 0;
         textureUnit < glConfig.maxActiveTextures;
         ++textureUnit) {
        if ((stage->flags &
             (SHADER_STAGE_TEXCOORD_ARRAY0 << (uint32_t)textureUnit)) == 0) {
            continue;
        }

        const int32_t texCoordBytes =
            stage->bundle[textureUnit].texCoordComponentCount *
            tess.vertexCount * (int32_t)sizeof(float);
        const int32_t offset = RB_PickBufferOffsetATI(
            &currentOffset, texCoordBytes, capacity);

        texCoordOffsets[textureUnit] = (uint32_t)offset;
        qglUpdateObjectBufferATI(
            objectBuffer, (uint32_t)offset, texCoordBytes,
            tess.activeTexCoords[textureUnit], GL_PRESERVE_ATI);
    }

    RB_SetupMultitextureATI(stage, objectBuffer, texCoordOffsets, 0);

    if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) != 0) {
        const int32_t colorBytes =
            tess.vertexCount * (int32_t)sizeof(tess.stageVertexColors[0]);
        const int32_t colorOffset = RB_PickBufferOffsetATI(
            &currentOffset, colorBytes, capacity);

        RB_ComputeColors(stage);
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
            qglEnableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
        }
        qglUpdateObjectBufferATI(
            objectBuffer, (uint32_t)colorOffset, colorBytes,
            tess.stageVertexColors, GL_PRESERVE_ATI);
        qglArrayObjectATI(GL_COLOR_ARRAY, 4, GL_UNSIGNED_BYTE, 0,
                          objectBuffer, (uint32_t)colorOffset);
    } else {
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }

        if (stage->rgbGen == CGEN_LIGHTING_PRECALC) {
            qglColor4ubv(backEnd.currentEntity->e.shaderRGBA);
        } else {
            qglColor4ubv(stage->constantColor);
        }
    }

    if ((stage->flags & SHADER_STAGE_NORMAL_ARRAY) != 0) {
        const int32_t normalBytes =
            tess.vertexCount * (int32_t)sizeof(tess.stageNormals[0]);
        const int32_t normalOffset = RB_PickBufferOffsetATI(
            &currentOffset, normalBytes, capacity);

        qglUpdateObjectBufferATI(
            objectBuffer, (uint32_t)normalOffset, normalBytes,
            tess.stageNormals, GL_PRESERVE_ATI);
        if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) == 0) {
            qglEnableClientState(GL_NORMAL_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_NORMAL_ARRAY;
        }
        qglArrayObjectATI(GL_NORMAL_ARRAY, 3, GL_FLOAT, 0,
                          objectBuffer, (uint32_t)normalOffset);
    } else if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    const int32_t vertexBytes =
        tess.vertexComponentCount * tess.vertexCount *
        (int32_t)sizeof(float);
    const int32_t vertexOffset = RB_PickBufferOffsetATI(
        &currentOffset, vertexBytes, capacity);

    qglUpdateObjectBufferATI(objectBuffer, (uint32_t)vertexOffset,
                             vertexBytes, tess.xyz, GL_PRESERVE_ATI);
    if ((glState.clientStateBits & GLS_CLIENT_VERTEX_ARRAY) == 0) {
        qglEnableClientState(GL_VERTEX_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_VERTEX_ARRAY;
    }
    qglArrayObjectATI(GL_VERTEX_ARRAY, tess.vertexComponentCount,
                      GL_FLOAT, 0, objectBuffer,
                      (uint32_t)vertexOffset);

    GL_State(stage->stateBits);
    backEnd.dynamicBuffer.currentOffset = currentOffset;
    qglDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexes);
    qglDisable(GL_ELEMENT_ARRAY_ATI);
}

/* Source: CoDUOMP.exe 0x0051d810..0x0051e182.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051d810_0051e183.mcode.
 * Name and source-level calls to RB_GetBuffersNV, RB_SetupMultitexture,
 * GL_State, and GL_DrawElements: exact same-module Mac symbols. The four
 * common packed layouts retain the original fast paths; the fallback packs
 * every enabled texture-coordinate array plus optional color and normal
 * arrays into the same per-vertex record. All copies are byte-preserving,
 * matching the Windows MOV operations rather than performing float
 * conversions. */
void RB_SingleStageGenericNV(shaderStage_t *stage, int32_t indexCount,
                             const uint16_t *indexes)
{
    enum {
        RB_NV_TEXCOORD_COMPONENTS_2D = 2,
        RB_NV_VERTEX_COMPONENTS_3D = 3
    };

    const uint8_t *texCoordSources[R_MAX_TEXTURE_UNITS] = {NULL};
    int32_t texCoordComponentCounts[R_MAX_TEXTURE_UNITS] = {0};
    int32_t texCoordOffsets[R_MAX_TEXTURE_UNITS] = {0};
    const void *packedTexCoords[R_MAX_TEXTURE_UNITS] = {NULL};
    int32_t vertexStride = 0;
    int32_t colorOffset = 0;
    int32_t normalOffset = 0;

    RB_ComputeTexCoords(stage);

    for (int32_t textureUnit = 0;
         textureUnit < glConfig.maxActiveTextures;
         ++textureUnit) {
        const uint32_t arrayFlag =
            SHADER_STAGE_TEXCOORD_ARRAY0 << (uint32_t)textureUnit;

        if ((stage->flags & arrayFlag) == 0)
            continue;

        texCoordOffsets[textureUnit] = vertexStride;
        texCoordComponentCounts[textureUnit] =
            stage->bundle[textureUnit].texCoordComponentCount;
        texCoordSources[textureUnit] =
            (const uint8_t *)tess.activeTexCoords[textureUnit];
        vertexStride += texCoordComponentCounts[textureUnit] *
                        (int32_t)sizeof(float);
    }

    if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) != 0) {
        colorOffset = vertexStride;
        vertexStride += (int32_t)sizeof(tess.stageVertexColors[0]);
        RB_ComputeColors(stage);
    }

    if ((stage->flags & SHADER_STAGE_NORMAL_ARRAY) != 0) {
        normalOffset = vertexStride;
        vertexStride += (int32_t)sizeof(tess.stageNormals[0]);
    }

    const int32_t vertexOffset = vertexStride;
    vertexStride += tess.vertexComponentCount * (int32_t)sizeof(float);

    const int32_t packedBytes = tess.vertexCount * vertexStride;
    uint8_t *const packedVertices = RB_GetBuffersNV(packedBytes);

    for (int32_t textureUnit = 0;
         textureUnit < glConfig.maxActiveTextures;
         ++textureUnit) {
        packedTexCoords[textureUnit] =
            packedVertices + texCoordOffsets[textureUnit];
    }
    RB_SetupMultitexture(stage, packedTexCoords, vertexStride);

    if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) != 0) {
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
            qglEnableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
        }
        qglColorPointer(4, GL_UNSIGNED_BYTE, vertexStride,
                        packedVertices + colorOffset);
    } else {
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }
        if (stage->rgbGen == CGEN_LIGHTING_PRECALC)
            qglColor4ubv(backEnd.currentEntity->e.shaderRGBA);
        else
            qglColor4ubv(stage->constantColor);
    }

    if ((stage->flags & SHADER_STAGE_NORMAL_ARRAY) != 0) {
        if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) == 0) {
            qglEnableClientState(GL_NORMAL_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_NORMAL_ARRAY;
        }
        qglNormalPointer(GL_FLOAT, vertexStride,
                         packedVertices + normalOffset);
    } else if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    if ((glState.clientStateBits & GLS_CLIENT_VERTEX_ARRAY) == 0) {
        qglEnableClientState(GL_VERTEX_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_VERTEX_ARRAY;
    }
    qglVertexPointer(tess.vertexComponentCount, GL_FLOAT, vertexStride,
                     packedVertices + vertexOffset);

    const uint32_t arrayFlags =
        stage->flags & SHADER_STAGE_DYNAMIC_ARRAY_MASK;
    const uint32_t texture0Flag = SHADER_STAGE_TEXCOORD_ARRAY0;
    const uint32_t texture1Flag = SHADER_STAGE_TEXCOORD_ARRAY0 << 1;

    if (arrayFlags == (texture0Flag | SHADER_STAGE_NORMAL_ARRAY) &&
        texCoordComponentCounts[0] == RB_NV_TEXCOORD_COMPONENTS_2D &&
        tess.vertexComponentCount == RB_NV_VERTEX_COMPONENTS_3D) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;
            memcpy(destination + texCoordOffsets[0],
                   texCoordSources[0] +
                       vertexIndex * RB_NV_TEXCOORD_COMPONENTS_2D *
                           (int32_t)sizeof(float),
                   RB_NV_TEXCOORD_COMPONENTS_2D * sizeof(float));
            memcpy(destination + normalOffset,
                   tess.stageNormals[vertexIndex],
                   sizeof(tess.stageNormals[vertexIndex]));
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   RB_NV_VERTEX_COMPONENTS_3D * sizeof(float));
        }
    } else if (arrayFlags ==
                   (texture0Flag | SHADER_STAGE_COLOR_ARRAY) &&
               texCoordComponentCounts[0] ==
                   RB_NV_TEXCOORD_COMPONENTS_2D &&
               tess.vertexComponentCount == RB_NV_VERTEX_COMPONENTS_3D) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;
            memcpy(destination + texCoordOffsets[0],
                   texCoordSources[0] +
                       vertexIndex * RB_NV_TEXCOORD_COMPONENTS_2D *
                           (int32_t)sizeof(float),
                   RB_NV_TEXCOORD_COMPONENTS_2D * sizeof(float));
            memcpy(destination + colorOffset,
                   &tess.stageVertexColors[vertexIndex],
                   sizeof(tess.stageVertexColors[vertexIndex]));
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   RB_NV_VERTEX_COMPONENTS_3D * sizeof(float));
        }
    } else if (arrayFlags == texture0Flag &&
               texCoordComponentCounts[0] ==
                   RB_NV_TEXCOORD_COMPONENTS_2D &&
               tess.vertexComponentCount == RB_NV_VERTEX_COMPONENTS_3D) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;
            memcpy(destination + texCoordOffsets[0],
                   texCoordSources[0] +
                       vertexIndex * RB_NV_TEXCOORD_COMPONENTS_2D *
                           (int32_t)sizeof(float),
                   RB_NV_TEXCOORD_COMPONENTS_2D * sizeof(float));
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   RB_NV_VERTEX_COMPONENTS_3D * sizeof(float));
        }
    } else if (arrayFlags ==
                   (texture0Flag | texture1Flag |
                    SHADER_STAGE_COLOR_ARRAY) &&
               texCoordComponentCounts[0] ==
                   RB_NV_TEXCOORD_COMPONENTS_2D &&
               texCoordComponentCounts[1] ==
                   RB_NV_TEXCOORD_COMPONENTS_2D &&
               tess.vertexComponentCount == RB_NV_VERTEX_COMPONENTS_3D) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;
            for (int32_t textureUnit = 0; textureUnit < 2; ++textureUnit) {
                memcpy(destination + texCoordOffsets[textureUnit],
                       texCoordSources[textureUnit] +
                           vertexIndex * RB_NV_TEXCOORD_COMPONENTS_2D *
                               (int32_t)sizeof(float),
                       RB_NV_TEXCOORD_COMPONENTS_2D * sizeof(float));
            }
            memcpy(destination + colorOffset,
                   &tess.stageVertexColors[vertexIndex],
                   sizeof(tess.stageVertexColors[vertexIndex]));
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   RB_NV_VERTEX_COMPONENTS_3D * sizeof(float));
        }
    } else {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;

            /* The DLL fully unrolls this general-path texcoord loop over the fixed
             * R_MAX_TEXTURE_UNITS (8) units (per-unit flag tests 0x100..0x8000, gated
             * ONLY by stage->flags) and never reads glConfig.maxActiveTextures here, so
             * the bound is the compile-time constant 8, not the runtime
             * maxActiveTextures a prior pass used (which would skip a flagged unit
             * index >= maxActiveTextures). Same fixed-8 unroll in the NV and ARB
             * single-stage general paths. */
            for (int32_t textureUnit = 0;
                 textureUnit < R_MAX_TEXTURE_UNITS;
                 ++textureUnit) {
                const uint32_t arrayFlag =
                    SHADER_STAGE_TEXCOORD_ARRAY0 <<
                    (uint32_t)textureUnit;
                if ((stage->flags & arrayFlag) == 0)
                    continue;

                const int32_t componentBytes =
                    texCoordComponentCounts[textureUnit] *
                    (int32_t)sizeof(float);
                memcpy(destination + texCoordOffsets[textureUnit],
                       texCoordSources[textureUnit] +
                           vertexIndex * componentBytes,
                       (size_t)componentBytes);
            }

            if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) != 0) {
                memcpy(destination + colorOffset,
                       &tess.stageVertexColors[vertexIndex],
                       sizeof(tess.stageVertexColors[vertexIndex]));
            }
            if ((stage->flags & SHADER_STAGE_NORMAL_ARRAY) != 0) {
                memcpy(destination + normalOffset,
                       tess.stageNormals[vertexIndex],
                       sizeof(tess.stageNormals[vertexIndex]));
            }
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   (size_t)tess.vertexComponentCount * sizeof(float));
        }
    }

    const int32_t alignedBytes =
        (packedBytes + (R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1)) &
        ~(R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1);
    memset(packedVertices + packedBytes, 0,
           (size_t)(alignedBytes - packedBytes));

    GL_State(stage->stateBits);
    GL_DrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexes);
    RB_SetFenceNV();
}

/* Source: CoDUOMP.exe 0x0051eee0..0x0051eef5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051eee0_0051eef6.mcode.
 * Name and signature: exact same-module Mac symbol
 * RB_PickBufferOffsetARB. The Windows compiler also inlined this helper
 * throughout RB_SingleStageGenericARB2 at 0x0051ef8a..0x0051f167 and once
 * for the interleaved allocation in RB_SingleStageGenericARB at
 * 0x0051e556..0x0051e568. */
int32_t RB_PickBufferOffsetARB(int32_t *currentOffset, int32_t size,
                               int32_t capacity)
{
    int32_t offset = *currentOffset;
    /* Both original LEA/ADD results wrap before their signed uses. */
    int32_t nextOffset =
        (int32_t)((uint32_t)offset + (uint32_t)size);

    if (nextOffset > capacity)
        offset = 0;
    nextOffset = (int32_t)((uint32_t)offset + (uint32_t)size);
    *currentOffset = nextOffset;
    return offset;
}

/* Source: CoDUOMP.exe 0x0051ef00..0x0051f249.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051ef00_0051f24a.mcode.
 * Name and source calls to RB_PickBufferOffsetARB, RB_SetupMultitexture,
 * GL_State, and GL_DrawElements: exact same-module Mac symbols. This ARB2
 * path keeps source arrays separate and issues one BufferSubData upload for
 * each enabled array. Buffer offsets are passed through OpenGL's pointer
 * parameters only while GL_ARRAY_BUFFER_ARB is bound. */
void RB_SingleStageGenericARB2(shaderStage_t *stage, int32_t indexCount,
                               const uint16_t *indexes)
{
    enum {
        RB_ARB_UPLOAD_CAPACITY = R_MAX_TEXTURE_UNITS + 3
    };
    /* NOT_FROM_ORIGINAL_SOURCE: readable carrier for the offset, size, and
     * source values that Windows scalar-replaces into three stack arrays. */
    typedef struct rb_arb_upload_s {
        int32_t offset;
        int32_t size;
        const void *source;
    } rb_arb_upload_t;

    rb_arb_upload_t uploads[RB_ARB_UPLOAD_CAPACITY];
    const void *texCoordOffsets[R_MAX_TEXTURE_UNITS] = {NULL};
    int32_t uploadCount = 0;
    int32_t currentOffset;
    int32_t capacity;
    uint32_t buffer;


    if (backEnd.dynamicBuffer.storage.glBuffer != 0) {
        buffer = backEnd.dynamicBuffer.storage.glBuffer;
        capacity = backEnd.dynamicBuffer.capacity;
        currentOffset = backEnd.dynamicBuffer.currentOffset;
    } else {
        currentOffset = 0;
        capacity = INT32_MAX;
        /* Preserve the target x86 INC's 32-bit modulo behavior. */
        backEnd.dynamicBuffer.frameSerial = (int32_t)(
            (uint32_t)backEnd.dynamicBuffer.frameSerial + 1u);
        buffer = (uint32_t)backEnd.dynamicBuffer.frameSerial;
    }

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
    RB_ComputeTexCoords(stage);

    for (int32_t textureUnit = 0;
         textureUnit < glConfig.maxActiveTextures;
         ++textureUnit) {
        const uint32_t arrayFlag =
            SHADER_STAGE_TEXCOORD_ARRAY0 << (uint32_t)textureUnit;

        if ((stage->flags & arrayFlag) == 0)
            continue;

        rb_arb_upload_t *const upload = &uploads[uploadCount++];
        upload->size =
            stage->bundle[textureUnit].texCoordComponentCount *
            tess.vertexCount * (int32_t)sizeof(float);
        upload->offset = RB_PickBufferOffsetARB(
            &currentOffset, upload->size, capacity);
        upload->source = tess.activeTexCoords[textureUnit];
        texCoordOffsets[textureUnit] =
            (const void *)(uintptr_t)(uint32_t)upload->offset;
    }

    RB_SetupMultitexture(stage, texCoordOffsets, 0);

    if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) != 0) {
        rb_arb_upload_t *const upload = &uploads[uploadCount++];
        upload->size =
            tess.vertexCount * (int32_t)sizeof(tess.stageVertexColors[0]);
        upload->offset = RB_PickBufferOffsetARB(
            &currentOffset, upload->size, capacity);
        upload->source = tess.stageVertexColors;

        RB_ComputeColors(stage);
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
            qglEnableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
        }
        qglColorPointer(
            4, GL_UNSIGNED_BYTE, 0,
            (const void *)(uintptr_t)(uint32_t)upload->offset);
    } else {
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }
        if (stage->rgbGen == CGEN_LIGHTING_PRECALC)
            qglColor4ubv(backEnd.currentEntity->e.shaderRGBA);
        else
            qglColor4ubv(stage->constantColor);
    }

    if ((stage->flags & SHADER_STAGE_NORMAL_ARRAY) != 0) {
        rb_arb_upload_t *const upload = &uploads[uploadCount++];
        upload->size =
            tess.vertexCount * (int32_t)sizeof(tess.stageNormals[0]);
        upload->offset = RB_PickBufferOffsetARB(
            &currentOffset, upload->size, capacity);
        upload->source = tess.stageNormals;

        if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) == 0) {
            qglEnableClientState(GL_NORMAL_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_NORMAL_ARRAY;
        }
        qglNormalPointer(
            GL_FLOAT, 0,
            (const void *)(uintptr_t)(uint32_t)upload->offset);
    } else if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    rb_arb_upload_t *const vertexUpload = &uploads[uploadCount++];
    vertexUpload->size = tess.vertexComponentCount * tess.vertexCount *
                         (int32_t)sizeof(float);
    vertexUpload->offset = RB_PickBufferOffsetARB(
        &currentOffset, vertexUpload->size, capacity);
    vertexUpload->source = tess.xyz;

    if ((glState.clientStateBits & GLS_CLIENT_VERTEX_ARRAY) == 0) {
        qglEnableClientState(GL_VERTEX_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_VERTEX_ARRAY;
    }
    qglVertexPointer(
        tess.vertexComponentCount, GL_FLOAT, 0,
        (const void *)(uintptr_t)(uint32_t)vertexUpload->offset);

    if (backEnd.dynamicBuffer.storage.glBuffer == 0) {
        qglBufferDataARB(GL_ARRAY_BUFFER_ARB, (intptr_t)currentOffset,
                         NULL, GL_STREAM_DRAW_ARB);
    } else {
        backEnd.dynamicBuffer.currentOffset = currentOffset;
    }

    for (int32_t uploadIndex = 0;
         uploadIndex < uploadCount;
         ++uploadIndex) {
        const rb_arb_upload_t *const upload = &uploads[uploadIndex];
        qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB,
                            (intptr_t)upload->offset,
                            (intptr_t)upload->size,
                            upload->source);
    }

    GL_State(stage->stateBits);
    GL_DrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexes);
    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

/* Source: CoDUOMP.exe 0x0051e470..0x0051eedf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051e470_0051eee0.mcode.
 * Name and source calls to RB_PickBufferOffsetARB, RB_SetupMultitexture,
 * GL_State, and GL_DrawElements: exact same-module Mac symbols. A persistent
 * VBO receives one interleaved BufferSubData upload from temporary memory.
 * Stream mode instead tries to map a freshly orphaned buffer, falling back to
 * temporary memory and BufferData when mapping fails. The original suppresses
 * the draw if UnmapBufferARB reports failure. */
void RB_SingleStageGenericARB(shaderStage_t *stage, int32_t indexCount,
                              const uint16_t *indexes)
{
    enum {
        RB_ARB_TEXCOORD_COMPONENTS_2D = 2,
        RB_ARB_VERTEX_COMPONENTS_3D = 3
    };

    const uint8_t *texCoordSources[R_MAX_TEXTURE_UNITS] = {NULL};
    int32_t texCoordComponentCounts[R_MAX_TEXTURE_UNITS] = {0};
    int32_t texCoordOffsets[R_MAX_TEXTURE_UNITS] = {0};
    const void *packedTexCoords[R_MAX_TEXTURE_UNITS] = {NULL};
    int32_t vertexStride = 0;
    int32_t colorOffset = 0;
    int32_t normalOffset = 0;

    RB_ComputeTexCoords(stage);

    for (int32_t textureUnit = 0;
         textureUnit < glConfig.maxActiveTextures;
         ++textureUnit) {
        const uint32_t arrayFlag =
            SHADER_STAGE_TEXCOORD_ARRAY0 << (uint32_t)textureUnit;

        if ((stage->flags & arrayFlag) == 0)
            continue;

        texCoordOffsets[textureUnit] = vertexStride;
        texCoordComponentCounts[textureUnit] =
            stage->bundle[textureUnit].texCoordComponentCount;
        texCoordSources[textureUnit] =
            (const uint8_t *)tess.activeTexCoords[textureUnit];
        vertexStride += texCoordComponentCounts[textureUnit] *
                        (int32_t)sizeof(float);
    }

    if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) != 0) {
        colorOffset = vertexStride;
        vertexStride += (int32_t)sizeof(tess.stageVertexColors[0]);
        RB_ComputeColors(stage);
    }

    if ((stage->flags & SHADER_STAGE_NORMAL_ARRAY) != 0) {
        normalOffset = vertexStride;
        vertexStride += (int32_t)sizeof(tess.stageNormals[0]);
    }

    const int32_t vertexOffset = vertexStride;
    vertexStride += tess.vertexComponentCount * (int32_t)sizeof(float);
    const int32_t packedBytes = tess.vertexCount * vertexStride;

    const uint32_t configuredPersistentBuffer =
        backEnd.dynamicBuffer.storage.glBuffer;
    const uint32_t persistentBuffer = configuredPersistentBuffer;
    uint32_t activeBuffer;
    int32_t bufferOffset = 0;

    if (persistentBuffer != 0) {
        activeBuffer = persistentBuffer;
        int32_t currentOffset = backEnd.dynamicBuffer.currentOffset;
        bufferOffset = RB_PickBufferOffsetARB(
            &currentOffset, packedBytes, backEnd.dynamicBuffer.capacity);
    } else {
        /* Preserve the target x86 INC's 32-bit modulo behavior. */
        backEnd.dynamicBuffer.frameSerial = (int32_t)(
            (uint32_t)backEnd.dynamicBuffer.frameSerial + 1u);
        activeBuffer = (uint32_t)backEnd.dynamicBuffer.frameSerial;
    }
    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, activeBuffer);

    for (int32_t textureUnit = 0;
         textureUnit < glConfig.maxActiveTextures;
         ++textureUnit) {
        packedTexCoords[textureUnit] =
            (const void *)(uintptr_t)(uint32_t)(
                bufferOffset + texCoordOffsets[textureUnit]);
    }
    RB_SetupMultitexture(stage, packedTexCoords, vertexStride);

    if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) != 0) {
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) == 0) {
            qglEnableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_COLOR_ARRAY;
        }
        qglColorPointer(
            4, GL_UNSIGNED_BYTE, vertexStride,
            (const void *)(uintptr_t)(uint32_t)(bufferOffset + colorOffset));
    } else {
        if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
            qglDisableClientState(GL_COLOR_ARRAY);
            glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
        }
        if (stage->rgbGen == CGEN_LIGHTING_PRECALC)
            qglColor4ubv(backEnd.currentEntity->e.shaderRGBA);
        else
            qglColor4ubv(stage->constantColor);
    }

    if ((stage->flags & SHADER_STAGE_NORMAL_ARRAY) != 0) {
        if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) == 0) {
            qglEnableClientState(GL_NORMAL_ARRAY);
            glState.clientStateBits |= GLS_CLIENT_NORMAL_ARRAY;
        }
        qglNormalPointer(
            GL_FLOAT, vertexStride,
            (const void *)(uintptr_t)(uint32_t)(bufferOffset + normalOffset));
    } else if ((glState.clientStateBits & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        qglDisableClientState(GL_NORMAL_ARRAY);
        glState.clientStateBits &= ~GLS_CLIENT_NORMAL_ARRAY;
    }

    if ((glState.clientStateBits & GLS_CLIENT_VERTEX_ARRAY) == 0) {
        qglEnableClientState(GL_VERTEX_ARRAY);
        glState.clientStateBits |= GLS_CLIENT_VERTEX_ARRAY;
    }
    qglVertexPointer(
        tess.vertexComponentCount, GL_FLOAT, vertexStride,
        (const void *)(uintptr_t)(uint32_t)(bufferOffset + vertexOffset));

    uint8_t *packedVertices = NULL;
    uint8_t *temporaryVertices = NULL;
    if (persistentBuffer == 0) {
            qglBufferDataARB(GL_ARRAY_BUFFER_ARB, (intptr_t)packedBytes,
                             NULL, GL_STREAM_DRAW_ARB);
            packedVertices = qglMapBufferARB(GL_ARRAY_BUFFER_ARB,
                                             GL_WRITE_ONLY_ARB);
    }
    if (packedVertices == NULL) {
        temporaryVertices =
            ri.Hunk_AllocateTempMemory((size_t)packedBytes);
        packedVertices = temporaryVertices;
    }

    const uint32_t arrayFlags =
        stage->flags & SHADER_STAGE_DYNAMIC_ARRAY_MASK;
    const uint32_t texture0Flag = SHADER_STAGE_TEXCOORD_ARRAY0;
    const uint32_t texture1Flag = SHADER_STAGE_TEXCOORD_ARRAY0 << 1;

    if (arrayFlags == (texture0Flag | SHADER_STAGE_NORMAL_ARRAY) &&
        texCoordComponentCounts[0] == RB_ARB_TEXCOORD_COMPONENTS_2D &&
        tess.vertexComponentCount == RB_ARB_VERTEX_COMPONENTS_3D) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;
            memcpy(destination + texCoordOffsets[0],
                   texCoordSources[0] +
                       vertexIndex * RB_ARB_TEXCOORD_COMPONENTS_2D *
                           (int32_t)sizeof(float),
                   RB_ARB_TEXCOORD_COMPONENTS_2D * sizeof(float));
            memcpy(destination + normalOffset,
                   tess.stageNormals[vertexIndex],
                   sizeof(tess.stageNormals[vertexIndex]));
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   RB_ARB_VERTEX_COMPONENTS_3D * sizeof(float));
        }
    } else if (arrayFlags ==
                   (texture0Flag | SHADER_STAGE_COLOR_ARRAY) &&
               texCoordComponentCounts[0] ==
                   RB_ARB_TEXCOORD_COMPONENTS_2D &&
               tess.vertexComponentCount == RB_ARB_VERTEX_COMPONENTS_3D) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;
            memcpy(destination + texCoordOffsets[0],
                   texCoordSources[0] +
                       vertexIndex * RB_ARB_TEXCOORD_COMPONENTS_2D *
                           (int32_t)sizeof(float),
                   RB_ARB_TEXCOORD_COMPONENTS_2D * sizeof(float));
            memcpy(destination + colorOffset,
                   &tess.stageVertexColors[vertexIndex],
                   sizeof(tess.stageVertexColors[vertexIndex]));
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   RB_ARB_VERTEX_COMPONENTS_3D * sizeof(float));
        }
    } else if (arrayFlags == texture0Flag &&
               texCoordComponentCounts[0] ==
                   RB_ARB_TEXCOORD_COMPONENTS_2D &&
               tess.vertexComponentCount == RB_ARB_VERTEX_COMPONENTS_3D) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;
            memcpy(destination + texCoordOffsets[0],
                   texCoordSources[0] +
                       vertexIndex * RB_ARB_TEXCOORD_COMPONENTS_2D *
                           (int32_t)sizeof(float),
                   RB_ARB_TEXCOORD_COMPONENTS_2D * sizeof(float));
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   RB_ARB_VERTEX_COMPONENTS_3D * sizeof(float));
        }
    } else if (arrayFlags ==
                   (texture0Flag | texture1Flag |
                    SHADER_STAGE_COLOR_ARRAY) &&
               texCoordComponentCounts[0] ==
                   RB_ARB_TEXCOORD_COMPONENTS_2D &&
               texCoordComponentCounts[1] ==
                   RB_ARB_TEXCOORD_COMPONENTS_2D &&
               tess.vertexComponentCount == RB_ARB_VERTEX_COMPONENTS_3D) {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;
            for (int32_t textureUnit = 0; textureUnit < 2; ++textureUnit) {
                memcpy(destination + texCoordOffsets[textureUnit],
                       texCoordSources[textureUnit] +
                           vertexIndex * RB_ARB_TEXCOORD_COMPONENTS_2D *
                               (int32_t)sizeof(float),
                       RB_ARB_TEXCOORD_COMPONENTS_2D * sizeof(float));
            }
            memcpy(destination + colorOffset,
                   &tess.stageVertexColors[vertexIndex],
                   sizeof(tess.stageVertexColors[vertexIndex]));
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   RB_ARB_VERTEX_COMPONENTS_3D * sizeof(float));
        }
    } else {
        for (int32_t vertexIndex = 0;
             vertexIndex < tess.vertexCount; ++vertexIndex) {
            uint8_t *const destination =
                packedVertices + vertexIndex * vertexStride;

            /* The DLL fully unrolls this general-path texcoord loop over the fixed
             * R_MAX_TEXTURE_UNITS (8) units (per-unit flag tests 0x100..0x8000, gated
             * ONLY by stage->flags) and never reads glConfig.maxActiveTextures here, so
             * the bound is the compile-time constant 8, not the runtime
             * maxActiveTextures a prior pass used (which would skip a flagged unit
             * index >= maxActiveTextures). Same fixed-8 unroll in the NV and ARB
             * single-stage general paths. */
            for (int32_t textureUnit = 0;
                 textureUnit < R_MAX_TEXTURE_UNITS;
                 ++textureUnit) {
                const uint32_t arrayFlag =
                    SHADER_STAGE_TEXCOORD_ARRAY0 <<
                    (uint32_t)textureUnit;
                if ((stage->flags & arrayFlag) == 0)
                    continue;

                const int32_t componentBytes =
                    texCoordComponentCounts[textureUnit] *
                    (int32_t)sizeof(float);
                memcpy(destination + texCoordOffsets[textureUnit],
                       texCoordSources[textureUnit] +
                           vertexIndex * componentBytes,
                       (size_t)componentBytes);
            }

            if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) != 0) {
                memcpy(destination + colorOffset,
                       &tess.stageVertexColors[vertexIndex],
                       sizeof(tess.stageVertexColors[vertexIndex]));
            }
            if ((stage->flags & SHADER_STAGE_NORMAL_ARRAY) != 0) {
                memcpy(destination + normalOffset,
                       tess.stageNormals[vertexIndex],
                       sizeof(tess.stageNormals[vertexIndex]));
            }
            memcpy(destination + vertexOffset,
                   &tess.xyz[vertexIndex * tess.vertexComponentCount],
                   (size_t)tess.vertexComponentCount * sizeof(float));
        }
    }

    GL_State(stage->stateBits);

    if (persistentBuffer != 0) {
        qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB,
                            (intptr_t)bufferOffset,
                            (intptr_t)packedBytes,
                            packedVertices);
        backEnd.dynamicBuffer.currentOffset =
            (bufferOffset + packedBytes +
             (R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1)) &
            ~(R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1);
        ri.Hunk_FreeTempMemory(temporaryVertices);
    } else if (temporaryVertices == NULL) {
        if (qglUnmapBufferARB(GL_ARRAY_BUFFER_ARB) == 0) {
            qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
            return;
        }
    } else {
        qglBufferDataARB(GL_ARRAY_BUFFER_ARB, (intptr_t)packedBytes,
                         temporaryVertices, GL_STREAM_DRAW_ARB);
        ri.Hunk_FreeTempMemory(temporaryVertices);
    }

    GL_DrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexes);
    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

/* Source: CoDUOMP.exe 0x0051f250..0x0051f2ba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051f250_0051f2bb.mcode.
 * Name and source-level no-argument boundary: exact same-module Mac symbol
 * RB_IterateStagesGenericARB. Stream mode selects the mapped/interleaved path;
 * the persistent mode uses segmented BufferSubData uploads. */
void RB_IterateStagesGenericARB(void)
{
    for (int32_t stageIndex = 0;
         stageIndex < R_MAX_SHADER_STAGES;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if (stage == NULL)
            return;
        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        if (tr.vboStreamDraw != qfalse) {
            RB_SingleStageGenericARB(stage, tess.indexCount,
                                     tess.indexes);
        } else {
            RB_SingleStageGenericARB2(stage, tess.indexCount,
                                      tess.indexes);
        }

        if (r_lightmap->integer != 0 &&
            (stage->bundle[0].isLightmap != 0 ||
             stage->bundle[1].isLightmap != 0)) {
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x0051f2c0..0x0051f3f4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051f2c0_0051f3f5.mcode.
 * Name: exact same-module Mac symbol ProjectDlightTextureARB. The selector
 * between the two ARB upload paths is repeated for both per-light shader
 * stages and the dedicated tr.dlightShader stage read from original address
 * 0x048850f4. */
void ProjectDlightTextureARB(void)
{
    uint16_t filteredIndexes[R_MAX_TESS_INDEXES];

    if (backEnd.refdef.num_dlights == 0)
        return;

    for (int32_t lightIndex = 0;
         lightIndex < backEnd.refdef.num_dlights;
         ++lightIndex) {
        if ((tess.dlightBits &
             (1u << (uint32_t)lightIndex)) == 0) {
            continue;
        }

        renderer_light_t *light = &backEnd.refdef.dlights[lightIndex];
        const int32_t filteredIndexCount = RB_BuildDlightArrays(
            light,
            tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET],
            (uint8_t (*)[4])tess.vertexColors,
            filteredIndexes);

        if (filteredIndexCount == 0)
            continue;

        if ((tess.shader->lightingFlags &
             SHADER_LIGHTING_PER_ENTITY) != 0) {
            backEnd.currentLight = light;
            for (int32_t stageIndex = 0;
                 stageIndex < R_MAX_SHADER_STAGES;
                 ++stageIndex) {
                shaderStage_t *stage = tess.activeStages[stageIndex];

                if (stage == NULL)
                    break;
                if ((stage->flags & SHADER_STAGE_PER_LIGHT) == 0)
                    continue;

                if (tr.vboStreamDraw != qfalse) {
                    RB_SingleStageGenericARB(stage, filteredIndexCount,
                                             filteredIndexes);
                } else {
                    RB_SingleStageGenericARB2(stage, filteredIndexCount,
                                              filteredIndexes);
                }
            }
        } else if (tr.dlightShader->stages[0] != NULL) {
            if (tr.vboStreamDraw != qfalse) {
                RB_SingleStageGenericARB(tr.dlightShader->stages[0],
                                         filteredIndexCount,
                                         filteredIndexes);
            } else {
                RB_SingleStageGenericARB2(tr.dlightShader->stages[0],
                                          filteredIndexCount,
                                          filteredIndexes);
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x0051f400..0x0051f587.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051f400_0051f588.mcode.
 * Name: exact same-module Mac symbol RB_StageIteratorGenericARB. Its stage,
 * projected-light, and entity-light control flow is the ARB twin of the ATI
 * and NV iterators, with the original upload-path selector at every draw. */
void RB_StageIteratorGenericARB(qboolean portalPass)
{
    const float projectedDlightSortLimit = 5.0f;

    if (tess.activeStageCount <= 0)
        return;
    if (portalPass != qfalse && tess.dlightBits == 0)
        return;

    if (r_logFile->integer != 0) {
        GLimp_LogComment(va("--- RB_StageIteratorGenericARB( %s ) ---\n",
                            tess.shader->name));
    }
    RB_DeformTessGeometry();
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    if (portalPass == qfalse)
        RB_IterateStagesGenericARB();

    if (tess.dlightBits != 0 &&
        (tess.shader->sort <= projectedDlightSortLimit ||
         (tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) != 0)) {
        if ((tess.shader->surfaceParmFlags &
             SHADER_DLIGHT_PROJECTION_BLOCK_MASK) == 0) {
            ProjectDlightTextureARB();
            return;
        }
    }

    if ((tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) == 0)
        return;
    if (backEnd.currentEntity == NULL ||
        backEnd.currentEntity->lightCount <= 0) {
        return;
    }

    for (int32_t lightIndex = 0;
         lightIndex < backEnd.currentEntity->lightCount;
         ++lightIndex) {
        renderer_entity_light_t *entityLight =
            &backEnd.currentEntity->lights[lightIndex];
        backEnd.currentLight = entityLight->light;

        if (backEnd.currentLight->type == R_LIGHT_TYPE_DIFFUSE_SUN)
            continue;

        backEnd.currentLightScale = entityLight->scale;
        for (int32_t stageIndex = 0;
             stageIndex < R_MAX_SHADER_STAGES;
             ++stageIndex) {
            shaderStage_t *stage = tess.activeStages[stageIndex];

            if (stage == NULL)
                break;
            if ((stage->flags & SHADER_STAGE_PER_LIGHT) == 0)
                continue;

            if (tr.vboStreamDraw != qfalse) {
                RB_SingleStageGenericARB(stage, tess.indexCount,
                                         tess.indexes);
            } else {
                RB_SingleStageGenericARB2(stage, tess.indexCount,
                                          tess.indexes);
            }
        }
    }

    backEnd.currentLight = NULL;
}

/* Source: CoDUOMP.exe 0x0051d300..0x0051d353.
 * Name: exact same-module Mac symbol RB_IterateStagesGenericATI. The lightmap
 * debug mode draws only the first enabled stage whose first or second bundle
 * is a lightmap; otherwise all eight active-stage slots are considered. */
void RB_IterateStagesGenericATI(void)
{
    for (int32_t stageIndex = 0;
         stageIndex < R_MAX_SHADER_STAGES;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if (stage == NULL)
            return;
        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        RB_SingleStageGenericATI(stage, tess.indexCount, tess.indexes);
        if (r_lightmap->integer != 0 &&
            (stage->bundle[0].isLightmap != 0 ||
             stage->bundle[1].isLightmap != 0)) {
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x0051e190..0x0051e1e3.
 * Name: exact same-module Mac symbol RB_IterateStagesGenericNV. The control
 * flow is the original NV twin of RB_IterateStagesGenericATI. */
void RB_IterateStagesGenericNV(void)
{
    for (int32_t stageIndex = 0;
         stageIndex < R_MAX_SHADER_STAGES;
         ++stageIndex) {
        shaderStage_t *stage = tess.activeStages[stageIndex];

        if (stage == NULL)
            return;
        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;

        RB_SingleStageGenericNV(stage, tess.indexCount, tess.indexes);
        if (r_lightmap->integer != 0 &&
            (stage->bundle[0].isLightmap != 0 ||
             stage->bundle[1].isLightmap != 0)) {
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x0051d470..0x0051d5ce.
 * Name: exact same-module Mac symbol RB_StageIteratorGenericATI. The 5.0f
 * limit is the original shader-sort threshold for projected dynamic lights. */
void RB_StageIteratorGenericATI(qboolean portalPass)
{
    const float projectedDlightSortLimit = 5.0f;

    if (tess.activeStageCount <= 0)
        return;
    if (portalPass != qfalse && tess.dlightBits == 0)
        return;

    if (r_logFile->integer != 0) {
        GLimp_LogComment(va("--- RB_StageIteratorGenericATI( %s ) ---\n",
                            tess.shader->name));
    }
    RB_DeformTessGeometry();
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    if (portalPass == qfalse)
        RB_IterateStagesGenericATI();

    if (tess.dlightBits != 0 &&
        (tess.shader->sort <= projectedDlightSortLimit ||
         (tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) != 0)) {
        if ((tess.shader->surfaceParmFlags &
             SHADER_DLIGHT_PROJECTION_BLOCK_MASK) == 0) {
            ProjectDlightTextureATI();
            return;
        }
    }

    if ((tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) == 0)
        return;
    if (backEnd.currentEntity == NULL ||
        backEnd.currentEntity->lightCount <= 0) {
        return;
    }

    for (int32_t lightIndex = 0;
         lightIndex < backEnd.currentEntity->lightCount;
         ++lightIndex) {
        renderer_entity_light_t *entityLight =
            &backEnd.currentEntity->lights[lightIndex];
        backEnd.currentLight = entityLight->light;

        if (backEnd.currentLight->type == R_LIGHT_TYPE_DIFFUSE_SUN)
            continue;

        backEnd.currentLightScale = entityLight->scale;
        for (int32_t stageIndex = 0;
             stageIndex < R_MAX_SHADER_STAGES;
             ++stageIndex) {
            shaderStage_t *stage = tess.activeStages[stageIndex];

            if (stage == NULL)
                break;
            if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0) {
                RB_SingleStageGenericATI(stage, tess.indexCount,
                                         tess.indexes);
            }
        }
    }

    backEnd.currentLight = NULL;
}

/* Source: CoDUOMP.exe 0x0051e310..0x0051e46e.
 * Name: exact same-module Mac symbol RB_StageIteratorGenericNV. This is the
 * original NV backend twin of RB_StageIteratorGenericATI. */
void RB_StageIteratorGenericNV(qboolean portalPass)
{
    const float projectedDlightSortLimit = 5.0f;

    if (tess.activeStageCount <= 0)
        return;
    if (portalPass != qfalse && tess.dlightBits == 0)
        return;

    if (r_logFile->integer != 0) {
        GLimp_LogComment(va("--- RB_StageIteratorGenericNV( %s ) ---\n",
                            tess.shader->name));
    }
    RB_DeformTessGeometry();
    RB_SetIteratorFog();
    GL_Cull(tess.shader->cullType);

    if (portalPass == qfalse)
        RB_IterateStagesGenericNV();

    if (tess.dlightBits != 0 &&
        (tess.shader->sort <= projectedDlightSortLimit ||
         (tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) != 0)) {
        if ((tess.shader->surfaceParmFlags &
             SHADER_DLIGHT_PROJECTION_BLOCK_MASK) == 0) {
            ProjectDlightTextureNV();
            return;
        }
    }

    if ((tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) == 0)
        return;
    if (backEnd.currentEntity == NULL ||
        backEnd.currentEntity->lightCount <= 0) {
        return;
    }

    for (int32_t lightIndex = 0;
         lightIndex < backEnd.currentEntity->lightCount;
         ++lightIndex) {
        renderer_entity_light_t *entityLight =
            &backEnd.currentEntity->lights[lightIndex];
        backEnd.currentLight = entityLight->light;

        if (backEnd.currentLight->type == R_LIGHT_TYPE_DIFFUSE_SUN)
            continue;

        backEnd.currentLightScale = entityLight->scale;
        for (int32_t stageIndex = 0;
             stageIndex < R_MAX_SHADER_STAGES;
             ++stageIndex) {
            shaderStage_t *stage = tess.activeStages[stageIndex];

            if (stage == NULL)
                break;
            if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0) {
                RB_SingleStageGenericNV(stage, tess.indexCount,
                                        tess.indexes);
            }
        }
    }

    backEnd.currentLight = NULL;
}

/* Source: CoDUOMP.exe 0x0051d360..0x0051d463.
 * Name: exact same-module Mac symbol ProjectDlightTextureATI. The local index
 * array is the original 393,216-element scratch span; RB_BuildDlightArrays
 * fills only the returned prefix. The default arm reads tr.dlightShader from
 * original address 0x048850f4. */
void ProjectDlightTextureATI(void)
{
    uint16_t filteredIndexes[R_MAX_TESS_INDEXES];

    for (int32_t lightIndex = 0;
         lightIndex < backEnd.refdef.num_dlights;
         ++lightIndex) {
        if ((tess.dlightBits &
             (1u << ((uint32_t)lightIndex & 31U))) == 0) {
            continue;
        }

        renderer_light_t *light = &backEnd.refdef.dlights[lightIndex];
        const int32_t filteredIndexCount = RB_BuildDlightArrays(
            light,
            tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET],
            (uint8_t (*)[4])tess.vertexColors,
            filteredIndexes);

        if (filteredIndexCount == 0)
            continue;

        if ((tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) != 0) {
            backEnd.currentLight = light;
            for (int32_t stageIndex = 0;
                 stageIndex < R_MAX_SHADER_STAGES;
                 ++stageIndex) {
                shaderStage_t *stage = tess.activeStages[stageIndex];

                if (stage == NULL)
                    break;
                if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0) {
                    RB_SingleStageGenericATI(stage, filteredIndexCount,
                                             filteredIndexes);
                }
            }
        } else if (tr.dlightShader->stages[0] != NULL) {
            RB_SingleStageGenericATI(tr.dlightShader->stages[0],
                                     filteredIndexCount,
                                     filteredIndexes);
        }
    }
}

/* Source: CoDUOMP.exe 0x0051e1f0..0x0051e303.
 * Name: exact same-module Mac symbol ProjectDlightTextureNV. NV requires the
 * dynamic ring capacity in addition to a nonempty world-dlight list. Its
 * default arm reads tr.dlightShader from original address 0x048850f4. */
void ProjectDlightTextureNV(void)
{
    uint16_t filteredIndexes[R_MAX_TESS_INDEXES];

    if (backEnd.refdef.num_dlights == 0 ||
        backEnd.dynamicBuffer.capacity == 0) {
        return;
    }

    for (int32_t lightIndex = 0;
         lightIndex < backEnd.refdef.num_dlights;
         ++lightIndex) {
        if ((tess.dlightBits &
             (1u << ((uint32_t)lightIndex & 31U))) == 0) {
            continue;
        }

        renderer_light_t *light = &backEnd.refdef.dlights[lightIndex];
        const int32_t filteredIndexCount = RB_BuildDlightArrays(
            light,
            tess.texCoords[R_TESS_LIGHTMAP_TEXCOORD_SET],
            (uint8_t (*)[4])tess.vertexColors,
            filteredIndexes);

        if (filteredIndexCount == 0)
            continue;

        if ((tess.shader->lightingFlags & SHADER_LIGHTING_PER_ENTITY) != 0) {
            backEnd.currentLight = light;
            for (int32_t stageIndex = 0;
                 stageIndex < R_MAX_SHADER_STAGES;
                 ++stageIndex) {
                shaderStage_t *stage = tess.activeStages[stageIndex];

                if (stage == NULL)
                    break;
                if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0) {
                    RB_SingleStageGenericNV(stage, filteredIndexCount,
                                            filteredIndexes);
                }
            }
        } else if (tr.dlightShader->stages[0] != NULL) {
            RB_SingleStageGenericNV(tr.dlightShader->stages[0],
                                    filteredIndexCount,
                                    filteredIndexes);
        }
    }
}
