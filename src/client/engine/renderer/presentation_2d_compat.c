#include "backend.h"


#include "renderer_api.h"
#include "../client/widescreen_2d_compat.h"

#include <string.h>

enum {
    CODUOMP_VIRTUAL_WIDTH = 640,
    CODUOMP_VIRTUAL_HEIGHT = 480
};

void RE_BeginFrame(stereoFrame_t stereoFrame);

void RE_StretchPic(float x, float y, float width, float height,
                   float s1, float t1, float s2, float t2,
                   int32_t shaderHandle);
void RE_StretchPicGradient(float x, float y, float width, float height,
                           float s1, float t1, float s2, float t2,
                           int32_t shaderHandle,
                           const float *gradientColor,
                           int32_t gradientType);
void RE_StretchPicRotate(float x, float y, float width, float height,
                         float s1, float t1, float s2, float t2,
                         float angleDegrees, int32_t shaderHandle);
void RE_DrawQuadPic(const vec2_t positions[4], const vec2_t texCoords[4],
                    int32_t shaderHandle);
/* This translation unit is an isolated improved compatibility interface. */

/* NOT_FROM_ORIGINAL_SOURCE: identifies native-widescreen cgame command
 * submission. Gameplay HUD and cgame menus share the centered proportional
 * canvas; fullscreen UI, engine-owned 2D, and classic 4:3 retain their
 * dedicated presentation paths. */
static qboolean coduomp_uses_widescreen_cgame_2d(void)
{
    if (coduomp_cgame_rendering_compat_active == qfalse ||
        coduomp_cgame_hud_stretch_active != qfalse ||
        r_aspectMode == NULL || r_aspectMode->integer != 0 ||
        glConfig.vidWidth <= 0 || glConfig.vidHeight <= 0) {
        return qfalse;
    }

    return (int64_t)glConfig.vidWidth * 3 >
                   (int64_t)glConfig.vidHeight * 4
               ? qtrue
               : qfalse;
}
/* NOT_FROM_ORIGINAL_SOURCE: renderer commands queued since the last issued
 * frame — the widescreen backdrop pre-queue and any 2D the cgame loading pump
 * submits before forcing a present — must not execute before this frame's
 * draw-buffer selection and presentation clears (RB_DrawBuffer erases the
 * drawable for classic 4:3 and fullscreen-UI frames). The command buffer is
 * linear, so rotate the recovered RE_BeginFrame's own commands to its head. */
void coduomp_re_begin_frame_compat(stereoFrame_t stereoFrame)
{
    int32_t preQueuedBytes = 0;

    if (rendererBackendData != NULL)
        preQueuedBytes = rendererBackendData->commandUsed;

    RE_BeginFrame(stereoFrame);

    if (preQueuedBytes <= 0 || rendererBackendData == NULL)
        return;

    const int32_t totalBytes = rendererBackendData->commandUsed;
    const int32_t beginBytes = totalBytes - preQueuedBytes;
    uint8_t beginCommands[64];

    /* A cvar-triggered R_SyncRenderThread inside RE_BeginFrame issues the
     * buffer early (totalBytes < preQueuedBytes); nothing left to rotate. */
    if (beginBytes <= 0 || (size_t)beginBytes > sizeof(beginCommands))
        return;

    memcpy(beginCommands,
           &rendererBackendData->commandBuffer[preQueuedBytes],
           (size_t)beginBytes);
    memmove(&rendererBackendData->commandBuffer[beginBytes],
            &rendererBackendData->commandBuffer[0],
            (size_t)preQueuedBytes);
    memcpy(&rendererBackendData->commandBuffer[0], beginCommands,
           (size_t)beginBytes);
}

/* NOT_FROM_ORIGINAL_SOURCE: record a cgame presentation transition in the
 * renderer command stream. Renderer execution is deferred, so a frontend
 * boolean alone cannot reliably describe the source of later text commands. */
