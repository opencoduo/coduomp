#include "backend.h"

#include "renderer_cvars.h"

#include <math.h>
#include <stdint.h>

#define R_COLOR_MAPPING_INV_255_F 0.003921568859368563f /* 0x3b808081 */

/* Source: CoDUOMP.exe 0x0050a650..0x0050a851.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050a650_0050a852.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * R_SetColorMappings. The four tables consumed by R_LightScaleTexture are
 * kept distinct because hardware gamma changes which overbright operation
 * must be performed in software. */
void R_SetColorMappings(void)
{
    tr.overbrightBits = 0;
    if (glConfig.deviceSupportsGamma && glConfig.isFullscreen &&
        r_overBrightBits->integer != 0) {
        tr.overbrightBits = 1;
    }

    /* 0x0050a693..0x0050a6a9 stores the reciprocal to identityLight but
     * retains it for the identity-byte conversion. */
    const long double identityLightRaw =
        1.0L / (long double)(1 << tr.overbrightBits);
    tr.identityLight = (float)identityLightRaw;
    tr.identityLightByte =
        ((int32_t)(255.0L * identityLightRaw)) & 255;

    if (r_intensity->value <= 1.0f)
        ri.Cvar_Set("r_intensity", "1");
    if (r_gamma->value < 0.5f)
        ri.Cvar_Set("r_gamma", "0.5");
    else if (r_gamma->value > 3.0f)
        ri.Cvar_Set("r_gamma", "3.0");

    const float inverseGamma = 1.0f / r_gamma->value;
    for (int32_t input = 0; input < 256; ++input) {
        int32_t gammaValue = input;

        if (r_gamma->value != 1.0f) {
            gammaValue = (int32_t)(
                pow((double)((float)input * R_COLOR_MAPPING_INV_255_F),
                    (double)inverseGamma) * 255.0 + 0.5);
        }
        if (gammaValue < 0)
            gammaValue = 0;
        else if (gammaValue > 255)
            gammaValue = 255;
        rendererGammaTable[input] = (uint8_t)gammaValue;

        int32_t overbrightValue = input << tr.overbrightBits;
        if (overbrightValue < 0)
            overbrightValue = 0;
        else if (overbrightValue > 255)
            overbrightValue = 255;
        rendererOverbrightTable[input] = (uint8_t)overbrightValue;
        rendererInverseOverbrightTable[input] =
            (uint8_t)(input >> tr.overbrightBits);

        int32_t gammaOverbrightValue =
            gammaValue << tr.overbrightBits;
        if (gammaOverbrightValue < 0)
            gammaOverbrightValue = 0;
        else if (gammaOverbrightValue > 255)
            gammaOverbrightValue = 255;
        rendererGammaOverbrightTable[input] =
            (uint8_t)gammaOverbrightValue;
    }

    for (int32_t input = 0; input < 256; ++input) {
        int32_t intensityValue =
            (int32_t)((float)input * r_intensity->value);

        if (intensityValue > 255)
            intensityValue = 255;
        rendererIntensityTable[input] = (uint8_t)intensityValue;
    }

    if (glConfig.deviceSupportsGamma) {
        GLimp_SetGamma(rendererGammaOverbrightTable,
                       rendererGammaOverbrightTable,
                       rendererGammaOverbrightTable);
    }
}
