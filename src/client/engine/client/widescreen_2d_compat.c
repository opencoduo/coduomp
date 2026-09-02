#include "widescreen_2d_compat.h"


#include "cgame.h"
#include "console_display_compat.h"

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: true only while the recovered
 * client is accepting renderer commands from the cgame VM.  The renderer
 * export compatibility layer uses it to avoid changing UI, cinematic, and
 * engine-owned 2D calls. */
qboolean coduomp_cgame_rendering_compat_active;
/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: true while native-width console
 * primitives are being submitted. Renderer-side console icons call the client
 * AdjustFrom640 import immediately instead of waiting for backend execution. */
qboolean coduomp_console_rendering_compat_active;

/* NOT_FROM_ORIGINAL_SOURCE: expose the UI VM's authoritative fullscreen state
 * to the renderer's existing 4:3 fullscreen viewport policy.  The recovered
 * screen dispatcher otherwise keeps that state in a stack local, leaving an
 * in-game main/options menu stretched across a native widescreen drawable. */
void coduomp_set_ui_fullscreen_compat(qboolean fullscreen)
{
    const int32_t requestedValue = fullscreen != qfalse ? 1 : 0;
    const cvar_t *const currentValue = Cvar_FindVar("r_uifullscreen");

    if (currentValue != NULL && currentValue->integer == requestedValue)
        return;

    Cvar_Set("r_uifullscreen", requestedValue != 0 ? "1" : "0");
}
/* NOT_FROM_ORIGINAL_SOURCE: move only the gameplay notify/join-feed origin
 * from the centered 640 canvas to the physical left edge.  The console text
 * backend still applies the ordinary proportional 640 transform afterward. */
int32_t coduomp_left_hud_virtual_x_compat(int32_t x)
{
    const cvar_t *aspectMode = Cvar_FindVar("r_aspectMode");

    if (cls.rendererConfig.vidWidth <= 0 || cls.rendererConfig.vidHeight <= 0 ||
        (int64_t)cls.rendererConfig.vidWidth * 3 <= (int64_t)cls.rendererConfig.vidHeight * 4 ||
        (aspectMode != NULL && aspectMode->integer != 0)) {
        return x;
    }

    return x - (((cls.rendererConfig.vidWidth * 480) / cls.rendererConfig.vidHeight - 640) / 2);
}

/* NOT_FROM_ORIGINAL_SOURCE: maps virtual 640x480 coordinates to the same
 * centered proportional canvas used by the cgame image helpers. Renderer text
 * is command-buffered and does not call AdjustFrom640 until the backend runs,
 * after CL_CGameRendering has returned. Console icons embedded in that text
 * are adjusted immediately during cgame submission, so both the frontend and
 * backend halves of the command-ordered scope are accepted. Fullscreen UI and
 * classic presentation use a fitted 4:3 renderer viewport and retain the
 * recovered stock transform. */
void coduomp_scr_adjust_from_640_compat(float *x, float *y, float *width, float *height)
{
    float verticalScale = (float)cls.rendererConfig.vidHeight / 480.0f;
    float horizontalScale = (float)cls.rendererConfig.vidWidth / 640.0f;
    const cvar_t *aspectMode = Cvar_FindVar("r_aspectMode");
    float horizontalBias = 0.0f;

    if (coduomp_console_rendering_compat_active != qfalse || coduomp_backend_console_2d_compat_active != qfalse) {
        horizontalScale = coduomp_console_canvas_scale_compat();
        verticalScale = horizontalScale;
    } else if (cls.rendererConfig.vidWidth > 0 && cls.rendererConfig.vidHeight > 0 &&
               (int64_t)cls.rendererConfig.vidWidth * 3 > (int64_t)cls.rendererConfig.vidHeight * 4 &&
               (aspectMode == NULL || aspectMode->integer == 0) &&
               (coduomp_cgame_rendering_compat_active != qfalse || coduomp_backend_cgame_2d_compat_active != qfalse)) {
        horizontalScale = verticalScale;
        horizontalBias = ((float)cls.rendererConfig.vidWidth - 640.0f * verticalScale) * 0.5f;
    }

    if (x != NULL)
        *x = *x * horizontalScale + horizontalBias;
    if (y != NULL)
        *y *= verticalScale;
    if (width != NULL)
        *width *= horizontalScale;
    if (height != NULL)
        *height *= verticalScale;
}
