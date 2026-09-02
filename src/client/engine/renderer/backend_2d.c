#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <string.h>

enum {
    RB_X86_SHIFT_COUNT_MASK = 31
};

/* NOT_FROM_ORIGINAL_SOURCE: computes the aspect-correct physical output
 * region for the stock 640x480 2D coordinate system. Native presentation
 * uses this safe area for fullscreen menus; classic presentation uses it
 * for the complete renderer. The stock source line omits this interface. */
void coduomp_get_presentation_viewport(int32_t *x, int32_t *y, int32_t *width, int32_t *height)
{
    *x = 0;
    *y = 0;
    *width = glConfig.vidWidth;
    *height = glConfig.vidHeight;

    if (*width <= 0 || *height <= 0) {
        return;
    }

    if ((int64_t)*width * 3 > (int64_t)*height * 4) {
        *width = (*height * 4 / 3) & ~1;
        *x = (glConfig.vidWidth - *width) / 2;
    } else if ((int64_t)*width * 3 < (int64_t)*height * 4) {
        *height = (*width * 3 / 4) & ~1;
        *y = (glConfig.vidHeight - *height) / 2;
    }
}

/* Source: CoDUOMP.exe 0x00590e9c..0x00590ebc (.rdata). These are the
 * RGBA byte sequences selected by inline color escapes ^0 through ^7. */
static const renderer_rgba8_t rendererBaseColorCodes[8] = {{.components = {0, 0, 0, 255}},     {.components = {255, 0, 0, 255}},
                                                           {.components = {0, 255, 0, 255}},   {.components = {255, 255, 0, 255}},
                                                           {.components = {0, 0, 255, 255}},   {.components = {0, 255, 255, 255}},
                                                           {.components = {255, 0, 255, 255}}, {.components = {255, 255, 255, 255}}};

/* Source: CoDUOMP.exe 0x004ea900..0x004ea96e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ea900_004ea96e.mcode.
 * Name: same-module Mac symbol RB_DisableTMU. */
void RB_DisableTMU(int32_t textureUnit)
{
    if (glState.currentTextureTargets[textureUnit] == 0)
        return;

    if (glState.currenttmu != textureUnit) {
        qglActiveTextureARB(GL_TEXTURE0_ARB + (uint32_t)textureUnit);
        glState.currenttmu = textureUnit;
    }

    qglDisable(glState.currentTextureTargets[textureUnit]);
    glState.currentTextureTargets[textureUnit] = 0;

    if (glState.textureShaderEnabled[textureUnit] != qfalse) {
        qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, 0);
        glState.textureShaderEnabled[textureUnit] = qfalse;
    }
}

/* Source: CoDUOMP.exe 0x004eb180..0x004eb251.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eb180_004eb251.mcode.
 * Name: same-module Mac symbol RB_EndMultitexture. */
void RB_EndMultitexture(void)
{
    for (int32_t textureUnit = 1; textureUnit < glConfig.maxActiveTextures; ++textureUnit) {
        /* D3 E6 at 0x004eb19c masks CL to five bits. */
        const uint32_t texcoordArrayBit = 1u << ((uint32_t)textureUnit & RB_X86_SHIFT_COUNT_MASK);

        if ((glState.clientStateBits & texcoordArrayBit) != 0) {
            if (glState.currentClientTmu != textureUnit) {
                qglClientActiveTextureARB(GL_TEXTURE0_ARB + (uint32_t)textureUnit);
                glState.currentClientTmu = textureUnit;
            }
            qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glState.clientStateBits &= ~texcoordArrayBit;
        }

        RB_DisableTMU(textureUnit);
    }

    if (glState.currenttmu != 0) {
        qglActiveTextureARB(GL_TEXTURE0_ARB);
        glState.currenttmu = 0;
    }
    if (glState.currentClientTmu != 0) {
        qglClientActiveTextureARB(GL_TEXTURE0_ARB);
        glState.currentClientTmu = 0;
    }
    GL_TexEnv(GL_MODULATE);
}

typedef struct coduomp_output_presentation_s {
    int32_t renderWidth;
    int32_t renderHeight;
    int32_t outputWidth;
    int32_t outputHeight;
    int32_t nativeWidth;
    int32_t nativeHeight;
    qboolean preserveSelectedAspect;
} coduomp_output_presentation_t;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: separates the resolution rendered by
 * the game from the hardware-native fullscreen surface that presents it. */
static coduomp_output_presentation_t coduompOutputPresentation;

