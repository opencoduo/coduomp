#ifndef QCOMMON_XANIM_TYPES_H
#define QCOMMON_XANIM_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "file_data.h"
#include "q_vector_types.h"

enum {
    XANIM_ROOT_NODE_INDEX = 0,
    XANIM_SMALL_FRAME_KEY_LIMIT = 256,
    XANIM_PART_REMAP_TABLE_COUNT = 2,
    XANIM_POOL_NODE_COUNT = 2048,
    XANIM_STATE_SLOT_COUNT = 2,
    XANIM_RUNTIME_TREE_TAIL_WORD_COUNT = 3,
    XANIM_DEFERRED_NOTIFY_CAPACITY = 128,
    SCR_ANIM_TREE_INDEX_SHIFT = 16
};

/* Each source-tree node owns three trailing uint16_t words after the runtime
 * handle array.  Both original engines consequently allocate a 6-byte tail
 * span per node plus the final two-byte terminator. */
typedef struct xanim_runtime_tree_tail_span_s {
    uint16_t words[XANIM_RUNTIME_TREE_TAIL_WORD_COUNT];
} xanim_runtime_tree_tail_span_t;

typedef char xanim_runtime_tree_tail_span_size[sizeof(xanim_runtime_tree_tail_span_t) == 0x06 ? 1 : -1];

typedef enum xanimUser_e {
    XANIM_USER_CLIENT = 0,
    XANIM_USER_SERVER = 1
} xanimUser_t;

/* Packed script animation reference shared by the engine, cgame, and game
 * module.  The exact scr_anim_s tag is retained by the Mac assignment symbol.
 * CoDUOMP.exe Scr_GetAnim (RVA 0x0008ec90) loads the value as one dword, uses
 * its low half as animIndex, and shifts the high half into treeIndex.  The
 * Linux game consumer at RVA 0x000676c6 receives the same four-byte value
 * through the platform's hidden struct-return pointer. */
typedef struct scr_anim_s {
    uint16_t animIndex;
    uint16_t treeIndex;
} scr_anim_t;

typedef char xanim_scr_anim_index_offset[offsetof(scr_anim_t, animIndex) == 0x00 ? 1 : -1];
typedef char xanim_scr_anim_tree_index_offset[offsetof(scr_anim_t, treeIndex) == 0x02 ? 1 : -1];
typedef char xanim_scr_anim_size[sizeof(scr_anim_t) == 0x04 ? 1 : -1];

enum {
    BG_RUNTIME_ANIMATION_NAME_SIZE = 64
};

/* Runtime animation registration record shared by cgame and game.  The
 * Windows cgame body at 0x300012a0 and Windows game body at 0x20001290 have
 * the same instruction graph: they advance records by 0x48, read and write
 * the hash at +0x04, compare/copy the inline name at +0x08, and pass the
 * record base to Scr_FindAnim for the leading scr_anim_t.  Linux game at
 * RVA 0x00019b3b uses the same map.  The supporting Mac cgame/game
 * BG_AnimationIndexForString and BG_LoadAnimForAnimIndex bodies independently
 * use the same 72-byte stride and +0x00/+0x04/+0x08 accesses. */
typedef struct bg_runtime_animation_s {
    scr_anim_t anim;
    int32_t hash;
    char name[BG_RUNTIME_ANIMATION_NAME_SIZE];
} bg_runtime_animation_t;

typedef char xanim_bg_runtime_animation_anim_offset[offsetof(bg_runtime_animation_t, anim) == 0x00 ? 1 : -1];
typedef char xanim_bg_runtime_animation_hash_offset[offsetof(bg_runtime_animation_t, hash) == 0x04 ? 1 : -1];
typedef char xanim_bg_runtime_animation_name_offset[offsetof(bg_runtime_animation_t, name) == 0x08 ? 1 : -1];
typedef char xanim_bg_runtime_animation_name_size[sizeof(((bg_runtime_animation_t *)0)->name) == BG_RUNTIME_ANIMATION_NAME_SIZE ? 1 : -1];
typedef char xanim_bg_runtime_animation_size[sizeof(bg_runtime_animation_t) == 0x48 ? 1 : -1];

