#ifndef QCOMMON_PMOVE_TYPES_H
#define QCOMMON_PMOVE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "entity_event_types.h"
#include "player_state_types.h"
#include "q_collision_types.h"
#include "q_shared_types.h"
#include "q_vector_types.h"

/* One row of the shared PM view-height interpolation tables.  The Windows
 * cgame/game and Linux game implementations all advance these tables with a
 * 12-byte stride and read the same signed percentage, binary32 height, and
 * signed origin adjustment fields. */
typedef struct pmLerpEntry_s {
    int32_t percent;
    float height;
    int32_t originAdjust;
} pmLerpEntry_t;

/* Effective stance selected from the current view-height target. Windows
 * cgame movement and the Windows/Linux game modules all use 0/1/2 for
 * standing/prone/crouched respectively. */
typedef enum effectiveStance_e {
    EFFECTIVE_STANCE_STAND = 0,
    EFFECTIVE_STANCE_PRONE = 1,
    EFFECTIVE_STANCE_CROUCH = 2
} effectiveStance_t;

enum {
    PM_LERP_TABLE_END = -1,

    /* Capacity of the collision-impact list embedded in pmove_t. */
    PM_MAX_TOUCH_ENTS = 32,

    /* Shared client/game pmove_msec cvar range. Both original Windows
     * modules clamp the setting to 8..33; the Linux game module agrees. */
    PMOVE_MSEC_MIN = 8,
    PMOVE_MSEC_MAX = 33
};

/* The shared footstep trace clears CONTENTS_BODY and the low water contents
 * bit, then substitutes surface type 13 when no typed surface was hit.
 * Windows cgame/game and Linux game use the same values. */
enum {
    PM_FOOTSTEP_TRACE_MASK_CLEAR = 0x02010000u,
    PM_FOOTSTEP_DEFAULT_SURFACE = 13
};

#define PM_LERP_LAYOUT_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]
PM_LERP_LAYOUT_ASSERT(q_pm_lerp_entry_stride, sizeof(pmLerpEntry_t) == 0x0c);
PM_LERP_LAYOUT_ASSERT(q_pm_lerp_entry_percent_offset, offsetof(pmLerpEntry_t, percent) == 0x00);
PM_LERP_LAYOUT_ASSERT(q_pm_lerp_entry_height_offset, offsetof(pmLerpEntry_t, height) == 0x04);
PM_LERP_LAYOUT_ASSERT(q_pm_lerp_entry_origin_adjust_offset, offsetof(pmLerpEntry_t, originAdjust) == 0x08);
#undef PM_LERP_LAYOUT_ASSERT

typedef void (*pm_trace_fn_t)(trace_t *results, const float *start, const float *mins, const float *maxs, const float *end,
                              int passEntityNum, int traceType);
typedef int (*pm_entity_type_fn_t)(int entityNum);
typedef int (*pm_pointcontents_fn_t)(const float *point, int passEntityNum, int contentMask);

/* Shared BG movement context. Windows cgame/game and the Linux game module
 * agree on the complete 0x11c-byte i386 record, including all three trace
 * callbacks. Their PM_SetWaterLevel bodies (cgame 0x3000a7a0, Windows game
 * 0x2000a560, Linux game 0x00026cf1) clear +0xf0/+0xf1, write the bottom
 * point-contents byte to +0xf0, and advance +0xf1 through water depths 1..3.
 * The former server-only "overhead" spelling was therefore a reconstruction
 * artifact. Native pointers widen normally. */
typedef struct pmove_s {
    playerState_t *ps;
    usercmd_t command;
    usercmd_t oldCommand;
    int32_t traceMask;
    int32_t debugMove;
    vec3_t viewClampTargetAngles;
    vec3_t viewClampMaxDeltas;
    int32_t numtouch;
    int32_t impactEntityNums[PM_MAX_TOUCH_ENTS];
    vec3_t mins;
    vec3_t maxs;
    uint8_t watertype;
    uint8_t waterlevel;
    uint8_t paddingF2[2];
    float horizontalSpeed;
    int32_t pmove_msec_min;
    int32_t pmove_msec_max;
    int32_t weaponAnimscriptEnabled;
    pm_trace_fn_t trace;
    pm_trace_fn_t trace2;
    pm_trace_fn_t trace3;
    pm_pointcontents_fn_t pointContents;
    pm_entity_type_fn_t entityType;
    int32_t adsInputBlocked;
} pmove_t;

/*
 * Per-invocation movement locals.  The Windows cgame object at
 * 0x30539580 and the Linux game object at 0x002023a0 have the same complete
 * 0x8c-byte i386 layout.  The former game-only names `groundPlane`,
 * `waterlevel`, and `waterlevel2` at +0x2c/+0x30/+0x34 described their uses
 * incorrectly: those words are the canonical walking/ground-plane flags and
 * the CoD near-ground/lift-contact flag used by both modules.
 */
