#ifndef QCOMMON_CLIENT_INFO_TYPES_H
#define QCOMMON_CLIENT_INFO_TYPES_H

#include "asset_type_names.h"
#include "q_shared_types.h"
#include "q_vector_types.h"

#include <stddef.h>
#include <stdint.h>

/* Restart toggle carried alongside the animation-table index in playerState_t
 * legsAnim, torsoAnim, and weaponAnim, and in clientInfo_t animation-slot
 * words. Writers flip it when restarting an animation; readers clear it before
 * indexing or comparing. Windows cgame/game use 0x200/0xfffffdff, Linux game
 * uses the same immediates, and the Mac game module clears the corresponding
 * PowerPC bit. Canonical Quake3 calls this ANIM_TOGGLEBIT; CoD widened its value
 * from 0x100 to 0x200. */
#define ANIM_TOGGLEBIT UINT32_C(0x200)

enum {
    CLIENT_INFO_NAME_SIZE = 32,
    CLIENT_INFO_MODEL_NAME_SIZE = 64,
    CLIENT_INFO_ATTACHMENT_COUNT = 6,
    CLIENT_INFO_SPINE_CONTROL_COUNT = 6,
    BG_ANIM_CONDITION_WORD_COUNT = 2,
    BG_ANIM_CONDITION_BIT_COUNT = BG_ANIM_CONDITION_WORD_COUNT * 32
};

/*
 * Fixed rows of clientInfo_t.conditionWords and bgAnimConditionTypes.  The
 * ordering is proved independently by the initialized condition-name tables
 * in the Windows cgame and the Windows/Linux game modules.
 */
typedef enum bg_anim_condition_index_e {
    ANIM_COND_WEAPON = 0,
    ANIM_COND_WEAPONCLASS = 1,
    ANIM_COND_MOUNTED = 2,
    ANIM_COND_VEHICLE = 3,
    ANIM_COND_VEHICLE_MOTION = 4,
    ANIM_COND_MOVETYPE = 5,
    ANIM_COND_UNDERHAND = 6,
    ANIM_COND_CROUCHING = 7,
    ANIM_COND_FIRING = 8,
    ANIM_COND_WEAPON_POSITION = 9,
    ANIM_COND_STRAFING = 10,
    ANIM_COND_COUNT = 11
} bg_anim_condition_index_t;

/*
 * One of the two 0x30-byte animation slots embedded in clientInfo_t.  The
 * Windows cgame/game and Linux game bodies use the same slot offsets.  The
 * first 0x10 bytes have legs-specific and torso-specific meanings; the
 * remaining animation state is common to both slots.
 */
typedef struct bg_anim_slot_s {
    union {
        struct {
            float yawAngle;
            qboolean yawActive;
            int32_t animTimer;
            uint32_t animIndex;
        } legs;
        struct {
            float yawAngle;
            qboolean yawActive;
            float leanAngle;
            qboolean leanActive;
        } torso;
    } controlState;
    uint32_t animationWord;
    uint32_t animationOffset;
    int32_t blendTime;
    vec3_t lastOrigin;
    float animRate;
    int32_t lastUpdateTime;
} bg_anim_slot_t;

/*
 * Shared BG client animation/DObj record.
 *
 * Authoritative i386 bodies agree on the 0x4d0 stride and the model,
 * attachment, animation-slot, controller, condition, and DObj-tail offsets.
 * Windows cgame uses +0x004 as its per-snapshot presence latch, while the game
 * modules use that same word as pmType; those module-local meanings are the
 * sole intentional module-specific union in the common physical layout.
 *
 * Windows cgame CG_TransitionEntity copies the complete 0x4d0 live-player row
 * into a corpse row and restores only animTree.  The corpse/model-effect types
 * formerly maintained by the reconstruction were therefore partial overlay
 * views of this record, not separate original structures.
 */
