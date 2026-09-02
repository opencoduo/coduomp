#ifndef CODUOMP_RENDERER_RETICLE_UPSCALE_COMPAT_H
#define CODUOMP_RENDERER_RETICLE_UPSCALE_COMPAT_H

#include <stdint.h>

#include "backend.h"


/* NOT_FROM_ORIGINAL_SOURCE: edge-preserving load-time upscale for the
 * gfx/reticle/ image family, so proportionally sized crosshair pieces stay
 * crisp on high-resolution drawables without any replacement assets. */
void coduomp_compat_upscale_reticle_pixels(
    const char *name, uint8_t **pixels,
    uint16_t *width, uint16_t *height, uint32_t format,
    renderer_image_load_mode_t loadMode);


#endif
