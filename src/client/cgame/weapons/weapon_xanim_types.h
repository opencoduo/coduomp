#ifndef CGAME_WEAPON_XANIM_TYPES_H
#define CGAME_WEAPON_XANIM_TYPES_H

/*
 * Indices in the 24-node first-person weapon XAnim tree constructed by
 * CG_RegisterWeapon. Slots 1..23 correspond exactly, in order, to the
 * weaponInfo_t animation-name fields copied into CG_RegisterWeapon's
 * animNames[] initializer.
 *
 * This is a cgame-local tree-node domain, not the shared pmWeaponAnim_t domain
 * stored in playerState_t.weaponAnim. CG_WeaponRunXModelAnims maps the shared
 * player-state value to one of these local nodes (for example,
 * PM_WEAPON_ANIM_FIRE_LASTSHOT maps to WEAPON_XANIM_LAST_SHOT).
 */
typedef enum weaponXAnimIndex_e {
    WEAPON_XANIM_ROOT          = 0,
    WEAPON_XANIM_IDLE          = 1,
    WEAPON_XANIM_EMPTY_IDLE    = 2,
    WEAPON_XANIM_FIRE          = 3,
    WEAPON_XANIM_HOLD_FIRE     = 4,
    WEAPON_XANIM_LAST_SHOT     = 5,
    WEAPON_XANIM_RECHAMBER     = 6,
    WEAPON_XANIM_MELEE         = 7,
    WEAPON_XANIM_RELOAD        = 8,
    WEAPON_XANIM_RELOAD_EMPTY  = 9,
    WEAPON_XANIM_RELOAD_START  = 10,
    WEAPON_XANIM_RELOAD_END    = 11,
    WEAPON_XANIM_RAISE         = 12,
    WEAPON_XANIM_DROP          = 13,
    WEAPON_XANIM_ALT_RAISE     = 14,
    WEAPON_XANIM_ALT_DROP      = 15,
    WEAPON_XANIM_ADS_FIRE      = 16,
    WEAPON_XANIM_ADS_LAST_SHOT = 17,
    WEAPON_XANIM_ADS_RECHAMBER = 18,
    WEAPON_XANIM_LMG_DEPLOYED  = 19,
    WEAPON_XANIM_LMG_DEPLOY    = 20,
    WEAPON_XANIM_LMG_BREAKDOWN = 21,
    WEAPON_XANIM_ADS_UP        = 22,
    WEAPON_XANIM_ADS_DOWN      = 23,
    WEAPON_XANIM_COUNT         = 24
} weaponXAnimIndex_t;

#endif