/* NOT_FROM_ORIGINAL_SOURCE: records the drawable and selected render sizes.
 * A narrower selected fullscreen aspect is fitted inside the hardware aspect;
 * explicit classic mode remains an inner 4:3 composition and takes final
 * precedence over the selected aspect. */
void coduomp_configure_output_presentation_compat(int32_t renderWidth, int32_t renderHeight, int32_t outputWidth, int32_t outputHeight,
                                                  int32_t nativeWidth, int32_t nativeHeight, qboolean fullscreenOutput)
{
    coduompOutputPresentation.renderWidth = renderWidth;
    coduompOutputPresentation.renderHeight = renderHeight;
    coduompOutputPresentation.outputWidth = outputWidth;
    coduompOutputPresentation.outputHeight = outputHeight;
    coduompOutputPresentation.nativeWidth = nativeWidth;
    coduompOutputPresentation.nativeHeight = nativeHeight;
    coduompOutputPresentation.preserveSelectedAspect = fullscreenOutput != qfalse && renderWidth > 0 && renderHeight > 0 &&
                                                               nativeWidth > 0 && nativeHeight > 0 &&
                                                               (int64_t)renderWidth * nativeHeight < (int64_t)nativeWidth * renderHeight
                                                           ? qtrue
                                                           : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: returns the final presentation rectangle. The
 * renderer composites only when its selected render surface differs from the
 * drawable; black outside this rectangle supplies automatic fullscreen bars. */
qboolean coduomp_get_output_presentation_compat(int32_t *outputWidth, int32_t *outputHeight, int32_t *viewportX, int32_t *viewportY,
                                                int32_t *viewportWidth, int32_t *viewportHeight)
{
    const int32_t renderWidth = coduompOutputPresentation.renderWidth;
    const int32_t renderHeight = coduompOutputPresentation.renderHeight;

    *outputWidth = coduompOutputPresentation.outputWidth;
    *outputHeight = coduompOutputPresentation.outputHeight;
    *viewportX = 0;
    *viewportY = 0;
    *viewportWidth = *outputWidth;
    *viewportHeight = *outputHeight;

    if (renderWidth <= 0 || renderHeight <= 0 || *outputWidth <= 0 || *outputHeight <= 0) {
        return qfalse;
    }

    /* Explicit classic presentation has final precedence: its 4:3 inner
     * composition must never be distorted by a differently shaped output. */
    if (coduompOutputPresentation.preserveSelectedAspect != qfalse || (r_aspectMode != NULL && r_aspectMode->integer != 0)) {
        if ((int64_t)*outputWidth * renderHeight > (int64_t)*outputHeight * renderWidth) {
            *viewportWidth = (int32_t)((int64_t)*outputHeight * renderWidth / renderHeight);
            *viewportX = (*outputWidth - *viewportWidth) / 2;
        } else if ((int64_t)*outputWidth * renderHeight < (int64_t)*outputHeight * renderWidth) {
            *viewportHeight = (int32_t)((int64_t)*outputWidth * renderHeight / renderWidth);
            *viewportY = (*outputHeight - *viewportHeight) / 2;
        }
    }

    return renderWidth != *outputWidth || renderHeight != *outputHeight || *viewportX != 0 || *viewportY != 0 ||
                   *viewportWidth != *outputWidth || *viewportHeight != *outputHeight
               ? qtrue
               : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: backend copy of the presentation
 * scope recorded in the renderer command stream. It is distinct from the
 * frontend submission flag because renderer commands execute later. */
qboolean coduomp_backend_cgame_2d_compat_active;
/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: backend copy of the native-width
 * console scope recorded in the renderer command stream. */
qboolean coduomp_backend_console_2d_compat_active;
/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: backend copy of the fitted UI scope
 * recorded in the renderer command stream. */
qboolean coduomp_backend_ui_2d_compat_active;

/* NOT_FROM_ORIGINAL_SOURCE: select a viewport for the composition currently
 * executing in the backend. Native-aspect cgame owns the complete drawable;
 * classic cgame and fullscreen UI share one fitted 4:3 viewport. The console
 * is an explicit native-width overlay in either gameplay aspect policy. */
void coduomp_apply_2d_presentation_viewport(void)
{
    if (coduomp_backend_console_2d_compat_active == qfalse &&
        ((r_aspectMode != NULL && r_aspectMode->integer != 0) || coduomp_backend_ui_2d_compat_active != qfalse ||
         (coduomp_backend_cgame_2d_compat_active == qfalse && r_uifullscreen != NULL && r_uifullscreen->integer != 0))) {
        int32_t viewportX;
        int32_t viewportY;
        int32_t viewportWidth;
        int32_t viewportHeight;

        coduomp_get_presentation_viewport(&viewportX, &viewportY, &viewportWidth, &viewportHeight);
        qglViewport(viewportX, viewportY, viewportWidth, viewportHeight);
        qglScissor(viewportX, viewportY, viewportWidth, viewportHeight);
    } else {
        qglViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
        qglScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
    }
}

/* Source: CoDUOMP.exe 0x004bf220..0x004bf335.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bf220_004bf335.mcode.
 * Name: same-module Mac symbol RB_SetGL2D. */
void RB_SetGL2D(void)
{
    enum {
        GLS_2D = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA
    };

    backEnd.projection2D = qtrue;
    RB_EndMultitexture();

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): native gameplay 2D
     * shares the drawable with the Hor+ view, while complete fullscreen UI
     * uses its fitted viewport. The command-buffer presentation scope makes
     * that decision independent of later UI state. */
    coduomp_apply_2d_presentation_viewport();

    qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity();
    qglOrtho(0.0, (double)glConfig.vidWidth, (double)glConfig.vidHeight, 0.0, 0.0, 1.0);
    qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity();

    GL_State(GLS_2D);
    if ((glState.glStateBits & GLS_FOG) != 0) {
        qglDisable(GL_FOG);
        glState.glStateBits &= ~GLS_FOG;
    }
    if (glState.faceCulling != CT_TWO_SIDED) {
        qglDisable(GL_CULL_FACE);
        glState.faceCulling = CT_TWO_SIDED;
    }
    qglDisable(GL_CLIP_PLANE0);

    backEnd.refdef.time = ri.Milliseconds();
    /* Exact float32 at 0x005b9b44; semantically milliseconds / 1000. */
    backEnd.refdef.floatTime = (float)backEnd.refdef.time * 0.0010000000474974513f;
}

/* Source: CoDUOMP.exe 0x004bf930..0x004bf93d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bf930_004bf93d.mcode.
 * Name: same-module Mac symbol RB_SetColor. */
const void *RB_SetColor(const setColorCommand_t *command)
{
    backEnd.color2D = command->color;
    return command + 1;
}

/* Source: CoDUOMP.exe 0x004bf940..0x004bfb34.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bf940_004bfb34.mcode.
 * Name: same-module Mac symbol RB_DrawStretchPic. */
void RB_DrawStretchPic(shader_t *shader, float x, float y, float width, float height, float s1, float t1, float s2, float t2,
                       const renderer_rgba8_t *color)
{
    int32_t baseVertex;
    int32_t xyzOffset;
    uint32_t packedColor;

    memcpy(&packedColor, color->components, sizeof(packedColor));

    if (!backEnd.projection2D)
        RB_SetGL2D();

    if (shader != tess.shader) {
        if (tess.indexCount != 0)
            RB_EndSurface();

        backEnd.currentEntity = &backEnd.entity2D;
        if (tr.defaultStorageMode != glState.currentStorageMode) {
            if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE)
                RB_SelectStorageNV(tr.defaultStorageMode);
            else if (glConfig.vertexArrayObjectATIAvailable)
                RB_SelectStorageATI(tr.defaultStorageMode);
            glState.currentStorageMode = tr.defaultStorageMode;
        }
        RB_BeginSurface(shader, 3);
    }

    if (tess.vertexCount + 4 >= R_MAX_TESS_VERTICES || tess.indexCount + 6 >= R_MAX_TESS_INDEXES) {
        RB_CheckOverflow(4, 6);
    }

    baseVertex = tess.vertexCount;
    tess.indexes[tess.indexCount + 0] = (uint16_t)(baseVertex + 3);
    tess.indexes[tess.indexCount + 1] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 2] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 4] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 5] = (uint16_t)(baseVertex + 1);
    tess.indexCount += 6;
    tess.vertexCount += 4;

    tess.vertexColors[baseVertex + 0] = packedColor;
    tess.vertexColors[baseVertex + 1] = packedColor;
    tess.vertexColors[baseVertex + 2] = packedColor;
    tess.vertexColors[baseVertex + 3] = packedColor;

    /* 0x4bfa6a IMUL by tess.vertexComponentCount scales ONLY the base
     * (baseVertex*vcc); the four vertex.x stores then land at a fixed 3-float
     * stride (base+0/+12/+24/+36), not a re-multiply by vcc. Benign on this 2D
     * path (RB_BeginSurface(shader,3) forces vcc==3) but the faithful stride is 3.
     * A prior pass advanced by tess.vertexComponentCount between vertices. */
    /* The gradient sibling repeats the same fixed-stride pattern at
     * 0x4bfcac; neither path rescales every vertex index by vcc. */
    /* The first xyz destination at 0x4bfa6a is that single scaled base; the
     * remaining destinations are reached only by fixed +12-byte advances. */
    xyzOffset = baseVertex * tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = x;
    tess.xyz[xyzOffset + 1] = y;
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = x + width;
    tess.xyz[xyzOffset + 1] = y;
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = x + width;
    tess.xyz[xyzOffset + 1] = y + height;
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = x;
    tess.xyz[xyzOffset + 1] = y + height;
    tess.xyz[xyzOffset + 2] = 0.0f;

    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][0] = s1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][1] = t1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][0] = s2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][1] = t1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][0] = s2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][1] = t2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][0] = s1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][1] = t2;
}