void coduomp_queue_cgame_2d_presentation(qboolean enabled)
{
    coduomp_cgame_2d_presentation_command_t *command;

    /* Snapshot the cgame-published stretch policy once per scope open so the
     * frontend picture bias and the deferred text transform inside this scope
     * agree, even if the policy cvar changes mid-frame. */
    if (enabled != qfalse) {
        coduomp_cgame_hud_stretch_active =
            coduomp_cgame_hud_stretch_requested();
    }

    command = (coduomp_cgame_2d_presentation_command_t *)R_GetCommandBuffer(
        (int32_t)sizeof(*command));

    if (command == NULL)
        return;

    command->commandId = RC_SET_CGAME_2D_PRESENTATION;
    command->enabled = enabled;
    command->stretched =
        enabled != qfalse ? coduomp_cgame_hud_stretch_active : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: record a console presentation transition in the
 * deferred renderer stream. Console glyphs use a height-derived native-width
 * canvas and therefore must not inherit either the cgame or fullscreen-UI
 * projection that happened to precede them. */
void coduomp_queue_console_2d_presentation(qboolean enabled)
{
    coduomp_console_2d_presentation_command_t *command =
        (coduomp_console_2d_presentation_command_t *)R_GetCommandBuffer(
            (int32_t)sizeof(*command));

    if (command == NULL)
        return;

    command->commandId = RC_SET_CONSOLE_2D_PRESENTATION;
    command->enabled = enabled;
}

/* NOT_FROM_ORIGINAL_SOURCE: record a UI presentation transition in command
 * order. Unlike the legacy r_uifullscreen cvar, this cannot change meaning
 * between a menu's submitted background and its later submitted items when a
 * tab script rebuilds the open-menu stack. */
void coduomp_queue_ui_2d_presentation(qboolean enabled)
{
    coduomp_ui_2d_presentation_command_t *command =
        (coduomp_ui_2d_presentation_command_t *)R_GetCommandBuffer(
            (int32_t)sizeof(*command));

    if (command == NULL)
        return;

    command->commandId = RC_SET_UI_2D_PRESENTATION;
    command->enabled = enabled;
    /* A mod that owns HUD presentation authored its in-game menu screens
     * against the same stock full-width stretch; those UI scopes skip the
     * fitted 4:3 viewport. Main-menu, connect, and loading UI scopes are
     * stock-owned and stay fitted. */
    command->stretched =
        enabled != qfalse ? coduomp_ui_hud_stretch_requested() : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: returns the single physical translation from the
 * proportional 640x480 canvas to the center of the native drawable. Every
 * primitive in an ordinary cgame composition receives this exact translation;
 * edge placement is selected only at explicit cgame HUD-group boundaries. */
static float coduomp_centered_cgame_canvas_bias(void)
{
    const float uniformScale =
        (float)glConfig.vidHeight / (float)CODUOMP_VIRTUAL_HEIGHT;
    return ((float)glConfig.vidWidth -
            (float)CODUOMP_VIRTUAL_WIDTH * uniformScale) * 0.5f;
}

/* NOT_FROM_ORIGINAL_SOURCE: places an already proportionally scaled cgame
 * picture on the centered canvas. */
void coduomp_re_stretch_pic_compat(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2, int32_t shaderHandle)
{
    if (coduomp_uses_widescreen_cgame_2d() != qfalse)
        x += coduomp_centered_cgame_canvas_bias();
    RE_StretchPic(x, y, width, height, s1, t1, s2, t2, shaderHandle);
}

/* NOT_FROM_ORIGINAL_SOURCE: widescreen cgame wrapper for gradient pictures. */
void coduomp_re_stretch_pic_gradient_compat(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2, int32_t shaderHandle,
    const float *gradientColor, int32_t gradientType)
{
    if (coduomp_uses_widescreen_cgame_2d() != qfalse)
        x += coduomp_centered_cgame_canvas_bias();
    RE_StretchPicGradient(x, y, width, height, s1, t1, s2, t2,
                          shaderHandle, gradientColor, gradientType);
}

/* NOT_FROM_ORIGINAL_SOURCE: widescreen cgame wrapper for rotated pictures. */
void coduomp_re_stretch_pic_rotate_compat(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2,
    float angleDegrees, int32_t shaderHandle)
{
    if (coduomp_uses_widescreen_cgame_2d() != qfalse)
        x += coduomp_centered_cgame_canvas_bias();
    RE_StretchPicRotate(x, y, width, height, s1, t1, s2, t2,
                        angleDegrees, shaderHandle);
}

/* NOT_FROM_ORIGINAL_SOURCE: applies the centered-canvas translation uniformly
 * to every corner of a cgame quad. */
void coduomp_re_draw_quad_pic_compat(
    const vec2_t positions[4], const vec2_t texCoords[4],
    int32_t shaderHandle)
{
    if (coduomp_uses_widescreen_cgame_2d() != qfalse) {
        vec2_t adjusted[4];
        const float shift = coduomp_centered_cgame_canvas_bias();
        for (int32_t corner = 0; corner < 4; ++corner) {
            adjusted[corner][0] =
                positions[corner][0] + shift;
            adjusted[corner][1] = positions[corner][1];
        }
        RE_DrawQuadPic(adjusted, texCoords, shaderHandle);
        return;
    }

    RE_DrawQuadPic(positions, texCoords, shaderHandle);
}
