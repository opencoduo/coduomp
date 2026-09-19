#include "backend.h"

#include "output_gamma_compat.h"
#include "platform_gamma.h"
#include "renderer_cvars.h"
#include "client/common/client_color_settings.h"

#include <math.h>
#include <stdint.h>

#define R_COLOR_MAPPING_INV_255_F 0.003921568859368563f /* 0x3b808081 */
#define CODUOMP_COLOR_CHANNEL_MAXIMUM 255
#define CODUOMP_CONTRAST_MIDPOINT 127.5f

/* NOT_FROM_ORIGINAL_SOURCE: adjust the completed gamma/overbright mapping
 * around mid-gray, then round and saturate to an output byte. The neutral
 * setting preserves every existing table entry exactly. */
static uint8_t coduomp_contrast_channel(int32_t value)
{
    const float adjusted = ((float)value - CODUOMP_CONTRAST_MIDPOINT) * r_contrast->value + CODUOMP_CONTRAST_MIDPOINT;

    if (adjusted <= 0.0f)
        return 0;
    if (adjusted >= CODUOMP_COLOR_CHANNEL_MAXIMUM)
        return CODUOMP_COLOR_CHANNEL_MAXIMUM;
    return (uint8_t)(adjusted + 0.5f);
}

/* Source: CoDUOMP.exe 0x0050a650..0x0050a851.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050a650_0050a852.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * R_SetColorMappings. This compatibility provider is selected as a whole so
 * output-LUT policy does not interleave preprocessor branches through the
 * recovered provider. */
void R_SetColorMappings(void)
{
    tr.overbrightBits = 0;
    if (coduomp_gamma_output_available() != qfalse &&
        glConfig.isFullscreen && r_overBrightBits->integer != 0) {
        tr.overbrightBits = 1;
    }

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

    /* NOT_FROM_ORIGINAL_SOURCE: console/config values obey the same limits
     * as the menu, including non-finite input before float-to-byte conversion. */
    if (!isfinite(r_contrast->value))
        ri.Cvar_Set("r_contrast", CODUOMP_CONTRAST_DEFAULT_STRING);
    AssertCvarRange(r_contrast, CODUOMP_CONTRAST_MINIMUM, CODUOMP_CONTRAST_MAXIMUM, qfalse);

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
        rendererGammaTable[input] = coduomp_contrast_channel(gammaValue);

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
            coduomp_contrast_channel(gammaOverbrightValue);
    }

    for (int32_t input = 0; input < 256; ++input) {
        int32_t intensityValue =
            (int32_t)((float)input * r_intensity->value);

        if (intensityValue > 255)
            intensityValue = 255;
        rendererIntensityTable[input] = (uint8_t)intensityValue;
    }

    const qboolean nativeGammaBeforeSet =
        glConfig.deviceSupportsGamma;
    if (nativeGammaBeforeSet != qfalse) {
        GLimp_SetGamma(rendererGammaOverbrightTable,
                       rendererGammaOverbrightTable,
                       rendererGammaOverbrightTable);
    }

    if (coduomp_output_gamma_software_active_compat() != qfalse) {
        coduomp_output_gamma_set_lut_compat(
            rendererGammaOverbrightTable,
            rendererGammaOverbrightTable,
            rendererGammaOverbrightTable);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: if every native and final-output path failed
     * after mappings were built with overbright, rebuild once without that
     * hardware-only assumption. */
    if (nativeGammaBeforeSet != qfalse &&
        coduomp_gamma_output_available() == qfalse &&
        tr.overbrightBits != 0) {
        R_SetColorMappings();
    }
}