/* Source: CoDUOMP.exe 0x004bfb40..0x004bfb76.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bfb40_004bfb76.mcode.
 * Name and command layout: same-module Mac symbol RB_StretchPic plus the
 * Windows render-command dispatcher at 0x004c1110. */
const void *RB_StretchPic(const stretchPicCommand_t *command)
{
    RB_DrawStretchPic(command->shader, command->x, command->y, command->w, command->h, command->s1, command->t1, command->s2, command->t2,
                      &backEnd.color2D);
    return command + 1;
}

/* Source: CoDUOMP.exe 0x004bfb80..0x004bfd80.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bfb80_004bfd80.mcode.
 * Name: same-module Mac symbol RB_StretchPicGradient. */
const void *RB_StretchPicGradient(const stretch_pic_gradient_command_t *command)
{
    int32_t baseVertex;
    int32_t xyzOffset;
    uint32_t packedColor2D;
    uint32_t packedGradientColor;

    memcpy(&packedColor2D, backEnd.color2D.components, sizeof(packedColor2D));
    memcpy(&packedGradientColor, command->gradientColor.components, sizeof(packedGradientColor));

    if (!backEnd.projection2D)
        RB_SetGL2D();

    if (command->shader != tess.shader) {
        if (tess.indexCount != 0)
            RB_EndSurface();

        backEnd.currentEntity = &backEnd.entity2D;
        if (tr.defaultStorageMode != glState.currentStorageMode) {
            if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE)
                RB_SelectStorageNV(tr.defaultStorageMode);
            else if (glConfig.vertexArrayObjectATIAvailable)
                RB_SelectStorageATI(tr.defaultStorageMode);
            glState.currentStorageMode = tr.defaultStorageMode;
        }
        RB_BeginSurface(command->shader, 3);
    }

    if (tess.vertexCount + 4 >= R_MAX_TESS_VERTICES || tess.indexCount + 6 >= R_MAX_TESS_INDEXES) {
        RB_CheckOverflow(4, 6);
    }

    baseVertex = tess.vertexCount;
    tess.indexes[tess.indexCount + 0] = (uint16_t)(baseVertex + 3);
    tess.indexes[tess.indexCount + 1] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 2] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 4] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 5] = (uint16_t)(baseVertex + 1);
    tess.indexCount += 6;
    tess.vertexCount += 4;

    tess.vertexColors[baseVertex + 0] = packedColor2D;
    tess.vertexColors[baseVertex + 1] = packedColor2D;
    tess.vertexColors[baseVertex + 2] = packedGradientColor;
    tess.vertexColors[baseVertex + 3] = packedGradientColor;

    /* 0x4bfcac scales the base by vcc once; the following vertices use the
     * machine's fixed three-float (+12-byte) stride. */
    xyzOffset = baseVertex * tess.vertexComponentCount;
    tess.xyz[xyzOffset + 0] = command->x;
    tess.xyz[xyzOffset + 1] = command->y;
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = command->x + command->width;
    tess.xyz[xyzOffset + 1] = command->y;
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = command->x + command->width;
    tess.xyz[xyzOffset + 1] = command->y + command->height;
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = command->x;
    tess.xyz[xyzOffset + 1] = command->y + command->height;
    tess.xyz[xyzOffset + 2] = 0.0f;

    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][0] = command->s1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][1] = command->t1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][0] = command->s2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][1] = command->t1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][0] = command->s2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][1] = command->t2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][0] = command->s1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][1] = command->t2;

    return command + 1;
}