/* Signed fixed-point rotation components.  CoDUOMP.exe XAnimLoadFile at
 * 0x00495ed0 and coduo_lnxded XAnimLoadFile at 0x080b816e use the same
 * two- and four-halfword frame records. */
typedef struct xanim_int16_vec2_s {
    int16_t components[2];
} xanim_int16_vec2_t;

typedef struct xanim_int16_vec4_s {
    int16_t components[4];
} xanim_int16_vec4_t;

/* Parsed notetrack entries have an eight-byte stride in both loaders.  The
 * Linux ReadNoteTracks body at 0x080b8058 and Windows body at 0x00495e10
 * write the name handle at +0x00 and normalized time at +0x04. */
typedef struct xanim_notetrack_s {
    uint16_t nameHandle;
    uint8_t padding02[2];
    float time;
} xanim_notetrack_t;

/* A one-frame rotation stores its first two signed lanes inline.  A
 * multi-frame rotation overlays the same four bytes with a frame pointer. */
typedef union xanim_rotation_stream_data_u {
    xanim_int16_vec4_t *frames4;
    xanim_int16_vec2_t *frames2;
    struct {
        int16_t lane0;
        int16_t lane1;
    } inlinePrefix;
} xanim_rotation_stream_data_t;

/* At +0x06, a one-frame four-lane rotation stores lanes 2 and 3.  A keyed
 * multi-frame rotation instead starts its variable-length key list here. */
typedef union xanim_rotation_stream_tail_u {
    struct {
        int16_t lane2;
        int16_t lane3;
    } inlineFull;
    uint8_t byteKeys[1];
    uint16_t shortKeys[1];
} xanim_rotation_stream_tail_t;

/* Both shipped i386 loaders allocate the complete inline quaternion record as
 * exactly 0x0a bytes (Windows 0x00496666 and Linux 0x080b8c90).  Pack(2)
 * preserves that common source layout rather than admitting a compiler-
 * rounded 0x0c tail. */
#pragma pack(push, 2)
typedef struct xanim_rotation_stream_s {
    xanim_rotation_stream_data_t data;
    uint16_t frameIndex;
    xanim_rotation_stream_tail_t tail;
} xanim_rotation_stream_t;
#pragma pack(pop)

/* A one-frame translation stores X inline.  A multi-frame translation
 * overlays it with the vec3 frame-array pointer. */
typedef union xanim_translation_stream_data_u {
    vec3_t *frames;
    float inlineLane0;
} xanim_translation_stream_data_t;

/* Variable-length frame keys begin at +0x06 in either byte or word form. */
typedef union xanim_translation_stream_key_u {
    uint8_t byteKeys[1];
    uint16_t shortKeys[1];
} xanim_translation_stream_key_t;

typedef struct xanim_translation_stream_inline_lanes_s {
    float lane1;
    float lane2;
} xanim_translation_stream_inline_lanes_t;

typedef struct xanim_translation_stream_s {
    xanim_translation_stream_data_t data;
    uint16_t frameIndex;
    xanim_translation_stream_key_t key;
    xanim_translation_stream_inline_lanes_t inlineLanes;
} xanim_translation_stream_t;

/* XAnimLoadFile allocates partCount * 8 bytes and stores this table at loaded
 * record +0x10 on both authoritative targets. */
typedef struct xanim_part_stream_pair_s {
    xanim_translation_stream_t *translation;
    xanim_rotation_stream_t *rotation;
} xanim_part_stream_pair_t;

/* Exact retail type name recovered from Mac XAnimParts signatures.  Windows
 * XAnimLoadFile at 0x00495fb8..0x00496033 and Linux at
 * 0x080b8341..0x080b8407 independently allocate 0x20 bytes and populate this
 * same field map. */
