#include "compat/crt/qsort_compat.h"

#include <stdint.h>
#include <stdlib.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Shared server qsort requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    CODUO_QSORT_CUTOFF = 8,
    CODUO_QSORT_STACK_DEPTH = 30
};

/* NOT_FROM_ORIGINAL_SOURCE: byte-swap factoring for the compatibility sorter.
 * The retail CRT inlines this loop at each swap site. */
static void coduo_qsort_compat_swap_bytes(uint8_t *left, uint8_t *right,
                                          size_t width)
{
    while (width-- != 0) {
        const uint8_t temporary = *left;
        *left++ = *right;
        *right++ = temporary;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: compatibility transcription of the bundled CRT
 * shortsort. The four Windows binaries contain the same instructions at:
 * CoDUOMP.exe RVA 0x16d520, uo_cgame_mp_x86.dll RVA 0x5b9d0,
 * uo_ui_mp_x86.dll RVA 0x1e670, and uo_game_mp_x86.dll RVA 0x59a10.
 * It selects the first maximum on comparator ties. */
static void coduo_qsort_compat_shortsort(
    uint8_t *base, ptrdiff_t low, ptrdiff_t high, size_t width,
    int (*compare)(const void *, const void *))
{
    while (high > low) {
        ptrdiff_t maximum = low;

        for (ptrdiff_t current = low + 1; current <= high; ++current) {
            if (compare(base + (size_t)current * width,
                        base + (size_t)maximum * width) > 0) {
                maximum = current;
            }
        }

        coduo_qsort_compat_swap_bytes(base + (size_t)maximum * width,
                                      base + (size_t)high * width, width);
        --high;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: host-portable compatibility transcription of the
 * bundled MSVC qsort. The four identical Windows bodies begin at CoDUOMP.exe
 * RVA 0x16d590, uo_cgame_mp_x86.dll RVA 0x5ba40, uo_ui_mp_x86.dll RVA
 * 0x1e6e0, and uo_game_mp_x86.dll RVA 0x59a80. The zero-width check, cutoff,
 * median-of-three comparisons, pivot tracking, equal-element scans, and
 * smaller-partition-first stack order match the retail instructions. Indexes
 * avoid forming the before-base pointer that the original 32-bit routine
 * temporarily represents arithmetically. */
void coduo_crt_qsort(void *base, size_t count, size_t width,
                     int (*compare)(const void *, const void *))
{
    ptrdiff_t lowStack[CODUO_QSORT_STACK_DEPTH];
    ptrdiff_t highStack[CODUO_QSORT_STACK_DEPTH];
    ptrdiff_t stackDepth = 0;
    ptrdiff_t low;
    ptrdiff_t high;
    uint8_t *bytes = base;

    if (count < 2 || width == 0)
        return;

    low = 0;
    high = (ptrdiff_t)count - 1;

    for (;;) {
        const ptrdiff_t partitionCount = high - low + 1;

        if (partitionCount <= CODUO_QSORT_CUTOFF) {
            coduo_qsort_compat_shortsort(bytes, low, high, width, compare);
            if (stackDepth == 0)
                return;

            --stackDepth;
            low = lowStack[stackDepth];
            high = highStack[stackDepth];
            continue;
        }

        {
            const ptrdiff_t originalLow = low;
            const ptrdiff_t originalHigh = high;
            ptrdiff_t middle = low + partitionCount / 2;
            ptrdiff_t lowCursor;
            ptrdiff_t highCursor;

            if (compare(bytes + (size_t)low * width,
                        bytes + (size_t)middle * width) > 0) {
                coduo_qsort_compat_swap_bytes(
                    bytes + (size_t)low * width,
                    bytes + (size_t)middle * width, width);
            }
            if (compare(bytes + (size_t)low * width,
                        bytes + (size_t)high * width) > 0) {
                coduo_qsort_compat_swap_bytes(bytes + (size_t)low * width,
                                              bytes + (size_t)high * width,
                                              width);
            }
            if (compare(bytes + (size_t)middle * width,
                        bytes + (size_t)high * width) > 0) {
                coduo_qsort_compat_swap_bytes(
                    bytes + (size_t)middle * width,
                    bytes + (size_t)high * width, width);
            }

            lowCursor = low;
            highCursor = high;
            for (;;) {
                if (middle > lowCursor) {
                    do {
                        ++lowCursor;
                    } while (lowCursor < middle &&
                             compare(bytes + (size_t)lowCursor * width,
                                     bytes + (size_t)middle * width) <= 0);
                }
                if (middle <= lowCursor) {
                    do {
                        ++lowCursor;
                    } while (lowCursor <= high &&
                             compare(bytes + (size_t)lowCursor * width,
                                     bytes + (size_t)middle * width) <= 0);
                }

                do {
                    --highCursor;
                } while (highCursor > middle &&
                         compare(bytes + (size_t)highCursor * width,
                                 bytes + (size_t)middle * width) > 0);

                if (lowCursor > highCursor)
                    break;

                if (lowCursor != highCursor) {
                    coduo_qsort_compat_swap_bytes(
                        bytes + (size_t)lowCursor * width,
                        bytes + (size_t)highCursor * width, width);
                }
                if (middle == highCursor)
                    middle = lowCursor;
            }

            ++highCursor;
            if (middle < highCursor) {
                do {
                    --highCursor;
                } while (highCursor > middle &&
                         compare(bytes + (size_t)highCursor * width,
                                 bytes + (size_t)middle * width) == 0);
            }
            if (middle >= highCursor) {
                do {
                    --highCursor;
                } while (highCursor > originalLow &&
                         compare(bytes + (size_t)highCursor * width,
                                 bytes + (size_t)middle * width) == 0);
            }

            if (highCursor - originalLow >= originalHigh - lowCursor) {
                if (originalLow < highCursor) {
                    lowStack[stackDepth] = originalLow;
                    highStack[stackDepth] = highCursor;
                    ++stackDepth;
                }
                if (lowCursor < originalHigh) {
                    low = lowCursor;
                    high = originalHigh;
                    continue;
                }
            } else {
                if (lowCursor < originalHigh) {
                    lowStack[stackDepth] = lowCursor;
                    highStack[stackDepth] = originalHigh;
                    ++stackDepth;
                }
                if (originalLow < highCursor) {
                    low = originalLow;
                    high = highCursor;
                    continue;
                }
            }
        }

        if (stackDepth == 0)
            return;

        --stackDepth;
        low = lowStack[stackDepth];
        high = highStack[stackDepth];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: shared server-code dispatch. Client-only callers
 * use coduo_crt_qsort directly and therefore have no host-qsort path. */
void coduo_qsort(void *base, size_t count, size_t width,
                 int (*compare)(const void *, const void *))
{
#if defined(WINDOWS_BEHAVIOR)
    coduo_crt_qsort(base, count, width, compare);
#else
    /* The original Linux engine and game module import qsort@GLIBC_2.0. The
     * symbol version proves the ABI floor, not one historical glibc sorting
     * algorithm or a stable order for comparator-equal records. */
    qsort(base, count, width, compare);
#endif
}