/* Source: CoDUOMP.exe 0x004bfd80..0x004c0047.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bfd80_004c0047.mcode.
 * Name: same-module Mac symbol RB_StretchPicRotate. */
const void *RB_StretchPicRotate(const stretch_pic_rotate_command_t *command)
{
    float halfWidth;
    float halfHeight;
    float centerX;
    float centerY;
    float radians;
    float sine;
    float cosine;
    long double sineWidthRaw;
    long double cosineWidthRaw;
    float negativeSineHeight;
    float cosineHeight;
    int32_t baseVertex;
    int32_t xyzOffset;
    uint32_t packedColor;

    memcpy(&packedColor, backEnd.color2D.components, sizeof(packedColor));

    if (!backEnd.projection2D)
        RB_SetGL2D();

    if (command->shader != tess.shader) {
        if (tess.indexCount != 0)
            RB_EndSurface();

        backEnd.currentEntity = &backEnd.entity2D;
        if (tr.defaultStorageMode != glState.currentStorageMode) {
            if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE)
                RB_SelectStorageNV(tr.defaultStorageMode);
            else if (glConfig.vertexArrayObjectATIAvailable)
                RB_SelectStorageATI(tr.defaultStorageMode);
            glState.currentStorageMode = tr.defaultStorageMode;
        }
        RB_BeginSurface(command->shader, 3);
    }

    if (tess.vertexCount + 4 >= R_MAX_TESS_VERTICES || tess.indexCount + 6 >= R_MAX_TESS_INDEXES) {
        RB_CheckOverflow(4, 6);
    }

    baseVertex = tess.vertexCount;
    tess.indexes[tess.indexCount + 0] = (uint16_t)(baseVertex + 3);
    tess.indexes[tess.indexCount + 1] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 2] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 4] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 5] = (uint16_t)(baseVertex + 1);
    tess.indexCount += 6;
    tess.vertexCount += 4;

    tess.vertexColors[baseVertex + 0] = packedColor;
    tess.vertexColors[baseVertex + 1] = packedColor;
    tess.vertexColors[baseVertex + 2] = packedColor;
    tess.vertexColors[baseVertex + 3] = packedColor;

    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][0] = command->s1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 0][1] = command->t1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][0] = command->s2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 1][1] = command->t1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][0] = command->s2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 2][1] = command->t2;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][0] = command->s1;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + 3][1] = command->t2;

    halfWidth = command->width * 0.5f;
    halfHeight = command->height * 0.5f;
    centerX = command->x + halfWidth;
    centerY = command->y + halfHeight;
    /* Exact float32 constants at 0x005b9ca8 and 0x005b9da8. The original
     * performs both multiplications before FSINCOS. */
    radians = (command->angleDegrees * 3.1415927410125732f) * 0.0055555556900799274f;
    coduo_x87_sincosf(radians, &sine, &cosine);
    /* 0x4bff5e..0x4c0023 retains these products across all four vertex
     * constructions; only the left/lower intermediates are stored. */
    sineWidthRaw = (long double)sine * (long double)halfWidth;
    cosineWidthRaw = (long double)cosine * (long double)halfWidth;
    /* 0x4bff75..0x4bff92: the FCHS lands on sine*halfHeight; the height
     * products pair as {-sin*hh, +cos*hh}. A prior pass swapped the sine and
     * cosine roles across both axes, which rendered every quad at 90-angle
     * instead of angle (visible as 90-degree-rotated crosshair arm pieces
     * once those were drawn at legible sizes). x terms use cos*halfWidth and
     * sin*halfHeight; y terms use sin*halfWidth and cos*halfHeight. */
    negativeSineHeight = -(sine * halfHeight);
    cosineHeight = cosine * halfHeight;

    xyzOffset = baseVertex * tess.vertexComponentCount;
    const long double leftXRaw = (long double)centerX - cosineWidthRaw;
    const float leftX = (float)leftXRaw;
    const long double lowerYRaw = (long double)centerY - sineWidthRaw;
    const float lowerY = (float)lowerYRaw;
    tess.xyz[xyzOffset + 0] = (float)(leftXRaw - (long double)negativeSineHeight);
    tess.xyz[xyzOffset + 1] = (float)(lowerYRaw - (long double)cosineHeight);
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = (float)((cosineWidthRaw + (long double)centerX) - (long double)negativeSineHeight);
    tess.xyz[xyzOffset + 1] = (float)(((long double)centerY + sineWidthRaw) - (long double)cosineHeight);
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = (float)(((long double)negativeSineHeight + cosineWidthRaw) + (long double)centerX);
    tess.xyz[xyzOffset + 1] = (float)(((long double)cosineHeight + sineWidthRaw) + (long double)centerY);
    tess.xyz[xyzOffset + 2] = 0.0f;
    xyzOffset += 3;
    tess.xyz[xyzOffset + 0] = leftX + negativeSineHeight;
    tess.xyz[xyzOffset + 1] = lowerY + cosineHeight;
    tess.xyz[xyzOffset + 2] = 0.0f;

    return command + 1;
}

