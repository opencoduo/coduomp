#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

#include "../client/debug_lines.h"

#include <math.h>
#include <string.h>

/* Source: CoDUOMP.exe 0x004c0910..0x004c0949.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0910_004c0949.mcode.
 * Name: same-module Mac symbol RB_Text_Paint.
 *
 * The standalone Windows body is unreferenced because
 * RB_ExecuteRenderCommands contains an inlined copy. */
const void *RB_Text_Paint(const text_paint_command_t *command)
{
    const int32_t textLength = RB_Text_PaintWithCursor(
        command->fontHandle, command->text,
        command->x, command->y, command->scale, &command->color,
        command->cursorPosition, command->cursorCharacter,
        command->fixedAdvance, command->textStyle);
    const size_t commandSize =
        offsetof(text_paint_command_t, text) +
        (size_t)textLength + 1u;
    const size_t alignedCommandSize =
        (commandSize + (sizeof(int32_t) - 1u)) &
        ~(sizeof(int32_t) - 1u);

    return (const uint8_t *)command + alignedCommandSize;
}


/* Source: CoDUOMP.exe 0x004c0950..0x004c09a1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0950_004c09a1.mcode.
 * Name: same-module Mac symbol RB_DrawSurfs.
 *
 * The two REP MOVSD blocks establish the original command layout: a complete
 * 0x188-byte refdef followed by a complete 0x260-byte viewParms record. */
const void *RB_DrawSurfs(const drawSurfsCommand_t *command)
{
    if (tess.indexCount != 0)
        RB_EndSurface();

    backEnd.refdef = command->refdef;
    backEnd.viewParms = command->viewParms;
    RB_RenderDrawSurfList(command->drawSurfs, command->numDrawSurfs);
    return command + 1;
}

/* Source: CoDUOMP.exe 0x004c09b0..0x004c09fa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c09b0_004c09fa.mcode.
 * Name: same-module Mac symbol RB_DrawBuffer. */
const void *RB_DrawBuffer(const drawBufferCommand_t *command)
{
    qglDrawBuffer(command->buffer);

    if (r_clear->integer != 0) {
        qglClearColor(tr.identityLight, 0.0f, tr.identityLight * 0.5f, 1.0f);
        qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }


    return command + 1;
}

/* Source: CoDUOMP.exe 0x004c0c80..0x004c0d08.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0c80_004c0d08.mcode.
 * Name: same-module Mac symbol RB_SaveScreen. */
const void *RB_SaveScreen(const save_screen_command_t *command)
{
    if (!backEnd.projection2D)
        RB_SetGL2D();
    if (tess.vertexCount != 0)
        RB_EndSurface();

    RB_EndMultitexture();
    GL_Bind(tr.screenImage);
    qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                         glConfig.vidWidth, glConfig.vidHeight);

    tr.screenImageSaveTime = backEnd.refdef.time;
    tr.screenImageSMax =
        (float)glConfig.vidWidth / (float)tr.screenImageWidth;
    tr.screenImageTMax =
        (float)glConfig.vidHeight / (float)tr.screenImageHeight;
    return command + 1;
}

/* Source: CoDUOMP.exe 0x004c0d10..0x004c0de6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0d10_004c0de6.mcode.
 * Name: same-module Mac symbol RB_BlendSavedScreen.
 *
 * The exponent, clamp, scale, and rounding bias retain the exact constants
 * loaded at 0x004c0d42, 0x004c0d55, 0x004c0d6a, and 0x004c0d82. */
