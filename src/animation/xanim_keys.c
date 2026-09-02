#include "xanim.h"

#include "compat/coduo_native_x87.h"

#if defined(EMULATE_X87) && EMULATE_X87
#include "compat/coduo_x87emu.h"
#endif

/*
 * The Windows client and Linux dedicated server retain the same key-search
 * graph for both key widths:
 *
 *   CoDUOMP.exe  XAnimFindByteKey   0x00496f00..0x00496f76
 *   CoDUOMP.exe  XAnimFindShortKey  0x00496f80..0x00496ff6
 *   coduo_lnxded XAnimFindByteKey   0x080b9914..0x080b9a30
 *   coduo_lnxded XAnimFindShortKey  0x080b9a32..0x080b9b71
 *
 * Each body seeds the search by multiplying the binary32 frame fraction by
 * the signed key count in x87 and truncating the retained product to int32.
 * Windows reaches the truncating conversion through its CRT helper; Linux
 * changes the x87 rounding control, performs FISTP, and restores the control
 * word inline.  The source operation is common, while the existing x87
 * adapters preserve those target/compiler lowering requirements.
 */
int32_t XAnimFindByteKey(float frameFrac, const uint8_t *keys,
                         int32_t keyCount, int32_t targetKey)
{
    int32_t low = 0;
    int32_t index;

#if defined(EMULATE_X87) && EMULATE_X87
    index = x87f_store_i32_trunc(
        x87f_mul(x87f_load_i32(keyCount), x87f_load_f32(frameFrac)));
#elif defined(__x86_64__) && \
      (defined(__GNUC__) || defined(__clang__))
    index = CODUO_X87_TRUNCATE_I32(
        (long double)keyCount * (long double)frameFrac);
#else
    index = (int32_t)(keyCount * frameFrac);
#endif

    if (targetKey < keys[index]) {
        do {
            do {
                keyCount = index;
                index = (low + keyCount) / 2;
            } while (targetKey < keys[index]);
            if (targetKey < keys[index + 1]) {
                return index;
            }
            low = index + 1;
            index = keyCount - 1;
        } while (targetKey < keys[index]);
    } else if (keys[index + 1] <= targetKey) {
        int32_t scan = index + 1;

        do {
            while (low = scan, scan = (low + keyCount) / 2,
                   keys[scan] <= targetKey) {
                if (targetKey < keys[scan + 1]) {
                    return scan;
                }
                ++scan;
            }
            keyCount = scan;
            scan = low + 1;
        } while (keys[low + 1] <= targetKey);
        index = low;
    }

    return index;
}

int32_t XAnimFindShortKey(float frameFrac, const uint16_t *keys,
                          int32_t keyCount, int32_t targetKey)
{
    int32_t low = 0;
    int32_t index;

#if defined(EMULATE_X87) && EMULATE_X87
    index = x87f_store_i32_trunc(
        x87f_mul(x87f_load_i32(keyCount), x87f_load_f32(frameFrac)));
#elif defined(__x86_64__) && \
      (defined(__GNUC__) || defined(__clang__))
    index = CODUO_X87_TRUNCATE_I32(
        (long double)keyCount * (long double)frameFrac);
#else
    index = (int32_t)(keyCount * frameFrac);
#endif

    if (targetKey < keys[index]) {
        do {
            do {
                keyCount = index;
                index = (low + keyCount) / 2;
            } while (targetKey < keys[index]);
            if (targetKey < keys[index + 1]) {
                return index;
            }
            low = index + 1;
            index = keyCount - 1;
        } while (targetKey < keys[index]);
    } else if (keys[index + 1] <= targetKey) {
        int32_t scan = index + 1;

        do {
            while (low = scan, scan = (low + keyCount) / 2,
                   keys[scan] <= targetKey) {
                if (targetKey < keys[scan + 1]) {
                    return scan;
                }
                ++scan;
            }
            keyCount = scan;
            scan = low + 1;
        } while (keys[low + 1] <= targetKey);
        index = low;
    }

    return index;
}
