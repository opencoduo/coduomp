#include "bg_animation.h"
#include "bg_animation_services.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Original Windows bodies are instruction-for-instruction equivalents after
 * their module-owned error edge and absolute globals are normalized:
 *
 *   uo_cgame_mp_x86.dll  0x300015a0..0x300015e6
 *   uo_game_mp_x86.dll   0x20001590..0x200015d4
 *
 * Supporting Mac cgame/game both retain the canonical
 * BG_LoadAnimForAnimIndex name and an 0x8c-byte body. Linux game carries the
 * same range check and 0x48-byte runtime-animation scan at RVA 0x00019f5b.
 */
bg_runtime_animation_t *BG_LoadAnimForAnimIndex(uint32_t animIndex)
{
    const int32_t entryCount = bgAnimStaticTable->entryCount;
    int32_t runtimeCount;
    bg_runtime_animation_t *animation;
    int32_t index;

    if (animIndex >= (uint32_t)entryCount) {
        bg_compat_anim_index_error(animIndex, entryCount);
    }

    runtimeCount = *bgRuntimeAnimationCount;
    animation = bgRuntimeAnimations;
    for (index = 0; index < runtimeCount; ++index, ++animation) {
        if (animIndex == (uint32_t)animation->anim.animIndex) {
            return animation;
        }
    }

    return NULL;
}
