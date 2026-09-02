#ifndef CLIENT_GLOBALS_H
#define CLIENT_GLOBALS_H

#include "qcommon/com_parse.h"
#include "qcommon/info.h"
#include "abi/cgame_module_abi.h"
#include "qcommon/asset_type_names.h"
#include "qcommon/bg_animation_types.h"
#include "qcommon/bg_item_types.h"
#include "bg/bg_pmove.h"
#include "qcommon/client_info_types.h"
#include "qcommon/client_state_types.h"
#include "qcommon/cgame_syscall_types.h"
#include "qcommon/collision_map_types.h"
#include "qcommon/entity_event_types.h"
#include "qcommon/entity_state_types.h"
#include "qcommon/filesystem_types.h"
#include "qcommon/game_state_types.h"
#include "qcommon/player_state_types.h"
#include "qcommon/pmove_types.h"
#include "qcommon/q_key_types.h"
#include "qcommon/q_renderer_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/sound_types.h"
#include "qcommon/snapshot_types.h"
#include "qcommon/trajectory_types.h"
#include "client/menu/ui_display_context_types.h"
#include "client/menu/ui_memory.h"
#include "ui_memory_config.h"
#include "client/menu/ui_menu_globals.h"
#include "client/menu/ui_menu_types.h"
#include "client/menu/ui_parse.h"
#include "qcommon/vehicle_types.h"
#include "qcommon/weapon_types.h"
#include "weapons/weapon_xanim_types.h"
#include "qcommon/xanim_types.h"

#include <stddef.h>
#include <stdint.h>

/* Canonical client-entity record, completed in client_recovered.h. */
typedef struct centity_s centity_t;

/* Forward typedef so the flame-chunk list-head globals below (cg_freeFlameChunks,
 * cg_activeFlameChunks, cg_flameChunks) type-check as pointers. The full
 * flameChunk_t layout is owned by the flame-chunk reconstruction; this only makes
 * the pointer declarations well-formed. */
typedef struct flameChunk_s flameChunk_t;

extern gameState_t cg_gameState;
/* NOT_FROM_ORIGINAL_SOURCE: portable typed equivalents of CG_Init's two original
 * monolithic REP-STOSD state clears. */
void cgame_compat_reset_recovered_cgs_state(void);
void cgame_compat_reset_recovered_cg_state(void);
void cgame_compat_reset_module_load_state(void);

typedef struct shellshock_s shellshock_t;

/*
 * Flame-chunk pool / per-owner flame-info sizing, proven by CG_ClearFlameChunks
 * (0x30025570) and CG_InitFlameChunks (0x300279d0):
 *   - The pool cg_flameChunks holds FLAME_CHUNK_COUNT nodes of FLAME_CHUNK_SIZE
 *     bytes; the allocator requests FLAME_CHUNK_COUNT*FLAME_CHUNK_SIZE == 0x2a0000
 *     bytes and CG_ClearFlameChunks threads exactly 0x2000 nodes of 0x150 bytes.
 *   - The per-owner flame-info region cg_flameInfo holds FLAME_INFO_COUNT elements
 *     of FLAME_INFO_SIZE bytes; CG_ClearFlameChunks clears the whole
 *     FLAME_INFO_COUNT*FLAME_INFO_SIZE == 0x2e000-byte region, and
 *     CG_AddFlameChunks indexes it with element stride 0xb8.
 */
enum {
    FLAME_CHUNK_COUNT = 0x2000,   /* 8192 pool nodes */
    FLAME_CHUNK_SIZE = 0x150,    /* 336 bytes per flameChunk_t */
    FLAME_INFO_COUNT = 1024,     /* per-owner flame-info elements */
    FLAME_INFO_SIZE = 0xb8      /* 184 bytes per element */
};

/*
 * Team-chat scroll buffer sizing, proven by CG_AddToTeamChat (0x30039390):
 *   - The ring height clamp compares cg_chatHeight_vmCvar.integer against 8, so
 *     TEAMCHAT_HEIGHT == 8 (matches teamChatMsgTimes[8], 0x3044b660..0x3044b680).
 *   - The line stride is IMUL by 0x10f == 271 bytes, which is TEAMCHAT_WIDTH*3+1
 *     with TEAMCHAT_WIDTH == 90 (worst case a "^x" color prefix per visible char
 *     plus a trailing NUL); the wrap test in the loop is `len > 89`, i.e.
 *     TEAMCHAT_WIDTH-1.
 */
enum {
    TEAMCHAT_HEIGHT = 8,
    TEAMCHAT_WIDTH = 90,
    TEAMCHAT_LINE_BYTES = TEAMCHAT_WIDTH * 3 + 1   /* 271 = stride 0x10f */
};

/*
 * First-pass globals for uo_cgame_mp_x86.dll.
 *
 * Names are assigned from direct data evidence where possible: string
 * contents, constant kind/value, or the first named function that references
 * mutable storage. Keep the source address suffix until later source-level
 * struct/table reconstruction proves a final owner and field name.
 */

/* hudElemSortCompare (0x3002a400) — qsort comparator ordering
 * hudElem_t entries ascending by their float sortKey (+0x6c).
 * Each qsort element is a `hudElem_t *`,
 * so the void* args point to stored pointers. Used only as the qsort callback in
 * CG_GetSortedHudElems (0x3002a440). */
int hudElemSortCompare(const void *a, const void *b);

/* LAG_SAMPLES — the lagometer ring capacity (a power of two). Proven from the
 * ring index being masked with 0x7f before every access (& (LAG_SAMPLES-1)). */
enum {
    LAG_SAMPLES = 128
};

/* lagometer_t — the cgame network-lag graph ring, based at 0x305380a4. Only the
 * snapshot-side tail is modelled here, since that is all the reconstructed
 * consumers touch: CG_AddLagometerSnapshotInfo (0x30018a40) and the inlined
 * dropped-snapshot path of CG_ReadNextSnapshot (0x3003d220) write snapshotSamples
 * and snapshotFlags at [snapshotCount & (LAG_SAMPLES-1)] and bump snapshotCount;
 * the lagometer draw pass (0x30018bc0) reads snapshotSamples via FILD and byte-
 * tests bit 0x1 of snapshotFlags. Field addresses proven: snapshotFlags at
 * 0x305380a4, snapshotSamples at 0x305382a4 (+0x200), snapshotCount at 0x305384a4
 * (+0x400); the 0x200 gaps are LAG_SAMPLES*4. The Q3 frame-side ring
 * (frameSamples/frameCount) precedes this base and is not modelled until its own
 * consumer is reconstructed. Provisional: field names by Q3 role, exact struct
 * name unresolved. Retyped from the three mechanical uint32_t symbols (the
 * owner=vector4scale label was the first-touching function, not the identity). */
typedef union lagometerSnapshotFlags_u {
    int32_t word;                         /* producer dword store */
    uint8_t bytes[sizeof(int32_t)];        /* consumer byte test */
} lagometerSnapshotFlags_t;
_Static_assert(sizeof(lagometerSnapshotFlags_t) == 4, "lagometer snapshot flag entry is one dword");

typedef struct lagometer_s {
    lagometerSnapshotFlags_t snapshotFlags[LAG_SAMPLES]; /* +0x000 (0x305380a4): dword entries; byte[0] bit 0x1 tested by draw */
    int32_t snapshotSamples[LAG_SAMPLES]; /* +0x200 (0x305382a4): per-snapshot latency sample; -1 marks a dropped snapshot */
    int32_t snapshotCount; /* +0x400 (0x305384a4): running count; index = snapshotCount & (LAG_SAMPLES-1) */
} lagometer_t;
_Static_assert(offsetof(lagometer_t, snapshotSamples) == 0x200, "lagometer snapshotSamples at +0x200");
_Static_assert(offsetof(lagometer_t, snapshotCount) == 0x400, "lagometer snapshotCount at +0x400");

/*
 * menuDef_t — element type of the global Menus table (0x30136940), looked up by
 * name via Menus_FindByName. Proven from that lookup: 0x810-byte stride and the
 * menu name pointer at +0x20 (the case-insensitive compare reads base+0x20).
 * Menu_New (0x3005ad40) proves the leading windowDef rectangle at +0x00..+0x0c
 * (rectDef_t rectClient) and the fullScreen flag at +0xbc.
 * The remaining fields are not yet reconstructed; the reserved bytes keep table
 * pointer arithmetic correct and are filled in as menu functions are recovered.
 */

/* The complete shared menuDef_s/menuDef_t declaration is in
 * ui_menu_types.h. It embeds the same windowDef_t as itemDef_t and keeps the
 * inherited Q3 member names. Machine checks prove the CoD extensions:
 * fadeInAmount, onKey[256], loadMode, and the 192-pointer item table, for an
 * i386 size of 0x810. */

/* Menu_Init (0x30058750) clears the leading window block separately from the
 * whole record: its second REP STOSD is ECX=0x2e dwords = 0xb8 bytes over
 * +0x00..+0xb7. That is the embedded windowDef (through window.outlineColor at
 * +0xa4) and its trailing `background` dword at +0xb4. The clear stops immediately
 * before the menu font pointer at +0xb8, so the span is exactly [0, font):
 * offsetof(menuDef_t, font) == 0xb8. Derived from the struct layout rather than
 * a bare literal (the assert anchors it to the machine-code-proven 0xb8). Original
 * the whole-record clear is just sizeof(menuDef_t) = 0x810. */
#define MENUDEF_WINDOW_SIZE ((size_t)offsetof(menuDef_t, font))

/* menuDef_t contains pointer fields, so its layout is only
 * ABI-exact at 32-bit pointer width (the target DLL); guard the asserts. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(menuDef_t, font) == 0xb8, "menuDef_t.font at +0xb8");
_Static_assert(offsetof(menuDef_t, window.cinematic) == 0x30, "menuDef_t.window.cinematic at +0x30");
_Static_assert(offsetof(menuDef_t, window.style) == 0x34, "menuDef_t.window.style at +0x34");
_Static_assert(offsetof(menuDef_t, window.borderSize) == 0x44, "menuDef_t.window.borderSize at +0x44");
_Static_assert(offsetof(menuDef_t, window.foreColor) == 0x74, "menuDef_t.window.foreColor at +0x74");
_Static_assert(offsetof(menuDef_t, window.outlineColor) == 0xa4, "menuDef_t.window.outlineColor at +0xa4");
_Static_assert(offsetof(menuDef_t, fadeClamp) == 0xd0, "menuDef_t.fadeClamp at +0xd0");
_Static_assert(offsetof(menuDef_t, soundName) == 0x4e8, "menuDef_t.soundName at +0x4e8");
_Static_assert(offsetof(menuDef_t, loadMode) == 0x4ec, "menuDef_t.loadMode at +0x4ec");
_Static_assert(offsetof(menuDef_t, focusColor) == 0x4f0, "menuDef_t.focusColor at +0x4f0");
_Static_assert(offsetof(menuDef_t, window.ownerDraw) == 0x3c, "menuDef_t.window.ownerDraw at +0x3c");
_Static_assert(offsetof(menuDef_t, window.ownerDrawFlags) == 0x40, "menuDef_t.window.ownerDrawFlags at +0x40");
_Static_assert(offsetof(menuDef_t, window.flags) == 0x48, "menuDef_t.window.flags at +0x48");
_Static_assert(offsetof(menuDef_t, window.background) == 0xb4, "menuDef_t.window.background at +0xb4");
_Static_assert(offsetof(menuDef_t, fullScreen) == 0xbc, "menuDef_t.fullScreen at +0xbc");
_Static_assert(offsetof(menuDef_t, itemCount) == 0xc0, "menuDef_t.itemCount at +0xc0");
_Static_assert(offsetof(menuDef_t, fadeCycle) == 0xcc, "menuDef_t.fadeCycle at +0xcc");
_Static_assert(offsetof(menuDef_t, fadeAmount) == 0xd4, "menuDef_t.fadeAmount at +0xd4");
_Static_assert(offsetof(menuDef_t, fadeInAmount) == 0xd8, "menuDef_t.fadeInAmount at +0xd8");
_Static_assert(offsetof(menuDef_t, cursorItem) == 0xc8, "menuDef_t.cursorItem at +0xc8");
_Static_assert(offsetof(menuDef_t, onOpen) == 0xdc, "menuDef_t.onOpen at +0xdc");
_Static_assert(offsetof(menuDef_t, onClose) == 0xe0, "menuDef_t.onClose at +0xe0");
_Static_assert(offsetof(menuDef_t, onESC) == 0xe4, "menuDef_t.onESC at +0xe4");
_Static_assert(offsetof(menuDef_t, onKey) == 0xe8, "menuDef_t.onKey at +0xe8");
_Static_assert(offsetof(menuDef_t, items) == 0x510, "menuDef_t.items[] at +0x510");
_Static_assert(sizeof(menuDef_t) == 0x810, "menuDef_t is exactly 0x810 bytes");
#endif

/*
 * BG animation / player-DObj context, reached through the pointer global
 * pm (0x30539850). Written while a player's animation state is
 * prepared (0x3000e050 / 0x3000e740) and consumed by the anim-update predicates
 * (e.g. PM_ShouldMakeFootsteps, 0x3000bb60). Layout is provisional: only the offsets
 * proven by the reconstructed consumers are named; reserved byte spans keep the
 * later fields at their machine-code offsets.
 */
/* The shared playerState_t flag domains and usercmd button domains are declared
 * with their records in player_state_types.h and q_shared_types.h. */

/* Static animation-script token tables. Names and contents match game_mp_uo;
 * addresses and NULL/-1 sentinels are verified against the client DLL. */
extern bg_indexed_string_t animStateStr[]; /* 0x30082054 */
extern bg_indexed_string_t bgAnimGroupStrings[]; /* 0x30082080 */
extern bg_indexed_string_t bgAnimEventStrings[]; /* 0x30082118 */
extern bg_indexed_string_t animBodyPartsStr[]; /* 0x300821a0 */
extern bg_indexed_string_t animMountedStr[]; /* 0x300821f8 */
extern bg_indexed_string_t animVehicleMotionStr[]; /* 0x30082240 */
extern bg_indexed_string_t animVehicleStr[]; /* 0x30082268 */
extern bg_indexed_string_t animWeaponClassStr[]; /* 0x30082290 */
extern bg_indexed_string_t animWeaponPositionStr[]; /* 0x300822f0 */
extern bg_indexed_string_t animStrafeStateStr[]; /* 0x30082308 */
extern bg_indexed_string_t bgAnimConditionTypeStrings[]; /* 0x30082328 */
extern bg_indexed_string_t bgAnimParseSectionStrings[]; /* 0x300823f8 */

/* The ABI-gap audit proved pm->ps is playerState_t: cg_pmove assigns
 * it &cg_predictedPlayerState, and the movement accesses match the shared server
 * layout. The former duplicate bg_animContext_t was removed. Animation condition
 * words at +0x468 belong instead to clientInfo_t. */

/*
 * Predictable player-state event id pushed by PM_Weapon_FireMelee (0x30014450)
 * into playerState_t.events[] when the melee swing lands its strike frame.
 * Value 170 (0xaa) is written as an immediate and agrees with the shared
 * entityEvent_t identity EV_FIRE_MELEE.
 */

/*
 * Predictable player-state event id pushed by PM_Weapon_CheckForMelee (0x30014520)
 * into playerState_t.events[] when it BEGINS a melee attack (enters the melee
 * wind-up state). Value 169 (0xa9) written as an immediate — exactly one less than
 * EV_FIRE_MELEE (170, the strike-landing event). The shared event-name tables
 * identify it as EV_MELEE_SWIPE.
 */

/*
 * Predictable player-state event id pushed by PM_PlayFatigueSound (0x3000c3a0) into
 * playerState_t.events[] each time the fatigue-sound cooldown elapses while the
 * player is fatigued. Value 140 (0x8c) is EV_FATIGUE_LAST_SOUND in the shared
 * event-name tables; the earlier EV_FATIGUE_SOUND label was off by one.
 */

/*
 * Predictable player-state event id pushed by PM_FoliageSounds (0x3000c110) into
 * playerState_t.events[] when the player's shrunk bounding box is embedded in
 * solid (moving through foliage/brush) and the speed-scaled repeat interval has
 * elapsed. Value 139 (0x8b) written as an immediate dword. Name adopted from the
 * shared entityEvent_t enum (EV_FOLIAGE_SOUND = 139); value matches exactly.
 */

/*
 * Predictable player-state event emitted when prone clearance fails or the
 * supporting ground is too steep, forcing the stance back to crouch. Value
 * 143 (0x8f) is written directly by PM_UpdatePronePitch (0x3000d470); the exact
 * EV_STANCE_FORCE_CROUCH name is proven by the recovered client event-name
 * table and agrees with the server entityEvent_t enum.
 */

/*
 * Predictable player-state event ids pushed by PM_WaterEvents
 * (0x3000c290) into playerState_t.events[] on the rising/falling edge of the
 * BG pmove context byte pm->waterlevel (+0xf1) versus the latched
 * previous value pml.previousWaterLevel (0x30539604). START (146/0x92) is written on a
 * 0 -> nonzero transition (water entry), STOP (147/0x93) on nonzero -> 0 (water
 * exit). Names are proven by the recovered cg event-name table and agree with
 * the PPC PM_WaterEvents symbol. */

/*
 * Predictable player-state event id pushed by PM_RemoveEmptyClipOnlyWeapon
 * (0x30013a00) into the player-state event ring when it drops a spent clipOnly
 * weapon. Value 151 (0x97) written as an immediate dword. Name adopted from the
 * shared entityEvent_t enum (EV_NOAMMO = 151); the value matches exactly.
 */

/* cgame engine system-call trap (client analog of the server's game_syscall_t).
 * The first argument is the syscall command id; the rest vary per call.
 *
 * i386 retains the original cdecl variadic ABI. Native 64-bit modules cannot
 * infer a variadic argument count or depend on adjacent stack dwords, so their
 * source-level trap expression constructs the same command/argument vector
 * consumed by CL_CgameSystemCalls. */
#if UINTPTR_MAX == UINT32_MAX
typedef intptr_t(CGAME_ABI_CDECL *cgame_syscall_t)(intptr_t command, ...);
#else
typedef intptr_t (*cgame_syscall_t)(intptr_t *arguments);

#define CGAME_NATIVE_SYSCALL_1(a0) cgame_syscall_vector((intptr_t[]){(intptr_t)(a0)})
#define CGAME_NATIVE_SYSCALL_2(a0, a1) cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1)})
#define CGAME_NATIVE_SYSCALL_3(a0, a1, a2) cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2)})
#define CGAME_NATIVE_SYSCALL_4(a0, a1, a2, a3) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3)})
#define CGAME_NATIVE_SYSCALL_5(a0, a1, a2, a3, a4) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4)})
#define CGAME_NATIVE_SYSCALL_6(a0, a1, a2, a3, a4, a5) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5)})
#define CGAME_NATIVE_SYSCALL_7(a0, a1, a2, a3, a4, a5, a6) \
    cgame_syscall_vector( \
        (intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6)})
#define CGAME_NATIVE_SYSCALL_8(a0, a1, a2, a3, a4, a5, a6, a7) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                      (intptr_t)(a6), (intptr_t)(a7)})
#define CGAME_NATIVE_SYSCALL_9(a0, a1, a2, a3, a4, a5, a6, a7, a8) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                      (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8)})
#define CGAME_NATIVE_SYSCALL_10(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                      (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9)})
#define CGAME_NATIVE_SYSCALL_11(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                      (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10)})
#define CGAME_NATIVE_SYSCALL_12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                      (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11)})
#define CGAME_NATIVE_SYSCALL_13(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
    cgame_syscall_vector((intptr_t[]){(intptr_t)(a0), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), \
                                      (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), \
                                      (intptr_t)(a12)})
#define CGAME_NATIVE_SYSCALL_SELECT(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, name, ...) name
#define cgame_syscall(...) \
    CGAME_NATIVE_SYSCALL_SELECT(__VA_ARGS__, CGAME_NATIVE_SYSCALL_13, CGAME_NATIVE_SYSCALL_12, CGAME_NATIVE_SYSCALL_11, \
                                CGAME_NATIVE_SYSCALL_10, CGAME_NATIVE_SYSCALL_9, CGAME_NATIVE_SYSCALL_8, CGAME_NATIVE_SYSCALL_7, \
                                CGAME_NATIVE_SYSCALL_6, CGAME_NATIVE_SYSCALL_5, CGAME_NATIVE_SYSCALL_4, CGAME_NATIVE_SYSCALL_3, \
                                CGAME_NATIVE_SYSCALL_2, CGAME_NATIVE_SYSCALL_1)(__VA_ARGS__)
#endif

/* Anim-tree memory allocator callback handed to Scr_PrecacheAnimTrees
 * (0x300f0984). The engine calls it back with a byte size and it returns a block
 * of engine-owned anim-tree memory. CGScr_LoadAnimTrees (0x30016360) passes
 * CG_AllocAnimTree (0x30016340), which forwards to cgame_syscall(205, size). Server
 * bank shape: void *GScr_AllocAnimTreeMemory(size_t). */
typedef void *(CGAME_ABI_CDECL *scr_anim_tree_alloc_t)(size_t size);

/* Script callback types shared by the five-function cgame->engine export table.
 * The exact roles and order are proved by the matching cgame_mp Mac symbols and
 * by the server/engine Scr_FarHook interface. */
typedef void(CGAME_ABI_CDECL *scr_function_callback_t)(void);
typedef void(CGAME_ABI_CDECL *scr_method_callback_t)(uint32_t scriptObject);
typedef scr_function_callback_t(CGAME_ABI_CDECL *scr_get_function_callback_t)(const char **name, int32_t *developerOnly);
typedef scr_method_callback_t(CGAME_ABI_CDECL *scr_get_method_callback_t)(const char **name, int32_t *developerOnly);
typedef void(CGAME_ABI_CDECL *scr_object_field_callback_t)(int32_t classNum, int32_t objectNum, int32_t fieldIndex);
typedef void *(CGAME_ABI_CDECL *scr_load_read_callback_t)(uint32_t size);

typedef struct cg_scriptExportTable_s {
    scr_get_function_callback_t getFunction;
    scr_get_method_callback_t getMethod;
    scr_object_field_callback_t setObjectField;
    scr_object_field_callback_t getObjectField;
    scr_load_read_callback_t loadRead;
} cg_scriptExportTable_t;

/* One native function-pointer cell in the script import ABI. A member using this
 * carrier has a proven slot and role but is not called by maintained cgame source,
 * so its full prototype is intentionally not asserted here. */
typedef void(CGAME_ABI_CDECL *cg_engineImportGeneric_t)(void);
typedef void(CGAME_ABI_CDECL *cg_script_load_marker_t)(void);
typedef void(CGAME_ABI_CDECL *cg_script_precache_anim_trees_t)(scr_anim_tree_alloc_t alloc);
typedef XAnim *(CGAME_ABI_CDECL *cg_script_find_anim_tree_t)(const char *name);
typedef void(CGAME_ABI_CDECL *cg_script_find_anim_t)(const char *treeName, const char *animName, scr_anim_t *outAnim);
typedef uint32_t(CGAME_ABI_CDECL *cg_script_get_anims_index_t)(XAnim *anims);

/* Native-pointer view of the complete 102-entry script import table copied by
 * Scr_FarHook. Slot names and order come from the independently recovered engine
 * and server callback tables. Cgame-called members have callable prototypes;
 * other named members use cg_engineImportGeneric_t only as an ABI-width carrier.
 * Slots 54, 55, 75, and 78 are genuine unwritten holes in the engine table. */
typedef struct cg_scriptImportTable_s {
    cg_engineImportGeneric_t getBool; /* slot 0 */
    cg_engineImportGeneric_t getInt;
    cg_engineImportGeneric_t getAnim;
    cg_engineImportGeneric_t getAnimTree;
    cg_engineImportGeneric_t getFloat;
    cg_engineImportGeneric_t getString;
    cg_engineImportGeneric_t getConstString;
    cg_engineImportGeneric_t getDebugString;
    cg_engineImportGeneric_t getIString;
    cg_engineImportGeneric_t getConstIString;
    cg_engineImportGeneric_t getVector;
    cg_engineImportGeneric_t getFunc;
    cg_engineImportGeneric_t getType;
    cg_engineImportGeneric_t getPointerType;
    cg_engineImportGeneric_t getEntityNum;
    cg_engineImportGeneric_t getNumParam;
    cg_engineImportGeneric_t addBool;
    cg_engineImportGeneric_t addInt;
    cg_engineImportGeneric_t addFloat;
    cg_engineImportGeneric_t addAnim;
    cg_engineImportGeneric_t addUndefined;
    cg_engineImportGeneric_t addEntityNum;
    cg_engineImportGeneric_t addStruct;
    cg_engineImportGeneric_t addString;
    cg_engineImportGeneric_t addIString;
    cg_engineImportGeneric_t addConstString;
    cg_engineImportGeneric_t addVector;
    cg_engineImportGeneric_t addObject;
    cg_engineImportGeneric_t addArray;
    cg_engineImportGeneric_t addArrayStringIndexed;
    cg_engineImportGeneric_t makeArray;
    cg_engineImportGeneric_t beginLoadScripts; /* slot 31 */
    cg_script_load_marker_t beginLoadAnimTrees; /* slot 32 */
    cg_engineImportGeneric_t endLoadScripts;
    cg_script_load_marker_t endLoadAnimTrees; /* slot 34 */
    cg_script_precache_anim_trees_t precacheAnimTrees; /* slot 35 */
    cg_engineImportGeneric_t freeScripts;
    cg_engineImportGeneric_t freeGameVariable;
    cg_engineImportGeneric_t shutdownSystem;
    cg_engineImportGeneric_t isSystemActive;
    cg_engineImportGeneric_t addExecThread;
    cg_engineImportGeneric_t addExecEntThreadNum;
    cg_engineImportGeneric_t execThread;
    cg_engineImportGeneric_t execEntThreadNum;
    cg_engineImportGeneric_t isThreadAlive;
    cg_engineImportGeneric_t error;
    cg_engineImportGeneric_t errorWithDialogMessage;
    cg_engineImportGeneric_t paramError;
    cg_engineImportGeneric_t objectError;
    cg_engineImportGeneric_t setDynamicEntityField;
    cg_engineImportGeneric_t freeEntityNum;
    cg_engineImportGeneric_t getEntityId;
    cg_engineImportGeneric_t setClassMap;
    cg_engineImportGeneric_t removeClassMap;
    cg_engineImportGeneric_t unusedSlot54;
    cg_engineImportGeneric_t unusedSlot55;
    cg_engineImportGeneric_t addClassField;
    cg_engineImportGeneric_t addFields;
    cg_engineImportGeneric_t findField;
    cg_engineImportGeneric_t getOffset;
    cg_engineImportGeneric_t copyEntityNum;
    cg_engineImportGeneric_t init;
    cg_engineImportGeneric_t shutdown;
    cg_engineImportGeneric_t abort;
    cg_engineImportGeneric_t setLoading;
    cg_engineImportGeneric_t initSystem;
    cg_engineImportGeneric_t allocGameVariable;
    cg_engineImportGeneric_t getChecksum;
    cg_engineImportGeneric_t hasSourceFiles;
    cg_engineImportGeneric_t saveSource;
    cg_engineImportGeneric_t loadSource;
    cg_engineImportGeneric_t skipSource;
    cg_engineImportGeneric_t savePre;
    cg_engineImportGeneric_t savePost;
    cg_engineImportGeneric_t saveShutdown;
    cg_engineImportGeneric_t unusedSlot75;
    cg_engineImportGeneric_t loadPre;
    cg_engineImportGeneric_t loadShutdown;
    cg_engineImportGeneric_t unusedSlot78;
    cg_engineImportGeneric_t loadScript;
    cg_script_find_anim_tree_t findAnimTree; /* slot 80 */
    cg_script_find_anim_t findAnim; /* slot 81 */
    cg_engineImportGeneric_t getFunctionHandle;
    cg_engineImportGeneric_t freeThread;
    cg_engineImportGeneric_t convertThreadToSave;
    cg_engineImportGeneric_t convertThreadFromLoad;
    cg_engineImportGeneric_t setString;
    cg_engineImportGeneric_t allocString;
    cg_engineImportGeneric_t notifyNum;
    cg_engineImportGeneric_t notifyId;
    cg_engineImportGeneric_t slConvertToString;
    cg_engineImportGeneric_t slGetString;
    cg_engineImportGeneric_t slGetLowercaseString;
    cg_engineImportGeneric_t slFindLowercaseString;
    cg_engineImportGeneric_t createCanonicalFilename;
    cg_engineImportGeneric_t setTime;
    cg_engineImportGeneric_t runCurrentThreads;
    cg_engineImportGeneric_t resetTimeout;
    cg_script_get_anims_index_t getAnimsIndex; /* slot 98 */
    cg_engineImportGeneric_t getAnims;
    cg_engineImportGeneric_t mtAlloc;
    cg_engineImportGeneric_t mtFree; /* slot 101 */
} cg_scriptImportTable_t;

_Static_assert(sizeof(cg_scriptImportTable_t) == 102 * sizeof(cg_engineImportGeneric_t),
               "script import table must contain 102 function pointers");
_Static_assert(offsetof(cg_scriptImportTable_t, beginLoadAnimTrees) == 32 * sizeof(cg_engineImportGeneric_t),
               "beginLoadAnimTrees must remain script import slot 32");
_Static_assert(offsetof(cg_scriptImportTable_t, findAnimTree) == 80 * sizeof(cg_engineImportGeneric_t),
               "findAnimTree must remain script import slot 80");
_Static_assert(offsetof(cg_scriptImportTable_t, getAnimsIndex) == 98 * sizeof(cg_engineImportGeneric_t),
               "getAnimsIndex must remain script import slot 98");
_Static_assert(offsetof(cg_scriptImportTable_t, unusedSlot54) == 54 * sizeof(cg_engineImportGeneric_t),
               "unused script import slot 54 moved");
_Static_assert(offsetof(cg_scriptImportTable_t, unusedSlot78) == 78 * sizeof(cg_engineImportGeneric_t),
               "unused script import slot 78 moved");

/*
 * The cgame system-call trap pointer (original address 0x30085e9c). dllEntry
 * stores the engine-provided trap here; every trap_* wrapper calls through it.
 */
#if UINTPTR_MAX == UINT32_MAX
extern cgame_syscall_t cgame_syscall;
#else
extern cgame_syscall_t cgame_syscall_vector;
#endif

/*
 * cg_trapStringBufferA/B (0x30538600 / 0x30538700) — two adjacent global string
 * buffers filled by strcpy in FUN_30002470 (0x3002ea34 copies into A, 0x3002ea56
 * copies into B) and passed by address to the CG_R_TEXT_PAINT emitter family: the twin
 * at 0x30031940 forwards A, the twin at 0x30031a00 forwards B. Sized 0x100 (256)
 * from the A->B address stride; the exact capacity is not proven by these
 * consumers, only that each holds a NUL-terminated string. Exact source names
 * unresolved; named by their proven role as the trap-54 string arguments.
 */
enum {
    CG_HUD_STRING_BUFFER_SIZE = 256
};
extern char cg_trapStringBufferA[CG_HUD_STRING_BUFFER_SIZE]; /* 0x30538600 */
extern char cg_trapStringBufferB[CG_HUD_STRING_BUFFER_SIZE]; /* 0x30538700 */

/* Code-referenced owned data. */
/* 0x3006f258 .rdata refs=1 width=1 first=0x3005cc39 owner=gscr_loadconsts */
/* 0x30071548 .rdata refs=1 width=imm first=0x30005ab6 owner=g_freevehicle */
extern const char bg_rightWeaponTagName[17];
/* 0x3007155c .rdata refs=1 width=imm first=0x30005aaf owner=g_freevehicle */
extern const char bg_leftWeaponTagName[16];
/* 0x3007156c .rdata refs=2 width=imm first=0x300059b1 owner label (g_freevehicle) is
 * a wrong first-touch artifact. This is a NUL-terminated .rdata string, not a scalar;
 * the mechanical export truncated it to its first dword (0x756f4315 = bytes 15 43 6f 75).
 * Repaired in place to the full string. Leading byte 0x15 is the CoD console-severity
 * marker prefix used by the Com_Error/Com_Printf family; '%s' takes the model name.
 * Consumed by CG_BuildCorpseDObjModels (0x300058f0) for main and attach model load
 * failures. */
extern const char bg_couldNotLoadModelErrorFormat[27];
/* 0x30071588 .rdata refs=2 width=imm first=0x30005816 owner=normaltolatlong */
extern const char cg_originTagName[11];
/*
 * consoleCommand_t / cg_consoleCommands — the cgame client console-command
 * dispatch table at 0x30071780 (.rdata). The mechanical export split this array
 * into three separate g_const_u32_* symbols (0x30071780/84/88, all mislabeled
 * owner=script_func_vectordot); those are superseded here by the real array.
 * Each entry pairs a command name (+0x00) with its handler (+0x04); a NULL name
 * terminates the table, and a NULL function marks a name-only entry ("mr").
 * Consumed by CG_ConsoleCommand (0x300178c0, reads name at +0x00 and function at
 * +0x04 with an 8-byte stride) and registered by the neighbor CG_InitConsoleCommands.
 */
typedef struct consoleCommand_s {
    const char *name; /* +0x00 */
    void (*function)(void); /* +0x04 */
} consoleCommand_t;
extern const consoleCommand_t cg_consoleCommands[];

/* Console-command handlers installed by cg_consoleCommands[]. */
void CG_PrintViewOriginAndSpin_f(void); /* 0x300172d0 */
void CG_ScoresDown_f(void); /* 0x30017330 */
void CG_ScoresUp_f(void); /* 0x30017310 */
void CG_SizeUp_f(void); /* 0x30017270 */
void CG_SizeDown_f(void); /* 0x300172a0 */
void CG_NextWeapon_f(void); /* 0x300474c0 */
void CG_PrevWeapon_f(void); /* 0x30047550 */
void CG_AltWeapon_f(void); /* 0x30047400 */
void CG_WeaponSlot_f(void); /* 0x30047750 */
void CG_Tcmd_f(void); /* 0x30017210 */
void CG_LoadHud_f(void); /* 0x30017380 */
void CG_Fade_f(void); /* 0x300173c0 */
void CG_FxSetTestPosition(void); /* 0x3003f390 */
void CG_FxTest(void); /* 0x3003f430 */
void CG_FxRestart(void); /* 0x3003f400 */
void CG_ShellShock_f(void); /* 0x300174b0 */
void CG_ShellShock_Load_f(void); /* 0x30017590 */
void CG_ShellShock_Save_f(void); /* 0x300175f0 */
void CG_TellTarget_f(void); /* 0x30017660 */
void CG_QuickMessage_f(void); /* 0x300176f0 */
void CG_QuickMap_f(void); /* 0x30017750 */
void CG_VoiceChat_f(void); /* 0x30017780 */
void CG_TeamVoiceChat_f(void); /* 0x30017820 */
void CG_OpenWMPurchase_f(void); /* 0x30017720 */
/* CG_InitConsoleCommands (0x30017920): register every cgame console command with
 * the engine (trap CG_ADD_COMMAND). Walks cg_consoleCommands[] registering each
 * name, then registers a fixed list of server-forwarded command names so they show
 * up in the client console/autocomplete. Takes no args, returns nothing. */
void CG_InitConsoleCommands(void); /* 0x30017920 */
/* 0x30071854 .rdata — cg_damageDirShaderParams: the static shader/texcoord parameter
 * block handed to the rotated-quad trap (CG_R_DRAW_ROTATED_QUAD via CG_DrawTurretTagQuad) by
 * CG_DrawDamageDirectionIndicators (0x3001aad5 PUSHes its address). Eight floats in .rdata:
 * {1,1,0, 1,0,0, 1,0}. The mechanical single-dword export g_const_float_one_30071854
 * captured only the leading 1.0f; superseded as the full const float[8]. */
extern const float cg_damageDirShaderParams[8];
/* 0x30071874..0x300718b4 .rdata — cg_rotatedPicShaderParams: the static shader/
 * texcoord parameter block that CG_DrawRotatedPic (0x3001cb60) hands by address
 * (PUSH 0x30071874) to the rotated-quad trap (CG_R_DRAW_ROTATED_QUAD). Sixteen floats in .rdata:
 * {0,0,1,0, 1,1,0,1, -1,-1,1,-1, 1,1,-1,1}. Ends at 0x300718b8, where the
 * cg_debugBoxEdges table begins, so the record is exactly 16 floats wide. Only
 * referenced by CG_DrawRotatedPic (refs=1, first=0x3001ccbd). Field meaning is a
 * default color/UV descriptor consumed by the engine's 2D-poly draw trap; modeled
 * as a typed const float array rather than named fields. The mechanical single-dword
 * export g_const_u32_00000000_30071874 captured only the leading 0.0f dword;
 * superseded in place by the full const float[16]. */
extern const float cg_rotatedPicShaderParams[16];
/* 0x300718b8 .rdata: the 12 edges of an axis-aligned box, as pairs of corner
 * indices into the 8-corner array built by CG_DebugBox (0x3001d970). Element i
 * is { corner index a @0x300718b8+i*8, corner index b @0x300718bc+i*8 }; the
 * whole table spans [0x300718b8, 0x30071918) = 12 records * 8 bytes. The
 * mechanical export mislabeled this (owner=yawtoaxis) and captured only the
 * first two dwords as scalars g_const_u32_*_300718b8 / *_300718bc; superseded
 * here in place by the real table shape proven from CG_DebugBox's loop. */
extern const uint32_t cg_debugBoxEdges[12][2];
/* 0x30071918..0x30071a38 .rdata: the 24 endpoints of a box wireframe (12 edges x
 * 2 ends) expressed as per-axis mins/maxs selectors. Each record is a 3-tuple
 * { selX, selY, selZ } where each selector is 0 (= use the bounds mins component
 * for that axis) or 1 (= use the maxs component). Consumed by
 * CG_DObjGetBoneBoundsWireframe (0x30020020), whose loop reads each dword D, forms
 * D*3, and loads src[D*3 + axis] from a bone's local {mins[3],maxs[3]} record to
 * build a box corner it then transforms by the bone world matrix. Companion to the
 * edge-pair table cg_debugBoxEdges above. The mechanical export mislabeled this
 * (owner=cg_loadmenus) and captured only the first dword as the scalar
 * g_const_u32_00000000_30071918; superseded here in place by the real table shape,
 * verified byte-for-byte from the .rdata dump (all values 0 or 1). */
extern const uint32_t cg_boxCornerSelectors[24][3];
/* 0x30071a3c..0x30071a5c (.rdata, refs=1, first=0x30031f4a) — the four
 * texture-coordinate pairs {{1,1},{1,0},{0,0},{0,1}} passed by
 * CG_DrawTankBarrel through CG_DrawTurretTagQuad to the rotated-quad renderer.
 * Original RE_DrawQuadPic consumes all eight floats, one pair per vertex, at
 * 0x004f0468..0x004f04cb. The dword at 0x30071a5c is alignment padding before
 * the next independent datum at 0x30071a60. */
extern const float cg_turretTagShaderParams[8];
/*
 * 0x30071a60 .rdata — cg_scoreboardColumnValueSelect[5]: the per-column value-
 * source selector read by CG_DrawScoreboardTeamHeader's totals-row loop
 * (0x30037314 MOV EAX,[ESI + 0x30071a60], stride 0x10). It is the
 * `.nextColumnValueSelect`
 * lane of cg_scoreboardColumns read one entry EARLY (0x30071a60 = the column
 * table base 0x30071a64 minus 4): element 0 is the standalone dword just before
 * the table (value 4), elements 1..4 alias
 * cg_scoreboardColumns[0..3].nextColumnValueSelect
 * (values 0,1,2,3). Selector 1 -> draw cg_scoreboardTeamScores[team]; 3 -> draw
 * cg_scoreboardTeamPings[team]; any other value (0/2/4) skips that column body.
 * Modeled as a distinct const int lane starting at 0x30071a60 (the address the
 * machine indexes); supersedes the mechanical single-dword split. The mechanical
 * owner=g_damage / "different table" note was a wrong size-match guess.
 */
enum {
    CG_SCOREBOARD_VALUESELECT_COUNT = 5
};

/*
 * cgScoreboardColumn_t / cg_scoreboardColumns[5] (.rdata @ 0x30071a64) — the
 * static scoreboard-header column table walked by the scoreboard cluster
 * (CG_DrawScoreboard_ListColumnHeaders @ 0x30036d60, and the per-client score
 * row drawer near 0x30037314/0x30037379/0x3003734a which loads the same base
 * 0x30071a64 into EBP at 0x30037593). Five 16-byte entries; proven fields:
 *   +0x00 widthFraction : column width as a fraction of the board width (FMUL)
 *   +0x04 headerRef     : localization reference passed to CG_SafeTranslateString_Internal
 *                         ("cgame" domain); "" means the column has no header
 *                         label (byte[headerRef]==0 gate skips its draw)
 *   +0x08 mode          : draw mode; 2 selects the trap-52 measure/right-align
 *                         path in the header drawer (CMP mode,2 / JNZ)
 *   +0x0c nextColumnValueSelect: selector consumed for the following column
 * Supersedes the mechanical per-field split (0x30071a64/68/6c) — these were one
 * .rdata table, not three unrelated constants.
 */
typedef struct cgScoreboardColumn_s {
    float widthFraction; /* +0x00 */
    const char *headerRef; /* +0x04 */
    int32_t mode; /* +0x08 */
    uint32_t nextColumnValueSelect; /* +0x0c: selector for the following column */
} cgScoreboardColumn_t;

typedef struct cgScoreboardColumnPrefix_s {
    float widthFraction;
    const char *headerRef;
    int32_t mode;
} cgScoreboardColumnPrefix_t;

/* One physical .rdata object beginning four bytes before the column array. The
 * selector reader uses a 0x10 stride from leadingValueSelect, so selectors 1..4
 * are the preceding column's nextColumnValueSelect field. */
typedef struct cgScoreboardLayout_s {
    int32_t leadingValueSelect;
    cgScoreboardColumn_t columns[4];
    cgScoreboardColumnPrefix_t lastColumn;
} cgScoreboardLayout_t;

enum {
    CG_SCOREBOARD_COLUMN_COUNT = 5
}; /* loop runs ESI 0..0x40 step 0x10 */

/* Draw mode selecting the trap-52 (measure/right-align) header path. */
enum {
    CG_SB_COLUMN_MODE_MEASURED = 2
};

extern const cgScoreboardLayout_t cg_scoreboardLayout;
#define CG_SCOREBOARD_COLUMN(index) \
    ((index) < 4 ? &cg_scoreboardLayout.columns[(index)] : (const cgScoreboardColumn_t *)(const void *)&cg_scoreboardLayout.lastColumn)
#define CG_SCOREBOARD_VALUE_SELECT(index) \
    ((index) == 0 ? cg_scoreboardLayout.leadingValueSelect : (int32_t)cg_scoreboardLayout.columns[(index) - 1].nextColumnValueSelect)

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(cgScoreboardColumn_t, widthFraction) == 0x00, "sb column widthFraction @ +0x00");
_Static_assert(offsetof(cgScoreboardColumn_t, headerRef) == 0x04, "sb column headerRef @ +0x04");
_Static_assert(offsetof(cgScoreboardColumn_t, mode) == 0x08, "sb column mode @ +0x08");
_Static_assert(offsetof(cgScoreboardLayout_t, columns) == 0x04, "sb columns begin four bytes after selector lane");
_Static_assert(offsetof(cgScoreboardLayout_t, lastColumn) == 0x44, "sb short final column begins at layout +0x44");
_Static_assert(sizeof(cgScoreboardLayout_t) == 0x50, "sb physical layout ends before 0x30071ab0 float table");
_Static_assert(sizeof(cgScoreboardColumn_t) == 0x10, "sb column stride 0x10");
#endif
/* 0x30071b18 .rdata: cg_shellshockRandomTable — a precomputed 2-D noise/direction
 * table of (x,y) float pairs used to drive the shellshock screen-blur displacement.
 * The per-frame screen-blur update (0x3003c630) indexes it with a phase-derived
 * index masked to 0..127 (`(dur*61 + floor(phase)) & 0x7f`), then cubic-interpolates
 * a window of 4 consecutive pairs (LEA [idx*8 + base]; reads +0..+0x1c = 8 floats).
 * 131 pairs (0x30071b18..0x30071f30): 128 unique rows plus 3 duplicated guard rows
 * (0x30071f18 repeats 0x30071b18) so a window at index 125..127 does not overrun.
 * Mechanical single-uint32 g_const_float_neg_0_56355101 repaired in place (wrong
 * type; it is a table, not one constant). Other ref 0x30036352 only CMPs against the
 * base address as a table bound. Exact source symbol name unresolved; named by role. */
#define CG_SHELLSHOCK_RANDOM_TABLE_ROWS 131
extern const float cg_shellshockRandomTable[CG_SHELLSHOCK_RANDOM_TABLE_ROWS][2];
/* 0x30071f30..0x30071f57 .rdata refs=1 first=0x3003c0f3. Ten 1.0f
 * alias-channel targets consumed by MSS_FadeSelectSounds when shellshock ends.
 * vec3_origin starts at the distinct following address 0x30071f58. */
extern const float cg_soundChannelFullVolumes[SND_ALIAS_CHANNEL_COUNT];
/* Read-only BG_CheckProneValid diagnostic colors. The raw .rdata bytes prove
 * four-float objects; the mechanical export had truncated each to one dword.
 * The client medium cyan is {0, 0.5, 0.5, 1}, which differs from the recovered
 * server binary's corresponding color. */
extern const vec4_t bg_proneColorRed; /* 0x30071f74 */
extern const vec4_t bg_proneColorGreen; /* 0x30071f84 */
extern const vec4_t bg_proneColorYellow; /* 0x30071fb4 */
extern const vec4_t bg_proneColorMagenta; /* 0x30071fe4 */
extern const vec4_t bg_proneColorCyan; /* 0x30071ff4 */
extern const vec4_t bg_proneColorMediumCyan; /* 0x30072014 */
/* 0x30072034 .rdata refs=7 width=imm first=0x3000ccf6 owner=veh_findvaliddismountspot */
extern const vec4_t cg_colorWhite;
/* 0x3007285c .rdata refs=4 width=imm first=0x300016e5 owner=bg_calculateweaponposition_sway */
extern const char bg_rootAnimationName[5];
/* 0x30072a40 .rdata refs=1 width=imm first=0x30001a50 owner=cmd_give_f */
extern const char bg_conditionNotKeyword[4];
/* 0x30072b30 .rdata refs=1 width=imm first=0x3001e973 owner=objectivestateindexfromstring */
extern const char bg_mg42WeaponName[5];
/* 0x30072d10 .rdata refs=2 width=4 first=0x300081c2 owner=gscr_getnumparts */
extern const char bg_passenger4TagName[15];
/* 0x30072d20 .rdata refs=2 width=4 first=0x300081ba owner=gscr_getnumparts */
extern const char bg_passenger3TagName[15];
/* 0x30072d30 .rdata refs=2 width=4 first=0x300081b2 owner=gscr_getnumparts */
extern const char bg_passenger2TagName[15];
/* 0x30072d40 .rdata refs=2 width=4 first=0x300081aa owner=gscr_getnumparts */
extern const char bg_passengerTagName[14];
/* 0x30072d50 .rdata refs=2 width=4 first=0x300081a2 owner=gscr_getnumparts */
extern const char bg_secondaryPlayerTagName[21];
/* 0x30072d68 .rdata refs=4 width=4 first=0x3000819a owner=gscr_getnumparts */
extern const char bg_playerTagName[11];
/* 0x30072d74 .rdata refs=1 width=4 first=0x30008193 owner=gscr_getnumparts */
extern const char bg_unusedBoneName[9];
/* 0x30072ddc .rdata: NUL-terminated Com_Error format "\x15BG_CanItemBeGrabbed: IT_BAD"
 * ('\x15' is the CoD console error-channel marker byte). The mechanical export
 * truncated it to its first dword (0x5f474215) as a uint32; repaired here to the
 * full char[] since BG_CanItemBeGrabbed (0x30005e00) takes its address as the
 * Com_Error(ERR_DROP, ...) format for the IT_BAD item-type case. The
 * owner=boxdistsqrdexceeds label was the exporter's size-guess for 0x30005e00. */
extern const char bg_canItemBeGrabbedInvalidItemErrorMessage[29];
/* 0x30072dfc .rdata: NUL-terminated Com_Error format
 * "\x15BG_CanItemBeGrabbed: index out of range". Same repair as +0x30072ddc:
 * the mechanical export truncated it to its first dword; BG_CanItemBeGrabbed
 * (0x30005e00) takes its address as the Com_Error(ERR_DROP, ...) format
 * when the item index is out of [1, 134). */
extern const char bg_canItemBeGrabbedIndexOutOfRangeErrorMessage[41];
/* 0x30074a0c .rdata refs=60 first=0x3000fe23: the shared empty-string literal "".
 * The mechanical export truncated this NUL-terminated string to a uint32_t 0;
 * repaired to a char[] since machine code takes its address as a cvar default and
 * reads its first byte as the NUL terminator. */
extern const char g_str_empty[1];
/* 0x30074e7c .rdata refs=1 first=0x30011129: NUL-terminated format string
 * "Couldn't find weapon \"%s\"\n" (mechanical export truncated it to one dword). */
extern const char g_str_couldnt_find_weapon[27];
/* 0x300769e0 .rdata refs=19 width=imm first=0x30017b4a owner=script_method_player_setfatigue */
/* 0x30076bf8 .rdata refs=1 width=imm first=0x3001ae33 owner=info_setvalueforkey */
extern const char cg_soundChannelDebugFormat[65];
/* 0x30076c3c .rdata refs=4 width=imm first=0x3001ae08 owner=info_setvalueforkey */
extern const char cg_soundChannelIndexFormat[4];
/* 0x30076c40 .rdata refs=1 width=imm first=0x3001adb4 owner=info_setvalueforkey */
extern const char cg_soundDebugHeaderFormat[70];
/* 0x30076c88 .rdata refs=1 width=imm first=0x3001ad7d owner=info_setvalueforkey */
extern const char mss_stereoCvarName[11];
/* 0x30076c94 .rdata refs=1 width=imm first=0x3001ad56 owner=info_setvalueforkey */
extern const char mss_khzCvarName[8];
/* 0x30076c9c .rdata refs=1 width=imm first=0x3001ad2d owner=info_setvalueforkey */
extern const char mss_bitsCvarName[9];
/* 0x30076ca8 .rdata refs=1 width=imm first=0x3001ad13 owner=info_setvalueforkey */
extern const char mss_3dProviderCvarName[16];
/* 0x30076cb8 .rdata: the cvar-name string literal "cl_conXOffset". */
extern const char g_str_cl_conXOffset[14];
/* 0x30076cc8 .rdata: the string literal "0", shared as a cvar value across the DLL. */
extern const char g_str_zero[2];
/* 0x30076ccc .rdata: the menu-name string literal "voiceMenu". */
extern const char g_str_voiceMenu[10];
/* 0x30076cec .rdata refs=1 width=imm first=0x30019005 owner=cmd_veh_fireturret */
extern const char cg_lagometerSyncLabel[4];
/* 0x30076cf0 .rdata refs=2 width=imm first=0x30018b93; the "connection interrupted"
 * net icon shader, registered by CG_DrawDisconnect (0x30018a90). (The mechanical
 * owner=script_method_player_setreverb label was the wrong size-guessed first-toucher.) */
extern const char cg_connectionInterruptedIconPath[15];
/* 0x30076d00 .rdata refs=1 width=imm first=0x30018af6; the localized "connection
 * interrupted" HUD string reference, looked up by CG_DrawDisconnect (0x30018a90) via
 * CG_SafeTranslateString_Internal. (owner=script_method_player_setreverb was the wrong first-toucher.) */
extern const char cg_connectionInterruptedLocalizationKey[28];
/* 0x30076d88 .rdata refs=1 width=imm first=0x30017f9e owner=g_checkpointinsidetriggermount */
extern const char cg_scriptStringUsageDebugFormat[17];
/* 0x30076d9c .rdata refs=1 width=imm first=0x30017f1d owner=g_checkpointinsidetriggermount */
extern const char cg_scriptNumThreadsDebugFormat[16];
/* 0x30076dac .rdata refs=1 width=imm first=0x30017e9f owner=g_checkpointinsidetriggermount */
extern const char cg_scriptNumVarsDebugFormat[16];
/* 0x30077138 .rdata refs=1 width=imm first=0x30022217; consumed by CG_AddCEntity
 * (0x30022170) as the "Bad entity type: %i\n" Com_ErrorMessage format. The
 * mechanical export truncated this NUL-terminated .rdata string to its first
 * dword ("Bad " = 0x20646142) as a uint32_t; superseded here with the real
 * string per its proven consumer (bytes verified 42 61 64 20 .. 25 69 0a 00). */
extern const char cg_badEntityTypeErrorFormat[21];
/* 0x300771a4 .rdata refs=1 width=imm first=0x30020c07 owner=bg_animparseanimscript */
extern const char cg_vehicleSteeringWheelTagName[18];
/* 0x300771b8 .rdata refs=3 width=imm first=0x30020ace owner=bg_animparseanimscript */
extern const char cg_vehicleSecondaryGunTagName[18];
/* 0x300771cc .rdata refs=1 width=imm first=0x30020a85 owner=bg_animparseanimscript */
extern const char cg_vehicleSecondaryBaseTagName[19];
/* 0x300771e0 .rdata refs=2 width=imm first=0x30020777 owner=bg_animparseanimscript */
extern const char cg_vehicleBarrelTagName[11];
/* 0x300771ec .rdata refs=3 width=imm first=0x3002072e owner=bg_animparseanimscript */
extern const char cg_vehicleTurretTagName[11];
/* 0x300771f8 .rdata refs=2 width=imm first=0x300206a3 owner=bg_animparseanimscript */
extern const char cg_vehicleBodyTagName[9];
/* 0x300772c0 .rdata refs=7 width=imm first=0x3001ebc6 owner=fire_artillery */
extern const char cg_muzzleFlashTagName[10];
/* 0x300772cc .rdata refs=1 width=imm first=0x3001eb68 owner=fire_artillery */
extern const char cg_animatedAimTagName[17];
/* 0x300772e0 .rdata refs=2 width=imm first=0x3001eb1c owner=fire_artillery */
extern const char cg_aimTagName[8];
/* 0x30077398 .rdata refs=11 width=imm first=0x30022bc6 owner=cmd_callvote_f */
extern const char cvarEnabledValue[2];
/* 0x30077418 .rdata refs=6 width=imm first=0x300226a8 owner=vectordistance2d */
extern const char cg_weaponSelectCvarName[16];
/* 0x3007746e .rdata refs=2 width=4 first=0x30022462 owner=scr_vehicle_think */
/* 0x30077594 .rdata refs=1 width=imm first=0x30027d70 owner=cg_fireflamechunks */
extern const char r_overbrightBitsCvarName[17];
/* 0x300775a8 .rdata refs=1 width=imm first=0x30027d54 owner=cg_fireflamechunks */
extern const char r_fullscreenCvarName[13];
/* 0x300777d4 .rdata refs=1 width=4 first=0x3002a05e owner=bg_getmaxpickupableammo */
/* 0x300777d6 .rdata refs=1 width=1 first=0x3002a064 owner=bg_getmaxpickupableammo */
/* 0x30077828 .rdata refs=6 width=imm first=0x3002a8c9 owner=pm_laddermove */
extern const char com_pathWithExtensionFormat[5];
/* 0x30077880 .rdata refs=16 width=imm first=0x3001c1b8 owner=g_getnonpvsfriendlyinfo */
extern const char mapNameInfoKey[8];
/* 0x30077888 .rdata refs=4 width=imm first=0x3002a5e0 owner=pm_laddermove */
extern const char ui_scriptMenuAllowResponseCvarName[27];
/* 0x300778a4 .rdata refs=17 width=imm first=0x3001c19c owner=g_getnonpvsfriendlyinfo */
extern const char cl_serverLoadWaitingCvarName[21];
/* 0x30077908 .rdata refs=5 width=imm first=0x3002aa87 owner=pm_weaponuseammo */
extern const char cg_freeLocalEntityInactiveErrorMessage[31];
/* 0x300779b4 .rdata refs=8 width=imm first=0x3002df96 owner=g_runframe */
extern const char cg_whiteMaterialName[6];
/* 0x300779d8 .rdata refs=1 width=imm first=0x3002ddd7 owner=playercmd_takeweapon */
extern const char cg_uiSliderThumbMaterialPath[27];
/* 0x300779f4 .rdata refs=1 width=imm first=0x3002ddb9 owner=playercmd_takeweapon */
extern const char cg_uiSliderTrackMaterialPath[22];
/* 0x30077a0c .rdata refs=1 width=imm first=0x3002dd9e owner=playercmd_takeweapon */
extern const char cg_uiScrollThumbMaterialPath[30];
/* 0x30077a2c .rdata refs=1 width=imm first=0x3002dd83 owner=playercmd_takeweapon */
extern const char cg_uiScrollRightArrowMaterialPath[36];
/* 0x30077a50 .rdata refs=1 width=imm first=0x3002dd68 owner=playercmd_takeweapon */
extern const char cg_uiScrollLeftArrowMaterialPath[35];
/* 0x30077a74 .rdata refs=1 width=imm first=0x3002dd4a owner=playercmd_takeweapon */
extern const char cg_uiScrollUpArrowMaterialPath[35];
/* 0x30077a98 .rdata refs=1 width=imm first=0x3002dd2f owner=playercmd_takeweapon */
extern const char cg_uiScrollDownArrowMaterialPath[36];
/* 0x30077abc .rdata refs=1 width=imm first=0x3002dd14 owner=playercmd_takeweapon */
extern const char cg_uiScrollBarMaterialPath[24];
/* 0x30077ad4 .rdata refs=1 width=imm first=0x3002dcf9 owner=playercmd_takeweapon */
extern const char cg_uiGradientBarMaterialPath[27];
/* 0x30077b18 .rdata refs=2 width=imm first=0x30029cc2 owner=cg_updateshellshocksound */
extern const char cg_hudElemLocalizationContext[15];
/* 0x30077b28 .rdata refs=33 width=imm first=0x300177bd owner=pm_interuptweaponwithsprintmove */
extern const char cg_localizationContext[6];
/* 0x30077b3c .rdata refs=3 width=4 first=0x3002d735 owner=script_method_scriptbuiltin_sethin */
/* 0x30077b40 .rdata refs=3 width=4 first=0x3002d73b owner=script_method_scriptbuiltin_sethin */
/* 0x30077b44 .rdata refs=3 width=4 first=0x3002d746 owner=script_method_scriptbuiltin_sethin */
/* 0x30077b48 .rdata refs=3 width=1 first=0x3002d751 owner=script_method_scriptbuiltin_sethin */
/* 0x30077d90 .rdata refs=28 width=imm first=0x3001e85f.
 * The shared "CG_ConfigString: bad index: %i" error format used by the inlined
 * CG_ConfigString bounds check across the cgame (28 sites incl. CG_SetConfigValues
 * 0x30038430). Mechanical owner=bg_takeplayerweapon is a wrong first-touch guess. */
extern const char cg_configStringBadIndexFmt[31];
/* 0x30078854 .rdata refs=5 width=imm first=0x3002b21d owner=cg_playsoundaliasbyname */
extern const char cl_runCvarName[7];
/* 0x30078f2c .rdata refs=6 first=0x3003a6ff: the "-1" cvar-value literal. */
extern const char g_str_minus_one[3];
/* 0x30078f70 .rdata refs=1 width=imm first=0x300383b0 owner=pm_weapon_addfiringaimspreadscale */
extern const char g_gametypeInfoKey[11];
/* 0x30079058 .rdata refs=3 width=imm first=0x3002b206 owner=cg_playsoundaliasbyname */
extern const char cl_stanceCvarName[10];
/* 0x300795f0 .rdata: cvar name whose float value scales owner-draw alpha. */
extern const char cg_hudAlphaCvarName[12];
/* 0x30079760 .rdata refs=2 width=imm first=0x30031538 owner=menuparse_itemdef */
extern const char cg_hudStatUnsetText[2];
/* 0x300797e0 .rdata: the HUD weapon-ammo-count clip/reserve separator string "|".
 * Both refs (0x30030e29, 0x30030e4a) are in CG_DrawPlayerAmmoValue
 * (FUN_30030c60_30030f10), which measures its width via trap_R_Text_Width and draws it
 * horizontally centered in the ammo rect between the left clip count and the
 * right reserve count. The mechanical owner=spectatorthink label is the exporter's
 * wrong first-toucher size-guess name for that function. Named by proven role. */
extern const char cg_ammoCountSeparator[2];
/* 0x30079a88 .rdata refs=1 width=imm first=0x300339f1 owner=pm_checkduck */
extern const char cg_missingTurretTagWarning[];
/* 0x30079ad4 .rdata refs=10 width=imm first=0x30033116 owner=pm_checkduck */
extern const char cg_playerAnimNoChildrenError[];
/* 0x30079bbc .rdata refs=2 width=imm first=0x30037b17 owner=item_textfield_paint */
extern const char cg_scoreboardScrollDownKeyMaterialName[28];
/* 0x30079bd8 .rdata refs=2 width=imm first=0x30037ae7 owner=item_textfield_paint */
extern const char cg_scoreboardScrollDownArrowMaterialName[30];
/* 0x30079bf8 .rdata refs=2 width=imm first=0x30037aa2 owner=item_textfield_paint */
extern const char cg_scoreboardScrollUpKeyMaterialName[26];
/* 0x30079c14 .rdata refs=2 width=imm first=0x30037a72 owner=item_textfield_paint */
extern const char cg_scoreboardScrollUpArrowMaterialName[28];
/* 0x30079c30 .rdata refs=1 width=imm first=0x3003719f owner=g_damage */
extern const char cg_scoreboardSpectatorsLocalizationKey[17];
/* 0x30079c44 .rdata refs=2 width=imm first=0x30037189 owner=g_damage */
extern const char cg_scoreboardSpectatorsBannerCvarName[26];
/* 0x30079c60 .rdata refs=1 width=imm first=0x300371e0 owner=g_damage */
extern const char cg_scoreboardAlliesTeamNameCvarName[18];
/* 0x30079c74 .rdata refs=2 width=imm first=0x300371c9 owner=g_damage */
extern const char cg_scoreboardAlliesBannerCvarName[22];
/* 0x30079c8c .rdata refs=1 width=imm first=0x300371b0 owner=g_damage */
extern const char cg_scoreboardTeamNameFormat[8];
/* 0x30079c94 .rdata refs=3 width=imm first=0x3003719a owner=g_damage */
extern const char cg_scoreboardTeamNameLocalizationContext[21];
/* 0x30079cac .rdata refs=1 width=imm first=0x3003721c owner=g_damage */
extern const char cg_scoreboardAxisTeamNameCvarName[16];
/* 0x30079cbc .rdata refs=2 width=imm first=0x30037205 owner=g_damage */
extern const char cg_scoreboardAxisBannerCvarName[20];
/* 0x30079cd0 .rdata refs=2 width=imm first=0x30037244 owner=g_damage */
extern const char cg_scoreboardNoneBannerCvarName[20];
/* 0x30079dc4 .rdata refs=6 width=imm first=0x3003621c owner=g_getactivateent */
extern const char cg_blackMaterialName[6];
/* 0x30079f90 .rdata refs=1 first=0x3003a981: "ui_scriptMenuIndex" cvar name. */
extern const char g_str_ui_scriptMenuIndex[19];
/* 0x30079fa4 .rdata refs=1 first=0x3003a96f: "ui_scriptMenu" cvar name. */
extern const char g_str_ui_scriptMenu[14];
/* 0x30079fb4 .rdata refs=4 first=0x3003a7b0: "ui_waitingScriptMenuNoMouse" cvar name. */
extern const char g_str_ui_waitingScriptMenuNoMouse[28];
/* 0x30079fd0 .rdata refs=1 first=0x3003a767: "cmd mr %s noop\n" — the menu-response
 * "noop" console command emitted by CG_OpenScriptMenu (0x3003a5b0) to release a
 * previously-waiting script menu. Repaired from the mechanical uint32_t truncation
 * (owner tag bg_calculateweaponposition_gunreco is the rejected size-match name). */
extern const char g_str_cmd_mr_noop_fmt[16];
/* 0x30079fe0 .rdata refs=5 first=0x3003a755: "ui_waitingScriptMenuIndex" cvar name. */
extern const char g_str_ui_waitingScriptMenuIndex[26];
/* 0x30079ffc .rdata refs=5 first=0x3003a71b: "ui_waitingScriptMenu" cvar name. */
extern const char g_str_ui_waitingScriptMenu[21];
/* 0x3007a014 .rdata refs=3 first=0x3003a6d5: "UIMENU_SCRIPT_POPUP" menu name. */
extern const char g_str_UIMENU_SCRIPT_POPUP[20];
/* 0x3007a028 .rdata refs=3 first=0x3003a6ce: "UIMENU_SCRIPT_POPUP_NO_MOUSE" menu name. */
extern const char g_str_UIMENU_SCRIPT_POPUP_NO_MOUSE[29];
/* 0x3007a048 .rdata refs=5 first=0x3003a6ba: "ui_newScriptMenuIndex" cvar name. */
extern const char g_str_ui_newScriptMenuIndex[22];
/* 0x3007a060 .rdata refs=5 first=0x3003a6a1: "ui_newScriptMenu" cvar name. */
extern const char g_str_ui_newScriptMenu[17];
/* 0x3007a074 .rdata refs=1 first=0x3003a62e: "Server tried to open a non-loaded
 * script menu index: %i\n" — Com_Printf diagnostic emitted by CG_OpenScriptMenu
 * (0x3003a5b0) when the requested script-menu config string is empty. Repaired from
 * the mechanical uint32_t truncation (owner tag is the rejected size-match name). */
extern const char g_str_scriptMenuNotLoadedFmt[57];
/* 0x3007a0b0 .rdata refs=2 first=0x3003a639: "cmd mr %i bad\n" — the menu-response
 * "bad" console command emitted by CG_OpenScriptMenu (0x3003a5b0) back to the
 * server for an out-of-range or non-loaded script-menu index. Repaired from the
 * mechanical uint32_t truncation (owner tag is the rejected size-match name). */
extern const char g_str_cmd_mr_bad_fmt[15];
/* 0x3007a0c0 .rdata refs=1 first=0x3003a7d7: "Server tried to open a bad script menu
 * index: %i\n" — Com_Printf diagnostic emitted by CG_OpenScriptMenu (0x3003a5b0)
 * when the requested script-menu index is out of the [0, 32) range. Repaired from
 * the mechanical uint32_t truncation (owner tag is the rejected size-match name). */
extern const char g_str_scriptMenuBadIndexFmt[50];
/* 0x3007a0f4 .rdata refs=1 width=imm first=0x3003a56e owner=bg_canitembegrabbed */
extern const char cg_voiceChatPraiseCommandName[7];
/* 0x3007a0fc .rdata refs=1 width=imm first=0x3003a559 owner=bg_canitembegrabbed */
extern const char cg_voiceChatGauntletKillCommandName[14];
/* 0x3007a10c .rdata refs=1 width=imm first=0x3003a544 owner=bg_canitembegrabbed */
extern const char cg_voiceChatDeathInsultCommandName[13];
/* 0x3007a11c .rdata refs=1 width=imm first=0x3003a52f owner=bg_canitembegrabbed */
extern const char cg_voiceChatTauntCommandName[6];
/* 0x3007a124 .rdata refs=1 width=imm first=0x3003a51a owner=bg_canitembegrabbed */
extern const char cg_voiceChatKillInsultCommandName[12];
/* 0x3007a340 .rdata refs=2 first=0x300387d0.
 * "Could not load script menu file '%s'\n" — the load-failure format used by the
 * two config-string menu registrars CG_RegisterConfigStringMenu (0x30038790) and
 * its sibling at 0x30038826 when trap(CG_R_REGISTERMENU, name) returns zero. The
 * mechanical owner=q_stricmp is a wrong size-guess first-touch label. */
extern const char cg_couldNotLoadScriptMenuFmt[38];
/* 0x3007a368 .rdata refs=1 width=imm first=0x30038408. "maps/mp/%s.bsp" format
 * used by CG_ParseServerinfo (0x30038380). owner=pm_weapon_addfiringaimspreadscale
 * was a wrong size-guess first-touch label and is rejected. */
extern const char mpMapBspPathFormat[15];
/* 0x3007a378 .rdata refs=1 width=imm first=0x300383e4. "sv_maxclients" info key,
 * used by CG_ParseServerinfo. owner=pm_weapon_addfiringaimspreadscale rejected. */
extern const char sv_maxClientsInfoKey[14];
/* 0x3007a388 .rdata refs=1 width=imm first=0x30038394. "sv_hostname" info key,
 * used by CG_ParseServerinfo. owner=pm_weapon_addfiringaimspreadscale rejected. */
extern const char sv_hostnameInfoKey[12];
/* 0x3007a394 .rdata refs=1 width=imm first=0x3003c4e5 owner=cg_getviewfov */
extern const char cg_shellshockEndAliasName[15];
/* 0x3007a3a4 .rdata refs=1 width=imm first=0x3003c42d owner=cg_getviewfov */
extern const char cg_shellshockSilentLoopAliasName[23];
/* 0x3007a3bc .rdata refs=1 width=imm first=0x3003c418 owner=cg_getviewfov */
extern const char cg_shellshockLoopAliasName[16];
/* 0x3007a3cc .rdata refs=2 width=imm first=0x3003c133 owner=player_getmethod */
extern const char cg_shellshockEndAbortAliasName[21];
/* 0x3007a3e4 .rdata refs=2 width=imm first=0x3003c112 owner=player_getmethod */
extern const char cg_genericShellshockAliasName[8];
/* 0x3007a4e0 / 0x3007a4f8: diagnostics owned by CG_DrawActiveFrame. */
extern const char cg_clientFrameDiagnostic[];
extern const char cg_invalidWeaponSelectWarning[];
/* 0x3007a698 .rdata: fatal view-builder diagnostic. The original spelling is
 * "Cinimatic"; preserve the binary string exactly. Repaired from the mechanical
 * first-dword scalar after CG_CalcViewValues proved the whole string object. */
extern const char cg_cinematicCameraUnavailableMessage[52];
/* 0x3007a6cc: fatal diagnostic used when a turret lacks tag_player. */
extern const char cg_turretMissingTagPlayerError[33];
/* 0x3007a850 .rdata refs=2 width=imm first=0x30047c10 owner=script_method_scriptbuiltin_detach */
extern const char cg_weaponIndexOutOfRangeErrorMessage[48];
/* 0x3007a8bc .rdata refs=1 width=imm first=0x30046a55 owner=scr_objective_current */
extern const char cg_weaponSlotCvarNameFormat[14];
/* 0x3007a8cc .rdata refs=3 width=imm first=0x3004611f owner=item_listbox_paint */
extern const char cg_brassEjectTagName[10];
/* 0x3007a8d8 .rdata: CG_FakeTrajectoryEffects start-solid diagnostic. */
extern const char cg_fakeTrajectoryStartedInSolidMessage[29];
/* 0x3007a8f8 .rdata refs=1 width=imm first=0x30044861 owner=bg_checkpronevalid */
extern const char cg_registerWeaponWarnAiOverlay[];
/* 0x3007a940 .rdata refs=1 width=imm first=0x30044848 owner=bg_checkpronevalid */
extern const char cg_registerWeaponErrorAiOverlay[];
/* 0x3007a97c .rdata refs=1 width=imm first=0x300447ff owner=bg_checkpronevalid */
extern const char cg_registerWeaponWarnModeName[];
/* 0x3007a9b8 .rdata refs=1 width=imm first=0x300447e6 owner=bg_checkpronevalid */
extern const char cg_registerWeaponErrorModeName[];
/* 0x3007a9e8 .rdata refs=1 width=imm first=0x3004479d owner=bg_checkpronevalid */
extern const char cg_registerWeaponWarnDisplayName[];
/* 0x3007aa28 .rdata refs=1 width=imm first=0x30044784 owner=bg_checkpronevalid */
extern const char cg_registerWeaponErrorDisplayName[];
/* 0x3007aa5c .rdata refs=1 width=imm first=0x30044646 owner=bg_checkpronevalid */
extern const char cg_registerWeaponInvalidProjectileModel[];
/* 0x3007aae0 .rdata refs=1 width=imm first=0x30043bf3 owner=bg_checkpronevalid */
extern const char cg_registerWeaponWorldModelWarning[];
/* 0x3007ab18 .rdata refs=2 width=imm first=0x30043a95 owner=bg_checkpronevalid */
extern const char cg_registerWeaponLoopingAdsError[];
/* 0x3007ab58 .rdata refs=1 width=imm first=0x3004369e owner=bg_checkpronevalid */
extern const char cg_registerWeaponMissingIdleError[];
/* 0x3007ab8c .rdata refs=1 width=imm first=0x3004367f owner=bg_checkpronevalid */
extern const char cg_registerWeaponMissingHandError[];
/* 0x3007abc4 .rdata refs=1 width=imm first=0x30042f45 owner=CG_WeaponRunXModelAnims.
 * Repaired mechanical global: the first-touch bytes are ASCII 'C','G','_','W'
 * (0x575f4743 LE), the head of the Com_Printf format string
 * "CG_WeaponRunXModelAnims: Unknown weapon animation %i\n" (dumped via objdump
 * -s -j .rdata @0x3007abc4). PUSHed by CG_WeaponRunXModelAnims' unknown-anim path. */
extern const char cg_weaponRunXModelAnimsInvalidAnimFmt[54];
/* 0x3007ace0 .rdata refs=1 first=0x3004d3b7 (mechanical owner g_dropartillery is
 * wrong). Resolved on consume by Com_ScriptError (0x3004d370): a NUL-terminated
 * .rdata format string, not a uint32 — the mechanical export truncated it to its
 * first dword. Its bytes are 0x15 followed by "File %s, line %i: %s"; the leading
 * 0x15 is a CoD console error/channel marker byte that Com_Error emits verbatim.
 * This is the error-path twin of the plain "File %s, line %i: %s" at 0x3007acc8
 * used by Com_ScriptWarning. */
extern const char com_scriptErrorWithSourceFormat[22];
/* 0x3007ad20 .rdata: the Com_BeginParseSession session-overflow error text. The
 * mechanical export truncated this NUL-terminated string to its first dword
 * (0x6d6f4315); it is really "\x15Com_BeginParseSession: session overflow"
 * (leading 0x15 = CoD console error-channel marker). Consumed only by
 * Com_BeginParseSession (0x3004d250) as the Com_Error format string. */
extern const char com_parseSessionOverflowErrorMessage[];
/* 0x3007afc8 .rdata — va() overflow message. Mechanical export truncated it to
 * its first dword (0x74744115); it is a NUL-terminated string consumed by va
 * (refs=1). Leading 0x15 is the CoD console error-channel marker byte. */
extern const char va_overrunErrorString[];
/* 0x3007b54c .rdata refs=1 width=4 first=0x30056ab9 owner=script_func_setwinningplayer */
/* 0x3007b550 .rdata refs=1 width=4 first=0x30056aae owner=script_func_setwinningplayer */
/* "%f" printf format for a single float/double; friendly alias for the sibling of
 * the ordinary "%i" literal. Used by Item_Multi_HandleKey (0x30053d10) to write a fractional
 * numeric multi setting via va()/DC->setCVar. */
/* 0x3007bc40 .rdata: NUL-terminated String_Alloc pool-overflow fatal-error format
 * "\x15String_Alloc: \x14EXE_ERR_OUT_OF_MEMORY" (Com_Error format at 0x3004ff0c).
 * Mechanical export truncated it to its first dword (0x72745315); superseded. */
extern const char stringAlloc_outOfMemory_msg[];
/* 0x3007bc68 .rdata: NUL-terminated String_Alloc pool-overflow diagnostic format
 * "String_Alloc: failed to allocate %d bytes\n" (Com_Printf format at 0x3004ff02).
 * Mechanical export truncated it to its first dword (0x69727453); superseded. */
extern const char stringAlloc_allocFailed_fmt[];
/* 0x3007bc94 .rdata: NUL-terminated UI_Alloc pool-overflow fatal-error format
 * "\x15UI_Alloc: \x14EXE_ERR_OUT_OF_MEMORY" (Com_Error format at 0x3004fd75).
 * Mechanical export truncated it to its first dword (0x5f495515); superseded. */
extern const char uiAlloc_outOfMemory_msg[];
/* 0x3007bcb8 .rdata: NUL-terminated UI_Alloc pool-overflow diagnostic format
 * "UI_Alloc: failed to allocate %d bytes\n" (Com_Printf format at 0x3004fd61).
 * Mechanical export truncated it to its first dword (0x415f4955) and mislabeled it
 * a float; superseded. */
extern const char uiAlloc_allocFailed_fmt[];
/* 0x3007bce0 .rdata refs=472 width=4 first=0x30005065 owner=cg_drawweaponselect */
extern const float floatOne;
/* 0x3007bce8 .rdata refs=155 width=4 first=0x300043a4 owner=bg_setupsharedammoindexes */
extern const float floatOneHalf;
/* 0x3007bcec .rdata refs=576 width=4 first=0x30003909 owner=cg_registersounds */
extern const float floatZero;
/* 0x3007bcf8 .rdata refs=66 width=8 first=0x3000640a owner=pm_weapon_startfiring.
 * Superseded from the mechanical uint8_t[8] byte capture: the eight bytes
 * 00 00 00 00 00 00 f0 3f are the IEEE-754 double 1.0. Every reference FLDs/FCOMPs
 * it as an 8-byte double (e.g. FLD double ptr [0x3007bcf8] at 0x3003533a in
 * CG_Trace), so it is the shared double constant 1.0. */
extern const double doubleOne;
/* 0x3007bd64 .rdata refs=20 width=4 first=0x30008e8c owner=cg_drawcrosshairnames.
 * The 4-byte pattern 0x437f0000 is the IEEE-754 float 255.0f; every recovered
 * reference reads it via `FMUL/FCOMP float ptr`, so it is typed `const float`
 * (the mechanical uint32_t-bit-pattern export is superseded). The standard
 * normalized-color -> byte scale (color in [0,1] * 255). */
extern const float colorByteScale;
/* 0x3007bd70 .rdata refs=24 width=4 first=0x3000cc94 owner=veh_findvaliddismountspot.
 * deg2rad = M_PI/180 = 0.017453292f, the engine degrees->radians scale. Used only as
 * an FMUL float operand (proves float width; supersedes mechanical uint32_t). */
extern const float deg2rad;
/* 0x3007bdb4 .rdata refs=20 width=4 first=0x3000416d owner=launchitem */
extern const float floatOneHundredth;
/* 0x3007be24 .rdata, binary32 0x3b808081: packed color byte -> [0,1]. */
extern const float colorByteToUnitScale;
/* 0x3007be44 .rdata refs=6 width=4 first=0x30005f81 owner=veh_updateweapon */
extern const float twoPi;
/* 0x3007be48 .rdata refs=2 width=4 first=0x3001531c owner=bg_gettotalammoreserve */
extern const float bobCycleRadiansPerStep;
/* 0x3007be4c .rdata refs=2 width=4 first=0x30013bcb owner=parseconfigstringtostruct */
extern const float weaponIdleAirborneAdd;
/* 0x3007be58 .rdata, binary32 0x3e800000. */
extern const float floatOneQuarter;
/* 0x3007be70 .rdata refs=12 width=4 first=0x30005234 owner=cg_drawweaponselect */
extern const float floatThreeHalves;
/* 0x3007be78 .rdata refs=9 width=4 first=0x3001949e owner=cg_parseimpacteffects.
 * = pi/360 (0.0087266462): the "degrees -> half-angle-in-radians" constant used
 * to feed FPTAN with (fov_degrees * pi/360) = half the FOV in radians. Read only
 * via FMUL (float); retyped from the mislabeled uint32_t bit-pattern placeholder. */
extern const float DEG_TO_HALF_RAD;
/* 0x3007bf80 .rdata, binary32 0x3eaaaaab. */
extern const float floatOneThird;
/* 0x3007c018 .rdata refs=8 width=4 first=0x30019b5b owner=stuckinclient.
 * A float constant (bit pattern 0x3e19999a == 0.15f), FMUL'd as `float ptr`, not an
 * integer bit pattern — repaired from the mechanical uint32_t capture to a real
 * float per the shared-decl repair policy. CG_AddMarks (0x3002e8c0) uses it as the
 * fade-window fraction: the last duration*0.15 ms of a mark's life is its fade. */
extern const float markFadeFraction;
/* 0x3007c090 .rdata refs=2 width=4 first=0x300402db.
 * = 360/pi (114.59155): converts a half-angle-in-radians (from FPATAN) back to a
 * full-circle degree scale. Read only via FMUL (float); retyped from the
 * mislabeled uint32_t placeholder. owner=veh_setupcollmap was a wrong size-match. */
extern const float HALF_RAD_TO_DEG;
/* 0x3007c094 .rdata refs=1 width=4 first=0x30040300.
 * = 2*pi/2500 (0.0025132743): angular rate for a 2500 ms sine pulse cycle
 * (sin(cg_time * 2*pi/2500)). Read only via FMUL (float); retyped from the
 * mislabeled uint32_t placeholder. owner=veh_setupcollmap was a wrong size-match. */
extern const float PULSE_RATE_2500MS;
/* 0x30082050 .data refs=5 first=0x3000122c (mechanical owner is wrong). The name
 * of the source currently being parsed/loaded; a char* string pointer (the export
 * captured the pointer bits as a scalar). When non-NULL, BG_AnimParseError (0x30001200)
 * uses its file/line-context template and emits this as the "(%s, ...)" source
 * name; it is also passed as the filename to the engine file syscall (0x300024a0).
 * Statically initialized to "mp/playeranim.script" (.rdata 0x30072cf8). */
extern const char *bgPlayerAnimScriptPath;
extern const char *const *cg_eventNamesPtr;
/* 0x30082774 .data: cg_eventNames[] — the table of event-name strings CG_EntityEvent
 * (0x30022810) indexes by event id for its debug trace ("CG_EntityEvent:%s") and its
 * "Unknown event: '%s'" diagnostic. An array of const char * pointers; retyped from
 * the mechanical single uint32_t (the exporter captured only element 0's pointer).
 * owner=cmd_callvote_f is a first-touch/size-guess artifact (this is CG_EntityEvent).
 * refs=6. The complete original table contains EV_MAX_EVENTS entries. */
extern const char *cg_eventNames[EV_MAX_EVENTS];
/* 0x3008277c/88/94 .data: the three vehicle-position offset vec3_t returned by
 * BG_GetVehiclePosOffset (0x300081e0). Each is a full 12-byte vec3 (repaired from the
 * mechanical single-dword uint32_t globals mis-owned to scr_localizationerror):
 *   bgVehicleArtilleryPositionOffset = {0,-24,0}
 *   bgVehicleTankPosition2Offset = {10,5,-39}
 *   bgVehicleTankPosition1Offset = {0,0,-20}
 * default case returns &vec3_origin {0,0,0}. The storage is shared through
 * bg_vehicle.h. */
/* 0x300827a0..0x300840c0 .data: bg_itemlist[134], the shared 0x30-byte item
 * definition table. CG_RegisterItemVisuals and BG_CanItemBeGrabbed consume the
 * ordinary gitem fields. BG_FillInWeaponItems fills weapon rows from weaponInfo_t
 * and resolves pre-seeded ammo rows through weapon/ammoIndex/clipIndex at
 * +0x24/+0x28/+0x2c. Its ESI base is row+0x24 at 0x30010486; the former
 * +0x00/+0x04/+0x08 union interpretation omitted that LEA displacement. */
extern gitem_t bg_itemlist[134];
extern const pmLerpEntry_t pm_viewHeightCrouchToProneLinear[3];
/* 0x300842e4: weapon-definition directory prefix, initialized to "weapons/mp". */
extern const char *bg_weaponDefsPath;
extern const vec4_t cg_hudColorTable[4];
/* cg_vehicleWheelTags (0x300851ac, .data) — the fixed 6-entry table of vehicle
 * wheel-bone tag-name pointers that CG_Vehicle_DoControllers (0x30020540) walks.
 * Each entry is itself the `const char *` passed to CG_DOBJ_GET_BONE_INDEX (0xb2):
 * the loop reads `tagName = *entryPtr` then issues trap(0xb2, self, tagName).
 * Proven from the walk at 0x30020d13
 * (base=0x300851ac) .. 0x30021323 with the terminator CMP against 0x300851c4 (6*4
 * bytes past the base). The first two entries (0x300851ac, 0x300851b0) are the
 * powered/steering wheels: they are special-cased twice (0x30020ef9, 0x30021201)
 * to apply the steering-angle / driven-torque contribution. The original pointer
 * values resolve to, in order: front-left, front-right, back-left, back-right,
 * middle-left, middle-right. Supersedes the mechanical
 * g_data_bg_animparseanimscript_300851ac (owner label was first-touching, not the
 * identity); the two mechanical symbols 300851ac/300851b0 are the first two array
 * cells of this one table. */
extern const char *const cg_vehicleWheelTags[6];
/* 0x300851c4 .data refs=4 width=imm first=0x3002132f owner=bg_animparseanimscript */
extern vec3_t cg_flameTraceMins;
/* 0x300851d0 .data refs=4 width=4 first=0x300244c4 owner=cg_addflamechunks */
extern vec3_t cg_flameTraceMaxs;
/* 0x300851dc .data refs=1 — cg_flameSpriteViewRelative: a boolean flag that selects
 * how CG_AddFlameSpriteToScene (0x300268e0) builds the flame billboard basis. When
 * nonzero (0x30026db1: TEST EAX,EAX / JZ) the function derives view angles from
 * cg_refdef.viewaxis[0] (vectoangles) and computes cg_flameSpriteRight/Up via
 * AngleVectors; when zero it copies the precomputed cg_flameSpriteSrcRight
 * (0x300a84f8) / cg_flameSpriteSrcUp (0x300ab744) instead. ROLE-RESOLVED from that sole
 * consumer; exact source name provisional (writer not reconstructed). */
extern uint32_t cg_flameSpriteState[5];
#define cg_flameSpriteViewRelative (cg_flameSpriteState[0])
/* 0x300851e0 .data refs=1 width=4 first=0x30027a99 owner=bg_finditem (mechanical
 * owner is wrong). Flame-subsystem dword zeroed as the final data step of
 * CG_InitFlameChunks (0x30027a99: MOV [0x300851e0],0). Only this single write is
 * visible in the recovered mcode (refs=1, no reader reconstructed), so the exact
 * role/name is unresolved; provisional-by-role as a flame-subsystem counter/state
 * reset to 0 at init. Retype/rename when its consumer is reconstructed. */
#define cg_flameInitStateReset (cg_flameSpriteState[1])
/* 0x300851f0 .data: this is the base of cg_cvarTable, the 184-entry cgame cvar
 * registration table (cgCvarTable_t[184], stride 0x10). The two former mechanical
 * scalars here (g_data_..._300851f0 and g_data_..._300851f8) each captured only the
 * first dword of one 0x10-byte entry as uint32_t and are superseded; the typed array
 * lives in client_recovered.h next to its element type cgCvarTable_t. Do not
 * re-add address-shaped scalars for addresses inside this table. */
/* 0x30085d70 .data — cg_numberShaderNames[11]: an initialized .rdata-pointer table of
 * the eleven HUD bitmap-number shader names ("gfx/2d/numbers/zero_32b".."nine_32b", then
 * "minus_32b"). CG_RegisterGraphics (0x3002ba50) walks it (MOV EAX,[ESI + 0x30085d70],
 * ESI = 0,4,..,0x28) to register each into cg_numberShaders[i]. Element count 11 matches
 * cg_numberShaders[11] and the loop bound. owner=pm_updateviewangles was a size-guess. */
extern const char *const cg_numberShaderNames[11];
/* 0x30085da0 .data — cg_stanceHintChangeTime: cg.time (ms) the player's stance/
 * snapshot flags last changed, or -1 when no snapshot. Consumed by CG_DrawPlayerStance.
 * Old owner label "bullet_fire_extended" was a wrong size-guess; superseded here. */
extern int32_t cg_stanceHintChangeTime;
/* 0x30085da4 .data — cg_stanceHintFlags: latched playerState snapshot flags masked
 * with 0x10003 (0x10000 flash/sprint, 0x1 prone, 0x2 crouch); picks the stance icon
 * and hint table. Consumed by CG_DrawPlayerStance. */
extern uint32_t cg_stanceHintFlags;
/* 0x30085da8 .data — cg_stanceHintExpireTime: cg.time (ms) until which the stance
 * hint/slide animation is active, or -1 when suppressed. Consumed by CG_DrawPlayerStance. */
extern int32_t cg_stanceHintExpireTime;
/* 0x30085dac .data — cg_statBarHoldSeed: the reset value (1 ms) loaded into
 * cg_statBarHoldTimer when the HUD stat bar's trailing indicator is re-seeded.
 * Read-only seed; the (owner=stopfollowing) label was a size-guess and is wrong.
 * Consumed only by CG_DrawStatBarWithDecay (0x3002f9d0). Provisional role name. */
extern int32_t cg_statBarHoldSeed;
/* 0x30085db0 .data — cg_statBarHoldTimer: countdown (ms) during which the stat
 * bar's trailing indicator holds its value before it begins to decay; decremented
 * by cg.frametime each frame and floored at 0. (owner=stopfollowing is wrong.)
 * Consumed only by CG_DrawStatBarWithDecay (0x3002f9d0). Provisional role name. */
extern int32_t cg_statBarHoldTimer;
/* 0x30085db4 .data — cg_statBarLastClientNum: the local playerState clientNum for
 * which the stat bar's smoothing state (cg_statBarDisplayFrac/HoldTimer) is valid;
 * when it changes the trailing bar snaps to the live value. Initialized to -1.
 * (owner=stopfollowing is wrong.) Consumed only by CG_DrawStatBarWithDecay. */
extern int32_t cg_statBarLastClientNum;
/* 0x30085db8 .data refs=2 — cg_scoreboardLeadTeam: which of the two scored teams
 * (TEAM_AXIS=1 / TEAM_ALLIES=2) currently leads, used as the tiebreak/latch that
 * decides which team's scoreboard section draws first. CG_DrawScoreboardBody
 * (0x30037b50) compares cg_scoreboardTeamScores[AXIS] vs [ALLIES]: the higher
 * aggregate leads and is stored here (0x30037c43 MOV [0x30085db8],EBX); on an exact
 * tie the previously latched value is reused (0x30037c29 MOV EBX,[0x30085db8]), so
 * the lead order is stable across frames. Both refs are in that one function.
 * Mechanical owner label cg_entitypreevent was the exporter's wrong size-match
 * guess. Signed team_t; named by proven role. */
extern int32_t cg_scoreboardLeadTeam;
/*
 * cg_shockParamNames / cg_shockParamTargets (0x30085dc0 / 0x30085e30) — the
 * shellshock (.shock) parameter definition tables. Each entry i pairs the
 * cvar name cg_shockParamNames[i] (a cg_shock_* / cg_shock_mouse_* cvar) with a
 * pointer cg_shockParamTargets[i] into cgame's vmCvar storage. CG_ShellShockLoad
 * (0x3003b950) loads "scripts/<name>.shock", parses it against the 27 names via
 * CG_COM_LOAD_CVARS_FROM_BUFFER (0x30085dc0, 27, text, path), then applies CG_CVAR_UPDATE to each of the
 * 27 target pointers to finalize/register the resulting cvar. The name table is
 * also referenced by CG_ShellShockSave (0x3003b8a2), the write-side counterpart:
 * it serializes the current values of the 27 named cvars into a text buffer via
 * CG_COM_SAVE_CVARS_TO_BUFFER (cg_shockParamNames, 27, buf, 0x10000) and writes it back out to
 * "scripts/<name>.shock". CG_ShellShockSave does not reference the target table.
 *
 * The mechanical export truncated each table to its first dword
 * (g_data_..._30085dc0 = 0x30078bc8, g_data_..._30085e30 = 0x30456e60) with
 * misleading owners; superseded here in place by the real 27-entry arrays.
 * Target elements are typed vmCvar_t pointers to 27 scattered cvar objects. Extent (27)
 * proven by the count 0x1b passed to CG_COM_LOAD_CVARS_FROM_BUFFER and the loop limit 0x6c
 * (27 * 4 bytes) over the target table in CG_ShellShockLoad. Storage in globals.c.
 */
#define CG_SHOCK_PARAM_COUNT 27
extern const char *const cg_shockParamNames[CG_SHOCK_PARAM_COUNT]; /* 0x30085dc0 */
extern vmCvar_t *const cg_shockParamTargets[CG_SHOCK_PARAM_COUNT]; /* 0x30085e30 */
/* 0x30085ea0 .data refs=3 width=imm first=0x3003f7bb owner=scr_vehicle_damagescale */
extern uint32_t cg_vehicleDamageBoundsMinsBits[3];
/* 0x30085eac .data refs=1 width=imm first=0x3003f7df owner=scr_vehicle_damagescale */
extern uint32_t cg_vehicleDamageBoundsMaxsBits[3];
/* 0x30085eb8 .data refs=1 width=imm first=0x3003fd06 owner=pm_weapon_finishweaponchange */
extern uint32_t cg_weaponChangeBoundsMinsBits[3];
/* 0x30085ec4 .data refs=1 width=imm first=0x3003fcc0 owner=pm_weapon_finishweaponchange */
extern uint32_t cg_weaponChangeBoundsMaxsBits[3];
extern float cg_weaponChangeAngleLimit;
/* 0x30085ed4 .data refs=1 width=imm first=0x30041d52 owner=pm_ufomove */
extern uint32_t cg_ufoMoveBoundsMinsBits[3];
/* 0x30085ee0 .data refs=1 width=imm first=0x30041d72 owner=pm_ufomove */
extern uint32_t cg_ufoMoveBoundsMaxsBits[3];
/* 0x30085eec .data refs=6 width=4 first=0x300463f5 owner=item_listbox_paint.
 * const char *[6]: the muzzle/flash bone-tag name table -- "tag_flash", "tag_flash_11",
 * "tag_flash_2", "tag_flash_22", "tag_altfire", "tag_secondary_flash". Read at element 0
 * (0x300463f5/0x3004642f/0x30049087, plain "tag_flash") and indexed
 * (0x30047e15/0x30047ed6). Handed as the tagName to CG_CalcMuzzlePoint / CG_SpawnTracer.
 * Entry [6] onward is a distinct float pool (-4.0f...), not part of this table. */
extern const char *cg_muzzleTagNames[6];
extern const float cg_muzzleEffectBoundsAndBias[7];
/* 0x30085efc .data refs=2 width=4 first=0x30047ee9 owner=pm_updatelean */
/* 0x30085f00 .data refs=3 width=4 first=0x30045ad4 owner=cg_drawplayerstance */
/* Item_RunScript owns the shared 30-entry menu-command table. */
/* 0x3008bd61 .data refs=2 width=1 first=0x30063dea owner=g_freeentityrefs */
/* 0x3008bf34 .data refs=3 width=4 first=0x300020c8 owner=cg_drawfps */
/* 0x3008bf34 resolved to bgAnimParseCurrentEvent (declared in client_recovered.h);
 * mechanical g_data_cg_drawfps_3008bf34 superseded, storage in globals.c. */
/* 0x3008bf38 .data: bgAnimConditionAliases — the per-condition value-name lookup
 * tables of the BG animation condition system (owner label cmd_give_f is wrong;
 * proven consumer is BG_ParseConditionBits 0x30001920). One flat
 * bg_indexed_string_t array; condition `c` owns a 16-entry window starting at
 * index c*16, so BG_ParseConditionBits passes &bgAnimConditionAliases[c*16] as a
 * NULL-name-terminated BG_IndexForString table (LEA base = 0x3008bf38 + c*0x80).
 * Extent 0x580 bytes = 11*16 entries, proven by the registration routine
 * (0x30002552: `MOV ECX,0x160; MOV EDI,0x3008bf38; REP STOS` zeroes 0x160=352
 * dwords = 0x580 bytes) and by BG_ANIM_MAX_CONDITIONS==11. The registration code
 * fills entries flat via [idx*8+0x3008bf38] (idx = c*16 + valueIndex).
 * Zero-initialized in .data. */
extern bg_indexed_string_t bgAnimConditionAliases[BG_ANIM_MAX_CONDITIONS * BG_ANIM_CONDITION_VALUE_COUNT];
/* 0x3008bf3c .data refs=1 width=4 first=0x30002789 owner=cg_drawcrosshair */
/* 0x3008c4b8 .data refs=3 width=4 first=0x3000202c owner=cg_drawfps */
/* 0x3008c4b8 resolved to bgAnimParseCurrentAnimGroup (declared in client_recovered.h);
 * mechanical g_data_cg_drawfps_3008c4b8 superseded, storage in globals.c. */
/* 0x3008c4bc .data: bgAnimConditionAliasCounts[BG_ANIM_MAX_CONDITIONS] — the running
 * count of registered value names for each animation condition type, filled by the
 * "defines" section of BG_AnimParseAnimScript (0x30002470): it is cleared (0xb
 * dwords at 0x30002565 `MOV ECX,0xb; MOV EDI,0x3008c4bc; REP STOS`), indexed by
 * condition type via [condType*4 + 0x3008c4bc], and incremented once per parsed
 * "set" define. It gives the fill index into bgAnimConditionAliases /
 * bgAnimConditionAliasBits for that condition type. Mechanical owner cg_drawcrosshair
 * is the wrong first-toucher; superseded here. Zero-initialized in .data. */
extern int32_t bgAnimConditionAliasCounts[BG_ANIM_MAX_CONDITIONS];
/* 0x3008c4e8 .data: bgAnimConditionAliasBits — parallel to bgAnimConditionAliases.
 * Each condition value (condition c, value index v) maps to a 64-bit condition
 * mask stored as two int32 words: lo at [+0x000], hi at [+0x004]. Indexed flat by
 * (c*16 + v), stride 8 bytes: BG_ParseConditionBits (0x30001920) reads
 * bgAnimConditionAliasBits[c*16 + v].bits[0] at 0x3008c4e8 (owner label cmd_give_f is
 * wrong) and .bits[1] at 0x3008c4ec once BG_IndexForString resolves the value
 * name to index v. Extent 11*16 = 176 entries (BG_ANIM_MAX_CONDITIONS *
 * BG_ANIM_CONDITION_VALUE_COUNT), matching bgAnimConditionAliases. Zero-init. */
extern bg_condition_bits_t bgAnimConditionAliasBits[BG_ANIM_MAX_CONDITIONS * BG_ANIM_CONDITION_VALUE_COUNT];
/* 0x3008ca68 .data: bgAnimScriptFileBuffer — the static text buffer the player
 * animation script file ("mp/playeranim.script") is read into once by
 * BG_AnimParseAnimScript (0x30002470) via the FS_READ trap, then NUL-terminated
 * and tokenized by the Com parser. Capacity is exactly 0x186a0 (100000) bytes:
 * the next .data object is 0x300a5108, and the loader rejects file lengths
 * >= 0x1869f (99999) before writing the trailing NUL at buffer[fileLen].
 * Mechanical owner cg_drawcrosshair is the wrong first-toucher; superseded here. */
enum {
    BG_ANIM_SCRIPT_TEXT_SIZE = 0x186a0
};
extern char bgAnimScriptFileBuffer[BG_ANIM_SCRIPT_TEXT_SIZE];
/* 0x300a510c .data: bgAnimConditionAliasStringUsed — the running byte cursor into
 * bgAnimConditionAliasStringBuffer, passed by address to BG_CopyStringIntoBuffer as the pool
 * write index. Cleared to 0 at the start of each BG_AnimParseAnimScript
 * (0x30002579). Mechanical owner cg_drawcrosshair is the wrong first-toucher;
 * superseded here. Zero-initialized in .data. */
extern int32_t bgAnimConditionAliasStringUsed;
/* 0x300a5110 .data: bgAnimConditionAliasStringBuffer — the linear string pool that holds the
 * copied condition-value define names for the BG animation condition tables
 * (bgAnimConditionAliases[].name point into it). 0x2710 bytes: BG_AnimParseAnimScript
 * clears it (0x3000255e `MOV ECX,0x9c4; MOV EDI,0x300a5110; REP STOS` = 0x2710
 * bytes) and passes it plus size 0x2710 to BG_CopyStringIntoBuffer; the next .data
 * symbol is 0x300a7820 (0x300a5110 + 0x2710). Mechanical owner cg_drawcrosshair is
 * the wrong first-toucher; superseded here. Zero-initialized in .data. */
extern char bgAnimConditionAliasStringBuffer[BG_ANIM_CONDITION_ALIAS_STRING_BUFFER_SIZE];
/*
 * MAX_AMMO_TYPES — number of distinct ammo/clip types. playerState_t.ammo[] and
 * .clips[] are each this long (server ammo[128]/clips[128]); the bg_ammoTypeMax[],
 * bg_sharedAmmoCapSizes[], and bg_ammoClipSizes[] index tables below share this domain.
 */
/* 0x300a7828 .data — bg_ammoClipNames[MAX_AMMO_TYPES]: the clip-type dedup
 * name table, parallel to the bg_ammoClipSizes[] value table (0x300a8238) and the
 * bg_numAmmoClips counter (0x300a7e28). InitWeaponInfo (0x30010df0) zeroes all
 * 128 entries, then reserves index 0 as bg_ammoClipNames[0] = "none" with
 * bg_numAmmoClips = 1; the clip dedup pass BG_SetupClipIndexes (0x300109a0) appends
 * each distinct clip name (writing bg_ammoClipNames[i] at 0x30010bd4 and
 * bg_ammoClipSizes[i] at 0x30010be7) and bumps the counter. Elements are const char*
 * (weapon name pointers). The mechanical owner=cg_shakecamera label is the
 * first-touching function, not the identity. */
extern const char *bg_ammoClipNames[MAX_AMMO_TYPES];
/* 0x300a7a28 .data (refs=5): bg_ammoTypeMax — per-ammo-type maximum reserve ammo,
 * indexed by weaponInfo_t::ammoIndex (+0x1e8), same domain as playerState_t::ammo[]
 * (+0x134). Filled by the ammo-type dedup pass (BG_SetupAmmoIndexes) as
 * bg_ammoTypeMax[ammoIndex] = weaponInfo->maxAmmo (+0x1f4); read by
 * BG_GetAmmoTypeMax (0x30010fb0) and BG_GetMaxPickupableAmmo (0x300116b0). Element
 * type int32_t (signed subtractions / FIDIV at 0x300317b7); the mechanical export
 * truncated it to its first element and the owner=pm_checkjump label is the
 * first-touching function, not the identity. Sized to the ammo-type index domain. */
extern int32_t bg_ammoTypeMax[MAX_AMMO_TYPES];
/* 0x300a7c28 .data — bg_sharedAmmoCapNames[MAX_AMMO_TYPES]: the shared-ammo-pool
 * dedup NAME table, parallel to the bg_sharedAmmoCapSizes[] value table (0x300a7e38)
 * and the bg_numSharedAmmoCaps counter (0x300a7e2c). BG_SetupSharedAmmoIndexes
 * (0x300107e0) is the sole writer/reader: it case-folds each weapon's
 * weaponInfo_t::sharedAmmoCapName (+0x1fc), Q_stricmpn-matches it against these entries,
 * and on a miss appends bg_sharedAmmoCapNames[n] = weapon->sharedAmmoCapName. Element
 * type const char*. (Mechanical owner=cg_vehicleownericon was a first-touch label.) */
extern const char *bg_sharedAmmoCapNames[MAX_AMMO_TYPES];
/* 0x300a7e28 .data — bg_numAmmoClips: count of distinct clip types registered in
 * the bg_ammoClipNames[]/bg_ammoClipSizes[] dedup tables. InitWeaponInfo
 * (0x30010df0) seeds it to 1 (index 0 == "none"); BG_SetupClipIndexes (0x300109a0,
 * the only other writer) bumps it as it appends each new clip type. Signed int (a
 * running index / count). Mechanical owner=cg_shakecamera is a first-touch label. */
extern int32_t bg_numAmmoClips;
/* 0x300a7e2c .data — bg_numSharedAmmoCaps: count of distinct shared-ammo pools
 * registered in the bg_sharedAmmoCapNames[]/bg_sharedAmmoCapSizes[] dedup tables.
 * BG_SetupSharedAmmoIndexes (0x300107e0) is the sole writer: it reads the count as
 * the search bound and bumps it by one each time it appends a new pool. Signed int
 * (a running index / count). (Mechanical owner=cg_vehicleownericon was a
 * first-touch label.) */
extern int32_t bg_numSharedAmmoCaps;
/* 0x300a7e30 .data — bg_numAmmoTypes: count of distinct ammo types registered in
 * the bg_ammoTypeNames[]/bg_ammoTypeMax[] dedup tables. InitWeaponInfo
 * (0x30010df0) seeds it to 1 (index 0 == "none"); BG_SetupAmmoIndexes (0x30010550,
 * the only other writer) bumps it as it appends each new ammo type. Signed int (a
 * running index / count). Mechanical owner=pm_checkjump is a first-touch label. */
extern int32_t bg_numAmmoTypes;
/* 0x300a7e34 .data refs=5 width=4 first=0x3000fd95.
 * cg_emptyString: cached pointer to a heap-allocated "" used as the CopyString
 * fast-path result. The weapon-registration setup at 0x3000ff23 allocates a
 * 1-byte buffer via cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT,...), stores a NUL into it, and
 * caches the pointer here; the CopyString helpers (0x3000fd90, CG_AllocWeaponInfo
 * 0x3000fe00, 0x30010100) return it verbatim when the source string is empty
 * instead of allocating. The mechanical owner=bg_getanimstring label is the
 * first-touching-function guess and does not name the datum. */
extern const char *cg_emptyString;
/* 0x300a7e38 .data (refs=4): bg_sharedAmmoCapSizes — per-shared-ammo-group ammo
 * capacity, indexed by weaponInfo_t::sharedAmmoCapIndex (+0x200). Filled by the
 * shared-ammo dedup pass (BG_SetupSharedAmmoIndexes) as
 * bg_sharedAmmoCapSizes[sharedAmmoCapIndex] = weaponInfo->sharedAmmoCap (+0x204); read
 * by BG_GetSharedAmmoCapSize (0x30010fe0) and BG_GetMaxPickupableAmmo (0x3001170e),
 * which treats it as the group cap it decrements each owned weapon's held
 * ammo/clips from. Element type int32_t; mechanical export truncated it to its
 * first element (owner=cg_vehicleownericon is the first-touching function). */
extern int32_t bg_sharedAmmoCapSizes[MAX_AMMO_TYPES];
/* 0x300a8038 .data — bg_ammoTypeNames[MAX_AMMO_TYPES]: the ammo-type dedup
 * name table, parallel to the bg_ammoTypeMax[] value table (0x300a7a28) and the
 * bg_numAmmoTypes counter (0x300a7e30). InitWeaponInfo (0x30010df0) zeroes all
 * 128 entries, then reserves index 0 as bg_ammoTypeNames[0] = "none" with
 * bg_numAmmoTypes = 1; the ammo-type dedup pass BG_SetupAmmoIndexes (0x30010550)
 * appends each distinct ammo name (writing bg_ammoTypeNames[i] at 0x30010784 and
 * bg_ammoTypeMax[i] at 0x30010797) and bumps the counter. Elements are const char*
 * (weapon name pointers). Mechanical owner=pm_checkjump is a first-touch label. */
extern const char *bg_ammoTypeNames[MAX_AMMO_TYPES];
/* 0x300a8238 .data (refs=7): bg_ammoClipSizes — per-clip-index clip capacity table.
 * The mechanical owner=cg_shakecamera label is only the first-touching function and
 * is NOT its identity. During weapon registration (0x30010be7/0x30010bee) the loader
 * assigns each distinct clip type a running clipIndex (counter at 0x300a7e28) and
 * stores bg_ammoClipSizes[clipIndex] = weaponInfo->clipSize (+0x1f8). It is read as
 * bg_ammoClipSizes[clipIndex] by the ammo/clip predicates, i.e. BG_GetAmmoClipSize
 * (int clipIndex). PM_Weapon_AllowReload (0x300132a0) compares the player's current
 * clip against this capacity. Element type int32_t (signed compares at 0x300132dc /
 * 0x300132fa). The mechanical export truncated this array to its first element;
 * sized MAX_AMMO_TYPES to match the clip-index domain (playerState clips[128]). */
extern int32_t bg_ammoClipSizes[MAX_AMMO_TYPES];
/* 0x300a8438 .data — cg_statsFrameTimes[32]: the rolling 32-slot ring of
 * per-frame elapsed-milliseconds samples maintained by CG_DrawFPS
 * (0x30018090). Each frame it stores (trap_Milliseconds() - previous) into
 * slot [frameCount & 31], then, once 32 samples exist, sums/min/maxes the whole
 * ring to derive the fps line. The mechanical export split the base
 * (owner=cg_drawweapreticle, a rejected size-guess name) and its +4 interior
 * (0x300a843c, only ever taken as the &array[1] loop cursor address) into two
 * dword symbols; unified here into the real int[32]. */
extern int32_t cg_statsFrameTimes[32];
/* 0x300a84b8 .data: last real-time (engine milliseconds) at which the screen-fade
 * animator (cg_fovFade) was advanced by CG_ScreenFade (0x3001a7c0). That evaluator
 * reads trap_Milliseconds(), computes deltaMs = now - this, advances the fade only
 * when 0 < deltaMs < 500, then stores the new "now" back here. Both refs (read at
 * 0x3001a800, write at 0x3001a887) are in CG_ScreenFade. The mechanical
 * owner=vectosignedangles label was a size-guess mislabel and is corrected. */
extern int32_t cg_screenFadeLastMs;
/* 0x300a84bc .data — cg_statsFrameCount: running count of frames sampled into
 * cg_statsFrameTimes[]. Incremented once per CG_DrawFPS call; while it is < 32
 * the routine only records a sample and bails (warmup). Consumed by CG_DrawFPS
 * (0x30018090). Mechanical owner=cg_drawweapreticle was a rejected size-guess. */
extern int32_t cg_statsFrameCount;
/* 0x300a84c0 .data — cg_statsPrevTimeMs: the trap_Milliseconds() value captured on
 * the previous CG_DrawFPS call; the current frame's ring sample is
 * now - cg_statsPrevTimeMs. Consumed by CG_DrawFPS (0x30018090). */
extern int32_t cg_statsPrevTimeMs;
/*
 * cg_rendererStats (0x300a84c4, .data) — the renderer performance-counter block
 * that CG_DrawFPS (0x30018090) fills each detailed pass via
 * cgame_syscall(CG_R_TRACK_STATISTICS, &cg_rendererStats) and then prints. Field roles are
 * proven from the CG_DrawFPS print sites and their exact offsets/widths; the
 * mechanical export split this one struct into nine owner=cg_drawweapreticle
 * dwords (0x300a84c4..0x300a84e4, a rejected size-guess name), unified here.
 * The shared type uses the renderer-side names proven by RE_TrackStatistics and
 * its producers.
 */
extern renderer_frame_statistics_t cg_rendererStats;
/* 0x300a84e8..0x300a84f4: CG_FadeColor's static return buffer (a vec4_t RGBA
 * color). RGB are forced to 1.0 and alpha holds the computed fade fraction;
 * CG_FadeColor (0x3001d200) returns &cg_fadeColor[0]. The mechanical export
 * split this vec4 into four owner=pm_addtouchent dwords (size-match misname,
 * rejected) and unified them here. */
extern vec4_t cg_fadeColor;
/* 0x300a84f8/fc/500 .data — cg_flameSpriteSrcRight: the RIGHT billboard basis vec3
 * used for third-person / world flames (as opposed to the local first-person flame,
 * which builds its own basis via AngleVectors). ROLE-RESOLVED (name provisional) from
 * CG_AddFlameSpriteToScene (0x300268e0): on the non-first-person path it copies this
 * triple into cg_flameSpriteRight (0x300ab72c) verbatim (0x30026de7..0x30026df8).
 * WRITER PROVEN: CG_FireFlameChunks (0x30027da4..0x30027dd3) copies cg_refdef.viewaxis[1]
 * (viewaxis[1]) into these three dwords once per frame. Retyped from three mechanical
 * uint32_t placeholders to the one vec3 they form. */
extern vec3_t cg_flameSpriteSrcRight;
/* 0x300a8508 .data refs=3 width=4 first=0x30027119 owner=cg_addflamespritetoscene.
 * RESOLVED: the flamethrower-fire sprite material handle table. CG_InitFlameChunks
 * (0x30027a24) fills entries 0..42 with the material registered for the shader name
 * "flamethrowerFire%i" (i = index+2, so "flamethrowerFire2".."flamethrowerFire44");
 * CG_AddFlameSpriteToScene (0x30027119) and the flame-fire path at 0x3002723d index
 * it as cg_flameFireMaterials[i] as the draw material. Element count 43 is proven by
 * the loop bound (EBX < 0x2b) and by the 0xac-byte gap to the next flame global
 * (0x300a85b4). Handles are cgame material/shader handles (registration trap 0x30). */
extern qhandle_t cg_flameFireMaterials[43];
/* 0x300a85b4 .data refs=5 width=4 first=0x30025592 owner=hudelem_destroyall.
 * RESOLVED: head of the flame-chunk FREE list. CG_ClearFlameChunks (0x30025592)
 * points it at node[0] after threading all 8192 nodes; CG_SpawnFlameChunk
 * (0x30025600) pops from it, CG_FreeFlameChunk (0x300256e0) pushes back onto it.
 * Free/active list threads through flameChunk_t.next (+0x0) / .prev (+0x4). */
extern flameChunk_t *cg_freeFlameChunks;
/* 0x300a85b8 .data — cg_flameDamageTrace: the module-static trace_t used as the
 * flame-damage-source trace record. RESOLVED from CG_MoveFlameChunk (0x30025da0),
 * which fills it via CG_FlamethrowerTrace(out=&cg_flameDamageTrace, ...) and reads
 * its fraction/endpos/normal/surfaceFlags/entityNum/startsolid fields to latch the
 * best flame-damage source for the frame.
 * Supersedes the ten mechanical dword aliases the exporter carved out of this one 48-byte
 * object (0x300a85b8/bc/c0/c4/c8/cc/d0/d4/e0/e7); the owner=cg_playerturretpositionandblend
 * label was the mis-sized first-touching function name, not the identity. The
 * original i386 object is the ordinary 0x30-byte trace_t, including the material
 * pointer at +0x24; keeping that real type also lets the pointer and following
 * fields move together on native 64-bit builds. refs were 6/5/4/4/3/2/3/2/1/2. */
extern trace_t cg_flameDamageTrace;
/* 0x300a85e8 .data — cg_rFullscreenCvar: the module-side vmCvar mirror handle for the
 * "r_fullscreen" cvar. RESOLVED from CG_FireFlameChunks (0x30027d59): its address is
 * pushed as the vmCvar arg to trap(7, &vmCvar, "r_fullscreen", "1", 0x21) on first
 * frame (register) and to trap(8, &vmCvar) on later frames (refresh). Compact 12-byte
 * cvar-handle slot (next .data symbol is 0x300a85f4), so it is NOT the classic
 * ~0x110-byte Q3 vmCvar_t; kept as the exported dword head, name resolved by role. */
extern vmCvar_t cg_rFullscreenCvar;
/* 0x300a85f4 .data refs=1 width=4 first=0x30026c58 owner=cg_addflamespritetoscene */
/* 0x300a86f8 .data refs=2 width=4 first=0x30027a54 owner=bg_finditem (mechanical
 * owner is wrong). RESOLVED: the flame-smoke sprite material handle table.
 * CG_InitFlameChunks (0x30027a54) fills entries 0..5 with the material registered
 * for the shader name "flameSmoke%i" (i = index+1, so "flameSmoke1".."flameSmoke6");
 * the flame path at 0x30028e5f indexes it as cg_flameSmokeMaterials[i]. Element
 * count 6 is proven by the loop bound (EBX < 6) and by the 0x18-byte gap to the next
 * global (0x300a8710). Handles are cgame material/shader handles (trap 0x30). */
extern qhandle_t cg_flameSmokeMaterials[6];
/* 0x300a8710 .data — cg_flameDamageBestPosTime: the cg_flameTime value stamped when
 * cg_flameDamageBestPos was last recorded. CG_FlameDamage (0x300265c0) sets
 * it to cg_flameTime after latching a new best flame source (0x300266b0), and uses
 * `== cg_flameTime` (0x3002666f) to decide whether a still-valid best position exists
 * this flame-frame, so nearby flame chunks within the damage radius are de-duplicated
 * against it. RESOLVED from that consumer; the mechanical owner=item_listbox_overlb was
 * the first-touching function, not the identity. refs=2. */
extern int32_t cg_flameDamageBestPosTime;
/* 0x300a8718 .data — cg_flameSoundLoops[]: the flame sound-loop envelope table
 * (base 0x300a8718, stride 12 bytes, FLAME_INFO_COUNT == 1024 elements, spanning
 * 0x300a8718..0x300ab718 exactly — its end abuts the flame-sprite globals at
 * 0x300ab718). RESOLVED from CG_UpdateFlamethrowerSounds (0x30029210), which indexes it by
 * flameChunk_t.field_34 and, in its final pass, walks it linearly with a +12 stride
 * for 1024 elements. Supersedes the three mechanical dword aliases the exporter
 * carved out of slot[0] (0x300a8718/871c/8720); the owner=g_radiusdamage/
 * cg_updateflamethrowersounds labels were first-touching functions, not identities.
 * The element type must be COMPLETE here because globals.h is parsed before
 * client_recovered.h (which includes it), and an array-of-struct extern needs a
 * complete element type. Defined here; client_recovered.h reuses this same tag. */
typedef struct cgFlameSoundLoop_s {
    float envA; /* +0x00: sound envelope A, accumulated then clamped to <= 1.0,
                         * decayed toward 0 in the final pass. */
    float envB; /* +0x04: sound envelope B, accumulated/clamped to <= 1.0. */
    int32_t frameOwner; /* +0x08: the flameTime stamp of the frame that last updated
                         * this entry (skip re-update when it already == flameTime). */
} cgFlameSoundLoop_t;
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(cgFlameSoundLoop_t) == 12, "cgFlameSoundLoop_t must be 12 bytes");
#endif
extern cgFlameSoundLoop_t cg_flameSoundLoops[1024];
/* 0x300ab718 .data refs=46 — cg_flameTime: the flame subsystem per-frame time base
 * (round(2*cg_time)); written by the flame update path and read via FILD as an int
 * timestamp. RESOLVED from CG_AddFlameChunks (0x300272b0) / CG_UpdateFlamethrowerSounds
 * (0x30029210); the mechanical owner=cg_playadsanim was a first-toucher label. */
extern uint32_t cg_flameTime;
/* 0x300ab71c/720/724 .data — cg_flameSpriteUp: the flame billboard's UP basis vec3.
 * ROLE-RESOLVED (name provisional) from CG_AddFlameSpriteToScene (0x300268e0): when
 * the flame is the local player's first-person flame the function derives view angles
 * from cg_refdef.viewaxis[0] via vectoangles and fills this triple as AngleVectors'
 * `up` output (0x30026dcc..0x30026de5, dst passed in EBX=0x300ab71c); otherwise it is
 * copied from cg_flameSpriteSrcUp (0x300ab744). The sprite quad corners are then built
 * as center +/- radius*cg_flameSpriteRight +/- radius*cg_flameSpriteUp. Three .data
 * dwords forming one vec3; left as three mechanical dword symbols (no reconstructed
 * consumer .c references them yet — this function's body is deferred). */
extern vec3_t cg_flameSpriteUp;
/* 0x300ab728 .data refs=6 width=4 first=0x30025598 owner=hudelem_destroyall.
 * RESOLVED: head of the flame-chunk ACTIVE list (nodes handed out by
 * CG_SpawnFlameChunk, 0x30025600, which pushes each newly popped node here via
 * .next/.prev at +0x0/+0x4). CG_ClearFlameChunks (0x30025598) zeroes it. */
extern flameChunk_t *cg_activeFlameChunks;
/* 0x300ab72c/730/734 .data — cg_flameSpriteRight: the flame billboard's RIGHT basis
 * vec3. ROLE-RESOLVED (name provisional) from CG_AddFlameSpriteToScene (0x300268e0):
 * filled as AngleVectors' `right` output for the first-person flame (dst in
 * EDI=0x300ab72c at 0x30026dd1), or copied from cg_flameSpriteSrcRight (0x300a84f8)
 * otherwise; paired with cg_flameSpriteUp (0x300ab71c) to span the sprite quad. Three
 * .data dwords forming one vec3; left mechanical (deferred body, no .c consumer yet). */
extern vec3_t cg_flameSpriteRight;
/* 0x300ab738/73c/740 .data — cg_flameLastSpritePos: the world position of the most
 * recently emitted flame sprite, a vec3. ROLE-RESOLVED (name provisional) from
 * CG_AddFlameSpriteToScene (0x300268e0), which on every non-early-out exit stores the
 * chunk's world origin (flameChunk_t.field_d8/dc/e0) here (0x30027259..0x30027279).
 * Three .data dwords forming one vec3; left mechanical (deferred body, no .c yet). */
extern vec3_t cg_flameLastSpritePos;
/* 0x300ab744/748/74c .data — cg_flameSpriteSrcUp: the UP billboard basis vec3 used for
 * third-person / world flames. ROLE-RESOLVED (name provisional) from
 * CG_AddFlameSpriteToScene (0x300268e0): on the non-first-person path it copies this
 * triple into cg_flameSpriteUp (0x300ab71c) verbatim (0x30026dfe..0x30026e25); the pair
 * with cg_flameSpriteSrcRight (0x300a84f8) spans the world-flame sprite quad. WRITER
 * PROVEN: CG_FireFlameChunks (0x30027dc2..0x30027dea) copies cg_refdef.viewaxis[2]
 * (viewaxis[2]) into these three dwords once per frame. Retyped from three mechanical
 * uint32_t placeholders to the one vec3 they form. */
extern vec3_t cg_flameSpriteSrcUp;
/* 0x300ab750 .data refs=9 width=4 first=0x300240a3 owner=cg_addflamechunks.
 * RESOLVED base of the contiguous per-owner flame-state region cleared by
 * CG_ClearFlameChunks (0x30025589: REP STOSD of 0xb800 (47104) dwords ==
 * 0x2e000 bytes, spanning 0x300ab750..0x300d9750). CG_AddFlameChunks (0x300240a3)
 * indexes it as base + i*0xb8 (IMUL EBP,EBP,0xb8; ADD EBP,0x300ab750), i.e.
 * FLAME_INFO_COUNT (1024) elements of 0xb8 (184) bytes. Its typed declaration is
 * in client_recovered.h after the complete cgFlameInfo_t definition. */
/* 0x300d9750 .data refs=7 width=4 first=0x3002559e owner=hudelem_destroyall.
 * RESOLVED: head of the secondary flame-chunk list (threaded through
 * flameChunk_t.listNext (+0xc) / .listPrev (+0x10)). CG_ClearFlameChunks
 * (0x3002559e) zeroes it; CG_SpawnFlameChunk/CG_FreeFlameChunk maintain it and
 * its per-node +0x24 marker. Exact source name of this second list unresolved;
 * named by proven role. Sits directly after the cg_flameInfo region
 * (0x300ab750 + 0x2e000 == 0x300d9750). */
extern flameChunk_t *cg_flameChunkList;
/* 0x300d9754 .data — cg_flameSoundsPrevTime: the cg.time (ms) at which
 * CG_UpdateFlamethrowerSounds (0x30029210) last ran. Read (0x30029232) and clamped so the
 * per-frame delta (cg.time - clamp(prev, cg.time - min)) can't exceed cg.time; then
 * rewritten to the current cg.time (0x3002925f). Its only two references are that
 * read/write pair. Retyped uint32_t -> int32_t (signed time). Provisional role name. */
extern int32_t cg_flameSoundsPrevTime;
/* 0x300d9758 .data — cg_flameDamageTakenThisFrame: once-per-frame flag, nonzero when the
 * local player has been found inside a flame damage volume this frame. The per-frame flame
 * updater clears it to 0 at the top (0x300290ef) and, after all chunks, forwards it to the
 * server via cgame_syscall(0x58, cg_flameDamageTakenThisFrame) (0x30029192). Each per-chunk
 * proximity/trace hit in CG_FlameDamage (0x300265c0) sets it to 1 (0x300268c6); the
 * function early-outs when it is already 1 (0x300265c8) so at most one server flame-damage
 * command is issued per frame. RESOLVED from those consumers; mechanical
 * owner=item_listbox_overlb was the first toucher, not the identity. refs=4. */
extern int32_t cg_flameDamageTakenThisFrame;
/* 0x300d975c .data refs=5 width=4 first=0x300255e6 owner=hudelem_destroyall.
 * RESOLVED: count of active flame chunks. CG_ClearFlameChunks (0x300255e6)
 * zeroes it; CG_SpawnFlameChunk increments it, CG_FreeFlameChunk decrements it. */
extern int32_t cg_numActiveFlameChunks;
/* 0x300d9760 .data — cg_flameDamageBestPos: world position of the flame source that damaged
 * the local player this flame-frame, kept so nearby flame chunks within the damage radius are
 * de-duplicated (only the first/closest triggers the once-per-frame server command). Written
 * as three contiguous floats from the incoming flame position (0x30026694/9c/a5) and read back
 * by VectorDistance(&cg_flameDamageBestPos, pos) (0x30026677) when cg_flameDamageBestPosTime ==
 * cg_flameTime. RESOLVED from those consumers; retyped from three mechanical dword aliases
 * (0x300d9760/64/68) into one vec3. mechanical owner=item_listbox_overlb was the first toucher. */
extern vec3_t cg_flameDamageBestPos;
/* 0x300d9770 .data — cg_rOverbrightBitsCvar: the module-side vmCvar mirror handle for
 * the "r_overbrightbits" cvar. RESOLVED from CG_FireFlameChunks (0x30027d75): its
 * address is pushed as the vmCvar arg to trap(7, &vmCvar, "r_overbrightbits", "1",
 * 0x21) on first frame and trap(8, &vmCvar) on later frames. Compact 12-byte
 * cvar-handle slot (next .data symbol is 0x300d977c), like cg_rFullscreenCvar; kept as
 * the exported dword head, name resolved by role. */
extern vmCvar_t cg_rOverbrightBitsCvar;
/* 0x300d977c .data refs=1 width=4 first=0x30026c61 owner=cg_addflamespritetoscene */
/* 0x300d9880 .data refs=3 width=4 first=0x30026d9b owner=cg_addflamespritetoscene */
extern uint32_t cg_flameDamageBillboardCount;
/* cg_translatedString (0x300d9888, .data) — the static result buffer that
 * CG_SafeTranslateString_Internal (0x3002d6e0) returns. On a lookup miss the function fills it
 * with "^1UNLOCALIZED(^7" + reference + "^1)^7" (or, when reporting is disabled,
 * a plain copy of the reference). The mechanical export split this 1024-byte
 * buffer (0x300d9c88 is the next .data symbol) into five dword/byte symbols
 * (…888/…88c/…890/…894/…898, all owner=script_method_scriptbuiltin_sethin, first
 * touched during the inlined 17-byte prefix strcpy at 0x3002d741..0x3002d764):
 * those were stores into offsets 0/4/8/0xc/0x10 of the one buffer, not separate
 * globals. Superseded here as the single MAX_STRING_CHARS buffer it actually is. */
extern char cg_translatedString[MAX_STRING_CHARS];
/* 0x300d9c88..0x300da487: two alternating 1024-byte result buffers used by
 * CG_TranslateMessage. Its toggle<<10 indexing proves the two mechanical immediate-address
 * records at 0x300d9c88 and 0x300da088 are one char[2][MAX_STRING_CHARS] object. */
extern char cg_translateMessageBuffers[2][MAX_STRING_CHARS];
/* 0x300da488 .data refs=60: a shared 1024-byte text scratch buffer, not a single
 * byte. CG_ConsoleCommand (0x300178c0) passes it to trap_Argv with length 0x400
 * (=1024) and then to Q_stricmpn as a C string; many other DLL sites reuse it as
 * a formatting/argv workspace. The mechanical export captured only its first byte
 * (width=1); superseded here with the proven array shape. owner=script_func_vectordot
 * is the wrong mechanical first-toucher label. */
extern char g_textScratchBuffer[MAX_STRING_CHARS];
/* cg_menuListText (0x300da888 .data) — the 4096-byte text buffer that CG_LoadMenus
 * (0x3002d2d0) reads a menu-list ("loadmenu { ... }") file into and then parses in
 * place. CG_LoadMenus opens the file, checks its byte length against MAX_MENULIST_FILE
 * (0x1000 = 4096; errors "menu file too large" when length >= that), reads the bytes
 * here via CG_FS_READ, NUL-terminates at [length], and Com_Compress()es the buffer
 * before walking it with Com_Parse. Extent (4096) proven both by that size guard and
 * by the next distinct global beginning exactly 0x1000 bytes later at 0x300db888.
 * The mechanical export captured only the first dword (width=1/4) and mislabeled the
 * owner `bg_setupweaponalts` (a wrong size-guess for CG_LoadMenus); superseded here
 * in place by the real char[4096] buffer. Storage in globals.c. */
#define MAX_MENULIST_FILE 4096
extern char cg_menuListText[MAX_MENULIST_FILE]; /* 0x300da888 */
/* cg_translatedLocationString (0x300db888, .data) — the static result buffer that
 * CG_GetTranslatedLocationString (0x300310b0) builds an untranslated location name
 * into: "^1UNLOCALIZED(^7" + raw + "^1)^7", or a plain strcpy of the raw location
 * string. It occupies [0x300db888, 0x300dbc88) in .data (0x400 = 1024 bytes; the
 * next distinct global begins at 0x300dbc88). The mechanical export split the
 * 17-byte prefix strcpy into five per-dword/byte symbols (g_data_menus_removefrom-
 * stack_300db888/88c/890/894/898); those were aliases of one buffer and are
 * superseded here by the proven array shape. owner=menus_removefromstack was a
 * pure size-match first-toucher label and is dropped. Provisional name by role
 * (location-string translation result). */
extern char cg_translatedLocationString[MAX_STRING_CHARS];
/* Exact cgame collision globals, named from the same-module Mac symbols and the
 * standard CG_BuildSolidList/CG_TouchTriggerPrediction behavior. The trigger list
 * at 0x300dbc88 contains ET_ITEM and brush-model trigger entities; the solid list
 * at 0x300dc090 contains entities participating in movement/contents collision. */
extern centity_t *cg_triggerEntities[256];
extern int32_t cg_numTriggerEntities;
extern int32_t cg_numSolidEntities;
extern centity_t *cg_solidEntities[256];
/* 0x300dc490..0x300dc5ab: one complete client-prediction pmove context. */
extern pmove_t cg_pmove;
/* 0x300dc5b0 .data — 1024-byte static result buffer for CG_GetTranslatedVoiceChatString
 * (0x3003a150). Its unbounded inline strcpy/strcat paths require the full extent to
 * the next global at 0x300dc9b0; the former 56-byte bound was a misread. The
 * function fills it with "^1UNLOCALIZED(^7" + input + "^1)^7" (or a plain copy of
 * input). The mechanical export split this one buffer into five dword/byte
 * symbols (…b0/…b4/…b8/…bc/…c0, all owner=g_activate, first touched 0x3003a1ad):
 * those were stores into offsets 0/4/8/0xc/0x10 during the inlined strcpy of the
 * constant prefix, not separate globals. Superseded here as the single buffer it
 * actually is. */
extern char cg_translatedVoiceChatString[MAX_STRING_CHARS];
/* 0x300dc9b0 .data — cg_vehicleViewSwayPrevTime: the cg.time (ms) latched by
 * CG_CalcVehicleViewPos (0x30040810) at the moment it recorded the smoothed
 * vehicle-view aim origin below. Written once (0x3004109d, MOV [..],cg_time) and
 * read back as the previous-frame time. Sole owner is CG_CalcVehicleViewPos (the
 * mechanical owner=pm_slidemove is this function's rejected size-guess name). */
extern int32_t cg_vehicleViewSwayPrevTime;
/* 0x300dc9b4/b8/bc .data — cg_vehicleViewSwayOrigin: the smoothed vehicle-view aim
 * origin vec3 stored by CG_CalcVehicleViewPos so the next frame can lerp the camera
 * toward it. The seat-view arm (0x30041097..0x300411af) writes the three floats and
 * later reloads them (0x300411a4..) into cg_refdef.vieworg. Three consecutive floats;
 * consolidated from the mechanical per-dword pm_slidemove symbols. */
extern vec3_t cg_vehicleViewSwayOrigin;
/* Mounted-turret BG weapon-sway output buffers. */
extern vec3_t cg_turretViewSwayPreviousViewAngles;
extern vec2_t cg_turretViewSwayViewAngles;
/* 0x300dc9d8 .data refs=4 width=4.
 * RESOLVED on consume by CG_CalcFov (0x3003ffc0): cg_fovLastVehiclePosition — the
 * last predicted vehiclePosition (cg_predictedPlayerState.vehiclePosition, values 1/2/3)
 * for which the vehicle/turret FOV path ran. CG_CalcFov compares the current
 * position against it to detect a change (0x300400f6), stores the new position
 * (0x30040120), and resets it to 0 when the vehicle-view flag is clear (0x3004023a).
 * A plain signed int. The mechanical owner=bg_parseweaponinfospecificfieldtyp label
 * was a wrong size-based first-touch (that function does no weapon-info parsing). */
extern int32_t cg_fovLastVehiclePosition;
/* 0x300dc9dc/e0/e4 .data — cg_vehicleViewSwayOffset: the
 * BG_CalculateWeaponPosition_Sway positional output (EAX arg at 0x30040977).
 * CG_CalcVehicleViewPos reads it back as float scalars in the
 * thompson-sway blend arm (FMUL [dc9dc]/[dc9e0], FLD [dc9e4]+FCHS at 0x30040b2a..0x30040be7).
 * Three consecutive floats; consolidated from the mechanical per-dword pm_slidemove symbols. */
extern vec3_t cg_vehicleViewSwayOffset;
/* 0x300dc9e8/ec/f0 .data — previous view angles passed on the stack to
 * BG_CalculateWeaponPosition_Sway at 0x3004096d and updated by the sway core. */
extern vec3_t cg_vehicleViewSwayPreviousViewAngles;
/* 0x300dc9f4 .data refs=5 width=4.
 * RESOLVED on consume by CG_CalcFov (0x3003ffc0): cg_fovTransitionTime — the
 * cg.time (ms) at which the most recent FOV/zoom transition was triggered, or -1
 * when none is pending. Set to cg.time by the vehicle-view path (0x300400ea,
 * 0x30040113, 0x30040234) and to -1 (0x30040283) after the non-vehicle transition
 * window elapses; the tail tests it against cg.time-50 (0x3004024d) to drive the
 * cg_fovFade animator. A signed int (holds -1). The mechanical
 * owner=bg_parseweaponinfospecificfieldtyp label was a wrong size-based first-touch. */
extern int32_t cg_fovTransitionTime;
/* 0x300dc9f8/fc .data — cg_vehicleViewSwayViewAngles: the BG_CalculateWeaponPosition_Sway
 * `out_angles` vec2 output buffer (address passed at 0x30040972). Written by the sway
 * core; sole owner CG_CalcVehicleViewPos. Only its address is taken here. */
extern vec2_t cg_vehicleViewSwayViewAngles;
extern vec3_t cg_turretViewSwayOffset;
/* 0x300dca10..0x300dca18 .data refs=3 width=4.
 * RESOLVED on consume by CG_CalcFov (0x3003ffc0): cg_fovAdsUpdateTime[2] -- the last
 * cg.time (ms) at which the vehicle/turret ADS FOV was committed, one slot per
 * ADS state (index 0 = not aiming, index 1 = aiming; the index is
 * cg.predictedPlayerState.adsFraction != 0.0f). Indexed [edi*4] and compared with
 * cg.time (0x300400ae), stored cg.time on update (0x300400e3, 0x30040119). Two
 * signed ints spanning 0x300dca10..0x300dca18. The mechanical
 * owner=bg_parseweaponinfospecificfieldtyp label was a wrong size-based first-touch. */
extern int32_t cg_fovAdsUpdateTime[2];
/* CG_FakeTrajectoryEffects one-per-frame/entity duplicate suppression cache. */
extern int32_t cg_fakeTrajectoryEntity;
/* Persistent interpolation state for the weapon-selection carousel. */
extern float cg_weaponSelectTransition;
extern int32_t cg_fakeTrajectoryTime;
/* MAX_VA_STRING — capacity (bytes) of each va() formatting buffer. Proven from
 * va (0x3004e8a0): the vsnprintf size argument (0x7d00) and the ring wrap
 * threshold (0x7cff = 0x7d00 - 1). The canonical shared value is declared in
 * q_shared_types.h. */

/* cg_scriptExports (0x300f08e0 .data) — five script callbacks returned to the
 * engine by Scr_FarHook (0x3004fd00, vmMain command 18). The same-module Mac
 * symbols and the matching server/engine interface prove the slot order:
 * Scr_GetFunction, Scr_GetMethod, Scr_SetObjectField, Scr_GetObjectField, and
 * Scr_LoadRead. The cgame implementations are intentionally empty/default.
 * Storage at 0x300f08f4 is not a sixth slot; the five-pointer table ends there,
 * four bytes before the independently aligned import table at 0x300f08f8. */
extern cg_scriptExportTable_t cg_scriptExports;
/*
 * cg_scriptImports (0x300f08f8 .data) — base of the cgame's 102-entry
 * (0x66 dword, 408-byte) engine-import function-pointer table. It runs from
 * 0x300f08f8 up to 0x300f0a90 (exactly 0x198 bytes; the next symbol, the UI
 * key-binding result buffer, begins there). Scr_FarHook (0x3004fd00)
 * REP-MOVSD copies all 102 pointers here from its argument when it is non-NULL,
 * i.e. this table is written by cgame at vmMain command 18 rather than patched
 * by the engine at load. Typed fields replace the former indexed cast macros.
 * Cross-binary script-VM evidence also corrects slots 80/81/98 to
 * Scr_FindAnimTree, Scr_FindAnim, and Scr_GetAnimsIndex respectively.
 */
extern cg_scriptImportTable_t cg_scriptImports;
extern char pc_sourceWarningMessage[4096];
/* pc_sourceErrorMessage (0x30133c48 .data) — the static scratch buffer that
 * PC_SourceError (0x30050090) formats its parse-error text into via vsprintf,
 * then reads back as the "%s" message argument to Com_Printf. The mechanical export
 * mislabeled this as a uint32_t immediate owned by `itemparse_elementtype` (a wrong
 * size-matched name); it is really a 4096-byte char array. Size proven by the
 * address gap to the next .data symbol (0x30134c48 - 0x30133c48 == 0x1000) and by
 * the two-instruction write/read pattern (refs=2, both in PC_SourceError). Matches
 * the Q3 ui_shared.c PC_SourceError `char string[4096]` message buffer. */
extern char pc_sourceErrorMessage[4096];
/* 0x30134ccc .data: bgAnimScriptLoaded — one-time guard for the animation-script
 * file load. BG_AnimParseAnimScript (0x30002470) reads it at entry (0x30002489)
 * and, when 0, reads "mp/playeranim.script" into bgAnimScriptFileBuffer and sets it to 1
 * (0x3000251c), so the file is loaded only once across all anim-tree parses.
 * Mechanical owner cg_drawcrosshair is the wrong first-toucher; superseded here.
 * Zero-initialized in .data. */
extern int32_t bgAnimScriptLoaded;
/* 0x30134cd4 .data first=0x30001524: number of registered weapon-info entries.
 * Resolved from the weaponInfo_t registration/parse path (0x3000fee0, 0x30010df0)
 * whose alloc-failure string is "Could not allocate WeaponInfo array"; the count
 * is a signed int (checked <0 / iterated <= max). Was mechanically labeled
 * cmd_veh_freevehicle after its first-touching function. */
extern int32_t bg_numWeapons;
/* 0x30134cd8 .data first=0x30001534: base of the weaponInfo_t pointer array,
 * allocated via engine syscall 0xcb in FUN_30010df0. Each element is a
 * weaponInfo_t* whose pickupName field is at +0x4 (see BG_GetWeaponIndexForName). */
extern weaponInfo_t **bg_weaponInfos;
/*
 * cgWeaponInfo_t — the cgame-side per-weapon registration/visual record and its
 * array cg_weaponInfos (0x30413580). Distinct from the bg_weaponInfos pointer
 * array above: this is a value array of fixed 0x1c4-byte records, indexed
 * 1..bg_numWeapons in parallel with bg_weaponInfos. The record caches the
 * weapon's animation tree/rates, models, registered effects and sounds, item and
 * HUD materials, localized names, and config-string name. The complete 0x1c4
 * stride and the registration-owned fields are proven by CG_RegisterWeapon
 * (0x300435d0); neighboring consumers resolve the remaining runtime fields. */
typedef struct cgWeaponInfo_s {
    /* +0x00 : the local (view) weapon's DObj/self context handle, used to resolve
     * bone/tag world matrices on the first-person weapon. The flash-sound emitter
     * CG_WeaponUpdateLoopingSound (0x300490b0) reads it as a dword (MOV EDI,[ESI]) and,
     * for the local predicted player, passes it as `self` to
     * CG_DObjGetSpecialTagWorldMatrix("tag_flash"); a zero handle skips that path.
     * Exact CoD member name unproven; named by proven role. The original field is
     * 4 bytes on i386 and widens with the native DObj pointer on 64-bit hosts. */
    DObj *viewDObjSelf; /* +0x00 : local view-weapon DObj self handle. Also read
                                     *         by CG_ResetWeaponAnimTrees (0x30042fc0, MOV
                                     *         EAX,[EDI]) as the weapon's overlay DObj handle
                                     *         resolved through CG_DOBJ_GET_TREE; a zero
                                     *         handle skips the weapon. Same physical dword. */
    float animRates[WEAPON_XANIM_COUNT]; /* +0x04..+0x63: registered XAnim playback rates */
    char name[64]; /* +0x64..+0xa3 : cached config-string weapon name
                                     *         (NUL-terminated). Proven 0x40-byte extent:
                                     *         CG_RefreshWeaponDObjModelSet (0x30044890)
                                     *         Q_strncpyz's it with destsize 0x3f then forces
                                     *         name[0x3f]='\0'; it ends exactly at +0xa4 where
                                     *         lastRunAnim begins. */
    int32_t lastRunAnim; /* +0xa4 : the weapon-anim number last processed by
                                     *         CG_WeaponRunXModelAnims (0x30042d30), latched
                                     *         from ps->weaponAnim each frame the anim changes.
                                     *         The function early-outs when ps->weaponAnim still
                                     *         equals this, and stores -1 here when a run is
                                     *         skipped because not all XModel anims are registered
                                     *         yet. Provisional role name; exact source unproven. */
    int32_t registered; /* +0xa8: registration-complete/in-progress gate */
    const gitem_t *item; /* +0xac on i386: registered item definition */
    /* +0xb0/+0xb4 : the weapon's HUD display-name string pointers. Proven by the
     * selected-weapon-name HUD draw (0x3002ecc9/0x3002ece6): +0xb0 is always
     * formatted as the primary display name ("%s"), and when the bg_weaponInfos
     * record's secondary-name pointer (+0x78) is non-empty the draw uses
     * va("%s / %s", displayName, modeName). Exact CoD member names unproven;
     * named by proven role. */
    const char *displayName; /* +0xb0 on i386: primary HUD display name */
    const char *modeName; /* +0xb4 on i386: localized alternate-mode name */
    const char *aiOverlayDescription; /* +0xb8 on i386: localized AI-overlay description */
    qhandle_t worldModelHandle; /* +0xbc : the weapon's world/pickup model DObj handle.
                                     *         CG_BuildCorpseDObjModels (0x300058f0) reads it
                                     *         as a dword (nonzero gate, wrapped via
                                     *         CG_DOBJ_WRAP_MODEL) and stores its low 16 bits as
                                     *         the corpse weapon element's model handle. Exact
                                     *         source member name unproven; named by proven role.
                                     *         qhandle_t is the 32-bit registration-handle
                                     *         domain returned and consumed at these sites. */
    qhandle_t pickupModelHandle; /* +0xc0 */
    qhandle_t viewModelHandle; /* +0xc4 */
    uint32_t viewFlashEffect; /* +0xc8: registered first-person muzzle effect */
    uint32_t worldFlashEffect; /* +0xcc: registered third-person muzzle effect */
    uint8_t registeredAssetsD0[12]; /* ABI_AUDITED_OPAQUE: registered weapon assets
                                       * not consumed by maintained cgame code. */
    const char *projectileSound; /* +0xdc on i386 */
    /* +0xe0..+0x114, +0x174/+0x178 : per-weapon canonical alias-name pointers, played by
     * CG_EntityEvent (0x30022810) for this weapon's mechanical events. Each is a
     * alias-name pointer handed to CG_PlayEntitySoundAliasByName; NULL means "no
     * sound for this event". Field names encode the weapon-event id that plays each
     * (proven from the dispatcher), not an invented semantic name — exact CoD member
     * names are unresolved. */
    const char *pullbackSound; /* +0xe0 on i386 */
    const char *fireSound; /* +0xe4 on i386 */
    const char *fireEchoSound; /* +0xe8 on i386 */
    const char *lastShotSound; /* +0xec on i386 */
    const char *rechamberSound; /* +0xf0 on i386 */
    const char *reloadSound; /* +0xf4 on i386 */
    const char *reloadEmptySound; /* +0xf8 on i386 */
    const char *reloadStartSound; /* +0xfc on i386 */
    const char *reloadEndSound; /* +0x100 on i386 */
    const char *raiseSound; /* +0x104 on i386 */
    const char *altSwitchSound; /* +0x108 on i386 */
    const char *putawaySound; /* +0x10c on i386 */
    const char *deploySound; /* +0x110 on i386 */
    const char *breakdownSound; /* +0x114 on i386 */
    /* +0x118..+0x124 : the four "noteTrackSound{A,B,C,D}" alias-name pointers played by
     * CG_ProcessWeaponNoteTracks (0x30042c40). That function scans the currently
     * bound animation's notetrack list (trap_XAnimGetNotetracks) and, when a track's
     * name Q_stricmp-matches one of the literals "noteTrackSoundA/B/C/D"
     * (0x300761e4/0x300761d4/0x300761c4/0x300761b4), reads the corresponding handle
     * here and, if nonzero, starts it via CG_PlaySoundAliasByName. Each is the
     * canonical alias-name pointer returned by trap_Com_SoundAliasString. */
    const char *noteTrackSoundA; /* +0x118 on i386 */
    const char *noteTrackSoundB; /* +0x11c on i386 */
    const char *noteTrackSoundC; /* +0x120 on i386 */
    const char *noteTrackSoundD; /* +0x124 on i386 */
    /* +0x128/+0x12c : the muzzle-flash "fire loop" and "fire tail" positional sound
     * alias-name pointers. Both are resolved sounds (CG_RegisterWeaponInfo at 0x300444e3/
     * 0x30044501 registers them via trap 0xc3 from the parse-struct name fields
     * +0xb0/+0xb4). The flash-sound emitter CG_WeaponUpdateLoopingSound (0x300490b0)
     * plays loopFireSound (+0x128) at the tag_flash world position while the
     * effect's remaining lifetime is > 0 and switches to stopFireSound (+0x12c)
     * on the frame the lifetime reaches <= 0, both via CG_PlaySoundAliasByName with the
     * tag-flash origin as the channel object. NULL means "no sound". */
    const char *loopFireSound; /* +0x128 on i386 */
    const char *stopFireSound; /* +0x12c on i386 */
    /* +0x130/+0x134 : two named effect/model resources for this weapon's muzzle
     * effect. CG_Missile (0x3001edb0) registers each via CG_COM_PICK_SOUND_ALIAS
     * (trap 0xc4: name, channelObj=&cent lerpOrigin) into an engine handle and
     * requires BOTH handles nonzero before running the view-facing alpha/beam
     * emission. A char pointer or qhandle_t on the target; kept 4-byte-wide so the
     * record stride holds at both ABIs. Exact CoD member names unproven; named by role. */
    const char *projectileSoundBlend1; /* +0x130 on i386 */
    const char *projectileSoundBlend2; /* +0x134 on i386 */
    qhandle_t itemHudIconShader; /* +0x138 */
    qhandle_t itemSelectIconShader; /* +0x13c */
    qhandle_t itemAmmoIconShader; /* +0x140 */
    qhandle_t hudIconShader; /* +0x144 */
    qhandle_t modeIconShader; /* +0x148: 2D HUD icon material/shader handle for
                                     *         this weapon. Proven by the selected-weapon
                                     *         icon draw CG_DrawPlayerWeaponModeIcon (0x3002ef78):
                                     *         MOV EAX,[record+0x148]; if zero the draw is
                                     *         skipped, otherwise it is passed as the hShader
                                     *         to CG_DrawPic. Exact CoD member name unproven;
                                     *         named by proven role. */
    qhandle_t ammoIconShader; /* +0x14c */
    int32_t clipModelHandle; /* +0x150: registered clip/magazine model */
    uint8_t clipModelState[4]; /* ABI_AUDITED_OPAQUE: auxiliary clip-model state. */
    /* +0x158..+0x164 : projectile dynamic-light parameters. CG_Missile
     * (0x3001edb0) emits the light only when projectileDLight (+0x158) != 0.0,
     * calling trap_R_AddLightToScene(&cent lerpOrigin, projectileDLight,
     * flashLightR, flashLightG, flashLightB). Four consecutive floats; exact CoD
     * member names unproven, named by proven role. */
    float projectileDLight; /* +0x158: projectile dynamic-light intensity */
    float flashLightR; /* +0x15c: dynamic-light red                      */
    float flashLightG; /* +0x160: dynamic-light green                    */
    float flashLightB; /* +0x164: dynamic-light blue                     */
    uint32_t flashRenderFx; /* +0x168: base RF_* render-flags for the muzzle-
                                     *         effect refEntity. CG_Missile OR-s in
                                     *         RF_NOSHADOW (0x40) before storing it into
                                     *         refEntity.renderfx. Exact CoD member name
                                     *         unproven; named by proven role. */
    /* +0x16c/+0x170 : the two shell-casing ("brass") eject effect handles played by
     * CG_EjectWeaponBrass (0x30047be0) on the firing entity's "tag_brass" bone via
     * CG_PLAY_EFFECT_ON_TAG (trap 0xe9), gated on a nonzero handle. The +0x170
     * variant is selected for weapon-event 0xa6; +0x16c for all other eject events.
     * qhandle_t on the target; kept 4-byte-wide so the 0x1c4 record stride holds at
     * both ABIs. Exact CoD member names unproven; named by proven role. */
    uint32_t shellEjectEffect; /* +0x16c */
    uint32_t lastShotEjectEffect; /* +0x170 */
    uint32_t projectileExplosionEffect; /* +0x174 */
    const char *projectileExplosionSound; /* +0x178 on i386 */
    uint32_t projectileTrailEffect; /* +0x17c */
    uint8_t projectileTrailAssets[8]; /* ABI_AUDITED_OPAQUE: two unconsumed projectile assets. */
    qhandle_t reticleCenterShader; /* +0x188: registered 2D material/shader handle for
                                     *         this weapon's 3D-view-centered HUD icon (distinct
                                     *         from the weapon-select icon hudIconShader at
                                     *         +0x148). Read by the view-centered weapon-icon
                                     *         draw at 0x30019ba0 (CG_DrawWeaponIcon3D) as
                                     *         MOV EAX,[weaponIndex*0x1c4 + 0x30413708] and
                                     *         passed as the hShader to trap_R_DrawStretchPic.
                                     *         (0x30413708 == 0x30413580 + 0x188, i.e. this member
                                     *         of cg_weaponInfos[]; the mechanical global
                                     *         g_data_stuckinclient_30413708 was a first-touch
                                     *         mislabel of this field and is superseded here.)
                                     *         Exact CoD member name unproven; named by proven
                                     *         role. */
    qhandle_t reticleSideShader; /* +0x18c */
    qhandle_t adsOverlayShader; /* +0x190 */
    uint8_t overlayAssets[48]; /* ABI_AUDITED_OPAQUE: trailing weapon overlay resources. */
} cgWeaponInfo_t;
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(cgWeaponInfo_t, animRates) == 0x04, "cgWeaponInfo animRates +0x04");
_Static_assert(offsetof(cgWeaponInfo_t, name) == 0x64, "cgWeaponInfo name offset");
_Static_assert(offsetof(cgWeaponInfo_t, lastRunAnim) == 0xa4, "cgWeaponInfo lastRunAnim +0xa4");
_Static_assert(offsetof(cgWeaponInfo_t, registered) == 0xa8, "cgWeaponInfo registered +0xa8");
_Static_assert(offsetof(cgWeaponInfo_t, item) == 0xac, "cgWeaponInfo item +0xac");
_Static_assert(offsetof(cgWeaponInfo_t, worldModelHandle) == 0xbc, "cgWeaponInfo worldModelHandle offset");
_Static_assert(offsetof(cgWeaponInfo_t, viewFlashEffect) == 0xc8, "cgWeaponInfo viewFlashEffect offset");
_Static_assert(offsetof(cgWeaponInfo_t, worldFlashEffect) == 0xcc, "cgWeaponInfo worldFlashEffect offset");
_Static_assert(offsetof(cgWeaponInfo_t, pullbackSound) == 0xe0, "cgWeaponInfo pullbackSound offset");
_Static_assert(offsetof(cgWeaponInfo_t, breakdownSound) == 0x114, "cgWeaponInfo breakdownSound offset");
_Static_assert(offsetof(cgWeaponInfo_t, noteTrackSoundA) == 0x118, "cgWeaponInfo noteTrackSoundA offset");
_Static_assert(offsetof(cgWeaponInfo_t, noteTrackSoundB) == 0x11c, "cgWeaponInfo noteTrackSoundB offset");
_Static_assert(offsetof(cgWeaponInfo_t, noteTrackSoundC) == 0x120, "cgWeaponInfo noteTrackSoundC offset");
_Static_assert(offsetof(cgWeaponInfo_t, noteTrackSoundD) == 0x124, "cgWeaponInfo noteTrackSoundD offset");
_Static_assert(offsetof(cgWeaponInfo_t, loopFireSound) == 0x128, "cgWeaponInfo loopFireSound offset");
_Static_assert(offsetof(cgWeaponInfo_t, stopFireSound) == 0x12c, "cgWeaponInfo stopFireSound offset");
_Static_assert(offsetof(cgWeaponInfo_t, hudIconShader) == 0x144, "cgWeaponInfo hudIconShader offset");
_Static_assert(offsetof(cgWeaponInfo_t, reticleCenterShader) == 0x188, "cgWeaponInfo reticleCenterShader offset");
_Static_assert(offsetof(cgWeaponInfo_t, reticleSideShader) == 0x18c, "cgWeaponInfo reticleSideShader offset");
_Static_assert(offsetof(cgWeaponInfo_t, adsOverlayShader) == 0x190, "cgWeaponInfo adsOverlayShader offset");
_Static_assert(offsetof(cgWeaponInfo_t, projectileExplosionEffect) == 0x174, "cgWeaponInfo projectileExplosionEffect offset");
_Static_assert(offsetof(cgWeaponInfo_t, projectileExplosionSound) == 0x178, "cgWeaponInfo projectileExplosionSound offset");
_Static_assert(offsetof(cgWeaponInfo_t, projectileSound) == 0xdc, "cgWeaponInfo projectileSound offset");
_Static_assert(offsetof(cgWeaponInfo_t, projectileSoundBlend1) == 0x130, "cgWeaponInfo projectileSoundBlend1 offset");
_Static_assert(offsetof(cgWeaponInfo_t, projectileSoundBlend2) == 0x134, "cgWeaponInfo projectileSoundBlend2 offset");
_Static_assert(offsetof(cgWeaponInfo_t, clipModelHandle) == 0x150, "cgWeaponInfo clipModelHandle offset");
_Static_assert(offsetof(cgWeaponInfo_t, projectileDLight) == 0x158, "cgWeaponInfo projectileDLight offset");
_Static_assert(offsetof(cgWeaponInfo_t, flashLightB) == 0x164, "cgWeaponInfo flashLightB offset");
_Static_assert(offsetof(cgWeaponInfo_t, flashRenderFx) == 0x168, "cgWeaponInfo flashRenderFx offset");
_Static_assert(offsetof(cgWeaponInfo_t, projectileTrailEffect) == 0x17c, "cgWeaponInfo projectileTrailEffect offset");
_Static_assert(sizeof(cgWeaponInfo_t) == 0x1c4, "cgWeaponInfo stride");
#endif
/* 0x30134ce0 .data — cg_hudSpinPrevTime: the cg_time (ms) at which the spinning
 * HUD element's angle was last advanced. Exclusively owned by CG_UpdateHudSpinAngle
 * (0x3001d3a0): read as prevTime, used to derive the elapsed-time delta (cg_time -
 * prevTime), and rewritten to cg_time each advance (and in the reset path when
 * prevTime > cg_time). All 3 refs are inside that one function; the mechanical
 * owner=pm_beginweaponchange label is a size-guess for the writer and is rejected.
 * Signed ms time (compared with signed JG / signed SUB). Exact CoD field name
 * unproven; role-derived. */
extern int32_t cg_hudSpinPrevTime;
/* cg_compassSpinPrevTime (0x30134ce4, .data) — last cg.time (ms) at which
 * CG_UpdateCompassOrientation (0x3001d6d0) advanced the compass reference-yaw spring. The
 * updater integrates over dt = cg.time - this, then stores cg.time back here. It is the
 * compass twin of cg_hudSpinPrevTime (0x30134ce0, adjacent). refs=3, owned solely by
 * 0x3001d6d0; the owner=veh_checkpushclients label is a wrong size-match name. int ms. */
extern uint32_t cg_compassSpinPrevTime;
/* 0x30134ce8 .data refs=4 width=4 first=0x30025571 owner=hudelem_destroyall.
 * RESOLVED to the flame-chunk pool base (see globals.c and CG_ClearFlameChunks
 * at 0x30025570): the allocator CG_InitFlameChunks (0x300279d0) requests a
 * 0x2a0000-byte (2752512-byte) block via cgame trap 0xc0, stores it here, then
 * calls CG_ClearFlameChunks which treats the block as an array of 0x2000 (8192)
 * flame-chunk nodes of 0x150 (336) bytes each (8192*336 == 0x2a0000) and builds
 * the free list from them. Freed via trap 0xc1 and nulled during effects
 * shutdown at 0x3002e3c0. Typed as flameChunk_t* at its canonical home. */
extern flameChunk_t *cg_flameChunks;
/* 0x30134cec .data refs=1 width=4 first=0x300255ee owner=hudelem_destroyall.
 * RESOLVED: flame-chunk-system "initialized" flag, set to 1 at the tail of
 * CG_ClearFlameChunks (0x300255ee). */
extern qboolean cg_flameChunksInited;
/* 0x30134cf0 .data refs=1 — cg_lastFlameChunkProcessed: scratch holding the last flame
 * chunk CG_MoveFlameChunk (0x30025da0) finalized this frame; it is the only
 * writer (MOV [0x30134cf0],EBP at 0x300265af, EBP = the chunk pointer). RESOLVED from
 * that sole consumer; the owner=cg_playerturretpositionandblend label was the mis-sized
 * first-touching function name, not the identity. Retyped uint32_t -> flameChunk_t*. */
extern flameChunk_t *cg_lastFlameChunkProcessed;
/* 0x30134cf4 .data — cg_fireFlameChunksCvarsRegistered: once-only latch guarding the
 * lazy cvar registration in CG_FireFlameChunks (0x30027d42). Zero on the first frame
 * (=> trap(7,...) registers r_fullscreen/r_overbrightbits), nonzero afterward (=>
 * trap(8,...) refreshes them). RESOLVED from that sole consumer; the mechanical
 * owner=cg_fireflamechunks label happened to be correct here. NOTE: no writer that
 * sets it to 1 is visible in this function — it is set by the register path elsewhere
 * or is engine-managed; verify when adjacent flame-init code is reconstructed. */
extern int32_t cg_fireFlameChunksCvarsRegistered;
/* 0x30134cf8 .data refs=58 width=4 first=0x3001c13b: cg_updateScreenActive — the
 * synchronous-redraw reentry latch bracketed set-1/DEC by CG_DrawActive
 * (0x3001c120) and the loading-screen updater at 0x3002a530 around their
 * levelshot/progress-bar draw + trap_UpdateScreen (CG_UPDATE_SCREEN). Retyped role name;
 * the mechanical owner=g_getnonpvsfriendlyinfo label was a wrong size-match. */
extern int32_t cg_updateScreenActive;
/* 0x30134cfc .data — cg_numLocalEntities: count of localEntity_t currently on the
 * cg_activeLocalEntities list. CG_InitLocalEntities (0x3002a9e0) zeroes it,
 * CG_AllocLocalEntity (0x3002aa70) increments it, CG_FreeLocalEntity decrements it.
 * (Provisional name by role; exporter owner label "anglenormalize180accurate" is wrong.) */
extern int32_t cg_numLocalEntities;
/* 0x30134d00: CG_TranslateMessage's 0/1 ping-pong buffer index; XOR-toggled before
 * indexing cg_translateMessageBuffers. */
extern int32_t cg_translateMessageBufferIndex;
/* 0x30134d04 .data — cg_statBarDisplayFrac: the smoothed/trailing fill fraction of
 * the HUD stat bar. Tracks the live fraction on a rise and decays toward it after a
 * hold delay on a fall, producing the trailing "ghost" segment. Written as a float
 * bit pattern by MOV and read/updated via FLD/FST/FSUBR. Retyped from the mechanical
 * uint32_t to float; (owner=stopfollowing) is a wrong size-guess. Consumed only by
 * CG_DrawStatBarWithDecay (0x3002f9d0). Provisional role name. */
extern float cg_statBarDisplayFrac;
/* 0x30134d08: one-shot skybox fog initialization latch. */
extern qboolean cg_skyboxFogConfigured;
extern int32_t cg_weaponSelectPreviousSlot;
extern int32_t cg_weaponSelectPreviousWeapon;
/* 0x30134d2c .data: UI display-context pointer (displayContextDef_t *DC in
 * ui_shared.c). Initialized once at 0x3002dc85 to the static context object
 * 0x30421f60 and set at 0x300509e0; consumed pervasively (refs=212). String_Init
 * (0x3004ffb0) tail-calls a binding-cache refresh only when DC != NULL and its
 * key-getter member DC->getBindingBuf (+0xb0) is non-NULL. (Mechanical owner
 * label pm_updateaimdownsightlerp was its first-touching function.) */
/* 0x30421f60 .data: the single static displayContextDef_t instance that DC points
 * at. CG_UIDisplayContextInit (0x3002da90) fills its DC->* vtable slots from fixed
 * cgame service addresses and then publishes it as DC (the store of 0x30421f60
 * into 0x30134d2c at 0x3002dc85). Other UI code invokes its slots directly (e.g.
 * `CALL [0x30421fb0]` = slot +0x50). (Mechanical export split this instance into
 * per-address placeholder globals owned by pm_updateaimdownsightlerp /
 * g_parsescrvehicleinfo; this typed object is the real backing store.) */
extern displayContextDef_t g_uiDCInstance;
extern int32_t cg_loadHudState;
/* 0x30134fd4..0x30135fec: MSVC CRT small-block-heap state, used exclusively by
 * statically linked heap initialization/allocation/termination routines. */
extern uint8_t msvc_crt_smallBlockHeapState[4120];
/* 0x30136000: MSVC CRT lowio __pioinfo table (array of ptrs to 32-entry ioinfo
 * blocks, stride 0x24, lock at +0xc). Used by _lock_fhandle/_unlock_fhandle. The
 * "scriptentcmd_notsolid" owner was a first-touch auto-name, not the real identity. */
extern uint32_t crt_pioinfo_table;
/* 0x301698c0 .data refs=3 width=4 first=0x30046228 owner=item_listbox_paint */
extern vec3_t cg_brassEffectOrigin;
/* 0x301698c4 .data refs=1 width=4 first=0x3004622e owner=item_listbox_paint */
/* 0x301698c8 .data refs=1 width=4 first=0x30046234 owner=item_listbox_paint */
extern uint8_t ui_unreferencedAggregateStorage[4372];
/* 0x3016a9e0 .data — the client voice-chat table region: eight fixed 0x49148-byte
 * table blocks. The mechanical owners (playercmd_finishplayerdamage /
 * bg_calculateweaponposition_damagek) were wrong; repaired to the real role.
 *
 * Block[0] @ 0x3016a9e0 is the "mp/axis_chat.voice" table and block[1] @ 0x301b3b28
 * (== 0x3016a9e0 + 0x49148) is the "mp/allies_chat.voice" table, both parsed in by
 * the per-table loader at 0x300396f0 (called from 0x3002b560 with those paths and a
 * 0x40 entry count). CG_PickSoundAlias (0x30039f10) views one block as
 * cg_sound_alias_list_t[64] (stride 0x1244; 64*0x1244 == 0x49100 fits inside 0x49148).
 * The table-index resolver at 0x30039d80 walks all eight blocks by 0x49148 and
 * case-insensitively compares each block's leading bytes against a parsed token.
 *
 * The region ends exactly at 0x303b3420 (== &cg_voiceChatTables[8]); that end
 * address is the loop sentinel at 0x30039ed2/0x3003a3d8. */
enum {
    CG_VOICE_CHAT_TABLE_COUNT = 8
};
enum {
    CG_VOICE_CHAT_TABLE_STRIDE = 0x49148
}; /* original i386 stride */
/* Mark-poly (decal) pool, proven by CG_InitMarkPolys (0x3002e400) and
 * CG_AllocMark (0x3002e490). The owner=quatinverse labels the mechanical exporter
 * put on 0x303b5d20/0x303b5d40/0x303b5eb4/0x30412d40/0x30412d44 were wrong (the
 * first-touching function it named is the pool initializer, not "QuatInverse").
 * CG_InitMarkPolys zeroes 0x17400 dwords (== MAX_MARK_POLYS * 0x174 bytes) at the
 * pool base and threads all MAX_MARK_POLYS nodes onto cg_freeMarkPolys, so the
 * pool holds exactly MAX_MARK_POLYS markPoly_t. 0x303b5eb4 (== &cg_markPolys[1])
 * is only the free-list threading cursor and is not a separate source global; it
 * is superseded by the cg_markPolys array below. The two mechanical dwords at
 * 0x30412d40 and 0x30412d44 are the +0x0/+0x4 link fields of the single
 * cg_activeMarkPolys sentinel node, folded into it here. */
enum {
    MARK_FRAGMENT_MAX_POINTS = 10
}; /* CG_ImpactMark clamps numPoints to 0xa */

/* markPoly_t — one mark-poly pool node (0x174 bytes, proven from CG_InitMarkPolys
 * zeroing MAX_MARK_POLYS * 0x174 bytes at the pool base). Intrusive link fields at
 * +0x0/+0x4; the render payload from +0x08 is filled by CG_ImpactMark (0x3002e520)
 * and read by CG_AddMarks (0x3002e8c0). The reserved gaps are decal state not yet
 * touched by a reconstructed function. */
typedef struct markPoly_s {
    struct markPoly_s *prevMark; /* +0x00: active-list back link (toward oldest) */
    struct markPoly_s *nextMark; /* +0x04: active-list forward / free-list link */
    int32_t markTime; /* +0x08: cg.time when the mark was created;
                                    * CG_AllocMark reclaims the oldest run sharing
                                    * this value when the free list is exhausted */
    qhandle_t markShader; /* +0x0c: surface shader (from the fragment) */
    int32_t alphaFade; /* +0x10: qboolean — CG_AddMarks fades the alpha
                                    * byte of every vert when set */
    float colors[4]; /* +0x14: red/green/blue at +0x14/+0x18/+0x1c,
                                    * alpha at +0x20 (all faded by CG_AddMarks) */
    uint8_t markState24[4]; /* ABI_AUDITED_OPAQUE: decal state not touched here. */
    int32_t numVerts; /* +0x28: vertex count for this mark's poly */
    uint8_t markState2C[4]; /* ABI_AUDITED_OPAQUE: decal state not touched here. */
    polyVert_t verts[MARK_FRAGMENT_MAX_POINTS]; /* +0x30: up to 10 verts,
                                    * 32 bytes each -> +0x30..+0x170 */
    int32_t duration; /* +0x170: mark lifetime in ms; CG_AddMarks
                                    * expires the mark when cg.time > markTime+duration */
} markPoly_t;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(markPoly_t) == 0x174, "markPoly_t must be 372 bytes");
_Static_assert(offsetof(markPoly_t, prevMark) == 0x00, "markPoly_t.prevMark @ +0x00");
_Static_assert(offsetof(markPoly_t, nextMark) == 0x04, "markPoly_t.nextMark @ +0x04");
_Static_assert(offsetof(markPoly_t, markTime) == 0x08, "markPoly_t.markTime @ +0x08");
_Static_assert(offsetof(markPoly_t, markShader) == 0x0c, "markPoly_t.markShader @ +0x0c");
_Static_assert(offsetof(markPoly_t, alphaFade) == 0x10, "markPoly_t.alphaFade @ +0x10");
_Static_assert(offsetof(markPoly_t, colors) == 0x14, "markPoly_t.colors @ +0x14");
_Static_assert(offsetof(markPoly_t, numVerts) == 0x28, "markPoly_t.numVerts @ +0x28");
_Static_assert(offsetof(markPoly_t, verts) == 0x30, "markPoly_t.verts @ +0x30");
_Static_assert(offsetof(markPoly_t, duration) == 0x170, "markPoly_t.duration @ +0x170");
#endif
enum {
    MAX_MARK_POLYS = 1024
}; /* pool node count (0x17400 dwords / 0x174 bytes) */
/* 0x303b5d20 .data — cg_freeMarkPolys: head of the singly-linked free list of
 * markPoly_t (threaded via ->nextMark). CG_InitMarkPolys points it at
 * &cg_markPolys[0]; CG_AllocMark pops from it, and reclaimed active marks are
 * pushed back onto it. */
extern struct markPoly_s *cg_freeMarkPolys;
/* 0x303b5d40 .data — cg_markPolys: the mark-poly pool array (MAX_MARK_POLYS
 * markPoly_t). CG_InitMarkPolys zeroes and free-list-threads the whole array. */
extern struct markPoly_s cg_markPolys[MAX_MARK_POLYS];
/* 0x30412d40 .data — cg_activeMarkPolys: the sentinel node of the circular
 * doubly-linked list of active marks. Its prevMark (+0x0, at 0x30412d40) points
 * at the oldest active mark and nextMark (+0x4, at 0x30412d44) at the newest;
 * CG_InitMarkPolys points both at the sentinel itself (empty list). CG_AllocMark
 * appends at nextMark and reclaims from prevMark. */
extern struct markPoly_s cg_activeMarkPolys;
/* 0x30412ecc .data — cg_marks_vmCvar.integer: the cached .integer value of the
 * cg_addMarks cvar (draw bullet/blast decals). Both mark-subsystem functions gate
 * on it identically: CG_ImpactMark (0x3002e536) returns early when it is 0 and
 * skips creating a new mark, and CG_AddMarks (0x3002e8c0) returns early when it is
 * 0 and skips drawing/aging the active marks. Read-only in code (the cvar refresh
 * writes it elsewhere). Mechanical owner=veh_unlinkplayer was a first-touch label
 * and is rejected: this is the marks-enable gate, not a player-unlink datum. */
/* 0x30413100 .data refs=1 width=imm first=0x30022c07 owner=cmd_callvote_f */

/* 0x30413460: cl_serverloadwaiting vmCvar_t. cg_cvarTable[179] binds this object
 * to "cl_serverloadwaiting"; loading-screen paths test .integer at +0x0c. */
extern vmCvar_t cl_serverloadwaiting;
/* 0x30413580 .data refs=32 width=4 first=0x30005a76 (mechanical owner=g_freevehicle
 * is a first-touch label, rejected). Base of the cg_weaponInfos[] table: an array
 * of cgWeaponInfo_t (0x1c4-byte PE32 per-weapon cgame records), indexed
 * 1..bg_numWeapons. The 128-entry extent is proven by the next original table
 * base, as documented at the definition in globals.c. */
extern cgWeaponInfo_t cg_weaponInfos[MAX_WEAPONS];
/* 0x304219c8 .data refs=20 width=4 first=0x300168d6 owner=cg_asset_parse.
 * Type repaired uint32_t->float: the mechanical dword truncation is wrong, this is a
 * float config/cvar value. CG_DrawObjectivePointers (0x3002fe70) reads it via FCOMP/
 * FSUB against a screen distance as the objective-pointer NEAR distance (below it the
 * marker is fully opaque and full-size). The compass and objective-pointer
 * consumers prove the shared near-distance role. */
/* View-weapon right-axis offset (classic cg_gun_y role), consumed as a float. */

/* 0x30421c0c .data refs=1 width=4 first=0x3003a50e — cg_noTaunt_vmCvar.integer:
 * voice-chat category filter flag read only by CG_ParseVoiceChat (0x3003a410); when
 * nonzero, insult/taunt/praise/gauntlet voice-chat categories are suppressed. The old
 * owner-tag bg_canitembegrabbed came from a rejected size-match name. */

/* 0x30421d2c is cg_scoreboardScrollStep_vmCvar.integer. The cvar table registers
 * `cg_scoreboardScrollStep` with default "3" and the engine updates this mirror
 * through the registered vmCvar_t pointer. CG_KeyEvent/CG_NextWeapon_f subtract
 * it from cg_scoreboardScrollPos; CG_ScrollScoreboardDown/CG_PrevWeapon_f add it.
 * Those direct consumers interpret the dword result as signed for clamping. */
/* 0x30421e4c .data refs=1 width=4 first=0x30048536 owner=veh_updateclient */
/* 0x30422054..0x3042205b are g_uiDCInstance.cursorx/cursory; vmMain command 8
 * copies its mouse-coordinate source pair into those fields. */
/* 0x30422064..0x3044028b are fields of g_uiDCInstance (+0x104..+0x1e32b), not independent globals. */
/* 0x3044034c .data refs=21: the local player's currently-selected weapon index,
 * used engine-wide as an index into bg_weaponInfos (e.g. CG_SelectWeaponIndex
 * 0x30047390 reads bg_weaponInfos[cg_weaponSelect_vmCvar.integer]->altWeapon; the weapon-select
 * iterators 0x30046bb0/0x30047820/0x300478a0/0x30047960 read it likewise). The
 * mechanical owner=vectordistance2d label is a first-touch artifact and wrong.
 * Name provisional-by-role; no cgame writer is visible in this DLL (engine- or
 * indirectly written), so the exact source name is not fully proven. */

/* 0x3044046c: cg_stats.integer. The cvar table entry at 0x30085760 binds
 * vmCvar 0x30440460 to the name "cg_stats" (0x30079094); this +0x0c member
 * enables the per-frame "cg.clientFrame:%i" diagnostic. */

/* 0x304407c8 .data — moving-tracer width/scale for tracer mode A (selected when
 * localEntity_t.leFlags == LEF_TRACER_MODE_A). A float, read (never written) by
 * CG_AddMovingTracer (0x3002ab00) and the sibling tracer builders 0x30048460 /
 * 0x30048a00, which FLD it as `float ptr` and multiply the tracer poly width by
 * it. Runtime-tunable value with no observed writer in this DLL (set by an
 * initializer outside .text, cvar-like). Type repaired uint32_t->float; the
 * mechanical owner=pm_switchifempty label was a size-guess and is dropped. Exact
 * original symbol name unresolved (no writer/string), so the address suffix is
 * kept as the only disambiguator. */

/* 0x30440a00..0x30447a04 is the single cg_gameState object declared above.
 * Address-shaped scalars in this interval are stringOffsets[] elements or bytes
 * within the packed stringData buffer, not independent globals. */
/* 0x30447a04: shared inherited glconfig_t block filled by trap 0x4e during CG_Init.
 * CoDUOMP.exe 0x0040339a copies the complete 0xa0-byte i386 renderer record
 * (MOV ECX,0x28; REP MOVSD), including five leading pointers. Keep those
 * pointers typed so this engine/module ABI expands consistently on 64-bit
 * hosts; a fixed 0x84-byte prefix would leave vidWidth/vidHeight at their
 * obsolete i386 offsets and make the engine's native-sized copy overrun this
 * object. The original i386 member offsets and total size remain asserted. */
extern glconfig_t cgs_glconfig;
/* 0x30447aa4 .data refs=96 (owner=pm_cmdscale label is wrong).
 * cgs.screenXScale: virtual -> real horizontal scale factor. Written once in
 * the cgs-init path (0x3002e016) as (float)glconfig.vidWidth * (1.0f/640.0f)
 * [FILD [0x30447a88]; FMUL [0x3007c3f4]=0.0015625f; FSTP [0x30447aa4]], and
 * multiplied into every X/width UI coordinate. Consumed as a float by CG_DrawPic
 * (0x3001caa0) and the other 2D draw wrappers (e.g. CG_DrawTurretTagQuad at
 * 0x3001ccf0). The 1/640 divisor is proven from the writer's .rdata constant
 * 0x3007c3f4. */
extern float cgs_screenXScale;
/* 0x30447aa8 .data refs=96 (owner=pm_cmdscale label is wrong).
 * cgs.screenYScale: virtual -> real vertical scale factor. Written once in the
 * cgs-init path (0x3002e028) as (float)glconfig.vidHeight * (1.0f/480.0f)
 * [FILD [0x30447a8c]; FMUL [0x3007c3f0]=0.0020833334f; FSTP [0x30447aa8]], and
 * multiplied into every Y/height UI coordinate. Consumed as a float by CG_DrawPic
 * (0x3001caa0) and the other 2D draw wrappers (e.g. CG_DrawTurretTagQuad at
 * 0x3001ccf0). The 1/480 divisor is proven from the writer's .rdata constant
 * 0x3007c3f0. */
extern float cgs_screenYScale;
/* 0x30447ab0 (.data): cgs.serverCommandSequence — the highest server-command
 * sequence number cgame has processed. Lives in the cgs block (adjacent to the
 * cgs.screenXScale/screenYScale fields above). Printed as the "cmd:%i" field of
 * CG_DrawSnapshot (0x30018020), matching stock CoD/Q3 which draws
 * cgs.serverCommandSequence there; also read/updated in the cgame init/config
 * path (write at 0x3002dfbb, reads at 0x3003b476/0x3003b49b). Retyped from the
 * mechanical uint32_t; the (owner=scr_getobjectfield) label was wrong. */
extern int32_t cgs_serverCommandSequence;
/* 0x30447ab4 (.data): cg.processedSnapshotNum — the snapshot number cgame has
 * consumed up through. CG_ReadNextSnapshot (0x3003d220) increments it before each
 * trap_GetSnapshot and prints the "way out of range" warning when
 * cg.latestSnapshotNum - processedSnapshotNum > 1000; CG_ProcessSnapshots
 * (0x3003d2d0) compares it against cg.latestSnapshotNum to drive the read loop.
 * Signed compare (JLE/JGE/JL). Retyped from the mechanical uint32_t (owner=g_runframe
 * was the first-touching function, not the identity). */
extern int32_t cg_processedSnapshotNum;
/* 0x30447ab8 .data refs=5 width=4 first=0x30018bd0.
 * cgs_localServer — cgame "config values already registered" once-guard.
 * Set in the config/init path (0x3002b20d), read as `if (flag != 0) return;` by
 * every config-value setter (CG_SetConfigValues 0x30038430, sibling 0x30038380,
 * 0x30018bd0) to make cvar registration idempotent. Role name; exact CoD symbol
 * unproven. Mechanical owner=cmd_veh_fireturret is a wrong first-touch guess. */
extern int32_t cgs_localServer;
/* ------------------------------------------------------------------------
 * cgs serverinfo mirror cluster (0x30447abc..0x30447c20). CG_ParseServerinfo
 * (0x30038380) parses config string 0 (the serverinfo string) and fills these
 * four fields via Info_ValueForKey; many cgame consumers then read them. The
 * four are contiguous in memory in this order: gametype[32], hostname[256],
 * maxclients (int32), mapname[64]. Sizes proven by the Q_strncpyz/Com_sprintf
 * size arguments (0x20, 0x100, 0x40) and the address deltas. Mechanical
 * owner=<first-touch> labels (script_method_scriptbuiltin_getatt,
 * g_getactivateent, cg_playerturretpositionandblend, g_runframe) are wrong
 * first-touch guesses and are rejected. Exact CoD cgs_t field names are the
 * stock Q3/CoD gametype/hostname/maxClients/mapname mirrors; kept as cgs_*.
 * ------------------------------------------------------------------------ */
/* 0x30447abc .data — cgs.gametype: g_gametype cvar value copied from serverinfo
 * (Q_strncpyz size 0x20). Read by e.g. 0x3002d5ef, 0x30031c40, 0x30036361. */
extern char cgs_gametype[32];
/* 0x30447adc .data — cgs.hostname: sv_hostname value from serverinfo
 * (Q_strncpyz size 0x100). Read by e.g. 0x30036757, 0x300367cd, 0x3003682e. */
extern char cgs_hostname[256];
/* 0x30447bdc .data — cgs.maxclients: atoi(sv_maxclients) from serverinfo.
 * Read as an int by 0x30026344 (CMP) and 0x300327f1 (MOV EBP). */
extern int32_t cgs_maxclients;
/* 0x30447be0 .data — cgs.mapname: "maps/mp/%s.bsp" built from the serverinfo
 * mapname (Com_sprintf size 0x40). Read by e.g. 0x3002e0f7..0x3002e1f8,
 * 0x30036420. */
extern char cgs_mapname[MAX_QPATH];
extern char cgs_teamNames[2][64];
/* ------------------------------------------------------------------------
 * cgame vote HUD display cluster (cgs.vote* mirror of the Q3/CoD vote state).
 * These four scalars + the display string + the "modified" flag are rebuilt in
 * batch by CG_BuildVoteHudStrings (0x3002ddf0) at gamestate setup, and updated
 * per-field by the CG_ConfigStringModified dispatcher (0x30038e70) cases 0x10
 * (voteTime), 0x11 (voteString), 0x12 (voteYes), 0x13 (voteNo). The vote HUD is
 * drawn by 0x3001b7d0: it early-outs if voteTime==0, computes the remaining
 * seconds as (voteTime - trap(6) ms)/1000, and formats voteString with the
 * (voteYes/voteNo) tallies. Mechanical owner=cg_drawtracer / itemparse_model_origin
 * first-touch labels are rejected; names proven by the vote-string format literal
 * (0x300779cc "vote string"), the trap(6) end-time idiom, and the /1000 draw. */
/* 0x30447ca0 .data refs=10 width=4. cgs.voteTime: absolute engine-ms end time of
 * the current vote. CG_BuildVoteHudStrings/dispatcher set it to Q_atoi(configString)
 * and, when nonzero, add the engine-milliseconds trap(6) to make it absolute. */
extern int32_t cg_voteTime;
/* 0x30447ca4 .data refs=5. cgs.voteYes: "yes" tally, Q_atoi of the vote-yes config
 * string; pushed as a printf arg by the vote HUD drawer (0x3001bae9/0x3001bb69).
 * CG_PlayFxOnTag's indexed instruction uses this numerical displacement only
 * because MSVC folded the ASCII digit bias into the cg_effectDefs address; it
 * does not access the vote object. */
extern int32_t cg_voteYes;
/* 0x30447ca8 .data refs=4. cgs.voteNo: "no" tally, Q_atoi of the vote-no config
 * string; pushed as a printf arg by the vote HUD drawer (0x3001bae7/0x3001bb56). */
extern int32_t cg_voteNo;
/* 0x30447cac .data refs=5 width=4 first=0x3001ba0e. cgs.voteModified: vote-state
 * "changed since last drawn" flag; set to 1 by the dispatcher (0x30039003), tested
 * and cleared by the vote HUD drawer (0x3001ba0e/0x3001ba1e). Not touched by
 * CG_BuildVoteHudStrings; retained address suffix pending its own consumer pass. */
extern qboolean cg_voteModified;
/* 0x30447cb0..0x30447daf .data. cgs.voteString[256]: the vote HUD display text.
 * Both writers call strncpy(...,255) and then write zero at +255, proving that
 * the final byte is the array terminator rather than a separate "dirty" flag.
 * The drawer at 0x3001ba7a consumes the buffer. */
extern char cg_voteString[256];
extern int32_t cg_teamVoteTime[2];
extern int32_t cg_teamVoteYes[2];
extern int32_t cg_teamVoteNo[2];
extern qboolean cg_teamVoteModified[2];
extern char cg_teamVoteString[2][256];
/* 0x30447fd0 .data refs=11 width=4 first=0x3001bbd3. Timeout HUD end time (ms):
 * CG_BuildTimeoutHudStrings and the CS-0x775 case of CG_ConfigStringModified
 * (0x300390da) set it to Q_atoi(configString) and, when nonzero, add the engine
 * milliseconds trap(6) to make an absolute end time. Written 0 when the scoreboard
 * is disabled. Address suffix retained: the remaining reads (weapon/sound-overlay
 * clusters) are not yet reconstructed, so the timeout role is proven but not fully
 * adjudicated; mechanical owner=pm_reloadclip first-touch label rejected. */
extern int32_t cg_timeoutEndTime;
/* 0x30447fd4 .data refs=4 width=4 first=0x3001bc28. Timeout-active flag: set to 1
 * by CG_BuildTimeoutHudStrings when the parsed end time is nonzero, else 0 (also
 * cleared by the CS-0x775 dispatcher case at 0x300390fa). Address suffix retained
 * pending adjudication of the other consumers. */
extern int32_t cg_timeoutActive;
/* 0x30447fd8..0x304480d7 .data. Timeout HUD display string[256]. Both writers
 * copy 255 bytes and then store a zero at +255, proving one bounded string object;
 * the former separate "dirty flag" interpretation had no read-side evidence. */
extern char cg_timeoutString[256];
/* 0x304480d8: first sibling of the d8/dc/e0 signed HUD-stat triple. Writers
 * parse config string 14 with Q_atoi; exact displayed stat name remains unresolved. */
extern int32_t cg_hudStat14Value;
/*
 * cg_hudStat5Value (0x304480dc) — middle sibling of the parallel HUD-stat
 * integer array d8/dc/e0 (see cg_hudStat6Value below). Written by the same
 * atoi-parsing writer cluster (0x3003885d stores an int here) and read by two of
 * the trap-54 HUD emitters: this slot's display emitter CG_DrawRedScore
 * (0x30031521) and CG_Draw1stPlace (0x30031b60). Signed-compared
 * against the "not set" sentinel -9999 (0xffffd8f1) by the display emitter, so it
 * is a signed int stat, not an unsigned dword. Retail UO owner-draw id
 * CG_RED_SCORE and the matching macOS CG_DrawRedScore symbol establish this
 * slot's displayed role; the flattened global's original enclosing-field name
 * remains unresolved. Mechanical
 * owner=menuparse_itemdef label REJECTED (no menu-parse code touches it). */
extern int32_t cg_hudStat5Value;
/*
 * cg_hudStat6Value (0x304480e0) — an atoi-parsed integer stat displayed by
 * the cgame system-info HUD. The two writers at 0x30038840/0x30038f60 store
 * atoi(str) (callee 0x3005b6ce is atoi: skips ctype-whitespace, parses digits) into
 * this slot and the two adjacent siblings 0x304480d8/0x304480dc, so d8/dc/e0 form a
 * small parallel array of parsed integer stats. Readers compare it against the
 * sentinel -9999 (0xffffd8f1): the display code at 0x300315f1 prints "-" (0x30079760)
 * when it holds the sentinel and otherwise formats the value, and the emitter at
 * 0x30031bd0 forwards va("%2i", value) to cgame trap 54 only when it is not the
 * sentinel. A third reader FILDs it to float at 0x30031856. The mechanical
 * owner=script_orbit label is REJECTED (no script code touches it; it is HUD stat
 * state). Retail UO owner-draw id CG_BLUE_SCORE and the matching macOS
 * CG_DrawBlueScore symbol establish this slot's displayed role; the flattened
 * global's original enclosing-field name remains unresolved. Zero-initialized
 * .data (VA past the on-disk .data tail).
 */
extern int32_t cg_hudStat6Value;
/* 0x304480e4..0x304484e3 .data — cg_gameModels[256], the registered render-model
 * handles for CS_MODELS. The registration loop starts at config string 0x196 and
 * destination 0x304480e8, filling elements 1..255; element 0 remains empty. vmMain
 * command 10 and four entity renderers index from 0x304480e4, proving the base. */
extern qhandle_t cg_gameModels[CS_MODELS_COUNT];
/*
 * cg_effectDefs[80] (0x304484e4, .data): the cgame effect-handle table, indexed
 * by an effect id in [1,80). Each element is an opaque effect handle returned by
 * the engine's "register effect" trap (id 0xe2) and later passed straight to the
 * effect-play traps (0xe7/0xe8/0xe9). Proven an 80-element array by three
 * consumers that index cg_effectDefs[id] after checking 0 < id < 80
 * (0x30022720, 0x30021a30, 0x300164b0), by the registration loop at 0x3002c760
 * that writes handles into it, and by the array end coinciding exactly with the
 * next distinct global (0x304484e4 + 80*4 = 0x30448624). The mechanical export
 * mislabeled this as owner=bg_indexforstring and split element [1] (0x304484e8)
 * into a separate symbol; superseded here as the real array.
 */
extern uint32_t cg_effectDefs[80];
/* NOTE: 0x304484e8 is &cg_effectDefs[1]; the registration loop at 0x3002c755
 * (FUN_3002ba50) loads it as a base pointer. Reconstruct that caller to use
 * &cg_effectDefs[1] rather than a standalone symbol. */
/* 0x30448624 .data: cg_consoleShellShock — the 124-byte shellshock_t parameter
 * block for the manual "cg_shellshock" console command. Declared as the typed
 * shellshock_t in client_recovered.h (next to CG_SetShellShockParams); its owning
 * consumer CG_ShellShock_f (0x300174b0) passes it by pointer. The mechanical
 * owner=concatargs label came from a size-guessed name for 0x300174b0 and is
 * wrong. Storage in globals.c. */
/* 0x304486a0 .data — cg_shellShocks[15]: the loaded shellshock_t parameter blocks for
 * the CS_SHELLSHOCKS config-string block (stride 0x7c). CG_RegisterGraphics (0x3002ba50)
 * loads each nonempty shellshock config string (CG_ShellShockLoad) and fills the block via
 * CG_SetShellShockParams. The loop visits config-string indices 1..15 (index zero
 * is the separate manual block), proving 15 array elements ending at 0x30448de4.
 * owner=pm_updateviewangles
 * was a size-guess and is rejected. Declared as the typed shellshock_t[16] in
 * client_recovered.h (where shellshock_t is complete); storage in globals.c. */
/* 0x30448de4 .data refs=2 width=4 first=0x3002c641 owner=pm_updateviewangles */
extern int32_t cg_numInlineModels;
/* 0x30448de8..0x304495e7 .data — cg_inlineModelHandles[MAX_SUBMODELS]:
 * per-submodel registered render-model
 * handle table (the "*N" inline/brush models). Filled at load time by the
 * registration loop at 0x3002c5.. (formats "*%i - inline models" and stores each
 * model-register trap (0x4d) result at table[i], write 0x3002c67b); the entry
 * count is the adjacent word 0x30448de4. Indexed by currentState.modelindex.
 * The mechanical export mislabeled owner=matrixmultiply43 (the rejected size-guess
 * name of CG_Mover at 0x3001f120) and captured only the first dword; the datum is
 * the array. Consumers: CG_Mover (0x3001f120, read 0x3001f22e) and 0x3001f388.
 * The next independently proven datum is cg_inlineModelMidpoints at 0x304495e8,
 * fixing this interval at 0x800 bytes, or 512 handles. There is no separate
 * original global at the reconstruction's former artificial 256-row split. */
extern int32_t cg_inlineModelHandles[MAX_SUBMODELS];
/* 0x304495e8..0x3044ade8 .data —
 * cgs.inlineModelMidpoints[MAX_SUBMODELS].
 * CG_RegisterGraphics starts at &array[1] (0x304495f4), advances by one vec3,
 * and stops at cg_numInlineModels. CG_EntityEffects indexes the array base with
 * currentState.modelindex. The enclosing interval proves MAX_MODELS == 512. */
extern vec3_t cg_inlineModelMidpoints[MAX_SUBMODELS];
/* 0x3044ade8 .data: cgs.teamChatMsgs — the team-chat scroll ring buffer.
 * TEAMCHAT_HEIGHT (8) lines of TEAMCHAT_WIDTH*3+1 = 271 bytes each (worst case
 * every visible char preceded by a "^x" color code, plus NUL). Written by
 * CG_AddToTeamChat (0x30039390) and read by the team-info drawer (0x30018770).
 * Was mechanically captured as a single uint32_t; the machine code (stride 0x10f
 * = 271 across an 8-entry mod ring) proves the array shape, so superseded here. */
extern char teamChatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_LINE_BYTES];
/* 0x3044b660 .data: cgs.teamChatMsgTimes[TEAMCHAT_HEIGHT] — cg.time stamp per
 * ring line, indexed by (teamChatPos % chatHeight). Written by CG_AddToTeamChat. */
extern int teamChatMsgTimes[TEAMCHAT_HEIGHT];
/* 0x3044b680 .data: cgs.teamChatPos — team-chat ring write cursor (monotonic;
 * indexed modulo the clamped chatHeight). Written by CG_AddToTeamChat. */
extern int teamChatPos;
/* 0x3044b684 .data: cgs.teamChatLastPos — oldest still-visible ring line cursor,
 * trails teamChatPos by at most chatHeight. Written by CG_AddToTeamChat. */
extern int teamChatLastPos;
/* 0x3044b688/0x3044b68c .data — cgs cursor coordinates synchronized into the
 * shared UI display context by vmMain's CGVM_MOUSE_EVENT command. */
extern int32_t cgs_cursorX;
extern int32_t cgs_cursorY;
/* 0x3044b69c..0x3044b6a8 .data: a single 16-byte timed float-value fade animator
 * used by the FOV/zoom subsystem. The mechanical export split it into four
 * g_data_vectosignedangles_* uint32 scalars with a wrong first-touch owner label;
 * superseded here as the struct it actually is. Updater CG_StartFovFade
 * (0x3001ab50) seeds all four fields; the evaluator at 0x3001a7c0 lerps
 * currentValue from startValue over durationMs starting at startTime (via
 * trap_Milliseconds); CG_CalcFov (0x3003ffc0) consumes/updates it. Provisional
 * field names (no cgame symbol table recovered); exact original names unproven. */
typedef struct cgFovFade_s {
    float startValue; /* +0x00 (0x3044b69c) fade start/anchor value */
    float currentValue; /* +0x04 (0x3044b6a0) current interpolated value */
    int32_t startTime; /* +0x08 (0x3044b6a4) cg.time when the fade began (ms) */
    int32_t durationMs; /* +0x0c (0x3044b6a8) fade duration (ms) */
} cgFovFade_t;
extern cgFovFade_t cg_fovFade;
/* 0x3044b6ac/b0/b4: the three initial shader registrations in CG_Init. Tracking
 * EAX across the pipelined stores proves the order white, hudSoftLine,
 * hudSoftLineH; earlier first-touch labels were shifted by one registration. */
extern qhandle_t cgs_media_whiteShader;
extern qhandle_t cgs_media_hudSoftLineShader;
extern qhandle_t cgs_media_hudSoftLineHShader;
/* 0x3044b6b8 .data refs=1 width=4 first=0x3002c4d6 resolved: cgs.media handle for "hudAxisIcon" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_hudAxisIcon;
/* 0x3044b6bc .data refs=1 width=4 first=0x3002c4c5 resolved: cgs.media handle for "hudAlliedIcon" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_hudAlliedIcon;
/* 0x3044b6c0 .data refs=3 width=4 first=0x30017e23 (owner=cg_initvote is wrong).
 * cgs.media hudColorBar shader handle: registered in the cgame media-init path at
 * 0x3002c4b4, storing the qhandle_t returned by the "hudColorBar" shader lookup
 * (string 0x30078048, registered by the call at 0x3002c4a8). Consumed as the
 * qhandle_t passed to trap_R_DrawStretchPic by CG_DrawTeamBackground (0x30017dd0,
 * MOV EAX,[0x3044b6c0] at 0x30017e23) and read again at 0x3001890c. */
extern qhandle_t cgs_media_hudColorBar;
/* 0x3044b6c8 .data — cg_railCoreShader: the qhandle_t of the "railCore" render
 * shader. Registered once by the cgame media-registration path at 0x3002c567
 * (EAX = trap_R_RegisterShader("railCore" @0x30077fb0, 2), stored here), then
 * consumed by the rail-trail segment builder (0x300430a0) which copies it into
 * refEntity.spriteShaderHandle (+0x68). Real name resolved from the registered
 * shader string; supersedes the mechanical owner=pm_updateviewangles guess. */
extern qhandle_t cg_railCoreShader;
/* 0x3044b6cc .data refs=3 width=4 (owner label pm_updateviewangles is the mechanical
 * first-touch and is wrong). cgs_voiceChatIcon: the registered head-icon shader handle
 * for the "headiconVoiceChat" icon, shown over a client while they are voice-chatting.
 * Written once by the cgame media-registration function at 0x3002c54d (result of
 * registering shader "headiconVoiceChat", pushed at 0x30077fd0); read twice by
 * CG_PlayVoiceChat (0x30039ff0) to decide the display duration and to stamp
 * cg_entities[client].voiceChatIcon. */
extern uint32_t cgs_voiceChatIcon;
/* 0x3044b6d0 .data refs=2 (owner label pm_updateviewangles is the mechanical
 * first-touch and is wrong). cgs_talkBalloonIcon: the registered head-icon shader
 * handle for the "headiconTalkBalloon" icon, drawn over a client whose currentState
 * eFlags carry the 0x20000 "typing/chatting" bit. Written once by the cgame
 * media-registration path at 0x3002c55d (result of registering shader
 * "headiconTalkBalloon" @0x30077fbc, type 2), then read only by CG_AddHeadIcon
 * (0x30032ac0). Real name resolved from the registered shader string; supersedes
 * the mechanical owner=pm_updateviewangles guess. */
extern qhandle_t cgs_talkBalloonIcon;
/* 0x3044b6d4 .data refs=2 (owner label pm_updateviewangles is the mechanical
 * first-touch and is wrong). cgs_disconnectedIcon: the registered head-icon shader
 * handle for the "headiconDisconnected" icon, drawn over a client whose currentState
 * eFlags carry the 0x800 "disconnected" bit. Written once by the cgame
 * media-registration path at 0x3002bb25 (result of registering shader
 * "headiconDisconnected" @0x30078320, type 5), then read only by CG_AddHeadIcon
 * (0x30032ac0). Real name resolved from the registered shader string. */
extern qhandle_t cgs_disconnectedIcon;
/* 0x3044b6d8 .data refs=2 (owner label pm_updateviewangles is the mechanical
 * first-touch and is wrong). cgs_youInKillCamIcon: the registered head-icon shader
 * handle for the "headiconYouInKillCam" icon, drawn over the local client's own
 * entity when it is being rendered as the kill-cam subject. Written once by the
 * cgame media-registration path at 0x3002bb36 (result of registering shader
 * "headiconYouInKillCam" @0x30078308, type 5), then read only by CG_AddHeadIcon
 * (0x30032ac0). Real name resolved from the registered shader string. */
extern qhandle_t cgs_youInKillCamIcon;
/* 0x3044b6dc .data refs=1 width=4 first=0x3002bbee resolved: cgs.media handle for "gfx/2d/select" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_selectShader;
/* 0x3044b6e4 .data refs=3 width=4 first=0x3002bbdd resolved: cgs.media handle for "gfx/misc/tracer" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_tracerShader;
/* 0x3044b6e8 .data refs=1 width=4 first=0x3002bb03 resolved: cgs.media handle for "gfx/2d/net.tga" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_lagometerShader;
/* 0x3044b6ec: registered "lagometer" sample-graph shader. */
extern qhandle_t cgs_lagometerShader;
/* 0x3044b6f0 (.data): cgs.media.backTileShader — the tiling shader handle used to
 * fill the letterbox border around a cropped 3D view. Loaded into EAX before each
 * of the four CG_TileClearBox edge draws in CG_TileClear (0x3001d160). qhandle_t
 * stored as a 32-bit int. Zero-initialized .data. (owner=initweaponinfo was the
 * mechanical first-toucher and is wrong.) */
extern qhandle_t cgs_media_backTileShader;
/* 0x3044b6f4 .data refs=2 width=4 first=0x3002c492 resolved: cgs.media handle for "hudNoWeaponIcon" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_hudNoWeaponIcon;
/* 0x3044b6f8..0x3044b720: cgs media handles indexed directly by
 * cg_usableHintKind. Slots 0 and 1 remain zero because those selector values draw
 * nothing; slots 2..9 are registered consecutively by CG_RegisterGraphics. */
/* playerState.serverCursorHint value 10 (== CURSOR_HINT_WEAPON_BASE) — the friendly-player
 * hint. CG_DrawCursorhint (0x300303a0) maps hint kinds >= 10 to
 * cg_weaponHudIcons[kind - 10], whose element 0 is registered from the material
 * "hintFriendly" (CG_RegisterGraphics, 0x3044b720), and CG_DrawCrosshair
 * (0x30019e5c) tints the crosshair green {0.25, 1, 0.25} for exactly this value.
 * Role proven by both consumers and by the game-module hintStrings table. */
extern qhandle_t cgs_media_usableHintShaders[CURSOR_HINT_BUILTIN_ICON_COUNT];
/* 0x3044b720..0x3044b91f and 0x3044b920..0x3044bb1f: per-weapon HUD
 * and ammo-icon handles. CG_RegisterWeapon indexes both arrays by weapon index;
 * CG_RegisterGraphics initializes element zero of the first array to hintFriendly. */
extern qhandle_t cg_weaponHudIcons[MAX_WEAPONS];
extern qhandle_t cg_weaponAmmoIcons[MAX_WEAPONS];
/* cg_stanceHudShaders[9] — the nine preregistered stance/fatigue HUD icon shader
 * handles at 0x3044bb24..0x3044bb47 (contiguous qhandle_t[9]). Registered by the
 * HUD asset setup (0x3002bc9b..0x3002bd26) via trap_R_RegisterShader(name,5):
 *   [0] hudStanceStand   [1] hudStanceCrouch [2] hudStanceProne [3] hudStanceSprint
 *   [4] hudStanceFlash   [5] hudFatigueStand [6] hudFatigueCrouch
 *   [7] hudFatigueProne  [8] hudFatigueSprint
 * Read by CG_DrawPlayerStance (0x3002f63b..0x3002f8db). The old mechanical owner
 * label "pm_updateviewangles" was a wrong size-guess; superseded here. */
extern qhandle_t cg_stanceHudShaders[9];
/* 0x3044bb48 .data — cg_hudObjectiveReserved: a single qhandle_t slot between the
 * cg_stanceHudShaders[9] array and cg_objectiveShaders[3]. CG_RegisterGraphics (0x3002ba50)
 * explicitly zeroes it (MOV [0x3044bb48],0 at 0x3002bd2b) as part of objective-asset setup;
 * no other reconstructed code references it. Provisional role name (reserved/unused objective
 * slot); owner=pm_updateviewangles was a size-guess and is rejected. */
extern qhandle_t cg_hudObjectiveReserved;
/*
 * cg_objectiveShaders[3] — the three preregistered HUD objective/compass icon
 * shader handles, at 0x3044bb4c/0x50/0x54. Written by the objective-asset setup
 * (0x3002bd35..0x3002bd68), each entry filled by trap_R_RegisterShader(name, 5):
 *   [0] = "hudObjective"     (0x300781b4)
 *   [1] = "hudObjectiveUp"   (0x300781a4)
 *   [2] = "hudObjectiveDown" (0x30078190)
 * Read by CG_GetObjectiveShaderForDir (0x3002fd90) as cg_objectiveShaders[dir]
 * when no per-shader config-string override is requested. The old mechanical
 * owner label "pm_updateviewangles" was wrong (size-guess); superseded here.
 */
extern qhandle_t cg_objectiveShaders[3];
/* Compass blip materials registered by CG_RegisterGraphics and consumed by the
 * two team/vehicle compass passes. */
extern qhandle_t cg_compassFriendlyShaders[2];
extern qhandle_t cg_compassTankShaders[3];
/* 0x3044bb6c .data — cg_hitDirectionShader: the 2D shader handle for the
 * damage-direction ("hit direction") HUD arrow. Registered by the asset-load path
 * (0x3002bdc5) via CG_RegisterMaterial("hudHitDirection", R_IMAGE_TRACK_HUD),
 * and consumed by CG_DrawDamageDirectionIndicators (0x3001aac9) as the quad's shader. The
 * material name "hudHitDirection" proves the role; mechanical owner=cg_ejectweaponbrass
 * rejected. */
extern qhandle_t cg_hitDirectionShader;
/* 0x3044bb70 (.data): cg_numberShaders[11] — the registered 2D shader handles for
 * the HUD bitmap number font: indices 0..9 are the digit glyphs '0'..'9' and index
 * 10 is the minus sign '-'. The registration loop at 0x3002bad1..0x3002baee fills all
 * eleven slots (ESI = 0,4,..,0x28; RegisterShader(name[i]) stored to [ESI+0x3044bb70]),
 * proving the array shape and the 0x2c-byte extent (next global at 0x3044bb9c). Read
 * back as cg_numberShaders[digit] by the HUD digit drawer 0x30031300 (MOV ECX,[ESI*4+
 * 0x3044bb70], ESI = digitValue or 10 for '-') and by 0x30017cfb. The mechanical export
 * only captured element 0; superseded here as the full qhandle_t[11] array (mechanical
 * owner=pm_cmdscale label rejected — a first-toucher, not the identity). Provisional
 * role name; exact source symbol unresolved. */
extern qhandle_t cg_numberShaders[11];
/* 0x3044bb9c .data refs=2 width=4 first=0x3002c5e4 resolved: cgs.media handle for "markShadow" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_markShadowShader;
/* 0x3044bba0 .data refs=1 width=4 first=0x3002c4a3 resolved: cgs.media handle for "flareShader" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_flareShader;
/* 0x3044bba4 .data refs=2 width=4 first=0x3002c5fd resolved: cgs.media handle for the
 * "wake" material (registered by CG_RegisterGraphics 0x3002c5f1: PUSH 0x30077f90
 * "wake"; the old markShadowFade label was a mis-read of the adjacent
 * "markShadow" string). Consumed by CG_AddPlayerWaterShadow, matching the
 * Q3/RTCW cgs.media.wakeMarkShader role. */
extern qhandle_t cgs_media_wakeMarkShader;
/* 0x3044bba8 .data refs=1 width=4 first=0x3002c503 resolved: cgs.media handle for "gfx/hud/headicon@allies_flag" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_headiconAlliesFlag;
/* 0x3044bbac .data refs=1 width=4 first=0x3002c4e7 resolved: cgs.media handle for "gfx/hud/headicon@axis_flag" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_headiconAxisFlag;
/* 0x3044bbb0 .data refs=1 width=4 first=0x3002b615 owner=playercmd_finishplayerdamage */
extern const char *cg_soundPlayerGib;
/* 0x3044bbb4 .data refs=1 width=4 first=0x3002b62a owner=playercmd_finishplayerdamage */
extern const char *cg_soundPlayerGibBounce;
/* 0x3044bbb8 (.data): cg_soundOutOfAmmo — the registered sound identifier for the
 * "player_out_of_ammo" local sound. The cgame sound-registration pass (0x3002b560)
 * registers "player_out_of_ammo" (string 0x30078794) via engine syscall 0xc3 and
 * stores the returned handle here (MOV [0x3044bbb8],EAX at 0x3002b654 captures the
 * result of the 0x3002b644 syscall whose arg was that string). CG_OutOfAmmoChange
 * (0x30034a00) loads it and passes it as the sound identifier to CG_PlaySoundAliasByName.
 * Typed const char * to match CG_PlaySoundAliasByName's soundName parameter. Was the
 * mechanical g_data_cmd_callvote_f_3044bbb8 (owner=first-touching function, wrong). */
extern const char *cg_soundOutOfAmmo;
/* 0x3044bbbc .data refs=2 width=4 first=0x30022a4f owner=cmd_callvote_f */
extern const char *cg_soundLandDamage;
/* 0x3044bbc0 .data refs=1 width=4 first=0x300234bc owner=cmd_callvote_f */
extern const char *cg_soundDeath; /* event sound-name pointer (retyped from uint32_t; CG_EntityEvent) */
/* 0x3044bbc4 .data refs=1 width=4 first=0x3002318f owner=cmd_callvote_f */
extern const char *cg_soundPlayerTeleportIn; /* event sound-name pointer (retyped from uint32_t; CG_EntityEvent) */
/* 0x3044bbc8 .data refs=1 width=4 first=0x300231a9 owner=cmd_callvote_f */
extern const char *cg_soundPlayerTeleportOut; /* event sound-name pointer (retyped from uint32_t; CG_EntityEvent) */
/* 0x3044bbcc .data refs=1 width=4 first=0x300231ce owner=cmd_callvote_f */
extern const char *cg_soundItemRespawn; /* event sound-name pointer (retyped from uint32_t; CG_EntityEvent) */
/* 0x3044bbd0 .data: registered "player_talk" sound reference. The registration
 * pipeline at 0x3002b64a..0x3002b669 stores the result of registering the literal
 * "player_talk" here; CG_DrawVote plays it when voteModified is consumed. */
extern const char *cgs_media_playerTalkSound;
/* 0x3044bbd4 .data refs=2 width=4 first=0x30023257 owner=cmd_callvote_f */
extern const char *cg_grenadeBounceSurfaceSounds[23];
/* 0x3044bc30 .data refs=2 width=4 first=0x30023417 owner=cmd_callvote_f */
extern const char *cg_grenadeExplodeSurfaceSounds[23];
extern const char *cg_unusedSurfaceSoundSet0[23];
/* 0x3044bce8 .data refs=1 width=imm first=0x3002b7c3 owner=playercmd_finishplayerdamage */
extern const char *cg_rocketExplodeSurfaceSounds[23];
extern const char *cg_unusedSurfaceSoundSet1[23];
/* 0x3044bda0 .data refs=1 width=imm first=0x3002b7e1 owner=playercmd_finishplayerdamage */
extern const char *cg_artilleryExplodeSurfaceSounds[23];
/* 0x3044bdfc .data refs=1 width=imm first=0x3002b7d2 owner=playercmd_finishplayerdamage */
extern const char *cg_mortarExplodeSurfaceSounds[23];
/* 0x3044be58 .data refs=1 width=imm first=0x3002b7f0 owner=playercmd_finishplayerdamage */
extern const char *cg_tankExplodeSurfaceSounds[23];
extern const char *cg_eventSurfaceSounds[23];
/* 0x3044bf10 .data refs=3 width=4 first=0x3002b7ff owner=playercmd_finishplayerdamage.
 * One canonical "bullet_small_<surfaceName>" alias-name pointer per
 * surface type (0..22). Filled once by CG_RegisterSurfaceTypeSounds(table, "bullet_small")
 * at 0x3002b7ff; consumed indexed by surface type at 0x30048f11 and by
 * CG_ModelEventFireWeapon (0x3004906b). */
extern const char *cg_bulletSmallSurfaceSounds[23];
/* 0x3044bf6c .data refs=3 width=4 first=0x3002b80e owner=playercmd_finishplayerdamage.
 * One canonical "bullet_large_<surfaceName>" alias-name pointer per
 * surface type (0..22). Filled once by CG_RegisterSurfaceTypeSounds(table, "bullet_large")
 * at 0x3002b80e; consumed indexed by surface type at 0x30048f04 and by
 * CG_ModelEventFireWeapon (0x30049074). */
extern const char *cg_bulletLargeSurfaceSounds[23];
/* 0x3044bfc8 .data refs=1 width=imm first=0x3002b81d owner=playercmd_finishplayerdamage */
extern const char *cg_stepSprintSurfaceSounds[23];
/* 0x3044c024 .data refs=1 width=imm first=0x3002b82c owner=playercmd_finishplayerdamage */
extern const char *cg_stepRunSurfaceSounds[23];
/* 0x3044c080 .data refs=1 width=imm first=0x3002b83b owner=playercmd_finishplayerdamage */
extern const char *cg_stepWalkSurfaceSounds[23];
/* 0x3044c0dc .data refs=1 width=imm first=0x3002b84a owner=playercmd_finishplayerdamage */
extern const char *cg_stepProneSurfaceSounds[23];
/* 0x3044c138 .data refs=1 width=imm first=0x3002b859 owner=playercmd_finishplayerdamage */
extern const char *cg_shellFlashSurfaceSounds[23];
/* 0x3044c194 .data refs=2 width=4 first=0x300232fa owner=cmd_callvote_f */
extern const char *cg_shellFlashSounds[8];
/* 0x3044c1b4 .data refs=2 width=4 first=0x3002332f owner=cmd_callvote_f */
extern const char *cg_barrageIncomingSounds[8];
/* 0x3044c1d4 .data refs=2 width=4 first=0x30022910 owner=cmd_callvote_f */
extern const char *cg_soundGearRattleSprint;
/* 0x3044c1d8 .data refs=3 width=4 first=0x300228c4 owner=cmd_callvote_f */
extern const char *cg_soundGearRattleRun;
/* 0x3044c1dc .data refs=2 width=4 first=0x3002295c owner=cmd_callvote_f */
extern const char *cg_soundGearRattleWalk;
/* 0x3044c1e0 .data refs=2 width=4 first=0x30022b09 owner=cmd_callvote_f */
extern const char *cg_soundMovementFoliage;
/* 0x3044c1e4 .data refs=2 width=4 first=0x3002b953 owner=playercmd_finishplayerdamage */
extern const char *cg_soundWhizby;
/* 0x3044c1e8 .data refs=2 width=4 first=0x3002312e owner=cmd_callvote_f */
extern const char *cg_soundMeleeSwingLarge;
/* 0x3044c1ec .data refs=2 width=4 first=0x30023115 owner=cmd_callvote_f */
extern const char *cg_soundMeleeSwingSmall;
/* 0x3044c1f0 .data refs=2 width=4 first=0x30023157 owner=cmd_callvote_f */
extern const char *cg_soundMeleeHit;
/* 0x3044c1f4 .data refs=2 width=4 first=0x30022b3c owner=cmd_callvote_f */
extern const char *cg_soundFatigueBreath;
/* 0x3044c1f8 .data refs=2 width=4 first=0x30022b23 owner=cmd_callvote_f */
extern const char *cg_soundSprintBreathLast;
/* 0x3044c1fc .data refs=2 width=4 first=0x30023298 owner=cmd_callvote_f */
extern const char *cg_soundUsGrenadeLever;
/* 0x3044c200 .data refs=2 width=4 first=0x3002324a owner=cmd_callvote_f */
extern const char *cg_soundSatchelBounce;
/* 0x3044c204 .data refs=2 width=4 first=0x300232d6 owner=cmd_callvote_f */
extern const char *cg_soundMgOverheat;
/* 0x3044c208 .data refs=2 width=4 first=0x300232c1 owner=cmd_callvote_f */
extern const char *cg_soundMgOverheatVehicle;
/* 0x3044c20c .data refs=2 width=4 first=0x3002ba13 owner=playercmd_finishplayerdamage */
extern const char *cg_soundGameMessage;
/* 0x3044c210 .data refs=1 width=4 first=0x3002ba2b owner=playercmd_finishplayerdamage */
extern const char *cg_soundObjectiveComplete;
/* 0x3044c214 .data refs=1 width=4 first=0x3002b5be owner=playercmd_finishplayerdamage */
extern const char *cg_soundMpAnnounceGTwoMinutes;
/* 0x3044c218 .data refs=1 width=4 first=0x3002b5d3 owner=playercmd_finishplayerdamage */
extern const char *cg_soundMpAnnounceATwoMinutes;
/* 0x3044c21c .data refs=1 width=4 first=0x3002b5eb owner=playercmd_finishplayerdamage */
extern const char *cg_soundMpAnnounceGThirtySeconds;
/* 0x3044c220 .data refs=1 width=4 first=0x3002b600 owner=playercmd_finishplayerdamage */
extern const char *cg_soundMpAnnounceAThirtySeconds;
/* 0x3044c224 .data refs=2 width=4 first=0x30022cff owner=cmd_callvote_f */
extern const char *cg_soundPlayerWaterIn;
/* 0x3044c228 .data refs=2 width=4 first=0x30022d19 owner=cmd_callvote_f */
extern const char *cg_soundPlayerWaterOut;
/* 0x3044c22c .data refs=1 width=4 first=0x3002b714 owner=playercmd_finishplayerdamage */
extern const char *cg_soundDebrisBounce;
/* 0x3044c230 .data — the four per-second grenade cook-off pulse sound alias
 * names. CG_DrawCrosshair (0x3001a150) plays cg_soundGrenadePulse[seconds] once
 * per wrap of the cook-off millisecond remainder for whole seconds 0..3; the
 * writer at 0x3002b6c0 fills the table. The old owner=veh_playercollision label
 * was the consumer's size-guess mislabel. */
extern const char *cg_soundGrenadePulse[4];
/* 0x3044c234 .data refs=1 width=4 first=0x3002b6d5 owner=playercmd_finishplayerdamage */
/* 0x3044c240 .data refs=1 width=4 first=0x3002ba3b owner=playercmd_finishplayerdamage */
extern const char *cg_soundSpotlightSpark;
/* 0x3044c244 .data — cg_flameFireSound: canonical alias-name pointer from trap 0xc3,
 * "flamethrower_fire") stored at 0x3002b741. RESOLVED by tracing the sound-
 * registration writer (0x3002b700 block registers the flamethrower sound aliases
 * in order). CG_UpdateFlamethrowerSounds gates a per-chunk sound emit on it (0x30029546). */
extern const char *cg_flameFireSound;
/* 0x3044c24c .data — cg_flameStartSound: an alias-name pointer resolved by a flame-init
 * function (writer 0x3002b756) and passed as the soundName to CG_PlaySoundAliasByName
 * by CG_EmitPlayerFlameChunks (0x3002522c, the terminal-chunk emit path).
 * RESOLVED (role) on consume; exact CoD sound alias name unresolved. */
extern const char *cg_flameStartSound;
/* 0x3044c250 .data — cg_flameStreamSound: alias-name pointer from trap 0xc3,
 * "flamethrower_stream") stored at 0x3002b76b. CG_UpdateFlamethrowerSounds gates the
 * active-stream sound emit on it (0x300294d2). */
extern const char *cg_flameStreamSound;
/* 0x3044c258 .data — cg_flameCooldownSound: alias-name pointer from trap 0xc3,
 * "flamethrower_cooldown") stored at 0x3002b795. CG_UpdateFlamethrowerSounds gates the
 * cooldown/decay sound emit on it (0x30029645/0x300296e8). */
extern const char *cg_flameCooldownSound;
/* 0x3044c25c .data refs=1 width=4 first=0x3002b780 owner=playercmd_finishplayerdamage */
extern const char *cg_soundPlayerBoneBounce;
/* 0x3044c260 .data refs=1 width=4 first=0x3002b729 owner=playercmd_finishplayerdamage */
extern const char *cg_soundDebrisHitPlayer;
/* 0x3044c264 .data refs=2 width=4 first=0x300231e8 owner=cmd_callvote_f */
extern const char *cg_soundFlameBarrelBounce;
/* 0x3044c268 .data refs=1 width=4 first=0x3002bffe resolved: cgs.media handle for "ui/assets/checkbox_clear" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_checkboxClear;
/* 0x3044c26c .data refs=1 width=4 first=0x3002c232 resolved: cgs.media handle for "ui/assets/checkbox_checked" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_checkboxChecked;
/* 0x3044c270 .data refs=1 width=4 first=0x3002c470 resolved: cgs.media handle for
 * "ui/assets/checkbox_fail" (0x30078080, trap id 0x59 at 0x3002c459). A prior mechanical
 * pass mislabeled this cgs_media_backTileShader (a name-address collision); the real
 * backTileShader is 0x3044b6f0 ("gfx/2d/backtile"). Binary-adjudicated. */
extern qhandle_t cgs_media_checkboxFail;
/* CG_IMPACT_EFFECT_TYPES — number of impact effect types (weapon/explosion
 * categories). Proven from CG_RegisterImpactEffects (0x3001dfb0): it walks 22
 * parallel (name, effectDef_t[24], qhandle_t[24]) triples — the final loop runs its
 * byte cursor from 0 to 0x58 in steps of 4 (0x58/4 == 22), and hands the parser the
 * count 0x16 (22). */
enum {
    CG_IMPACT_EFFECT_TYPES = 22
};

/* CG_IMPACT_SURFACE_TYPES — surface types per effect type (one effectDef_t /
 * qhandle_t per surface). Proven from CG_RegisterImpactEffects: the per-type
 * effectDef_t table stride is 0x600 == 24 * sizeof(effectDef_t), and the per-type
 * qhandle_t rows are 0x60 == 24 * 4 apart. Matches the 24-surface loop in
 * CG_RegisterEffectDefSurfaces. */
enum {
    CG_IMPACT_SURFACE_TYPES = 24
};

/* 0x3044c274 .data: cg_impactEffects[CG_IMPACT_EFFECT_TYPES][CG_IMPACT_SURFACE_TYPES]
 * — the registered impact/explosion effect-handle table, one qhandle_t per
 * (effectType, surfaceType). RESOLVED, superseding the mechanical export which
 * mislabeled this single contiguous array as 22 separate `g_trypushingentity`
 * scalars plus three overlapping `[0x400]` tables (the `g_trypushingentity` owner
 * name is a size-guess for FUN_3001dfb0, whose real name is CG_RegisterImpactEffects).
 *
 * Proven to be ONE array by two independent consumers:
 *  - the producer CG_RegisterImpactEffects (0x3001dfb0) hands the 22 row bases
 *    (&cg_impactEffects[i][0], each 0x60 == 24*4 apart, contiguous from 0x3044c274
 *    to 0x3044cab4 == 0x3044c274 + 22*24*4) to CG_RegisterEffectDefSurfaces, which
 *    fills 24 surface handles per row;
 *  - CG_EntityEvent (0x30022810) reads it flat as cg_impactEffects[row][weapon +
 *    surface*24] for the weapon-fire/impact events (rows 12/13/14 were the three
 *    interior `[0x400]` symbols; the flat index walks across rows, which is why
 *    row-major C storage reproduces the machine code exactly).
 * Retyped from uint32_t to qhandle_t (the register-effect trap returns a qhandle_t).
 * Zero-initialized. The `[0x400]` over-sizing on the old interior symbols is dropped
 * in favor of the array's proven [22][24] extent. */
extern qhandle_t cg_impactEffects[CG_IMPACT_EFFECT_TYPES][CG_IMPACT_SURFACE_TYPES];
/* 0x3044cab4 .data refs=2 width=4 first=0x3002c836 resolved: cgs.media handle for "fx/impacts/flesh_hit_noblood.efx" (registered by CG_RegisterGraphics 0x3002ba50; owner=pm_updateviewangles was a size-guess). */
extern qhandle_t cgs_media_fleshImpactEffect;
/* 0x3044cab8 .data refs=1 width=4 first=0x30027a7f owner=bg_finditem (mechanical
 * owner is wrong). RESOLVED: the flamethrower smoke effect handle. CG_InitFlameChunks
 * (0x30027a7f) stores here the effect handle returned by registering the effect asset
 * "fx/smoke/smoke_flamethrower.efx" via cgame trap 0xe2. cgame effect handle. */
extern qhandle_t cg_flameSmokeEffect;
/* 0x3044cabc .data refs=1 width=4 first=0x30027a94 owner=bg_finditem (mechanical
 * owner is wrong). RESOLVED: the large flamethrower smoke effect handle.
 * CG_InitFlameChunks (0x30027a94) stores here the effect handle returned by
 * registering "fx/smoke/smoke_flamethrower_lg.efx" via cgame trap 0xe2. */
extern qhandle_t cg_flameSmokeEffectLarge;
typedef enum vehicleTreadEffectIndex_e {
    VEH_TREAD_EFFECT_NONE = 0,
    VEH_TREAD_EFFECT_TANK_GRASS,
    VEH_TREAD_EFFECT_TANK_SAND,
    VEH_TREAD_EFFECT_TANK_DIRT,
    VEH_TREAD_EFFECT_TANK_ROCK,
    VEH_TREAD_EFFECT_TANK_SNOW,
    VEH_TREAD_EFFECT_TANK_SNOW_ALT,
    VEH_TREAD_EFFECT_JEEP_OFFSET = 6,
    VEH_TREAD_EFFECT_JEEP_GRASS = VEH_TREAD_EFFECT_TANK_GRASS + VEH_TREAD_EFFECT_JEEP_OFFSET,
    VEH_TREAD_EFFECT_JEEP_SAND = VEH_TREAD_EFFECT_TANK_SAND + VEH_TREAD_EFFECT_JEEP_OFFSET,
    VEH_TREAD_EFFECT_JEEP_DIRT = VEH_TREAD_EFFECT_TANK_DIRT + VEH_TREAD_EFFECT_JEEP_OFFSET,
    VEH_TREAD_EFFECT_JEEP_ROCK = VEH_TREAD_EFFECT_TANK_ROCK + VEH_TREAD_EFFECT_JEEP_OFFSET,
    VEH_TREAD_EFFECT_JEEP_SNOW = VEH_TREAD_EFFECT_TANK_SNOW + VEH_TREAD_EFFECT_JEEP_OFFSET,
    VEH_TREAD_EFFECT_JEEP_SNOW_ALT = VEH_TREAD_EFFECT_TANK_SNOW_ALT + VEH_TREAD_EFFECT_JEEP_OFFSET,
    VEH_TREAD_EFFECT_COUNT = VEH_TREAD_EFFECT_JEEP_SNOW_ALT + 1
} vehicleTreadEffectIndex_t;

/* cgs_media_vehicleTreadEffects (0x3044cac0, .data) — the vehicle
 * tread/wheel dust-effect handle table indexed by surface-material class.
 * CG_Vehicle_DoControllers (0x30020540) computes a material class 1..6 from the
 * traced surface flags, adds VEH_TREAD_EFFECT_JEEP_OFFSET for a jeep-type vehicle
 * (cg[+0x88]==1) to select the
 * jeepwheel_* row instead of the tanktread_* row, and reads
 * cgs_media_vehicleTreadEffects[class] as the CG_PLAY_EFFECT_ORIENTED (0xe8) handle.
 * Slots 1..6 are tank grass/sand/dirt/rock/snow/snow-alt; slots 7..12 are the
 * corresponding jeep-wheel variants. Slot 0 is the "no effect" default. The two
 * snow-alt slots intentionally register the same assets as their snow siblings,
 * matching the original machine code. Supersedes
 * the mechanical g_data_bg_animparseanimscript_3044cac0 (owner label was
 * first-touching, not the identity); this is the [0] cell of that array. */
extern qhandle_t cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_COUNT];
enum {
    CG_COMPLAINT_STATUS_COUNT = 4
};

/* 0x3044caf4/0x3044caf8: complaint-vote HUD state. CG_ServerCommand stores the
 * complained-about client number or one of four negative status codes and an
 * absolute display deadline (cg_time+20000, shortened to +10000 for a status
 * code); CG_MapRestart resets them to -1/0. */
extern int32_t cg_complaintClientNum;
extern int32_t cg_complaintEndTime;
/* 0x3044cafc: startup random value in [-1,1), computed from the MSVC rand result
 * as rand()/32768*2-1. Its downstream semantic role is not yet resolved. */
extern float cg_initialRandomValue;
/* 0x3044cb00 .data: base of the per-corpse client animation/info table
 * (0x4d0-stride clientInfo_t, indexed by corpse entity number - 0x40).
 * Superseded from the mechanical g_data_bg_calculateweaponangles_3044cb00 scalar
 * (wrong first-touch owner label). Declared as cg_corpseInfo in
 * client_recovered.h after this header is included; the shared clientInfo_t
 * declaration is already available from client_info_types.h. Storage is in
 * globals.c. */
/* 0x3044f18c .data — cg_drawFPS_vmCvar.integer: the detail level of the renderer-stats
 * HUD (a cvar-backed int). CG_DrawFPS (0x30018090) reads it and, only when it is
 * > 1, fetches cg_rendererStats and prints the detailed tris/verts/prims/ents/mb/dc
 * lines; the fps line alone is drawn regardless. Consumed identically by an
 * adjacent unreconstructed reader at 0x3001874f (`MOV EAX,[0x3044f18c]`). The
 * mechanical owner=cg_drawweapreticle is a rejected size-guess name, and the
 * earlier note claiming this dword was cg_localAnimTrees[7]+0x18 was wrong: this
 * address is 0x21c8 (8648) bytes past that 8-entry array's base, far outside it.
 * Exact original cvar name unresolved; named by proven role. */

/* 0x3044f2ac: cg_thirdPerson.integer. The cvar-table entry at 0x30085710 binds
 * vmCvar 0x3044f2a0 to "cg_thirdPerson" (0x300790ec). */

/* 0x3044f3cc .data refs=1: brass-ejection master enable flag. The sole reader,
 * CG_EjectWeaponBrass (0x30047be0), reads it as a dword and skips all shell-casing
 * effect play when it is 0 (an early-out gate, like a cg_brass cvar mirror).
 * Provisional role name; exact CoD cvar/global name unproven. */

/* 0x3044f60c .data — cg_currentSelectedPlayer_vmCvar.integer: the current-iteration index of the HUD
 * trap-54 emit family. Every member (script precacheHeadIcon at 0x30030f10, the
 * iterator-driven emitters at 0x30031020 and 0x300311f0) reads it, range-clamps
 * it to [0, cg_hudEmitCount) resetting to 0 when out of range (writing back to
 * this same slot), and uses it to index cg_hudEmitClientTable[]. Signed compare
 * (JL) so treated as int32. Supersedes the mechanical owner=script_func_precacheheadicon
 * label. Exact source name unresolved; named by proven cursor role. */
/* 0x3044f60c */
/* 0x3044f644 .data refs=1 width=imm first=0x30005c8b owner=script_method_hudelem_destroy */
/* 0x3044f96c .data refs=3 width=4 first=0x30022b7e owner=cmd_callvote_f */
/* 0x3044fa8c: cg_drawCrosshair_vmCvar.integer (vmCvar_t at 0x3044fa80,
 * cg_cvarTable[16] "cg_drawCrosshair"). Read as the enable gate by three
 * HUD/3D-view draw entry points: CG_DrawWeaponIcon3D (0x30019ba3, skips when
 * zero), CG_DrawCrosshair (0x30019ea5), and CG_DrawCrosshairNames (0x3001a610,
 * skips when signed-negative). The mechanical owner=pm_viewheighttablelerp
 * label was only the first toucher; not a standalone global. */

/* cg_hudObjectiveMinHeight_vmCvar.value (0x3044fba8, .data) — the lower height/elevation
 * threshold CG_DrawObjectivePointers (0x3002fe70, the only consumer, refs=1) compares
 * the objective's world-Z delta (dz = objOrigin.z - viewOrigin.z) against to pick the
 * "below" objective-icon variant (direction index 2) when dz is small/negative. Type
 * repaired uint32_t->float (FCOMP consumer). The owner=clientendframe label is the
 * mechanical first-toucher and is superseded by this consumer's proven role; exact CoD
 * cvar/source name unproven so the role name is provisional. */

/* 0x3044fcc8 .data refs=8 width=4 first=0x30016905 owner=cg_asset_parse.
 * Type repaired uint32_t->float. CG_DrawObjectivePointers (0x3002fe70) reads it as the
 * objective-pointer MIN ALPHA — the marker alpha at/after the far fade distance. */
/* 0x3044fdec .data refs=3 width=4 first=0x30032c2c owner=g_isvehicleusable */
/* bg_swingSpeed_vmCvar.value (0x3044ff08, .data) — per-frame swing speed passed as the
 * stepScale argument of BG_SwingAngles for the legs and torso
 * yaw axes in BG_PlayerAngles (0x30004550); all five refs to this address are in that
 * function. Read as a float. The writer that sets it each frame (a frametime-derived
 * swing rate in canonical BG_PlayerAngles) is not yet reconstructed, so both the exact
 * source name and the non-zero runtime value are provisional; the .data initializer is 0.
 * Type superseded from the mechanical uint32 scalar (owner=bg_getminspreadforweapon was
 * the exporter's wrong first-toucher). */

/* 0x30450020: cl_serverloadmap vmCvar_t. cg_cvarTable[177] binds this object to
 * "cl_serverloadmap"; loading-screen paths read .string at +0x10. */
extern vmCvar_t cl_serverloadmap;
/* 0x30450148 .data — timescale_vmCvar.value: a float read (FLD float ptr) only by
 * CG_EmitPlayerFlameChunks's terminal-chunk path (0x30025419), where it is the
 * divisor of 1000.0 (Q_rint(1000.0/timescale_vmCvar.value) % 360) that seeds the
 * terminal chunk's field_98 spin angle. RESOLVED (role) on consume; exact CoD
 * source name unresolved. Retyped from the mechanical uint32_t to float. */

/* 0x3045014c .data refs=2 width=4 first=0x3002de90. Read as a boolean/enum enable
 * gate: CG_DrawScoreboard (0x30037d90) returns qfalse when it is zero, and the
 * scoreboard-column setup at 0x3002de90 compares it against 1 to decide layout.
 * Gates whether the scoreboard is drawn at all. Named by its proven scoreboard-enable
 * role; the exact source symbol is not resolved, so the address suffix is retained. */
/* View-weapon up-axis offset (classic cg_gun_z role), consumed as a float. */
/* 0x30450388 .data — cg_hudDamageIconOffset_vmCvar.value: the top-edge Y offset (in virtual 640x480
 * units) of the damage-direction arrow icon quad, relative to the crosshair anchor.
 * Read only by CG_DrawDamageDirectionIndicators (0x30450388): it forms the near edge
 * (corner y = this) and, with cg_hudDamageIconHeight_vmCvar.value, the far edge (y = this +
 * height). Zero-initialized in .data; the value-setting writer is not in the
 * reconstructed set, so this is a proven-by-role name (retyped uint32_t->float).
 * Mechanical owner=cg_ejectweaponbrass rejected (not brass; a HUD icon layout). */
/* 0x304504a0 .data refs=1 width=imm first=0x30035a6e owner=pmovesingle */
extern vmCvar_t pmove_msec;
/* 0x304505cc .data refs=1 width=4. cg_noVoiceText_vmCvar.integer: cached .integer of the
 * cg_noVoiceText cvar; nonzero suppresses CG_PlayVoiceChat printing the chat text.
 * (owner label item_enableshowviacvar is the mechanical first-touch and is wrong.) */

/* 0x304506e8 .data — moving-tracer length for tracer mode A (selected when
 * localEntity_t.leFlags == LEF_TRACER_MODE_A). A float, read (never written) by
 * CG_AddMovingTracer (0x3002ab00) — FLD'd and used as the VectorMA distance the
 * tracer head is marched along its normalized velocity — and by the sibling
 * builders 0x300482e6 / 0x30048a2c. Runtime-tunable, no observed writer in this
 * DLL (cvar-like init outside .text). Type repaired uint32_t->float; mechanical
 * owner=pm_switchifempty size-guess dropped. Exact original name unresolved;
 * address suffix kept as disambiguator. */

/* 0x30450928 .data — cg_hudDamageIconHeight_vmCvar.value: the height (in virtual 640x480 units)
 * of the damage-direction arrow icon quad. Read only by CG_DrawDamageDirectionIndicators
 * (0x3001aa43), which sets the far corner y = cg_hudDamageIconOffset_vmCvar.value + this. Zero-init in
 * .data; value-setting writer not reconstructed, so a proven-by-role name (retyped
 * uint32_t->float). Mechanical owner=cg_ejectweaponbrass rejected. */

/* 0x30450a4c (.data): a cgame screen-draw gate flag, read-only in .text (no writer;
 * engine/config-set). CG_Draw2D (0x3001bfe0) reads it three times inside
 * the spectator/dead draw paths to decide whether to run the scoreboard/menu
 * overlay steps (Menu_PaintAll etc.), and the HUD element drawer at 0x300320e0
 * gates its whole body on it. Nonzero => "draw the scoreboard/overlay". Provisional
 * role name kept address-shaped (exact source name unresolved); the mechanical
 * owner=fire_lead label was a size-collision mislabel and is corrected here. */

/* 0x30450b6c .data — cg_drawSnapshot_vmCvar.integer: the cvar-backed enable flag for the
 * snapshot-timing debug HUD line ("time:%i snap:%i cmd:%i"). The debug-overlay
 * dispatcher CG_DrawDebugOverlays (0x30018730) reads it and, only when it is
 * non-zero, calls CG_DrawSnapshot (0x30018020) to draw that line. Sole reference
 * in the image (refs=1, first=0x30018731). The mechanical owner=pm_weapon_
 * finishrechamber label was a size-collision mislabel (this dword is never touched
 * by any weapon-rechamber code); corrected here. Exact original cvar name inferred
 * from the paired CG_DrawSnapshot behavior; modeled as an int gate. */

/* 0x30450dac .data refs=3 width=4. cg_voiceSpriteTime_vmCvar.integer: the voice-chat icon
 * display duration CG_PlayVoiceChat (0x30039ff0) adds to cg.time (added once for the
 * default icon, doubled otherwise). Engine/externally initialized; exact CoD source
 * symbol unresolved so the address suffix is retained. (owner label
 * item_enableshowviacvar is the mechanical first-touch and is wrong.) */

/* cg_hudCompassSpringyPointers_vmCvar.integer (0x30450fec, .data) — first-run flag for the compass
 * reference-yaw spring in CG_UpdateCompassOrientation (0x3001d6d0). While 0, the updater snaps
 * cg_compassRefYaw straight to cg_refdefViewAngles[1] and returns without integrating; the
 * flag is otherwise only tested (never written) inside this function, so the initial
 * snap depends on an external one-shot set. refs=1, owned solely by 0x3001d6d0; the
 * owner=veh_checkpushclients label is a wrong size-match name. Boolean int. */
/* 0x30451108 .data refs=1 width=4 first=0x30044ce6 owner=setclientviewangle */
/* 0x30451348 .data refs=1 width=4 first=0x30044ed1 owner=setclientviewangle */
/* 0x3045146c .data — cg_railTrailTime_vmCvar.integer: the rail-trail segment lifetime in
 * milliseconds. Consumed only by the rail-trail segment builder (0x300430a0):
 * it gates drawing (skip when <= 0), sets le->endTime = cg.time + this, and
 * sets le->lifeRate = 1.0f / this (the fade rate). Read as a signed int in all
 * three refs; no writer exists anywhere in this DLL, so its value is provided by
 * the engine/loader or a cvar bind outside this module (BSS, default 0).
 * Real name resolved by consumer behavior; supersedes the mechanical
 * owner=pmove guess (this function is the rail-trail builder, not Pmove). */

/* 0x304516ac (.data/.bss): boolean enable gate for a fixed-position debug/HUD
 * element drawn by CG_DrawFixedFadeElement (0x3001b0f0): that function early-outs
 * (TEST EAX,EAX / JZ) when this is zero, otherwise issues the trap-0x1d draw at
 * screen (135,425). Referenced by exactly one site (0x3001b0f1, this read) and
 * never written anywhere in this DLL, so its value is engine/loader-provided (a
 * cached cvar/visibility flag, sibling to developer_vmCvar.integer used by the adjacent
 * debug-draw cluster). Exact source name unresolved; named by proven boolean-gate
 * role. The mechanical owner=sp_script_origin label is a size-match artifact
 * (rejected). Retyped uint32_t -> int32_t: tested as a plain boolean int. */

/* 0x304517c8 .data refs=8 width=4 first=0x300169a3 owner=cg_asset_parse.
 * Type repaired uint32_t->float. CG_DrawObjectivePointers (0x3002fe70) reads it as the
 * objective-pointer FAR distance — at/beyond it the marker alpha is clamped to
 * cg_hudObjectiveMinAlpha_vmCvar.value; between near and far the alpha is
 * interpolated. */

/* 0x304518ec .data — cg_hudStanceHintPrints_vmCvar.integer: read-only gate in CG_DrawPlayerStance;
 * nonzero enables change detection, zero forces cg_stanceHintChangeTime=-1. No writer
 * in exported code; role name, exact source name unresolved. Old owner label wrong. */

/* 0x30451b2c .data — cl_languagewarningsaserrors_vmCvar.integer: when nonzero, a failed string
 * translation is a fatal BG_AnimParseError(7, ...) rather than a Com_Printf warning. Read
 * (never written) by all four cgame translate helpers — CG_GetTranslatedVoiceChat-
 * String (0x3003a174), CG_SafeTranslateString_Internal (0x3002d707), FUN_300310b0 (0x30031108),
 * FUN_300435d0 (0x30044773) — inside the branch already gated by
 * cl_languagewarnings_vmCvar.integer. It is a cvar-integer snapshot initialized elsewhere;
 * the exact cvar name is unresolved, so this is a provisional role name. */

/* 0x30451c48 .data — cg_hudDamageIconWidth_vmCvar.value: the full width (in virtual 640x480 units)
 * of the damage-direction arrow icon quad. Read only by CG_DrawDamageDirectionIndicators
 * (0x3001aa07), which halves it (* 0.5) to place the left/right corners at +/-w/2
 * about the crosshair anchor. Zero-init in .data; value-setting writer not
 * reconstructed, so a proven-by-role name (retyped uint32_t->float). Mechanical
 * owner=cg_ejectweaponbrass rejected. */

/* 0x30451e8c .data refs=2 width=4 first=0x3001c401: cg_skybox_vmCvar.integer — the boolean
 * that decides whether the 3D scene submitted to trap_R_RenderScene draws the world
 * model. CG_DrawActive (0x3001c3fb) always OR's bit 0x10 into cg_refdef.rdflags, then if
 * this flag is zero clears it again (AND ~0x10) — i.e. bit 0x10 stays set (no-world /
 * RDF_NOWORLDMODEL-style) only when cg_skybox_vmCvar.integer == 0. Also read by the refdef
 * builder at 0x30041e53. Role name from the proven consumer; owner=g_getnonpvsfriendlyinfo
 * was a wrong size-match. */

/* 0x304520cc (.data): the cgame "screen is active / draw enabled" gate, read-only
 * in .text (no writer; engine/config-set). CG_Draw2D (0x3001bfe0)
 * requires it to be nonzero to proceed past its entry guards (JZ to the tail when
 * clear), i.e. it must be set before any per-frame HUD/scene drawing runs.
 * Provisional role name kept address-shaped (exact source name unresolved); the
 * mechanical owner=fire_lead label was a size-collision mislabel and is corrected
 * here. */
/* View-weapon forward-axis offset (classic cg_gun_x role), consumed as a float. */

/* 0x30452300: cg_objectiveText vmCvar_t. cg_cvarTable[122] binds this object to
 * the name "cg_objectiveText"; CG_DrawObjectiveInfo reads its string
 * field at +0x10 (0x30452310). */
extern vmCvar_t cg_objectiveText;
/* 0x30452428: cg_crosshairAlpha_vmCvar.value (vmCvar_t at 0x30452420,
 * cg_cvarTable[42] "cg_crosshairAlpha"). Read across the 0x30019xxx HUD/3D-view
 * cluster: CG_DrawWeaponIcon3D (0x30019c21) FLDs it and skips the icon draw when
 * it is < 0.01f (FCOMP against 0.01f, JNP-on-less), and CG_DrawCrosshair
 * (0x30019e8a/0x3001a22f) FMULs it into both reticle alphas. The mechanical
 * owner=pm_viewheighttablelerp label was only the first toucher; not a
 * standalone global. */

/* 0x30452548 .data refs=10 width=4 first=0x30016a2c owner=cg_asset_parse.
 * Type repaired uint32_t->float. CG_DrawObjectivePointers (0x3002fe70) reads it as the
 * objective-pointer MAX-SIZE distance: at/below it the marker size scale is clamped to
 * cg_hudCompassMinRadius_vmCvar.value; between it and the near distance the size scales
 * toward 1.0. */

/* cg_debugposition_vmCvar.integer (0x3045266c, .data) — developer debug toggle. Its sole
 * consumer, CG_ResetPlayerEntity (0x30034880), gates a Com_PrintMessage of
 * "%i ResetPlayerEntity yaw=%i\n" on this dword being nonzero. Read as a boolean
 * flag (CMP against 0). Exact source cvar symbol unproven; named by proven role.
 * Supersedes the mechanical g_data_cg_interpolateentityposition_3045266c (whose
 * owner label was the exporter's wrong size-guess first-toucher). */
/* View-weapon draw mode: 0 hides, 2 forces, 1 hides behind an active ADS overlay. */
/* 0x30452aec .data — cl_languagewarnings_vmCvar.integer: when zero, a failed string
 * translation is silently returned as a plain copy of the input; when nonzero, the
 * translate helpers emit a "Could not translate ..." warning/error and return an
 * "^1UNLOCALIZED(^7...^1)^7" decorated placeholder. Read (never written) by all
 * four cgame translate helpers — CG_GetTranslatedVoiceChatString (0x3003a167),
 * CG_SafeTranslateString_Internal (0x3002d6fa), FUN_300310b0 (0x300310fb), FUN_300435d0
 * (0x3004476a). Cvar-integer snapshot initialized elsewhere; exact cvar name
 * unresolved, so this is a provisional role name. */

/* 0x30452c08 .data — cg_centertime_vmCvar.value: the center-print fade-out
 * duration in SECONDS. CG_DrawCenterString (0x300191b0) reads it as a float
 * (FLD @0x300191d4), multiplies by 1000.0 and rounds (Q_rint) to milliseconds,
 * then passes that as the totalMsec argument to CG_FadeColor(cg_centerPrintTime,
 * ms). The mechanical export mislabeled this as uint32_t and tagged it to a
 * size-guessed owner; it is a float. Only reader is CG_DrawCenterString; its
 * writer (a cvar-float snapshot) is not among reconstructed code, so the exact
 * cvar name is unresolved and the address suffix is kept. */

/* 0x30452d2c .data: cg_chatHeight_vmCvar.integer — integer value of the cg_chatHeight_vmCvar.integer
 * cvar (visible team-chat lines). CG_AddToTeamChat (0x30039390) clamps it to
 * [1..TEAMCHAT_HEIGHT] and uses it as the ring modulus; <=0 flushes the ring. */

/* 0x30452e40: vmCvar_t registered by cg_cvarTable[64] from the name string
 * "cg_debugProneCheckDepthCheck" (0x30079248). BG_CheckProneValid reads its
 * .integer field at 0x30452e4c for every diagnostic draw. */
extern vmCvar_t cg_debugProneCheckDepthCheck;
/* 0x30452f6c .data refs=1 width=4 first=0x3001b070. Signed-int developer/debug-draw
 * gate consumed by CG_DrawDebugFadeElement (0x3001b070): draw only when >0, or when
 * ==0 with the developer cvar set; never when negative. Never written in this DLL
 * (engine/loader-provided). Exact cvar name unresolved (refs=1, no writer/string);
 * role-named, suffix kept. owner=colorbytes3 first-touch label rejected. Retyped to
 * int32_t (the sole access is a signed compare). */

/* 0x304531a0 .data: cg_items[] — the cgame per-item registered-visuals cache,
 * itemInfo_t, stride 0x24 (36 bytes), indexed by the item index (1..0x85; the
 * fixed compare bound is 0x86 = 134). Resolved from two producers/consumers, all
 * addressing the SAME 0x24-stride entry (base + itemIndex*9 dwords):
 *   - CG_RegisterItemVisuals (0x30044ac0) LAZILY fills one entry: it null-tests
 *     +0x00 (registered flag), then writes +0x04 = worldModel handle
 *     (CG_RegisterModel(7,..) 0x3003d940), +0x08 = iconModel handle
 *     (CG_RegisterModel(6,..), same callee), +0x0c = icon shader (trap id 5,
 *     0x3003db80), +0x1c = pickup-sound (cgame_syscall(0xc3,
 *     bg_itemlist[i].pickupSound)) and copies it to +0x20, and finally sets
 *     +0x00 = 1. (+0x08 was previously abiGap_000; the second model handle is now
 *     proven by that function's reconstruction.)
 *   - CG_Item (0x3001e680) reads +0x00 (registered? else register), then +0x04 =
 *     model handle for the RT_MODEL render entity, and passes it into the anim-tree
 *     refresh + trap_R_AddRefEntityToScene.
 *   - CG_EntityEvent (0x30022810) plays the cached pickup/foley sound at +0x1c
 *     (event 0x94) / +0x20 (event 0x96) for the firing item.
 * The mechanical owner=sp_func_bobbing / "weapon-event record" label was a first-
 * touch/size-guess artifact and is REJECTED (this is cg_items, not a bobbing/spawn
 * table). +0x1c/+0x20 are the registered-sound value the engine returns from trap
 * 0xc3; CG_PlayEntitySoundAliasByName consumes them as const char *, so they are modeled as
 * such. Fields between the proven offsets stay reserved. refs=8. */
typedef struct itemInfo_s {
    int32_t registered; /* +0x00 : nonzero once CG_RegisterItemVisuals ran */
    int32_t modelHandle; /* +0x04 : gitem_t.worldModel handle, CG_RegisterModel(7,..) */
    int32_t iconModelHandle; /* +0x08 : gitem_t.iconModel handle, CG_RegisterModel(6,..).
                                     *          Proven a distinct second handle by
                                     *          CG_RegisterItemVisuals (0x30044ac0), which stores
                                     *          both model handles into +0x04 and +0x08. */
    int32_t iconShader; /* +0x0c : registered icon/HUD shader handle */
    uint8_t auxiliaryAssets[12]; /* ABI_AUDITED_OPAQUE: registered item assets not consumed here. */
    const char *pickupSound; /* +0x1c : registered pickup/foley sound (trap 0xc3);
                                     *          CG_EntityEvent plays this for event 0x94 */
    const char *pickupSoundAlt; /* +0x20 : copy of +0x1c; CG_EntityEvent plays it
                                     *          for event 0x96 */
} itemInfo_t;
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(itemInfo_t) == 0x24, "itemInfo_t stride");
_Static_assert(offsetof(itemInfo_t, modelHandle) == 0x04, "itemInfo_t modelHandle");
_Static_assert(offsetof(itemInfo_t, iconModelHandle) == 0x08, "itemInfo_t iconModelHandle");
_Static_assert(offsetof(itemInfo_t, iconShader) == 0x0c, "itemInfo_t iconShader");
_Static_assert(offsetof(itemInfo_t, pickupSound) == 0x1c, "itemInfo_t pickupSound");
_Static_assert(offsetof(itemInfo_t, pickupSoundAlt) == 0x20, "itemInfo_t pickupSoundAlt");
#endif
/* Storage is 256 itemInfo_t (0x100), proven by CG_RegisterGraphics (0x3002ba50), whose
 * load-time zero clears REP STOSD 0x900 dwords = 0x2400 bytes = 256 * sizeof(itemInfo_t)
 * (0x24), ending exactly before the next global at 0x304555a8. Only indices 1..133 are
 * ever populated (bg_numItems == 0x86 == 134, the compare bound in CG_RegisterItems); the
 * remaining slots stay zero. The declared element count is the STORAGE size (256), not the
 * used item count. */
extern itemInfo_t cg_items[256];
/* 0x304555a8 .data refs=1 width=4 first=0x30044f14 owner=setclientviewangle */

/* 0x304556c8 .data — cg_tracerSpeed: the LE_MOVING_TRACER travel speed (world
 * units per second) of the line-tracer local entity built by CG_SpawnTracerLine
 * (0x30048260, its only reader — all four refs at 0x300483f9/0x30048422/0x3004842f/
 * 0x3004843c). Used two ways there: le->pos.trDelta = cg_tracerSpeed * dir (the
 * TR_LINEAR velocity vector), and le->endTime = trTime + (int)(span*1000/speed)
 * (encoded as span * -1000 / speed via _ftol2), i.e. the ms at which the tracer
 * reaches the far endpoint. Never written in this DLL -> cvar-like init outside
 * .text. Type repaired uint32_t->float; the mechanical owner=menus_open label was a
 * first-toucher/size-guess artifact (0x30048260 is a tracer builder, not a menu
 * opener). Exact source cvar name unresolved; role name kept. */
/* 0x30455908 .data refs=2 — cg_tracerchancelmg_vmCvar.value: per-shot tracer spawn probability
 * [0..1] for the "mode A" ammo types (LMG/HMG/UMG). CG_SpawnTracer
 * (0x30048d60) overrides its default chance with this when
 * bg_weaponInfos[weapon]->ammoType is 3/4/5. Read (never written) as a float; type
 * repaired uint32_t->float. owner=cg_drawplayerstance was only its first toucher.
 * Exact CoD source name unproven; role name from CG_SpawnTracer. */
/* 0x30455fc8: cg_crosshairAlphaMin_vmCvar.value (vmCvar_t at 0x30455fc0,
 * cg_cvarTable[43] "cg_crosshairAlphaMin"). CG_DrawCrosshair (0x3001a239) clamps
 * the side-reticle alpha up to it. The old owner=veh_playercollision label was
 * that consumer's size-guess mislabel; not a standalone global. */
/* 0x30456208 .data refs=3 — cg_tracerchance_vmCvar.value: the default per-shot tracer spawn
 * probability [0..1]. CG_SpawnTracer (0x30048d60) gates itself on cg_tracerchance_vmCvar.value > 0.0
 * and uses it as the default spawn chance (rand()/32768 < chance draws the tracer).
 * Read (never written) as a float; type repaired uint32_t->float.
 * owner=cg_drawplayerstance was only its first toucher. Exact CoD source name unproven;
 * role name from CG_SpawnTracer. */

/* 0x3045632c: cg_norender_vmCvar.integer.integer. Proven by the cvar-table entry at
 * 0x30085880 (vmCvar 0x30456320, name 0x30078f60 "cg_norender_vmCvar.integer"). It trims
 * prediction input in CG_PredictPlayerState_Internal and suppresses scene work
 * in CG_DrawActiveFrame. */

/* cg_hudObjectiveMaxHeight_vmCvar.value (0x30456448, .data) — the upper height/elevation
 * threshold CG_DrawObjectivePointers (0x3002fe70, the only consumer, refs=1) compares
 * the objective's world-Z delta (dz) against to pick the "above" objective-icon variant
 * (direction index 1) when dz exceeds it. Type repaired uint32_t->float. The
 * owner=clientendframe mechanical label is superseded by this consumer's proven role;
 * exact CoD cvar/source name unproven so the role name is provisional. */

/* 0x30456568 .data refs=6 width=4 first=0x30016a70 owner=cg_asset_parse.
 * Type repaired uint32_t->float. CG_DrawObjectivePointers (0x3002fe70) reads it as the
 * objective-pointer size scale at the max-size distance (cg_hudCompassMinRange_vmCvar.value). */

/* 0x304567ac (.data): cg_letterbox_vmCvar.integer - a boolean gate that, when nonzero,
 * shrinks the vertical view size to 85% of the horizontal size, producing a
 * letterbox/cinematic crop. Read (only, refs=1) by CG_CalcVrect (0x3003f510):
 * if set, height uses Q_rint(viewsize * 0.85) instead of viewsize. Its writer is
 * not in the reconstructed set, so this is a proven-by-role name; exact source
 * identity (a cvar mirror or a cinematic-mode flag) unresolved. Retyped
 * uint32_t -> qboolean (TESTed as a truth value). owner=bg_animscriptevent is the
 * size-guess first-touch artifact from CG_CalcVrect (rejected). */
/* 0x304568cc .data refs=2 width=4 first=0x3002b350 owner=trap_syscall_2 */
/* 0x304569e8 .data refs=1 width=4 first=0x3003f7a9 owner=scr_vehicle_damagescale */
/* 0x30456b0c .data refs=2 width=4; read at 0x3001b360 (CG_DrawInfoScreens
 * dispatcher) and 0x3001acd1 (inside its overlay-A target 0x3001acc0). A cvar
 * integer enable-flag: when nonzero, the per-frame info dispatcher tail-jumps to
 * the overlay-A developer screen (0x3001acc0). Never written in .text (the cvar
 * system owns the store). Mechanical owner label "info_setvalueforkey" is the
 * exporter's first-toucher guess and is wrong. Exact cvar name unresolved. */
/* 0x30456d48 .data refs=1 width=4 first=0x30044ec1 owner=setclientviewangle */

/* cg_debugevents_vmCvar.integer (0x30456f8c, .data) -- cached integer of the "cg_debugevents"
 * cvar (string at .rdata 0x30079224). Read purely as a boolean gate
 * (MOV EAX,[0x30456f8c]; TEST EAX,EAX) by the entity-event dispatchers to print
 * per-event debug traces: CG_EntityEvent (0x30022810, 3 reads) and
 * CG_EntityPreEvent (0x30023690, 1 read) emit their "CG_Entity(Pre)Event:*" trace
 * strings only when this is nonzero. Loaded as a plain dword (not dereferenced as a
 * cvar_t*), so it is the cvar's cached .integer value. Supersedes the mechanical
 * owner=cmd_callvote_f first-touch label. */

/* 0x304570a8 .data refs=2 width=4 first=0x3001c35f: cg_stereoSeparation_vmCvar.value — the eye
 * separation distance (float) applied when rendering a stereo frame. CG_DrawActive
 * (0x3001c120) FLDs it and multiplies by -0.5f (left eye, stereoView 1) or +0.5f
 * (right eye, stereoView 2) to offset cg_refdef.vieworg along viewaxis[1] before
 * trap_R_RenderScene. Retyped float role name; owner=g_getnonpvsfriendlyinfo was a
 * wrong size-match (first toucher is CG_DrawActive). */

/* 0x304571cc .data refs=1 width=4; read only at 0x3001b36e (CG_DrawInfoScreens
 * dispatcher). A cvar integer enable-flag: when nonzero (and overlay-A off), the
 * dispatcher tail-jumps to the overlay-B script-VM debug screen (0x30017e90,
 * "threads: %d"/"vars: %d"). Never written in .text. Mechanical owner label
 * "cmd_fogswitch_f" is a size-match/first-toucher artifact and is wrong. Exact
 * cvar name unresolved. */
/* 0x3045764c: cg_crosshairDynamic_vmCvar.integer (vmCvar_t at 0x30457640,
 * cg_cvarTable[44] "cg_crosshairDynamic"). CG_DrawCrosshair (0x3001a0c0) keeps
 * the projected impact-point crosshair shift only when it is set; otherwise the
 * reticle is re-centered (x=0) at the ADS vertical offset. The old
 * owner=veh_playercollision label was that consumer's size-guess mislabel. */

/* 0x3045776c .data — cg_hudDamageIconInScope_vmCvar.integer: a gate consulted by
 * CG_DrawDamageDirectionIndicators (0x3001a9bf) only when the ADS/scope overlay is active. If
 * the aim-down-sight overlay is up (CG_CalcAdsOverlayFrac != 0) but this is zero, the
 * function bails without drawing; otherwise it projects the aim point to screen
 * (CG_ProjectDamageDirToScreen) to anchor the arrows there. Non-ADS frames ignore it
 * and anchor at screen center (320,240). Read-only in .text (writer not reconstructed:
 * plausibly a registered scope-anchor shader handle or an enable flag); proven-by-role
 * name. Mechanical owner=cg_ejectweaponbrass rejected. */

/* 0x3045788c cg_drawCrosshairNames_vmCvar.integer (.data refs=1 width=4 first=0x3001a620). No
 * .text writer in this DLL — set externally (cvar-integer style, like
 * cg_drawCrosshair_vmCvar.integer). Read once, purely as an enable gate, by the crosshair
 * player-name HUD drawer CG_DrawCrosshairNames (0x3001a620: MOV EAX,[..];
 * TEST EAX; JZ exit) — the name is only drawn when this is nonzero. This is the
 * classic cg_drawCrosshairNames_vmCvar.integer cvar->integer. The mechanical
 * owner=cg_drawplayerweaponnameback label (a size-match guess) is rejected; named
 * by proven draw-enable role, address suffix retained since the exact cvar is not
 * resolved. */

/* 0x304579a0: vmCvar_t registered by cg_cvarTable[63] from the name string
 * "cg_debugProneCheck" (0x30079268). Its .integer at 0x304579ac gates prone
 * diagnostic drawing and is also read by the surrounding stance logic. */
extern vmCvar_t cg_debugProneCheck;
/* 0x30457be8 .data refs=1 width=4 first=0x30044e1f owner=setclientviewangle */
/* 0x30457d0c .data refs=4 width=4 first=0x300228a6 owner=cmd_callvote_f */
/* 0x30457f48 .data refs=1 width=4 first=0x30044e11 owner=setclientviewangle */
/* 0x3045818c .data refs=1 width=4. cg_noVoiceChats_vmCvar.integer: cached .integer of the
 * cg_noVoiceChats cvar; nonzero makes CG_PlayVoiceChat (0x30039ff0) skip the voice sound
 * and talking-icon stamp. (owner label item_enableshowviacvar is the mechanical
 * first-touch and is wrong.) */

/* 0x304582ac RESOLVED: not a standalone scalar — it is
 * cg_vehicletrails_vmCvar.integer (+0xc of the vmCvar object at 0x304582a0,
 * cg_cvarTable entry 183 "cg_vehicletrails" default "1"). The gate
 * CG_Vehicle_DoControllers (0x30020540) tests at 0x30021004 (MOV
 * EAX,[0x304582ac]; TEST; JZ skip) is that cvar mirror; the provisional
 * cg_vehicleTreadMarksEnabled name that used to live here was a mis-model. */
/* 0x304583c8 (.data/.bss): a global float scale applied to fade/overlay color
 * alpha. Read via FLD DWORD at 11 sites and multiplied into color/alpha
 * components (CG_FadeColor 0x3001d200 does color[3]*=this; the drawer at
 * 0x3001c640 does color[3]/[esp+n]*=this). Never written anywhere in this DLL,
 * so its value is engine/loader-provided. Exact source name unresolved; named
 * by proven role. The mechanical owner=bytetodir label is a first-touch
 * artifact (rejected). Retyped uint32_t -> float: every access is a 4-byte
 * float load. */
/* 0x3045860c .data refs=4 width=4 first=0x30018fe5 owner=cmd_veh_fireturret */

/* 0x3045884c .data refs=1 width=4: an enable gate read at the top of the guarded
 * CG_LocalSound_f command handler (0x3003ac00): MOV EAX,[0x3045884c] / TEST EAX,EAX
 * / JZ <return>, so a zero value makes that handler a no-op. This is the ONLY
 * reference in the DLL (refs=1) and no writer is present in this binary, so the
 * exact source identity is unresolved; named provisionally by its proven role as a
 * boolean gate. The mechanical owner=menuparse_forecolor label is a size-guess
 * first-touch artifact (rejected — the touching function is the local-sound command
 * handler, not a menu forecolor parser). Retyped uint32_t -> qboolean: it is TESTed
 * as a truth value. */

/* 0x30458960: cl_serverloadgametype vmCvar_t. cg_cvarTable[178] binds this object
 * to "cl_serverloadgametype"; loading-screen paths read .string at +0x10. */
extern vmCvar_t cl_serverloadgametype;
/* 0x30458bac .data refs=4 width=4 first=0x30018ff3 owner=cmd_veh_fireturret */

/* 0x30458ccc .data refs=1: cg_dumpAnims_vmCvar.integer — a special entity/client index
 * whose DObj CG_AddPacketEntities (0x3001f810) finalizes once per frame after the
 * main entity loops. It reads the index, requires 0 <= index < 0x400 (MAX_GENTITIES)
 * and the draw-inhibit gate cl_paused_vmCvar.integer == 0, then fetches its DObj handle via
 * CG_DOBJ_GET_HANDLE(index) and, when nonzero, commits it via CG_DOBJ_DISPLAY_ANIM. No
 * .text writer in the exported set (engine/init-owned). owner=finishspawningitem was
 * the first-touching function's size-guess name; exact CoD source symbol unresolved,
 * so the address suffix is retained. Provisional role name. */
/* 0x30458dec .data refs=1 width=4 first=0x30018bc0 owner=cmd_veh_fireturret */
/* 0x30458f08 .data: cg_errorDecay's float value in milliseconds. Both the
 * prediction updater and CG_CalcViewValues compute
 * (errorDecay - elapsed) / errorDecay from it. */
/* 0x30458f0c .data refs=1 width=4 first=0x30035c34 owner=pmovesingle */

/* 0x3045902c .data refs=1 width=4; read only at 0x3001b37c (CG_DrawInfoScreens
 * dispatcher). A cvar integer enable-flag: when nonzero (and overlays A and B
 * off), the dispatcher tail-jumps to CG_DrawViewInfoOverlay (0x3001b2b0). Never
 * written in .text. Mechanical owner label "cmd_fogswitch_f" is a
 * size-match/first-toucher artifact and is wrong. Exact cvar name unresolved. */

/* 0x30459140: cg.clientFrame. CG_DrawActiveFrame is its only writer (one INC per
 * rendered client frame) and optionally prints it with the literal
 * "cg.clientFrame:%i\n". Flame records store this value as a last-touched frame
 * stamp and compare against clientFrame-1 to recognize current/recent emitters. */
extern int32_t cg_clientFrame;
/* 0x30459144 (.data): cg.clientNum, installed from CG_Init's clientNum argument.
 * Consumed as an index into
 * bgs.clientinfo[] (IMUL 0x4d0; ADD 0x305e1f34) at 0x30022477, and compared
 * against cg_snap->ps.psClientNum by the trap-54 HUD digit emitter at 0x30031321 and by
 * 0x30037498 to decide whether the drawn element belongs to the local player (use
 * the live snapshot value) or a cached per-client value. Retyped uint32_t->int32_t
 * (used as a signed client index/compare). The role is fully resolved; the
 * mechanical owner=scr_vehicle_think label (a first-toucher) is superseded. */
extern int32_t cg_clientNum;
/* cg_demoPlayback (0x30459148): installed from the demoPlayback argument of the
 * active-frame driver at 0x30042160, then used to suppress live usercmd prediction
 * and player-state transition work in the vehicle DObj pass, entity-event pass,
 * predictor, and snapshot transition. This is the standard cgame demoPlayback
 * gate; the mechanical owner=bg_animparseanimscript was only first-touching. */
extern qboolean cg_demoPlayback;
/* 0x3045914c (.data): a cgame screen-draw suppress flag (refs=1, read only by
 * CG_Draw2D 0x3001bfe0 as its very first entry guard; nonzero => draw
 * nothing this frame and return immediately). No .text writer (engine/config-set).
 * Sits one dword below cg_lockedViewFace (0x30459150), the sibling render-mode gate
 * checked right after it. Provisional role name kept address-shaped (exact source
 * name unresolved); the mechanical owner=fire_lead label was a size-collision
 * mislabel and is corrected here. */
extern uint32_t g_cgScreenSuppressFlag;
/* 0x30459150 .data refs=5 width=4 — cg_lockedViewFace: locked/cube-face render
 * selector. 0 = normal view; 1..6 pick one of six axis-aligned camera
 * orientations. The refdef dispatcher (0x30041a30) branches into the locked-view
 * builder (0x30040360) when != 0, which uses (value-1) as a 6-entry jump-table
 * index. Named by proven role; exact source field name unresolved. */
extern int32_t cg_lockedViewFace;
/* 0x30459154 .data refs=3 width=4 — cg_lockedViewSize: locked-view square
 * viewport base size (int). Builder (0x30040360) sets refdef width==height==size+2
 * and square fov = atan2(size+2,size). owner=cg_spawntracer was a size-collision
 * mislabel. */
extern int32_t cg_lockedViewSize;
/* 0x30459158 (.data): cg.latestSnapshotNum — the most recent snapshot number
 * cgame has received. Written by CG_ProcessSnapshots (0x3003d30a), right after it
 * references the "CG_ProcessSnapshots: n < cg.latestSnapshotNum" error string
 * (0x3007a454), which names this datum directly. Printed as the "snap:%i" field
 * of CG_DrawSnapshot (0x30018020). Retyped from the mechanical uint32_t; the
 * (owner=scr_getobjectfield) label was wrong. */
extern int32_t cg_latestSnapshotNum;
/* 0x3045915c (.data): cg_latestSnapshotServerTime — the engine server time (ms) of
 * the most recent snapshot. Written by CG_ProcessSnapshots (0x3003d2d0) as the second
 * out-parameter of cgame_syscall(CG_GET_CURRENT_SNAPSHOT_NUMBER, &n, &this), then
 * mirrored into cg_effectAnimTime (0x3053a034). Consumed as a snapshot-time base for
 * elapsed deltas from cg.time (lagometer ping ring at 0x30018a10; 0x300421e5).
 * Superseded from the size-guessed mechanical g_data_script_func_playfxontag_3045915c
 * (that owner was a wrong size-match to a server GSC builtin; this is a cgame client
 * datum). refs=3, width=4. */
extern int32_t cg_latestSnapshotServerTime;
/* 0x30459160 .data refs=137: the current client snapshot pointer, cg_snap.
 * CG_InstallSnapshotResetEffects (0x3003c9d0) stores the installed snapshot here;
 * the snapshot-transition loop (0x3003d2d0) reads snap->snapFlags (+0x00) and
 * snap->serverTime (+0x08) through it, and the paired next-snapshot pointer sits
 * at 0x30459164. Retyped from the mechanical uint32_t/void* to snapshot_t *. */
extern snapshot_t *cg_snap;
/* 0x30459164 (.data): cg_nextSnap — the next/incoming client snapshot pointer,
 * paired with cg_snap at 0x30459160. CG_BuildSolidList (0x30035030) reads it
 * (MOV EDI,[0x30459164]) and walks snap->entities[0..numEntities) to rebuild the
 * per-frame mark lists. Retyped from the mechanical uint32_t (the owner=cg_asset_parse
 * label was the first-touching function, not the identity). refs=32; provisional
 * name by role (paired next-snapshot pointer). */
extern snapshot_t *cg_nextSnap;
/* 0x30459168 (.data): cg_activeSnapshots[0] — the first of the two double-buffered
 * snapshot storage slots that CG_ReadNextSnapshot (0x3003d220) fills via
 * trap_GetSnapshot and installs as cg_snap. CG_ReadNextSnapshot picks the slot NOT
 * currently pointed at by cg_snap: `dest = (cg_snap == &cg_activeSnapshots[0]) ?
 * &cg_activeSnapshots[1] : &cg_activeSnapshots[0]`. The two slots are 0x15020 apart
 * in .data (cg_activeSnapshots[1] at 0x3046e188), larger than the modelled
 * snapshot_t prefix, so they are declared as two separate objects rather than a
 * strided array. Retyped from the mechanical uint32_t (owner=keywordhash_key was the
 * first-touching function, not the identity). */
extern snapshot_t cg_activeSnapshots0;
/* 0x3046e188 (.data): cg_activeSnapshots[1] — the second double-buffered snapshot
 * storage slot (paired with cg_activeSnapshots0 at 0x30459168); see that entry.
 * Retyped from the mechanical uint32_t (owner=keywordhash_key was the first-touching
 * function, not the identity). Kept next to its pair rather than in strict address
 * order because the two form one logical cg.activeSnapshots[2]. */
extern snapshot_t cg_activeSnapshots1;
/* 0x304831a8 (.data): cg.frameInterpolation — the [0,1) fraction between the
 * current snapshot (cg_snap) and the next snapshot (cg_nextSnap), used to
 * interpolate entity/view state each frame. Written by CG_SetFrameInterpolation
 * (0x3001f710) as (cg.time - cg_snap->serverTime) / (cg_nextSnap->serverTime -
 * cg_snap->serverTime), clamped to 0 when the span is 0 or the result is negative.
 * Read as a float lerp weight by ~16 consumers (e.g. 0x3001ea35/0x3001eae9/
 * 0x3001ebac push it as the `frac` arg to the interpolation helper 0x3004bd00).
 * Retyped from the mechanical uint32_t to float; the owner=fire_artillery label
 * was the first-touching function, not the identity. */
extern float cg_frameInterpolation;
/* 0x304831ac .data — cg.frametime: the elapsed time (ms) of the current client
 * frame, cg.time minus the previous frame's cg.time. Sits between cg.frameInterpolation
 * (0x304831a8) and cg.time (0x304831b0), matching the stock Quake3/CoD cg_t field
 * order. Consumed across ~38 sites as an int->float per-frame delta (FILD) for
 * animation/decay rates, and subtracted as a raw ms count (e.g. the stat-bar hold
 * timer in CG_DrawStatBarWithDecay at 0x3002fb83). Retyped from the mechanical
 * uint32_t; the (owner=finishspawningitem) label was the first-touching function,
 * not the identity. Provisional-but-well-supported name. */
extern int32_t cg_frametime;
/* 0x304831b0 .data: the cgame time base (cg.time), the current client game time
 * in ms. Read as the "now" value for elapsed-time math across ~146 sites and
 * written by the CG frame/init cluster. */
extern uint32_t cg_time;
/* 0x304831b4 .data refs=7 width=4 first=0x30035b1c owner=pmovesingle */
extern int32_t cg_physicsTime;
/* 0x304831b8 .data: cg.latestSnapshotTime — the newest installed snapshot's
 * server time. Snapshot-installer FUN_30035940
 * copies cg_nextSnap->serverTime (snapshot_t +0x08 at 0x30459164) into it, and
 * CG_ClipMoveToEntities (0x300350d0) reads it as the `int time` argument to
 * BG_EvaluateTrajectory when evaluating each solid entity's pos/apos trajectories.
 * refs=6; the (owner=cg_registerimpacteffects) mechanical label was a size-guess,
 * not the identity. Retyped from the mechanical uint32_t to signed int32_t (it is
 * a server-time value fed to BG_EvaluateTrajectory's signed `time` parameter). */
extern int32_t cg_latestSnapshotTime;
/* 0x304831bc .data refs=4 width=4 first=0x30034d4a owner=cg_addscalefade */
extern qboolean cg_initialSnapshotPending;
/* 0x304831c0 .data — cg_thirdPerson. Written only as 0/1 by the frame builder;
 * CG_CalcViewValues selects CG_OffsetThirdPersonView when it is set, while the
 * view-weapon and first-person effect/tag consumers all suppress their work in
 * that state. The old pm_viewheighttablelerp owner was a size-match artifact. */
extern qboolean cg_thirdPerson;
/* 0x304831c4..0x304876c7 .data.
 * RESOLVED: cg.predictedPlayerState, the full 0x4504-byte playerState_s block.
 * Its fields are modeled in playerState_t; do not add standalone globals inside
 * this address span. owner=veh_playercollision was only the first toucher. */
extern playerState_t cg_predictedPlayerState;
/* 0x304876c8..0x30487950 .data: the complete predicted local-player centity.
 * CG_AddPacketEntities (0x3001f810) projects cg.predictedPlayerState directly into
 * nextState at +0xf4 (0x304877bc), then copies that 0xf4-byte entityState_t into
 * currentState at +0x00. It subsequently passes the base to
 * CG_CalcEntityLerpPositions and CG_AddCEntity, which read and write centity
 * members through +0x220. CG_PlayVoiceChat writes voiceChatIcon/voiceChatTime at
 * +0x230/+0x234. These ranges are subobjects of one 0x288-byte centity_t,
 * not separately allocated globals. */
extern centity_t cg_predictedEventEntity;
/* 0x30487950 .data refs=7 width=4 first=0x30034e01.
 * RESOLVED: cg_prevAdsFraction — the previous frame's ADS (aim-down-sight) zoom
 * fraction. CG_TrackAdsZoomDirection (0x30045480) compares this frame's
 * cg_predictedPlayerState.adsFraction (0x304832a4) against this to set cg_adsZoomingIn,
 * then copies the current fraction here for the next frame. Read/written as a
 * single float scalar (FLD/FCOMP at 0x300454d0/0x300454e5/0x300454f4/0x3004550f;
 * dword copy-store at 0x3004552b/0x3004553d). The mechanical owner=cg_addscalefade
 * was only the first toucher: CG_AddScaleFade (0x30034d40) STOSD-zero-fills a
 * 12-dword scratch block that merely starts at this address (resets it to 0.0f).
 * Retyped from the mislabeled uint32 placeholder to float. */
extern float cg_prevAdsFraction;
/* 0x30487954 .data refs=5 width=4 first=0x30019560.
 * RESOLVED: cg_adsZoomingIn — qboolean set each frame by the ADS-fraction tracker
 * at 0x30045509: 1 when this frame's adsFraction rose above last frame's
 * (cg_prevAdsFraction, 0x30487950), else 0. CG_CalcAdsOverlayFrac (0x30019520)
 * reads it to pick adsZoomInFrac (zooming in) vs adsZoomOutFrac (zooming out).
 * owner=sp_trigger_mount_no_brush was a wrong size-based auto-name. */
extern qboolean cg_adsZoomingIn;
/* 0x30487958..0x30487963 .data vec3_t: weapon-movement view-angle offset computed
 * and applied for the current frame. Written as one vector store by CG_ApplyWeaponMovementAngles
 * (0x30045070); supersedes the mechanical uint32 triple that mislabeled these
 * three consecutive floats (wrong owner=com_dprintf). Sole writer; role-proven
 * provisional name. */
extern vec3_t cg_weaponMovementAngles;
/* Persistent scalar preserved around the BG weapon-position angle calculation. */
extern float cg_weaponPositionMoveScale;
/* 0x30487968..0x30487973: persistent weapon-movement angular offset, smoothed
 * toward the stance/speed-derived target by CG_CalcWeaponMovementAngles. */
extern vec3_t cg_weaponMoveAngles;
/* Persistent angle triple preserved around the BG weapon-position calculation. */
extern vec3_t cg_weaponPositionPrevAngles;
/* 0x30487980 .data refs=28 width=4 first=0x3001952c.
 * RESOLVED: cg_currentWeaponInfo — pointer to the current predicted weapon's
 * weaponInfo_t record, cached once per predicted frame at 0x30034d79 as
 * bg_weaponInfos[cg.predictedPlayerState.currentWeapon]. Widely read across the
 * cgame view/HUD code (28 refs) via +offset field access (e.g. adsZoomInFrac
 * +0x278, adsOverlayShader +0x280 in CG_CalcAdsOverlayFrac, 0x30019520).
 * owner=sp_trigger_mount_no_brush was a wrong size-based auto-name. */
extern weaponInfo_t *cg_currentWeaponInfo;
/* 0x30487984..0x30487993: prediction-error decay state. The prediction updater
 * stores the error timestamp/vector; CG_CalcViewValues adds
 * cg_predictedError * decayFraction to the camera and clears the timestamp when
 * the fraction leaves (0,1). */
extern int32_t cg_predictedErrorTime;
extern vec3_t cg_predictedError;
/* 0x30487994 .data: cg_predictedEventSequence -- monotonically incrementing
 * counter of predicted local-player events. CG_CheckPlayerstateEvents (0x30034ec0)
 * increments it once per fired predicted event. Immediately followed by
 * cg_predictedEvents[16] at 0x30487998. Supersedes the mechanical
 * owner=sp_script_vehicle (a wrong size-guess owner). */
extern int32_t cg_predictedEventSequence;
/* MAX_PREDICTED_EVENTS: size of the cg_predictedEvents[] ring; the index mask
 * (i & 0xf) at 0x30034f23 proves 16 entries (a power of two). */
enum {
    MAX_PREDICTED_EVENTS = 16
};
/* 0x30487998..0x304879d8 .data: cg_predictedEvents[MAX_PREDICTED_EVENTS] -- ring
 * of recently fired predicted event ids, written by CG_CheckPlayerstateEvents
 * (0x30034f26) at index (i & (MAX_PREDICTED_EVENTS-1)). Supersedes the mechanical
 * single-dword owner=sp_script_vehicle; the array spans exactly to the next
 * mechanical entry at 0x304879d8. */
extern int32_t cg_predictedEvents[MAX_PREDICTED_EVENTS];
/* 0x304879d8 .data: a float envelope value CG_EntityEvent (0x30022810) maintains
 * for the weapon-changed / low-ammo warning event (0x91). Read and written via
 * FLD/FSTP (proven float shape; retyped from the mechanical uint32_t). Exact CoD
 * name unresolved — kept address-suffixed. owner=cmd_callvote_f is a first-touch
 * artifact (this function is CG_EntityEvent, not Cmd_CallVote_f). refs=7. */
extern float cg_weaponChangeViewOffset;
/* 0x304879dc .data: the cg.time stamp paired with cg_weaponChangeViewOffset;
 * CG_EntityEvent stores cg.time here when it updates that envelope. Exact CoD name
 * unresolved. owner=cmd_callvote_f first-touch artifact. refs=5. */
extern int32_t cg_weaponChangeViewOffsetTime;
/* 0x304879e0 .data: cg_impactViewKick — signed vertical view/weapon impulse set
 * by CG_EntityEvent for local bullet-flesh/body-hit events. CG_OffsetFirstPersonView
 * applies its 150 ms rise / 300 ms fall envelope directly to vieworg.z, while
 * CG_CalcViewLeanKickOffset applies the same envelope at quarter scale. */
extern float cg_impactViewKick;
/* 0x304879e4 .data: cg_impactViewKickTime — cg.time stamp paired with
 * cg_impactViewKick and used by both view/weapon kick consumers. */
extern int32_t cg_impactViewKickTime;
/* 0x304879e8..0x30487a77 (.data): cg_dobjPreviewOrientations[3] — three per-frame
 * scratch orientations that CG_AddPacketEntities (0x3001f810) rebuilds from cg_time
 * before the entity add pass. Each 48-byte record holds an Euler `angles` vector
 * whose yaw spins at a distinct rate ((cg_time & mask) * 360 / divisor, with masks
 * 0xfff/0x7ff/0x3ff and divisors 4095/2048/1024, pitch=roll=0), the `forward` and
 * `up` basis vectors produced by AngleVectors from those angles, and the NEGATED
 * `right` vector (AngleVectors' `right` output is taken into a stack temp, then
 * stored as `fld 0.0f; fsub right`). The output register mapping is confirmed against
 * AngleVectors' body (0x3004a200): ESI=forward, EDI=right, EBX=up. All twelve floats
 * of each record are written each frame; nothing in reconstructed code reads them
 * back, so this is a write-only per-frame debug/preview axis set handed to the engine
 * implicitly. This supersedes the mechanical g_data_finishspawningitem_* dwords
 * (0x304879e8..0x30487a6c), which captured only the individually-xref'd words of one
 * contiguous scratch block; the owner=finishspawningitem label was the first-touching
 * function's size-guess name. Exact CoD source symbol unresolved (address suffix
 * retained on the record fields' provisional roles). Members are vec3_t so they decay
 * to float* for AngleVectors. */
typedef struct cgDObjPreviewOrientation_s {
    vec3_t angles; /* +0x00: {pitch=0, spinYaw, roll=0} input to AngleVectors */
    vec3_t forward; /* +0x0c: AngleVectors `forward` output (ESI at the call) */
    vec3_t negRight; /* +0x18: -(AngleVectors `right`), stored via fld 0; fsub right */
    vec3_t up; /* +0x24: AngleVectors `up` output (EBX at the call) */
} cgDObjPreviewOrientation_t;
enum {
    CG_DOBJ_PREVIEW_ORIENTATION_COUNT = 3
};
extern cgDObjPreviewOrientation_t cg_dobjPreviewOrientations[CG_DOBJ_PREVIEW_ORIENTATION_COUNT]; /* 0x304879e8 */
_Static_assert(sizeof(cgDObjPreviewOrientation_t) == 0x30, "cgDObjPreviewOrientation_t is 48 bytes (4 vec3_t)");
_Static_assert(offsetof(cgDObjPreviewOrientation_t, negRight) == 0x18, "preview orientation negRight at record +0x18");
/* 0x30487a78..0x30487a84 (.data): the cropped 3D-view rectangle (cg.refdef view
 * region), four contiguous ints x/y/width/height in real screen pixels. Named by
 * their CG_TileClear consumer (0x3001d160): it skips border drawing when
 * (x==0 && y==0 && width==vidWidth && height==vidHeight) — the full-screen case —
 * and otherwise tiles the four letterbox edges around this rect. The same block is
 * read via FILD/FIADD across the 0x30019xxx view-scaling cluster. Exact cg.refdef
 * field binding inferred from this consumer; owner=stuckinclient was the mechanical
 * first-toucher and is wrong. */
extern refdef_t cg_refdef;
/* 0x30487a88 / 0x30487a8c .data — cached view-projection scale pair, two floats
 * produced together by the screen-projection helper at 0x300402b0 (which follows
 * CG_CalcFov): it FSTPs a fov/aspect-derived scale to 0x30487a88 and the paired
 * fov-degrees angle term to 0x30487a8c. Consumers (the 0x30019xxx view/impact
 * cluster, e.g. 0x30019448/0x3001a29f/0x30019fbd) FLD/FDIV them as floats to map
 * world coordinates into the letterboxed 3D view. Retyped from the mislabeled
 * uint32_t placeholders; owner=cg_parseimpacteffects was only the first toucher.
 * Exact original cg field names not fully proved (likely a projection-scale pair);
 * float width and producer/consumer roles are proven. */
/*
 * 0x30487a90 .data — cg_refdef.vieworg: the current client view/camera origin as
 * a 3-component float vector (0x30487a90 .x, 0x30487a94 .y, 0x30487a98 .z). The
 * mechanical owner=bg_indexforstring label is just the first-touching function
 * and is wrong. Proven vec3: it is written as three consecutive dwords copied
 * from a stack vec3 (0x3003fd39 MOV [0x30487a90],EDX / [0x30487a94],EAX /
 * [0x30487a98],ECX) and read as a vec3 distance base by the camera-shake
 * evaluator (0x3001b390 passes &this vector to VectorDistance) and accumulated
 * onto as a vec3 by the shake aggregate (0x3001b669 FLD/FADD/FSTP over
 * .90/.94/.98). Exact original cg field name (likely cg.refdef.vieworg) not
 * fully proved; the vec3 shape and role are. Supersedes the three prior
 * uint32_t g_data_bg_indexforstring_30487a{90,94,98} placeholders (one vec3,
 * not three ints). refs=79/60/75 across the three components.
 */
/*
 * 0x30487a9c .data — cg_refdef.viewaxis[0]: the current view/aim forward direction
 * as a 3-component float vector (.x@0x30487a9c, .y@0x30487aa0, .z@0x30487aa4),
 * stored immediately after cg_refdef.vieworg (0x30487a90). Proven vec3: the setup
 * paths at 0x300403e1.. write it as pure unit axis vectors (e.g. (0,0,1), (1,0,0),
 * (-1,0,0)), and the consumers FLD all three consecutive dwords in order
 * (CG_ScanForCrosshairEntity 0x3001a4d3/508/525 forms cg_refdef.vieworg + 8192*this
 * as the crosshair trace endpoint; the tracer/fire paths at 0x30026c88/0x30034c45
 * read the same triple). The mechanical owner=cg_parseimpacteffects label is only
 * the first-touching function. Exact CoD field name (likely cg.refdef.viewaxis[0])
 * not fully proved; the vec3 shape and forward-direction role are. Supersedes the
 * three prior uint32_t g_data_cg_parseimpacteffects_30487a{9c,a0,a4} placeholders. */
/* 0x30487aa8..0x30487ab3 .data — cg_refdef.viewaxis[1]: second row of
 * cg.refdef.viewaxis (Q3 viewaxis[1]), a vec3 immediately after
 * cg_refdef.viewaxis[0] (viewaxis[0]). Written as an axis-aligned unit row by the
 * locked/cube-face refdef builder (0x30040360); consumers FLD the three
 * consecutive dwords. Supersedes the three uint32_t
 * g_data_cg_parseimpacteffects_30487a{a8,ac,b0} placeholders. */
/* 0x30487ab4..0x30487abf .data — cg_refdef.viewaxis[2]: third row of
 * cg.refdef.viewaxis (Q3 viewaxis[2]), a vec3 after cg_refdef.viewaxis[1]. Written as
 * an axis-aligned unit row by the locked/cube-face refdef builder (0x30040360).
 * Supersedes the three uint32_t g_data_cg_parseimpacteffects_30487a{b4,b8,bc}
 * placeholders. */
/* 0x30487ac0 .data refs=2 width=4 first=0x300420e7 owner=script_method_player_cloneplayer */
/* 0x30487ac4 .data refs=11 width=4 first=0x3001c3fb: cg_refdef.rdflags — the rdflags
 * bitfield of the cg.refdef block (base 0x30487a78, this is base+0x4c) submitted to
 * trap_R_RenderScene (CG_R_RENDER_SCENE). CG_DrawActive (0x3001c3fb..0x3001c418)
 * OR's bit 0x10 in and conditionally clears it per cg_skybox_vmCvar.integer; the refdef
 * builders at 0x300402eb/0x30042096/0x300420c9 OR/AND other view-mode bits (0x8, and
 * a mask ~0x10). Provisional flag constants named below. Role name from the proven
 * refdef consumers; owner=g_getnonpvsfriendlyinfo was a wrong size-match. */
/* cg_refdef.rdflags uses the shared renderer_refdef_flags_e domain from
 * q_renderer_types.h. The cgame writers use RDF_SKYBOX_PORTAL (0x08),
 * RDF_SKYBOX_PORTAL_ACTIVE (0x10), and RDF_DRAW_SKYBOX (0x20). */
/* 0x30487ac8..0x30487ad3 .data — cg_refdefViewAngles: the contiguous pitch/yaw/roll
 * Euler vector immediately after cg_refdef.rdflags in the cg.refdef block. Identity as
 * one vec3 is proven by CG_CalcVehicleViewPos (0x30040f1c), which passes the base
 * 0x30487ac8 in EDX to AnglesToAxisNegRight. The three components are also updated
 * together by CG_CalcViewShake (0x3001b6ba..0x3001b716) and consumed as view/effect
 * angles throughout the renderer/HUD cluster. Supersedes the three independent
 * cg_effectSpinAngle* scalar aliases and the original mechanical placeholders. */
extern vec3_t cg_refdefViewAngles;
/* Previous view angles retained by the sway integrator as one vec3. */
extern vec3_t cg_weaponSwayViewAngles;
/* Current weapon-sway Euler triple used as the BG calculation reference angles. */
extern vec3_t cg_weaponSwayAngles;
/* Persistent weapon-sway positional output filled as one vec3. */
extern vec3_t cg_weaponSwayOffset;

enum {
    CG_DOBJINFO_LOW_COUNT = 64
};

/*
 * cg_dObjInfoKeys[] / cg_dObjInfoHandles[] — the parallel "DObj info" registration
 * table, indexed by a table index (register-argument in every accessor). Each slot
 * caches the identity/type key (cg_dObjInfoKeys) and the engine XModel pointer
 * returned by CG_DOBJ_WRAP_MODEL (cg_dObjInfoHandles) for that index. Proven a
 * 1023-entry table by the address layout (keys at 0x30487af8, handles at 0x30488af4;
 * 0x30488af4 - 0x30487af8 = 4092 bytes = 1023 dwords, with handles immediately
 * followed by the next datum at 0x30489af0) and by
 * the free helpers CG_FreeRegisteredHandlesLow (frees 0..63) /
 * CG_FreeRegisteredHandlesHigh (frees 64..1022), which cover exactly indices 0..1022.
 *
 * The mechanical export captured only slot 0 of each array as a scalar
 * (g_data_pm_clearaimdownsightflag_30487af8/..._30488af4, wrong owner: the first
 * writer that touched the datum); superseded here with the proven array shape.
 * Consumers/accessors (all index by ESI*4): CG_SetDObjInfo (0x30016400, store
 * key+handle), CG_CheckDObjInfoMatches (0x30016410, compare key+handle -> qboolean),
 * the three free helpers above, and CG_UpdateEntityDObjModel (0x30021ea0). Table role
 * proven; the "DObjInfo" name follows the same-module cgame PPC cluster
 * (CG_SetDObjInfo / CG_CheckDObjInfoMatches / CG_Free*DObjInfo). */
extern uint32_t cg_dObjInfoKeys[ENTITYNUM_NONE];
extern XModel *cg_dObjInfoHandles[ENTITYNUM_NONE];
/* 0x30489af0 .data refs=2 width=4 first=0x30040358 — a float produced by the
 * screen-projection helper at 0x300402b0 as (cg_viewProjScaleA / cg_fov_vmCvar.value),
 * i.e. the aspect scale normalized by the base FOV. Read back as a float and
 * clamped/compared at 0x300426be (FUCOMP vs 0x3048bfec, conditional FMUL).
 * Retyped from the mislabeled uint32_t placeholder; owner=veh_setupcollmap was a
 * wrong size-match. Exact original cg field name unproven; float role proven. */
extern float cg_zoomSensitivity;
/* 0x30489af4 .data — cg_loadingScratch[1024]: the shared scratch string buffer used
 * to build the "LOADING... %s" status text during asset registration. CG_RegisterGraphics
 * (0x3002ba50) and its media-batch siblings do Q_strncpyz(cg_loadingScratch, section, 0x3ff)
 * then Com_PrintMessage(va("LOADING... %s\n", cg_loadingScratch)) + trap_UpdateScreen. The
 * separate byte 0x30489ef3 (== cg_loadingScratch[0x3ff]) is the trailing NUL forced to 0
 * before each copy; it is element [0x3ff] of this array, not a distinct global. Size 0x400
 * is proven by the 0x3ff cap passed to Q_strncpyz and the [0x3ff]=0 terminator write.
 * (owner=pm_updateviewangles was a size-guess and is rejected.) */
extern char cg_loadingScratch[MAX_STRING_CHARS];
/* 0x30489ef4 .data refs=3 width=4 — cg_vehicleViewReset: a qboolean latch set to 1 by
 * CG_SnapshotTransitionStage2 (0x30034d40) on a snapshot reset, telling
 * CG_CalcVehicleViewValues (0x30040580) to reseed cg_vehicleViewPrevAxis from the current
 * tag orientation this frame instead of composing against the previous frame; the view
 * builder clears it back to 0 after reseeding. owner=cg_addscalefade was a mechanical
 * first-toucher. Provisional role name; exact CoD field name unresolved. */
extern int32_t cg_vehicleViewReset;
/* 0x30489ef8..0x30489f1b .data — cg_vehicleViewPrevAxis: the previous frame's vehicle/turret
 * tag orientation basis (axis_t, 3x3). CG_CalcVehicleViewValues (0x30040580) composes it with
 * the current tag orientation to smooth the derived view angles, then transposes the current
 * orientation back into it for the next frame. owner=convertquattomat was a mechanical
 * first-toucher (this address is the routine's only consumer). */
extern axis_t cg_vehicleViewPrevAxis;
/* 0x30489f1c .data refs=2 width=4 first=0x30037dd7; resolved as cgs.scoreboardTime,
 * the cg_time of the last "score" info request. CG_DrawScoreboard (0x30037d90) and
 * the sibling score-request path (0x30017330) both throttle the request: when
 * cg_time - cgs_scoreboardTime > 2000 they latch cg_time here and issue
 * cgame_syscall(CG_SEND_CLIENT_COMMAND, "score"). Signed cg_time value. (Exact
 * original symbol not proven; named by proven role.) */
extern int32_t cgs_scoreboardTime;
/*
 * 0x30489f20 .data refs=9 width=4 (mechanical owner label turret_think_client is
 * the exporter's wrong size-match guess; this is scoreboard state, see below).
 *
 * cg_scoreboardNumClients — number of client rows collected into the scoreboard
 * this frame. The scoreboard-build routine at 0x30038000 queries the connected
 * client count and clamps it to MAX_CLIENTS (0x30038082: `cmp eax,0x40` /
 * `jle`; > 64 is forced to 64 at 0x3003808c). CG_ScoreboardHeight (0x30036e50)
 * reads it as a SIGNED row-count loop bound (JLE/JL), so it is int32_t.
 */
extern int32_t cg_scoreboardNumClients;
/*
 * 0x30489f24..0x30489f33 .data — cg_scoreboardTeamScores[TEAM_COUNT]: the per-team scores
 * sent in server-command args 2 and 3 and drawn in the totals row's score
 * column, indexed by team_t. Proven a 4-element int array: CG_ParseScores stores
 * the two command arguments team-indexed
 * (0x300380a1 MOV [0x30489f24],EAX and the team loop), CG_DrawScoreboardBody
 * reads elements 1 and 2 directly to pick the leading team (0x30037c0c
 * MOV EAX,[0x30489f28]; 0x30037c11 MOV ECX,[0x30489f2c]; CMP), and
 * CG_DrawScoreboardTeamHeader (0x30037090) reads cg_scoreboardTeamScores[team]
 * (0x3003732d MOV EAX,[EBX*4 + 0x30489f24]) when the column value-selector is 1.
 * Supersedes the mechanical per-element split (0x30489f28 as cg_entitypreevent,
 * 0x30489f2c as cg_entitypreevent, 0x30489f30 as g_touchtriggers) — one array,
 * not four unrelated scalars. Exact source symbol unproven; named by proven role.
 */
extern int32_t cg_scoreboardTeamScores[TEAM_COUNT];
/*
 * 0x30489f34..0x30489f43 .data — cg_scoreboardTeamPings[TEAM_COUNT]: the sum, then
 * integer average, of each team's client pings. Proven a 4-element int array:
 * the build routine accumulates row +0x08 (ping) team-indexed
 * (0x300382a2 ADD [EAX*4 + 0x30489f34],ECX;
 * 0x30038121 MOV [0x30489f34],EAX) and CG_DrawScoreboardTeamHeader (0x30037090)
 * reads cg_scoreboardTeamPings[team] (0x30037336 MOV EAX,[EBX*4 + 0x30489f34])
 * when the column value-selector is 3, formatting it into the totals row's ping
 * column. Supersedes the mechanical per-element split (0x30489f38/0x30489f3c/
 * 0x30489f40 as g_touchtriggers) — one array, not four unrelated scalars. Exact
 * source symbol unproven; named by proven role (parallel to cg_scoreboardTeamCount).
 */
extern int32_t cg_scoreboardTeamPings[TEAM_COUNT];
/*
 * 0x30489f44..0x30489f53 .data — cg_scoreboardTeamCount[TEAM_COUNT]: the number
 * of scoreboard rows belonging to each team, indexed by team_t (FREE=0, AXIS=1,
 * ALLIES=2, SPECTATOR=3). Proven as a 4-element int array by the scoreboard
 * build loop at 0x30038295 (`inc dword ptr [0x30489f44 + eax*4]`, eax = the
 * row's team_t), the parallel per-team ping array cg_scoreboardTeamPings is at
 * 0x30489f34. CG_ScoreboardHeight (0x30036e50) reads the four elements as flags
 * (nonzero => that team's section is present). Mechanical owner label
 * turret_think_client is the exporter's wrong size-match guess.
 */
extern int32_t cg_scoreboardTeamCount[TEAM_COUNT];
/*
 * cgScore_t — one collected scoreboard row (element of cg_scoreboardEntries[]
 * below). The build routine at 0x30038060 zeroes 0x180 dwords (0x600 bytes) from
 * 0x30489f54 (`mov ecx,0x180 / rep stosd`), then fills up to
 * cg_scoreboardNumClients rows at stride 0x18 (0x300382b7 `add esi,0x18`) — a
 * 24-byte element. The server producer at 0x20021864..0x20021882 formats each
 * row as clientNum, score, ping, deaths, statusIcon; the parser stores those at
 * +0x00/+0x04/+0x08/+0x0c/+0x14 and derives team at +0x10.
 */
typedef struct cgScore_s {
    int32_t client; /* +0x00: clientNum (0..63); indexes
                                      *        bgs.clientinfo. 0x30037850 MOV
                                      *        EAX,[ESI] / IMUL 0x4d0. */
    int32_t score; /* +0x04: reported client score */
    int32_t ping; /* +0x08: ping; averaged per team */
    int32_t deaths; /* +0x0c: reported deaths */
    int32_t team; /* +0x10: team_t of this row; compared to the
                                      *        requested section (0x30037866). */
    qhandle_t statusIcon; /* +0x14: status index or registered shader */
} cgScore_t;

/*
 * cg_scoreboardEntries[] — the per-frame collected scoreboard rows, backed at
 * 0x30489f54. The build routine at 0x30038060 `rep stosd` zeroes 0x180 dwords
 * (0x600 bytes) from here, then fills up to cg_scoreboardNumClients rows at
 * stride 0x18 (element cgScore_t, defined just above). 0x600 == 64*0x18, and
 * 0x30489f54+0x600 == 0x3048a554 (cg_scoreboardShowing), so the array is exactly
 * MAX_CLIENTS (64) rows and captures the entire span the single mechanical
 * item_correctedtextrect dword mislabelled. CG_DrawScoreboard_ScoresList
 * (0x30037810) walks it filtering by entry->team and the client's live
 * bgs.clientinfo. Supersedes the mechanical g_data_item_correctedtextrect
 * fragment (owner label was the exporter's wrong size-match guess). */
extern cgScore_t cg_scoreboardEntries[64];
_Static_assert(sizeof(cgScore_t) == 0x18, "cgScore_t stride 0x18 (imul/add esi)");
_Static_assert(offsetof(cgScore_t, client) == 0x00, "cgScore_t.client at +0x00");
_Static_assert(offsetof(cgScore_t, team) == 0x10, "cgScore_t.team at +0x10");
_Static_assert(offsetof(cgScore_t, statusIcon) == 0x14, "cgScore_t.statusIcon at +0x14");
/* 0x3048a554 .data refs=7 width=4 first=0x3001af19; resolved as the "scoreboard
 * showing" state flag. Set to qtrue by the scoreboard-show path (0x3001bd20,
 * which also records cg.time into cg_scoreboardShowTime and inits the scoreboard),
 * cleared on map restart (0x30039500). CG_KeyEvent (0x30032780) gates all
 * scoreboard scroll input on it; various HUD/draw routines branch on it. qboolean.
 * (Exact original symbol not proven; named by proven role.) */
extern qboolean cg_scoreboardShowing;
/* 0x3048a55c .data refs=5 width=4 first=0x3001af46; resolved as cg.scoreboardShowTime,
 * the cg_time latched when cg_scoreboardShowing (0x3048a554) toggles. The
 * show/toggle paths (0x3001bd20, 0x30039660) store cg_time here on a state change;
 * CG_DrawScoreboard (0x30037d90) feeds it to CG_FadeColor as startMsec to fade the
 * scoreboard over a 100 ms window. Signed cg_time value. (Exact original symbol not
 * proven; named by proven role.) */
extern int32_t cg_scoreboardShowTime;
/* 0x3048a560 .data refs=13 width=4 first=0x300327c3; resolved as the scoreboard
 * scroll position (top line offset). CG_KeyEvent (0x30032780) decrements it by
 * cg_scoreboardScrollStep_vmCvar.integer on scroll-up (clamped to >= 0); the scroll-down helper
 * (0x30037e60) increments it (clamped to line-count-1). Signed int. */
extern int32_t cg_scoreboardScrollPos;
/* 0x3048a564 .data refs=6 width=4 first=0x300370a2 — cg_scoreboardOverflowed: the
 * "scoreboard content has flowed past the bottom of the visible area" latch.
 * CG_DrawScoreboardBody clears it at frame start; the per-line drawers
 * CG_DrawScoreboardTeamHeader (0x300370a2 reads it; 0x300370f0 sets it to 1 when
 * y+24 > 432.0) and CG_DrawClientScore (0x30037420 reads it) early-out once it is
 * set. CG_ScrollScoreboardDown (0x30037e60) gates its scroll on it. qboolean.
 * Mechanical owner=g_damage was the exporter's wrong size-match guess. */
extern qboolean cg_scoreboardOverflowed;
/* 0x3048a568 .data refs=6 first=0x3002251d; the scoreboard "fragged by" name
 * buffer, a char[0x20]. The mechanical export mis-captured it as a lone byte flag
 * (cg_scoreboardFadedOut / and the trailing terminator as a second symbol at
 * 0x3048a587); machine code proves one buffer:
 *   - CG_Obituary (0x30022270, 0x3002251d) fills it by copying 0x1f bytes from
 *     the 32-byte attacker-name view, then zeroes the byte at +0x1f
 *     (0x3048a587) as the fixed terminator, i.e. a 32-byte string;
 *   - CG_DrawObituaryLine (0x30031a90) tests cg_fraggedByName[0] as the
 *     "has a target name" gate (MOV AL,[0x3048a568]; TEST AL,AL) and passes the
 *     buffer as the %s of va("Fragged by %s", cg_fraggedByName);
 *   - CG_DrawScoreboard (0x30037d90) stores 0 into cg_fraggedByName[0] when
 *     CG_FadeColor returns NULL (scoreboard faded out), emptying the string so the
 *     obituary line stops drawing.
 * Buffer size proven by the CRT strncpy count 0x1f plus the explicit +0x1f
 * terminator zero. (Exact original symbol name not proven; named by proven role.
 * Supersedes the mis-captured cg_scoreboardFadedOut byte and the aliased
 * 0x3048a587 terminator symbol.) */
extern char cg_fraggedByName[32];
extern uint8_t cg_unreferencedSpectatorState[1060];
/* cg.centerPrint* state block. Written by CG_PriorityCenterPrint (0x30019050)
 * and read by the center-string renderer at 0x300191b0 (CG_DrawCenterString:
 * MOV ESI,[0x3048a9ac] time @0x300191bd, FILD [0x3048a9b0] charWidth
 * @0x30019220, FILD [0x3048a9b4] y @0x30019256, [0x3048adbc] priority
 * @0x300191fd). The
 * mechanical owner=pm_weapon_allowreload label was wrong (size-guess): the
 * "Center Print" .rdata tag (0x30076cdc) and the classic Q3 center-print
 * layout prove these are the cg center-print HUD fields. */
/* 0x3048a9ac .data — cg.centerPrintTime: cg_time + 2000 (ms) display expiry. */
extern int32_t cg_centerPrintTime;
/* 0x3048a9b0 .data — truncated charWidth argument; divided by 32 for scale. */
extern int32_t cg_centerPrintCharWidth;
/* 0x3048a9b4 .data — truncated y argument; vertical center of the text block. */
extern int32_t cg_centerPrintY;
/* 0x3048a9b8 .data — cg.centerPrint[1024]: the center-print text buffer.
 * CRT-strncpy'd with count 1023; byte [0x3048adb7] zeroed at
 * 0x3001909d is this buffer's last element, so it is not a separate symbol. */
extern char cg_centerPrintString[MAX_STRING_CHARS];
/* 0x3048adb8 .data — cg.centerPrintLines: line count (init 1, ++ per '\n' and
 * per literal "\\n" escape). */
extern int32_t cg_centerPrintLines;
/* 0x3048adbc .data — cg.centerPrintPriority: the priority of the pending print;
 * a new print is dropped when its priority < this while a print is active. */
extern int32_t cg_centerPrintPriority;
/* 0x3048adc0..0x3048ade4 (.bss, zero-init): cg_screenFade — the per-frame full-screen
 * fade/flash overlay state block, ten contiguous 4-byte fields. The mechanical export
 * split this into ten g_data_script_func_getangledelta_* dwords with a wrong
 * owner/name (the size-guess "script_func_getangledelta"); the sole reader/writer is
 * the fade *draw* step CG_DrawFade (0x3001ab90), which the machine code proves does a
 * time-driven RGBA lerp and a full-screen CG_FillRect — not angle math. Superseded to
 * one typed struct. (These addresses land above .data's end 0x3008c000, so they are
 * BSS: zero-initialized runtime state, no file bytes.) endTime is compared signed
 * against cg_time; rate==0.0f means the fade is inactive. color[] is the RGBA drawn
 * this frame; fromColor[] is the fade's stored source RGBA. See CG_DrawFade. */
typedef struct cg_screen_fade_s {
    int32_t endTime; /* +0x00 (0x3048adc0): fade end time in cg_time ms */
    float rate; /* +0x04 (0x3048adc4): 1/duration; 0.0f => fade inactive */
    vec4_t color; /* +0x08 (0x3048adc8): current RGBA composed and drawn */
    vec4_t fromColor; /* +0x18 (0x3048add8): stored source RGBA of the fade */
} cg_screen_fade_t;
extern cg_screen_fade_t cg_screenFade;
/* 0x3048ade8 (.data): cg_outOfAmmoState — persistent low/out-of-ammo warning state
 * owned solely by CG_OutOfAmmoChange (0x30034a00). 0 = has ammo / not warned;
 * 1 = warned, some ammo remaining (<threshold); 2 = warned, zero ammo. The warning
 * sound plays only on the 0 -> {1,2} transition, and the state is reset to 0 once the
 * scaled ammo total reaches the 5000 threshold. Was the mechanical
 * g_data_quateigentrace_3048ade8 (owner label = first-touching function). */
extern int32_t cg_outOfAmmoState;
/* 0x3048adec .data refs=4: cg_crosshairEntNum — the client/entity number of the
 * entity currently under the crosshair. Written by CG_ScanForCrosshairEntity
 * (0x3001a59c stores the traced entity number) and read by the crosshair-name HUD
 * drawer (0x3001a65a/0x3001a6ea) as the %s player-name index. The mechanical
 * owner=cg_drawscoreboard was only the first-touching label (that name-guess is
 * itself wrong; see CG_ScanForCrosshairEntity). Exact CoD symbol unconfirmed;
 * named by proven role. */
extern int32_t cg_crosshairEntNum;
/* 0x3048adf0 .data refs=3: cg_crosshairEntTime — the cg_time latched when the
 * crosshair entity was last acquired. Written by CG_ScanForCrosshairEntity
 * (0x3001a5a2 stores cg_time) and read by the drawer at 0x3001a640 as the
 * CG_FadeColor start time so the name fades out after the target is lost.
 * Provisional role name; exact CoD symbol unconfirmed. */
extern int32_t cg_crosshairEntTime;
/* 0x3048adfc cg_crosshairHealthEntNum (.data refs=2 width=4 first=0x3001a6f0). The
 * entity/client number whose health is currently latched for the crosshair-name
 * tint. Written by the snapshot-processing path at 0x3003cfab
 * (MOV EAX,[playerState+0x134]; MOV [0x3048adfc],EAX) and read by
 * CG_DrawCrosshairNames (0x3001a6f0) which compares it against
 * cg_crosshairEntNum: only when they match does the name get tinted by
 * cg_crosshairHealth (else it is drawn white). Provisional role name; exact CoD
 * symbol unconfirmed. */
extern int32_t cg_crosshairHealthEntNum;
/* 0x3048ae00 cg_crosshairHealth (.data refs=2 width=4 first=0x3001a6f8). The
 * latched health value (0..100 scale) of cg_crosshairHealthEntNum. Written by the
 * snapshot path at 0x3003cfb6 (MOV ECX,[playerState+0x138]; MOV [0x3048ae00],ECX)
 * and read by CG_DrawCrosshairNames (0x3001a6f8 FILDs it), scaled by 0.01 and
 * clamped to [0,1] to drive the green->yellow->red name tint. Provisional role
 * name; exact CoD symbol unconfirmed. */
extern int32_t cg_crosshairHealth;
/* 0x3048ae08: cg_usableHintKind — the latched "which usable-entity hint to draw"
 * selector. CG_LatchOverlaySource (0x3001a5b0) copies cg_snap.field_5d0 here; the
 * drawer CG_DrawCursorhint (0x300303a0) dispatches on it: <=1 draws nothing, a
 * weapon-pickup selector in [0xb,0x8a] or [0x8b,0x10a] selects a weapon hint,
 * CURSOR_HINT_LMG (7) selects the mount hint, CURSOR_HINT_HEALTH (8)
 * selects the health hint, and it is used to index the validity
 * table cgs_media_usableHintShaders (0x3044b6f8). CG_DrawCursorhint clears it to 0 once the
 * hint has fully faded (CG_FadeColor returned NULL). Reset with 0x3048ae0c/0x10 by
 * the state-reset block at 0x30034d8c. Mechanical owner=pm_jumpforsurface (a
 * size-match mislabel) rejected; named by proven role, exact CoD name unproven. */
extern cursorHint_t cg_usableHintKind;
/* 0x3048ae0c: CG_FadeColor start time (startMsec); latched from cg_time by
 * CG_LatchOverlaySource (0x3001a5b0), consumed by the drawer FUN_300303a0. */
extern uint32_t cg_overlayFadeStartTime;
/* 0x3048ae10: CG_FadeColor duration (totalMsec); latched from the fade-duration
 * constant 0x3052fc4c by CG_LatchOverlaySource, consumed by the same drawer. */
extern uint32_t cg_overlayFadeDuration;
/* 0x3048ae14: cg_usableHintColorByte — a latched 0..255 color/alpha byte for the
 * usable-entity hint. Latched from cg_snap.field_5d4 by CG_LatchOverlaySource; the
 * drawer CG_DrawCursorhint gates the optional CG_FilledBar overlay on it being
 * nonzero and FILDs it, multiplying by 1/255 (0x3007be24) to a [0,1] fraction used
 * as the bar's alpha. Mechanical owner=pm_jumpforsurface (size-match mislabel)
 * rejected; named by proven role, exact CoD name unproven. */
extern int32_t cg_usableHintColorByte;
/* 0x3048ae18: cg_usableHintCommandIndex — a latched signed selector for the "raw
 * command" hint. Latched from cg_snap.field_5d8 by CG_LatchOverlaySource. In
 * CG_DrawCursorhint, when >= 0 it is a gameState hint slot (index + 0x555 passed to
 * CG_ConfigString) whose text is localized via CG_TranslateMessage(.., "Hint String"); when
 * < 0 the hint falls back to the serverCursorHint selector's named
 * CURSOR_HINT_LMG/CURSOR_HINT_HEALTH branches.
 * Mechanical owner=pm_jumpforsurface (size-match mislabel) rejected; named by
 * proven role, exact CoD name unproven. */
extern int32_t cg_usableHintCommandIndex;
/* 0x3048ae24 .data — cg_damageFeedbackTime: cg.time latched at the top of
 * CG_DamageFeedback (0x30034acc) on every hit. Write-only in this DLL; a "time of
 * last damage feedback" latch. Renamed from the size-match owner label. */
extern uint32_t cg_damageFeedbackTime;
/* 0x3048ae28 .data: function-private persistent state (a static local in the
 * original) holding a cg.time timestamp; the only referencing function reads it
 * and clears it to 0 (0x3001ab00). Named by role; exact source name unproven. */
extern uint32_t s_voiceMenuStartTime;
/* 0x3048ae38 .data width=4: the weapon index most recently passed to
 * CG_SetSelectedWeapon (0x30022660); that routine unconditionally records the
 * requested index here (0x30022666 MOV [0x3048ae38],EAX) before deciding whether
 * to apply it. Sole writer, no reader visible in this DLL (a "last requested
 * weapon" latch consumed by engine or unreconstructed code). The mechanical
 * owner=vectordistance2d label was only a first-touch artifact of that function.
 * Named by proven role; exact CoD source name unproven. */
extern int32_t cg_lastRequestedWeapon;
/* 0x3048ae3c .data width=4: a cg.time (ms) snapshot written by
 * CG_SetSelectedWeapon (0x3002267e writes cg_time here) and re-stamped by the HUD
 * overlay-reset cluster (0x3003953d). Write-only in this DLL — a HUD/overlay
 * "last touched" timestamp with no reader visible here. Adjacent to
 * cg_weaponSelectTime (0x3048ae44) in the weapon-select timer group. The
 * owner=vectordistance2d label was a first-touch artifact. Named by proven role
 * (cg.time snapshot); exact CoD source name unproven. */
extern int32_t cg_weaponSelectTimeA;
/* 0x3048ae40 .data width=4: a second cg.time (ms) snapshot written only by
 * CG_SetSelectedWeapon (0x30022684 writes cg_time here). Write-only in this DLL;
 * no reader visible. Sits between the two other weapon-select time snapshots
 * (0x3048ae3c and cg_weaponSelectTime at 0x3048ae44). The owner=vectordistance2d
 * label was a first-touch artifact. Named by proven role; exact CoD source name
 * unproven. */
extern int32_t cg_weaponSelectTimeB;
/* 0x3048ae44 .data width=4.
 * cg_weaponSelectTime — the cg.time (ms) at which the local player last
 * (re)selected a weapon; drives the brief on-screen weapon-name overlay that
 * fades out over 1800 ms after a switch. Snapshotted from cg_time by the
 * weapon-select cluster (writers at 0x30047398/0x3004753f/0x300475dd/0x300477b5/
 * 0x30047b94, plus the per-frame weapon-info cache at 0x30034d73 and 0x3002269c)
 * and read as CG_FadeColor's startMsec by the selected-weapon-name HUD draws
 * (0x3002ec3c here, and the siblings at 0x3002ed8c/0x3002ef11). The mechanical
 * owner=vectordistance2d label was only a first-touch artifact. Exact CoD source
 * name unproven; named by proven role. */
extern int32_t cg_weaponSelectTime;
/* 0x3048ae50..0x3048ae6f: slot expansion fractions. Element zero is the
 * otherwise-unused sentinel/reset cell; slots 1..7 are indexed directly. */
extern float cg_weaponSelectSlotScale[8];
/* +0x70: previous cg.time used to calculate the carousel frame delta. */
extern int32_t cg_weaponSelectLastTime;
/* 0x3048ae74..0x3048aed3 .data — cg_damageDirIndicators[8]: the ring of active
 * "damage-direction" HUD arrows, the sibling of the red damage flash
 * (cg_damageFlashEndTime/Scale just below). Each 0xc-byte slot records one recent
 * damage event:
 *   +0x0 int32_t serverTime  — cg_snap->serverTime when the hit was registered
 *                              (writer 0x30034ca1: MOV [+0], cg_snap->serverTime);
 *                              the drawer expires the slot once cg.time-serverTime
 *                              leaves (0, duration).
 *   +0x4 int32_t duration    — display lifetime in ms (writer 0x30034ca7 copies it
 *                              from the global fade-length @0x3052ea4c).
 *   +0x8 float   yaw         — world yaw toward the damage source, in degrees
 *                              (writer 0x30034d06 stores the projected attacker
 *                              direction); the drawer rotates the arrow icon by
 *                              vectoyaw(cg_refdef.viewaxis[0]) - yaw so it points at
 *                              the attacker relative to the current view.
 * Registered by CG_AddDamageDirection (0x30034ac0), drawn by CG_DrawDamageDirectionIndicators
 * (0x3001a980), zeroed on effect reset (0x30039595 / 0x30034e36). Supersedes the
 * mechanical brass/scratch placeholders g_data_cg_ejectweaponbrass_3048ae74 and the
 * two g_data_cg_addpacketentities_3048ae7{8,c} field labels (a size-guess: this is a
 * 2D HUD indicator ring, not ejected 3D shell brass). */
typedef struct cg_damageDirIndicator_s {
    int32_t serverTime;
    int32_t duration;
    float yaw;
} cg_damageDirIndicator_t;
enum {
    CG_DAMAGE_DIRECTION_SLOT_COUNT = 8
};
extern cg_damageDirIndicator_t cg_damageDirIndicators[CG_DAMAGE_DIRECTION_SLOT_COUNT];
/* 0x3048aed4 .data — cg_damageDirLatestServerTime: cg_snap->serverTime of the most
 * recent damage event, latched right after the array (writer 0x30034d28). It sits
 * exactly at &cg_damageDirIndicators[8], which is why the drawer's loop terminator
 * is the hard address 0x3048aed4. Mechanical owner=cg_ejectweaponbrass rejected. */
extern int32_t cg_damageDirLatestServerTime;
/* 0x3048aee8: previous grenade cook-off counter used to detect the next pulse. */
extern int32_t cg_grenadePulseLastSpecialTime;
/* 0x3048aeec .data — a "suppress marks" gate dword read once, by CG_ImpactMark
 * (0x3002e54b): after the cg_marks_vmCvar.integer enable check, ImpactMark also
 * returns early WITHOUT creating a mark when this dword is nonzero (TEST EAX,EAX;
 * JNZ to the epilogue). It is part of a larger struct cluster at 0x3048ae00.. and
 * has no recovered writer or second reader, so its exact source name (plausibly a
 * demo-playback / no-marks-in-this-state flag) is unresolved from current
 * evidence; the address suffix is retained as the honest disambiguator. Mechanical
 * owner=veh_unlinkplayer was a wrong first-touch label and is rejected. */
extern int32_t cg_suppressMarksGate;
/* 0x3048af0c (.data): cg_damageFlashEndTime — cg.time (ms) at which the red
 * damage-flash screen overlay finishes fading out. The trigger writer sets it to
 * cg_time + 500 (0x30034d1e: MOV EDX,cg_time; ADD EDX,0x1f4; MOV [af0c],EDX), and
 * the effect-reset path (0x3003959d) zeroes it. CG_DrawFlashDamage (0x3001a8e0)
 * draws while cg_damageFlashEndTime > cg_time, so the effect lasts 500 ms and the
 * remaining time (endTime - cg_time) drives the alpha ramp. Signed comparison
 * (CMP/JLE) proves int32_t. Mechanical owner=dynasink label rejected (size guess). */
extern int32_t cg_damageFlashEndTime;
/* 0x3048af10 (.data): cg_damageFlashScale — signed magnitude of the red
 * damage-flash overlay, multiplied into the fade alpha by CG_DrawFlashDamage
 * (0x3001a90d FMUL float ptr). Written as a float by the trigger path (0x30034b3c
 * FSTP after FCHS — can be negative; 0x30034c6b FSTP) and cleared by the reset
 * path. Exact source field name unresolved; named by proven role. Mechanical
 * owner=dynasink label rejected (size guess). */
extern float cg_damageFlashScale;
/* 0x3048af14 .data — cg_damageFlashX: horizontal component of the directional
 * damage blend, the {X} paired with cg_damageFlashScale (the value @0x3048af10).
 * CG_DamageFeedback writes X = -DotProduct(cg_refdef.viewaxis[1], damageDir)*kick
 * (0x30034c3f), or 0 in the all-directions special case (0x30034b30); read together
 * with cg_damageFlashScale by the damage-blend renderers (0x3003fbba/0x30046729) and
 * cleared on reset (0x30034e13). Classic Quake3/CoD cg.damageX. A float. Renamed
 * from the size-match owner label. */
extern float cg_damageFlashX;
/* 0x3048af18: current low-byte bobCycle phase in radians, computed as
 * (bobCycle & 255) / 255 * 2*pi + 2*pi and passed to the BG bob-factor helpers. */
extern float cg_bobCyclePhase;
/* 0x3048af1c .data refs=7 width=4 first=0x30034dec owner=cg_addscalefade */
extern float cg_weaponMoveSpeed;
extern uint8_t cg_unreferencedViewWeaponStateA[224];
/* Per-frame gate suppressing the local view weapon. */
extern qboolean cg_viewWeaponSuppressed;
/* 0x3048b004..0x3048b057 — the fxTest command/CG_UpdatePeriodicEffect state
 * block: cg_periodicEffectName (char[64] @0x3048b004), cg_periodicEffectOrigin
 * (vec3_t @0x3048b044), cg_periodicEffectLastTime (@0x3048b050) and
 * cg_periodicEffectInterval (@0x3048b054). Typed and declared in
 * client_recovered.h after the common vector types are available. */
/* 0x3048b058..0x3048b063 .data — cg_viewKickVel: the persistent view-kick
 * angular velocity vec3 (.x@0x3048b058, .y@0x3048b05c, .z@0x3048b060), the
 * time-derivative of cg_viewKickAngles. CG_UpdateViewKick (0x3003f9f0) treats it
 * as a 3-element float array indexed together with cg_viewKickAngles: it
 * integrates a weapon-dependent centering acceleration into it and zeroes a
 * component when its angle centers/clamps. Consolidated from the three
 * uint32_t g_data_cg_addscalefade_3048b05{8,c}/3048b060 placeholders (one vec3,
 * not three scalars). Exact original cg field name unproven; role name from the
 * view-kick centering integrator. */
extern vec3_t cg_viewKickVel;
/* 0x3048b064..0x3048b06f .data — cg_viewKickAngles: the persistent view-kick
 * angular offset vec3 (.x@0x3048b064, .y@0x3048b068, .z@0x3048b06c) added to the
 * view during first-person rendering. CG_UpdateViewKick (0x3003f9f0) integrates
 * cg_viewKickVel into it, damps motion toward center, clamps each component to
 * +/-10.0 degrees, and resets to 0 on a zero crossing. Other view code reads the
 * components directly (e.g. FADD [0x3048b064] at 0x300426f7). Consolidated from
 * the three uint32_t g_data_cg_addscalefade_3048b06{4,8,c} placeholders (one
 * vec3). Exact original cg field name unproven; role name from the integrator. */
extern vec3_t cg_viewKickAngles;
/* 0x3048b070..0x3048b07b .data — cg_adsViewErrorAngles: the persistent ADS
 * view-error (idle aim-wander) angular offset vec3 (.x@0x3048b070, .y@0x3048b074,
 * .z@0x3048b078). CG_UpdateAdsViewError (0x30036070) steps .x and .y each activation
 * by AngleMod(prev + trig(phase)*magnitude) with a random magnitude in the weapon's
 * [adsViewErrorMin, adsViewErrorMax] and a random phase (rand()*2*pi/1666); .z is
 * written by the sway integrator at 0x30041746. The view builder at 0x300426ed adds
 * this whole vec3 to cg_viewKickAngles when composing the final view angle offset
 * (FLD [0x3048b070] + [0x3048b064], etc.). Consolidated from the three
 * g_data_*_3048b07{0,4,8} placeholders (one vec3); the owner labels
 * scriptent_moveaxis / g_moverpush were mechanical first-touchers and are wrong.
 * Exact original cg field name unproven; role name from the writer/consumer. */
extern vec3_t cg_adsViewErrorAngles;
extern vec3_t cg_unreferencedViewVectors[5];
/* 0x3048b0b8 / 0x3048b0bc .data -- cg_effectProjAnglePitch / cg_effectProjAngleYaw:
 * a cached {pitch, yaw} angle pair (degrees, floats) describing a fixed world
 * direction that is projected onto the screen. Proven floats: written together by
 * the effect-update path at 0x30046810 (FSTP [0xb8] / FSTP [0xbc]; the else-branch
 * at 0x30046838 falls back to copying cg_refdefViewAngles[0]/cg_refdefViewAngles[1]),
 * and read as an angle pair by CG_ProjectEffectAnglesToScreen (0x30019373/0x30019379)
 * which multiplies each by DEG2RAD and feeds them through sincos to build a forward
 * unit vector. Retyped from the mislabeled uint32_t placeholders; owner=cg_parseimpacteffects
 * was only the first toucher. Exact original cg field names unproven; float width,
 * {pitch,yaw} shape and projection role are proven. */
extern float cg_effectProjAnglePitch;
extern float cg_effectProjAngleYaw;
/* 0x3048b0c0..0x3048b0cb .data — cg_adsViewOffset: the ADS view-relative offset
 * vec3 (.x@0x3048b0c0, .y@0x3048b0c4, .z@0x3048b0c8). Written only as a unit:
 * CG_CalcAdsViewOffset (0x300451a0) stores (pos - cg_refdef.vieworg)*adsFraction
 * component-by-component, and both it and 0x30046a00 zero all three dwords when
 * ADS is inactive. Consolidates the three prior
 * uint32_t g_data_script_method_scriptbuiltin_setrig_3048b0c{0,4,8} placeholders
 * (one vec3, not three ints). Exact original cg field name not fully proved;
 * role name from the ADS-gated projection. */
extern vec3_t cg_adsViewOffset;
/* Persistent BG weapon-position base-angle triple. */
extern vec3_t cg_weaponPositionBaseAngles;
/* 0x3048b0d8..0x3048b0e3: persistent pitch/yaw/roll recoil accumulator. */
extern vec3_t cg_weaponRecoilAngles;
/* cg_specialTagPlacement (0x3048b0e4 .data) — a single fixed entity placement
 * (world origin + 3x3 axis), 12 dwords / 0x30 bytes: origin at 0x3048b0e4 (+0x0),
 * axis[0] at 0x3048b0f0 (+0xc), axis[1] at 0x3048b0fc (+0x18), axis[2] at
 * 0x3048b108 (+0x24), ending 0x3048b110 (+0x2c). Supersedes the 13 mechanical
 * g_data_pm_beginreloadloop_3048b0{e4..} scalars: "pm_beginreloadloop" was only the
 * first-touching function, not the subsystem. Proven layout: CG_GetEntityOriginAxis
 * (0x3002adb0) returns this record for the pseudo-entity index band
 * [MAX_GENTITIES, MAX_GENTITIES+0x80) — copying 0x3048b0e4/e8/ec into outOrigin and
 * 0x3048b0f0..0x3048b110 (9 dwords) into outAxis; the tag/effect setup at 0x3001fec0
 * lays the same 12 dwords out (origin-after-axis) into a stack orientation.
 * Provisional name by role; exact CoD symbol unproven. */
extern orientation_t cg_specialTagPlacement;
/* 0x3048b114 (.data): a cgame screen/view runtime-state gate (refs=5). Written as
 * part of a state-reset block at 0x30034d8c (zeroed alongside 0x3048ae08/0c and
 * 0x30487ad8/dc), compared at 0x30041a60 (CMP [0x3048b114],EBP) and read by
 * CG_Draw2D (0x3001bfe0, third entry guard: nonzero => skip drawing and
 * take the tail path) and 0x30034511 / 0x30045d07. Nonzero appears to mean the
 * screen/view is in a transitional (not-ready-to-draw) state. Provisional role name
 * kept address-shaped (exact source name unresolved); the mechanical owner=fire_lead
 * label was a size-collision mislabel and is corrected here. */
extern uint32_t g_cgScreenReadyState;
extern uint8_t cg_unreferencedViewEffectState[1044];
/* 0x3048b52c..0x3048b5bc .data: the client's fixed 4-slot camera-shake table,
 * cg_shakeSource_t cg_shakeSources[4] (stride 0x24). The mechanical exporter
 * captured individual dwords inside this array as separate g_data_*_viewki_*
 * symbols (0x3048b52c slot0.startMsec, 0x3048b548 slot0.scaledAmplitude,
 * 0x3048b56c slot1.scaledAmplitude, 0x3048b590 slot2.scaledAmplitude,
 * 0x3048b5b4 slot3.scaledAmplitude); those are superseded here by the typed
 * array declared as cg_shakeSources in client_recovered.h (defined in
 * globals.c). Proven by the shake trio: CG_AddCameraShake (0x3001b420) REP MOVSDs
 * a 9-dword (0x24) cg_shakeSource_t into a free/weakest slot;
 * CG_EvaluateCameraShakeSource (0x3001b390) reads a slot; the aggregate walker
 * (0x3001b550) iterates all 4. See cg_shakeSources in client_recovered.h. */
/* 0x3048b5bc .data -- cg_shakeSpinPhase (float): the random phase offset (radians) of
 * the camera-shake sway. NOT part of cg_shakeSources[4] (that array ends at 0x3048b5bc;
 * &cg_shakeSources[4] is only CG_AddCameraShake's loop-end bound). CG_CalcViewShake
 * (0x3001b550) FSTPs a fresh random phase in [-PI, PI) here on frames with no active
 * shake and FADDs it as the phase of its three FSIN sway terms. Type repaired
 * uint32_t->float; mechanical viewki owner rejected. */
extern float cg_shakeSpinPhase;
/* 0x3048b5c0 .data -- cg_shakeExternAmplitude (float): an externally-set shake amplitude
 * written by 0x3001f901 and merged (max) into the shake amplitude by CG_CalcViewShake
 * (0x3001b5b1). Type repaired uint32_t->float; mechanical item_slider_paint owner
 * rejected. */
extern float cg_shakeExternAmplitude;
/* 0x3048b5c4: write-only camera-shake restart cell; no reader exists in this DLL. */
extern int32_t cg_shakeRestartLatch;
/* 0x3048b5c8 .data — cg_hudSpinBaseTime (float). Written (FSTP float) by
 * CG_ConfigString11Modified (0x3001d391) and its sibling 0x3002e350 as the atof of
 * config string 11; consumed by CG_UpdateHudSpinAngle (0x3001d3aa) via FSUB float as
 * the base time subtracted from a running clock, then scaled to advance the spinning
 * HUD element whose angle is the adjacent cg_hudSpinAngle at 0x3048b5cc. Type repaired
 * uint32_t->float (all accesses are x87 float); size-match owner bg_getweapontypename
 * rejected. Exact original symbol name unresolved (role-derived), so the address
 * suffix is kept as the only disambiguator. */
extern float cg_hudSpinBaseTime;
/* 0x3048b5cc .data refs=5 width=4 first=0x3001d412.
 * RESOLVED (role): cg_hudSpinAngle — the current rotation angle (float, degrees)
 * of the time-animated spinning HUD element. Sole writer is CG_UpdateHudSpinAngle
 * (0x3001d3a0), which advances it from cg_time via a stepped interpolation and
 * FSTPs a float here; sole consumer is CG_DrawSpinningPic (0x3002f910), which
 * reads it as the `angle` argument to CG_DrawRotatedPic. Written as a raw dword
 * in the callee's reset paths (a float bit pattern), so the datum is a float.
 * Exact CoD source name unproven; role name. The mechanical owner label
 * "pm_beginweaponchange" is a size-guess for the writer and is rejected. */
extern float cg_hudSpinAngle;
/* 0x3048b5d0 .data — cg_hudSpinVel: the angular velocity (float, degrees/second)
 * of the time-animated spinning HUD element whose angle is cg_hudSpinAngle
 * (0x3048b5cc). Exclusively owned by CG_UpdateHudSpinAngle (0x3001d3a0) — all 29
 * refs are inside that function — which integrates it against the substep time,
 * damps it (spring/critical-damping toward the target angle), clamps it to
 * [-30000, 30000], and zeroes it when the element settles or on reset. Type
 * repaired uint32_t->float (every access is x87 float; the raw-dword MOVs store
 * float bit patterns, e.g. 0x46ea6000 = 30000.0f). The owner=pm_beginweaponchange
 * label is a size-guess for the writer and is rejected. Exact CoD field name
 * unproven; role-derived. */
extern float cg_hudSpinVel;
/* cg_compassRefYaw (0x3048b5d4, .data) — the current compass/objective-pointer
 * reference yaw (float, degrees). CG_DrawObjectivePointers (0x3002fe70) subtracts it
 * from each objective's world bearing (vectoyaw of objOrigin - viewOrigin) before
 * converting to a screen-relative angle, so it is the yaw the HUD compass is oriented
 * to. Written/advanced by the sibling spin-angle updater at 0x3001d6d0 (a twin of
 * CG_UpdateHudSpinAngle, which writes the adjacent cg_hudSpinAngle at 0x3048b5cc);
 * that writer FSTPs a float here. Type repaired uint32_t->float. refs=10 across the DLL
 * are not all reconstructed, so the address suffix is kept and the exact CoD source name
 * is left unproven; the owner=cg_asset_parse label is only the first toucher. */
extern float cg_compassRefYaw;
/* cg_compassRefVel (0x3048b5d8, .data) — angular velocity (float, deg/s) of the
 * compass reference-yaw spring driven by CG_UpdateCompassOrientation (0x3001d6d0). It is the
 * velocity twin of cg_compassRefYaw, exactly as cg_hudSpinVel (0x3048b5d0) is
 * the velocity for cg_hudSpinAngle. CG_UpdateCompassOrientation integrates it against substep
 * time with a critically-damped spring (accel ~1500, damping terms 3/5*dt), clamps its
 * magnitude to [-2000, 2000], and zeroes it on settle/reset. Type repaired uint32_t->float
 * (every value access is an x87 FLD; the reset MOVs store float bit patterns, e.g.
 * 0x44fa0000 = 2000.0f, 0xc4fa0000 = -2000.0f). The owner=item_slider_paint label is a
 * mislead: its first ref (0x3001b5a9 `CMP EDI,0x3048b5d8`) uses the address as an array
 * sentinel, not a value read. Not all 18 refs are reconstructed, so the address suffix is
 * kept and the exact CoD source name is left unproven; role-derived. */
extern float cg_compassRefVel;
enum {
    CG_COMPASS_BLIP_COUNT = 64
};
typedef struct cgCompassBlip_s {
    int32_t updateTime; /* +0x00: last snapshot update time */
    vec3_t origin; /* +0x04: world/map origin and yaw in z */
    int32_t kindOrExpireTime; /* +0x10: tank kind or friendly pulse deadline */
} cgCompassBlip_t;
_Static_assert(sizeof(cgCompassBlip_t) == 0x14, "compass blip stride 0x14");
extern cgCompassBlip_t cg_compassFriendlies[CG_COMPASS_BLIP_COUNT];
extern cgCompassBlip_t cg_compassTanks[CG_COMPASS_BLIP_COUNT];
/* 0x3048bfdc .data refs=4 width=4. RESOLVED (see globals.c): pointer to the active
 * shell-shock params record; its +0x00 field is the sway envelope duration (ms).
 * Written by CG_DrawActiveFrame (0x300424e0), consumed by
 * CG_WeaponSway_ApplyShellShock (0x30044c10). */
extern shellshock_t *cg_shellShockSwayParams;
/* 0x3048bfe0 .data refs=5 width=4. RESOLVED (see globals.c): shell-shock sway start
 * time (signed ms); operand of the sway elapsed envelope in
 * CG_WeaponSway_ApplyShellShock (0x30044c10). */
extern int32_t cg_shellShockSwayStartTime;
/* 0x3048bfe4 .data refs=3 width=4: active shellshock duration (signed ms).
 * CG_WeaponSway_ApplyShellShock forms remaining = startTime + duration - cg.time. */
extern int32_t cg_shellShockSwayDuration;
/* 0x3048bfe8 .data refs=5 first=0x3003c122. Shellshock looping-sound end time (a
 * signed millisecond deadline; 0 = no shellshock sound active). Mechanical
 * uint32/owner=player_getmethod repaired to int32_t; see globals.c. */
extern int32_t cg_shellshockSoundEndTime;
/* 0x3048bfec .data: shellshock mouse-sensitivity multiplier. The mouse update
 * interpolates it toward shellshock_t.mouseSensitivityScale, EndShellShockMouse
 * restores 1.0f, and CG_DrawActiveFrame multiplies it into cg_zoomSensitivity
 * before CG_SET_USER_CMD_VALUE. */
extern float cg_shellshockMouseSensitivityScale;
/* 0x3048bff0 .data: shellshock screen-blur amount, component X (a float — x87
 * FLD/FUCOMP accesses; mechanical uint32 width was wrong). Default 0.0f. Paired
 * with cg_shellshockScreenBlurY below: the screen-blur pass (0x3003b670) runs only
 * when either differs from 0.0f, and both are zeroed together by CG_EndShellShock
 * (0x3003c1d0) and by 0x3003c630. Consumed by CG_EndShellShock. Exact source field
 * name unresolved; named by proven role. */
extern float cg_shellshockScreenBlurX;
/* 0x3048bff4 .data: shellshock screen-blur amount, component Y (a float; see
 * cg_shellshockScreenBlurX). Default 0.0f. Consumed by CG_EndShellShock
 * (0x3003c1d0). Exact source field name unresolved; named by proven role. */
extern float cg_shellshockScreenBlurY;
/* 0x3048bff8 .data: cg_fadeOverlayActive — the "drew the timed 2D overlay on the
 * previous frame" latch owned solely by CG_UpdateFadeOverlay (0x3003b7e0), its only
 * accessor (all 3 refs). Set to 1 after a frame that issued CG_R_SAVE_SCREEN/71 and read
 * next frame to gate the CG_R_BLEND_SAVED_SCREEN(value) call; cleared to 0 whenever the overlay
 * is not visible (remaining time <= 0). The mechanical owner=itemparse_forecolor
 * label is wrong — 0x3003b7e0 does no menu-item color parse. */
extern int32_t cg_fadeOverlayActive;
/* 0x3048bffc .data: cg_shellShockStartTime — the cg.time (ms) captured when the
 * manual "cg_shellshock" console command triggers a shellshock. Written by
 * CG_ShellShock_f (0x30017572: MOV [0x3048bffc],cg_time) and read by the scene
 * reader 0x30042160 (0x300424fd) as the effect start time. The mechanical
 * owner=concatargs label is a wrong size-guess name for 0x300174b0. */
extern int32_t cg_shellShockStartTime;
/* 0x3048c000 .data: cg_shellShockDuration — the shellshock duration in ms for the
 * manual "cg_shellshock" console command. Written by CG_ShellShock_f
 * (0x30017577: the rounded (durationSeconds*1000) value) and read by the scene
 * reader 0x30042160 (0x30042502). The mechanical owner=concatargs label is a
 * wrong size-guess name for 0x300174b0. */
extern int32_t cg_shellShockDuration;
/* 0x3048c004 .data — cg_adsViewErrorLatched: one-shot latch guarding the ADS
 * view-error (idle aim-wander) step in CG_UpdateAdsViewError (0x30036070). While the
 * player is scoped and the ADS overlay is active, the sway is stepped exactly once
 * per activation: the step runs only when this flag is 0, then sets it to 1
 * (0x30036110); any frame where the scope/overlay gate is not satisfied resets it to
 * 0 (0x300361b8), re-arming the next activation. Also cleared by the cgame state
 * reset at 0x30034e4e. Modeled as an int32 boolean; the mechanical owner label
 * cg_addscalefade was the first toucher, not the identity. Role name; exact source
 * name unproven. */
extern int32_t cg_adsViewErrorLatched;
/* 0x3048c030 RESOLVED: cg_crosshairNoGun_vmCvar.string (+0x10 of the vmCvar at
 * 0x3048c020, cg_cvarTable entry 45 "cg_crosshairNoGun", default
 * "gfx/reticle/hud@center_ads.tga"). CG_DrawCrosshair (0x3001a007,
 * PUSH 0x3048c030) registers that cvar's string as the ADS-transition overlay
 * fallback material — the engine's Cvar_Update fills it, which is why no .text
 * writer exists in this DLL. A former cg_adsReticleMaterialName_3048c030[64]
 * standalone buffer here was a mis-model aliasing the vmCvar's string field. */
/* 0x3048c14c .data refs=1 width=4 first=0x3002b370 owner=trap_syscall_3 */
/* 0x3048c268 .data — moving-tracer width/scale for tracer mode B (the default,
 * selected when localEntity_t.leFlags != LEF_TRACER_MODE_A). Float twin of
 * cg_tracerwidthlmg_vmCvar.value; read (never written) by CG_AddMovingTracer
 * (0x3002ab00) and the sibling builders 0x30048460 / 0x30048a00 as the tracer
 * poly width multiplier. No observed writer in this DLL (cvar-like). Type
 * repaired uint32_t->float; owner=pm_switchifempty size-guess dropped; exact
 * name unresolved, address suffix kept. */
/* 0x3048c388 .data refs=1 width=4 first=0x30048541 owner=veh_updateclient */
/* 0x3048c458 .data refs=1 width=4 first=0x30048eda owner=registeritem */

/* 0x3048c4a8 .data refs=31 width=4 first=0x300167e5.
 * RESOLVED (role): cg_hudCompassSize_vmCvar.value — a float animation fraction in the HUD/cursor
 * draw cluster. Read-only in the exported code (FLD/FMUL only, never stored here) and
 * used as a normalized slide/scale factor: the tag/render family and the weapon-select
 * draws compute a horizontal screen offset with the idiom `(cg_hudCompassSize_vmCvar.value - 1.0f)
 * * 112.0f + rect.x` (0x30031cf1, 0x30031db4, 0x30031ecc, 0x30031feb, 0x3002fe82) and
 * FMUL by it as a scale elsewhere (0x30016a7e, 0x3002f91b, 0x300302db). Set by the
 * asset/init path (owner=cg_asset_parse, its first toucher; no writer in the exported
 * set). Exact CoD source name unproven; role name. */

/* 0x3048c5cc .data refs=1 (0x3002cb21) width=4. Sole consumer: CG_PlaySoundAliasByName
 * (0x3002ca80), which loads this dword (MOV EDX,[0x3048c5cc]) and passes it as the
 * last argument of the subtitle trap cgame_syscall(CG_SUBTITLE, subtitleReference,
 * clampedTime, cg_subtitleWidth_vmCvar.integer). Zero in the exported image and never written by
 * .text in this DLL, so it is engine/externally-initialized cgame sound state.
 * Provisional role name; the exact CoD source symbol is unproven. */

/* The typed cg_entities[] declaration for the 0x3048c6e0 array lives after the
 * complete centity_t layout in client_recovered.h. */
/* 0x3048c6f8 is &cg_entities[0].currentStatePos.trBase (0x3048c6e0 + 0x18), not
 * independent storage. CG_PlayEntitySoundAliasByName and its siblings use
 * &cg_entities[clientNum].currentStatePos.trBase with the 0x288-byte entity stride. */
/* 0x3048c768 .data refs=1 width=1 first=0x30047ea4 owner=pm_updatelean */
/* 0x3048c8c8 is &cg_entities[0].currentValid (0x3048c6e0 + 0x1e8), not a separate
 * pool. Addresses 0x3048c8e8/0x3048c8f4 are correspondingly the +0x208/+0x214
 * fields of the same 0x288-byte entity record. */
/* 0x3052e6ec .data refs=1 width=4 first=0x30035a7e owner=pmovesingle */

/* 0x3052e808 .data: the team-chat fade window as a float (milliseconds). Sole
 * reader is CG_DrawTeamInfo (0x30018770, FSUBR at 0x300187e9), which computes
 * (cg_chatTime_vmCvar.value - lineAge) and, once that drops below 43.75, ramps the
 * line alpha as (cg_chatTime_vmCvar.value - lineAge) * 200.0f before culling at <=0.
 * The integer sibling cg_chatTime_vmCvar.integer (0x3052e80c) gates the scroll-out; this
 * float mirror drives the per-line fade. Writer is not among reconstructed code
 * (a cvar-float snapshot), so the exact cvar name is unresolved; retyped from the
 * mechanical uint32_t (the FSUBR proves it is a single-precision float). */

/* 0x3052e80c .data: cg_chatTime_vmCvar.integer — integer value of the cg_chatTime_vmCvar.integer cvar
 * (ms a team-chat line stays visible). CG_AddToTeamChat flushes the ring when
 * this is <=0; the team-info drawer uses it as the fade window. */
/* 0x3052e8c8 .data refs=2 width=imm first=0x3003c9fb owner=debugdumpanims */

/* 0x3052ea4c .data — cg_hudDamageIconTime_vmCvar.integer: display lifetime (ms) copied
 * into cg_damageDirIndicators[best].duration by CG_DamageFeedback (0x30034c8c) when
 * registering a damage-direction arrow. Read-only here; renamed from the size-match
 * owner label. */

/* 0x3052ec8c .data — cg_blood_vmCvar.integer: the integer value of the "cg_blood_vmCvar.integer" cvar (blood
 * effects enabled). Sole reader is CG_BulletHitEvent (0x30048e60, at 0x30048f2d):
 * when cg_blood_vmCvar.integer == 0 AND the impacted surface is flesh (surfaceType == 7) it swaps
 * the primary impact effect for the no-blood flesh effect (cgs_media_fleshImpactEffect,
 * "fx/impacts/flesh_hit_noblood.efx") and suppresses the secondary blood effect. No
 * writer in this DLL (set by the engine/cvar layer), so left as a plain int32 mirror.
 * Named by proven role (the "cg_blood_vmCvar.integer" cvar string @0x300788cc + the noblood.efx
 * swap); exact source symbol assumed to be the cg_blood_vmCvar.integer cvar int mirror. */

/* 0x3052edac .data — cg_teamChatsOnly_vmCvar.integer gate (repaired; mechanical owner
 * "playercmd_cloneplayer" was the rejected size-match guess). A nonzero flag that
 * suppresses NON-team voice/quick chat display: CG_VoiceChat (0x3003a250, reader at
 * 0x3003a2da) early-outs when mode != 1 (not a team message) AND this flag is set;
 * the second reader (0x3003adc1) gates another non-team chat path the same way. No
 * writer exists in this DLL (set by the engine/cvar layer), so it is left as a plain
 * int32 mirror. Named by proven role; exact source symbol unresolved. */

/* 0x3052eec0 (.data): cg_viewSizeCvar - the module-side vmCvar_t mirror for the
 * "cg_viewsize" cvar (the 3D-view size percentage, 30..100). Resolved from
 * CG_CalcVrect (0x3003f510): it reads cg_viewSizeCvar.integer at +0xc
 * (0x3052eecc), clamps it into [30,100], and re-sets the cvar via trap 10
 * (trap_Cvar_SetValue(&cg_viewSizeCvar, "30"/"100")) when it was out of range.
 * The base at +0x00 (0x3052eec0) is pushed as the vmCvar handle. The two former
 * mechanical symbols g_data_bg_animscriptevent_3052eec0 (the base) and _3052eecc
 * (its .integer field) were the same vmCvar_t split across two addresses; merged
 * here into one typed object. owner=bg_animscriptevent is a size-guess first-touch
 * artifact (rejected - the touching function is CG_CalcVrect, not an anim-script
 * handler). The shared vmCvar_t definition is included above, so the typed
 * object can be declared here; storage is defined in globals.c. */
extern vmCvar_t cg_viewSizeCvar;
/* 0x3052efec .data: cg_debuganim_vmCvar.integer -- integer debug level for the animation
 * subsystem (a cvar mirror). BG_SetNewAnimation (0x30003a90) prints its
 * "Anim-%s: %i, %s, (blend time) %i" trace when this == 1; sibling sites compare
 * it against 2 (0x3000421f) and 5 (0x30034323) for higher verbosity. Supersedes
 * the mechanical g_data_bg_evaluatetrajectory_3052efec (size-matched wrong owner
 * label). */

/* 0x3052f10c .data refs=7 width=4: cgame developer/debug-print gate. Read
 * (TEST/CMP against 0, and once `== 1`) before emitting Com_PrintMessage diagnostics
 * in the config-string/command-parse cluster (0x30034fac, 0x300358f0..0x30035fe1)
 * and at the head of CG_MapRestart (0x30039500: `if (flag) Com_PrintMessage("CG_MapRestart\n")`).
 * Role name; exact CoD symbol unproven (likely a `bg_debugWeaponMessages_vmCvar.integer`/`developer` mirror).
 * The mechanical owner=pmovesingle is the wrong first-touch guess. */

/* 0x3052f34c .data refs=1 width=4.
 * RESOLVED (behavioral): cg_predictItems_vmCvar.integer — the cached integer value of the
 * cg_predictItems_vmCvar.integer cvar. CG_TouchItem (0x30035680, the sole reader) begins with
 * `if (!cg_predictItems_vmCvar.integer) return;`, exactly the Q3/CoD CG_TouchItem prediction gate.
 * Read-only here; a cvar-refresh path writes it. owner=axiscopy was a size-guess
 * mislabel. Name is behavioral (cvar string not proven from this function alone). */

/* 0x3052f6ac .data refs=1 width=4 first=0x3001bd53 owner=pm_airmove (mislabel).
 * RESOLVED on consume by CG_DrawSpectatorFollowHints (0x3001bd50): a boolean
 * enable gate for the spectator "follow" key-hint HUD. The draw function's very
 * first act is `if (!cg_descriptiveText_vmCvar.integer) return;` (MOV EAX,[0x3052f6ac];
 * TEST EAX,EAX; JZ end). Read-only in this DLL (no .text writer); an engine
 * cvar-registration / refresh path sets it — the classic cached cvar->integer
 * mirror. Read as a plain boolean int. The exact cvar name is not proven from the
 * single consumer, so the role name is behavioral. */

/* 0x3052f7c8 .data refs=2 width=4.
 * RESOLVED on consume by CG_CalcFov (0x3003ffc0): cg_fov_vmCvar.value -- the base view
 * field-of-view in degrees (the cg_fov cvar's float value mirror). CG_CalcFov reads
 * it and clamps it to [80, 160] degrees; a screen-projection helper at 0x30040330
 * divides a screen coordinate by it (FDIV) as a float. Read as a float by both
 * consumers; no direct .text writer (engine cvar-registration path updates it). The
 * mechanical owner=bg_parseweaponinfospecificfieldtyp label was a wrong size-based
 * first-touch. */

/* 0x3052f8ec .data refs=2 width=4 first=0x3001b07c owner=colorbytes3 (mechanical owner is wrong).
 * Resolved on consume by Com_DPrintf (0x3002b470): the developer-mode gate. Both
 * readers test it as a plain boolean int and skip developer-only work when zero:
 * Com_DPrintf (0x3002b482: TEST EAX,EAX / JZ skips the whole format+print) and a
 * debug-draw path at 0x3001b07c. This is the cached integer value of the Quake3
 * `developer` cvar (developer->integer). Named developer_vmCvar.integer; the exact cvar
 * registration string is not reconstructed here but the dev-gate role is proven. */
/* 0x3052fb28 .data refs=1 width=4 first=0x30044e31 owner=setclientviewangle */
/* 0x3052fc4c .data refs=1 width=4 first=0x3001a5ce owner=pm_jumpforsurface */
/* 0x3052fc4c: default duration copied into cg_overlayFadeDuration. */

/* 0x3052ffa8 .data refs=2 (0x3002cac7, 0x3002caf3) width=4. Sole consumer:
 * CG_PlaySoundAliasByName (0x3002ca80), which loads it as a float (FLD float ptr) and
 * scales it by 1000.0f to build a millisecond time value passed to the sound-update
 * trap cgame_syscall(CG_SUBTITLE, ...). A seconds-valued cgame sound timing datum.
 * Retyped from the mechanical uint32_t to `float` (the machine code reads it with
 * FLD float ptr). Zero in the exported image and never written by .text in this DLL,
 * so it is engine/externally-initialized. Provisional role name; exact CoD symbol
 * unproven. */

/* 0x305300cc: cl_paused_vmCvar.integer (vmCvar_t at 0x305300c0, cg_cvarTable[99]
 * "cl_paused"). Read purely as a boolean draw-inhibit gate: CG_DrawScoreboard
 * (0x30037d90) returns qfalse immediately when it is nonzero; CG_DrawWeaponIcon3D
 * (0x30019bd0), CG_DrawCrosshair (0x30019eb2), and the effect path (0x3001fb3f)
 * likewise skip work when it is set. Not a standalone global. */

/* 0x305301ec: cg_cursorHints_vmCvar.integer — a small-integer style/animation-mode selector for
 * the usable-entity hint, read (only) by CG_DrawCursorhint (0x300303a0) at three
 * sites. Style 3 sine-wobbles the hint rect height and suppresses the pulse; style 2
 * pulses on the fractional seconds of the pickup timer; any other style applies a
 * fixed sine-wobble color pulse. Written elsewhere (a cvar/config latch, writer not
 * in this function). Mechanical owner=pm_footsteps (size-match mislabel) rejected;
 * named by proven role, exact CoD name unproven. */
/* 0x30530308 .data refs=1 width=4 first=0x3003f798 owner=scr_vehicle_damagescale */
/* 0x30530428 .data refs=1 width=4 first=0x30044eb5 owner=setclientviewangle */
/* 0x30530668 .data — moving-tracer length for tracer mode B (the default,
 * selected when localEntity_t.leFlags != LEF_TRACER_MODE_A). Float twin of
 * cg_tracerlengthlmg_vmCvar.value; read (never written) by CG_AddMovingTracer
 * (0x3002ab00) as the VectorMA march distance, and by the sibling builders
 * 0x300482f1 / 0x30048a3e. No observed writer in this DLL (cvar-like). Type
 * repaired uint32_t->float; owner=pm_switchifempty size-guess dropped; exact
 * name unresolved, address suffix kept. */

/* 0x30530780 .data — cg_localEntities[MAX_LOCAL_ENTITIES]: the fixed pool of 128
 * localEntity_t backing both the free list and the active list. Because the array's
 * element type localEntity_t must be COMPLETE (it is an array, not a pointer) and
 * localEntity_t / MAX_LOCAL_ENTITIES are defined in client_recovered.h — which is
 * included AFTER globals.h — the extern declaration lives there (search cg_localEntities).
 * CG_InitLocalEntities (0x3002a9e0) zeroes it (rep stosd 0x1d80 dwords = 128*236) and
 * chains the free list via ->next. The mechanical export mislabeled this
 * owner=anglenormalize180accurate (a pure size guess; not an angle helper) and split two
 * interior element addresses into their own symbols — 0x30530784 == &cg_localEntities[0].next
 * and 0x30537c98 == &cg_localEntities[127].next — both fields inside this one array, so the
 * three bogus symbols were removed and unified into the single typed array. */
/* localEntity_t is defined in client_recovered.h; forward-declared here so the
 * local-entity list globals can carry their real pointer/struct types. */
struct localEntity_s;
/* 0x30537d80 .data — cg_freeLocalEntities: head of the singly-linked free list of
 * localEntity_t (chained via ->next). CG_InitLocalEntities points it at
 * cg_localEntities[0]; CG_AllocLocalEntity pops from it, CG_FreeLocalEntity pushes.
 * (Exporter owner label "anglenormalize180accurate" is wrong — resolved by consumer.) */
extern struct localEntity_s *cg_freeLocalEntities;
/* 0x30537da0 .data — cg_activeLocalEntities: the sentinel node of the circular
 * doubly-linked list of active localEntity_t. Its .prev (+0x0, at 0x30537da0) and
 * .next (+0x4, at 0x30537da4) are the list head/tail links; CG_InitLocalEntities
 * self-links both. (The mechanical export split .next into a separate symbol at
 * 0x30537da4; both are fields of this one struct, so they are unified here.) */
extern struct localEntity_s cg_activeLocalEntities;
/* 0x30537ea0 .data refs=2 width=4 first=0x30018c9e owner=cmd_veh_fireturret */
extern int32_t cg_lagometerFrameSamples[LAG_SAMPLES];
/* 0x305380a0 .data refs=3 width=4 first=0x30018c92 owner=cmd_veh_fireturret */
extern int32_t cg_lagometerFrameCount;
/* 0x305380a4 (.data): cg_lagometer — the network-lag graph ring; its snapshot-side
 * tail (snapshotFlags at 0x305380a4, snapshotSamples at 0x305382a4, snapshotCount at
 * 0x305384a4) is modelled by lagometer_t above. Retyped from the three mechanical
 * uint32_t symbols (owner=vector4scale was the first-touching function, not the
 * identity); the three addresses are three fields of this one object. */
extern lagometer_t cg_lagometer;
/* 0x305384c0 .data — cg_hudEmitClientTable[]: the index->clientNum lookup table of
 * the HUD trap-54 emit family. Read as `MOV EAX,[EAX*4 + 0x305384c0]` (dword array
 * indexed by cg_currentSelectedPlayer_vmCvar.integer) by 0x30030f10 / 0x30031020 / 0x300311f0; the result
 * is multiplied by 0x4d0 and added to bgs.clientinfo's base, i.e. it holds the
 * per-client anim-state index for each active HUD entry. Physically bounded above by
 * the next distinct symbol at 0x305384e0 (0x20 bytes == 8 dwords). Supersedes the
 * single-uint32 mechanical export and owner=script_func_precacheheadicon label.
 * Exact source name/extent unresolved; named by proven role. */
extern int32_t cg_hudEmitClientTable[]; /* 0x305384c0 */
/* 0x305384e0 .data refs=1 width=imm first=0x300319c8 owner=sp_trigger_lookat */
extern char cg_hudEmitScratch[CG_HUD_STRING_BUFFER_SIZE];
/* 0x305385e0 .data — cg_hudEmitCount: the number of active entries in
 * cg_hudEmitClientTable[]; the exclusive upper bound the HUD trap-54 emit family
 * range-clamps cg_currentSelectedPlayer_vmCvar.integer against (`CMP EAX,[0x305385e0]; JL ok`, signed).
 * Supersedes the mechanical owner=script_func_precacheheadicon label. Exact source
 * name unresolved; named by proven bound role. */
extern int32_t cg_hudEmitCount; /* 0x305385e0 */
/* 0x30538600 is cg_trapStringBufferA[256], declared above near cgame_syscall;
 * the mechanical uint32_t alias here was superseded (the datum is a NUL-terminated
 * strcpy destination, not a 4-byte immediate) — do not re-add a second symbol. */
/* 0x30538700 .data refs=1 width=imm first=0x30031a28 owner=bg_playanimname */
/* 0x3053880c .data refs=1 width=4: developer-print gate for the weapon-state change
 * logger. PM_Weapon (0x30014710) reads it once and, when it is nonzero AND not equal
 * to 3, calls PM_Weapon_PrintWeaponState (0x30014a80) to print ps->weaponState
 * transitions. Never written in this DLL -> engine/cvar-owned (a cg_debug* integer
 * cvar mirror); the "3" is a print-target sentinel of a shared dev-print level enum.
 * The mechanical owner=calcmuzzlepoint label was a size-guess first-toucher artifact
 * (the real 0x30014710 is PM_Weapon, not CalcMuzzlePoint); renamed by proven role,
 * exact source cvar name unresolved. */

/* 0x3053892c .data refs=1 width=4: developer-print gate for the weapon weaponAnim
 * change logger. PM_Weapon (0x30014710) reads it once and, when nonzero AND not equal
 * to 3, calls PM_Weapon_PrintWeaponAnim (0x30014bd0) to print ps->weaponAnim
 * (+0x624) weapon-anim pose. Never written in this DLL -> engine/cvar-owned; same "3"
 * print-target sentinel as bg_debugWeaponState.integer. Mechanical owner=calcmuzzlepoint was
 * the same size-guess artifact; renamed by proven role, exact cvar name unresolved. */

/* 0x30538a48 .data: cg_bobMax_vmCvar.value, the runtime value registered under the literal
 * cvar name "cg_bobMax_vmCvar.value" (0x30078be4). CG_OffsetFirstPersonView passes it as the
 * maximum amplitude to the vertical and horizontal bob-factor helpers. */

/*
 * Stance-specific weapon/view bob amplitude scales, selected by comparing
 * playerState_t::viewHeightTarget against proneViewHeight/crouchViewHeight. Read
 * (never written) as a prone/crouch/stand triplet by the whole bob family:
 * BG_GetVerticalBobFactor (0x30014dd0), BG_GetHorizontalBobFactor (0x30014e50),
 * and the two CalculateWeaponPosition bob callers at 0x30015300 / 0x300153c0.
 * Typed float, superseding the mechanical uint32_t bit-pattern export; the
 * `owner=vector5add` label is a rejected size-only guess (the consumers are bob
 * factors, not a vector add). Exact source names unresolved — named by proven
 * stance role.
 */
/* 0x30538b68 */
/* 0x30538c88 */
/* 0x30538da8 */
/* 0x30538ecc: developer parsing verbosity mode used during weapon loading. */

/* 0x30538fec .data (sole reader PM_UpdateViewAngles 0x3000cde9): a spread-weapon
 * pitch-clamp limit, an integer degree bound read via FILD only on the
 * weaponClass==WEAPCLASS_LMG branch (it overrides the default pitch limit
 * derived from pml.frametime). Read-only in this DLL (no writers found; cvar/config
 * owned). Retyped from the mechanical uint32; owner=veh_findvaliddismountspot is a
 * wrong first-touch/size-guess artifact. Exact cvar name unresolved; named by role,
 * address kept as the disambiguator. */

/* 0x3053910c .data: bg_foliagesnd_minspeed.integer — the horizontal-speed floor (units/s,
 * signed int) below which PM_FoliageSounds (0x3000c110, the sole reader) plays no
 * foliage sound. Used two ways there: (1) FILD'd to float and compared against
 * pm->horizontalSpeed (+0xf4) as the below-floor early-out, and (2) as
 * the low end of the speed->interval interpolation numerator/denominator
 * ((speed - floor) / (ceil - floor)). Never written in this DLL (cvar/config
 * owned). Retyped from the mechanical uint32_t; the owner=g_spawnvehicle label is a
 * wrong first-touch/size artifact. Exact source cvar name unresolved; named by
 * proven role. */

/* 0x3053922c .data: bg_viewheight_prone_vmCvar.integer — integer view height (units) added to the
 * muzzle Z for an entity in the prone stance. CG_CalcMuzzlePoint (0x30048b60) is the
 * sole reader (refs=1), selecting it when the entity's eFlags has the 0x40 stance bit
 * set (FILD dword ptr [0x3053922c]; loaded as signed int, converted to float). Never
 * written in this DLL (engine/config-owned). Retyped from the mechanical uint32_t; the
 * owner=pm_weapon_checkforchangeweapon label was the first-touching function, not the
 * identity. Exact CoD name unproven; stance role proven by the eFlags 0x40 discriminant. */

/* 0x3053934c .data: bg_foliagesnd_fastinterval.integer — the foliage-sound repeat interval
 * (ms, signed int) selected when the player's horizontal speed is at/above the
 * speed ceiling. PM_FoliageSounds (0x3000c110, sole reader) interpolates the
 * repeat interval as lerp(bg_foliagesnd_slowinterval.integer, bg_foliagesnd_fastinterval.integer,
 * frac) where frac is the clamped speed fraction: interval =
 * round((bg_foliagesnd_fastinterval.integer - bg_foliagesnd_slowinterval.integer) * frac +
 * bg_foliagesnd_slowinterval.integer). Never written in this DLL (cvar/config owned).
 * Retyped from the mechanical uint32_t; owner=g_spawnvehicle is a wrong
 * first-touch/size artifact. Exact source cvar name unresolved; named by role. */

/* 0x30539468 .data: bg_fallDamageMaxHeight.value, the upper fall-height threshold at
 * which PM_CrashLand assigns 100 damage. It must be greater than
 * bg_fallDamageMinHeight.value for damage to be enabled. Also used by CG_EntityEvent to
 * reconstruct a landing height from the event's 0..100 damage parameter. */

/* 0x30539580..0x3053958b: pml.forward, the pmove locals' horizontal forward basis
 * vector (pml.forward), one vec3_t. Built/z-flattened/normalized and consumed by
 * PM_AirMove (0x30009060). Superseded from three mechanical uint32 dwords (owner
 * label cg_drawcrosshairnames was only the first toucher). See globals.c. */
/* 0x3053958c..0x30539597: pml.right, the pmove locals' horizontal right basis vector
 * (pml.right), one vec3_t; sibling of pml.forward. Superseded from three mechanical
 * uint32 dwords (owner label lookatkiller was only the first toucher). See globals.c. */
/* 0x30539598..0x305395a3: pml.up, the pmove locals' up basis vector (pml.up), one
 * vec3_t; the third output of AngleVectors. PmoveSingle (0x3000e050) passes its
 * address in EBX to AngleVectors (0x3004a200) alongside pml.forward (ESI) and
 * pml.right (EDI) at 0x3000e399. Superseded from the mechanical uint32 (owner
 * g_checkforcursorhints was the wrong-first-touch/size label). See globals.c. */
/* 0x305395a4 .data refs=33 width=4 first=0x3000854e.
 * pml.frametime: current pmove frame time in seconds (pml.frametime). Computed in
 * PmoveSingle setup (0x3000e2f9..0x3000e345) as (float)pml.msec * 0.001f and stored
 * here; read as a float by pmove physics (e.g. PM_Accelerate 0x300085d0 multiplies
 * the acceleration by it). The mechanical owner=vectoangles label is just the first
 * function to touch the datum and is not its identity. See globals.c. */
/* 0x305395a8 .data refs=21 width=4 first=0x3000bd05.
 * pml.msec: current pmove frame time delta in ms (to->serverTime - from->serverTime,
 * clamped 1..200), set in PmoveSingle setup (0x3000e2ce). Signed. See globals.c. */
/* 0x305395ac: pml.walking, qboolean (int32_t) for whether the pmove locals consider the
 * player to be walking on a walkable ground plane this frame (pml.walking). PM_GroundTrace
 * (0x3000a2a0) clears it to 0 together with pml.groundPlane on the ground-miss path (the exact
 * Quake3 PM_GroundTraceMissed epilogue). Superseded from mechanical uint32 (owner=vectoangles
 * was a first-touch artifact). See globals.c. */
/* 0x305395b0: pml.groundPlane, qboolean (int32_t) for whether the pmove locals hold a
 * valid ground trace plane this frame (pml.groundPlane). PM_Jump (0x30008c70) clears it
 * to 0 on take-off; PM_AirMove (0x30009060) gates its ground velocity clip on it.
 * Superseded from mechanical uint32 (owner scr_getgametypenameforscript was only the
 * first toucher). See globals.c for the adjudication note on the 0x3000a/0x3000d writers. */
/* 0x305395b4: pml.groundLiftFlag, pmove-locals flag set by PM_GroundTrace (0x3000a2a0) when the
 * vertical probe trace detects a near/flat lift-style contact, cleared otherwise. Exact original
 * pml field name unresolved; named provisionally by proven role. Superseded from mechanical uint32
 * (owner=scr_getgametypenameforscript was a first-touch artifact). See globals.c. */
/* 0x305395b8 .data: pml.groundTrace — the pmove locals' cached ground-trace result
 * (pml.groundTrace, a 48-byte trace_t). Written by whole-trace_t `rep movsd` copies in
 * PM_CorrectAllSolid (0x3000a26e) and the two ground-trace callers (0x3000a50e/0x3000a5ae).
 * (owner=display_mousemove was a wrong first-touch/size guess — this is pmove, not UI.)
 * See globals.c. */
/* 0x305395e8 .data: pml.maxClipImpact (pml.maxClipImpact) — running high-watermark
 * of the largest negated into-plane velocity magnitude seen during PM_SlideMove's
 * two-plane clip. PM_SlideMove (0x3000e930) is the sole consumer: it FCOMPs the
 * candidate against this latch (0x3000ece4) and stores the new max (0x3000ecf5).
 * A debug/telemetry scratch float; PmoveSingle (0x3000e050) resets it to 0 at the
 * start of each pmove run. Superseded from the mechanical
 * uint32 (owner=pm_stepslidemove was a wrong first-touch/size label). Matches the
 * server pml_s::maxClipImpact. See globals.c. */
/* 0x305395ec..0x305395f7: pml.previousOrigin, the pmove locals' pre-move
 * playerState origin snapshot (pml.previous_origin), one vec3_t. PmoveSingle
 * (0x3000e050) captures ps->psOrigin into it (0x3000e305/e316/e322) and, at the tail,
 * differences the post-move origin against it to derive the frame velocity
 * (0x3000e652/e65b/e668). Other pmove code (0x30008abb) reads it the same way.
 * Superseded from three mechanical uint32 dwords (owner item_setfocus was a wrong
 * first-touch label). See globals.c. */
/* 0x305395f8..0x30539603: pml.previousVelocity, the pmove locals' pre-move
 * playerState velocity snapshot (pml.previous_velocity), one vec3_t. PmoveSingle
 * (0x3000e050) captures ps->velocity into it (0x3000e32e/e339/e34b). Read by the
 * pmove velocity-decay/air code (0x30009d49/0x30009d64). Superseded from three
 * mechanical uint32 dwords (owner g_checkforcursorhints/g_spawnturret were wrong
 * first-touch labels). See globals.c. */
/*
 * pml.previousWaterLevel (0x30539604, .data, refs=3): the previous-frame latch of the
 * BG pmove context byte pm->waterlevel (+0xf1). PmoveSingle caches
 * waterlevel here at 0x3000e5bd (movzbl, so zero-extended) after PM_SetWaterLevel
 * (0x3000a7a0) runs, and PM_WaterEvents (0x3000c290) compares this latch
 * against the fresh waterlevel to detect a 0<->nonzero transition (water enter/
 * exit) and emit EV_WATER_TOUCH/LEAVE. Zero-initialized .data; the mechanical
 * owner=bg_calculateweaponposition_baseang label is a first-touch/size artifact,
 * not the identity. Named by proven role. */
/* 0x30539608 .data (refs=62): cached pointer to the current weapon's weaponInfo_t
 * record. The mechanical owner=bg_bullet_endpos label is merely the first function
 * that touched the datum and is NOT its identity: the writer at 0x3000e0e7 loads it
 * as bg_weaponInfos[weaponIndex] (MOV EDX,[bg_weaponInfos + weaponIndex*4]; MOV
 * [0x30539608],EDX), and PM_Weapon_AddFiringAimSpreadScale (0x30014240) consumes
 * pml.weaponInfo->fireAimSpreadScale (+0x2ac). Typed as weaponInfo_t *. */
/* 0x30539620..0x3053972f is the cgame copy of the shared bg_nofatigue
 * vmCvar_t. Its integer at +0x0c is read by PM_UpdateFatigue at 0x3000c43f. */
/* 0x30539748 .data (both readers in PM_UpdateViewAngles 0x3000cd60/0x3000cd8c): a
 * prone yaw-swing half-range in degrees (float). PM_UpdateViewAngles builds the prone
 * traverse window as [proneDirection - this .. proneDirection + this] when drawing the
 * debug-line traverse marker. Read-only in this DLL (no writers; cvar/config owned).
 * Retyped float from the mechanical uint32; owner=veh_findvaliddismountspot is a wrong
 * first-touch/size-guess artifact. Exact cvar name unresolved; named by role. */

/* 0x3053974c .data (sole reader PM_UpdateViewAngles 0x3000cdce, FILD): an integer
 * degree bound for the default pitch clamp, converted to float and used as the pitch
 * traverse limit on the non-spread-weapon path. Read-only in this DLL (no writers;
 * cvar/config owned). Retyped int32 from the mechanical uint32;
 * owner=veh_findvaliddismountspot is a wrong first-touch/size-guess artifact. Exact
 * cvar name unresolved; named by role. */

/* 0x30539850 .data (refs=186): the current BG animation player-DObj context
 * pointer, pm. The mechanical owner=bg_getweaponindexforname label is
 * merely the first function that touched the datum and is NOT its identity; the
 * writers at 0x3000e050/0x3000e740 set it while preparing a player's animation
 * state. Typed as pmove_t * (see below). */
/* 0x3053986c .data: bg_foliagesnd_resetinterval.integer — quiet-time delay (ms, signed int) added
 * to the last-foliage-sound timestamp (playerState_t::foliageSoundTime, +0x38) on
 * the below-floor path of PM_FoliageSounds (0x3000c110, sole reader): when speed is
 * below bg_foliagesnd_minspeed.integer and (foliageSoundTime + bg_foliagesnd_resetinterval.integer) has
 * elapsed relative to pm->command.commandTime, the foliage timer is reset to 0. Never
 * written in this DLL (cvar/config owned). Retyped from the mechanical uint32_t;
 * owner=g_spawnvehicle is a wrong first-touch artifact. Exact cvar name unresolved;
 * named by role. */

/* 0x3053998c .data: bg_foliagesnd_slowinterval.integer — the foliage-sound repeat interval
 * (ms, signed int) at the speed floor (frac == 0), i.e. the base of the
 * speed->interval lerp in PM_FoliageSounds (0x3000c110). Read there twice: as the
 * subtrahend forming the lerp span (bg_foliagesnd_fastinterval.integer -
 * bg_foliagesnd_slowinterval.integer) and FIADD'd back as the lerp base. Never written in
 * this DLL (cvar/config owned). Retyped from the mechanical uint32_t;
 * owner=g_spawnvehicle is a wrong first-touch artifact. Exact cvar name unresolved;
 * named by role. */

/* 0x30539aac .data: bg_viewheight_standing.integer — integer view height (units) used as the
 * default/standing stance height. CG_CalcMuzzlePoint (0x30048b60) adds it to the muzzle
 * Z on the fallthrough (neither the eFlags 0x40 prone nor 0x20 crouch bit set), via FILD
 * dword ptr [0x30539aac] (signed int -> float). Also read by PM_VerifyPronePosition
 * (0x30009700, MOV, refs=5) as the standing height it checks prone transitions against,
 * which corroborates a stance-height cvar though the exact CoD name is unproven. Never
 * written in this DLL. Retyped from the mechanical uint32_t; owner=pm_verifyproneposition
 * was the first-touching function. Naming note: role proven (fallthrough stance height);
 * the precise stance label (standing vs default) is the least certain of the three. */

/* 0x30539bc8 .data: bg_fallDamageMinHeight.value, the nonnegative lower fall-height
 * threshold below which PM_CrashLand assigns no damage. Forms the low endpoint
 * of the same landing-height envelope consumed by CG_EntityEvent. */

/* 0x30539cec .data (three readers, all in PM_UpdateViewAngles 0x3000cb14/0x3000cb4e/
 * 0x3000cbd3): the ladder yaw-snap step count, an integer number of yaw increments the
 * ladder-mount view is quantized to. PM_UpdateViewAngles takes the ladder-normal yaw
 * (vectoyaw of ps->ladderNormal), and when this count is nonzero snaps the yaw to +/-
 * this many steps (loaded as int for the compare, negated for the lower bound, FILD'd for
 * the float step size). A zero value disables ladder yaw snapping. Read-only in this DLL
 * (no writers; cvar/config owned). Retyped int32 from the mechanical uint32;
 * owner=veh_findvaliddismountspot is a wrong first-touch/size-guess artifact. Exact cvar
 * name unresolved; named by role. */

/* 0x30539e0c .data: bg_viewheight_crouched_vmCvar.integer — integer view height (units) added to the
 * muzzle Z for an entity in the crouch stance. CG_CalcMuzzlePoint (0x30048b60) is the
 * sole reader (refs=1), selecting it when the entity's eFlags has the 0x20 stance bit
 * set (FILD dword ptr [0x30539e0c]; loaded as signed int, converted to float). Never
 * written in this DLL (engine/config-owned). Retyped from the mechanical uint32_t; the
 * owner=pm_weapon_checkforchangeweapon label was the first-touching function, not the
 * identity. Exact CoD name unproven; stance role proven by the eFlags 0x20 discriminant. */

/* 0x30539f2c .data: bg_foliagesnd_maxspeed.integer — the horizontal-speed ceiling (units/s,
 * signed int) of the speed->interval interpolation in PM_FoliageSounds (0x3000c110,
 * sole reader): the lerp denominator is (bg_foliagesnd_maxspeed.integer - bg_foliagesnd_minspeed.integer)
 * and the speed fraction is clamped to <= 1.0 at this ceiling. Never written in this
 * DLL (cvar/config owned). Retyped from the mechanical uint32_t; owner=g_spawnvehicle
 * is a wrong first-touch artifact. Exact cvar name unresolved; named by role. */

/* 0x3053a030 .data refs=14: cg_effectTime, the effect subsystem's current time
 * base in ms. CG_InstallSnapshotResetEffects (0x3003c9d0) mirrors the installed
 * snapshot's serverTime here alongside cg_time; effect/trajectory code FILDs it
 * (0x30004da0 loads it as an integer -> float in several effect computations) and
 * subtracts it for elapsed-time math. Provisional role name (bg_evaluatetrajectory
 * owner label is the mechanical first-toucher, not the subsystem). */
extern uint32_t cg_effectTime;
/* 0x3053a034 .data: cg_effectAnimTime -- a third effect-subsystem time value (ms),
 * sibling of cg_effectTime (0x3053a030) and cg_effectFrameTime (0x3053a038). All
 * three are seeded to the same value by the effect reset at 0x3002df81..df8b; this
 * one is separately refreshed at 0x3003d31c (from the snapshot time at 0x3045915c).
 * BG_RunLerpFrameRate (0x30004050) uses it as the animation-blend clock: it diffs
 * it against emitter->lastEffectTime to form the elapsed-ms delta for the per-frame
 * move-speed division, then stores it into emitter->lastEffectTime. Superseded from
 * the mechanical g_data_launchitem_3053a034 (owner=launchitem was the size-guessed
 * first-touching function, not the identity). Provisional role name -- an int ms
 * effect-time clock; the exact distinction from its two siblings is unresolved. */
extern int32_t cg_effectAnimTime;
/* 0x3053a038 .data refs=6: cg_effectFrameTime, an integer time value (ms) in the
 * effect subsystem, sibling of cg_effectTime (0x3053a030). Initialized alongside
 * cg_effectTime/cg_effectAnimTime by the effect reset at 0x3002df81 and refreshed at
 * 0x30042203 (from 0x304831ac). Consumed only as int->float: BG_Player_DoControllers
 * (0x30005746/0x300057f6) FILDs it and scales it (x0.36 for the per-axis angle step,
 * x0.1 for the origin-offset step) to derive per-frame lerp magnitudes; effect code
 * at 0x300043ca/0x30004442 FILDs it likewise. Superseded from the mechanical dword
 * g_data_bg_setupsharedammoindexes_3053a038 (owner label was the first toucher).
 * Provisional role name -- an int ms time value scaled per frame; the exact source
 * symbol (absolute effect time vs. a delta) is unresolved. */
extern uint32_t cg_effectFrameTime;
/* 0x3053a040 .data: the WEAPON animation-condition value table
 * (bgAnimConditionTypes[0].values). A fixed 128-entry bg_indexed_string_t array
 * occupying exactly [0x3053a040, 0x3053a440) = 0x400 bytes (the next distinct
 * .data symbol is at 0x3053a440), i.e. 128 * 8-byte { name, hash } records.
 * Built by BG_InitWeaponStrings (0x30001500): index 0 = "none",
 * indices 1..bg_numWeapons = each registered weapon's name + BG_StringHashValue.
 * Supersedes the mechanical g_data_cmd_veh_freevehicle_3053a040/..a044 fragments
 * (which were only table[0].name and table[0].hash mislabeled after the wrong
 * first-touching owner). Declared as `extern bg_indexed_string_t
 * weaponStrings[MAX_WEAPONS]` in client_recovered.h (that header defines
 * bg_indexed_string_t and is included after this one); storage in globals.c. */
/* 0x3053a440..0x305e1f0c .data: the complete bg_static_animation_table_t.
 * Its definition and non-overlapping field aliases are in client_recovered.h,
 * after the complete animation element/list/command types are available. */
/* 0x3053a484 .data: NOT a distinct global -- this address is bgs.animationTable.entries[0] + 0x44
 * (0x3053a440 + 0x44), i.e. bg_static_animation_t.moveSpeed of table entry 0. The lone
 * reference at 0x30004230 is `MOV ECX,[EAX + 0x3053a484]` with EAX = animIndex*0x5c, which
 * is `bgs.animationTable.entries[animIndex].moveSpeed` (the %i logged by the BG_RunLerpFrameRate
 * MoveSpeed trace). The mechanical g_data_launchitem_3053a484 fragment aliased into the
 * already-declared bgs.animationTable.entries[] array (owner=launchitem was the size-guessed
 * first-toucher); removed here so the one address is not owned by two symbols. Consumers
 * index bgs.animationTable.entries[] directly. */
/* 0x3053a490 / 0x3053a494 .data: NOT distinct globals -- these are
 * bgs.animationTable.entries[0] + 0x50 / +0x54, i.e. bg_static_animation_t.flags and
 * .stateFlags of table entry 0. BG_AnimPlayerConditions (0x30004860) reads them as
 * `bgs.animationTable.entries[animNum].flags` (0x30004c80: MOV EAX,[EAX + 0x3053a490] with
 * EAX = animNum*0x5c, then TEST AL,0x10/0x20 for the SCRIPTMOVE bits) and
 * `bgs.animationTable.entries[animNum].stateFlags` (0x30004c64: MOV ECX,[EAX + 0x3053a494],
 * stored raw into ANIM_COND_MOVETYPE when nonzero); the byte-flags test at
 * 0x30004765 (`TEST byte ptr [ECX + 0x3053a490],0x30`) is the same +0x50 flags field.
 * The mechanical g_data_bg_getminspreadforweapon_3053a490 / g_data_fire_grenade_3053a494
 * fragments (owner labels were size-guessed wrong first-touchers) aliased into the
 * already-declared bgs.animationTable.entries[] array; removed here so one address is not owned by
 * two symbols. Consumers index bgs.animationTable.entries[] directly. */
/* 0x305e1f0c .data: bgs.resolvedTorsoAnimHandle -- a copy of bgs.torsoAnimHandle
 * (0x305e1f28), written by BG_FindAnimTrees (0x30005be0). Its
 * animIndex lane is read with MOVZX by BG_SetNewAnimation (0x30003a90).
 * Supersedes the mechanical g_data_bg_evaluatetrajectory_305e1f0c. */
/* 0x305e1f10 .data: bgs.resolvedLegsAnimHandle -- a copy of bgs.legsAnimHandle
 * (0x305e1f2c), written by BG_FindAnimTrees (0x30005be0). Read as a
 * animIndex lane by BG_SetNewAnimation (0x30003a90). Supersedes the
 * mechanical g_data_bg_evaluatetrajectory_305e1f10. */
/* 0x305e1f14 .data: bgs.resolvedTurningAnimHandle -- a copy of bgs.turningAnimHandle
 * (0x305e1f30), written by BG_FindAnimTrees (0x30005be0) alongside the
 * torso/legs resolved copies. Its animIndex lane is read with MOVZX at 0x30034945.
 * Supersedes the mechanical size-guess g_data_vectorlength_305e1f14. */
/* 0x305e1f18/1c on i386: callbacks installed before the animation-script load.
 * They are typed directly in bgs_t so native builds retain the
 * complete host function pointers. */
/*
 * bgs.multiplayerAnimTree (0x305e1f20, .data) — the loaded master "multiplayer"
 * anim tree. The loader's setup function (0x30005be0) resolves it by name (error
 * string "Could not find animation tree '%s'") and stores it here; the tree-
 * instance loader (0x30005c40) reads it back as the source tree passed to the
 * engine for every per-entity instance it creates. Exact source symbol unresolved;
 * named provisionally by its proven role. (0x305e1f08 holds the same handle for a
 * different consumer and is left as its own mechanical entry.)
 */
/*
 * bgs.rootAnimHandle / Torso / Legs / Turning (0x305e1f24, 0x305e1f28,
 * 0x305e1f2c, 0x305e1f30; .data) — packed scr_anim_t references for the four
 * named body-part animations ("root", "torso", "legs", "turning"). Filled by
 * BG_FindAnims (0x30005b50) via Scr_FindAnim; the loader
 * setup (0x30005be0) later copies torso/legs/turning into an adjacent resolved
 * block. Scr_FindAnim writes both uint16_t lanes; a consumer at 0x300348a9 reads
 * root's animIndex lane (MOVZX). Exact source names unresolved (role-named from the
 * strings). The mechanical owner=bg_evaluatetrajectory/pm_getjumpfactor labels are
 * the exporter's wrong first-touchers, superseded here. */
/*
 * clientInfo_t / bgs.clientinfo[] — the per-client BG animation /
 * player-state array backed at 0x305e1f34, element STRIDE 0x4d0 (1232) bytes, proven
 * by the explicit `imul 0x4d0` element index in both BG_EvaluateConditions's callers
 * (0x30002ee0) and CG_DrawPlayerLocation (0x30031280), which compute
 * `0x305e1f34 + clientNum*0x4d0`. Element 0's +0x468 sub-block is exactly
 * bgs.clientinfo[0].conditionWords[0] at 0x305e239c
 * (0x305e1f34 + 0x468). The condition block contains 11 two-word entries and
 * occupies +0x468..+0x4bf; it is a member of the 0x4d0-byte player record, not
 * a separately allocated 154-condition table.
 *
 * IMPORTANT — this element type is deliberately SEPARATE from playerState_t: that
 * type is modelled at ~0x62c bytes because several PM_/BG consumers reach it via a
 * DIFFERENT pointer (pm->ps, a larger playerState) and read fields at
 * +0x574/+0x628, which lie beyond this array's 0x4d0 stride. Using playerState_t
 * (sizeof 0x62c) as this array's element would make C compute the wrong element
 * address (imul 0x62c instead of 0x4d0). The two names sharing offsets is the known
 * red-flag divergence documented on playerState_t; the client stride 0x4d0 is
 * authoritative here, so this array gets its own exactly-0x4d0 element type. The
 * common declaration is provided by client_info_types.h.
 *
 * Supersedes the mechanical [0][0] fragment (owner=clearregistereditems). CONSOLIDATION
 * THREAD: the mechanical g_data_* fragments in 0x305e1f40.. are interior columns of
 * element 0 of THIS array; fold them in as their own consumers are reconstructed.
 * Outer extent (MAX_CLIENTS) is not proven; storage sized to the standard MP client
 * count (64) in globals.c, mirroring per-client conditionWords. */
/* 0x305e1f60 is an interior column of bgs.clientinfo[] element 0 (element base
 * 0x305e1f34, so 0x305e1f60 == +0x2c). All 12 refs access it as `[reg + 0x305e1f60]`
 * where reg == clientNum*0x4d0, confirming it is the per-client team field folded
 * into the shared clientInfo_t (consumed by CG_EmitLocalTeamBackground,
 * 0x300316b0).
 * The mechanical g_data_cg_asset_parse_305e1f60 fragment is removed to avoid two
 * symbols aliasing the same storage. */
/* 0x305e2334 / 0x305e2338 are interior columns of bgs.clientinfo[] element 0:
 * element base 0x305e1f34, so 0x305e2334 == +0x400 (gunHandLeft) and 0x305e2338 ==
 * +0x404 (dobjNeedsUpdate). Folded into the shared clientInfo_t (consumed by
 * CG_SetGunHandFromNotetracks, 0x3001f760); the mechanical
 * g_data_pm_beginaimdownsight_305e2334/_305e2338 fragments are removed to avoid
 * aliasing the same storage. */
/* 0x305e23c4 / 0x305e23c8 are interior columns of bgs.clientinfo[] element 0
 * (element base 0x305e1f34): 0x305e23c4 == +0x490, 0x305e23c8 == +0x494. They are the
 * two words of clientInfo_t.conditionWords[ANIM_COND_MOVETYPE][2] (folded into that struct
 * declaration; consumers BG_ExecuteCommand 0x300031d0 and the anim-slot writer
 * 0x30004c7a,
 * both indexing base + client*0x4d0). The mechanical g_data_turret_think_305e23c4/
 * _305e23c8 fragments are removed to avoid two symbols aliasing the same storage. */
/* 0x305e23f8 is the animTree member of bgs.clientinfo[0]; the
 * loader advances by sizeof(clientInfo_t), so no separate array exists. */

extern vmCvar_t cg_ignore;
extern vmCvar_t cg_drawCrosshairPickups;
extern vmCvar_t cg_drawRewards;
extern vmCvar_t cg_hudStanceFlash_r;
extern vmCvar_t cg_hudStanceFlash_g;
extern vmCvar_t cg_hudStanceFlash_b;
extern vmCvar_t cg_weaponCycleDelay;
extern vmCvar_t cg_noplayeranims;
extern vmCvar_t cg_drawTeamOverlay;
extern vmCvar_t cg_hudFiles;
extern vmCvar_t cg_currentSelectedPlayerName;
extern vmCvar_t cg_deadbodyque;
extern vmCvar_t g_gametype;
extern vmCvar_t cg_animState;
extern vmCvar_t cl_waitForFire;
extern vmCvar_t cg_autoscreenshot;
extern vmCvar_t cg_autodemo;
extern vmCvar_t cg_showdemoname;
extern vmCvar_t version;
extern vmCvar_t r_optimize;
extern vmCvar_t r_optimizeXModels;
extern vmCvar_t cg_atmos;
extern vmCvar_t cg_atmosDense;


/* cg_cvarTable-proven vmCvar objects. Consumers access .integer or .value
 * directly; these objects do not have secondary compatibility names. */
extern vmCvar_t cg_drawGun_vmCvar;
extern vmCvar_t cg_cursorHints_vmCvar;
extern vmCvar_t cg_hintFadeTime_vmCvar;
extern vmCvar_t cg_fov_vmCvar;
extern vmCvar_t cg_letterbox_vmCvar;
extern vmCvar_t cg_stereoSeparation_vmCvar;
extern vmCvar_t cg_shadows_vmCvar;
extern vmCvar_t cg_draw2D_vmCvar;
extern vmCvar_t cg_drawStatus_vmCvar;
extern vmCvar_t cg_drawFPS_vmCvar;
extern vmCvar_t cg_drawSoundOverlay_vmCvar;
extern vmCvar_t cg_drawScriptUsage_vmCvar;
extern vmCvar_t cg_drawShader_vmCvar;
extern vmCvar_t cg_drawSnapshot_vmCvar;
extern vmCvar_t cg_drawCrosshair_vmCvar;
extern vmCvar_t cg_drawCrosshairNames_vmCvar;
extern vmCvar_t sv_night_vmCvar;
extern vmCvar_t cg_hudAlpha_vmCvar;
extern vmCvar_t cg_hudCompassSize_vmCvar;
extern vmCvar_t cg_hudCompassMaxRange_vmCvar;
extern vmCvar_t cg_hudCompassMinRange_vmCvar;
extern vmCvar_t cg_hudCompassMinRadius_vmCvar;
extern vmCvar_t cg_hudCompassSpringyPointers_vmCvar;
extern vmCvar_t cg_hudObjectiveMinHeight_vmCvar;
extern vmCvar_t cg_hudObjectiveMaxHeight_vmCvar;
extern vmCvar_t cg_hudObjectiveMaxRange_vmCvar;
extern vmCvar_t cg_hudObjectiveMinAlpha_vmCvar;
extern vmCvar_t cg_hudStanceHintPrints_vmCvar;
extern vmCvar_t cg_hudDamageIconWidth_vmCvar;
extern vmCvar_t cg_hudDamageIconHeight_vmCvar;
extern vmCvar_t cg_hudDamageIconOffset_vmCvar;
extern vmCvar_t cg_hudDamageIconTime_vmCvar;
extern vmCvar_t cg_hudDamageIconInScope_vmCvar;
extern vmCvar_t cg_weaponSelect_vmCvar;
extern vmCvar_t cg_crosshairAlpha_vmCvar;
extern vmCvar_t cg_crosshairAlphaMin_vmCvar;
extern vmCvar_t cg_crosshairDynamic_vmCvar;
extern vmCvar_t cg_brass_vmCvar;
extern vmCvar_t cg_marks_vmCvar;
extern vmCvar_t cg_lagometer_vmCvar;
extern vmCvar_t cg_railTrailTime_vmCvar;
extern vmCvar_t cg_gunX_vmCvar;
extern vmCvar_t cg_gunY_vmCvar;
extern vmCvar_t cg_gunZ_vmCvar;
extern vmCvar_t cg_gun_move_f_vmCvar;
extern vmCvar_t cg_gun_move_r_vmCvar;
extern vmCvar_t cg_gun_move_u_vmCvar;
extern vmCvar_t cg_gun_ofs_f_vmCvar;
extern vmCvar_t cg_gun_ofs_r_vmCvar;
extern vmCvar_t cg_gun_ofs_u_vmCvar;
extern vmCvar_t cg_gun_move_rate_vmCvar;
extern vmCvar_t cg_gun_move_minspeed_vmCvar;
extern vmCvar_t cg_centertime_vmCvar;
extern vmCvar_t cg_skybox_vmCvar;
extern vmCvar_t cg_debugposition_vmCvar;
extern vmCvar_t cg_debugevents_vmCvar;
extern vmCvar_t cg_nopredict_vmCvar;
extern vmCvar_t cg_showmiss_vmCvar;
extern vmCvar_t cg_footsteps_vmCvar;
extern vmCvar_t cg_tracerchance_vmCvar;
extern vmCvar_t cg_tracerchancelmg_vmCvar;
extern vmCvar_t cg_tracerSpeed_vmCvar;
extern vmCvar_t cg_tracerlength_vmCvar;
extern vmCvar_t cg_tracerwidth_vmCvar;
extern vmCvar_t cg_tracerlengthlmg_vmCvar;
extern vmCvar_t cg_tracerwidthlmg_vmCvar;
extern vmCvar_t cg_thirdPersonRange_vmCvar;
extern vmCvar_t cg_thirdPersonAngle_vmCvar;
extern vmCvar_t cg_thirdPerson_vmCvar;
extern vmCvar_t cg_chatHeight_vmCvar;
extern vmCvar_t cg_predictItems_vmCvar;
extern vmCvar_t cg_stats_vmCvar;
extern vmCvar_t pmove_fixed_vmCvar;
extern vmCvar_t cl_stanceTemp_vmCvar;
extern vmCvar_t cg_noTaunt_vmCvar;
extern vmCvar_t cg_voiceSpriteTime_vmCvar;
extern vmCvar_t cg_teamChatsOnly_vmCvar;
extern vmCvar_t cg_noVoiceChats_vmCvar;
extern vmCvar_t cg_noVoiceText_vmCvar;
extern vmCvar_t cl_paused_vmCvar;
extern vmCvar_t g_synchronousClients_vmCvar;
extern vmCvar_t cg_norender_vmCvar;
extern vmCvar_t cg_dumpAnims_vmCvar;
extern vmCvar_t developer_vmCvar;
extern vmCvar_t con_minicon_vmCvar;
extern vmCvar_t cg_subtitles_vmCvar;
extern vmCvar_t cg_subtitleMinTime_vmCvar;
extern vmCvar_t cg_subtitleWidth_vmCvar;
extern vmCvar_t cg_gameMessageWidth_vmCvar;
extern vmCvar_t cg_gameBoldMessageWidth_vmCvar;
extern vmCvar_t cl_languagewarnings_vmCvar;
extern vmCvar_t cl_languagewarningsaserrors_vmCvar;
extern vmCvar_t cg_scoreboardScrollStep_vmCvar;
extern vmCvar_t cg_descriptiveText_vmCvar;
extern vmCvar_t bg_viewheight_crouched_vmCvar;
extern vmCvar_t bg_viewheight_prone_vmCvar;
extern vmCvar_t bg_ladder_yawcap;
extern vmCvar_t bg_lmg_yawcap;
extern vmCvar_t bg_debugWeaponAnim;
extern vmCvar_t bg_debugWeaponState;
extern vmCvar_t bg_debugWeaponMessages_vmCvar;
extern vmCvar_t cg_bobAmplitudeStanding_vmCvar;
extern vmCvar_t cg_bobAmplitudeDucked_vmCvar;
extern vmCvar_t cg_bobAmplitudeProne_vmCvar;
extern vmCvar_t cg_bobMax_vmCvar;
extern vmCvar_t cg_debuganim_vmCvar;
extern vmCvar_t bg_swingSpeed_vmCvar;
extern vmCvar_t cg_blood_vmCvar;
extern vmCvar_t cg_announcerSounds_vmCvar;
extern vmCvar_t cg_vehicletrails_vmCvar;

/* Additional consolidated cg_cvarTable objects. */

/* These entries were identified after the main contiguous table pass. */
extern vmCvar_t cg_crosshairNoGun_vmCvar;
extern vmCvar_t cg_errordecay_vmCvar;
extern vmCvar_t cg_tracernightscale_vmCvar;
extern vmCvar_t cg_chatTime_vmCvar;
extern vmCvar_t timescale_vmCvar;
extern vmCvar_t cl_stance_vmCvar;
extern vmCvar_t cg_currentSelectedPlayer_vmCvar;
extern vmCvar_t bg_prone_yawcap;

#endif /* CLIENT_GLOBALS_H */