/* Source: CoDUOMP.exe 0x004c0050..0x004c0245.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0050_004c0245.mcode.
 * Name: same-module Mac symbol RB_DrawQuadPic. */
const void *RB_DrawQuadPic(const draw_quad_pic_command_t *command)
{
    int32_t baseVertex;
    int32_t xyzOffset;
    uint32_t packedColor;

    memcpy(&packedColor, backEnd.color2D.components, sizeof(packedColor));

    if (!backEnd.projection2D)
        RB_SetGL2D();

    if (command->shader != tess.shader) {
        if (tess.indexCount != 0)
            RB_EndSurface();

        backEnd.currentEntity = &backEnd.entity2D;
        if (tr.defaultStorageMode != glState.currentStorageMode) {
            if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE)
                RB_SelectStorageNV(tr.defaultStorageMode);
            else if (glConfig.vertexArrayObjectATIAvailable)
                RB_SelectStorageATI(tr.defaultStorageMode);
            glState.currentStorageMode = tr.defaultStorageMode;
        }
        RB_BeginSurface(command->shader, 3);
    }

    if (tess.vertexCount + 4 >= R_MAX_TESS_VERTICES || tess.indexCount + 6 >= R_MAX_TESS_INDEXES) {
        RB_CheckOverflow(4, 6);
    }

    baseVertex = tess.vertexCount;
    tess.indexes[tess.indexCount + 0] = (uint16_t)(baseVertex + 3);
    tess.indexes[tess.indexCount + 1] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 2] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 3] = (uint16_t)(baseVertex + 2);
    tess.indexes[tess.indexCount + 4] = (uint16_t)baseVertex;
    tess.indexes[tess.indexCount + 5] = (uint16_t)(baseVertex + 1);
    tess.indexCount += 6;
    tess.vertexCount += 4;

    /* 0x4c01c5 scales the base by vcc once. The loop then advances the xyz
     * destination by a fixed three floats, independent of vcc. */
    xyzOffset = baseVertex * tess.vertexComponentCount;
    for (int32_t vertex = 0; vertex < 4; ++vertex) {
        tess.vertexColors[baseVertex + vertex] = packedColor;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + vertex][0] = command->texCoords[vertex][0];
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][baseVertex + vertex][1] = command->texCoords[vertex][1];

        tess.xyz[xyzOffset + 0] = command->positions[vertex][0];
        tess.xyz[xyzOffset + 1] = command->positions[vertex][1];
        tess.xyz[xyzOffset + 2] = 0.0f;
        xyzOffset += 3;
    }

    return command + 1;
}

