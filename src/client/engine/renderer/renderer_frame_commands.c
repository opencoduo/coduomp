#include "backend.h"

#include "compat/coduo_fp_conversion.h"
#include "qcommon/q_string.h"
#include "gl_api.h"
#include "gl_state.h"
#include "renderer_api.h"
#include "renderer_cvars.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    R_COMMAND_BUFFER_USABLE_BYTES = 262136
};

static const float rendererBytesToMebibytes = 0.00000095367431640625f; /* 0x35800000, 1 / 1048576 */
static const float rendererAngleToU16 = 182.04444885253906f; /* 0x43360b61, 65536 / 360 */
static const float rendererU16ToAngle = 0.0054931640625f; /* 0x3bb40000, 360 / 65536 */

/* Source: CoDUOMP.exe 0x004f0030..0x004f0071.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f0030_004f0072.mcode.
 * Name: exact same-module Mac symbol R_GetCommandBuffer. The final eight
 * bytes of the 256 KiB allocation remain reserved for stream termination. */
void *R_GetCommandBuffer(int32_t byteCount)
{
    const uint32_t newCommandUsed = (uint32_t)rendererBackendData->commandUsed + (uint32_t)byteCount;

    if (newCommandUsed > R_COMMAND_BUFFER_USABLE_BYTES) {
        if ((uint32_t)byteCount > R_COMMAND_BUFFER_USABLE_BYTES) {
            ri.Error(ERR_FATAL,
                     "\x15"
                     "R_GetCommandBuffer: bad size %i",
                     byteCount);
        }
        return NULL;
    }

    void *const command = &rendererBackendData->commandBuffer[rendererBackendData->commandUsed];
    rendererBackendData->commandUsed = (int32_t)newCommandUsed;
    return command;
}

/* Source: CoDUOMP.exe 0x004f00f0..0x004f0171.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f00f0_004f0172.mcode.
 * Name and signature: exact same-module Mac symbol RE_SetColor and renderer
 * export slot 29. A NULL input selects the all-white color. */
void RE_SetColor(const float *rgba)
{
    setColorCommand_t *const command = R_GetCommandBuffer((int32_t)sizeof(*command));
    if (command == NULL)
        return;

    /* 0x004f011f publishes the command id before reading any color lane. */
    command->commandId = RC_SET_COLOR;
    if (rgba == NULL)
        rgba = colorWhite;

    for (int32_t component = 0; component < 4; ++component) {
        command->color.components[component] = coduo_fp_to_u8_extended((long double)rgba[component] * 255.0f);
    }
}

/* Source: CoDUOMP.exe 0x004f0180..0x004f021d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f0180_004f021e.mcode.
 * Name and signature: exact same-module Mac symbol RE_StretchPic and renderer
 * export slot 30. */
void RE_StretchPic(float x, float y, float width, float height, float s1, float t1, float s2, float t2, int32_t shaderHandle)
{
    stretchPicCommand_t *const command = R_GetCommandBuffer((int32_t)sizeof(*command));
    if (command == NULL)
        return;

    command->commandId = RC_STRETCH_PIC;
    if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
        ri.Printf(R_PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", shaderHandle);
        command->shader = tr.defaultShader;
    } else {
        command->shader = tr.shaders[shaderHandle];
    }

    command->x = x;
    command->y = y;
    command->w = width;
    command->h = height;
    command->s1 = s1;
    command->t1 = t1;
    command->s2 = s2;
    command->t2 = t2;
}

/* Source: CoDUOMP.exe 0x004f0220..0x004f031a.
 * Evidence: repaired record
 * coduomp/mcode/CoDUOMP/FUN_004f0220_004f031b.mcode.
 * Name and signature: exact same-module Mac symbol RE_StretchPicGradient and
 * renderer export slot 31. */
void RE_StretchPicGradient(float x, float y, float width, float height, float s1, float t1, float s2, float t2, int32_t shaderHandle,
                           const float *gradientColor, int32_t gradientType)
{
    stretch_pic_gradient_command_t *const command = R_GetCommandBuffer((int32_t)sizeof(*command));
    if (command == NULL)
        return;

    command->commandId = RC_STRETCH_PIC_GRADIENT;
    if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
        ri.Printf(R_PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", shaderHandle);
        command->shader = tr.defaultShader;
    } else {
        command->shader = tr.shaders[shaderHandle];
    }

    command->x = x;
    command->y = y;
    command->width = width;
    command->height = height;
    command->s1 = s1;
    command->t1 = t1;
    command->s2 = s2;
    command->t2 = t2;

    if (gradientColor == NULL)
        gradientColor = colorWhite;
    for (int32_t component = 0; component < 4; ++component) {
        command->gradientColor.components[component] = coduo_fp_to_u8_extended((long double)gradientColor[component] * 255.0f);
    }
    command->gradientType = gradientType;
}

