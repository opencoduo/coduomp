#ifndef CODUOMP_PLATFORM_GAMMA_H
#define CODUOMP_PLATFORM_GAMMA_H

#include "../q_shared.h"


qboolean coduomp_gamma_output_available(void);

#if !defined(_WIN32)
void coduomp_gamma_window_focus_changed(qboolean active);
#endif

#endif