const void *RB_BlendSavedScreen(
    const blend_saved_screen_command_t *command)
{
    const int32_t elapsed = backEnd.refdef.time - tr.screenImageSaveTime;

    if (!backEnd.projection2D)
        RB_SetGL2D();

    if (elapsed < command->duration) {
        const double blendExponent = 0.009999999776482582;
        const double roundingBias = 9.313225746154785e-10;
        /* 0x004c0d42..0x004c0d7a retains the pow result through the clamp
         * and multiplication, then performs the sole binary32 spill. */
        long double blendRaw = (long double)pow(
            (double)elapsed / (double)command->duration,
            blendExponent);
        renderer_rgba8_t color = {
            .components = {255, 255, 255, 0}
        };

        if (blendRaw > (long double)0.9900000095367432f)
            blendRaw = (long double)0.9900000095367432f;
        const float blend = (float)(
            blendRaw * (long double)255.0f);
        color.components[3] =
            (uint8_t)lrint((double)blend + roundingBias);

        RB_DrawStretchPic(tr.screenImageShader,
                          0.0f, 0.0f,
                          (float)glConfig.vidWidth,
                          (float)glConfig.vidHeight,
                          0.0f, tr.screenImageTMax,
                          tr.screenImageSMax, 0.0f,
                          &color);
    }

    return command + 1;
}

/* Source: CoDUOMP.exe 0x004c0a00..0x004c0c74.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0a00_004c0c74.mcode.
 * Name: same-module Mac symbol RB_ShowImages. */
void RB_ShowImages(void)
{
    const float columnScale = 0.03125f;
    const float rowScale = 0.0416666679084301f;
    const float imageSizeScale = 0.0009765625f;
    int32_t startTime;

    if (!backEnd.projection2D)
        RB_SetGL2D();

    qglClear(GL_COLOR_BUFFER_BIT);
    qglFinish();
    startTime = ri.Milliseconds();

    GL_Bind(tr.screenImage);
    qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                         glConfig.vidWidth, glConfig.vidHeight);

    for (int32_t imageIndex = 0; imageIndex < tr.imageCount; ++imageIndex) {
        image_t *image = tr.images[imageIndex];
        if (image->link.textureSheet != NULL)
            continue;

        tr.showImagesShader->stages[0]->bundle[0].image[0] = image;
        RB_BeginSurface(tr.showImagesShader, 3);

        /* 0x004c0aad begins these dimensions only after the sheet-image
         * rejection and RB_BeginSurface side effects above. */
        long double widthRaw =
            (long double)glConfig.vidWidth *
            (long double)columnScale;
        long double heightRaw =
            (long double)glConfig.vidHeight *
            (long double)rowScale;
        const int32_t column = imageIndex % 32;
        const int32_t row = imageIndex / 32;
        const float x = (float)(
            (long double)column * widthRaw);
        const long double yRaw =
            (long double)row * heightRaw;
        const float y = (float)yRaw;

        if (r_showImages->integer == 2) {
            widthRaw =
                (widthRaw * (long double)image->uploadWidth) *
                (long double)imageSizeScale;
            heightRaw =
                (heightRaw * (long double)image->uploadHeight) *
                (long double)imageSizeScale;
        }

        /* 0x4c0b3d..0x4c0bfb keeps Y, width, and height on the x87 stack;
         * X was explicitly rounded before the right-edge addition. */
        const float right = (float)(
            (long double)x + widthRaw);
        const float bottom = (float)(
            yRaw + heightRaw);

        tess.xyz[0] = x;
        tess.xyz[1] = y;
        tess.xyz[2] = 0.0f;
        tess.xyz[3] = right;
        tess.xyz[4] = y;
        tess.xyz[5] = 0.0f;
        tess.xyz[6] = right;
        tess.xyz[7] = bottom;
        tess.xyz[8] = 0.0f;
        tess.xyz[9] = x;
        tess.xyz[10] = bottom;
        tess.xyz[11] = 0.0f;

        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][0][0] = 0.0f;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][0][1] = 0.0f;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][1][0] = 1.0f;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][1][1] = 0.0f;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][2][0] = 1.0f;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][2][1] = 1.0f;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][3][0] = 0.0f;
        tess.texCoords[R_TESS_BASE_TEXCOORD_SET][3][1] = 1.0f;

        tess.indexes[0] = 0;
        tess.indexes[1] = 1;
        tess.indexes[2] = 3;
        tess.indexes[3] = 3;
        tess.indexes[4] = 1;
        tess.indexes[5] = 2;
        tess.indexCount = 6;
        tess.vertexCount = 4;
        RB_EndSurface();
    }

    qglFinish();
    ri.Printf(R_PRINT_ALL, "%i msec to draw all images\n",
              ri.Milliseconds() - startTime);
}