/* Source: CoDUOMP.exe 0x004f0320..0x004f03f0.
 * Evidence: repaired record
 * coduomp/mcode/CoDUOMP/FUN_004f0320_004f03f1.mcode.
 * Name and signature: exact same-module Mac symbol RE_StretchPicRotate and
 * renderer export slot 32. The retained scaled angle passes through the
 * original _ftol2 low dword and uint16 mask before conversion back to
 * degrees. */
void RE_StretchPicRotate(float x, float y, float width, float height, float s1, float t1, float s2, float t2, float angleDegrees,
                         int32_t shaderHandle)
{
    stretch_pic_rotate_command_t *const command = R_GetCommandBuffer((int32_t)sizeof(*command));
    if (command == NULL)
        return;

    command->commandId = RC_STRETCH_PIC_ROTATE;
    if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
        ri.Printf(R_PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", shaderHandle);
        command->shader = tr.defaultShader;
    } else {
        command->shader = tr.shaders[shaderHandle];
    }

    command->x = x;
    command->y = y;
    command->width = width;
    command->height = height;
    command->s1 = s1;
    command->t1 = t1;
    command->s2 = s2;
    command->t2 = t2;
    const long double rawAngle = (long double)angleDegrees * rendererAngleToU16;
    const uint32_t angleBits = coduo_fp_to_u32_extended(rawAngle);
    const float angleUnits = (float)(angleBits & UINT16_MAX);
    command->angleDegrees = (float)((long double)angleUnits * rendererU16ToAngle);
}

/* Source: CoDUOMP.exe 0x004f0400..0x004f04cf.
 * Evidence: repaired record
 * coduomp/mcode/CoDUOMP/FUN_004f0400_004f04d0.mcode.
 * Name and signature: exact same-module Mac symbol RE_DrawQuadPic and
 * renderer export slot 33. */
void RE_DrawQuadPic(const vec2_t positions[4], const vec2_t texCoords[4], int32_t shaderHandle)
{
    draw_quad_pic_command_t *const command = R_GetCommandBuffer((int32_t)sizeof(*command));
    if (command == NULL)
        return;

    command->commandId = RC_DRAW_QUAD_PIC;
    if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
        ri.Printf(R_PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", shaderHandle);
        command->shader = tr.defaultShader;
    } else {
        command->shader = tr.shaders[shaderHandle];
    }
    /* 0x004f0468..0x004f04cb alternates one position and one texture
     * coordinate for each vertex rather than performing two bulk copies. */
    for (int32_t vertex = 0; vertex < 4; ++vertex) {
        command->positions[vertex][0] = positions[vertex][0];
        command->positions[vertex][1] = positions[vertex][1];
        command->texCoords[vertex][0] = texCoords[vertex][0];
        command->texCoords[vertex][1] = texCoords[vertex][1];
    }
}

/* Source: CoDUOMP.exe 0x004efcc0..0x004eff97.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004efcc0_004eff98.mcode.
 * Name: exact same-module Mac symbol R_PerformanceCounters. */
