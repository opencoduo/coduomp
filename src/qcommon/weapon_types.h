#ifndef QCOMMON_WEAPON_TYPES_H
#define QCOMMON_WEAPON_TYPES_H

#include "info.h"
#include "q_vector_types.h"

#include <stddef.h>
#include <stdint.h>

/* Shared BG weapon dimensions established independently by the Windows cgame
 * and game modules and the Linux game module. */
enum {
    MAX_WEAPONS = 128,
    MAX_WEAPON_FILES = MAX_WEAPONS - 1,
    WEAPON_BITSET_WORD_COUNT = MAX_WEAPONS / 32,
    BG_WEAPON_FIELD_COUNT = 293,
    MAX_AMMO_TYPES = 128,
    WEAPON_OVERLAY_RETICLE_COUNT = 5,
    WEAPON_STANCE_COUNT = 3,
    WEAPON_GRENADE_TYPE_COUNT = 9
};

/*
 * Weapon-specific extension of parseFieldType_t.  The Windows cgame and game
 * modules and the Linux game module use the same values, lookup domains, and
 * weaponInfo_t destinations for all seven cases.  ParseConfigStringToStruct
 * handles values below PARSE_FIELD_CUSTOM_FIRST itself and accepts these custom
 * values only below the exclusive limit.
 */
typedef enum weaponFieldParseType_e {
    WEAPON_FIELD_PARSE_WEAPON_TYPE = PARSE_FIELD_CUSTOM_FIRST,
    WEAPON_FIELD_PARSE_WEAPON_CLASS = 9,
    WEAPON_FIELD_PARSE_AMMO_TYPE = 10,
    WEAPON_FIELD_PARSE_OVERLAY_RETICLE = 11,
    WEAPON_FIELD_PARSE_WEAPON_SLOT = 12,
    WEAPON_FIELD_PARSE_WEAPON_STANCE = 13,
    WEAPON_FIELD_PARSE_PROJECTILE_EXPLOSION = 14
} weaponFieldParseType_t;

enum {
    WEAPON_FIELD_CUSTOM_TYPE_LIMIT = 15
};

/* The retained weapon-field parser tables and the cgame/game discriminant
 * consumers agree on this five-value projectile-kind domain. */
typedef enum weaponType_e {
    WEAPTYPE_BULLET = 0,
    WEAPTYPE_GRENADE = 1,
    WEAPTYPE_PROJECTILE = 2,
    WEAPTYPE_SPOTTER = 3,
    WEAPTYPE_GAS = 4,
    WEAPTYPE_COUNT = 5
} weaponType_t;

/* Parser custom field type 9 writes the stock 11-entry weapon-class table to
 * weaponInfo_t +0x080; ordinal 3 is "lmg" in each retained module. */
typedef enum weaponClass_e {
    WEAPCLASS_RIFLE = 0,
    WEAPCLASS_MG = 1,
    WEAPCLASS_SMG = 2,
    WEAPCLASS_LMG = 3,
    WEAPCLASS_PISTOL = 4,
    WEAPCLASS_GRENADE = 5,
    WEAPCLASS_ROCKETLAUNCHER = 6,
    WEAPCLASS_TURRET = 7,
    WEAPCLASS_SPOTTER = 8,
    WEAPCLASS_NON_PLAYER = 9,
    WEAPCLASS_FLAMETHROWER = 10,
    WEAPCLASS_COUNT = 11
} weaponClass_t;

/* Symbolic values accepted by the original "ammoType" weapon-field parser. */
typedef enum weaponAmmoType_e {
    WEAPON_AMMO_TYPE_SMG = 0,
    WEAPON_AMMO_TYPE_PISTOL = 1,
    WEAPON_AMMO_TYPE_RIFLE = 2,
    WEAPON_AMMO_TYPE_LMG = 3,
    WEAPON_AMMO_TYPE_HMG = 4,
    WEAPON_AMMO_TYPE_UMG = 5,
    WEAPON_AMMO_TYPE_COUNT = 6
} weaponAmmoType_t;

/* The string table is "none", "primary", "primaryb", "pistol", "grenade",
 * "smokegrenade", "satchel", "binocular" in every retained module. */
typedef enum weaponSlot_e {
    WEAPSLOT_NONE = 0,
    WEAPSLOT_PRIMARY_FIRST = 1,
    WEAPSLOT_PRIMARY_SECOND = 2,
    WEAPSLOT_PRIMARY = WEAPSLOT_PRIMARY_FIRST,
    WEAPSLOT_PRIMARYB = WEAPSLOT_PRIMARY_SECOND,
    WEAPSLOT_PISTOL = 3,
    WEAPSLOT_PRIMARY_LIMIT = WEAPSLOT_PISTOL,
    WEAPSLOT_GRENADE = 4,
    WEAPSLOT_SMOKE_GRENADE = 5,
    WEAPSLOT_SATCHEL = 6,
    WEAPSLOT_BINOCULAR = 7,
    WEAPSLOT_LAST_DROPPABLE = WEAPSLOT_BINOCULAR,
    WEAPSLOT_COUNT = 8
} weaponSlot_t;

/*
 * Shared PM weapon-animation values stored in the low bits of
 * playerState_t.weaponAnim. The complete game-module switch and the independent
 * cgame call-site immediates agree, including the unused value 1 and values
 * 0,2..20. The original Mac cgame and game symbols both name the diagnostic
 * consumer PM_Weapon_PrintWeaponAnim; weaponAnimPose_t was a provisional
 * cgame-only reconstruction name for this same domain and is intentionally not
 * retained as an alias.
 */
typedef enum pmWeaponAnim_e {
    PM_WEAPON_ANIM_IDLE = 0,
    PM_WEAPON_ANIM_FIRE = 2,
    PM_WEAPON_ANIM_FIRE_LASTSHOT = 3,
    PM_WEAPON_ANIM_RECHAMBER = 4,
    PM_WEAPON_ANIM_ADS_FIRE = 5,
    PM_WEAPON_ANIM_ADS_FIRE_LASTSHOT = 6,
    PM_WEAPON_ANIM_ADS_RECHAMBER = 7,
    PM_WEAPON_ANIM_MELEE = 8,
    PM_WEAPON_ANIM_LOWER = 9,
    PM_WEAPON_ANIM_SWITCH_RAISE = 10,
    PM_WEAPON_ANIM_RELOAD = 11,
    PM_WEAPON_ANIM_RELOAD_EMPTY = 12,
    PM_WEAPON_ANIM_RELOAD_START = 13,
    PM_WEAPON_ANIM_RELOAD_END = 14,
    PM_WEAPON_ANIM_ALT_SWITCH_LOWER = 15,
    PM_WEAPON_ANIM_ALT_SWITCH_RAISE = 16,
    PM_WEAPON_ANIM_SPECIAL_FIRE = 17,
    PM_WEAPON_ANIM_DEPLOYED = 18,
    PM_WEAPON_ANIM_ADS_IN = 19,
    PM_WEAPON_ANIM_ADS_OUT = 20
} pmWeaponAnim_t;

/* Shared transition point used when selecting the hip/ADS fire and rechamber
 * animations. The Windows cgame and game modules and Linux game module all
 * compare the binary32 adsFraction against exactly 0.75f. */
#define PM_WEAPON_ADS_RAISE_THRESHOLD 0.75f

typedef enum weaponState_e {
    WEAPON_STATE_IDLE = 0,
    WEAPON_STATE_RAISING = 1,
    WEAPON_STATE_DROPPING = 2,
    WEAPON_STATE_FIRING = 3,
    WEAPON_STATE_RECHAMBERING = 4,
    WEAPON_STATE_RELOADING = 5,
    WEAPON_STATE_RELOADING_INTERRUPT = 6,
    WEAPON_STATE_RELOAD_START = 7,
    WEAPON_STATE_RELOAD_START_INTERRUPT = 8,
    WEAPON_STATE_RELOAD_END = 9,
    WEAPON_STATE_MELEE_WINDUP = 10,
    WEAPON_STATE_MELEE_RELAX = 11,
    WEAPON_STATE_DEPLOYING = 12,
    WEAPON_STATE_BREAKING_DOWN = 13
} weaponState_t;

/*
 * Quake III's cgame header supplies the inherited shared weaponInfo_s/weaponInfo_t
 * identity. The Mac PEF names the owning BG weapon functions but, because they
 * are plain C symbols, does not encode a private parameter-type spelling.
 *
 * The global weaponInfo_t pointer array (bg_weaponInfos,
 * 0x30134cd8) holds weaponInfo_t* entries; BG_GetWeaponIndexForName (0x300110f0) reads
 * the name at +0x04 (0x3001110a MOV ECX,[EAX+0x4]).
 *
 * Field +0x84 (slot): the weapon's inventory-slot category, a weaponSlot_t in the
 * range 1..7. Proven from BG_SetPlayerWeaponForSlot (0x300113f0) and its query
 * sibling (0x30011460), which load weaponInfo->slot (MOV ...,[weaponInfo+0x84]),
 * range-check it as (slot-1) unsigned <= 6, and dispatch: PRIMARY_FIRST/SECOND
 * weapons may go in either primary slot (1 or 2) while PISTOL..LAST_DROPPABLE
 * weapons must go in the matching slot. The complete 0x4bc declaration below
 * was compared positionally with game_mp_uo's record and both original parser
 * descriptor tables.
 */
