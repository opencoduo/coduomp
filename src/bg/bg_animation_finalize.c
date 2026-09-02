// Authoritative sources:
//   uo_cgame_mp_x86.dll 0x300016a0..0x30001911
//   uo_game_mp_x86.dll  0x20001690..0x200018f2
//   game.mp.uo.i386.so  RVA 0x0001a0de..0x0001a41e
//
// BG_FinalizePlayerAnims(void) — the anim-tree load finalizer run by
// CGScr_LoadAnimTrees (0x30016360) immediately after Scr_EndLoadAnimTrees. It
// resolves the loaded "multiplayer" player DObj anim tree by name, learns how many
// bones it has, and rebuilds the shared BG static-animation table so that every
// bone becomes one bg_static_animation_t entry: entry 0 is the fixed "root" node,
// and entries 1..boneCount-1 carry each bone's name, a hash, its parent/anim length
// (clamped to a 500 ms floor), a move-speed derived from the bone's per-animation
// displacement, and the hold flag.
//
// Name resolution: the .mcode header's mechanical `# name
// BG_CalculateWeaponPosition_Sway` is a pure SIZE guess (win size 0x271 vs corpus
// 0x274) and is REJECTED — this function contains none of that family's behavior
// (BG_CalculateWeaponPosition_* take (pm_weapon_angle_state_t*, vec3_t) and do pure
// angle math; they issue no engine syscalls, no string copies, and touch no anim
// table). Identity is proven by (1) its sole caller CGScr_LoadAnimTrees, which calls
// it between Scr_EndLoadAnimTrees and BG_LoadAnimTreeInstances (already reconstructed,
// see FUN_30016360_300163ce.c, where this slot is BG_FinalizePlayerAnims); (2) the
// "root" node string (0x3007285c) written into entry 0; and (3) the DObj bone-query
// trap cluster it issues against the resolved tree handle. The Mac
// BG_FinalizePlayerAnims has the corresponding anim lookup, name, length, delta,
// loop, script-parse, and note-type calls, resolving the source name.
//
// Data model:
//   * animTable = *(bg_static_animation_table_t **)0x30134cc8 — the shared BG anim
//     table (script-VM global, same pointer BG_AnimationIndexForString / BG_PlayAnim
//     read). This function writes animTable->entryCount (+0xb800) and fills the
//     entries[] (bg_static_animation_t, stride 0x5c).
//   * animTable->animTreeHandle is table +0xa7ac8, now a modeled field of
//     bg_static_animation_table_t (the full container layout is reconstructed), read
//     directly as animTable->animTreeHandle. The parser initially uses this union
//     lane for the tree name; BG_FindAnimTrees replaces it with the
//     resolved XAnim pointer before this finalizer runs.
//   * runtimeAnims = *(bg_runtime_animation_t **)0x300a7820 with count
//     *(int *)0x300a5108 — the runtime-registered animation array built during the
//     anim-tree parse (see BG_AnimationIndexForString, FUN_300012a0). Each entry's
//     anim handle low word (+0x00 animIndex) is matched against the bone index to
//     recover the registered name/hash for bones the engine reports as "not present".
//
// ABI notes: Scr_GetAnimsIndex is script-import slot 98. The dedicated adapter at
// 0x3004fcc0 performs caller cleanup, proving cdecl; this function reclaims its
// accumulated pushes in its final frame teardown. cgame_syscall is also cdecl.
// The 500 ms floor, the *1000 scale, and the
// -1 blend sentinel are the exact constants in the stream.

#include "bg_animation.h"
#include "bg_animation_services.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"
#include "qcommon/q_string.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>
#include <stdint.h>

/* Each original copy passes 0x3f to strncpy and then writes NUL at +0x3f.
 * Q_strncpyz with the complete 0x40-byte destination performs that same bounded
 * copy and explicit termination while deriving the bound from the field. */
typedef char bg_anim_entry_name_size_check[(sizeof(((bg_static_animation_t *)0)->name) == 0x40) ? 1 : -1];

/* The root carries both shared entry flags. */
#define BG_ANIM_ROOT_FLAGS (BG_ANIM_ENTRY_NON_PRIMITIVE | BG_ANIM_ENTRY_UNUSED)

/* Millisecond conversion / floor constants. */
enum {
    BG_ANIM_MOVE_SPEED_SCALE = 1000, /* IMUL EAX,EAX,0x3e8 */
    BG_ANIM_MIN_ANIM_LENGTH = 500,  /* MOV EAX,0x1f4; clamp entry->duration up */
    BG_ANIM_BLEND_NONE = -1    /* MOV [ESI+0x40],0xffffffff sentinel */
};

