#include "animation_private.h"

#include "compat/coduo_fp_platform.h"

#if defined(LINUX_BEHAVIOR) && EMULATE_X87
#include "compat/coduo_x87emu.h"
#endif

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The authoritative client and dedicated-server bodies have the same branch
 * graph and the same unspilled x87 operation graph on every calculated path:
 *
 *   CoDUOMP.exe   0x004984b0..0x0049858c
 *   coduo_lnxded  0x080bbb04..0x080bbc44
 *
 * Each path executes FILD frameDelta, FLD notifyTime, FSUB startTime, FADDP,
 * and FDIV timeStep.  The surrounding process control word makes that chain
 * PC=53 on Windows and PC=64 on Linux.  Linux then stores the result to
 * binary32 before returning.  Windows leaves it in ST0: six direct notetrack
 * callers store it to binary32, but XAnimGetNotifyFracServer forwards the live
 * value into paths that can compare or multiply it before a store.  The shared
 * carrier therefore retains PC=53 width for Windows and performs the original
 * binary32 narrowing only for Linux.
 */
xanim_notify_fraction_t XAnimGetNotifyFracLeaf(float notifyTime)
{
    int32_t frameDelta;

    if (xanim_evalWindowTime == 1.0f) {
        return 1.0f;
    }

    if (xanim_evalCurrentTime < xanim_evalWindowTime) {
        if (notifyTime < xanim_evalCurrentTime) {
            frameDelta = (int32_t)xanim_evalWindowFrame - (int32_t)xanim_evalStartFrame + 1;
        } else if (xanim_evalWindowTime <= notifyTime) {
            frameDelta = (int32_t)xanim_evalWindowFrame - (int32_t)xanim_evalStartFrame;
        } else {
            return 1.0f;
        }
    } else {
        if ((xanim_evalCurrentTime <= notifyTime && xanim_evalCurrentTime != 1.0f) || notifyTime < xanim_evalWindowTime) {
            return 1.0f;
        }

        frameDelta = (int32_t)xanim_evalWindowFrame - (int32_t)xanim_evalStartFrame;
    }

#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
    /* Binary64 has the same 53-bit significand selected by the original
     * Win32 x87 control word.  Volatile stores make each original arithmetic
     * instruction's precision boundary explicit on SSE and NEON. */
    volatile double fraction = (double)notifyTime - (double)xanim_evalStartTime;
    fraction = (double)frameDelta + fraction;
    fraction /= (double)xanim_evalTimeStep;
    return fraction;
#else
    x87f frameValue = x87f_load_i32(frameDelta);
    x87f fraction = x87f_load_f32(notifyTime);

    fraction = x87f_sub(fraction, x87f_load_f32(xanim_evalStartTime));
    fraction = x87f_add(frameValue, fraction);
    fraction = x87f_div(fraction, x87f_load_f32(xanim_evalTimeStep));
    return x87f_store_f32(fraction);
#endif
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
#if defined(WINDOWS_BEHAVIOR)
    double fraction;

    /* Preserve the live PC=53 value.  Its significand already fits binary64,
     * and this float-derived calculation cannot approach binary64's exponent
     * limits, so the carrier store is exact. */
    __asm__ __volatile__("fildl %[frameDelta]\n\t"
                         "flds %[notifyTime]\n\t"
                         "fsubs %[startTime]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fdivs %[timeStep]\n\t"
                         "fstpl %[fraction]"
                         : [fraction] "=m"(fraction)
                         : [frameDelta] "m"(frameDelta), [notifyTime] "m"(notifyTime), [startTime] "m"(xanim_evalStartTime),
                           [timeStep] "m"(xanim_evalTimeStep)
                         : "st", "st(1)", "memory");
    return fraction;
#else
    float fraction;

    /* The native Linux control word supplies PC=64.  FSTPS implements its
     * original binary32 return spill. */
    __asm__ __volatile__("fildl %[frameDelta]\n\t"
                         "flds %[notifyTime]\n\t"
                         "fsubs %[startTime]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fdivs %[timeStep]\n\t"
                         "fstps %[fraction]"
                         : [fraction] "=m"(fraction)
                         : [frameDelta] "m"(frameDelta), [notifyTime] "m"(notifyTime), [startTime] "m"(xanim_evalStartTime),
                           [timeStep] "m"(xanim_evalTimeStep)
                         : "st", "st(1)", "memory");
    return fraction;
#endif
#else
    /* Explicitly non-faithful EMULATE_X87=0 builds on non-x87 targets retain
     * the source operation graph.  Normal builds on those targets select the
     * software-x87 path above. */
#if defined(WINDOWS_BEHAVIOR)
    double fraction = (double)notifyTime - (double)xanim_evalStartTime;
    fraction = (double)frameDelta + fraction;
    fraction /= (double)xanim_evalTimeStep;
    return fraction;
#else
    long double fraction = (long double)notifyTime - (long double)xanim_evalStartTime;
    fraction = (long double)frameDelta + fraction;
    fraction /= (long double)xanim_evalTimeStep;
    volatile float narrowedFraction = (float)fraction;
    return narrowedFraction;
#endif
#endif
}

/*
 * The forwarding bodies agree in every authoritative instruction-level
 * dependency after normalizing the already-shared fileData_t/XAnimParts view:
 * CoDUOMP.exe 0x00498590..0x00498614 and coduo_lnxded
 * 0x080bbc46..0x080bbcf4.
 */
xanim_notify_fraction_t XAnimGetNotifyFracServer(XAnimInfo *node, XAnimEntry *entry)
{
    if (xanim_evalRootHandle == 0 || node->notifyName == 0) {
        return 1.0f;
    }

    if (entry->childCount != 0) {
        if (node->notifyChildIndex == 0) {
            return XAnimGetNotifyFracLeaf(1.0f);
        }
        entry = &xanim_currentTree->sourceTree->entries[node->notifyChildIndex];
    }

    float notifyTime = node->notifyIndex < 0 ? 1.0f : entry->payload.leafAsset->data.xanimParts->noteTracks[node->notifyIndex].time;
    return XAnimGetNotifyFracLeaf(notifyTime);
}