void R_PerformanceCounters(void)
{
    if (com_statmon->integer != 0)
        R_SumOfUsedImages(NULL, NULL, NULL);

    if (tr.frameStatistics != NULL) {
        renderer_frame_statistics_t *const statistics = tr.frameStatistics;
        statistics->indexCount = backEnd.pc.indexCount;
        statistics->drawnIndexCount = backEnd.pc.drawnIndexCount;
        statistics->vertexCount = backEnd.pc.vertexCount;
        statistics->drawCallCount = backEnd.pc.drawCallCount;
        R_SumOfUsedImages(&statistics->imageMemory, &statistics->lightmapMemory, &statistics->textureMemory);
        statistics->entityCount = rendererSceneFrameState.entityCount;
        statistics->overdrawRatio = backEnd.pc.overdrawSum / (float)(glConfig.vidWidth * glConfig.vidHeight);
    }

    switch (r_speeds->integer) {
    case 1: {
        int32_t imageMemory;
        int32_t lightmapMemory;
        int32_t textureMemory;
        R_SumOfUsedImages(&imageMemory, &lightmapMemory, &textureMemory);

        ri.Printf(R_PRINT_ALL,
                  "%i/%i shaders/surfs %i leafs %i verts %i/%i tris "
                  "%.2f/%.2f/%.2f MB %.2f dc\n",
                  backEnd.pc.shaderCount, backEnd.pc.surfaceCount, tr.pc.leafCount, backEnd.pc.vertexCount, backEnd.pc.indexCount / 3,
                  backEnd.pc.drawnIndexCount / 3, (double)((float)imageMemory * rendererBytesToMebibytes),
                  (double)((float)(imageMemory - lightmapMemory) * rendererBytesToMebibytes),
                  (double)((float)textureMemory * rendererBytesToMebibytes),
                  (double)(backEnd.pc.overdrawSum / (float)(glConfig.vidWidth * glConfig.vidHeight)));
        break;
    }

    case 2:
        ri.Printf(R_PRINT_ALL, "(patch) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n", tr.pc.patchSphereCullIn,
                  tr.pc.patchSphereCullClip, tr.pc.patchSphereCullOut, tr.pc.patchBoxCullIn, tr.pc.patchBoxCullClip, tr.pc.patchBoxCullOut);
        break;

    case 3:
        ri.Printf(R_PRINT_ALL, "viewcluster: %i\n", tr.viewCluster);
        break;

    case 4:
        if (backEnd.pc.dlightVertexCount != 0) {
            ri.Printf(R_PRINT_ALL, "dlight srf:%i  culled:%i  verts:%i  tris:%i\n", tr.pc.dlightSurfaceCount, tr.pc.dlightSurfaceCullCount,
                      backEnd.pc.dlightVertexCount, backEnd.pc.dlightIndexCount / 3);
        }
        break;

    case 6:
        ri.Printf(R_PRINT_ALL, "flare adds:%i tests:%i renders:%i\n", backEnd.pc.flareAddCount, backEnd.pc.flareTestCount,
                  backEnd.pc.flareRenderCount);
        break;

    default:
        break;
    }

    memset(&tr.pc, 0, sizeof(tr.pc));
    memset(&backEnd.pc, 0, sizeof(backEnd.pc));
}

/* Source: CoDUOMP.exe 0x004effa0..0x004effe8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004effa0_004effe9.mcode.
 * Name: exact same-module Mac symbol R_IssueRenderCommands. */
void R_IssueRenderCommands(qboolean runPerformanceCounters)
{
    const int32_t endCommand = RC_END_OF_LIST;
    memcpy(&rendererBackendData->commandBuffer[rendererBackendData->commandUsed], &endCommand, sizeof(endCommand));
    rendererBackendData->commandUsed = 0;

    if (runPerformanceCounters != qfalse)
        R_PerformanceCounters();

    if (r_skipBackEnd->integer == 0) {
        RB_ExecuteRenderCommands(rendererBackendData->commandBuffer);
    }
}

/* Source: CoDUOMP.exe 0x004efff0..0x004f002e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004efff0_004f002f.mcode.
 * Name: exact same-module Mac symbol R_SyncRenderThread. This non-SMP build
 * executes the pending command list immediately. */
void R_SyncRenderThread(void)
{
    if (tr.registered == qfalse)
        return;

    const int32_t endCommand = RC_END_OF_LIST;
    memcpy(&rendererBackendData->commandBuffer[rendererBackendData->commandUsed], &endCommand, sizeof(endCommand));
    rendererBackendData->commandUsed = 0;

    if (r_skipBackEnd->integer == 0) {
        RB_ExecuteRenderCommands(rendererBackendData->commandBuffer);
    }
}

/* Source: CoDUOMP.exe 0x004f04d0..0x004f08ad.
 * Evidence: repaired record
 * coduomp/mcode/CoDUOMP/FUN_004f04d0_004f08ae.mcode.
 * Name and signature: exact same-module Mac symbol RE_BeginFrame and renderer
 * export slot 36. The Mac body independently confirms the source-level
 * R_GetCommandBuffer call and the three incremental VBO refresh calls. */