typedef struct pml_s {
    vec3_t forward;                       /* +0x00 */
    vec3_t right;                         /* +0x0c */
    vec3_t up;                            /* +0x18 */
    float frametime;                      /* +0x24 */
    int32_t msec;                         /* +0x28 */
    int32_t walking;                      /* +0x2c */
    int32_t groundPlane;                  /* +0x30 */
    int32_t groundLiftFlag;               /* +0x34 */
    trace_t groundTrace;                  /* +0x38 */
    float maxClipImpact;                  /* +0x68 */
    vec3_t previousOrigin;                /* +0x6c */
    vec3_t previousVelocity;              /* +0x78 */
    int32_t previousWaterLevel;           /* +0x84 */
    const weaponInfo_t *weaponInfo;       /* +0x88 */
} pml_t;

#if UINTPTR_MAX == UINT32_MAX
#define PMOVE_LAYOUT_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]
PMOVE_LAYOUT_ASSERT(q_pmove_command_offset, offsetof(pmove_t, command) == 0x004);
PMOVE_LAYOUT_ASSERT(q_pmove_old_command_offset, offsetof(pmove_t, oldCommand) == 0x01c);
PMOVE_LAYOUT_ASSERT(q_pmove_trace_mask_offset, offsetof(pmove_t, traceMask) == 0x034);
PMOVE_LAYOUT_ASSERT(q_pmove_debug_move_offset, offsetof(pmove_t, debugMove) == 0x038);
PMOVE_LAYOUT_ASSERT(q_pmove_view_clamp_target_offset, offsetof(pmove_t, viewClampTargetAngles) == 0x03c);
PMOVE_LAYOUT_ASSERT(q_pmove_numtouch_offset, offsetof(pmove_t, numtouch) == 0x054);
PMOVE_LAYOUT_ASSERT(q_pmove_touch_list_offset, offsetof(pmove_t, impactEntityNums) == 0x058);
PMOVE_LAYOUT_ASSERT(q_pmove_mins_offset, offsetof(pmove_t, mins) == 0x0d8);
PMOVE_LAYOUT_ASSERT(q_pmove_maxs_offset, offsetof(pmove_t, maxs) == 0x0e4);
PMOVE_LAYOUT_ASSERT(q_pmove_watertype_offset, offsetof(pmove_t, watertype) == 0x0f0);
PMOVE_LAYOUT_ASSERT(q_pmove_waterlevel_offset, offsetof(pmove_t, waterlevel) == 0x0f1);
PMOVE_LAYOUT_ASSERT(q_pmove_horizontal_speed_offset, offsetof(pmove_t, horizontalSpeed) == 0x0f4);
PMOVE_LAYOUT_ASSERT(q_pmove_weapon_animscript_offset, offsetof(pmove_t, weaponAnimscriptEnabled) == 0x100);
PMOVE_LAYOUT_ASSERT(q_pmove_trace_offset, offsetof(pmove_t, trace) == 0x104);
PMOVE_LAYOUT_ASSERT(q_pmove_point_contents_offset, offsetof(pmove_t, pointContents) == 0x110);
PMOVE_LAYOUT_ASSERT(q_pmove_entity_type_offset, offsetof(pmove_t, entityType) == 0x114);
PMOVE_LAYOUT_ASSERT(q_pmove_ads_blocked_offset, offsetof(pmove_t, adsInputBlocked) == 0x118);
PMOVE_LAYOUT_ASSERT(q_pmove_extent, sizeof(pmove_t) == 0x11c);
PMOVE_LAYOUT_ASSERT(q_pml_walking_offset, offsetof(pml_t, walking) == 0x2c);
PMOVE_LAYOUT_ASSERT(q_pml_ground_plane_offset, offsetof(pml_t, groundPlane) == 0x30);
PMOVE_LAYOUT_ASSERT(q_pml_ground_lift_flag_offset, offsetof(pml_t, groundLiftFlag) == 0x34);
PMOVE_LAYOUT_ASSERT(q_pml_ground_trace_offset, offsetof(pml_t, groundTrace) == 0x38);
PMOVE_LAYOUT_ASSERT(q_pml_max_clip_impact_offset, offsetof(pml_t, maxClipImpact) == 0x68);
PMOVE_LAYOUT_ASSERT(q_pml_previous_water_level_offset, offsetof(pml_t, previousWaterLevel) == 0x84);
PMOVE_LAYOUT_ASSERT(q_pml_weapon_info_offset, offsetof(pml_t, weaponInfo) == 0x88);
PMOVE_LAYOUT_ASSERT(q_pml_extent, sizeof(pml_t) == 0x8c);
#undef PMOVE_LAYOUT_ASSERT
#endif

#endif
