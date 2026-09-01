#include <string.h>

#include "../module/ui_functions.h"

enum {
    UI_BYTES_PER_KILOBYTE = 1024,
    UI_BYTES_PER_MEGABYTE = 1024 * 1024,
    UI_BYTES_PER_GIGABYTE = 1024 * 1024 * 1024,
    UI_SECONDS_PER_MINUTE = 60,
    UI_SECONDS_PER_HOUR = 60 * 60,
    UI_SIZE_FRACTION_SCALE = 100
};

// Source: uo_ui_mp_x86.dll 0x40010450..0x400105c8
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40010450_400105c8.mcode
// Exact same-module PPC symbol: UI_ReadableSize.
void UI_ReadableSize(char *buffer, int32_t bufferSize, int32_t value)
{
    if (value > UI_BYTES_PER_GIGABYTE) {
        int32_t length;
        int32_t remainder;
        int32_t scaledRemainder;
        const char *unitName;

        Com_sprintf(buffer, bufferSize, "%d",
                    value / UI_BYTES_PER_GIGABYTE);
        length = (int32_t)strlen(buffer);
        unitName = UI_SafeTranslateString("EXE_GIGABYTE");
        remainder = value % UI_BYTES_PER_GIGABYTE;
        scaledRemainder = (int32_t)((uint32_t)remainder *
                                    (uint32_t)UI_SIZE_FRACTION_SCALE);
        Com_sprintf(buffer + length, bufferSize - length, ".%02d %s",
                    scaledRemainder / UI_BYTES_PER_GIGABYTE, unitName);
        return;
    }
    if (value > UI_BYTES_PER_MEGABYTE) {
        int32_t length;
        int32_t remainder;
        int32_t scaledRemainder;
        const char *unitName;

        Com_sprintf(buffer, bufferSize, "%d",
                    value / UI_BYTES_PER_MEGABYTE);
        length = (int32_t)strlen(buffer);
        unitName = UI_SafeTranslateString("EXE_MEGABYTE");
        remainder = value % UI_BYTES_PER_MEGABYTE;
        scaledRemainder = (int32_t)((uint32_t)remainder *
                                    (uint32_t)UI_SIZE_FRACTION_SCALE);
        Com_sprintf(buffer + length, bufferSize - length, ".%02d %s",
                    scaledRemainder / UI_BYTES_PER_MEGABYTE, unitName);
        return;
    }
    if (value > UI_BYTES_PER_KILOBYTE) {
        const char *unitName = UI_SafeTranslateString("EXE_KILOBYTE");
        int32_t kilobytes = value / UI_BYTES_PER_KILOBYTE;

        Com_sprintf(buffer, bufferSize, "%d %s", kilobytes, unitName);
        return;
    }
    {
        const char *unitName = UI_SafeTranslateString("EXE_BYTES");
        Com_sprintf(buffer, bufferSize, "%d %s", value, unitName);
    }
}

// Source: uo_ui_mp_x86.dll 0x400105d0..0x4001068d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400105d0_4001068d.mcode
// Exact same-module PPC symbol: UI_PrintTime.
void UI_PrintTime(char *buffer, int32_t bufferSize, int32_t time)
{
    if (time > UI_SECONDS_PER_HOUR) {
        int32_t hours = time / UI_SECONDS_PER_HOUR;
        int32_t minutes =
            (time % UI_SECONDS_PER_HOUR) / UI_SECONDS_PER_MINUTE;
        const char *minutesName = UI_SafeTranslateString("EXE_MINUTES");
        const char *hoursName = UI_SafeTranslateString("EXE_HOURS");

        Com_sprintf(buffer, bufferSize, "%d %s %d %s", hours, hoursName,
                    minutes, minutesName);
        return;
    }
    if (time > UI_SECONDS_PER_MINUTE) {
        int32_t minutes = time / UI_SECONDS_PER_MINUTE;
        int32_t seconds = time % UI_SECONDS_PER_MINUTE;
        const char *secondsName = UI_SafeTranslateString("EXE_SECONDS");
        const char *minutesName = UI_SafeTranslateString("EXE_MINUTES");

        Com_sprintf(buffer, bufferSize, "%d %s %d %s", minutes,
                    minutesName, seconds, secondsName);
        return;
    }
    Com_sprintf(buffer, bufferSize, "%d %s", time,
                UI_SafeTranslateString("EXE_SECONDS"));
}
