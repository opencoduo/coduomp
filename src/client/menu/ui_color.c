#include "ui_runtime.h"

/*
 * The original Windows LerpColor bodies are instruction-identical:
 * uo_cgame_mp_x86.dll 0x30050110 and uo_ui_mp_x86.dll 0x40011c30.  Each lane
 * keeps the interpolated x87 value live while storing the binary32 result and
 * performing the [0,1] clamp comparisons.
 */
void LerpColor(vec4_t output, const vec4_t from, const vec4_t to,
               float fraction)
{
    int32_t component;

    for (component = 0; component < 4; ++component) {
        long double value =
            (long double)from[component] +
            (long double)fraction *
                ((long double)to[component] -
                 (long double)from[component]);

        output[component] = (float)value;
        if (value < 0.0L) {
            output[component] = 0.0f;
        } else if (value > 1.0L) {
            output[component] = 1.0f;
        }
    }
}