typedef struct clientInfo_s {
    int32_t infoValid;                              /* +0x000 */
    union clientInfoModuleState_u {
        int32_t active;                             /* +0x004, cgame */
        int32_t pmType;                             /* +0x004, game */
    } moduleState;
    int32_t clientNum;                              /* +0x008, phase seed/index */
    char name[CLIENT_INFO_NAME_SIZE];                /* +0x00c */
    int32_t team;                                   /* +0x02c */
    int32_t obituaryTeam;                           /* +0x030 */
    int32_t score;                                  /* +0x034 */
    int32_t location;                               /* +0x038 */
    int32_t health;                                 /* +0x03c */
    char modelName[CLIENT_INFO_MODEL_NAME_SIZE];    /* +0x040 */
    char attachModelNames[CLIENT_INFO_ATTACHMENT_COUNT]
                         [CLIENT_INFO_MODEL_NAME_SIZE]; /* +0x080 */
    char attachTagNames[CLIENT_INFO_ATTACHMENT_COUNT]
                       [CLIENT_INFO_MODEL_NAME_SIZE];   /* +0x200 */

    float legsYawAngle;                             /* +0x380 */
    qboolean legsYawActive;                         /* +0x384 */
    int32_t legsTimer;                              /* +0x388 */
    uint32_t legsAnim;                              /* +0x38c */
    uint32_t legsAnimWord;                          /* +0x390 */
    uint32_t legsAnimEntryWord;                     /* +0x394 */
    int32_t legsAnimBlendTime;                      /* +0x398 */
    vec3_t legsMoveOrigin;                          /* +0x39c */
    float legsAnimMoveRate;                         /* +0x3a8 */
    int32_t legsAnimMoveTime;                       /* +0x3ac */

    float torsoYawAngle;                            /* +0x3b0 */
    qboolean torsoYawActive;                        /* +0x3b4 */
    float leanAngle;                                /* +0x3b8 */
    qboolean leanActive;                            /* +0x3bc */
    uint32_t torsoAnimWord;                         /* +0x3c0 */
    uint32_t torsoAnimEntryWord;                    /* +0x3c4 */
    int32_t torsoAnimBlendTime;                     /* +0x3c8 */
    vec3_t torsoMoveOrigin;                         /* +0x3cc */
    float torsoAnimMoveRate;                        /* +0x3d8 */
    int32_t torsoAnimMoveTime;                      /* +0x3dc */

    float leanAmount;                               /* +0x3e0 */
    float leanFraction;                             /* +0x3e4 */
    float viewPitch;                                /* +0x3e8 */
    float viewYaw;                                  /* +0x3ec */
    float viewRoll;                                 /* +0x3f0 */
    vec3_t turretOverrideAngles;                     /* +0x3f4 */
    int32_t gunHandLeft;                            /* +0x400, left-hand tag */
    int32_t dobjNeedsUpdate;                        /* +0x404, rebuild dirty */

    vec3_t controllerAngles[CLIENT_INFO_SPINE_CONTROL_COUNT]; /* +0x408 */
    vec3_t localTagAngles;                          /* +0x450 */
    vec3_t localTagOffset;                          /* +0x45c */
    uint32_t conditionWords[ANIM_COND_COUNT]
                           [BG_ANIM_CONDITION_WORD_COUNT]; /* +0x468 */
    int32_t animTransitionTime;                     /* +0x4c0 */
    XAnimTree *animTree;                            /* i386 +0x4c4 */
    int32_t dobjSavedModel;                         /* i386 +0x4c8 */
    uint8_t dobjVersion;                            /* i386 +0x4cc */
    uint8_t padding4cd[3];                          /* i386 +0x4cd */
} clientInfo_t;

#define CLIENT_INFO_LAYOUT_ASSERT(name, expression) \
    typedef char name[(expression) ? 1 : -1]

CLIENT_INFO_LAYOUT_ASSERT(bg_anim_slot_size,
                          sizeof(bg_anim_slot_t) == 0x30);
CLIENT_INFO_LAYOUT_ASSERT(bg_anim_slot_animation_word_offset,
                          offsetof(bg_anim_slot_t, animationWord) == 0x10);
CLIENT_INFO_LAYOUT_ASSERT(bg_anim_slot_animation_offset_offset,
                          offsetof(bg_anim_slot_t, animationOffset) == 0x14);
CLIENT_INFO_LAYOUT_ASSERT(bg_anim_slot_blend_time_offset,
                          offsetof(bg_anim_slot_t, blendTime) == 0x18);
CLIENT_INFO_LAYOUT_ASSERT(bg_anim_slot_last_origin_offset,
                          offsetof(bg_anim_slot_t, lastOrigin) == 0x1c);
CLIENT_INFO_LAYOUT_ASSERT(bg_anim_slot_anim_rate_offset,
                          offsetof(bg_anim_slot_t, animRate) == 0x28);