/*
 * NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the one platform-specific
 * arithmetic island in BG_FinalizePlayerAnims. Windows keeps the z+y+x squared
 * sum and FSQRT result live in x87 through `_ftol2`. Linux forms x+y+z, stores
 * the sum as binary64 for glibc sqrt, stores the result as binary32, reloads it,
 * and truncates it with a temporary x87 control word. See the platform record.
 */
static int32_t bg_compat_anim_displacement_i32(const vec3_t moveDelta)
{
#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
    return x87f_store_i32_trunc(x87f_sqrt(x87f_add(x87f_add(x87f_mul(x87f_load_f32(moveDelta[2]), x87f_load_f32(moveDelta[2])),
                                                            x87f_mul(x87f_load_f32(moveDelta[1]), x87f_load_f32(moveDelta[1]))),
                                                   x87f_mul(x87f_load_f32(moveDelta[0]), x87f_load_f32(moveDelta[0])))));
#else
    const long double squared =
        ((long double)moveDelta[2] * (long double)moveDelta[2] + (long double)moveDelta[1] * (long double)moveDelta[1]) +
        (long double)moveDelta[0] * (long double)moveDelta[0];
    return coduo_fp_to_i32_extended(sqrtl(squared));
#endif
#else
#if EMULATE_X87
    const x87f squared = x87f_add(x87f_add(x87f_mul(x87f_load_f32(moveDelta[0]), x87f_load_f32(moveDelta[0])),
                                           x87f_mul(x87f_load_f32(moveDelta[1]), x87f_load_f32(moveDelta[1]))),
                                  x87f_mul(x87f_load_f32(moveDelta[2]), x87f_load_f32(moveDelta[2])));
    const float distance = (float)CoduoLibm_SqrtGlibc(x87f_store_f64(squared));
    return x87f_store_i32_trunc(x87f_load_f32(distance));
#else
    const long double squared =
        ((long double)moveDelta[0] * (long double)moveDelta[0] + (long double)moveDelta[1] * (long double)moveDelta[1]) +
        (long double)moveDelta[2] * (long double)moveDelta[2];
    const float distance = (float)CoduoLibm_SqrtGlibc((double)squared);
#if CODUO_ARCH_HAS_X87 && (defined(__GNUC__) || defined(__clang__))
    return CODUO_X87_TRUNCATE_I32((long double)distance);
#else
    return coduo_fp_to_i32_extended((long double)distance);
#endif
#endif
#endif
}

