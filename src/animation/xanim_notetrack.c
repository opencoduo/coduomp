#include "xanim.h"

/*
 * The authoritative Windows and Linux bodies perform the same notetrack
 * search and return the selected eight-byte row index as an unsigned word:
 *
 *   CoDUOMP.exe   0x00498450..0x004984af
 *   coduo_lnxded  0x080bba7e..0x080bbb04
 *
 * Linux 0x080bbaff and Mac PEF 0x000eee48 explicitly zero-extend the low
 * 16 bits. All seven Windows callers consume only AX, so the upper EAX bits
 * left by the Win32 body are outside the return contract.
 */
uint16_t XAnimGetNextNotifyTime(XAnimEntry *entry, XAnimInfo *node,
                                float time)
{
    (void)node;

    xanim_notetrack_t *notetracks =
        entry->payload.leafAsset->data.xanimParts->noteTracks;
    xanim_notetrack_t *best = NULL;
    float bestTime = 2.0f;

    for (xanim_notetrack_t *cursor = notetracks;
         cursor->nameHandle != 0; ++cursor) {
        if (time <= cursor->time && cursor->time < bestTime) {
            best = cursor;
            bestTime = cursor->time;
        }
    }

    if (best == NULL) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        uint32_t byteDelta =
            UINT32_C(0) - (uint32_t)(uintptr_t)(const void *)notetracks;
        uint32_t shifted = byteDelta >> 3;

        if ((byteDelta & UINT32_C(0x80000000)) != 0) {
            shifted |= UINT32_C(0xe0000000);
        }
        return (uint16_t)shifted;
    }

    return (uint16_t)(best - notetracks);
}
