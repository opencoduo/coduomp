#ifndef CODUOMP_OUTPUT_GAMMA_COMPAT_H
#define CODUOMP_OUTPUT_GAMMA_COMPAT_H

#include "../q_shared.h"

#include <stdint.h>


void coduomp_output_gamma_initialize_compat(void);
qboolean coduomp_output_gamma_activate_software_compat(const char *reason);
qboolean coduomp_output_gamma_software_active_compat(void);
void coduomp_output_gamma_set_lut_compat(const uint8_t red[256], const uint8_t green[256], const uint8_t blue[256]);
qboolean coduomp_output_gamma_try_xrandr_compat(uint16_t originalRamp[3][256]);
qboolean coduomp_output_gamma_xrandr_active_compat(void);
qboolean coduomp_output_gamma_set_xrandr_compat(const uint16_t ramp[3][256]);
void coduomp_output_gamma_restore_xrandr_compat(void);
void coduomp_output_gamma_present_compat(void);
qboolean coduomp_capture_presented_frame_compat(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t pixelFormat, uint8_t *pixels);
void coduomp_output_gamma_shutdown_compat(void);


#endif