typedef struct weaponInfo_s {
    int32_t weaponIndex;      /* +0x00, this weapon's registration index. Written by
                               * CG_AllocWeaponInfo (0x3000fe00: MOV [EBX],ESI) to the
                               * index it was registered under; matches server
                               * weaponInfo_s::weaponIndex (BG_GetWeaponForInfo reads it). */
    const char *pickupName;   /* +0x04, weapon "pickup"/config name; CG_AllocWeaponInfo
                               * initializes it to the shared empty string via CopyString(""). */
    const char *displayName;  /* +0x08, localized display-name reference */
    const char *aiOverlayDescription; /* +0x0c, AI overlay localization reference */
    const char *gunModel;     /* +0x10, weapon key "gunModel": the first-person view-weapon
                               * XModel name. CG_RegisterWeapon (0x300435d0) tests gunModel[0]
                               * as the "this weapon has a view model" gate; when set it builds
                               * the view DObj from "xmodel/<gunModel>". Matches server
                               * weaponInfo_s::gunModel (+0x10). */
    const char *handModel;    /* +0x14, weapon key "handModel": the player-hands XModel name
                               * paired with gunModel. CG_RegisterWeapon requires it non-empty
                               * (else Com_Error "No hand model"), registers "xmodel/<handModel>",
                               * and copies the raw name into cgWeaponInfo.name. Matches server
                               * weaponInfo_s::handModel (+0x14). */
    uint8_t reserved018[4]; /* +0x018: no descriptor or direct access in either
                             * original module; retained in the 0x4bc layout. */
    const char *idleAnim;         /* +0x1c */
    const char *emptyIdleAnim;    /* +0x20 */
    const char *fireAnim;         /* +0x24 */
    const char *holdFireAnim;     /* +0x28 */
    const char *lastShotAnim;     /* +0x2c */
    const char *rechamberAnim;    /* +0x30 */
    const char *meleeAnim;        /* +0x34 */
    const char *reloadAnim;       /* +0x38 */
    const char *reloadEmptyAnim;  /* +0x3c */
    const char *reloadStartAnim;  /* +0x40 */
    const char *reloadEndAnim;    /* +0x44 */
    const char *raiseAnim;        /* +0x48 */
    const char *dropAnim;         /* +0x4c */
    const char *altRaiseAnim;     /* +0x50 */
    const char *altDropAnim;      /* +0x54 */
    const char *adsFireAnim;      /* +0x58 */
    const char *adsLastShotAnim;  /* +0x5c */
    const char *adsRechamberAnim; /* +0x60 */
    const char *lmgDeployedAnim;  /* +0x64 */
    const char *lmgDeployAnim;    /* +0x68 */
    const char *lmgBreakdownAnim; /* +0x6c */
    const char *adsUpAnim;        /* +0x70 */
    const char *adsDownAnim;      /* +0x74 */
    const char *modeName;         /* +0x78, localized alternate-mode display name */
    weaponType_t weaponType;  /* +0x7c, weapon's projectile class (bullet/grenade/projectile/
                               * spotter/gas). PM_Weapon_FinishMelee (0x30013ea0) gates its
                               * grenade-cook-off check on weaponType == WEAPTYPE_GRENADE;
                               * PM_StartWeaponAnim (0x300123e0) tests == WEAPTYPE_GAS.
                               * Matches server weaponInfo_s::weaponType
                               * (UO_WEAPON_INFO_OFFSET_WEAPON_TYPE). */
    weaponClass_t weaponClass; /* +0x80, weapon handling class (rifle/mg/smg/spread/pistol/...).
                                * PM_InteruptWeaponWithProneMove (0x30012120) treats an LMG-class
                                * (shotgun) weapon specially. Matches server weaponInfo_s::weaponClass
                                * (+0x80, UO_WEAPON_INFO_OFFSET_WEAPON_CLASS). */
    weaponSlot_t slot;        /* +0x84, this weapon's inventory slot */
    int32_t stackable;        /* +0x88, nonzero if multiple copies of this weapon
                               * may share an inventory slot. Consumed by
                               * BG_GetStackSlotForWeapon (0x30011590): the queried
                               * weapon must itself be stackable, and a slot whose
                               * current occupant is stackable is treated as
                               * available. Matches server weaponInfo_s::stackable
                               * (+0x88, UO_WEAPON_INFO_OFFSET_STACKABLE). */
    int32_t stance;           /* +0x8c: weapon stance enum (stand/duck/prone). */
    weaponAmmoType_t ammoType; /* +0x90, literal parser key "ammoType". Values
                               * LMG/HMG/UMG (3/4/5) select the alternate tracer
                               * chance and dimensions in both original modules. */
    /* +0x94/+0x98: weapon muzzle-flash effect NAME strings, proven as const char*
     * inputs by CG_RegisterWeapon (0x300435d0) which registers each via cgame trap
     * 0xe2 (CG_FX_REGISTER_EFFECT). Adopted from the matching server weaponInfo_s. */
    const char *viewFlashEffect;  /* +0x94, weapon key "viewFlashEffect" (first-person muzzle FX) */
    const char *worldFlashEffect; /* +0x98, weapon key "worldFlashEffect" (third-person muzzle FX) */
    const char *pickupSound;  /* +0x9c, item pickup sound name */
    /* +0xa0..+0x104: the weapon's registered SOUND / model-eject NAME strings. Proven
     * as const char* inputs by CG_RegisterWeapon (0x300435d0), which registers each
     * via cgame trap 0xc3 (CG_COM_SOUND_ALIAS_STRING) and caches the canonical name in the
     * matching cgWeaponInfo_t field. Names adopted from the server weaponInfo_s
     * layout, whose offsets and access widths match this consumer exactly. */
    const char *ammoPickupSound;  /* +0xa0, weapon ammo pickup sound name              */
    const char *projectileSound;  /* +0xa4, weapon key "projectileSound"               */
    const char *pullbackSound;    /* +0xa8, weapon key "pullbackSound"                 */
    const char *fireSound;        /* +0xac, weapon key "fireSound"                     */
    const char *loopFireSound;    /* +0xb0, weapon key "loopFireSound" (turret sustained fire) */
    const char *stopFireSound;    /* +0xb4, weapon key "stopFireSound"                 */
    const char *fireEchoSound;    /* +0xb8, weapon key "fireEchoSound"                 */
    const char *lastShotSound;    /* +0xbc, weapon key "lastShotSound"                 */
    const char *rechamberSound;   /* +0xc0, weapon key "rechamberSound"                */
    const char *reloadSound;      /* +0xc4, weapon key "reloadSound"                   */
    const char *reloadEmptySound; /* +0xc8, weapon key "reloadEmptySound"              */
    const char *reloadStartSound; /* +0xcc, weapon key "reloadStartSound"              */
    const char *reloadEndSound;   /* +0xd0, weapon key "reloadEndSound"                */
    const char *raiseSound;       /* +0xd4, weapon key "raiseSound"                    */
    const char *altSwitchSound;   /* +0xd8, weapon key "altSwitchSound"                */
    const char *deploySound;      /* +0xdc, weapon key "deploySound"                   */
    const char *breakdownSound;   /* +0xe0, weapon key "breakdownSound"                */
    const char *putawaySound;     /* +0xe4, weapon key "putawaySound"                  */
    const char *noteTrackSoundA;  /* +0xe8, weapon key "noteTrackSoundA"               */
    const char *noteTrackSoundB;  /* +0xec, weapon key "noteTrackSoundB"               */
    const char *noteTrackSoundC;  /* +0xf0, weapon key "noteTrackSoundC"               */
    const char *noteTrackSoundD;  /* +0xf4, weapon key "noteTrackSoundD"               */
    const char *projectileSoundBlend1; /* +0xf8, weapon key "projectileSoundBlend1"   */
    const char *projectileSoundBlend2; /* +0xfc, weapon key "projectileSoundBlend2"   */
    const char *shellEjectEffect;      /* +0x100, weapon key "shellEjectEffect" (register effect 0xe2) */
    const char *lastShotEjectEffect;   /* +0x104, weapon key "lastShotEjectEffect" (register effect 0xe2) */
    const char *reticleCenter; /* +0x108, center-reticle material name */
    const char *reticleSide;  /* +0x10c, weapon key "reticleSide": the side-reticle material
                               * NAME. CG_RegisterWeapon (0x300435d0) reads it and, when its
                               * first byte is non-zero (gate at 0x30043c9f), enters the
                               * ADS-overlay/reticle registration + loading-screen path. Adopted
                               * from server weaponInfo_s::reticleSide (+0x10c). NOTE: server
                               * names +0x108 "reticleCenter"; CG_DrawWeaponIcon3D consumes the
                               * registered center-reticle material from the matching cgame slot. */
    int32_t reticleCenterSize; /* +0x110 */
    int32_t reticleSideSize;   /* +0x114 */
    int32_t reticleMinOfs;     /* +0x118 */
    vec3_t sprintMove;             /* +0x11c */
    vec3_t sprintRot;              /* +0x128 */
    vec3_t standMove;              /* +0x134 */
    vec3_t standRot;               /* +0x140 */
    vec3_t duckedOffset;           /* +0x14c */
    vec3_t duckedMove;             /* +0x158 */
    vec3_t duckedRot;              /* +0x164 */
    vec3_t proneOffset;            /* +0x170 */
    vec3_t proneMove;              /* +0x17c */
    vec3_t proneRot;               /* +0x188 */
    float moveSmooth;              /* +0x194 */
    float moveSmoothProne;         /* +0x198 */
    float moveThresholdAlt;        /* +0x19c */
    float moveThresholdStand;      /* +0x1a0 */
    float moveThresholdCrouch;     /* +0x1a4 */
    float moveThresholdProne;      /* +0x1a8 */
    float posRotRate;              /* +0x1ac */
    float posProneRotRate;         /* +0x1b0 */
    float sprintRotMinSpeed;       /* +0x1b4 */
    float standRotMinSpeed;        /* +0x1b8 */
    float duckedRotMinSpeed;       /* +0x1bc */
    float proneRotMinSpeed;        /* +0x1c0 */
    const char *scriptClassname; /* +0x1c4, weapon key "radiantName" */
    const char *worldModel;      /* +0x1c8 */
    const char *pickupModel;     /* +0x1cc */
    const char *viewModel;       /* +0x1d0 */
    const char *hudIcon;         /* +0x1d4 */
    const char *modeIcon;        /* +0x1d8 */
    const char *ammoIcon;        /* +0x1dc */
    int32_t startAmmo;           /* +0x1e0 */
    const char *ammoName;    /* +0x1e4: mutable ammo-type name. BG_SetupAmmoIndexes
                              * lowercases it bytewise, deduplicates it against
                              * bg_ammoTypeNames, and uses it in mismatch diagnostics. */
    int32_t ammoIndex;        /* +0x1e8, index into playerState_t::ammo[] for this weapon's
                               * reserve ammo. Consumed by PM_Weapon_AllowReload (0x300132a0).
                               * Matches server weaponInfo_s::ammoIndex
                               * (UO_WEAPON_INFO_OFFSET_AMMO_INDEX). */
    const char *clipName;    /* +0x1ec: mutable clip-type name. BG_SetupClipIndexes
                              * lowercases and deduplicates it against
                              * bg_ammoClipNames. */
    int32_t clipIndex;        /* +0x1f0, index into playerState_t::clips[] and into the
                               * bg_ammoClipSizes[] table (BG_GetAmmoClipSize). Assigned during
                               * weapon registration (0x30010bee). Consumed by
                               * PM_Weapon_AllowReload (0x300132a0). Matches server
                               * weaponInfo_s::clipIndex (+0x1f0). */
    int32_t maxAmmo;          /* +0x1f4, literal parser key "maxAmmo"; doubles as the
                               * ammo-type dedup key during BG_SetupAmmoIndexes (0x30010633/
                               * 0x30010797), which assigns each distinct maxAmmo a fresh ammoIndex
                               * and stores bg_ammoTypeMax[ammoIndex] = maxAmmo. */
    int32_t clipSize;        /* +0x1f8: per-clip capacity and the value paired with
                              * clipName in bg_ammoClipSizes by BG_SetupClipIndexes. */
    const char *sharedAmmoCapName; /* +0x1fc, this weapon's shared-ammo-pool NAME string (the dedup
                               * key). BG_SetupSharedAmmoIndexes (0x300107e0) case-folds it in place
                               * (per-char tolower at 0x30010844), then Q_stricmpn-matches it against
                               * the bg_sharedAmmoCapNames[] dedup table to assign sharedAmmoCapIndex
                               * (+0x200). Proven a char* here: dereferenced as bytes (CMP byte
                               * ptr[ESI] at 0x3001082c), lowercased, passed to Q_stricmpn and to the
                               * "%s" in the mismatch Com_Error. (An earlier int32_t
                               * "sharedAmmoParentIndex" model was wrong; this is the sole reader.) */
    int32_t sharedAmmoCapIndex; /* +0x200, index into bg_sharedAmmoCapSizes[] for this weapon's shared
                               * ammo pool, or -1 when the weapon has no shared pool (set to
                               * 0xffffffff at 0x30010822, overwritten with a real index only when
                               * the weapon joins a group at 0x3001089f/0x3001095f). Consumed by
                               * BG_GetMaxPickupableAmmo (0x300116f6): a negative value selects the
                               * single-weapon fast path, a non-negative value drives the
                               * shared-pool accumulation loop. Matches the server
                               * sharedAmmoCapIndex passed to BG_GetSharedAmmoCapSize. */
    int32_t sharedAmmoCap;    /* +0x204, literal parser key "sharedAmmoCap"; this
                               * weapon's shared-ammo pool capacity / dedup key,
                               * stored as bg_sharedAmmoCapSizes[sharedAmmoCapIndex] during
                               * BG_SetupSharedAmmoIndexes (0x30010958). */
    int32_t flameDamage;                  /* +0x208: server weaponInfo_s::flameDamage */
    int32_t grenadeTouchDamageEnabled;    /* +0x20c: server weaponInfo_s::grenadeTouchDamageEnabled */
    int32_t damageFalloffMinDamagePercent;/* +0x210: server weaponInfo_s::damageFalloffMinDamagePercent */
    int32_t damageFalloffMinRange;        /* +0x214: server weaponInfo_s::damageFalloffMinRange */
    int32_t damageFalloffMaxRange;        /* +0x218: server weaponInfo_s::damageFalloffMaxRange */
    int32_t meleeDamage;      /* +0x21c, parsed from the "meleeDamage" weapon key;
                               * PM_Weapon_CheckForMelee also uses zero damage as the
                               * no-melee capability gate. Matches server weaponInfo_s. */
    uint8_t reserved220[4]; /* +0x220: no descriptor or direct access in either
                             * original module; retained in the 0x4bc layout. */
    int32_t fireDelay;        /* +0x224, weaponDelay (ms) applied after firing a non-grenade
                               * weapon. PM_Weapon_FireWeapon (0x30013f20) writes it into
                               * ps->weaponDelay on the non-grenade fire commit. Matches server
                               * weaponInfo_s::fireDelay (+0x224, UO_WEAPON_INFO_OFFSET_FIRE_DELAY). */
    int32_t meleeWindup;      /* +0x228, melee swing wind-up time (ms). Matches server
                               * weaponInfo_s::meleeWindup (UO_WEAPON_INFO_OFFSET_MELEE_WINDUP). */
    int32_t fireTime;         /* +0x22c, weaponTime (ms) countdown set after firing a non-grenade
                               * weapon. PM_Weapon_FireWeapon (0x30013f20) writes it into
                               * ps->weaponTime on the non-grenade fire commit. Matches server
                               * weaponInfo_s::fireTime (+0x22c, UO_WEAPON_INFO_OFFSET_FIRE_TIME). */
    int32_t raiseTime;        /* +0x230, bolt-action raise/rechamber duration (ms).
                               * PM_Weapon_CheckForRechamber (0x300124b0) writes it into
                               * ps->weaponTime when it starts a rechamber, and clamps the
                               * pending weaponDelay against it (delay = raiseInterruptTime when
                               * that is nonzero and < raiseTime, else 1). Matches server
                               * weaponInfo_s::raiseTime (+0x230, adjacent to raiseInterruptTime). */
    int32_t raiseInterruptTime; /* +0x234, time (ms) a raise/rechamber can be interrupted after.
                               * PM_SetWeaponReloadAddAmmoDelay (0x30012660) clamps the pending weaponDelay
                               * down to this for a bolt-action (raiseEnabled) weapon whose current
                               * weapon has a pending rechamber. Matches server
                               * weaponInfo_s::raiseInterruptTime (+0x234,
                               * UO_WEAPON_INFO_OFFSET_RAISE_INTERRUPT_TIME). */
    int32_t specialFireDelay; /* +0x238, weaponDelay (ms) applied when firing a grenade-type
                               * weapon (its cook-off "special" fire). Matches server
                               * weaponInfo_s::specialFireDelay (+0x238,
                               * UO_WEAPON_INFO_OFFSET_SPECIAL_FIRE_DELAY). PM_Weapon_FireWeapon
                               * (0x30013f20) reaches this offset via the grenade path's meleeTime
                               * write (+0x238 is specialFireDelay in the server layout; the client
                               * writes ps->weaponDelay from weaponInfo_t+0x238 on the grenade commit). */
    int32_t meleeTime;        /* +0x23c, total melee swing time (ms). PM_Weapon_FireMelee
                               * (0x30014450) clamps ps->weaponTime up to (meleeTime -
                               * meleeWindup). Matches server weaponInfo_s::meleeTime
                               * (UO_WEAPON_INFO_OFFSET_MELEE_TIME). */
    int32_t reloadTime;       /* +0x240, full reload duration (ms) when the clip is not empty
                               * (or the weapon has a nonzero weaponType). PM_SetWeaponReloadAddAmmoDelay
                               * (0x30012660) selects this as the base reload delay. Matches server
                               * weaponInfo_s::reloadTime (+0x240, UO_WEAPON_INFO_OFFSET_RELOAD_TIME). */
    int32_t reloadEmptyTime;  /* +0x244, reload duration (ms) used when the clip is empty and the
                               * weapon's weaponType is 0. PM_SetWeaponReloadAddAmmoDelay (0x30012660) selects
                               * this instead of reloadTime for that case. Matches server
                               * weaponInfo_s::reloadEmptyTime (+0x244,
                               * UO_WEAPON_INFO_OFFSET_RELOAD_EMPTY_TIME). */
    int32_t reloadAddTime;    /* +0x248, per-chunk "add" delay (ms) for a partial reload loop.
                               * PM_SetWeaponReloadAddAmmoDelay (0x30012660), when nonzero, clamps the base
                               * reload delay down to this. Matches server
                               * weaponInfo_s::reloadAddTime (+0x248,
                               * UO_WEAPON_INFO_OFFSET_RELOAD_ADD_TIME). */
    int32_t reloadStartTime;  /* +0x24c, reload-start delay (ms), used in the
                               * weapon states 7/8. PM_SetWeaponReloadAddAmmoDelay (0x30012660) clamps the
                               * start-add delay down to this. Matches server
                               * weaponInfo_s::reloadStartTime (+0x24c,
                               * UO_WEAPON_INFO_OFFSET_RELOAD_START_TIME). */
    int32_t reloadStartAddTime; /* +0x250, reload-start "add" delay (ms). PM_SetWeaponReloadAddAmmoDelay
                               * (0x30012660), in reload-start states 7/8, uses this as the base
                               * delay (0 if it is 0), clamped down to reloadStartTime. Matches
                               * server weaponInfo_s::reloadStartAddTime (+0x250,
                               * UO_WEAPON_INFO_OFFSET_RELOAD_START_ADD_TIME). */
    int32_t reloadEndTime;    /* +0x254, reload-loop wind-down / reload-end duration (ms). When
                               * PM_Weapon_FinishReload (0x30013470) advances a segmented reload out
                               * of the reload-loop/interrupt states, a nonzero value seeds
                               * ps->weaponTime and moves weaponState to WEAPON_STATE_RELOAD_END (9)
                               * with the reload-end pose; a zero value ends the reload immediately
                               * (weaponState -> WEAPON_STATE_IDLE). Matches server
                               * weaponInfo_s::reloadEndTime (+0x254,
                               * UO_WEAPON_INFO_OFFSET_RELOAD_END_TIME). */
    int32_t lowerTime;        /* +0x258, weapon lower/put-away duration (ms) used on an ordinary
                               * (non-alt) weapon change. PM_BeginWeaponChange (0x30012bc0) copies
                               * it into ps->weaponTime when the change is NOT an alt-weapon switch.
                               * Matches server weaponInfo_s::lowerTime (+0x258,
                               * UO_WEAPON_INFO_OFFSET_LOWER_TIME). */
    int32_t switchRaiseTime;  /* +0x25c, weapon raise/deploy duration (ms) on an ordinary
                               * (non-alt) weapon change. PM_Weapon_FinishWeaponChange
                               * (0x30012e70) copies it into ps->weaponTime when raising the
                               * newly-selected weapon (and when raising after empty hands).
                               * Matches server weaponInfo_s::switchRaiseTime (+0x25c,
                               * UO_WEAPON_INFO_OFFSET_SWITCH_RAISE_TIME). */
    int32_t altSwitchLowerTime; /* +0x260, weapon lower duration (ms) used when the change is an
                               * alt-weapon switch (nextWeapon == this weapon's altWeapon).
                               * PM_BeginWeaponChange (0x30012bc0) copies it into ps->weaponTime on
                               * that path. Matches server weaponInfo_s::altSwitchLowerTime (+0x260,
                               * UO_WEAPON_INFO_OFFSET_ALT_SWITCH_LOWER_TIME). */
    int32_t altSwitchRaiseTime; /* +0x264, weapon raise duration (ms) used when the raise is an
                               * alt-weapon switch (newWeapon == prevWeapon's altWeapon).
                               * PM_Weapon_FinishWeaponChange (0x30012e70) copies it into
                               * ps->weaponTime on that path. Matches server
                               * weaponInfo_s::altSwitchRaiseTime (+0x264,
                               * UO_WEAPON_INFO_OFFSET_ALT_SWITCH_RAISE_TIME). */
    int32_t specialTimeThreshold; /* +0x268, grenade cook-off "special time" threshold (ms).
                               * PM_Weapon_FinishMelee (0x30013ea0) forbids finishing the melee
                               * while the player's grenadeTimeLeft is running below this
                               * threshold. Matches server weaponInfo_s::specialTimeThreshold
                               * (UO_WEAPON_INFO_OFFSET_SPECIAL_TIME_THRESHOLD). */
    float moveSpeedScale;     /* +0x26c, weapon key "moveSpeedScale": per-weapon walking speed
                               * multiplier. PM_CmdScale_Walk (0x30008770) multiplies the walk
                               * command scale by it when the player's currentWeapon is set, this
                               * value is > 0, and the player is not sprinting. Matches server
                               * weaponInfo_s::moveSpeedScale (+0x26c). */
    float adsSensitivity;    /* +0x270: server weaponInfo_s::adsSensitivity */
    float adsZoomFov;         /* +0x274, weapon key "adsZoomFov" (alias "turret_fov"): the fully
                               * zoomed aim-down-sight field of view (degrees). CG_CalcFov
                               * (0x3003ffc0) blends the clamped base FOV toward this while the
                               * player aims down the sight. Matches server
                               * weaponInfo_s::adsZoomFov (+0x274). */
    float adsZoomInFrac;      /* +0x278, weapon key "adsZoomInFrac": fraction of the full ADS
                               * lerp spent easing INTO the zoomed overlay. CG_CalcAdsOverlayFrac
                               * (0x30019520) maps adsFraction through this while the player is
                               * zooming in. Matches server weaponInfo_s::adsZoomInFrac (+0x278). */
    float adsZoomOutFrac;     /* +0x27c, weapon key "adsZoomOutFrac": fraction of the full ADS
                               * lerp spent easing OUT of the zoomed overlay. Used by
                               * CG_CalcAdsOverlayFrac when the player is zooming out. Matches
                               * server weaponInfo_s::adsZoomOutFrac (+0x27c). */
    const char *adsOverlayShader; /* +0x280, weapon key "adsOverlayShader": name of the ADS
                               * scope/overlay shader, or "" when the weapon has none.
                               * CG_CalcAdsOverlayFrac tests adsOverlayShader[0] != 0 as the
                               * "this weapon has an ADS overlay" gate. Matches server
                               * weaponInfo_s::adsOverlayShader (+0x280). */
    int32_t adsOverlayReticle; /* +0x284, weapon key "adsOverlayReticle": overlay-reticle enum
                               * (none/crosshair/FG42/Springfield/Gewehr43). A nonzero mode also
                               * enables the ADS-reduction paths when no overlay shader exists. */
    float adsOverlayWidth;    /* +0x288: ADS overlay width in virtual-screen units */
    float adsOverlayHeight;   /* +0x28c: ADS overlay height in virtual-screen units */
    float adsBobFactor;       /* +0x290, weapon key "adsBobFactor": per-weapon view-bob
                               * retention while aiming down sights. BG_CalcWeaponAngles_AddSway
                               * (0x300152f0) forms the ADS scale as
                               * 1 - adsFraction*(1 - adsBobFactor) and multiplies the bob-driven
                               * weapon-angle offset by it, so adsBobFactor == 1.0f keeps full bob
                               * at full ADS and 0.0f removes it. Matches server
                               * weaponInfo_s::adsBobFactor (+0x290,
                               * UO_WEAPON_INFO_OFFSET_ADS_BOB_FACTOR). */
    float adsViewBobScale;    /* +0x294, weapon key "adsViewBobScale": how much of the
                               * player's view bob this weapon transfers to the ADS view
                               * offset. BG_CalculateWeaponPosition_BobOffset (0x30015b50)
                               * multiplies the vertical/horizontal bob factors by this and
                               * by ps->adsFraction; a weapon with adsViewBobScale == 0.0f
                               * gets no ADS view bob. Matches server
                               * weaponInfo_s::adsViewBobScale (+0x294,
                               * UO_WEAPON_INFO_OFFSET_ADS_VIEW_BOB_SCALE). */
    /* +0x298..+0x2a0: the weapon's hip-fire aim-spread by stance, in the same
     * standing/crouched/prone triplet order used for the ADS variant at
     * +0x410..+0x418. BG_GetWeaponSpreadForStance (0x30011950) selects and
     * blends these across the player's current viewheight (stance) transition:
     * flags bit 0x1 (prone) -> Prone, bit 0x2 (crouch) -> Ducked, else Standing.
     * Server weaponInfo_s names +0x298..+0x2a4 hipSpreadStandMin/Ducked/Prone +
     * maxSpread. */
    float hipSpreadStandMin;  /* +0x298, weapon key "hipSpreadStandMin" */
    float hipSpreadDucked;    /* +0x29c, weapon key "hipSpreadDucked" (crouched) */
    float hipSpreadProne;     /* +0x2a0, weapon key "hipSpreadProne" */
    float maxSpread;          /* +0x2a4, weapon key "maxSpread"; not touched by any
                               * reconstructed consumer yet. */
    float aimSpreadDecayRate; /* +0x2a8, per-second hip-fire aim-spread ceiling/decay rate.
                               * PM_AdjustAimSpreadScale (0x30013a90) uses it as the base
                               * spread the frame's accumulated spread is driven toward:
                               * a weapon with aimSpreadDecayRate == 0.0f is decayed straight
                               * to 0. Also scaled by 0.5f while airborne and by
                               * aimSpreadCrouchScale/aimSpreadProneScale by stance. Matches
                               * server weaponInfo_s::aimSpreadDecayRate (+0x2a8). */
    float fireAimSpreadScale; /* +0x2ac, per-shot aim-spread increment (0..1). Consumed by
                               * PM_Weapon_AddFiringAimSpreadScale (0x30014240):
                               * aimSpreadScale += fireAimSpreadScale * 255.0f. Matches the
                               * server weaponInfo_s::fireAimSpreadScale (+0x2ac,
                               * UO_WEAPON_INFO_OFFSET_FIRE_AIM_SPREAD_SCALE). */
    float aimSpreadTurnRate;  /* +0x2b0, per-axis turn-rate contribution to hip-fire aim spread.
                               * PM_AdjustAimSpreadScale (0x30013a90), when nonzero, adds
                               * fabs(AngleSubtract(cmdAngle, oldCmdAngle)) * aimSpreadTurnRate *
                               * 0.01f / pml.frametime for pitch and yaw. Matches server
                               * weaponInfo_s::aimSpreadTurnRate (+0x2b0). */
    float aimSpreadMoveAdd;   /* +0x2b4, movement contribution to hip-fire aim spread.
                               * PM_AdjustAimSpreadScale (0x30013a90), when nonzero, adds this
                               * whenever the current usercmd has nonzero forward or right move.
                               * Matches server weaponInfo_s::aimSpreadMoveAdd (+0x2b4). */
    float aimSpreadCrouchScale; /* +0x2b8, hip-fire aim-spread scale applied while crouched
                               * (ps->entityStateFlags & 0x20). PM_AdjustAimSpreadScale (0x30013a90)
                               * multiplies aimSpreadDecayRate by it. Matches server
                               * weaponInfo_s::aimSpreadCrouchScale (+0x2b8). */
    float aimSpreadProneScale; /* +0x2bc, hip-fire aim-spread scale applied while prone
                               * (ps->entityStateFlags & 0x40). PM_AdjustAimSpreadScale (0x30013a90)
                               * multiplies aimSpreadDecayRate by it (checked before crouch).
                               * Matches server weaponInfo_s::aimSpreadProneScale (+0x2bc). */
    float hipReticleSidePos; /* +0x2c0: scales the dynamic reticle separation */
    int32_t adsInTime;        /* +0x2c4, aim-down-sight raise (zoom-in) transition duration (ms).
                               * The ADS-in weapon-state step PM_BeginWeaponDeploy (0x30012980)
                               * sets ps->weaponTime from this. Matches server weaponInfo_s::adsInTime
                               * (+0x2c4, UO_WEAPON_INFO_OFFSET_ADS_IN_TIME). */
    int32_t adsOutTime;       /* +0x2c8, aim-down-sight lower (zoom-out) transition duration (ms).
                               * The ADS-out weapon-state step PM_BeginWeaponBreakingdown (0x30012ab0) sets
                               * ps->weaponTime from this. Matches server weaponInfo_s::adsOutTime
                               * (+0x2c8, UO_WEAPON_INFO_OFFSET_ADS_OUT_TIME). */
    float idleSwayAds;        /* +0x2cc, weapon idle-sway magnitude while fully aimed down sight.
                               * BG_CalcWeaponAngles_AddIdleSway (0x300151d0), when the weapon
                               * supports ADS (adsEnabled != 0), lerps the idle-sway magnitude from
                               * idleSwayHip toward this by ps->adsFraction. Matches server
                               * weaponInfo_s::idleSwayAds (+0x2cc, UO_WEAPON_INFO_OFFSET_IDLE_SWAY_ADS). */
    float idleSwayHip;        /* +0x2d0, weapon idle-sway magnitude while hip-firing (base). Used by
                               * BG_CalcWeaponAngles_AddIdleSway as the non-ADS idle-sway magnitude
                               * (falling back to 80.0f when it is exactly 0.0f) and as the ADS lerp
                               * origin. Matches server weaponInfo_s::idleSwayHip (+0x2d0,
                               * UO_WEAPON_INFO_OFFSET_IDLE_SWAY_HIP). */
    float idleSwayCrouchScale; /* +0x2d4, idle-sway stance multiplier applied while crouched
                               * (entityStateFlags & EF_CROUCHING). BG_CalcWeaponAngles_AddIdleSway
                               * ramps its running idle-sway scale toward this while crouched. Matches
                               * server weaponInfo_s::idleSwayCrouchScale (+0x2d4,
                               * UO_WEAPON_INFO_OFFSET_IDLE_SWAY_CROUCH_SCALE). */
    float idleSwayProneScale; /* +0x2d8, idle-sway stance multiplier applied while prone
                               * (entityStateFlags & EF_PRONE). BG_CalcWeaponAngles_AddIdleSway
                               * ramps its running idle-sway scale toward this while prone (checked
                               * before crouch). Matches server weaponInfo_s::idleSwayProneScale (+0x2d8,
                               * UO_WEAPON_INFO_OFFSET_IDLE_SWAY_PRONE_SCALE). */
    /* +0x2dc/+0x2e0: literal keys "gunMaxPitch"/"gunMaxYaw".
     * BG_CalcWeaponAngles_AddWeaponIdle passes them as the pitch/yaw spring
     * position limits; CG_SpringStepToZero clamps to +/- each value. */
    float gunMaxPitch;
    float gunMaxYaw;
    float swayMaxAngle;       /* +0x2e4: hip sway angular clamp */
    float swayLerpSpeed;      /* +0x2e8: hip sway response speed */
    float swayPitchScale;     /* +0x2ec: hip pitch-angle output scale */
    float swayYawScale;       /* +0x2f0: hip yaw-angle output scale */
    float swayHorizScale;     /* +0x2f4: hip horizontal position scale */
    float swayVertScale;      /* +0x2f8: hip vertical position scale */
    float swayShellShockScale; /* +0x2fc, weapon key "swayShellShockScale": the weapon's
                               * fully-shell-shocked view-sway multiplier. The weapon-sway
                               * shell-shock scaler CG_WeaponSway_ApplyShellShock (0x30044c10)
                               * eases from 1.0f toward this value over the active shell-shock
                               * time window and passes the eased scale to the hip->ADS sway
                               * blend transform. Matches server weaponInfo_s::swayShellShockScale
                               * (+0x2fc, weapon key "swayShellShockScale"). */
    float adsSwayMaxAngle;    /* +0x300: ADS sway angular clamp */
    float adsSwayLerpSpeed;   /* +0x304: ADS sway response speed */
    float adsSwayPitchScale;  /* +0x308: ADS pitch-angle output scale */
    float adsSwayYawScale;    /* +0x30c: ADS yaw-angle output scale */
    float adsSwayHorizScale;  /* +0x310: ADS horizontal position scale */
    float adsSwayVertScale;   /* +0x314: ADS vertical position scale */
    int32_t twoHanded;       /* +0x318: server weaponInfo_s::twoHanded */
    int32_t ricochet;        /* +0x31c: server weaponInfo_s::ricochet */
    int32_t weaponTimeHold;   /* +0x320, weapon key "semiAuto": nonzero if this weapon holds
                               * its fire timer (semi-automatic). PM_Weapon_WeaponTimeAdjust
                               * (0x30013c50) gates the "finish firing" branch on it: when the
                               * fire-time countdown expires, only a semiAuto weapon with a
                               * loaded clip re-arms and finishes the fire animation. Matches
                               * server weaponInfo_s::weaponTimeHold (+0x320, key "semiAuto"). */
    int32_t raiseEnabled;     /* +0x324, weapon key "boltAction": nonzero for a bolt-action
                               * weapon that rechambers between shots. PM_SetWeaponReloadAddAmmoDelay
                               * (0x30012660) only applies the rechamber/raiseInterruptTime delay
                               * path for a weapon with this set. Matches server
                               * weaponInfo_s::raiseEnabled (+0x324, weapon key "boltAction"). */
    int32_t adsEnabled;       /* +0x328, weapon key "aimDownSight": nonzero if this weapon
                               * supports aim-down-sight. CG_CalcAdsViewOffset (0x300451a0)
                               * gates its ADS view-offset computation on
                               * bg_weaponInfos[currentWeapon]->adsEnabled != 0. Matches server
                               * weaponInfo_s::adsEnabled (+0x328). */
    int32_t adsRaiseEnabled;  /* +0x32c, weapon key "rechamberWhileAds". Matches server
                               * weaponInfo_s::adsRaiseEnabled (+0x32c). Not consumed by the
                               * exported code yet; named from the server bank to complete the
                               * ADS-error block below it. */
    float adsViewErrorMin;    /* +0x330, weapon key "adsViewErrorMin": low bound (degrees) of the
                               * random ADS aim-error (idle sway) magnitude. CG_UpdateAdsViewError
                               * (0x30036070) uses it as the min arg of Q_RandomRange when stepping
                               * the ADS view-error accumulators. Matches server
                               * weaponInfo_s::adsViewErrorMin (+0x330). */
    float adsViewErrorMax;    /* +0x334, weapon key "adsViewErrorMax": high bound (degrees) of the
                               * random ADS aim-error magnitude, and the enable gate: when it is
                               * exactly 0.0f CG_UpdateAdsViewError skips the sway step entirely.
                               * Matches server weaponInfo_s::adsViewErrorMax (+0x334). */
    int32_t specialTimeEnabled; /* +0x338, weapon key "cookOffHold": nonzero for weapons that
                               * hold a grenade cook-off timer. PM_Weapon_FinishMelee
                               * (0x30013ea0) requires it set to run the cook-off gate. Matches
                               * server weaponInfo_s::specialTimeEnabled (+0x338). */
    int32_t cloth;           /* +0x33c: server weaponInfo_s::cloth */
    int32_t clipRequired;     /* +0x340, weapon key "clipOnly": nonzero for weapons that cannot
                               * fire without a clip. PM_RemoveEmptyClipOnlyWeapon (0x30013a00)
                               * gates its whole body on this being set (drops a spent clipOnly
                               * weapon). Matches server weaponInfo_s::clipRequired (+0x340). */
    int32_t wideListIcon;     /* +0x344, weapon key "wideListIcon": nonzero for weapons
                               * whose HUD list icon is drawn double-width. The usable-entity
                               * hint drawer (0x300303a0) reads it (MOV EAX,[weaponInfo+0x344])
                               * and, when set, doubles the hint's icon width scale. Matches
                               * server weaponInfo_s::wideListIcon (+0x344). */
    int32_t adsFireDelayEnabled; /* +0x348, weapon key "adsFire": nonzero when this weapon scales
                               * its post-fire weaponDelay by the ADS fraction. PM_Weapon_FireWeapon
                               * (0x30013f20), on the non-grenade fire commit, replaces ps->weaponDelay
                               * with round((1.0 - ps->adsFraction) / adsFireDelayRate) when this is
                               * set. Matches server weaponInfo_s::adsFireDelayEnabled (+0x348). */
    int32_t dropDisabled; /* +0x34c: weaponInfo_s::dropDisabled (key "doNotDrop") */
    const char *killIcon;     /* +0x350, weapon key "killIcon": kill-feed material NAME.
                               * CG_RegisterWeapon (0x300435d0) registers it (trap 5) for its
                               * side effect when non-empty; the returned handle is discarded.
                               * Adopted from server weaponInfo_s::killIcon (+0x350). */
    int32_t wideKillIcon;     /* +0x354, weapon key "wideKillIcon": use the
                               * double-width obituary icon scale when nonzero. */
    int32_t reloadRequiresAddRoom; /* +0x358, weapon key "noPartialReload": when set, a reload is
                                    * only allowed if there is room for a full reloadAmmoAdd chunk.
                                    * Consumed by PM_Weapon_AllowReload (0x300132a0). Matches server
                                    * weaponInfo_s::reloadRequiresAddRoom (+0x358). */
    int32_t segmentedReload; /* +0x35c, literal key "segmentedReload"; nonzero for
                               * a weapon whose segmented/looped
                               * reload can be cut short by a fresh attack press. Consumed by
                               * PM_Weapon_CheckForReload (0x300136d0): only when this is set
                               * does a fresh PM_BUTTON_FIRE press change RELOADING to
                               * RELOADING_INTERRUPT or RELOAD_START to
                               * RELOAD_START_INTERRUPT. */
    int32_t reloadAmmoAdd;    /* +0x360, ammo added per reload chunk. Consumed by
                               * PM_Weapon_AllowReload (0x300132a0) and, for the
                               * non-loop reload states, by PM_ReloadClip
                               * (0x30012290) as the per-fill add-chunk size. Matches
                               * server weaponInfo_s::reloadAmmoAdd (+0x360,
                               * UO_WEAPON_INFO_OFFSET_RELOAD_AMMO_ADD). */
    int32_t reloadStartAmmoAdd; /* +0x364, ammo added on the first fill of a reload
                               * loop. PM_ReloadClip (0x30012290) uses it in
                               * the reload-start states (weaponState 7/8): the fill only
                               * proceeds when it is nonzero, and it caps the transfer.
                               * Matches server weaponInfo_s::reloadStartAmmoAdd (+0x364,
                               * UO_WEAPON_INFO_OFFSET_RELOAD_START_AMMO_ADD). */
    const char *altWeaponName; /* +0x368, the configured NAME of this weapon's alt-fire
                               * weapon (the "altWeapon" weapon key), before it is resolved
                               * to an index. Proven by BG_SetupAltWeaponIndexes (0x30010c30):
                               * it reads this pointer, gates on *altWeaponName != 0 to
                               * decide the weapon has an alt, Q_stricmp's it against every
                               * bg_weaponInfos[]->pickupName (+0x4) to find the match, then stores
                               * that match's index into altWeapon (+0x36c). Also emitted as
                               * the '%s' alt-weapon name in this pass's Com_Error diagnostics
                               * ("could not find altWeapon '%s'", etc.). Previously modelled
                               * as +0x368..+0x36c padding; the string deref proves it a
                               * const char*. Matches server weaponInfo_s::altWeaponName. */
    int32_t altWeapon;        /* +0x36c, this weapon's alt-fire weapon index. Resolved from
                               * altWeaponName (+0x368) by BG_SetupAltWeaponIndexes (0x30010c30),
                               * which also zeroes it for every entry before the resolve pass.
                               * Consumed by
                               * CG_SelectWeaponIndex (0x30047390): a selection request equal
                               * to bg_weaponInfos[current]->altWeapon is treated as an
                               * alt-weapon re-selection. Matches the server
                               * weaponInfo_s::altWeapon (+0x36c,
                               * UO_WEAPON_INFO_OFFSET_ALT_WEAPON). */
    int32_t dropAmmoMin;          /* +0x370: server weaponInfo_s::dropAmmoMin */
    int32_t dropAmmoMax;          /* +0x374: server weaponInfo_s::dropAmmoMax */
    int32_t missileSplashRadius;  /* +0x378: server weaponInfo_s::missileSplashRadius */
    int32_t missileSplashDamage; /* +0x37c, weapon flag: when nonzero, PM_Weapon (0x30014710)
                               * additionally appends EV_GRENADE_SUICIDE (210) on the grenade
                               * cook-off run-out path (grenadeTimeLeft expired) so the grenade
                               * is launched rather than only detonating in hand. Exact server
                               * weaponInfo_s field name unresolved; named by proven role. */
    int32_t missileSplashMinDamage; /* +0x380: server weaponInfo_s::missileSplashMinDamage */
    int32_t missileSpeed;           /* +0x384: server weaponInfo_s::missileSpeed */
    int32_t missileVerticalSpeed;   /* +0x388: server weaponInfo_s::missileVerticalSpeed */
    /* +0x38c..+0x3b8: projectile clip-model / explosion / trail resource fields,
     * proven by CG_RegisterWeapon (0x300435d0): the char* names are registered
     * (model trap 0x38 / effect trap 0xe2), the +0x3b8 int is FILD'd to a float and
     * cached. Adopted from the matching server weaponInfo_s layout. */
    const char *clipModel;    /* +0x38c, weapon key "clipModel" (magazine model, registered
                               * via trap 0x38 when non-empty; also names the "no valid
                               * projectile model" Com_Printf warning). */
    int32_t projectileExplosionType; /* +0x390, parsed projectile explosion enum (not read here) */
    const char *projectileExplosionEffect; /* +0x394, weapon key "projExplosionEffect"
                                            * (registered via effect trap 0xe2) */
    const char *projectileExplosionSound;  /* +0x398, weapon key "projExplosionSound"
                                            * (registered via sound trap 0xc3) */
    int32_t projectileImpactExplode;  /* +0x39c: projImpactExplode; not touched by this consumer. */
    int32_t artilleryBarrageCount;    /* +0x3a0: artillery barrage scalar; not touched by this consumer. */
    int32_t artilleryBarrageSpread;   /* +0x3a4: artillery barrage scalar; not touched by this consumer. */
    int32_t artilleryBarrageFirstDelay; /* +0x3a8: artillery barrage scalar; not touched by this consumer. */
    int32_t artilleryBarrageDelayMin; /* +0x3ac: artillery barrage scalar; not touched by this consumer. */
    int32_t artilleryBarrageDelayMax; /* +0x3b0: artillery barrage scalar; not touched by this consumer. */
    const char *projectileTrailEffect; /* +0x3b4, weapon key "projTrailEffect" (registered via
                                        * effect trap 0xe2 when its first byte is non-zero) */
    int32_t projectileDLight; /* +0x3b8, weapon key "projectileDLight": integer dynamic-light
                               * id. CG_RegisterWeapon FILD's it and stores the float into the
                               * cgWeaponInfo record (+0x158). Adopted from server
                               * weaponInfo_s::projectileDLight (+0x3b8). */
    float projectileRed;   /* +0x3bc: server weaponInfo_s::projectileRed, not read here. */
    float projectileGreen; /* +0x3c0: server weaponInfo_s::projectileGreen, not read here. */
    float projectileBlue;  /* +0x3c4: server weaponInfo_s::projectileBlue, not read here. */
    float adsPitchOffset;     /* +0x3c8, weapon key ADS pitch offset (degrees). When this weapon
                               * supports ADS (adsEnabled != 0), BG_CalculateWeaponPosition_IdleAngles
                               * (0x30015920) sets the pitch component out[0] = adsPitchOffset *
                               * ps->adsFraction. Matches server weaponInfo_s::adsPitchOffset (+0x3c8,
                               * UO_WEAPON_INFO_OFFSET_ADS_PITCH_OFFSET). */
    float adsCrosshairInFrac; /* +0x3cc: ordinary-crosshair fade while zooming in */
    float adsCrosshairOutFrac;/* +0x3d0: ordinary-crosshair fade while zooming out */
    /* +0x3d4..+0x3e0: ADS fire recoil pitch/yaw random range. BG_WeaponFireRecoil
     * (0x30016120), when the player is aiming down sight (adsFraction > 0), draws
     * pitch = rand()/32768 * (fireRecoilAdsPitchMax - fireRecoilAdsPitchMin) +
     * fireRecoilAdsPitchMin and the yaw analog, then ADDs them into the recoil
     * angle accumulator. Match server weaponInfo_s +0x3d4..+0x3e0. */
    float fireRecoilAdsPitchMin;  /* +0x3d4 */
    float fireRecoilAdsPitchMax;  /* +0x3d8 */
    float fireRecoilAdsYawMin;    /* +0x3dc */
    float fireRecoilAdsYawMax;    /* +0x3e0 */
    /* +0x3e4..+0x3f0: ADS recoil-return spring coefficients, parsed from
     * adsGunKickAccel/SpeedMax/SpeedDecay/StaticDecay and blended with their
     * hip counterparts by the spring integrator at 0x30015760. */
    float recoilReturnAds;   /* +0x3e4 */
    float recoilVelocityAds; /* +0x3e8 */
    float recoilDampingAds;  /* +0x3ec */
    float recoilFrictionAds; /* +0x3f0 */
    /* +0x3f4..+0x400: ADS fire viewkick pitch/yaw random range. BG_WeaponFireRecoil
     * (0x30016120), when exactly fully aimed down sight (adsFraction == 1.0), draws
     * pitch = rand()/32768 * (fireViewkickAdsPitchMax - fireViewkickAdsPitchMin) +
     * fireViewkickAdsPitchMin (and the yaw analog) for the instantaneous view kick.
     * Match server weaponInfo_s +0x3f4..+0x400. */
    float fireViewkickAdsPitchMin; /* +0x3f4 */
    float fireViewkickAdsPitchMax; /* +0x3f8 */
    float fireViewkickAdsYawMin;   /* +0x3fc */
    float fireViewkickAdsYawMax;   /* +0x400 */
    float adsViewKickCenterSpeed; /* +0x404, weapon key "adsViewKickCenterSpeed": the per-second
                               * centering acceleration (deg/s^2 scale) applied to the ADS
                               * view-kick offset. CG_UpdateViewKick (0x3003f9f0) selects this
                               * field over hipViewKickCenterSpeed when the predicted player's
                               * adsFraction (cg_predictedPlayerState.adsFraction) is >= 0.5, then
                               * multiplies it by the signed restoring direction and dt to
                               * integrate the view-kick velocity. Read as FMUL float [EAX+0x404]
                               * at 0x3003fa8d. Matches server weaponInfo_s::adsViewKickCenterSpeed
                               * (+0x404, weapon key "adsViewKickCenterSpeed"). */
    uint8_t reserved408[8]; /* +0x408..+0x40f: no descriptor or direct access in
                             * either original module; retained for layout. */
    /* +0x410..+0x418: the weapon's ADS (aim-down-sight) aim-spread by stance, the
     * ADS counterpart of hipSpread* (+0x298..+0x2a0), same standing/crouched/prone
     * triplet order. Selected/blended by BG_GetWeaponSpreadForStance (0x30011950)
     * when its ads-flag argument is nonzero: flags bit 0x1 (prone) -> Prone,
     * bit 0x2 (crouch) -> Ducked, else Standing. Server weaponInfo_s names these
     * adsSpread/Ducked/Prone. */
    float adsSpread;  /* +0x410, weapon key "adsSpread" */
    float adsSpreadDucked;    /* +0x414, weapon key "adsSpreadDucked" (crouched) */
    float adsSpreadProne;     /* +0x418, weapon key "adsSpreadProne" */
    /* +0x41c..+0x428: hip fire recoil pitch/yaw random range, the hip-fire
     * counterpart of fireRecoilAds*. BG_WeaponFireRecoil (0x30016120) uses these
     * when the player is not aiming down sight (adsFraction <= 0). Match server
     * weaponInfo_s +0x41c..+0x428. */
    float fireRecoilHipPitchMin;  /* +0x41c */
    float fireRecoilHipPitchMax;  /* +0x420 */
    float fireRecoilHipYawMin;    /* +0x424 */
    float fireRecoilHipYawMax;    /* +0x428 */
    /* +0x42c..+0x438: hip-fire recoil-return spring coefficients parsed from
     * hipGunKickAccel/SpeedMax/SpeedDecay/StaticDecay. */
    float recoilReturnHip;   /* +0x42c */
    float recoilVelocityHip; /* +0x430 */
    float recoilDampingHip;  /* +0x434 */
    float recoilFrictionHip; /* +0x438 */
    /* +0x43c..+0x448: hip fire viewkick pitch/yaw random range, the hip-fire
     * counterpart of fireViewkickAds*. BG_WeaponFireRecoil (0x30016120) uses these
     * for every adsFraction other than an ordered exact 1.0. Match server
     * weaponInfo_s +0x43c..+0x448. */
    float fireViewkickHipPitchMin; /* +0x43c */
    float fireViewkickHipPitchMax; /* +0x440 */
    float fireViewkickHipYawMin;   /* +0x444 */
    float fireViewkickHipYawMax;   /* +0x448 */
    float hipViewKickCenterSpeed; /* +0x44c, weapon key "hipViewKickCenterSpeed": the hip-fire
                               * counterpart of adsViewKickCenterSpeed. CG_UpdateViewKick
                               * (0x3003f9f0) uses it when adsFraction < 0.5. Read as
                               * FMUL float [EAX+0x44c] at 0x3003fa95. Matches server
                               * weaponInfo_s::hipViewKickCenterSpeed (+0x44c). */
    uint8_t reserved450[8]; /* +0x450..+0x457: no descriptor or direct access in
                             * either original module; retained for layout. */
    float aiEffectiveRange; /* +0x458: server weaponInfo_s::aiEffectiveRange */
    float aiMissRange;      /* +0x45c: server weaponInfo_s::aiMissRange */
    int32_t reloadLoopTime;   /* +0x460, weapon key "adsReloadTransTime": the reload-loop /
                               * ADS reload-transition duration (ms). PM_UpdateAimDownSightLerp
                               * (0x30011f50) differences it against ps->weaponTime
                               * (weaponTime - reloadLoopTime) to decide whether a reloading weapon
                               * (weaponState RELOADING / RELOAD_END) is still inside its
                               * reload-transition window, which gates whether ADS is allowed to
                               * zoom in this frame. Matches server weaponInfo_s::reloadLoopTime
                               * (+0x460, weapon key "adsReloadTransTime"). */
    int32_t adsTransBlendTime;      /* +0x464: server weaponInfo_s::adsTransBlendTime */
    float turretLeftArcDefault;     /* +0x468: server weaponInfo_s::turretLeftArcDefault */
    float turretRightArcDefault;    /* +0x46c: server weaponInfo_s::turretRightArcDefault */
    float turretTopArcDefault;      /* +0x470: server weaponInfo_s::turretTopArcDefault */
    float turretBottomArcDefault;   /* +0x474: server weaponInfo_s::turretBottomArcDefault */
    float turretAccuracy;           /* +0x478: server weaponInfo_s::turretAccuracy */
    float turretPitchRate;          /* +0x47c: server weaponInfo_s::turretPitchRate */
    float turretYawRate;            /* +0x480: server weaponInfo_s::turretYawRate */
    float turretConvergenceTime;    /* +0x484: server weaponInfo_s::turretConvergenceTime */
    float turretMaxRange;           /* +0x488: server weaponInfo_s::turretMaxRange */
    float animHorRotateInc;    /* +0x48c, literal key "animHorRotateInc": used by
                               * CG_PlayerTurretPositionAndBlend (0x30033b70) as the denominator of
                               * FDIV [weapInfo+0x48c] when converting the barrel-position delta into
                               * a bone-tree blend duration. */
    float turretPlayerSpacing; /* +0x490: server weaponInfo_s::turretPlayerSpacing */
    const char *hintString;    /* +0x494: server weaponInfo_s::hintString */
    int32_t hintStringIndex;   /* +0x498: server weaponInfo_s::hintStringIndex */
    float turretHeatPerShot;   /* +0x49c: server weaponInfo_s::turretHeatPerShot */
    float turretHeatDecay;     /* +0x4a0: server weaponInfo_s::turretHeatDecay */
    float horizViewJitter;   /* +0x4a4, literal key "horizViewJitter": mounted yaw amplitude */
    float vertViewJitter;    /* +0x4a8, literal key "vertViewJitter": mounted pitch amplitude */
    float grenadeSplashVehicleDamageScale; /* +0x4ac: server weaponInfo_s::grenadeSplashVehicleDamageScale */
    const char *script;                    /* +0x4b0: server weaponInfo_s::script */
    float adsFireDelayRate;   /* +0x4b4, ADS fire-delay rate divisor. When adsFireDelayEnabled is
                               * set, PM_Weapon_FireWeapon (0x30013f20) computes ps->weaponDelay =
                               * Q_rint((1.0f - ps->adsFraction) / adsFireDelayRate). Matches server
                               * weaponInfo_s::adsFireDelayRate (+0x4b4,
                               * UO_WEAPON_INFO_OFFSET_ADS_FIRE_DELAY_RATE). */
    float adsFireDelayOutRate; /* +0x4b8, ADS fire-delay OUT rate divisor (the zoom-out twin of
                               * adsFireDelayRate). Written alongside adsFireDelayRate by the
                               * ADS-rate precompute (BG_CalcWeaponADSFireDelayRates, 0x300101d0):
                               * for each registered weapon it stores 1.0f/adsOutTime when
                               * adsOutTime > 0, else the default 1.0f/500. Last field of the
                               * record; fills it to the allocated 0x4bc size (CG_AllocWeaponInfo).
                               * Matches server weaponInfo_s::adsFireDelayOutRate (+0x4b8). */
} weaponInfo_t;

