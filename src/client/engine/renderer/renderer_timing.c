#include "backend.h"

/* Source: CoDUOMP.exe 0x004e3290..0x004e32fe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3290_004e32fe.mcode.
 * Name: same-module Mac symbol R_UpdateOverTime. The Windows x87 body stores
 * each candidate back to single precision before applying its target clamp. */
float R_UpdateOverTime(float currentValue, float targetValue,
                       int32_t riseTime, int32_t fallTime,
                       int32_t elapsedTime)
{
    if (currentValue < targetValue) {
        if (riseTime <= 0)
            return targetValue;

        const long double candidate =
            (long double)elapsedTime / riseTime + currentValue;
        currentValue = (float)candidate;
        if (candidate > targetValue)
            return targetValue;
    } else if (currentValue > targetValue) {
        if (fallTime <= 0)
            return targetValue;

        const long double candidate =
            (long double)currentValue -
            (long double)elapsedTime / fallTime;
        currentValue = (float)candidate;
        if (candidate < targetValue)
            return targetValue;
    }

    return currentValue;
}
