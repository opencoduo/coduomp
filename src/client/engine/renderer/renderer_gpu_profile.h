#ifndef CODUOMP_RENDERER_GPU_PROFILE_H
#define CODUOMP_RENDERER_GPU_PROFILE_H

/* NOT_FROM_ORIGINAL_SOURCE: declarations for the compiler- and runtime-gated
 * asynchronous renderer GPU timing diagnostic. */
#if defined(CODUOMP_RENDERER_GPU_PROFILE)

#include "../q_shared.h"

typedef enum coduomp_gpu_profile_phase_e {
    CODUOMP_GPU_PROFILE_PHASE_VIEW_SETUP,
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

void coduomp_gpu_profile_register(void);
void coduomp_gpu_profile_shutdown(void);
void coduomp_gpu_profile_frame_begin(void);
void coduomp_gpu_profile_frame_end(void);
qboolean coduomp_gpu_profile_begin(
    coduomp_gpu_profile_phase_t phase, const char *shaderName);
qboolean coduomp_gpu_profile_begin_surface(void);
void coduomp_gpu_profile_end(qboolean started);

#endif

#endif