#define WEAPON_TYPE_LAYOUT_ASSERT(name_, expression_) typedef char name_[(expression_) ? 1 : -1]

WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_type_size, sizeof(weaponType_t) == 4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_class_size, sizeof(weaponClass_t) == 4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_ammo_type_size, sizeof(weaponAmmoType_t) == 4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_slot_size, sizeof(weaponSlot_t) == 4);
WEAPON_TYPE_LAYOUT_ASSERT(q_pm_weapon_anim_size, sizeof(pmWeaponAnim_t) == 4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_state_size, sizeof(weaponState_t) == 4);

#if UINTPTR_MAX == UINT32_MAX
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_weapon_index_offset, offsetof(weaponInfo_t, weaponIndex) == 0x000);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_pickup_name_offset, offsetof(weaponInfo_t, pickupName) == 0x004);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_display_name_offset, offsetof(weaponInfo_t, displayName) == 0x008);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_hand_model_offset, offsetof(weaponInfo_t, handModel) == 0x014);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_mode_name_offset, offsetof(weaponInfo_t, modeName) == 0x078);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_weaponType_offset, offsetof(weaponInfo_t, weaponType) == 0x07c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_weaponClass_offset, offsetof(weaponInfo_t, weaponClass) == 0x080);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_slot_offset, offsetof(weaponInfo_t, slot) == 0x084);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_stance_offset, offsetof(weaponInfo_t, stance) == 0x08c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ammo_type_offset, offsetof(weaponInfo_t, ammoType) == 0x090);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_pickup_sound_offset, offsetof(weaponInfo_t, pickupSound) == 0x09c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_loop_sound_offset, offsetof(weaponInfo_t, loopFireSound) == 0x0b0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_stop_sound_offset, offsetof(weaponInfo_t, stopFireSound) == 0x0b4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sprint_move_offset, offsetof(weaponInfo_t, sprintMove) == 0x11c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sprint_rot_offset, offsetof(weaponInfo_t, sprintRot) == 0x128);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_stand_move_offset, offsetof(weaponInfo_t, standMove) == 0x134);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_stand_rot_offset, offsetof(weaponInfo_t, standRot) == 0x140);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ducked_move_offset, offsetof(weaponInfo_t, duckedMove) == 0x158);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ducked_rot_offset, offsetof(weaponInfo_t, duckedRot) == 0x164);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_prone_move_offset, offsetof(weaponInfo_t, proneMove) == 0x17c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_prone_rot_offset, offsetof(weaponInfo_t, proneRot) == 0x188);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_pos_move_rate_offset, offsetof(weaponInfo_t, moveSmooth) == 0x194);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_pos_prone_move_rate_offset, offsetof(weaponInfo_t, moveSmoothProne) == 0x198);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_pos_rot_rate_offset, offsetof(weaponInfo_t, posRotRate) == 0x1ac);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_pos_prone_rot_rate_offset, offsetof(weaponInfo_t, posProneRotRate) == 0x1b0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sprint_rot_min_speed_offset, offsetof(weaponInfo_t, sprintRotMinSpeed) == 0x1b4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_script_classname_offset, offsetof(weaponInfo_t, scriptClassname) == 0x1c4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_pickup_model_offset, offsetof(weaponInfo_t, pickupModel) == 0x1cc);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_hud_icon_offset, offsetof(weaponInfo_t, hudIcon) == 0x1d4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_mode_icon_offset, offsetof(weaponInfo_t, modeIcon) == 0x1d8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ammo_icon_offset, offsetof(weaponInfo_t, ammoIcon) == 0x1dc);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_start_ammo_offset, offsetof(weaponInfo_t, startAmmo) == 0x1e0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ammo_name_offset, offsetof(weaponInfo_t, ammoName) == 0x1e4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ammo_index_offset, offsetof(weaponInfo_t, ammoIndex) == 0x1e8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_clip_name_offset, offsetof(weaponInfo_t, clipName) == 0x1ec);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_clip_index_offset, offsetof(weaponInfo_t, clipIndex) == 0x1f0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ammo_max_offset, offsetof(weaponInfo_t, maxAmmo) == 0x1f4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_clip_size_offset, offsetof(weaponInfo_t, clipSize) == 0x1f8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_shared_ammo_cap_name_offset, offsetof(weaponInfo_t, sharedAmmoCapName) == 0x1fc);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_shared_ammo_cap_size_offset, offsetof(weaponInfo_t, sharedAmmoCap) == 0x204);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_flame_damage_offset, offsetof(weaponInfo_t, flameDamage) == 0x208);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_grenade_touch_damage_enabled_offset, offsetof(weaponInfo_t, grenadeTouchDamageEnabled) == 0x20c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_damage_falloff_min_damage_percent_offset,
                          offsetof(weaponInfo_t, damageFalloffMinDamagePercent) == 0x210);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_damage_falloff_min_range_offset, offsetof(weaponInfo_t, damageFalloffMinRange) == 0x214);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_damage_falloff_max_range_offset, offsetof(weaponInfo_t, damageFalloffMaxRange) == 0x218);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_melee_damage_offset, offsetof(weaponInfo_t, meleeDamage) == 0x21c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fire_delay_offset, offsetof(weaponInfo_t, fireDelay) == 0x224);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reload_time_offset, offsetof(weaponInfo_t, reloadTime) == 0x240);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_movement_speed_scale_offset, offsetof(weaponInfo_t, moveSpeedScale) == 0x26c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_sensitivity_offset, offsetof(weaponInfo_t, adsSensitivity) == 0x270);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_zoom_fov_offset, offsetof(weaponInfo_t, adsZoomFov) == 0x274);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_aim_spread_decay_rate_offset, offsetof(weaponInfo_t, aimSpreadDecayRate) == 0x2a8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fire_aim_spread_scale_offset, offsetof(weaponInfo_t, fireAimSpreadScale) == 0x2ac);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_overlay_reticle_offset, offsetof(weaponInfo_t, adsOverlayReticle) == 0x284);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_aim_spread_turn_rate_offset, offsetof(weaponInfo_t, aimSpreadTurnRate) == 0x2b0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_aim_spread_move_add_offset, offsetof(weaponInfo_t, aimSpreadMoveAdd) == 0x2b4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_aim_spread_crouch_scale_offset, offsetof(weaponInfo_t, aimSpreadCrouchScale) == 0x2b8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_aim_spread_prone_scale_offset, offsetof(weaponInfo_t, aimSpreadProneScale) == 0x2bc);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_in_time_offset, offsetof(weaponInfo_t, adsInTime) == 0x2c4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_out_time_offset, offsetof(weaponInfo_t, adsOutTime) == 0x2c8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sway_max_angle_offset, offsetof(weaponInfo_t, swayMaxAngle) == 0x2e4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sway_lerp_speed_offset, offsetof(weaponInfo_t, swayLerpSpeed) == 0x2e8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sway_pitch_scale_offset, offsetof(weaponInfo_t, swayPitchScale) == 0x2ec);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sway_yaw_scale_offset, offsetof(weaponInfo_t, swayYawScale) == 0x2f0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sway_horiz_scale_offset, offsetof(weaponInfo_t, swayHorizScale) == 0x2f4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sway_vert_scale_offset, offsetof(weaponInfo_t, swayVertScale) == 0x2f8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_sway_max_angle_offset, offsetof(weaponInfo_t, adsSwayMaxAngle) == 0x300);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_sway_lerp_speed_offset, offsetof(weaponInfo_t, adsSwayLerpSpeed) == 0x304);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_sway_pitch_scale_offset, offsetof(weaponInfo_t, adsSwayPitchScale) == 0x308);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_sway_yaw_scale_offset, offsetof(weaponInfo_t, adsSwayYawScale) == 0x30c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_sway_horiz_scale_offset, offsetof(weaponInfo_t, adsSwayHorizScale) == 0x310);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_sway_vert_scale_offset, offsetof(weaponInfo_t, adsSwayVertScale) == 0x314);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_two_handed_offset, offsetof(weaponInfo_t, twoHanded) == 0x318);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_weapon_time_hold_offset, offsetof(weaponInfo_t, weaponTimeHold) == 0x320);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_enabled_offset, offsetof(weaponInfo_t, adsEnabled) == 0x328);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_raise_enabled_offset, offsetof(weaponInfo_t, adsRaiseEnabled) == 0x32c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ads_fire_delay_enabled_offset, offsetof(weaponInfo_t, adsFireDelayEnabled) == 0x348);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_kill_icon_offset, offsetof(weaponInfo_t, killIcon) == 0x350);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_alt_weapon_name_offset, offsetof(weaponInfo_t, altWeaponName) == 0x368);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_alt_weapon_offset, offsetof(weaponInfo_t, altWeapon) == 0x36c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_missile_splash_radius_offset, offsetof(weaponInfo_t, missileSplashRadius) == 0x378);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_missile_splash_damage_offset, offsetof(weaponInfo_t, missileSplashDamage) == 0x37c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_missile_splash_min_damage_offset, offsetof(weaponInfo_t, missileSplashMinDamage) == 0x380);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_missile_speed_offset, offsetof(weaponInfo_t, missileSpeed) == 0x384);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_missile_vertical_speed_offset, offsetof(weaponInfo_t, missileVerticalSpeed) == 0x388);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_clip_model_offset, offsetof(weaponInfo_t, clipModel) == 0x38c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_projectile_explosion_type_offset, offsetof(weaponInfo_t, projectileExplosionType) == 0x390);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_projectile_explosion_effect_offset, offsetof(weaponInfo_t, projectileExplosionEffect) == 0x394);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_projectile_trail_effect_offset, offsetof(weaponInfo_t, projectileTrailEffect) == 0x3b4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_artillery_barrage_count_offset, offsetof(weaponInfo_t, artilleryBarrageCount) == 0x3a0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_artillery_barrage_spread_offset, offsetof(weaponInfo_t, artilleryBarrageSpread) == 0x3a4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_artillery_barrage_first_delay_offset, offsetof(weaponInfo_t, artilleryBarrageFirstDelay) == 0x3a8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_artillery_barrage_delay_min_offset, offsetof(weaponInfo_t, artilleryBarrageDelayMin) == 0x3ac);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_artillery_barrage_delay_max_offset, offsetof(weaponInfo_t, artilleryBarrageDelayMax) == 0x3b0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reload_loop_time_offset, offsetof(weaponInfo_t, reloadLoopTime) == 0x460);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ai_effective_range_offset, offsetof(weaponInfo_t, aiEffectiveRange) == 0x458);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ai_miss_range_offset, offsetof(weaponInfo_t, aiMissRange) == 0x45c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_left_arc_offset, offsetof(weaponInfo_t, turretLeftArcDefault) == 0x468);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_right_arc_offset, offsetof(weaponInfo_t, turretRightArcDefault) == 0x46c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_top_arc_offset, offsetof(weaponInfo_t, turretTopArcDefault) == 0x470);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_bottom_arc_offset, offsetof(weaponInfo_t, turretBottomArcDefault) == 0x474);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_pitch_rate_offset, offsetof(weaponInfo_t, turretPitchRate) == 0x47c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_yaw_rate_offset, offsetof(weaponInfo_t, turretYawRate) == 0x480);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_anim_horizontal_rotate_increment_offset, offsetof(weaponInfo_t, animHorRotateInc) == 0x48c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_player_spacing_offset, offsetof(weaponInfo_t, turretPlayerSpacing) == 0x490);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_hint_string_offset, offsetof(weaponInfo_t, hintString) == 0x494);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_hint_string_index_offset, offsetof(weaponInfo_t, hintStringIndex) == 0x498);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_heat_per_shot_offset, offsetof(weaponInfo_t, turretHeatPerShot) == 0x49c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_turret_heat_decay_offset, offsetof(weaponInfo_t, turretHeatDecay) == 0x4a0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_horizontal_view_jitter_offset, offsetof(weaponInfo_t, horizViewJitter) == 0x4a4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_vertical_view_jitter_offset, offsetof(weaponInfo_t, vertViewJitter) == 0x4a8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_grenade_splash_vehicle_damage_scale_offset,
                          offsetof(weaponInfo_t, grenadeSplashVehicleDamageScale) == 0x4ac);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_script_offset, offsetof(weaponInfo_t, script) == 0x4b0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsFireDelayRate_offset, offsetof(weaponInfo_t, adsFireDelayRate) == 0x4b4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsFireDelayOutRate_offset, offsetof(weaponInfo_t, adsFireDelayOutRate) == 0x4b8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_aiOverlayDescription_offset, offsetof(weaponInfo_t, aiOverlayDescription) == 0x0c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_gunModel_offset, offsetof(weaponInfo_t, gunModel) == 0x10);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_idleAnim_offset, offsetof(weaponInfo_t, idleAnim) == 0x1c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsDownAnim_offset, offsetof(weaponInfo_t, adsDownAnim) == 0x74);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_stackable_offset, offsetof(weaponInfo_t, stackable) == 0x88);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsZoomInFrac_offset, offsetof(weaponInfo_t, adsZoomInFrac) == 0x278);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsZoomOutFrac_offset, offsetof(weaponInfo_t, adsZoomOutFrac) == 0x27c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsOverlayShader_offset, offsetof(weaponInfo_t, adsOverlayShader) == 0x280);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsBobFactor_offset, offsetof(weaponInfo_t, adsBobFactor) == 0x290);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsViewBobScale_offset, offsetof(weaponInfo_t, adsViewBobScale) == 0x294);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_idleSwayAds_offset, offsetof(weaponInfo_t, idleSwayAds) == 0x2cc);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_idleSwayHip_offset, offsetof(weaponInfo_t, idleSwayHip) == 0x2d0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_idleSwayCrouchScale_offset, offsetof(weaponInfo_t, idleSwayCrouchScale) == 0x2d4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_idleSwayProneScale_offset, offsetof(weaponInfo_t, idleSwayProneScale) == 0x2d8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_gunMaxPitch_offset, offsetof(weaponInfo_t, gunMaxPitch) == 0x2dc);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_gunMaxYaw_offset, offsetof(weaponInfo_t, gunMaxYaw) == 0x2e0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_recoilReturnAds_offset, offsetof(weaponInfo_t, recoilReturnAds) == 0x3e4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_recoilFrictionAds_offset, offsetof(weaponInfo_t, recoilFrictionAds) == 0x3f0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_recoilReturnHip_offset, offsetof(weaponInfo_t, recoilReturnHip) == 0x42c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_recoilFrictionHip_offset, offsetof(weaponInfo_t, recoilFrictionHip) == 0x438);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_swayShellShockScale_offset, offsetof(weaponInfo_t, swayShellShockScale) == 0x2fc);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_clipRequired_offset, offsetof(weaponInfo_t, clipRequired) == 0x340);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsViewErrorMin_offset, offsetof(weaponInfo_t, adsViewErrorMin) == 0x330);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsViewErrorMax_offset, offsetof(weaponInfo_t, adsViewErrorMax) == 0x334);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsPitchOffset_offset, offsetof(weaponInfo_t, adsPitchOffset) == 0x3c8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_raiseTime_offset, offsetof(weaponInfo_t, raiseTime) == 0x230);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_raiseInterruptTime_offset, offsetof(weaponInfo_t, raiseInterruptTime) == 0x234);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_meleeTime_offset, offsetof(weaponInfo_t, meleeTime) == 0x23c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reloadEmptyTime_offset, offsetof(weaponInfo_t, reloadEmptyTime) == 0x244);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reloadAddTime_offset, offsetof(weaponInfo_t, reloadAddTime) == 0x248);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reloadStartTime_offset, offsetof(weaponInfo_t, reloadStartTime) == 0x24c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reloadStartAddTime_offset, offsetof(weaponInfo_t, reloadStartAddTime) == 0x250);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_lowerTime_offset, offsetof(weaponInfo_t, lowerTime) == 0x258);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_altSwitchLowerTime_offset, offsetof(weaponInfo_t, altSwitchLowerTime) == 0x260);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_segmentedReload_offset, offsetof(weaponInfo_t, segmentedReload) == 0x35c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reloadAmmoAdd_offset, offsetof(weaponInfo_t, reloadAmmoAdd) == 0x360);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_raiseEnabled_offset, offsetof(weaponInfo_t, raiseEnabled) == 0x324);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_ammoPickupSound_offset, offsetof(weaponInfo_t, ammoPickupSound) == 0xa0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reticleCenter_offset, offsetof(weaponInfo_t, reticleCenter) == 0x108);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_reticleSide_offset, offsetof(weaponInfo_t, reticleSide) == 0x10c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_worldModel_offset, offsetof(weaponInfo_t, worldModel) == 0x1c8);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_sharedAmmoCapIndex_offset, offsetof(weaponInfo_t, sharedAmmoCapIndex) == 0x200);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_meleeWindup_offset, offsetof(weaponInfo_t, meleeWindup) == 0x228);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireTime_offset, offsetof(weaponInfo_t, fireTime) == 0x22c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_specialFireDelay_offset, offsetof(weaponInfo_t, specialFireDelay) == 0x238);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_specialTimeThreshold_offset, offsetof(weaponInfo_t, specialTimeThreshold) == 0x268);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_adsViewKickCenterSpeed_offset, offsetof(weaponInfo_t, adsViewKickCenterSpeed) == 0x404);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_hipViewKickCenterSpeed_offset, offsetof(weaponInfo_t, hipViewKickCenterSpeed) == 0x44c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireRecoilAdsPitchMin_offset, offsetof(weaponInfo_t, fireRecoilAdsPitchMin) == 0x3d4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireRecoilAdsYawMax_offset, offsetof(weaponInfo_t, fireRecoilAdsYawMax) == 0x3e0);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireViewkickAdsPitchMin_offset, offsetof(weaponInfo_t, fireViewkickAdsPitchMin) == 0x3f4);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireViewkickAdsYawMax_offset, offsetof(weaponInfo_t, fireViewkickAdsYawMax) == 0x400);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireRecoilHipPitchMin_offset, offsetof(weaponInfo_t, fireRecoilHipPitchMin) == 0x41c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireRecoilHipYawMax_offset, offsetof(weaponInfo_t, fireRecoilHipYawMax) == 0x428);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireViewkickHipPitchMin_offset, offsetof(weaponInfo_t, fireViewkickHipPitchMin) == 0x43c);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_fireViewkickHipYawMax_offset, offsetof(weaponInfo_t, fireViewkickHipYawMax) == 0x448);
WEAPON_TYPE_LAYOUT_ASSERT(q_weapon_info_size, sizeof(weaponInfo_t) == 0x4bc);
#endif

#undef WEAPON_TYPE_LAYOUT_ASSERT

#endif