struct XAnimParts {
    uint16_t frameCountMinusOne;
    uint8_t looped;
    uint8_t hasDeltaMotion;
    float frameRate;
    float frequency;
    uint16_t *partNameHandles;
    xanim_part_stream_pair_t *partStreamPairs;
    xanim_notetrack_t *noteTracks;
    xanim_part_stream_pair_t *deltaMotion;
    uint8_t *compressedRotationBits;
};

/* Source-tree storage created after XAnimLoadFile.  Mac symbols distinguish
 * XAnimEntry from XAnim_s.  The authoritative Windows client bodies at
 * CoDUOMP.exe 0x00496d00, 0x00496d40, and 0x00496d80 and Linux server bodies
 * at coduo_lnxded 0x080b9570, 0x080b95e2, and 0x080b966a agree exactly:
 * an eight-byte source-tree header followed by eight-byte entries, with the
 * entry halfwords at +0x00/+0x02 and its pointer-or-halfword payload at +0x04.
 * Both parent-node bodies also stamp each child's parentIndex at entry +0x02. */
typedef struct XAnimEntry {
    uint16_t childCount;
    uint16_t parentIndex;
    union {
        fileData_t *leafAsset;
        struct {
            uint16_t flags;
            uint16_t firstChildIndex;
        } parent;
    } payload;
} XAnimEntry;

struct XAnim_s {
    const char *name;
    uint32_t nodeCount;
    XAnimEntry entries[];
};

/* Live evaluator wrapper, named XAnimTree_s by the Mac symbols.  Windows
 * XAnimAllocRuntimeTree at CoDUOMP.exe 0x00496da0 and Linux at coduo_lnxded
 * 0x080b969a both use a 12-byte prefix, a nodeCount-entry uint16_t handle
 * table, and the same trailing storage, for a total of 8 * nodeCount + 14
 * bytes on i386.  The first three fields are independently consumed at
 * +0x00, +0x04, and +0x08 on both targets.  Windows XAnimAllocInfo at
 * 0x0049b370 and XAnimFreeInfo at 0x00498180, and their Linux counterparts at
 * 0x080bf9be and 0x080bb6ae, prove the active count and 16-bit handle table.
 * Windows DObjCreate at 0x00494970 and Linux at 0x080c667a prove the remap
 * selector. */
struct XAnimTree_s {
    XAnim *sourceTree;
    int32_t activePoolNodeCount;
    int32_t partRemapTableSelector;
    uint16_t poolNodeHandles[];
};

/* Exact retail names recovered from the Mac XAnimLoadAnimState and
 * XAnimLoadAnimInfo signatures.  CoDUOMP.exe and coduo_lnxded agree on two
 * 0x1c-byte state lanes in one 0x44-byte pool record.  Both XAnimHasFinished
 * bodies compare the frame-cycle words with signed JG (Windows 0x0049ac44;
 * Linux 0x080bf06b), so the shared fields are signed 16-bit values. */
typedef struct XAnimState {
    float time;
    float oldTime;
    int16_t cycleCount;
    int16_t oldCycleCount;
    float weightBlendTimeRemaining;
    float targetWeight;
    float currentWeight;
    float rateScale;
} XAnimState;

typedef struct XAnimInfo {
    uint16_t notifyChildIndex;
    int16_t notifyIndex;
    uint16_t notifyName;
    uint16_t notifyType;
    uint16_t freePrev;
    uint16_t freeNext;
    XAnimState states[XANIM_STATE_SLOT_COUNT];
} XAnimInfo;

/* Both engines use the same 12-byte deferred-notify record.  The Windows
 * client name is retained for the word at +0x04; the Linux server transports
 * its script notify handle through the same notify-type field.  Windows cgame
 * CG_SetGunHandFromNotetracks consumes type 1 from this exact record domain. */
enum {
    XANIM_NOTIFY_CLIENT = 1
};

typedef struct xanim_deferred_notify_s {
    const char *name;
    uint16_t notifyType;
    uint8_t padding06[2];
    float timeFrac;
} xanim_deferred_notify_t;