/* Source: CoDUOMP.exe 0x004c0df0..0x004c1109.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c0df0_004c1109.mcode.
 * Name and source-level helper boundaries: same-module Mac RB_SwapBuffers call
 * graph. MSVC inlines CL_UpdateDebugData, GL_Cull, RB_glColor3fv,
 * RB_gl{Begin,Vertex2i,End}, and RB_EndImmediateMode into the Windows body.
 * The maintained source restores those original boundaries while retaining
 * the exact Windows conditions, table values, and command result. */
const void *RB_SwapBuffers(const swapBuffersCommand_t *command)
{
    static const int32_t overdrawStencilReferences[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 20, 40, 80, 200
    };
    static const vec3_t overdrawColors[] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f},
        {1.0f, 0.699999988079071f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {1.0f, 0.30000001192092896f, 0.30000001192092896f},
        {1.0f, 0.5f, 0.5f},
        {1.0f, 0.0f, 0.699999988079071f},
        {1.0f, 0.0f, 1.0f},
        {0.5199999809265137f, 0.4099999964237213f,
         0.30000001192092896f},
        {1.0f, 1.0f, 1.0f},
        {0.5f, 0.5f, 0.5f}
    };

    if (tess.indexCount != 0)
        RB_EndSurface();

    CL_UpdateDebugData();
    RB_DrawDebug();
    CL_FlushDebugData(qfalse);

    if (r_showImages->integer != 0)
        RB_ShowImages();

    if (r_measureOverdraw->integer != 0) {
        if (r_measureOverdraw->integer >= 2) {
            const int32_t pixelCount = (int32_t)(
                (uint32_t)glConfig.vidWidth *
                (uint32_t)glConfig.vidHeight);
            uint8_t *stencilValues =
                ri.Hunk_AllocateTempMemory(
                    (size_t)(uint32_t)pixelCount);
            int32_t stencilSum = 0;

            qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight,
                          GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilValues);
            for (int32_t pixelIndex = 0;
                 pixelIndex < pixelCount;
                 ++pixelIndex) {
                stencilSum = (int32_t)(
                    (uint32_t)stencilSum +
                    (uint32_t)stencilValues[pixelIndex]);
            }
            backEnd.pc.overdrawSum = (float)(
                (long double)backEnd.pc.overdrawSum +
                (long double)stencilSum);
            ri.Hunk_FreeTempMemory(stencilValues);
        }

        if (r_measureOverdraw->integer != 2) {
            RB_SetGL2D();
            RB_BeginImmediateMode();
            GL_State(GLS_DEPTHTEST_DISABLE |
                     GLS_SRCBLEND_ONE |
                     GLS_DSTBLEND_ONE);
            GL_Bind(tr.dlightImage);
            GL_Cull(CT_TWO_SIDED);
            qglLoadIdentity();
            qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

            for (int32_t colorIndex =
                     (int32_t)(sizeof(overdrawColors) /
                               sizeof(overdrawColors[0])) - 1;
                 colorIndex >= 0;
                 --colorIndex) {
                qglStencilFunc(GL_GEQUAL,
                               overdrawStencilReferences[colorIndex],
                               UINT32_MAX);
                RB_glColor3fv(overdrawColors[colorIndex]);
                RB_glBegin(GL_POLYGON);
                RB_glVertex2i(0, 0);
                RB_glVertex2i(glConfig.vidWidth, 0);
                RB_glVertex2i(glConfig.vidWidth, glConfig.vidHeight);
                RB_glVertex2i(0, glConfig.vidHeight);
                RB_glEnd();
            }

            RB_EndImmediateMode();
        }
    }

    if (glState.finishCalled == qfalse)
        qglFinish();

    GLimp_LogComment(
        "***************** RB_SwapBuffers *****************\n\n\n");

    if (r_swapDelay->integer == 0) {
        GLimp_EndFrame();
        backEnd.projection2D = qfalse;
    } else {
        qglFlush();
        backEnd.projection2D = qfalse;
        backEnd.endFramePending = qtrue;
    }

    return command + 1;
}

