#ifndef BG_STATE_H
#define BG_STATE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "recovered_game.h"

/* NOT_FROM_ORIGINAL_SOURCE: maintained accessor for host-safe recovered table reads. */
static inline uint32_t game_compat_bg_read_u32_at_offset(const void *base, size_t offset)
{
    uint32_t value;

    memcpy(&value, (const uint8_t *)(const void *)base + offset,
           sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained accessor for generated animation-slot word reads. */
static inline uint32_t game_compat_bg_anim_slot_animation_word(const void *clientInfo,
                                                size_t slotOffset)
{
    return game_compat_bg_read_u32_at_offset(clientInfo,
                              slotOffset +
                                  offsetof(bg_anim_slot_t, animationWord));
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained accessor for generated animation-slot word reads. */
static inline uint32_t game_compat_bg_anim_slot_animation_word_from_slot(const void *slot)
{
    return game_compat_bg_read_u32_at_offset(
        slot, offsetof(bg_anim_slot_t, animationWord));
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained accessor for generated animation-entry reference reads. */
static inline uint32_t game_compat_bg_anim_slot_animation_reference(const void *clientInfo,
                                                     size_t slotOffset)
{
    return game_compat_bg_read_u32_at_offset(clientInfo,
                              slotOffset +
                                  offsetof(bg_anim_slot_t, animationOffset));
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained accessor for generated animation-entry reference reads. */
static inline uint32_t game_compat_bg_anim_slot_animation_reference_from_slot(const void *slot)
{
    return game_compat_bg_read_u32_at_offset(
        slot, offsetof(bg_anim_slot_t, animationOffset));
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained accessor for original-i386 pointer/native-offset animation references. */
static inline const bg_static_animation_t *game_compat_bg_static_animation_from_reference(
    const bg_static_animation_table_t *table, uint32_t animationReference)
{
#if UINTPTR_MAX == UINT32_MAX
    const bg_static_animation_t *animation;

    /* Windows 0x2002dbdf..0x2002dbf1 and Linux 0x5ac5c..0x5ac75 load the
     * slot +0x14 word as a pointer, then read the flags at pointer +0x50. */
    (void)table;
    memcpy(&animation, &animationReference, sizeof(animation));
    return animation;
#else
    /* Native 64-bit builds keep the original 0x30-byte slot layout and store
     * this reference as a byte offset from the widened animation table. */
    return (const bg_static_animation_t *)(const void *)
        ((const uint8_t *)(const void *)table + animationReference);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained accessor for generated static-animation flag reads. */
static inline uint32_t game_compat_bg_static_animation_flags_from_reference(
    const bg_static_animation_table_t *table, uint32_t animationReference)
{
    return game_compat_bg_static_animation_from_reference(
        table, animationReference)->flags;
}

/*
 * bg_t -- small 12-byte global timing state.
 *
 * Binary symbol: `bg` at ELF RVA 0x136a60, size 12.
 * Ghidra address range: 0x146a60 .. 0x146a6b (imagebase 0x10000).
 * G_InitGame memsets it to zero; G_RunFrame writes all three fields.
 *
 * RECOVERED(UO-GAME-UNK-0142): field names are recovered.
 */
typedef struct bg_s {
    int32_t time;                        /* +0x00, set to levelTime */
    int32_t levelFrameTime;              /* +0x04, DAT_00146a64, set to levelTime */
    int32_t frameTime;                   /* +0x08, DAT_00146a68, frame delta msec */
} bg_t;

/*
 * Partial bgs animation-prefix map.
 *
 * Binary symbol: `bgs` at ELF RVA 0x136a80, size 0xBAEF4 (765684 bytes).
 * Ghidra address range: 0x146a80 .. 0x201973 (imagebase 0x10000).
 * The complete global is declared as bgs_t in game_globals.h: this file maps
 * the animation prefix that starts at bgs+0. The client-info tail begins at
 * the original i386 offset +0xa7af4 and is typed as bgs_t::clientinfo.
 *
 * NOTE: The DAT_0024xxxx addresses from the decompiler are NOT bgs offsets.
 * They are level fields (level is at Ghidra 0x24b520). Only the large-offset
 * accesses like bgs._686808_ are genuine bgs fields.
 *
 * RECOVERED(UO-GAME-UNK-0143): offsets confirmed from decompiler output.
 */

#endif /* BG_STATE_H */
