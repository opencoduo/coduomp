#ifndef CODUOMP_PLATFORM_GAMMA_H
#define CODUOMP_PLATFORM_GAMMA_H

#include "../q_shared.h"

typedef enum coduomp_gamma_mode_e {
    CODUOMP_GAMMA_MODE_DISABLED = 0,
    CODUOMP_GAMMA_MODE_AUTOMATIC = 1,
    CODUOMP_GAMMA_MODE_SOFTWARE = 2
} coduomp_gamma_mode_t;

qboolean coduomp_gamma_texture_fallback_enabled_compat(void);

qboolean coduomp_gamma_output_available(void);

#if !defined(_WIN32)
void coduomp_gamma_window_focus_changed(qboolean active);
#endif

#endif