/* Source: CoDUOMP.exe 0x004c02f0..0x004c033c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c02f0_004c033c.mcode.
 * Name: same-module Mac symbol RB_LookupColor. */
void RB_LookupColor(uint8_t colorCode, renderer_rgba8_t *outColor)
{
    const uint8_t numericCode = (uint8_t)(colorCode - (uint8_t)'0');
    const uint8_t tableIndex = numericCode < 10 ? numericCode : 7;

    if (tableIndex < 8) {
        *outColor = rendererBaseColorCodes[tableIndex];
    } else if (colorCode == (uint8_t)'8') {
        *outColor = backEnd.colorCode8;
    } else if (colorCode == (uint8_t)'9') {
        *outColor = backEnd.colorCode9;
    } else {
        *outColor = (renderer_rgba8_t){{255, 255, 255, 255}};
    }
}

/* Source: CoDUOMP.exe 0x004c0340..0x004c0378.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0340_004c0378.mcode.
 * Name: same-module Mac symbol RB_UpdateColorInternal. */
void RB_UpdateColorInternal(const float color[4], uint8_t outColor[4])
{
    for (int32_t component = 0; component < 4; ++component) {
        const float scaled = color[component] * 255.0f;

        /* The MSVC x87 sequence stores the float product, reloads it, adds
         * this exact double bias, then uses FISTP under round-to-nearest. */
        outColor[component] = (uint8_t)lrint((double)scaled + 9.31322574615478515625e-10);
    }
}