/* Source: CoDUOMP.exe 0x004c1110..0x004c1274.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1110_004c1274.mcode.
 * Name: same-module Mac symbol RB_ExecuteRenderCommands.
 *
 * The Windows jump table at 0x004c1274 proves command ids 1 through 11 and
 * their handlers. An id outside that range terminates the command stream. */
void RB_ExecuteRenderCommands(const void *data)
{
    const uint8_t *commandBytes = data;
    const int32_t startTime = ri.Milliseconds();

    if (backEnd.endFramePending != qfalse) {
        backEnd.endFramePending = qfalse;
        GLimp_EndFrame();
    }

    backEnd.dynamicBuffer.freeBytes = backEnd.dynamicBuffer.capacity;
    backEnd.dynamicBuffer.allocationSequence = 0;
    backEnd.dynamicBuffer.reclaimSequence = 1;
    backEnd.dynamicBuffer.allocations[0].offset = -1;
    backEnd.dynamicBuffer.currentOffset = 0;
    backEnd.dynamicBuffer.frameSerial = tr.dynamicBufferFrameSerial;

    for (;;) {
        int32_t commandId;
        memcpy(&commandId, commandBytes, sizeof(commandId));

        switch ((renderer_command_id_t)commandId) {
        case RC_SET_COLOR:
            commandBytes = RB_SetColor(
                (const setColorCommand_t *)commandBytes);
            break;

        case RC_STRETCH_PIC: {
            const stretchPicCommand_t *command =
                (const stretchPicCommand_t *)commandBytes;

            RB_DrawStretchPic(command->shader,
                              command->x, command->y,
                              command->w, command->h,
                              command->s1, command->t1,
                              command->s2, command->t2,
                              &backEnd.color2D);
            commandBytes = (const uint8_t *)(command + 1);
            break;
        }

        case RC_STRETCH_PIC_GRADIENT:
            commandBytes = RB_StretchPicGradient(
                (const stretch_pic_gradient_command_t *)commandBytes);
            break;

        case RC_STRETCH_PIC_ROTATE:
            commandBytes = RB_StretchPicRotate(
                (const stretch_pic_rotate_command_t *)commandBytes);
            break;

        case RC_DRAW_QUAD_PIC:
            commandBytes = RB_DrawQuadPic(
                (const draw_quad_pic_command_t *)commandBytes);
            break;

        case RC_TEXT_PAINT_WITH_CURSOR:
            commandBytes = RB_Text_Paint(
                (const text_paint_command_t *)commandBytes);
            break;

        case RC_DRAW_SURFS:
            commandBytes = RB_DrawSurfs(
                (const drawSurfsCommand_t *)commandBytes);
            break;

        case RC_DRAW_BUFFER:
            commandBytes = RB_DrawBuffer(
                (const drawBufferCommand_t *)commandBytes);
            break;

        case RC_SAVE_SCREEN:
            commandBytes = RB_SaveScreen(
                (const save_screen_command_t *)commandBytes);
            break;

        case RC_BLEND_SAVED_SCREEN:
            commandBytes = RB_BlendSavedScreen(
                (const blend_saved_screen_command_t *)commandBytes);
            break;

        case RC_SWAP_BUFFERS:
            commandBytes = RB_SwapBuffers(
                (const swapBuffersCommand_t *)commandBytes);
            break;


        case RC_END_OF_LIST:
        default:
            backEnd.pc.commandMsec = ri.Milliseconds() - startTime;
            if (tr.dynamicBufferMaxFrameSerial <
                backEnd.dynamicBuffer.frameSerial) {
                tr.dynamicBufferMaxFrameSerial =
                    backEnd.dynamicBuffer.frameSerial;
            }
            return;
        }
    }
}