#if UINTPTR_MAX == UINT32_MAX
#if defined(_MSC_VER)
#define XANIM_TYPES_ALIGNOF(type) __alignof(type)
#elif defined(__cplusplus)
#define XANIM_TYPES_ALIGNOF(type) alignof(type)
#else
#define XANIM_TYPES_ALIGNOF(type) __alignof__(type)
#endif
#define XANIM_TYPES_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

XANIM_TYPES_ASSERT(xanim_int16_vec2_alignment, XANIM_TYPES_ALIGNOF(xanim_int16_vec2_t) == 0x02);
XANIM_TYPES_ASSERT(xanim_scr_anim_alignment, XANIM_TYPES_ALIGNOF(scr_anim_t) == 0x02);
XANIM_TYPES_ASSERT(xanim_bg_runtime_animation_alignment, XANIM_TYPES_ALIGNOF(bg_runtime_animation_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_int16_vec2_components_offset, offsetof(xanim_int16_vec2_t, components) == 0x00);
XANIM_TYPES_ASSERT(xanim_int16_vec2_size, sizeof(xanim_int16_vec2_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_int16_vec4_alignment, XANIM_TYPES_ALIGNOF(xanim_int16_vec4_t) == 0x02);
XANIM_TYPES_ASSERT(xanim_int16_vec4_components_offset, offsetof(xanim_int16_vec4_t, components) == 0x00);
XANIM_TYPES_ASSERT(xanim_int16_vec4_size, sizeof(xanim_int16_vec4_t) == 0x08);

XANIM_TYPES_ASSERT(xanim_notetrack_alignment, XANIM_TYPES_ALIGNOF(xanim_notetrack_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_notetrack_name_offset, offsetof(xanim_notetrack_t, nameHandle) == 0x00);
XANIM_TYPES_ASSERT(xanim_notetrack_padding_offset, offsetof(xanim_notetrack_t, padding02) == 0x02);
XANIM_TYPES_ASSERT(xanim_notetrack_time_offset, offsetof(xanim_notetrack_t, time) == 0x04);
XANIM_TYPES_ASSERT(xanim_notetrack_size, sizeof(xanim_notetrack_t) == 0x08);

XANIM_TYPES_ASSERT(xanim_rotation_data_alignment, XANIM_TYPES_ALIGNOF(xanim_rotation_stream_data_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_rotation_data_frames4_offset, offsetof(xanim_rotation_stream_data_t, frames4) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_data_frames2_offset, offsetof(xanim_rotation_stream_data_t, frames2) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_data_inline_offset, offsetof(xanim_rotation_stream_data_t, inlinePrefix) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_data_lane0_offset, offsetof(xanim_rotation_stream_data_t, inlinePrefix.lane0) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_data_lane1_offset, offsetof(xanim_rotation_stream_data_t, inlinePrefix.lane1) == 0x02);
XANIM_TYPES_ASSERT(xanim_rotation_data_size, sizeof(xanim_rotation_stream_data_t) == 0x04);

XANIM_TYPES_ASSERT(xanim_rotation_tail_alignment, XANIM_TYPES_ALIGNOF(xanim_rotation_stream_tail_t) == 0x02);
XANIM_TYPES_ASSERT(xanim_rotation_tail_inline_offset, offsetof(xanim_rotation_stream_tail_t, inlineFull) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_tail_lane2_offset, offsetof(xanim_rotation_stream_tail_t, inlineFull.lane2) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_tail_lane3_offset, offsetof(xanim_rotation_stream_tail_t, inlineFull.lane3) == 0x02);
XANIM_TYPES_ASSERT(xanim_rotation_tail_byte_keys_offset, offsetof(xanim_rotation_stream_tail_t, byteKeys) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_tail_short_keys_offset, offsetof(xanim_rotation_stream_tail_t, shortKeys) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_tail_size, sizeof(xanim_rotation_stream_tail_t) == 0x04);

XANIM_TYPES_ASSERT(xanim_rotation_stream_alignment, XANIM_TYPES_ALIGNOF(xanim_rotation_stream_t) == 0x02);
XANIM_TYPES_ASSERT(xanim_rotation_stream_data_offset, offsetof(xanim_rotation_stream_t, data) == 0x00);
XANIM_TYPES_ASSERT(xanim_rotation_stream_frame_offset, offsetof(xanim_rotation_stream_t, frameIndex) == 0x04);
XANIM_TYPES_ASSERT(xanim_rotation_stream_tail_offset, offsetof(xanim_rotation_stream_t, tail) == 0x06);
XANIM_TYPES_ASSERT(xanim_rotation_stream_size, sizeof(xanim_rotation_stream_t) == 0x0a);

XANIM_TYPES_ASSERT(xanim_translation_data_alignment, XANIM_TYPES_ALIGNOF(xanim_translation_stream_data_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_translation_data_frames_offset, offsetof(xanim_translation_stream_data_t, frames) == 0x00);
XANIM_TYPES_ASSERT(xanim_translation_data_inline_offset, offsetof(xanim_translation_stream_data_t, inlineLane0) == 0x00);
XANIM_TYPES_ASSERT(xanim_translation_data_size, sizeof(xanim_translation_stream_data_t) == 0x04);

XANIM_TYPES_ASSERT(xanim_translation_key_alignment, XANIM_TYPES_ALIGNOF(xanim_translation_stream_key_t) == 0x02);
XANIM_TYPES_ASSERT(xanim_translation_key_byte_offset, offsetof(xanim_translation_stream_key_t, byteKeys) == 0x00);
XANIM_TYPES_ASSERT(xanim_translation_key_short_offset, offsetof(xanim_translation_stream_key_t, shortKeys) == 0x00);
XANIM_TYPES_ASSERT(xanim_translation_key_size, sizeof(xanim_translation_stream_key_t) == 0x02);

XANIM_TYPES_ASSERT(xanim_translation_inline_alignment, XANIM_TYPES_ALIGNOF(xanim_translation_stream_inline_lanes_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_translation_inline_lane1_offset, offsetof(xanim_translation_stream_inline_lanes_t, lane1) == 0x00);
XANIM_TYPES_ASSERT(xanim_translation_inline_lane2_offset, offsetof(xanim_translation_stream_inline_lanes_t, lane2) == 0x04);
XANIM_TYPES_ASSERT(xanim_translation_inline_size, sizeof(xanim_translation_stream_inline_lanes_t) == 0x08);

XANIM_TYPES_ASSERT(xanim_translation_stream_alignment, XANIM_TYPES_ALIGNOF(xanim_translation_stream_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_translation_stream_data_offset, offsetof(xanim_translation_stream_t, data) == 0x00);
XANIM_TYPES_ASSERT(xanim_translation_stream_frame_offset, offsetof(xanim_translation_stream_t, frameIndex) == 0x04);
XANIM_TYPES_ASSERT(xanim_translation_stream_key_offset, offsetof(xanim_translation_stream_t, key) == 0x06);
XANIM_TYPES_ASSERT(xanim_translation_stream_inline_offset, offsetof(xanim_translation_stream_t, inlineLanes) == 0x08);
XANIM_TYPES_ASSERT(xanim_translation_stream_size, sizeof(xanim_translation_stream_t) == 0x10);

XANIM_TYPES_ASSERT(xanim_stream_pair_alignment, XANIM_TYPES_ALIGNOF(xanim_part_stream_pair_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_stream_pair_translation_offset, offsetof(xanim_part_stream_pair_t, translation) == 0x00);
XANIM_TYPES_ASSERT(xanim_stream_pair_rotation_offset, offsetof(xanim_part_stream_pair_t, rotation) == 0x04);
XANIM_TYPES_ASSERT(xanim_stream_pair_size, sizeof(xanim_part_stream_pair_t) == 0x08);

XANIM_TYPES_ASSERT(xanim_parts_alignment, XANIM_TYPES_ALIGNOF(XAnimParts) == 0x04);
XANIM_TYPES_ASSERT(xanim_parts_frame_count_offset, offsetof(XAnimParts, frameCountMinusOne) == 0x00);
XANIM_TYPES_ASSERT(xanim_parts_looped_offset, offsetof(XAnimParts, looped) == 0x02);
XANIM_TYPES_ASSERT(xanim_parts_delta_flag_offset, offsetof(XAnimParts, hasDeltaMotion) == 0x03);
XANIM_TYPES_ASSERT(xanim_parts_frame_rate_offset, offsetof(XAnimParts, frameRate) == 0x04);
XANIM_TYPES_ASSERT(xanim_parts_frequency_offset, offsetof(XAnimParts, frequency) == 0x08);
XANIM_TYPES_ASSERT(xanim_parts_names_offset, offsetof(XAnimParts, partNameHandles) == 0x0c);
XANIM_TYPES_ASSERT(xanim_parts_streams_offset, offsetof(XAnimParts, partStreamPairs) == 0x10);
XANIM_TYPES_ASSERT(xanim_parts_notetracks_offset, offsetof(XAnimParts, noteTracks) == 0x14);
XANIM_TYPES_ASSERT(xanim_parts_delta_motion_offset, offsetof(XAnimParts, deltaMotion) == 0x18);
XANIM_TYPES_ASSERT(xanim_parts_rotation_bits_offset, offsetof(XAnimParts, compressedRotationBits) == 0x1c);
XANIM_TYPES_ASSERT(xanim_parts_size, sizeof(XAnimParts) == 0x20);

typedef char xanim_entry_child_count_offset[offsetof(XAnimEntry, childCount) == 0x00 ? 1 : -1];
typedef char xanim_entry_parent_index_offset[offsetof(XAnimEntry, parentIndex) == 0x02 ? 1 : -1];
typedef char xanim_entry_payload_offset[offsetof(XAnimEntry, payload) == 0x04 ? 1 : -1];
typedef char xanim_entry_leaf_asset_offset[offsetof(XAnimEntry, payload.leafAsset) == 0x04 ? 1 : -1];
typedef char xanim_entry_parent_flags_offset[offsetof(XAnimEntry, payload.parent.flags) == 0x04 ? 1 : -1];
typedef char xanim_entry_first_child_offset[offsetof(XAnimEntry, payload.parent.firstChildIndex) == 0x06 ? 1 : -1];
typedef char xanim_entry_size[sizeof(XAnimEntry) == 0x08 ? 1 : -1];
typedef char xanim_entry_alignment[XANIM_TYPES_ALIGNOF(XAnimEntry) == 0x04 ? 1 : -1];

typedef char xanim_name_offset[offsetof(XAnim, name) == 0x00 ? 1 : -1];
typedef char xanim_node_count_offset[offsetof(XAnim, nodeCount) == 0x04 ? 1 : -1];
typedef char xanim_entries_offset[offsetof(XAnim, entries) == 0x08 ? 1 : -1];
typedef char xanim_size[sizeof(XAnim) == 0x08 ? 1 : -1];
typedef char xanim_alignment[XANIM_TYPES_ALIGNOF(XAnim) == 0x04 ? 1 : -1];

typedef char xanim_tree_source_offset[offsetof(XAnimTree, sourceTree) == 0x00 ? 1 : -1];
typedef char xanim_tree_active_count_offset[offsetof(XAnimTree, activePoolNodeCount) == 0x04 ? 1 : -1];
typedef char xanim_tree_remap_selector_offset[offsetof(XAnimTree, partRemapTableSelector) == 0x08 ? 1 : -1];
typedef char xanim_tree_handles_offset[offsetof(XAnimTree, poolNodeHandles) == 0x0c ? 1 : -1];
typedef char xanim_tree_handle_size[sizeof(((XAnimTree *)0)->poolNodeHandles[0]) == 0x02 ? 1 : -1];
typedef char xanim_tree_size[sizeof(XAnimTree) == 0x0c ? 1 : -1];
typedef char xanim_tree_alignment[XANIM_TYPES_ALIGNOF(XAnimTree) == 0x04 ? 1 : -1];

XANIM_TYPES_ASSERT(xanim_state_alignment, XANIM_TYPES_ALIGNOF(XAnimState) == 0x04);
XANIM_TYPES_ASSERT(xanim_state_time_offset, offsetof(XAnimState, time) == 0x00);
XANIM_TYPES_ASSERT(xanim_state_old_time_offset, offsetof(XAnimState, oldTime) == 0x04);
XANIM_TYPES_ASSERT(xanim_state_cycle_offset, offsetof(XAnimState, cycleCount) == 0x08);
XANIM_TYPES_ASSERT(xanim_state_old_cycle_offset, offsetof(XAnimState, oldCycleCount) == 0x0a);
XANIM_TYPES_ASSERT(xanim_state_blend_time_offset, offsetof(XAnimState, weightBlendTimeRemaining) == 0x0c);
XANIM_TYPES_ASSERT(xanim_state_target_weight_offset, offsetof(XAnimState, targetWeight) == 0x10);
XANIM_TYPES_ASSERT(xanim_state_current_weight_offset, offsetof(XAnimState, currentWeight) == 0x14);
XANIM_TYPES_ASSERT(xanim_state_rate_scale_offset, offsetof(XAnimState, rateScale) == 0x18);
XANIM_TYPES_ASSERT(xanim_state_size, sizeof(XAnimState) == 0x1c);

XANIM_TYPES_ASSERT(xanim_info_alignment, XANIM_TYPES_ALIGNOF(XAnimInfo) == 0x04);
XANIM_TYPES_ASSERT(xanim_info_notify_child_offset, offsetof(XAnimInfo, notifyChildIndex) == 0x00);
XANIM_TYPES_ASSERT(xanim_info_notify_index_offset, offsetof(XAnimInfo, notifyIndex) == 0x02);
XANIM_TYPES_ASSERT(xanim_info_notify_name_offset, offsetof(XAnimInfo, notifyName) == 0x04);
XANIM_TYPES_ASSERT(xanim_info_notify_type_offset, offsetof(XAnimInfo, notifyType) == 0x06);
XANIM_TYPES_ASSERT(xanim_info_free_prev_offset, offsetof(XAnimInfo, freePrev) == 0x08);
XANIM_TYPES_ASSERT(xanim_info_free_next_offset, offsetof(XAnimInfo, freeNext) == 0x0a);
XANIM_TYPES_ASSERT(xanim_info_states_offset, offsetof(XAnimInfo, states) == 0x0c);
XANIM_TYPES_ASSERT(xanim_info_states_size, sizeof(((XAnimInfo *)0)->states) == 0x38);
XANIM_TYPES_ASSERT(xanim_info_size, sizeof(XAnimInfo) == 0x44);

XANIM_TYPES_ASSERT(xanim_deferred_notify_alignment, XANIM_TYPES_ALIGNOF(xanim_deferred_notify_t) == 0x04);
XANIM_TYPES_ASSERT(xanim_deferred_notify_name_offset, offsetof(xanim_deferred_notify_t, name) == 0x00);
XANIM_TYPES_ASSERT(xanim_deferred_notify_type_offset, offsetof(xanim_deferred_notify_t, notifyType) == 0x04);
XANIM_TYPES_ASSERT(xanim_deferred_notify_padding_offset, offsetof(xanim_deferred_notify_t, padding06) == 0x06);
XANIM_TYPES_ASSERT(xanim_deferred_notify_time_offset, offsetof(xanim_deferred_notify_t, timeFrac) == 0x08);
XANIM_TYPES_ASSERT(xanim_deferred_notify_size, sizeof(xanim_deferred_notify_t) == 0x0c);

#undef XANIM_TYPES_ASSERT
#undef XANIM_TYPES_ALIGNOF
#endif

#endif