/* Source: CoDUOMP.exe 0x004c0380..0x004c039c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0380_004c039d.mcode.
 * Name and argument roles: exact same-module Mac symbol RB_UpdateColor. */
void RB_UpdateColor(const float color8[4], const float color9[4])
{
    RB_UpdateColorInternal(color8, backEnd.colorCode8.components);
    RB_UpdateColorInternal(color9, backEnd.colorCode9.components);
}

/* Source: CoDUOMP.exe 0x004c0250..0x004c02ef.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0250_004c02ef.mcode.
 * Name: same-module Mac symbol RB_Text_PaintChar. The Windows compiler keeps
 * shaderHandle in EAX and the remaining arguments on the stack; the maintained
 * source expresses the same logical call with a normal C prototype. */
void RB_Text_PaintChar(int32_t shaderHandle, float x, float y, float width, float height, float scale, float s1, float t1, float s2,
                       float t2, const renderer_rgba8_t *color)
{
    shader_t *shader;

    width *= scale;
    height *= scale;
    ri.AdjustFrom640(&x, &y, &width, &height);

    if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
        /* 0x004c029d calls through refimport slot 0 (Printf), not slot 1
         * (Error); the paint continues with the default shader. */
        ri.Printf(R_PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", shaderHandle);
        shader = tr.defaultShader;
    } else {
        shader = tr.shaders[shaderHandle];
    }

    RB_DrawStretchPic(shader, x, y, width, height, s1, t1, s2, t2, color);
}

/* Source: CoDUOMP.exe 0x004c03a0..0x004c0900.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c03a0_004c0900.mcode.
 * Name: same-module Mac symbol RB_Text_PaintWithCursor. */
