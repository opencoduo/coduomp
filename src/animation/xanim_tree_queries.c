#include "xanim.h"

enum {
    XANIM_SYNC_LOOPING = 1,
    XANIM_SYNC_NON_LOOPING = 2,
    XANIM_SYNC_MASK = XANIM_SYNC_LOOPING | XANIM_SYNC_NON_LOOPING
};

/* Sources: CoDUOMP.exe 0x0049ac50 and coduo_lnxded 0x080bf082.
 * Name: exact same-version Mac symbol XAnimGetNumChildren. */
int32_t XAnimGetNumChildren(XAnim *tree, int32_t animIndex)
{
    return tree->entries[animIndex].childCount;
}

/* Sources: CoDUOMP.exe 0x0049ac60 and coduo_lnxded 0x080bf092.
 * Name: exact same-version Mac symbol XAnimGetChildAt. */
int32_t XAnimGetChildAt(XAnim *tree, int32_t animIndex,
                        int32_t childIndex)
{
    return tree->entries[animIndex].payload.parent.firstChildIndex +
           childIndex;
}

/* Sources: CoDUOMP.exe 0x0049ac70 and coduo_lnxded 0x080bf0a6.
 * Name: exact same-version Mac symbol XAnimGetAnimName. */
const char *XAnimGetAnimName(XAnim *tree, int32_t animIndex)
{
    XAnimEntry *entry = &tree->entries[animIndex];

    return entry->childCount != 0
               ? "<non-leaf anim>"
               : entry->payload.leafAsset->name;
}

/* Sources: CoDUOMP.exe 0x00496df0 and coduo_lnxded 0x080bf0de.
 * Name: exact same-version Mac symbol XAnimRuntimeTreeSourceTree. */
XAnim *XAnimRuntimeTreeSourceTree(XAnimTree *runtimeTree)
{
    return runtimeTree->sourceTree;
}

/* Linux has a standalone body at coduo_lnxded 0x080bf0e8. MSVC inlined the
 * equivalent node-count load at both CoDUOMP.exe VM dispatch sites; the exact
 * same-version Mac symbol supplies the retained name XAnimGetAnimTreeSize. */
int32_t XAnimGetAnimTreeSize(XAnim *tree)
{
    return (int32_t)tree->nodeCount;
}

/* Sources: CoDUOMP.exe 0x0049c0f0 and coduo_lnxded 0x080c0ac4.
 * Name: exact same-version Mac symbol XAnimHasTime. */
qboolean XAnimHasTime(XAnim *tree, int32_t animIndex)
{
    XAnimEntry *entry = &tree->entries[animIndex];

    return entry->childCount == 0 ||
           (entry->payload.parent.flags & XANIM_SYNC_MASK) != 0;
}

/* Sources: CoDUOMP.exe 0x0049c110 and coduo_lnxded 0x080c0b06.
 * Name: exact same-version Mac symbol XAnimIsPrimitive. */
qboolean XAnimIsPrimitive(XAnim *tree, int32_t animIndex)
{
    return tree->entries[animIndex].childCount == 0 ? qtrue : qfalse;
}

/* Sources: CoDUOMP.exe 0x0049c590 and coduo_lnxded 0x080c1142.
 * Both leaf paths return the serialized loop byte without normalizing it;
 * Linux's former qtrue/qfalse source reconstruction lost that distinction.
 * Name: exact same-version Mac symbol XAnimIsLooped. */
qboolean XAnimIsLooped(XAnim *tree, int32_t animIndex)
{
    XAnimEntry *entry = &tree->entries[animIndex];

    return entry->childCount != 0
               ? (entry->payload.parent.flags & XANIM_SYNC_LOOPING) != 0
               : entry->payload.leafAsset->data.xanimParts->looped;
}

/* Sources: CoDUOMP.exe 0x0049c5b0 and coduo_lnxded 0x080c1186.
 * Name: exact same-version Mac symbol XAnimNotetrackExists. */
qboolean XAnimNotetrackExists(XAnim *tree, int32_t animIndex,
                              uint16_t notetrack)
{
    xanim_notetrack_t *noteTrack =
        tree->entries[animIndex].payload.leafAsset->data.xanimParts
            ->noteTracks;

    if (noteTrack != NULL) {
        for (; noteTrack->nameHandle != 0; ++noteTrack) {
            if (noteTrack->nameHandle == notetrack) {
                return qtrue;
            }
        }
    }

    return qfalse;
}

/*
 * Sources: CoDUOMP.exe 0x0049ab80..0x0049ab95 and coduo_lnxded
 * 0x080beef2..0x080bef2e.  Both bodies zero-extend the binary uint16_t frame
 * count, load it with FILD DWORD, divide by the binary32 frame rate with FDIV,
 * and return the quotient in ST0.  Name: exact same-version Mac symbol
 * XAnimGetLength.
 *
 * The operation graph is shared, but the process x87 precision control is not:
 * Windows uses PC=53 and Linux uses PC=64.  The platform return carrier in
 * xanim.h preserves that live value until each caller's original narrowing
 * point.  The stores below are exact carrier transfers, not arithmetic
 * narrowing: binary64 holds the complete PC=53 Windows result, and native
 * long double holds the complete PC=64 Linux result.
 */
xanim_length_t XAnimGetLength(XAnim *tree, int32_t animIndex)
{
    const XAnimParts *record =
        tree->entries[animIndex].payload.leafAsset->data.xanimParts;
    const int32_t frameCountMinusOne = record->frameCountMinusOne;

#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
    volatile double length =
        (double)frameCountMinusOne / (double)record->frameRate;

    return length;
#else
    return x87f_div(x87f_load_i32(frameCountMinusOne),
                    x87f_load_f32(record->frameRate));
#endif
#elif (defined(__i386__) || defined(__x86_64__)) && \
      (defined(__GNUC__) || defined(__clang__))
#if defined(WINDOWS_BEHAVIOR)
    double length;

    __asm__ __volatile__("fildl %[frameCount]\n\t"
                         "fdivs %[frameRate]\n\t"
                         "fstpl %[length]"
                         : [length] "=m"(length)
                         : [frameCount] "m"(frameCountMinusOne),
                           [frameRate] "m"(record->frameRate)
                         : "st", "memory");
    return length;
#else
    long double length;

    __asm__ __volatile__("fildl %[frameCount]\n\t"
                         "fdivs %[frameRate]\n\t"
                         "fstpt %[length]"
                         : [length] "=m"(length)
                         : [frameCount] "m"(frameCountMinusOne),
                           [frameRate] "m"(record->frameRate)
                         : "st", "memory");
    return length;
#endif
#else
    /* Explicitly non-faithful EMULATE_X87=0 builds on non-x87 targets use the
     * widest native carrier available. */
#if defined(WINDOWS_BEHAVIOR)
    return (double)frameCountMinusOne / (double)record->frameRate;
#else
    return (long double)frameCountMinusOne /
           (long double)record->frameRate;
#endif
#endif
}
