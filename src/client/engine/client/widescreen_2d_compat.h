#ifndef CODUOMP_CLIENT_WIDESCREEN_2D_COMPAT_H
#define CODUOMP_CLIENT_WIDESCREEN_2D_COMPAT_H

#include "../effects/fx_render_types.h"
#include "../q_shared.h"


extern qboolean coduomp_cgame_rendering_compat_active;
extern qboolean coduomp_console_rendering_compat_active;
extern qboolean coduomp_cgame_hud_stretch_active;
extern qboolean coduomp_backend_cgame_2d_compat_active;
extern qboolean coduomp_backend_console_2d_compat_active;
extern qboolean coduomp_backend_cgame_2d_stretch_active;
extern qboolean coduomp_backend_ui_2d_compat_active;

qboolean coduomp_cgame_hud_stretch_requested(void);
void coduomp_scr_adjust_from_640_compat(float *x, float *y,
                                       float *width, float *height);
void coduomp_set_ui_fullscreen_compat(qboolean fullscreen);
int32_t coduomp_left_hud_virtual_x_compat(int32_t x);
void coduomp_queue_cgame_2d_presentation(qboolean enabled);
void coduomp_queue_console_2d_presentation(qboolean enabled);
void coduomp_queue_ui_2d_presentation(qboolean enabled);

void coduomp_re_stretch_pic_compat(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2, int32_t shaderHandle);
void coduomp_re_stretch_pic_gradient_compat(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2, int32_t shaderHandle,
    const float *gradientColor, int32_t gradientType);
void coduomp_re_stretch_pic_rotate_compat(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2,
    float angleDegrees, int32_t shaderHandle);
void coduomp_re_draw_quad_pic_compat(
    const vec2_t positions[4], const vec2_t texCoords[4],
    int32_t shaderHandle);

#endif