void BG_FinalizePlayerAnims(void)
{
    bg_static_animation_table_t *animTable = bgAnimStaticTable;

    /* 0x300016ac: the resolved tree pointer, retained for the whole build (it is both the
     * import argument and, later, the CG_XANIM_GET_LENGTH argument). */
    XAnim *tree = animTable->animTreeHandle;

    /* 0x300016b7: convert the loaded anim-tree object to its engine index; this
     * consumer intentionally keeps only AX. */
    uint16_t treeHandle = (uint16_t)Scr_GetAnimsIndex(tree);

    /* 0x300016c8: the XAnim source-tree node count becomes the table's entry
     * count. */
    int32_t initialBoneCount = bg_compat_xanim_get_tree_size(tree);
    bg_static_animation_table_t *countTable = bgAnimStaticTable;
    countTable->entryCount = initialBoneCount;

    /* 0x300016db..0x30001700: entry 0 is the fixed "root" node. */
    animTable->entries[0].flags |= BG_ANIM_ROOT_FLAGS;
    Q_strncpyz(animTable->entries[0].name, "root", (int32_t)sizeof(animTable->entries[0].name));
    animTable->entries[0].hash = 0;

    /* 0x30001713: loop over bones 1..boneCount-1 (signed <= guard). */
    for (int32_t bone = 1; bone < initialBoneCount; bone = coduo_int32_from_bits((uint32_t)bone + 1u)) {
        bg_static_animation_t *entry = &animTable->entries[bone];

        /* 0x30001720: reload the count and bounds-check the bone index (unsigned).
         * A bone index at/above the current count is a fatal load error. */
        bg_static_animation_table_t *liveTable = bgAnimStaticTable;
        int32_t liveBoneCount = liveTable->entryCount;
        if ((uint32_t)bone >= (uint32_t)liveBoneCount) {
            bg_compat_anim_index_error((uint32_t)bone, liveBoneCount);
        }

        /* 0x30001741: find the runtime-registered animation whose engine anim index
         * (low 16 bits of its handle) equals this bone index. */
        int32_t runtimeCount = *bgRuntimeAnimationCount;
        bg_runtime_animation_t *runtimeAnims = bgRuntimeAnimations;
        bg_runtime_animation_t *matched = (bg_runtime_animation_t *)0;

        for (int32_t r = 0; r < runtimeCount; r = coduo_int32_from_bits((uint32_t)r + 1u)) {
            if ((uint32_t)runtimeAnims[r].anim.animIndex == (uint32_t)bone) {
                matched = &runtimeAnims[r];
                break;
            }
        }

        if (matched == (bg_runtime_animation_t *)0) {
            /* 0x30001765: no registered animation for this bone — mark "unused". */
            entry->flags |= BG_ANIM_ENTRY_UNUSED;
            Q_strncpyz(entry->name, "unused", (int32_t)sizeof(entry->name));
            entry->hash = 0;
            continue;
        }

        /* 0x30001791: registered anim exists. treeHandle and animation index are
         * passed zero-extended (MOVZX word / MOVZX DI) to the XAnim queries. */
        uint16_t animIndex = (uint16_t)bone;
        if (!bg_compat_xanim_is_primitive(treeHandle, animIndex)) {
            /* 0x300017ad: the registered node is not a primitive animation.
             * Copy the registered name/hash and zero the geometry fields. */
            entry->flags |= BG_ANIM_ENTRY_NON_PRIMITIVE;
            Q_strncpyz(entry->name, matched->name, (int32_t)sizeof(entry->name));
            entry->hash = matched->hash;

            if (entry->blendTime == 0) {
                entry->blendTime = BG_ANIM_BLEND_NONE; /* 0x300017d8 */
            }
            entry->duration = 0;  /* 0x300017df */
            entry->moveSpeed = 0; /* 0x300017e2 */
            continue;
        }

        /* 0x300017ea: real bone. Name it from the engine and hash the name. */
        Q_strncpyz(entry->name, bg_compat_xanim_get_anim_name(treeHandle, animIndex), (int32_t)sizeof(entry->name));
        entry->hash = BG_StringHashValue(entry->name); /* 0x30001809 */

        if (entry->blendTime == 0) {
            entry->blendTime = BG_ANIM_BLEND_NONE; /* 0x3000181a */
        }

        /* 0x3000182c: the bone's animation length (ms), keyed on the tree pointer. */
        int32_t animLength = bg_compat_animation_get_length(tree, animIndex);
        entry->duration = animLength; /* 0x30001837 */

        if (animLength != 0) {
            /* 0x3000183c..0x3000188c: query relative rotation and movement deltas
             * across the animation, then take the movement-vector length as the
             * displacement. The first original stack slot is three floats even
             * though XAnimGetRelDelta writes only its leading vec2. */
            vec3_t rotationDeltaStorage;
            vec3_t moveDelta;
            bg_compat_xanim_get_rel_delta(treeHandle, animIndex, rotationDeltaStorage, moveDelta);

            /* The exact platform arithmetic/store graph is retained above. */
            int32_t dist = bg_compat_anim_displacement_i32(moveDelta);

            if (dist != 0) {
                /* 0x30001897: signed (IMUL/CDQ/IDIV) move speed in units/second. */
                int32_t scaledDistance = coduo_int32_from_bits((uint32_t)dist * (uint32_t)BG_ANIM_MOVE_SPEED_SCALE);
                entry->moveSpeed = scaledDistance / entry->duration;
            } else {
                entry->moveSpeed = 0; /* 0x300018a6 */
            }
        } else {
            entry->moveSpeed = 0; /* 0x300018a6 (via the animLength==0 branch) */
        }

        /* 0x300018a9: floor the animation length at 500 ms. */
        if (entry->duration < BG_ANIM_MIN_ANIM_LENGTH) {
            entry->duration = BG_ANIM_MIN_ANIM_LENGTH;
        }

        /* 0x300018b8: looped animations get BG_ANIM_ENTRY_LOOPED (0x80). */
        if (bg_compat_xanim_is_looped(treeHandle, animIndex)) {
            entry->flags |= BG_ANIM_ENTRY_LOOPED;
        }
    }

    /* 0x300018ec: reconcile the script/runtime tables against the rebuilt entries. */
    BG_AnimParseAnimScript(bgAnimStaticTable, NULL, NULL);

    /* 0x30001905: clear+re-derive the script-used flags over the new entries. */
    BG_SetupAnimNoteTypes(bgAnimStaticTable);
}