void RE_BeginFrame(stereoFrame_t stereoFrame)
{
    if (tr.registered == qfalse)
        return;

    ++tr.frameCount;
    glState.finishCalled = qfalse;
    tr.frameSceneNum = 0;

    if (r_measureOverdraw->integer != 0) {
        if (glConfig.stencilBits < 4) {
            ri.Printf(R_PRINT_ALL, "Warning: not enough stencil bits to measure overdraw: %d\n", glConfig.stencilBits);
            ri.Cvar_Set("r_measureOverdraw", "0");
            r_measureOverdraw->modified = qfalse;
        } else if (cg_shadows->integer == 2) {
            ri.Printf(R_PRINT_ALL, "Warning: stencil shadows and overdraw measurement are "
                                   "mutually exclusive\n");
            ri.Cvar_Set("r_measureOverdraw", "0");
            r_measureOverdraw->modified = qfalse;
        } else {
            R_SyncRenderThread();
            qglEnable(GL_STENCIL_TEST);
            qglStencilMask(UINT32_MAX);
            qglClearStencil(0);
            qglStencilFunc(GL_ALWAYS, 0, UINT32_MAX);
            qglStencilOp(GL_KEEP, GL_INCR, GL_INCR);
        }
    } else if (r_measureOverdraw->modified != qfalse) {
        R_SyncRenderThread();
        qglDisable(GL_STENCIL_TEST);
    }
    r_measureOverdraw->modified = qfalse;

    if (r_textureMode->modified != qfalse) {
        R_SyncRenderThread();
        GL_TextureMode(r_textureMode->string);
        r_textureMode->modified = qfalse;
    }

    R_SetHwLightGlobals();

    if (qglPNTrianglesiATI != NULL) {
        if (r_ati_truform_tess->modified != qfalse) {
            r_ati_truform_tess->modified = qfalse;
            if ((float)glConfig.maxPNTrianglesTessellationLevel < r_ati_truform_tess->value) {
                ri.Cvar_Set("r_ati_truform_tess", va("%i", glConfig.maxPNTrianglesTessellationLevel));
            }
            qglPNTrianglesiATI(GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, r_ati_truform_tess->integer);
        }

        if (r_ati_truform_pointmode->modified != qfalse) {
            r_ati_truform_pointmode->modified = qfalse;
            if (Q_stricmp(r_ati_truform_pointmode->string, "LINEAR") == 0) {
                glConfig.pnTrianglesPointMode = GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI;
            } else if (Q_stricmp(r_ati_truform_pointmode->string, "CUBIC") == 0) {
                glConfig.pnTrianglesPointMode = GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI;
            } else {
                /* The original applies CUBIC for this frame even though it
                 * corrects the cvar to LINEAR for the next update. At
                 * 0x004f06c4 the current-frame mode is published before the
                 * cvar callback. */
                glConfig.pnTrianglesPointMode = GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI;
                ri.Cvar_Set("r_ati_truform_pointmode", "LINEAR");
            }
            qglPNTrianglesiATI(GL_PN_TRIANGLES_POINT_MODE_ATI, glConfig.pnTrianglesPointMode);
        }

        if (r_ati_truform_normalmode->modified != qfalse) {
            r_ati_truform_normalmode->modified = qfalse;
            if (Q_stricmp(r_ati_truform_normalmode->string, "LINEAR") == 0) {
                glConfig.pnTrianglesNormalMode = GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI;
            } else if (Q_stricmp(r_ati_truform_normalmode->string, "QUADRATIC") == 0) {
                glConfig.pnTrianglesNormalMode = GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI;
            } else {
                /* 0x004f074c publishes the fallback before Cvar_Set. */
                glConfig.pnTrianglesNormalMode = GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI;
                ri.Cvar_Set("r_ati_truform_normalmode", "LINEAR");
            }
            qglPNTrianglesiATI(GL_PN_TRIANGLES_NORMAL_MODE_ATI, glConfig.pnTrianglesNormalMode);
        }
    }

    if (glConfig.fogDistanceAvailable != qfalse && r_nv_fogdist_mode->modified != qfalse) {
        r_nv_fogdist_mode->modified = qfalse;
        R_SetNVFogMode();
    }

    if (r_gamma->modified != qfalse) {
        r_gamma->modified = qfalse;
        R_SyncRenderThread();
        R_SetColorMappings();
    }

    if (glConfig.vertexBufferObjectAvailable != qfalse && r_vbo_paranoia->integer != 0) {
        R_IncrementalRefreshOptimizedWorldSurfaces_ARB();
        R_IncrementalRefreshStaticModels_ARB(R_VBO_REFRESH_ALL);
        R_IncrementalRefreshXModels_ARB(R_VBO_REFRESH_ALL);
    }

    if (r_ignoreGLErrors->integer == 0) {
        R_SyncRenderThread();
        const uint32_t error = qglGetError();
        if (error != GL_NO_ERROR) {
            ri.Error(ERR_FATAL, "\x15RE_BeginFrame() - glGetError() failed (0x%x)!\n", error);
        }
    }
    drawBufferCommand_t *const command = R_GetCommandBuffer((int32_t)sizeof(*command));
    if (command == NULL)
        return;
    command->commandId = RC_DRAW_BUFFER;

    if (glConfig.stereoEnabled != qfalse) {
        if (stereoFrame == STEREO_LEFT) {
            command->buffer = GL_BACK_LEFT;
            return;
        }
        if (stereoFrame == STEREO_RIGHT) {
            command->buffer = GL_BACK_RIGHT;
            return;
        }

        ri.Error(ERR_FATAL, "\x15RE_BeginFrame: Stereo is enabled, but stereoFrame was %i", stereoFrame);
        return;
    }

    if (stereoFrame != STEREO_CENTER) {
        ri.Error(ERR_FATAL, "\x15RE_BeginFrame: Stereo is disabled, but stereoFrame was %i", stereoFrame);
    }

    command->buffer = Q_stricmp(r_drawBuffer->string, "GL_FRONT") == 0 ? GL_FRONT : GL_BACK;
}

