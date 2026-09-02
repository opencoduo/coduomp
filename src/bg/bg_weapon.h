#ifndef BG_WEAPON_H
#define BG_WEAPON_H

#include "bg_bob.h"
#include "bg_bullet.h"
#include "qcommon/bg_item_types.h"
#include "bg_weapon_position.h"
#include "qcommon/entity_state_types.h"
#include "qcommon/info.h"
#include "qcommon/player_state_types.h"
#include "qcommon/weapon_types.h"

#include <stdint.h>

/* Consumer-owned tables used by the common BG weapon-registration passes. */
extern gitem_t bg_itemlist[];
extern int32_t bg_numWeapons;
extern weaponInfo_t **bg_weaponInfos;

extern const char *bg_ammoTypeNames[MAX_AMMO_TYPES];
extern int32_t bg_ammoTypeMax[MAX_AMMO_TYPES];
extern int32_t bg_numAmmoTypes;

extern const char *bg_sharedAmmoCapNames[MAX_AMMO_TYPES];
extern int32_t bg_sharedAmmoCapSizes[MAX_AMMO_TYPES];
extern int32_t bg_numSharedAmmoCaps;

extern const char *bg_ammoClipNames[MAX_AMMO_TYPES];
extern int32_t bg_ammoClipSizes[MAX_AMMO_TYPES];
extern int32_t bg_numAmmoClips;
/* Canonical symbolic-name tables used by weapon-file parsing.  Each table has
 * its own enum-defined bound even though the original linker placed the seven
 * arrays consecutively. */
extern const char *const bg_weaponTypeNames[WEAPTYPE_COUNT];
extern const char *const bg_weaponOverlayReticleNames[WEAPON_OVERLAY_RETICLE_COUNT];
extern const char *const bg_weaponSlotNames[WEAPSLOT_COUNT];
extern const char *const bg_weaponStanceNames[WEAPON_STANCE_COUNT];
extern const char *const bg_weaponClassNames[WEAPCLASS_COUNT];
extern const char *const bg_weaponAmmoTypeNames[WEAPON_AMMO_TYPE_COUNT];
extern const char *const bg_weaponGrenadeTypeNames[WEAPON_GRENADE_TYPE_COUNT];
extern const parseField_t bg_weaponFieldDefs[BG_WEAPON_FIELD_COUNT];

const weaponInfo_t *BG_GetInfoForWeapon(int32_t weapon);
const char *BG_GetWeaponTypeName(int32_t weaponType);
int32_t BG_ClipForWeapon(int32_t weapon);
int32_t BG_AmmoForWeapon(int32_t weapon);
int32_t BG_WeaponIsClipOnly(int32_t weapon);
int32_t BG_GetWeaponForInfo(const weaponInfo_t *weaponInfo);
int32_t BG_GetNumWeapons(void);
int32_t BG_GetNumAmmoTypes(void);
int32_t BG_GetAmmoTypeMax(int32_t ammoIndex);
int32_t BG_GetNumAmmoClips(void);
int32_t BG_GetAmmoClipSize(int32_t clipIndex);
int32_t BG_GetSharedAmmoCapSize(int32_t sharedAmmoCapIndex);
const char *BG_GetAmmoTypeName(int32_t ammoIndex);
const char *BG_GetAmmoClipName(int32_t clipIndex);
int32_t BG_GetAmmoTypeForName(const char *name);
int32_t BG_GetAmmoClipForName(const char *name);
int32_t BG_GetWeaponSlotForName(const char *name);
const char *BG_GetWeaponSlotNameForIndex(int32_t slot);
int32_t BG_GetWeaponIndexForName(const char *name);
qboolean BG_IsAimDownSightWeapon(int32_t weapon);
#if defined(WINDOWS_BEHAVIOR)
long double BG_GetMinSpreadForWeapon(const playerState_t *ps, int32_t weapon, int32_t time, int32_t isAds);
#else
float BG_GetMinSpreadForWeapon(const playerState_t *ps, int32_t weapon, int32_t time, int32_t isAds);
#endif
qboolean BG_ParseWeaponInfoSpecificFieldType(void *weaponInfoBase, const char *value, int32_t fieldType);
void BG_WeaponFireRecoil(playerState_t *ps, vec2_t recoilVelocity, vec3_t viewKick);
int32_t BG_GetMaxPickupableAmmo(const playerState_t *ps, int32_t weapon);
int32_t BG_GetTotalAmmoReserve(const playerState_t *ps, int32_t weapon);
int32_t BG_GetEmptySlotForWeapon(const playerState_t *ps, int32_t weapon);
int32_t BG_GetStackSlotForWeapon(const playerState_t *ps, int32_t weapon, int32_t preferredSlot);
qboolean BG_SetPlayerWeaponForSlot(playerState_t *ps, int32_t slot, int32_t weapon);
int32_t BG_IsPlayerWeaponInSlot(const playerState_t *ps, int32_t weapon, qboolean includeAltWeapons);
qboolean BG_IsPlayerWeaponAnAlt(int32_t weapon, int32_t altWeapon);
qboolean BG_GivePlayerWeapon(playerState_t *ps, int32_t weapon);
int32_t BG_TakePlayerWeapon(playerState_t *ps, int32_t weapon);
qboolean BG_CanItemBeGrabbed(const entityState_t *item, const playerState_t *ps, int32_t traceMode);
gitem_t *BG_FindItemForWeapon(int32_t weapon);
gitem_t *BG_FindItem(const char *pickupName);

void BG_FillInWeaponItems(void);
void BG_SetupWeaponADSRates(void);
void BG_SetupAmmoIndexes(void);
void BG_SetupSharedAmmoIndexes(void);
void BG_SetupClipIndexes(void);
void BG_SetupAltWeaponIndexes(void);

#endif
