#include "hunk.h"

#include <stdint.h>

enum {
    HUNK_TOUCH_WORD_SHIFT = 2,
    HUNK_TOUCH_WORD_STRIDE = 64
};

int32_t Sys_Milliseconds(void);
void Com_Printf(const char *format, ...);

/*
 * CoDUOMP.exe 0x004356a0 and coduo_lnxded 0x0806bcbf retain the same two
 * pretouch loops and final diagnostic.  Arithmetic which the i386 bodies
 * perform in dwords is expressed through uint32_t so native builds retain
 * the original wrapping checksum and millisecond subtraction.
 */
void Com_TouchMemory(void)
{
    const uint32_t startTime = (uint32_t)Sys_Milliseconds();
    const int32_t *const words = (const int32_t *)hunk_data;
    uint32_t checksum = 0;

    const size_t lowStop = hunk.lowUsed >> HUNK_TOUCH_WORD_SHIFT;
    for (size_t word = 0; word < lowStop;
         word += HUNK_TOUCH_WORD_STRIDE) {
        checksum += (uint32_t)words[word];
    }

    const size_t highStart =
        (hunk.totalSize - hunk.highUsed) >> HUNK_TOUCH_WORD_SHIFT;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const size_t highStop = hunk.highUsed >> HUNK_TOUCH_WORD_SHIFT;
    for (size_t word = highStart; word < highStop;
         word += HUNK_TOUCH_WORD_STRIDE) {
        checksum += (uint32_t)words[word];
    }

    const uint32_t elapsed =
        (uint32_t)Sys_Milliseconds() - startTime;
    Com_Printf("Com_TouchMemory: %i msec. Using sum: %d\n",
               (int32_t)elapsed, (int32_t)checksum);
}
