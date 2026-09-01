#include "sound_alias_private.h"

#include "sound_alias_runtime_services.h"
#include "compat/crt/random_compat.h"

enum { SOUND_ALIAS_REPICK_MINIMUM = 3 };

/*
 * Complete common weighted-selection and anti-repeat loop.  The original
 * operation graph and record walk agree in CoDUOMP.exe
 * 0x00438300..0x00438552 and coduo_lnxded 0x0806e6a2..0x0806e84f.
 *
 * The original build configurations supply different selector and CRT-random
 * boundaries: the client uses listener distance and MSVC's
 * 15-bit rand, while dedicated uses LOD 10 and glibc rand with its retained
 * negative scale constant.  Those inputs stay target-local; the loop below
 * does not homogenize them.
 */
snd_alias_t *Com_PickSoundAlias(const char *name,
                                sndAliasBank_t bank,
                                const vec3_t origin)
{
    const float selector = sound_alias_compat_pick_selector(origin);
    snd_alias_t *const firstAlias =
        Com_FindSoundAlias(name, bank, selector);
    if (firstAlias == NULL) {
        return NULL;
    }

    snd_alias_t *selectedAlias = firstAlias;
    float totalWeight = firstAlias->selectionWeight;
    int32_t matchingCount = 1;
    int32_t maxPickSequence = firstAlias->pickSequence;
    snd_alias_t *const tableBase = com_soundAliases[bank];

    snd_alias_t *cursor = firstAlias;
    while (cursor != tableBase) {
        --cursor;
        if (cursor->aliasName != firstAlias->aliasName) {
            break;
        }

        if (Com_ValidateSoundAliasLOD(cursor, selector)) {
            ++matchingCount;
            totalWeight += cursor->selectionWeight;
            if (cursor->selectionWeight *
                    SOUND_ALIAS_COMPAT_RANDOM_SCALE >
                (float)coduo_server_rand() * totalWeight) {
                selectedAlias = cursor;
            }

            if (maxPickSequence < cursor->pickSequence) {
                maxPickSequence = cursor->pickSequence;
            }
        }
    }

    if (matchingCount >= SOUND_ALIAS_REPICK_MINIMUM &&
        maxPickSequence == selectedAlias->pickSequence) {
        totalWeight = 0.0f;
        cursor = firstAlias;

        int32_t remainingMatches = matchingCount;
        while (remainingMatches != 0) {
            if (Com_ValidateSoundAliasLOD(cursor, selector)) {
                if (maxPickSequence != cursor->pickSequence) {
                    totalWeight += cursor->selectionWeight;
                    if (cursor->selectionWeight *
                            SOUND_ALIAS_COMPAT_RANDOM_SCALE >
                        (float)coduo_server_rand() * totalWeight) {
                        selectedAlias = cursor;
                    }
                }
                --remainingMatches;
            }

            /* Both originals form one final unconsumed one-before-array
             * address. Do not form that invalid pointer in portable C. */
            if (remainingMatches != 0) {
                --cursor;
            }
        }
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    selectedAlias->pickSequence = maxPickSequence + 1;
    return selectedAlias;
}
