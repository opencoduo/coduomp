#include "reticle_upscale_compat.h"


#include "gl_api.h"
#include "renderer_cvars.h"

/* NOT_FROM_ORIGINAL_SOURCE: the stock reticle textures are tiny hard-edged
 * alpha shapes authored for a 640x480 presentation.  The recovered crosshair
 * drawer now sizes them proportionally to the drawable, so at high
 * resolutions the sampler magnifies them several times over and bilinear
 * filtering smears the edges.  Rather than shipping or requiring replacement
 * art, upscale the loaded pixels in code with two EPX/Scale2x passes (a
 * pixel-art upscaler that preserves hard edges) before the GL upload.  The
 * transform runs on the pixels of the user's own, server-approved asset, so
 * it is invisible to pure-server checks, and it applies equally to mod
 * reticles under the same gfx/reticle/ prefix.  r_hiresReticles 0 disables
 * it (latched: images are transformed at load). */

enum {
    /* Only genuinely stock-era-sized art is upscaled; user- or mod-provided
     * high-resolution replacements pass through untouched. */
    RETICLE_UPSCALE_MAX_SOURCE_DIM = 128,
    RETICLE_UPSCALE_FACTOR = 4
};

static const char reticleUpscalePrefix[] = "gfx/reticle/";

/* One EPX/Scale2x pass: each source pixel P with 4-neighbors
 *   A (above), B (right), C (left), D (below)
 * expands to a 2x2 block initialized to P, with
 *   topLeft=A when C==A and C!=D and A!=B,
 *   topRight=B when A==B and A!=C and B!=D,
 *   bottomLeft=C when D==C and D!=B and C!=A,
 *   bottomRight=D when B==D and B!=A and D!=C.
 * Pixels compare as whole RGBA words; neighbors clamp at the borders. */
static void reticle_upscale_epx_pass(
    const uint32_t *source, int32_t sourceWidth, int32_t sourceHeight,
    uint32_t *destination)
{
    const int32_t destinationWidth = sourceWidth * 2;

    for (int32_t y = 0; y < sourceHeight; ++y) {
        const int32_t upY = y > 0 ? y - 1 : 0;
        const int32_t downY = y < sourceHeight - 1 ? y + 1 : y;

        for (int32_t x = 0; x < sourceWidth; ++x) {
            const int32_t leftX = x > 0 ? x - 1 : 0;
            const int32_t rightX = x < sourceWidth - 1 ? x + 1 : x;
            const uint32_t center = source[y * sourceWidth + x];
            const uint32_t above = source[upY * sourceWidth + x];
            const uint32_t right = source[y * sourceWidth + rightX];
            const uint32_t left = source[y * sourceWidth + leftX];
            const uint32_t below = source[downY * sourceWidth + x];
            uint32_t topLeft = center;
            uint32_t topRight = center;
            uint32_t bottomLeft = center;
            uint32_t bottomRight = center;

            if (left == above && left != below && above != right)
                topLeft = above;
            if (above == right && above != left && right != below)
                topRight = right;
            if (below == left && below != right && left != above)
                bottomLeft = left;
            if (right == below && right != above && below != left)
                bottomRight = below;

            uint32_t *destinationBlock =
                &destination[(y * 2) * destinationWidth + (x * 2)];
            destinationBlock[0] = topLeft;
            destinationBlock[1] = topRight;
            destinationBlock[destinationWidth] = bottomLeft;
            destinationBlock[destinationWidth + 1] = bottomRight;
        }
    }
}
void coduomp_compat_upscale_reticle_pixels(
    const char *name, uint8_t **pixels,
    uint16_t *width, uint16_t *height, uint32_t format,
    renderer_image_load_mode_t loadMode)
{
    if (r_hiresReticles == NULL || r_hiresReticles->integer == 0)
        return;
    if (name == NULL || pixels == NULL || *pixels == NULL ||
        width == NULL || height == NULL)
        return;
    /* Pixel loads only: metadata-only (delayed) loads carry no image data,
     * and only plain uncompressed RGBA is transformable. */
    if (loadMode != R_IMAGE_LOAD_PIXELS || format != GL_RGBA)
        return;
    if (Q_stricmpn(name, reticleUpscalePrefix,
                   (int32_t)(sizeof(reticleUpscalePrefix) - 1)) != 0)
        return;

    const int32_t sourceWidth = *width;
    const int32_t sourceHeight = *height;
    if (sourceWidth <= 0 || sourceHeight <= 0 ||
        sourceWidth > RETICLE_UPSCALE_MAX_SOURCE_DIM ||
        sourceHeight > RETICLE_UPSCALE_MAX_SOURCE_DIM)
        return;

    const int32_t finalWidth = sourceWidth * RETICLE_UPSCALE_FACTOR;
    const int32_t finalHeight = sourceHeight * RETICLE_UPSCALE_FACTOR;
    if (glConfig.maxTextureSize > 0 &&
        (finalWidth > glConfig.maxTextureSize ||
         finalHeight > glConfig.maxTextureSize))
        return;

    /* Both buffers are transient image allocations released by the caller's
     * ordinary R_FreeImageAllocations sweep after the GL upload copies out. */
    uint32_t *doubled = R_AllocTempMemory(
        (size_t)(sourceWidth * 2) * (size_t)(sourceHeight * 2) *
        sizeof(uint32_t));
    if (doubled == NULL)
        return;
    uint32_t *quadrupled = R_AllocTempMemory(
        (size_t)finalWidth * (size_t)finalHeight * sizeof(uint32_t));
    if (quadrupled == NULL)
        return;

    reticle_upscale_epx_pass((const uint32_t *)*pixels,
                             sourceWidth, sourceHeight, doubled);
    reticle_upscale_epx_pass(doubled, sourceWidth * 2, sourceHeight * 2,
                             quadrupled);

    *pixels = (uint8_t *)quadrupled;
    *width = (uint16_t)finalWidth;
    *height = (uint16_t)finalHeight;
}