CLIENT_INFO_LAYOUT_ASSERT(bg_anim_slot_last_update_time_offset,
                          offsetof(bg_anim_slot_t, lastUpdateTime) == 0x2c);

CLIENT_INFO_LAYOUT_ASSERT(client_info_valid_offset,
                          offsetof(clientInfo_t, infoValid) == 0x000);
CLIENT_INFO_LAYOUT_ASSERT(client_info_module_state_offset,
                          offsetof(clientInfo_t, moduleState) == 0x004);
CLIENT_INFO_LAYOUT_ASSERT(client_info_client_num_offset,
                          offsetof(clientInfo_t, clientNum) == 0x008);
CLIENT_INFO_LAYOUT_ASSERT(client_info_name_offset,
                          offsetof(clientInfo_t, name) == 0x00c);
CLIENT_INFO_LAYOUT_ASSERT(client_info_team_offset,
                          offsetof(clientInfo_t, team) == 0x02c);
CLIENT_INFO_LAYOUT_ASSERT(client_info_obituary_team_offset,
                          offsetof(clientInfo_t, obituaryTeam) == 0x030);
CLIENT_INFO_LAYOUT_ASSERT(client_info_score_offset,
                          offsetof(clientInfo_t, score) == 0x034);
CLIENT_INFO_LAYOUT_ASSERT(client_info_location_offset,
                          offsetof(clientInfo_t, location) == 0x038);
CLIENT_INFO_LAYOUT_ASSERT(client_info_health_offset,
                          offsetof(clientInfo_t, health) == 0x03c);
CLIENT_INFO_LAYOUT_ASSERT(client_info_model_name_offset,
                          offsetof(clientInfo_t, modelName) == 0x040);
CLIENT_INFO_LAYOUT_ASSERT(client_info_attach_model_names_offset,
                          offsetof(clientInfo_t, attachModelNames) == 0x080);
CLIENT_INFO_LAYOUT_ASSERT(client_info_attach_tag_names_offset,
                          offsetof(clientInfo_t, attachTagNames) == 0x200);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_yaw_angle_offset,
                          offsetof(clientInfo_t, legsYawAngle) == 0x380);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_yaw_active_offset,
                          offsetof(clientInfo_t, legsYawActive) == 0x384);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_timer_offset,
                          offsetof(clientInfo_t, legsTimer) == 0x388);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_anim_offset,
                          offsetof(clientInfo_t, legsAnim) == 0x38c);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_anim_word_offset,
                          offsetof(clientInfo_t, legsAnimWord) == 0x390);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_anim_entry_word_offset,
                          offsetof(clientInfo_t, legsAnimEntryWord) == 0x394);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_anim_blend_time_offset,
                          offsetof(clientInfo_t, legsAnimBlendTime) == 0x398);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_move_origin_offset,
                          offsetof(clientInfo_t, legsMoveOrigin) == 0x39c);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_anim_move_rate_offset,
                          offsetof(clientInfo_t, legsAnimMoveRate) == 0x3a8);
CLIENT_INFO_LAYOUT_ASSERT(client_info_legs_anim_move_time_offset,
                          offsetof(clientInfo_t, legsAnimMoveTime) == 0x3ac);
CLIENT_INFO_LAYOUT_ASSERT(client_info_torso_yaw_angle_offset,
                          offsetof(clientInfo_t, torsoYawAngle) == 0x3b0);
CLIENT_INFO_LAYOUT_ASSERT(client_info_torso_yaw_active_offset,
                          offsetof(clientInfo_t, torsoYawActive) == 0x3b4);
CLIENT_INFO_LAYOUT_ASSERT(client_info_lean_angle_offset,
                          offsetof(clientInfo_t, leanAngle) == 0x3b8);
CLIENT_INFO_LAYOUT_ASSERT(client_info_lean_active_offset,
                          offsetof(clientInfo_t, leanActive) == 0x3bc);
CLIENT_INFO_LAYOUT_ASSERT(client_info_torso_anim_word_offset,
                          offsetof(clientInfo_t, torsoAnimWord) == 0x3c0);
CLIENT_INFO_LAYOUT_ASSERT(client_info_torso_anim_entry_word_offset,
                          offsetof(clientInfo_t, torsoAnimEntryWord) == 0x3c4);
CLIENT_INFO_LAYOUT_ASSERT(client_info_torso_anim_blend_time_offset,
                          offsetof(clientInfo_t, torsoAnimBlendTime) == 0x3c8);