/* Source: CoDUOMP.exe 0x004f08b0..0x004f0987.
 * Evidence: repaired record
 * coduomp/mcode/CoDUOMP/FUN_004f08b0_004f0988.mcode.
 * Name and signature: exact same-module Mac symbol RE_EndFrame and renderer
 * export slot 37. */
void RE_EndFrame(int32_t *frontEndMsec, int32_t *backEndMsec)
{
    if (tr.registered == qfalse)
        return;

    swapBuffersCommand_t *const command = (swapBuffersCommand_t *)&rendererBackendData->commandBuffer[rendererBackendData->commandUsed];
    command->commandId = RC_SWAP_BUFFERS;
    rendererBackendData->commandUsed += (int32_t)sizeof(*command);

    R_IssueRenderCommands(qtrue);
    R_ToggleSmpFrame();

    if (frontEndMsec != NULL)
        *frontEndMsec = tr.frontEndMsec;
    tr.frontEndMsec = 0;

    if (backEndMsec != NULL)
        *backEndMsec = backEnd.pc.commandMsec;
    backEnd.pc.commandMsec = 0;
}

/* Source: CoDUOMP.exe 0x004f0990..0x004f09bd.
 * Evidence: repaired record
 * coduomp/mcode/CoDUOMP/FUN_004f0990_004f09be.mcode.
 * Name: exact same-module Mac symbol RE_SaveScreen. */
void RE_SaveScreen(void)
{
    save_screen_command_t *const command = R_GetCommandBuffer((int32_t)sizeof(*command));
    if (command != NULL)
        command->commandId = RC_SAVE_SCREEN;
}

/* Source: CoDUOMP.exe 0x004f09c0..0x004f09f8.
 * Evidence: repaired record
 * coduomp/mcode/CoDUOMP/FUN_004f09c0_004f09f9.mcode.
 * Name and signature: exact same-module Mac symbol RE_BlendSavedScreen and
 * renderer export slot 39. */
void RE_BlendSavedScreen(int32_t duration)
{
    if (duration <= 0)
        return;

    blend_saved_screen_command_t *const command = R_GetCommandBuffer((int32_t)sizeof(*command));
    if (command == NULL)
        return;

    command->commandId = RC_BLEND_SAVED_SCREEN;
    command->duration = duration;
}

/* Source: CoDUOMP.exe 0x004f0a00..0x004f0a09.
 * Evidence: repaired record
 * coduomp/mcode/CoDUOMP/FUN_004f0a00_004f0a0a.mcode.
 * Name and signature: exact same-module Mac symbol RE_TrackStatistics and
 * renderer export slot 42. */
void RE_TrackStatistics(renderer_frame_statistics_t *statistics)
{
    tr.frameStatistics = statistics;
}
