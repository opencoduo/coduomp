#ifndef CODUOMP_RENDERER_GPU_PROFILE_H
#define CODUOMP_RENDERER_GPU_PROFILE_H

/* NOT_FROM_ORIGINAL_SOURCE: declarations for the compiler- and runtime-gated
 * asynchronous renderer GPU timing diagnostic. */
#if defined(CODUOMP_RENDERER_GPU_PROFILE)

#include "../q_shared.h"

struct shader_s;

typedef enum coduomp_gpu_profile_phase_e {
    CODUOMP_GPU_PROFILE_PHASE_VIEW_SETUP,
    CODUOMP_GPU_PROFILE_PHASE_SCENE,
    CODUOMP_GPU_PROFILE_PHASE_WORLD,
    CODUOMP_GPU_PROFILE_PHASE_BRUSH_MODELS,
    CODUOMP_GPU_PROFILE_PHASE_MODELS,
    CODUOMP_GPU_PROFILE_PHASE_STATIC_MODELS,
    CODUOMP_GPU_PROFILE_PHASE_EFFECTS,
    CODUOMP_GPU_PROFILE_PHASE_SKY,
    CODUOMP_GPU_PROFILE_PHASE_SHADOWS,
    CODUOMP_GPU_PROFILE_PHASE_FLARES,
    CODUOMP_GPU_PROFILE_PHASE_2D,
    CODUOMP_GPU_PROFILE_PHASE_CLEAR,
    CODUOMP_GPU_PROFILE_PHASE_SCREEN_COPY,
    CODUOMP_GPU_PROFILE_PHASE_PRESENT,
    CODUOMP_GPU_PROFILE_PHASE_MISC,
    CODUOMP_GPU_PROFILE_PHASE_COUNT
} coduomp_gpu_profile_phase_t;

typedef enum coduomp_gpu_profile_break_reason_e {
    CODUOMP_GPU_PROFILE_BREAK_SHADER = 1u << 0,
    CODUOMP_GPU_PROFILE_BREAK_STORAGE = 1u << 1,
    CODUOMP_GPU_PROFILE_BREAK_DLIGHT = 1u << 2,
    CODUOMP_GPU_PROFILE_BREAK_BATCH_FLAG2 = 1u << 3,
    CODUOMP_GPU_PROFILE_BREAK_ENTITY = 1u << 4
} coduomp_gpu_profile_break_reason_t;

void coduomp_gpu_profile_register(void);
void coduomp_gpu_profile_shutdown(void);
void coduomp_gpu_profile_frame_begin(void);
void coduomp_gpu_profile_frame_end(void);
void coduomp_gpu_profile_segment(coduomp_gpu_profile_phase_t phase);
void coduomp_gpu_profile_suspend_segment(void);
qboolean coduomp_gpu_profile_begin(
    coduomp_gpu_profile_phase_t phase, const char *shaderName);
qboolean coduomp_gpu_profile_begin_surface(void);
void coduomp_gpu_profile_end_surface(qboolean started);
void coduomp_gpu_profile_record_draw_call(void);
void coduomp_gpu_profile_note_drawsurf_break(
    uint32_t reasons, const struct shader_s *nextShader);
void coduomp_gpu_profile_note_overflow(void);
void coduomp_gpu_profile_end(qboolean started);

#endif

#endif