CLIENT_INFO_LAYOUT_ASSERT(client_info_torso_move_origin_offset,
                          offsetof(clientInfo_t, torsoMoveOrigin) == 0x3cc);
CLIENT_INFO_LAYOUT_ASSERT(client_info_torso_anim_move_rate_offset,
                          offsetof(clientInfo_t, torsoAnimMoveRate) == 0x3d8);
CLIENT_INFO_LAYOUT_ASSERT(client_info_torso_anim_move_time_offset,
                          offsetof(clientInfo_t, torsoAnimMoveTime) == 0x3dc);
CLIENT_INFO_LAYOUT_ASSERT(client_info_lean_amount_offset,
                          offsetof(clientInfo_t, leanAmount) == 0x3e0);
CLIENT_INFO_LAYOUT_ASSERT(client_info_lean_fraction_offset,
                          offsetof(clientInfo_t, leanFraction) == 0x3e4);
CLIENT_INFO_LAYOUT_ASSERT(client_info_view_pitch_offset,
                          offsetof(clientInfo_t, viewPitch) == 0x3e8);
CLIENT_INFO_LAYOUT_ASSERT(client_info_view_yaw_offset,
                          offsetof(clientInfo_t, viewYaw) == 0x3ec);
CLIENT_INFO_LAYOUT_ASSERT(client_info_view_roll_offset,
                          offsetof(clientInfo_t, viewRoll) == 0x3f0);
CLIENT_INFO_LAYOUT_ASSERT(client_info_turret_override_angles_offset,
                          offsetof(clientInfo_t, turretOverrideAngles) == 0x3f4);
CLIENT_INFO_LAYOUT_ASSERT(client_info_gun_hand_left_offset,
                          offsetof(clientInfo_t, gunHandLeft) == 0x400);
CLIENT_INFO_LAYOUT_ASSERT(client_info_dobj_needs_update_offset,
                          offsetof(clientInfo_t, dobjNeedsUpdate) == 0x404);
CLIENT_INFO_LAYOUT_ASSERT(client_info_controller_angles_offset,
                          offsetof(clientInfo_t, controllerAngles) == 0x408);
CLIENT_INFO_LAYOUT_ASSERT(client_info_local_tag_angles_offset,
                          offsetof(clientInfo_t, localTagAngles) == 0x450);
CLIENT_INFO_LAYOUT_ASSERT(client_info_local_tag_offset_offset,
                          offsetof(clientInfo_t, localTagOffset) == 0x45c);
CLIENT_INFO_LAYOUT_ASSERT(client_info_condition_words_offset,
                          offsetof(clientInfo_t, conditionWords) == 0x468);
CLIENT_INFO_LAYOUT_ASSERT(client_info_anim_transition_time_offset,
                          offsetof(clientInfo_t, animTransitionTime) == 0x4c0);

#if UINTPTR_MAX == UINT32_MAX
CLIENT_INFO_LAYOUT_ASSERT(client_info_anim_tree_offset,
                          offsetof(clientInfo_t, animTree) == 0x4c4);
CLIENT_INFO_LAYOUT_ASSERT(client_info_dobj_saved_model_offset,
                          offsetof(clientInfo_t, dobjSavedModel) == 0x4c8);
CLIENT_INFO_LAYOUT_ASSERT(client_info_dobj_version_offset,
                          offsetof(clientInfo_t, dobjVersion) == 0x4cc);
CLIENT_INFO_LAYOUT_ASSERT(client_info_size,
                          sizeof(clientInfo_t) == 0x4d0);
#elif UINTPTR_MAX == UINT64_MAX
CLIENT_INFO_LAYOUT_ASSERT(client_info_native_anim_tree_offset,
                          offsetof(clientInfo_t, animTree) == 0x4c8);
CLIENT_INFO_LAYOUT_ASSERT(client_info_native_dobj_saved_model_offset,
                          offsetof(clientInfo_t, dobjSavedModel) == 0x4d0);
CLIENT_INFO_LAYOUT_ASSERT(client_info_native_dobj_version_offset,
                          offsetof(clientInfo_t, dobjVersion) == 0x4d4);
CLIENT_INFO_LAYOUT_ASSERT(client_info_native_size,
                          sizeof(clientInfo_t) == 0x4d8);
#endif

#undef CLIENT_INFO_LAYOUT_ASSERT

#endif