int32_t RB_Text_PaintWithCursor(int32_t fontHandle, const char *text, float x, float y, float scale, const renderer_rgba8_t *color,
                                int32_t cursorPosition, uint8_t cursorCharacter, float fixedAdvance, int32_t textStyle)
{
    enum renderer_text_style_e {
        R_TEXT_STYLE_SHADOWED = 3,
        R_TEXT_STYLE_SHADOWED_MORE = 6,
        R_CURSOR_BLINK_INTERVAL_MSEC = 256
    };
    fontInfo_t *font = ri.CL_GetFontInfo(fontHandle, scale);
    const float effectiveScale = scale * font->glyphScale;
    const renderer_rgba8_t originalColor = *color;
    const renderer_rgba8_t shadowColor = {.components = {0, 0, 0, originalColor.components[3]}};
    const int32_t textLength = (int32_t)strlen(text);
    const char *cursor = text;
    float asianScale = 1.0f;
    float asianTopOffset = 0.0f;
    float currentX = x;
    float currentY = y;
    renderer_rgba8_t currentColor = originalColor;
    int32_t characterIndex = 0;

    if (rendererMultibyteTextEnabled != qfalse) {
        asianScale = R_GetAsianScale(font, scale);
        asianTopOffset = ((float)rendererAsianGlyph.height * asianScale * scale - scale * font->lineHeight) * 0.125f;
    }

    while (*cursor != '\0' && characterIndex < textLength) {
        const uint8_t firstByte = (uint8_t)cursor[0];
        int32_t character = firstByte;

        if (rendererMultibyteTextEnabled != qfalse) {
            const uint8_t secondByte = (uint8_t)cursor[1];
            qboolean validPair = qfalse;

            switch (cl_language->integer) {
            case LANGUAGE_KOREAN:
                validPair = firstByte >= 0xb0 && firstByte <= 0xc8 && secondByte >= 0xa1 && secondByte <= 0xfe;
                break;
            case LANGUAGE_TAIWANESE:
                validPair = ((firstByte >= 0xa1 && firstByte <= 0xc6) || (firstByte >= 0xc9 && firstByte <= 0xf9)) &&
                            ((secondByte >= 0x40 && secondByte <= 0x7e) || (secondByte >= 0xa1 && secondByte <= 0xfe));
                break;
            case LANGUAGE_JAPANESE:
                validPair = ((firstByte >= 0x81 && firstByte <= 0x9f) || (firstByte >= 0xe0 && firstByte <= 0xef)) &&
                            ((secondByte >= 0x40 && secondByte <= 0x7e) || (secondByte >= 0x80 && secondByte <= 0xfc));
                break;
            case LANGUAGE_CHINESE:
                validPair = firstByte >= 0xa1 && firstByte <= 0xf7 && secondByte >= 0xa1 && secondByte <= 0xfe;
                break;
            default:
                break;
            }

            if (validPair != qfalse) {
                character = ((int32_t)firstByte << 8) | secondByte;
                cursor += 2;
            } else {
                ++cursor;
            }
        } else {
            ++cursor;
        }

        if (character == '^' && *cursor != '^' && *cursor >= '0' && *cursor <= '9') {
            const uint8_t colorCode = (uint8_t)*cursor++;

            if (colorCode == (uint8_t)'7') {
                currentColor = originalColor;
            } else {
                RB_LookupColor(colorCode, &currentColor);
                currentColor.components[3] = originalColor.components[3];
            }
            ++characterIndex;
            continue;
        }

        if (character == '\n') {
            currentX = x;
            currentY += scale * font->lineHeight;
            if (rendererMultibyteTextEnabled != qfalse)
                currentY += 4.0f;
            ++characterIndex;
            continue;
        }

        if (character == '\r') {
            currentX = x;
            ++characterIndex;
            continue;
        }

        glyphInfo_t *glyph = R_GetCharacterGlyph(character, font);
        float characterScale = effectiveScale;
        float topOffset;
        float leftOffset;

        if (character > 255)
            characterScale = asianScale * effectiveScale;

        topOffset = characterScale * glyph->top;
        leftOffset = characterScale * glyph->left;
        if (character > 255)
            topOffset += asianTopOffset;

        if (fixedAdvance != 0.0f) {
            leftOffset += (fixedAdvance - characterScale * glyph->xSkip) * 0.5f;
        }

        if (textStyle == R_TEXT_STYLE_SHADOWED || textStyle == R_TEXT_STYLE_SHADOWED_MORE) {
            const float shadowOffset = textStyle == R_TEXT_STYLE_SHADOWED ? 1.0f : 2.0f;

            RB_Text_PaintChar(glyph->glyph, currentX + leftOffset + shadowOffset, currentY - topOffset + shadowOffset,
                              (float)glyph->imageWidth, (float)glyph->imageHeight, characterScale, glyph->s, glyph->t, glyph->s2, glyph->t2,
                              &shadowColor);
        }

        RB_Text_PaintChar(glyph->glyph, currentX + leftOffset, currentY - topOffset, (float)glyph->imageWidth, (float)glyph->imageHeight,
                          characterScale, glyph->s, glyph->t, glyph->s2, glyph->t2, &currentColor);

        if (characterIndex == cursorPosition && ((Sys_Milliseconds() / R_CURSOR_BLINK_INTERVAL_MSEC) & 1u) != 0) {
            glyphInfo_t *cursorGlyph = &font->glyphs[cursorCharacter];

            RB_Text_PaintChar(cursorGlyph->glyph, currentX, currentY - characterScale * cursorGlyph->top, (float)cursorGlyph->imageWidth,
                              (float)cursorGlyph->imageHeight, characterScale, cursorGlyph->s, cursorGlyph->t, cursorGlyph->s2,
                              cursorGlyph->t2, &currentColor);
        }

        if (fixedAdvance != 0.0f)
            currentX += fixedAdvance;
        else
            currentX += characterScale * glyph->xSkip;
        ++characterIndex;
    }

    if (cursorPosition == textLength && ((Sys_Milliseconds() / R_CURSOR_BLINK_INTERVAL_MSEC) & 1u) != 0) {
        glyphInfo_t *cursorGlyph = &font->glyphs[cursorCharacter];

        RB_Text_PaintChar(cursorGlyph->glyph, currentX, currentY - effectiveScale * cursorGlyph->top, (float)cursorGlyph->imageWidth,
                          (float)cursorGlyph->imageHeight, effectiveScale, cursorGlyph->s, cursorGlyph->t, cursorGlyph->s2, cursorGlyph->t2,
                          &currentColor);
    }

    backEnd.color2D = (renderer_rgba8_t){{255, 255, 255, 255}};
    return textLength;
}
