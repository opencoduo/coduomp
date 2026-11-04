/*
 * Source reconstruction for item pickup helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "recovered_game.h"
#include "bg/bg_pmove.h"
#include "game_globals.h"
#include "level_locals.h"
#include "scr_vm.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "qcommon/info.h"
#include "compat/crt/qsort_compat.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87_eval.h"
#include "compat/coduo_native_x87.h"

#define DROPPED_WEAPON_SLOT_COUNT 32
#define DROPPED_WEAPON_MIN_SLOTS 1
#define ITEM_RESPAWN_PACKED_STATE (0x407c0000u | 0x00000008u)
#define ITEM_SPAWN_PACKED_STATE (0x407c0000u | 0x00000108u)
#define ITEM_SVF_RESPAWNING 0x00000001u
#define ITEM_DELAYED_SPAWN_MS 200
#define ITEM_WEAPON_ANGLE_ADJUST 90.0f
#define ITEM_DROP_TRACE_DISTANCE 4096.0f
#define ITEM_STARTSOLID_RETRY_Z 15.0f
#define ITEM_TRACE_CONTENTS 0x411
#define DROPPED_ITEM_CLIPMASK 0x81
#define DROPPED_ITEM_FLAGS 0x00000010u
#define DROPPED_ITEM_CLEAR_OWNER_MS 1000
#define DROPPED_ITEM_PICKUP_FREE_DELAY_MS 100
#define DROPPED_ITEM_THROW_SPEED 150.0f
#define DROPPED_ITEM_THROW_UP 200.0f
#define DROPPED_ITEM_THROW_UP_RANDOM 50.0f
#define DROPPED_WEAPON_TAG_YAW_RANDOM 50.0f
#define DROPPED_WEAPON_TAG_PITCH_RANDOM 40.0f
#define DROPPED_WEAPON_TAG_ROLL_RANDOM 60.0f
#define WEAPON_REDROP_TRACE_DISTANCE 256.0f
#define ITEM_DEFAULT_CLIPMASK 0x491
#define ITEM_POINT_CONTENTS_MASK 0x80000000u
/* Item spawn flags. Entity/server-state bits use the shared FL_*, EF_*, and
 * SVF_* domains rather than item-specific aliases. */
#define ITEM_SPAWNFLAG_NO_DROP_TO_FLOOR 0x00000001u
#define ITEM_SPAWNFLAG_WEAPON_RESPAWN 0x00000008u
#define ITEM_BOUNCE_RETRACE_Z 128.0f
#define ITEM_BOUNCE_REFLECTION_SCALE -2.0f
#define ITEM_BOUNCE_STOP_Z 40.0f
#define ITEM_BOUNCE_SETTLE_MIN_Z 0.5f
#define ITEM_BOUNCE_SETTLE_RANDOM_Z 0.5f
#define PICKUP_AMMO_RESPAWN_TIME 40
#define PICKUP_HEALTH_NO_RESPAWN -1
#define PICKUP_HEALTH_SMALL 5
#define PICKUP_HEALTH_LARGE 100
#define ITEM_REGISTERED_COUNT 256
#define ITEM_CONFIGSTRING_REGISTERED 8
#define ITEM_REGISTERED_NIBBLE_BITS 4
#define ITEM_INDEX_MIN 1
#define ITEM_INDEX_MAX 133
#define ITEM_TOUCH_XY_RANGE 36.0f
#define ITEM_TOUCH_Z_ABOVE 18.0f
#define ITEM_TOUCH_Z_BELOW 88.0f
#define ITEM_TOUCH_MANUAL 0
#define ITEM_TOUCH_AUTOGRAB 2
#define GCLIENT_OFFSET_WEAPON_SLOT_BASE 0x544
#define PICKUP_EVENT_EXISTING_WEAPON_AMMO EV_AMMO_PICKUP
#define CLIENT_CLEAN_NAME_LEN 64
#define WEAPON_FILE_BUFFER_SIZE 8192
#define WEAPON_FILE_LIST_BUFFER_SIZE 4096
#define WEAPON_FILE_MAX_STRING_CHARS 8192
#define WEAPON_CONFIGSTRING_FILES 7
#define HINTSTRING_WEAPON_MAX 32
#define WEAPON_FILE_MAGIC "WEAPONFILE"
#define WEAPON_FILE_LIST_EXTENSION ""
#define WEAPON_AMMO_NONE_NAME "none"
const char *bg_ammoTypeNames[MAX_AMMO_TYPES];
int32_t bg_ammoTypeMax[MAX_AMMO_TYPES];
int32_t bg_numAmmoTypes;
const char *bg_sharedAmmoCapNames[MAX_AMMO_TYPES];
int32_t bg_sharedAmmoCapSizes[MAX_AMMO_TYPES];
int32_t bg_numSharedAmmoCaps;
const char *bg_ammoClipNames[MAX_AMMO_TYPES];
int32_t bg_ammoClipSizes[MAX_AMMO_TYPES];
int32_t bg_numAmmoClips;
int bg_numWeapons;
weaponInfo_t **bg_weaponInfos;
const char *bg_weaponEmptyString;
const char *bg_szWeaponsFolder = "weapons/mp";

/* Stock addresses each registration flag with scale four and clears this
 * 256-entry table as one 0x400-byte span. */
int32_t itemRegistered[ITEM_REGISTERED_COUNT];
const int bg_numItems = 134;
/* NOT_FROM_ORIGINAL_SOURCE: local item table accessor used by recovered item
 * code; no standalone generated entry. Decompiler evidence indexes
 * `bg_itemlist + index * 0x30`, which is source-level `bg_itemlist[index]`
 * for the original i386 sizeof(gitem_t). */
static gitem_t *game_compat_item_def_for_index(int itemIndex)
{
    return &bg_itemlist[itemIndex];
}

/* NOT_FROM_ORIGINAL_SOURCE: inverse of game_compat_item_def_for_index; uses the original
 * item-table pointer arithmetic recovered from the decompiler. */
static int game_compat_item_index_from_def(const gitem_t *item)
{
    return (int)(item - bg_itemlist);
}

/* Item definitions have module lifetime. Weapon setup fills the active
 * weapon rows and resolves ammo bindings without resetting retained rows. */
#define BG_EMPTY_WEAPON_ITEM(name) { name, "", "", "", "", "", "", 0, IT_BAD, 0, 0, 0 }
gitem_t bg_itemlist[BG_ITEMLIST_SLOT_COUNT] = {
    { 0 },
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w01\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w02\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w03\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w04\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w05\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w06\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w07\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w08\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w09\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w10\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w11\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w12\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w13\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w14\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w15\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w16\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w17\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w18\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w19\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w20\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w21\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w22\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w23\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w24\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w25\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w26\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w27\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w28\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w29\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w30\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w31\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w32\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w33\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w34\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w35\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w36\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w37\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w38\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w39\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w40\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w41\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w42\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w43\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w44\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w45\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w46\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w47\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w48\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w49\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w50\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w51\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w52\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w53\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w54\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w55\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w56\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w57\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w58\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w59\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w60\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w61\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w62\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w63\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w64\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w65\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w66\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w67\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w68\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w69\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w70\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w71\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w72\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w73\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w74\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w75\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w76\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w77\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w78\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w79\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w80\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w81\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w82\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w83\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w84\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w85\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w86\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w87\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w88\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w89\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w90\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w91\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w92\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w93\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w94\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w95\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w96\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w97\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w98\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w99\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w100\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w101\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w102\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w103\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w104\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w105\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w106\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w107\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w108\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w109\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w110\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w111\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w112\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w113\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w114\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w115\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w116\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w117\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w118\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w119\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w120\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w121\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w122\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w123\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w124\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w125\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w126\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w127\""),
    BG_EMPTY_WEAPON_ITEM("emptyitem_\"w128\""),
    { "item_ammo_stielhandgranate_open", "grenade_pickup",
        "xmodel/ammo_stielhandgranate1", NULL,
        "gfx/icons/hud@steilhandgrenate", "gfx/icons/hud@steilhandgrenate",
        "Stielhandgranate_mp Ammo Open", 10, IT_AMMO, -1, -1, -1 },
    { "item_ammo_stielhandgranate_closed", "grenade_pickup",
        "xmodel/ammo_stielhandgranate2", NULL,
        "gfx/icons/hud@steilhandgrenate", "gfx/icons/hud@steilhandgrenate",
        "Stielhandgranate_mp Ammo Closed", 10, IT_AMMO, -1, -1, -1 },
    { "item_health_small", "health_pickup_small", "xmodel/health_small", NULL,
        "icons/iconh_small", NULL, "Small Health", 10, IT_HEALTH, 0, 0,
        0 },
    { "item_health", "health_pickup_medium", "xmodel/health_medium", NULL,
        "icons/iconh_med", NULL, "Med Health", 25, IT_HEALTH, 0, 0,
        0 },
    { "item_health_large", "health_pickup_large", "xmodel/health_large",
        NULL, "icons/iconh_large", NULL, "Large Health", 50,
        IT_HEALTH, 0, 0, 0 },
    { 0 }
};
#undef BG_EMPTY_WEAPON_ITEM

/* VERIFIED_DECOMPILER(0x1fba8, 2fba8_BG_PlayerTouchesItem.c, VERIFY-WAVE3-ITEM-TOUCH-VISIBILITY-2026-06-17): DATAFLOW_VERIFIED - trajectory evaluation, XY/Z touch ranges, and boolean return checked against current decompiler output. */
int BG_PlayerTouchesItem(gclient_t *client, gentity_t *itemEnt, int time)
{
    vec3_t itemOrigin;

    BG_EvaluateTrajectory(&itemEnt->s.pos, time, itemOrigin);

    /* Compare the live coordinate differences before any float store. */
    if ((WIDE_FLOAT)client->ps.psOrigin[0] - itemOrigin[0] > ITEM_TOUCH_XY_RANGE ||
        (WIDE_FLOAT)client->ps.psOrigin[0] - itemOrigin[0] < -ITEM_TOUCH_XY_RANGE ||
        (WIDE_FLOAT)client->ps.psOrigin[1] - itemOrigin[1] > ITEM_TOUCH_XY_RANGE ||
        (WIDE_FLOAT)client->ps.psOrigin[1] - itemOrigin[1] < -ITEM_TOUCH_XY_RANGE ||
        (WIDE_FLOAT)client->ps.psOrigin[2] - itemOrigin[2] > ITEM_TOUCH_Z_ABOVE ||
        (WIDE_FLOAT)client->ps.psOrigin[2] - itemOrigin[2] < -ITEM_TOUCH_Z_BELOW) {
        return qfalse;
    }

    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: local configstring formatting helper factored from RegisterItem/SaveRegisteredItems. */
static char game_compat_registered_items_config_char(int value)
{
    if (value < 10) {
        return (char)('0' + value);
    }
    return (char)('W' + value);
}

/* NOT_FROM_ORIGINAL_SOURCE: local rand scaling helper factored from generated bounce code. */
static float game_compat_item_random_settled_height(void)
{
    /* Stock 0x566a8: fild rand /DENOM * 0.5f + 0.5f, one float store. */
    return (float)((long double)coduo_server_rand_unit() *
                       (long double)ITEM_BOUNCE_SETTLE_RANDOM_Z +
                   (long double)ITEM_BOUNCE_SETTLE_MIN_Z);
}

/* NOT_FROM_ORIGINAL_SOURCE: names the Linux gentity +0x17e item auto-touch
 * byte without changing the shared mover/vehicle active-state layout. */
static uint8_t *game_compat_item_auto_touch_flag(gentity_t *ent)
{
    return &ent->activeState;
}

/* NOT_FROM_ORIGINAL_SOURCE: local cvar clamp helper factored from generated drop code. */
static int game_compat_dropped_weapon_slot_limit(void)
{
    int limit = g_maxDroppedWeapons.integer;

    if (limit < DROPPED_WEAPON_MIN_SLOTS) {
        return DROPPED_WEAPON_MIN_SLOTS;
    }
    if (limit > DROPPED_WEAPON_SLOT_COUNT) {
        return DROPPED_WEAPON_SLOT_COUNT;
    }
    return limit;
}

/* NOT_FROM_ORIGINAL_SOURCE: local bounds helper factored from generated spawn/drop code. */
static void game_compat_set_item_bounds(gentity_t *ent, itemType_t itemType)
{
    float *mins = ent->mins;
    float *maxs = ent->maxs;

    mins[0] = -1.0f;
    mins[1] = -1.0f;
    maxs[0] = 1.0f;
    maxs[1] = 1.0f;

    if (itemType == IT_WEAPON) {
        mins[2] = -1.0f;
        maxs[2] = 1.0f;
    } else {
        mins[2] = 0.0f;
        maxs[2] = 2.0f;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from GetFreeCueSpot client-state checks. */
static qboolean game_compat_level_client_is_active(int clientNum)
{
    gclient_t *client = &level.clients[clientNum];
    return client->connectedState == CON_CONNECTED &&
           client->sessionState == SESS_STATE_PLAYING;
}

/* VERIFIED_DECOMPILER(0x54d84, 64d84_FUN_00064d84.c, VERIFY-PACKET-ITEM-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED - slot clamp, empty-slot return, active-client nearest-distance scan, farthest occupied slot selection, delayed free scheduling, and return slot checked against current decompiler output. */
int GetFreeCueSpot(void)
{
    int slotLimit = game_compat_dropped_weapon_slot_limit();
    int farthestSlot = 0;
    float farthestNearestDistance = -1.0f;

    for (int slot = 0; slot < slotLimit; slot++) {
        gentity_t *dropped = level.droppedWeaponSlots[slot];
        float nearestClientDistance = 9.99998e11f; /* original float32 0x5368d487 */

        if (dropped == 0) {
            return slot;
        }

        for (int clientNum = 0; clientNum < level.maxclients; clientNum++) {
            if (game_compat_level_client_is_active(clientNum)) {
                float distance = (float)VectorDistanceSquared(
                    g_entities[clientNum].currentOrigin,
                    dropped->currentOrigin);

                if (distance < nearestClientDistance) {
                    nearestClientDistance = distance;
                }
            }
        }

        if (farthestNearestDistance < nearestClientDistance) {
            farthestNearestDistance = nearestClientDistance;
            farthestSlot = slot;
        }
    }

    level.droppedWeaponSlots[farthestSlot]->think = G_FreeEntity;
    level.droppedWeaponSlots[farthestSlot]->nextthink = coduo_int32_from_bits(
        (uint32_t)level.time + UINT32_C(1));
    return farthestSlot;
}

/* VERIFIED_DECOMPILER(0x54f16, 64f16_DroppedItemClearOwner.c, VERIFY-PACKET-ITEM-LIFECYCLE-2026-06-17): DATAFLOW_VERIFIED - callback signature and +0x94 owner/clientNum store to ENTITYNUM_WORLD checked against current decompiler output. */
void DroppedItemClearOwner(gentity_t *ent)
{
    ent->s.clientNum = ENTITYNUM_WORLD;
}

/* VERIFIED_DECOMPILER(0x545db, 645db_RespawnItem.c, VERIFY-WAVE3-ITEM-TOUCH-VISIBILITY-2026-06-17): DATAFLOW_VERIFIED - respawn team selection, visibility/content flag restore, link, respawn event, and nextthink clear checked against current decompiler output. */
void RespawnItem(gentity_t *ent)
{
    if ((uint16_t)ent->teamName != 0) {
        int teamCount = 0;
        int selected;

        if (ent->teamMaster == 0) {
            G_Error("RespawnItem: bad teammaster");
        }

        ent = ent->teamMaster;
        for (gentity_t *candidate = ent; candidate != 0;
             candidate = candidate->teamChain) {
            teamCount++;
        }

        selected = coduo_server_randrange(0, teamCount);
        for (int index = 0; index < selected; index++) {
            ent = ent->teamChain;
        }
    }

    ent->scriptContents = ITEM_RESPAWN_PACKED_STATE;
    ent->flags &= ~FL_NOCLIENT;
    ent->svFlags &= ~ITEM_SVF_RESPAWNING;
    trap_LinkEntity(ent);
    G_AddEvent(ent, EV_ITEM_RESPAWN, 0);
    ent->nextthink = 0;
}

/* VERIFIED_DECOMPILER(0x54700, 64700_Touch_Item_Auto.c, VERIFY-WAVE3-ITEM-TOUCH-VISIBILITY-2026-06-17): DATAFLOW_VERIFIED - +0x17e auto-touch byte store and Touch_Item argument order checked against current decompiler output. */
void Touch_Item_Auto(gentity_t *itemEnt, gentity_t *other, int traceMode)
{
    *game_compat_item_auto_touch_flag(itemEnt) = 1;
    Touch_Item(itemEnt, other, traceMode);
}

/* VERIFIED_DECOMPILER(0x559af, 659af_Use_Item.c, VERIFY-WAVE3-ITEM-TOUCH-VISIBILITY-2026-06-17): DATAFLOW_VERIFIED - generated body reads only the item entity and calls RespawnItem; three-argument signature preserves the +0x220 use callback ABI. */
void Use_Item(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)other;
    (void)activator;
    RespawnItem(ent);
}

/* VERIFIED_DECOMPILER(0x55d33, 65d33_ClearRegisteredItems.c, VERIFY-P1-ITEMS-2026-06-17): DATAFLOW_VERIFIED - memset length, forced slot zero registration, and return behavior checked against current decompiler output. */
void ClearRegisteredItems(void)
{
    memset(itemRegistered, 0, sizeof(itemRegistered));
    itemRegistered[0] = 1;
}

/* VERIFIED_DECOMPILER(0x55d75, 65d75_SaveRegisteredItems.c, VERIFY-P1-ITEMS-2026-06-17): DATAFLOW_VERIFIED - dirty flag clear, registered-item packing loop, trailing nibble handling, terminator, and configstring write checked against current decompiler output. */
void SaveRegisteredItems(void)
{
    char config[276];
    int out = 0;
    int bits = 0;
    int bit = 0;

    level.registeredItemsDirty = 0;

    for (int itemIndex = 0; itemIndex < bg_numItems; itemIndex++) {
        if (itemRegistered[itemIndex] != 0) {
            bits += 1 << bit;
        }

        bit++;
        if (bit == ITEM_REGISTERED_NIBBLE_BITS) {
            config[out++] = game_compat_registered_items_config_char(bits);
            bits = 0;
            bit = 0;
        }
    }

    if (bit != 0) {
        config[out++] = game_compat_registered_items_config_char(bits);
    }

    config[out] = '\0';
    trap_SetConfigstring(ITEM_CONFIGSTRING_REGISTERED, config);
}

/* VERIFIED_DECOMPILER(0x55f2a, 65f2a_RegisterItem.c, VERIFY-P1-ITEMS-2026-06-17): DATAFLOW_VERIFIED - duplicate guard, late-registration error path, weapon alt-chain registration, model precache calls, and dirty flag checked against current decompiler output. */
void RegisterItem(int itemIndex, int updateConfigString)
{
    if (itemRegistered[itemIndex] != 0) {
        return;
    }

    if (level.spawning == 0) {
        const char *name = game_compat_item_def_for_index(itemIndex)->pickupName;

        if ((name == 0 || *name == '\0') && itemIndex <= BG_GetNumWeapons()) {
            name = ((const weaponInfo_t *)BG_GetInfoForWeapon(itemIndex))->pickupName;
        }

        if (name == 0 || *name == '\0') {
            /* Intentional runtime fallback for late item-registration errors. */
            name = "<<unknown>>";
        }

        Scr_Error(va("game tried to register the item '%s' after initialization finished\n",
                     name));
    }

    itemRegistered[itemIndex] = 1;

    if (game_compat_item_def_for_index(itemIndex)->type == IT_WEAPON) {
        int weapon = itemIndex;

        do {
            const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);

            itemRegistered[weapon] = 1;
            G_ModelIndex(weaponInfo->worldModel);
            G_ModelIndex(weaponInfo->viewModel);
            G_ModelIndex(weaponInfo->pickupModel);
            G_ModelIndex(weaponInfo->clipModel);

            weapon = weaponInfo->altWeapon;
            if (weapon == 0) {
                break;
            }
        } while (weapon != itemIndex);
    } else {
        const char *worldModel = game_compat_item_def_for_index(itemIndex)->worldModel;

        if (worldModel != 0) {
            G_ModelIndex(worldModel);
        }
        const char *iconModel = game_compat_item_def_for_index(itemIndex)->iconModel;
        if (iconModel != 0) {
            G_ModelIndex(iconModel);
        }
    }

    if (updateConfigString != 0) {
        level.registeredItemsDirty = 1;
    }
}

/* VERIFIED_DECOMPILER(0x56128, 66128_IsItemRegistered.c, VERIFY-P1-ITEMS-2026-06-17): DATAFLOW_VERIFIED - itemRegistered indexed return checked against current decompiler output. */
qboolean IsItemRegistered(int itemIndex)
{
    return itemRegistered[itemIndex];
}

/* VERIFIED_DECOMPILER(0x559d2, 659d2_FinishSpawningItem.c, VERIFY-P1-ITEMS-2026-06-17): DATAFLOW_VERIFIED - item type lookup, floor trace, startsolid retry/free path, ground entity, origin/angle placement, weapon roll adjust, and link side effect checked against current decompiler output. */
void FinishSpawningItem(gentity_t *ent)
{
    if ((ent->spawnflags &
         ITEM_SPAWNFLAG_NO_DROP_TO_FLOOR) == 0) {
        trace_t trace;
        vec3_t end;
        vec3_t traceMins = {-1.0f, -1.0f, 0.0f};
        vec3_t traceMaxs = {1.0f, 1.0f, 2.0f};

        /* Linux RVA 0x55a14 and Windows RVA 0x29d82 select temporary
         * trace bounds through itemInfo; the entity's bounds stay intact. */
        if (ent->itemInfo->type == IT_WEAPON) {
            traceMins[2] = -1.0f;
            traceMaxs[2] = 1.0f;
        }
        ent->svFlags |= SVF_CAPSULE;
        ent->s.eFlags |= EF_CAPSULE;

        end[0] = ent->currentOrigin[0];
        end[1] = ent->currentOrigin[1];
        end[2] = ent->currentOrigin[2] - ITEM_DROP_TRACE_DISTANCE;

        trap_TraceCapsule(&trace, ent->currentOrigin,
                          traceMins,
                          traceMaxs,
                          end, ent->s.number, ITEM_TRACE_CONTENTS);

        if (trace.startsolid != 0) {
            vec3_t retryStart;

            retryStart[0] = ent->currentOrigin[0];
            retryStart[1] = ent->currentOrigin[1];
            retryStart[2] = ent->currentOrigin[2] - ITEM_STARTSOLID_RETRY_Z;
            end[0] = ent->currentOrigin[0];
            end[1] = ent->currentOrigin[1];
            end[2] = ent->currentOrigin[2] - ITEM_DROP_TRACE_DISTANCE;
            trap_TraceCapsule(&trace, retryStart,
                              traceMins,
                              traceMaxs,
                              end, ent->s.number, ITEM_TRACE_CONTENTS);
        }

        if (trace.startsolid != 0) {
            G_Printf("FinishSpawningItem: %s startsolid at %s\n",
                     SL_ConvertToString(ent->scriptClassname),
                     vtos(ent->currentOrigin));
            G_FreeEntity(ent);
            return;
        }

        ent->s.groundEntityNum = trace.entityNum;
        G_SetOrigin(ent, trace.endpos);

        if (trace.fraction < 1.0f) {
            axis_t axis;
            vec3_t angles;

            axis[2][0] = trace.normal[0];
            axis[2][1] = trace.normal[1];
            axis[2][2] = trace.normal[2];
            AngleVectors(ent->currentAngles, axis[0], 0, 0);
            CrossProduct(axis[2], axis[0], axis[1]);
            CrossProduct(axis[1], axis[2], axis[0]);
            AxisToAngles((const vec_t (*)[3])axis, angles);

            if (game_compat_item_def_for_index(ent->s.itemIndex)->type == IT_WEAPON) {
                angles[2] += ITEM_WEAPON_ANGLE_ADJUST;
            }

            G_SetAngle(ent, angles);
        }
    } else {
        G_SetOrigin(ent, ent->currentOrigin);
    }

    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x54f28, 64f28_LaunchItem.c, VERIFY-P1-ITEMS-2026-06-17): DATAFLOW_VERIFIED - registration, free dropped slot, entity fields, model/dobj setup, gravity trajectory, owner clear think, flags, link, and return checked against current decompiler output. */
gentity_t *LaunchItem(gitem_t *item, const float *origin, const float *velocity,
                      int ownerNum)
{
    int itemIndex = game_compat_item_index_from_def(item);
    itemType_t itemType = game_compat_item_def_for_index(itemIndex)->type;
    gentity_t *dropped;
    int slot;

    RegisterItem(itemIndex, 1);

    dropped = G_Spawn();
    slot = GetFreeCueSpot();
    level.droppedWeaponSlots[slot] = dropped;

    dropped->s.eType = ET_ITEM;
    dropped->s.itemIndex = itemIndex;
    G_SetConstString(&dropped->scriptClassname, item->classname);
    dropped->itemInfo = item;
    game_compat_set_item_bounds(dropped, itemType);
    dropped->svFlags |= SVF_CAPSULE;
    dropped->s.eFlags |= EF_CAPSULE;
    dropped->scriptContents = ITEM_SPAWN_PACKED_STATE;
    dropped->clipmask =
        DROPPED_ITEM_CLIPMASK;
    dropped->s.clientNum = ownerNum;

    G_SetModel(dropped, game_compat_item_def_for_index(itemIndex)->worldModel);
    G_DObjUpdate(dropped);
    dropped->touch = Touch_Item_Auto;
    G_SetOrigin(dropped, origin);

    dropped->s.pos.trType =
        TR_GRAVITY;
    dropped->s.pos.trTime = level.time;
    dropped->s.pos.trDelta[0] = velocity[0];
    dropped->s.pos.trDelta[1] = velocity[1];
    dropped->s.pos.trDelta[2] = velocity[2];

    dropped->think = DroppedItemClearOwner;
    dropped->nextthink = level.time + DROPPED_ITEM_CLEAR_OWNER_MS;
    dropped->flags = DROPPED_ITEM_FLAGS;
    trap_LinkEntity(dropped);
    return dropped;
}

/* VERIFIED_DECOMPILER(0x5518c, 6518c_Drop_Item.c, VERIFY-P1-ITEMS-2026-06-17): DATAFLOW_VERIFIED - throw angle, stationary/nonstationary velocity, randomized upward velocity, origin midpoint, and LaunchItem arguments checked against current decompiler output. */
gentity_t *Drop_Item(gentity_t *ent, gitem_t *item, float angleOffset,
                     int stationary)
{
    vec3_t angles;
    vec3_t velocity;
    vec3_t origin;

    angles[0] = 0.0f;
    angles[1] = ent->currentAngles[1] + angleOffset;
    angles[2] = 0.0f;

    if (stationary == 0) {
        AngleVectors(angles, velocity, 0, 0);
        velocity[0] *= DROPPED_ITEM_THROW_SPEED;
        velocity[1] *= DROPPED_ITEM_THROW_SPEED;
        velocity[2] *= DROPPED_ITEM_THROW_SPEED;
        /* Retain the random product and both additions until the float store.
         * Windows adds velocity before THROW_UP; Linux adds it afterward. */
        velocity[2] = ((WIDE_FLOAT)coduo_server_rand_signed_unit() * DROPPED_ITEM_THROW_UP_RANDOM + velocity[2]) + DROPPED_ITEM_THROW_UP;
    } else {
        velocity[0] = 0.0f;
        velocity[1] = 0.0f;
        velocity[2] = 0.0f;
    }

    origin[0] = ent->currentOrigin[0];
    origin[1] = ent->currentOrigin[1];
    /* Keep the bounds difference and half-height wide until adding the origin
     * and storing the completed coordinate as float. */
    origin[2] = ((WIDE_FLOAT)ent->maxs[2] - ent->mins[2]) * 0.5f + ent->currentOrigin[2];

    return LaunchItem(item, origin, velocity, ent->s.number);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Drop_Weapon generated body. */
static void game_compat_drop_weapon_default_ammo(int clipIndex, int *ammoCount, int *clipCount)
{
    int clipSize = BG_GetAmmoClipSize(clipIndex);
    /* Stock (v+1.0)*0.5 rounded once to float; (clipSize-1) fild'd raw *
     * ammoScale; ammoCount fild'd raw * (v*0.5+0.25) -> shim chains. */
    float ammoScale =
        (float)(((long double)coduo_server_rand_unit() + 1.0L) * 0.5L);

    *ammoCount = Game_RoundAmmoCount(
                     (float)((long double)(clipSize - 1) *
                             (long double)ammoScale)) +
                 1;
    *clipCount = Game_RoundAmmoCount(
        (float)((long double)*ammoCount *
                ((long double)coduo_server_rand_unit() * 0.5L + 0.25L)));
    *ammoCount -= *clipCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Drop_Weapon generated body. */
static void game_compat_drop_weapon_ranged_ammo(int clipIndex, int minAmmo, int maxAmmo,
                                 int *ammoCount, int *clipCount)
{
    int totalAmmo;

    if (maxAmmo == minAmmo) {
        totalAmmo = minAmmo;
    } else {
        totalAmmo = coduo_server_randrange(minAmmo, maxAmmo);
    }

    *ammoCount = totalAmmo;
    if (totalAmmo < 1) {
        *ammoCount = 0;
        *clipCount = 0;
        return;
    }

    totalAmmo = BG_GetAmmoClipSize(clipIndex);
    if (totalAmmo == 0) {
        *clipCount = 0;
    } else {
        *clipCount = coduo_server_randrange(0, totalAmmo);
    }

    if (*clipCount < *ammoCount) {
        *ammoCount -= *clipCount;
    } else {
        *clipCount = *ammoCount;
        *ammoCount = 0;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Drop_Weapon generated body. */
static void game_compat_drop_weapon_synthetic_ammo(int weapon, int clipIndex, int *ammoCount,
                                    int *clipCount)
{
    int maxAmmo = ((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->dropAmmoMax;
    int minAmmo = ((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->dropAmmoMin;

    if (maxAmmo < minAmmo) {
        int oldMax = maxAmmo;

        maxAmmo = minAmmo;
        minAmmo = oldMax;
    }

    if (maxAmmo == 0 && minAmmo == 0) {
        game_compat_drop_weapon_default_ammo(clipIndex, ammoCount, clipCount);
    } else if (maxAmmo < 0) {
        *ammoCount = 0;
        *clipCount = 0;
    } else {
        game_compat_drop_weapon_ranged_ammo(clipIndex, minAmmo, maxAmmo, ammoCount, clipCount);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Drop_Weapon generated body. */
static void game_compat_drop_weapon_place_at_tag(gentity_t *source, gentity_t *dropped,
                                 const char *tagName)
{
    DObjSkelMat tagMatrix;
    vec3_t tagAngles;
    vec3_t angularVelocity;

    if (G_DObjGetWorldTagMatrix(source, tagName, &tagMatrix) != 0) {
        trace_t trace;
        vec3_t start;

        /* The X half-extent rounds to float. Y stays wide through the
         * origin addition. Z rounds the bounds sum before halving. */
        WIDE_FLOAT sumX = (WIDE_FLOAT)source->maxs[0] + source->mins[0];
        WIDE_FLOAT sumY = (WIDE_FLOAT)source->maxs[1] + source->mins[1];
        WIDE_FLOAT sumZ = coduo_round_f32((WIDE_FLOAT)source->maxs[2] + source->mins[2]);
        start[0] = (WIDE_FLOAT)coduo_round_f32(sumX * 0.5f) + source->currentOrigin[0];
        start[1] = sumY * 0.5f + source->currentOrigin[1];
        start[2] = sumZ * 0.5f + source->currentOrigin[2];

        trap_TraceCapsule(&trace, start,
                          dropped->mins,
                          dropped->maxs,
                          tagMatrix.origin, source->s.number,
                          ITEM_TRACE_CONTENTS);

        dropped->s.pos.trBase[0] = trace.endpos[0];
        dropped->s.pos.trBase[1] = trace.endpos[1];
        dropped->s.pos.trBase[2] = trace.endpos[2];
        dropped->currentOrigin[0] = trace.endpos[0];
        dropped->currentOrigin[1] = trace.endpos[1];
        dropped->currentOrigin[2] = trace.endpos[2];
        dropped->s.pos.trTime = level.time;
        Axis4ToAngles(&tagMatrix, tagAngles);
    } else {
        tagAngles[0] = source->currentAngles[0];
        tagAngles[1] = source->currentAngles[1];
        tagAngles[2] = source->currentAngles[2];
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    tagAngles[2] += ITEM_WEAPON_ANGLE_ADJUST;
    (void)tagAngles[2];

    G_SetAngle(dropped, source->currentAngles);

    /* Round all three random products to float before publishing the trajectory. */
    angularVelocity[0] = (WIDE_FLOAT)coduo_server_rand_signed_unit() * DROPPED_WEAPON_TAG_YAW_RANDOM;
    angularVelocity[1] = (WIDE_FLOAT)coduo_server_rand_signed_unit() * DROPPED_WEAPON_TAG_PITCH_RANDOM;
    angularVelocity[2] = (WIDE_FLOAT)coduo_server_rand_signed_unit() * DROPPED_WEAPON_TAG_ROLL_RANDOM;
    dropped->s.apos.trType = TR_LINEAR;
    dropped->s.apos.trTime = level.time;
    dropped->s.apos.trDelta[0] = angularVelocity[0];
    dropped->s.apos.trDelta[1] = angularVelocity[1];
    dropped->s.apos.trDelta[2] = angularVelocity[2];
}

/* VERIFIED_DECOMPILER(0x552e6, 652e6_Drop_Weapon.c, VERIFY-P1-ITEMS-2026-06-17): DATAFLOW_VERIFIED - ownership weapon check, dropDisabled/clipRequired branches, ammo/clip transfer or synthetic ammo, dropped counts, tag-placement helper behavior, and return checked against current decompiler output. */
gentity_t *Drop_Weapon(gentity_t *ent, int weapon, const char *tagName)
{
    gclient_t *client = ent->client;
    int ammoIndex;
    int clipIndex;
    int ammoCount;
    int clipCount;
    gentity_t *dropped;

    if (client != 0 && Com_BitCheck(client->ps.weaponBits, weapon) == 0) {
        BG_TakePlayerWeapon(&client->ps, weapon);
        return 0;
    }

    ammoIndex = BG_AmmoForWeapon(weapon);
    clipIndex = BG_ClipForWeapon(weapon);

    if (((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->dropDisabled != 0) {
        return 0;
    }

    if (((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->clipRequired != 0 &&
        client->ps.clips[clipIndex] == 0) {
        BG_TakePlayerWeapon(&client->ps, weapon);
        return 0;
    }

    dropped = Drop_Item(ent, game_compat_item_def_for_index(weapon), 0.0f, 0);

    if (ent->client == 0) {
        game_compat_drop_weapon_synthetic_ammo(weapon, clipIndex, &ammoCount, &clipCount);
    } else {
        ammoCount = ent->client->ps.ammo[ammoIndex];
        ent->client->ps.ammo[ammoIndex] = 0;
        clipCount = ent->client->ps.clips[clipIndex];
        ent->client->ps.clips[clipIndex] = 0;
        BG_TakePlayerWeapon(&ent->client->ps, weapon);
    }

    dropped->itemCount = ammoCount;
    dropped->droppedClipCount = clipCount;

    if (dropped->itemCount == 0) {
        dropped->itemCount = -1;
    }
    if (dropped->droppedClipCount == 0) {
        dropped->droppedClipCount = -1;
    }

    if (tagName != 0) {
        game_compat_drop_weapon_place_at_tag(ent, dropped, tagName);
    }

    return dropped;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Pickup_Weapon generated body. */
static int game_compat_pickup_weapon_random_count(int weapon)
{
    int minAmmo = ((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->dropAmmoMin;
    int maxAmmo = ((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->dropAmmoMax;

    if (maxAmmo < minAmmo) {
        int oldMin = minAmmo;

        minAmmo = maxAmmo;
        maxAmmo = oldMin;
    }

    if (maxAmmo == 0 && minAmmo == 0) {
        int clipIndex = BG_ClipForWeapon(weapon);
        int clipSize = BG_GetAmmoClipSize(clipIndex);
        /* NOT_FROM_ORIGINAL_SOURCE: express the Windows count calculation with
         * its float rounding point and current-rounding-mode integer conversion. */
        static const float randomScale = 0x1p-15f;
        static const float randomOffset = 1.0f;
        static const float countScale = 0.5f;
        static const double roundingBias = 0x1p-30;
        int32_t randomValue = coduo_crt_rand();
        int32_t clipMultiplier = clipSize - 1;
        float storedCount = (float)(((long double)randomValue * randomScale + randomOffset) * clipMultiplier * countScale);

        /* The 15-bit random factor keeps the rounded count within int32_t for either long ABI. */
        return (int32_t)lrintl((long double)storedCount + roundingBias) + 1;
    }

    if (maxAmmo < 0) {
        return 0;
    }

    if (maxAmmo == minAmmo) {
        return minAmmo;
    }

    int count = coduo_server_randrange(minAmmo, maxAmmo);

    /* Only explicit ammo ranges clamp nonpositive results. */
    return count < 1 ? 0 : count;
}

/* VERIFIED_DECOMPILER(0x533b8, 633b8_Fill_Clip.c, VERIFY-P1-ITEMS2-2026-06-17): DATAFLOW_VERIFIED - weapon bounds guard, ammo/clip refill amount, and conditional stores checked against current decompiler output. */
void Fill_Clip(gclient_t *client, int weapon)
{
    int ammoIndex = BG_AmmoForWeapon(weapon);
    int clipIndex = BG_ClipForWeapon(weapon);
    int refill;

    if (weapon <= 0 || weapon > BG_GetNumWeapons()) {
        return;
    }

    refill = BG_GetAmmoClipSize(clipIndex) - client->ps.clips[clipIndex];
    if (client->ps.ammo[ammoIndex] < refill) {
        refill = client->ps.ammo[ammoIndex];
    }

    if (refill != 0) {
        client->ps.ammo[ammoIndex] -= refill;
        client->ps.clips[clipIndex] += refill;
    }
}

/* VERIFIED_DECOMPILER(0x53495, 63495_Add_Ammo.c, VERIFY-P1-ITEMS2-2026-06-17): DATAFLOW_VERIFIED - ammo addition, clip-only branch, fill-clip call, max clamps, pickup limit overflow handling, weapon removal, and return delta checked against current decompiler output. */
int Add_Ammo(gentity_t *ent, int weapon, int amount, qboolean fillClip)
{
    gclient_t *client = ent->client;
    int ammoIndex = BG_AmmoForWeapon(weapon);
    int clipIndex = BG_ClipForWeapon(weapon);
    int oldAmmo = client->ps.ammo[ammoIndex];
    int oldClip = client->ps.clips[clipIndex];
    qboolean clipOnly;

    client->ps.ammo[ammoIndex] += amount;
    clipOnly = BG_WeaponIsClipOnly(weapon) != 0;

    if (clipOnly) {
        BG_GivePlayerWeapon(&client->ps, weapon);
    }

    if (fillClip || clipOnly) {
        Fill_Clip(client, weapon);
    }

    if (clipOnly) {
        client->ps.ammo[ammoIndex] = 0;
    } else {
        int maxAmmo = BG_GetAmmoTypeMax(ammoIndex);

        if (maxAmmo < client->ps.ammo[ammoIndex]) {
            client->ps.ammo[ammoIndex] = BG_GetAmmoTypeMax(ammoIndex);
        }
    }

    if (BG_GetAmmoClipSize(clipIndex) < client->ps.clips[clipIndex]) {
        client->ps.clips[clipIndex] = BG_GetAmmoClipSize(clipIndex);
    }

    if (((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->sharedAmmoCapIndex >= 0) {
        int overflow = BG_GetMaxPickupableAmmo(&client->ps, weapon);

        if (overflow < 0) {
            if (!clipOnly) {
                client->ps.ammo[ammoIndex] += overflow;
                if (client->ps.ammo[ammoIndex] < 0) {
                    client->ps.ammo[ammoIndex] = 0;
                }
            } else {
                client->ps.clips[clipIndex] += overflow;
                if (client->ps.clips[clipIndex] < 1) {
                    client->ps.clips[clipIndex] = 0;
                    BG_TakePlayerWeapon(&client->ps, weapon);
                    return 0;
                }
            }
        }
    }

    return (client->ps.ammo[ammoIndex] - oldAmmo) +
           (client->ps.clips[clipIndex] - oldClip);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Pickup_Weapon generated body. */
static int game_compat_pickup_weapon_reserve_count(gentity_t *itemEnt, int weapon)
{
    int count = itemEnt->itemCount;
    int ammoIndex;
    int maxAmmo;

    if (count < 0) {
        return 0;
    }

    if (count == 0) {
        count = game_compat_pickup_weapon_random_count(weapon);
        itemEnt->itemCount = count;
    }

    ammoIndex = BG_AmmoForWeapon(weapon);
    maxAmmo = BG_GetAmmoTypeMax(ammoIndex);
    if (maxAmmo < itemEnt->itemCount) {
        itemEnt->itemCount = maxAmmo;
    }

    return itemEnt->itemCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Pickup_Weapon generated body. */
static int game_compat_pickup_weapon_clip_count(gentity_t *itemEnt, int weapon, int *reserveCount)
{
    int clipCount = itemEnt->droppedClipCount;
    int clipIndex;
    int clipSize;

    if (clipCount < 0) {
        return 0;
    }

    if (clipCount == 0) {
        if (itemEnt->itemCount < 0) {
            itemEnt->droppedClipCount = 0;
        } else {
            clipIndex = BG_ClipForWeapon(weapon);
            clipSize = BG_GetAmmoClipSize(clipIndex);
            itemEnt->droppedClipCount = clipSize;
            if (itemEnt->itemCount < itemEnt->droppedClipCount) {
                itemEnt->droppedClipCount = itemEnt->itemCount;
            }
            itemEnt->itemCount -= itemEnt->droppedClipCount;
            *reserveCount = itemEnt->itemCount;
        }
    }

    clipIndex = BG_ClipForWeapon(weapon);
    clipSize = BG_GetAmmoClipSize(clipIndex);
    if (clipSize < itemEnt->droppedClipCount) {
        itemEnt->droppedClipCount = clipSize;
    }

    return itemEnt->droppedClipCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Pickup_Weapon generated body. */
static void game_compat_pickup_weapon_trace_dropped_replacement(gentity_t *pickupItem,
                                                gentity_t *dropped)
{
    if ((pickupItem->flags & DROPPED_ITEM_FLAGS) == 0) {
        trace_t trace;
        vec3_t end;

        /* Keep the horizontal position and offset the vertical trace endpoint. */
        end[0] = dropped->currentOrigin[0];
        end[1] = dropped->currentOrigin[1];
        end[2] = dropped->currentOrigin[2] - WEAPON_REDROP_TRACE_DISTANCE;

        trap_Trace(&trace, dropped->currentOrigin,
                   dropped->mins,
                   dropped->maxs,
                   end, dropped->s.number,
                   dropped->clipmask);

        if (trace.fraction < 1.0f) {
            G_SetOrigin(dropped, trace.endpos);
            trap_LinkEntity(dropped);
        }
    } else {
        G_SetOrigin(dropped, pickupItem->currentOrigin);
        G_SetAngle(dropped, pickupItem->currentAngles);
        trap_LinkEntity(dropped);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Pickup_Weapon generated body. */
static gentity_t *game_compat_pickup_weapon_drop_for_slot(gentity_t *player, int weaponSlot)
{
    int weapon = (int)(int8_t)player->client->ps.weaponSlots[weaponSlot];

    return Drop_Weapon(player, weapon, 0);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Pickup_Weapon generated body. */
static int game_compat_pickup_weapon_drop_replacement(gentity_t *itemEnt, gentity_t *player,
                                       int weapon, const weaponInfo_t *weaponInfo,
                                       gentity_t **dropped)
{
    gclient_t *client = player->client;
    int currentWeapon = client->ps.currentWeapon;
    int slot = BG_GetStackSlotForWeapon(
        &client->ps, weapon,
        ((const weaponInfo_t *)BG_GetInfoForWeapon(currentWeapon))->slot);

    if (slot != 0) {
        *dropped = 0;
        return 1;
    }

    if (weaponInfo->slot ==
        ((const weaponInfo_t *)BG_GetInfoForWeapon(currentWeapon))->slot) {
        *dropped = Drop_Weapon(player, currentWeapon, 0);
    } else {
        int weaponSlot = weaponInfo->slot;

        if (weaponSlot >= WEAPSLOT_PISTOL &&
            weaponSlot <= WEAPSLOT_LAST_DROPPABLE) {
            *dropped = game_compat_pickup_weapon_drop_for_slot(player, weaponSlot);
        } else {
            for (weaponSlot = WEAPSLOT_PRIMARY_FIRST;
                 weaponSlot < WEAPSLOT_PRIMARY_LIMIT; weaponSlot++) {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                int ammoIndex = BG_AmmoForWeapon(weapon);
                int clipIndex = BG_ClipForWeapon(weapon);

                if (client->ps.ammo[ammoIndex] == 0 && client->ps.clips[clipIndex] == 0) {
                    *dropped = game_compat_pickup_weapon_drop_for_slot(player, weaponSlot);
                    break;
                }
            }

            if (weaponSlot >= WEAPSLOT_PRIMARY_LIMIT) {
                trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0,
                                       va("f \"GAME_CANT_GET_PRIMARY_WEAP_MESSAGE\""));
                return 0;
            }
        }
    }

    if (*dropped == 0) {
        if (BG_GetEmptySlotForWeapon(&player->client->ps, weapon) == 0) {
            return 0;
        }
    } else {
        game_compat_pickup_weapon_trace_dropped_replacement(itemEnt, *dropped);
    }

    return 1;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Pickup_Weapon generated body. */
static int game_compat_pickup_weapon_grant_new_weapon(gentity_t *itemEnt, gentity_t *player,
                                      int weapon, const weaponInfo_t *weaponInfo,
                                      int traceMode, gentity_t **dropped)
{
    gclient_t *client = player->client;

    *dropped = 0;

    if (client->ps.currentWeapon != 0) {
        if (Com_BitCheck(client->ps.weaponBits, client->ps.currentWeapon) == 0) {
            return 0;
        }

        if (BG_IsPlayerWeaponInSlot(&client->ps, client->ps.currentWeapon, 1) == 0) {
            int currentSlot = ((const weaponInfo_t *)BG_GetInfoForWeapon(client->ps.currentWeapon))->slot;

            if (BG_GetStackSlotForWeapon(&client->ps,
                                         client->ps.currentWeapon,
                                         currentSlot) == 0 &&
                BG_GetEmptySlotForWeapon(&client->ps, weapon) == 0) {
                Com_Printf("WARNING: cannot swap out a debug weapon "
                           "(can result from too many weapons given to the player)\n");
                return 0;
            }
        }
    }

    if (BG_GetEmptySlotForWeapon(&client->ps, weapon) == 0) {
        if (game_compat_pickup_weapon_drop_replacement(itemEnt, player, weapon, weaponInfo,
                                        dropped) == 0) {
            return 0;
        }
    }

    /* Drop placement and linking may publish updated player state. */
    BG_GivePlayerWeapon(&player->client->ps, weapon);
    if (traceMode == 0) {
        trap_SendServerCommand((uint32_t)(int)(player - g_entities), 1, va("a %i", weapon));
    }

    return 1;
}

/* VERIFIED_DECOMPILER(0x538e7, 638e7_FUN_000638e7.c, VERIFY-P1-ITEMS2-2026-06-17): DATAFLOW_VERIFIED - weapon grant/drop helpers, ammo/clip transfer, server messages, script notify arguments, pool handling, and respawn return checked against current decompiler output. */
int Pickup_Weapon(gentity_t *itemEnt, gentity_t *player, int *event,
                  int traceMode)
{
    int weapon = ((const gitem_t *)itemEnt->itemInfo)->weapon;
    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    int reserveCount = game_compat_pickup_weapon_reserve_count(itemEnt, weapon);
    int clipCount = game_compat_pickup_weapon_clip_count(itemEnt, weapon, &reserveCount);
    int alreadyOwned = Com_BitCheck(player->client->ps.weaponBits, weapon);
    gentity_t *dropped = 0;

    if (alreadyOwned == 0) {
        if (game_compat_pickup_weapon_grant_new_weapon(itemEnt, player, weapon, weaponInfo,
                                       traceMode, &dropped) == 0) {
            return 0;
        }
    }

    if (alreadyOwned == 0) {
        if (clipCount >= 0) {
            int clipIndex = BG_ClipForWeapon(weapon);
            int clipSize = BG_GetAmmoClipSize(clipIndex);

            if (clipSize < clipCount) {
                reserveCount += clipCount - clipSize;
                clipCount = clipSize;
            }

            player->client->ps.clips[clipIndex] = clipCount;
        }

        Add_Ammo(player, weapon, reserveCount, clipCount == -1);
    } else {
        int added;

        *event = PICKUP_EVENT_EXISTING_WEAPON_AMMO;
        reserveCount += clipCount;
        added = Add_Ammo(player, weapon, reserveCount, 0);

        if (added != 0) {
            if (BG_WeaponIsClipOnly(weapon) == 0) {
                trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0,
                                       va("f \"GAME_PICKUP_AMMO\x14%s\"",
                                          weaponInfo->displayName));
            } else {
                trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0,
                                       va("f \"GAME_PICKUP_CLIPONLY_AMMO\x14%s\"",
                                          weaponInfo->displayName));
            }
        }

        if (added != reserveCount) {
            itemEnt->itemCount -= added;
            if (itemEnt->itemCount < 1) {
                itemEnt->droppedClipCount += itemEnt->itemCount;
                itemEnt->itemCount = -1;
                if (itemEnt->droppedClipCount < 1) {
                    itemEnt->droppedClipCount = -1;
                }
            }

            if ((itemEnt->itemCount > 0 ||
                 itemEnt->droppedClipCount > 0) &&
                g_weaponAmmoPools.integer != 0) {
                return 0;
            }
        }
    }

    if (dropped == 0) {
        Scr_AddUndefined();
    } else {
        Scr_AddEntity(dropped);
    }
    Scr_AddEntity(player);
    Scr_Notify(itemEnt, scr_const_trigger, 2);

    if ((itemEnt->spawnflags &
         ITEM_SPAWNFLAG_WEAPON_RESPAWN) == 0) {
        return -1;
    }

    return g_weaponRespawn.integer;
}

/* VERIFIED_DECOMPILER(0x5644a, 6644a_G_BounceItem.c, VERIFY-P1-ITEMS2-2026-06-17): DATAFLOW_VERIFIED - bounce time, trajectory delta reflection, startsolid retrace, settle-vs-bounce branch, ground entity, angles, and link side effect checked against current decompiler output. */
void G_BounceItem(gentity_t *ent, trace_t *trace)
{
    /* Windows rounds elapsed milliseconds to float before multiplying and
     * consumes the low dword of a qword conversion. Linux retains the integer
     * load through multiplication and truncates directly to a dword. */
    WIDE_FLOAT elapsed = level.time - level.previousTime;
    elapsed = coduo_round_f32(elapsed);
    int bounceTime = coduo_fp_to_i32_extended(elapsed * trace->fraction) + level.previousTime;
    vec3_t velocity;
    volatile float dot;
    float *delta = ent->s.pos.trDelta;

    BG_EvaluateTrajectoryDelta(&ent->s.pos, bounceTime, velocity);

    /* Retain each dot-product term until the float result: Windows sums X/Z/Y,
     * Linux X/Y/Z. Windows also rounds the reflection scale to float; Linux
     * keeps that multiplication live through the reflected component. Each
     * component rounds before its separate bounce-factor multiplication. */
    const int second = 2, last = 1;
    dot = ((WIDE_FLOAT)velocity[0] * trace->normal[0] + (WIDE_FLOAT)velocity[second] * trace->normal[second])
          + (WIDE_FLOAT)velocity[last] * trace->normal[last];
    /* The -2 scale is exact within float range for the already-float dot.
     * Preserve its float store as well, including overflow, before reflection. */
    WIDE_FLOAT reflectionScale = coduo_round_f32((WIDE_FLOAT)dot * ITEM_BOUNCE_REFLECTION_SCALE);
    delta[0] = (WIDE_FLOAT)reflectionScale * trace->normal[0] + velocity[0];
    delta[1] = (WIDE_FLOAT)reflectionScale * trace->normal[1] + velocity[1];
    delta[2] = (WIDE_FLOAT)reflectionScale * trace->normal[2] + velocity[2];
    delta[0] *= ent->bounceFactor;
    delta[1] *= ent->bounceFactor;
    delta[2] *= ent->bounceFactor;

    if (trace->startsolid != 0) {
        vec3_t end;

        delta[0] = 0.0f;
        delta[1] = 0.0f;
        delta[2] = 0.0f;
        end[0] = ent->currentOrigin[0];
        end[1] = ent->currentOrigin[1];
        end[2] = ent->currentOrigin[2] - ITEM_BOUNCE_RETRACE_Z;

        if ((ent->s.eFlags & EF_CAPSULE) == 0) {
            trap_Trace(trace, ent->currentOrigin,
                       ent->mins,
                       ent->maxs,
                       end, ent->s.number, ITEM_TRACE_CONTENTS);
        } else {
            trap_TraceCapsule(trace, ent->currentOrigin,
                              ent->mins,
                              ent->maxs,
                              end, ent->s.number, ITEM_TRACE_CONTENTS);
        }
    }

    /* Original i386 0x56676..0x56689 loads normal.z, loads 0.0f, then FXCH
     * makes FUCOMPP compare normal.z against zero; JA therefore requires an
     * ordered normal.z > 0.0f.  The second JA requires 40.0f > delta.z. */
    if (!(trace->normal[2] > 0.0f && delta[2] < ITEM_BOUNCE_STOP_Z)) {
        ent->currentOrigin[0] += trace->normal[0];
        ent->currentOrigin[1] += trace->normal[1];
        ent->currentOrigin[2] += trace->normal[2];
        ent->s.pos.trBase[0] =
            ent->currentOrigin[0];
        ent->s.pos.trBase[1] =
            ent->currentOrigin[1];
        ent->s.pos.trBase[2] =
            ent->currentOrigin[2];
        ent->s.pos.trTime = level.time;
    } else {
        axis_t axis;
        vec3_t angles;
        int itemIndex = ent->s.itemIndex;

        trace->endpos[2] += game_compat_item_random_settled_height();
        G_SetOrigin(ent, trace->endpos);
        ent->s.groundEntityNum = trace->entityNum;

        axis[2][0] = trace->normal[0];
        axis[2][1] = trace->normal[1];
        axis[2][2] = trace->normal[2];
        AngleVectors(ent->currentAngles, axis[0], 0, 0);
        CrossProduct(axis[2], axis[0], axis[1]);
        CrossProduct(axis[1], axis[2], axis[0]);
        AxisToAngles((const vec_t (*)[3])axis, angles);

        if (game_compat_item_def_for_index(itemIndex)->type == IT_WEAPON) {
            angles[2] += ITEM_WEAPON_ANGLE_ADJUST;
        }

        G_SetAngle(ent, angles);
        trap_LinkEntity(ent);
    }
}

/* VERIFIED_DECOMPILER(0x56876, 66876_G_RunItem.c, VERIFY-P1-ITEMS2-2026-06-17): DATAFLOW_VERIFIED - original gravity/runthink/trace/link/bounce/free state changes checked against current decompiler output. */
void G_RunItem(gentity_t *ent)
{
    if (ent->s.groundEntityNum == ENTITYNUM_NONE &&
        ent->s.pos.trType !=
            TR_GRAVITY) {
        ent->s.pos.trType =
            TR_GRAVITY;
        ent->s.pos.trTime = level.time;
    }

    if (ent->s.pos.trType ==
            TR_STATIONARY ||
        ent->s.pos.trType ==
            TR_LINKED) {
        G_RunThink(ent);
    } else {
        trace_t trace;
        vec3_t nextOrigin;
        int clipmask = ent->clipmask;

        BG_EvaluateTrajectory(&ent->s.pos, level.time, nextOrigin);
        if (clipmask == 0) {
            clipmask = ITEM_DEFAULT_CLIPMASK;
        }

        if ((ent->s.eFlags & EF_CAPSULE) == 0) {
            trap_Trace(&trace, ent->currentOrigin,
                       ent->mins,
                       ent->maxs,
                       nextOrigin,
                       ent->passEntityNum,
                       clipmask);
        } else {
            trap_TraceCapsule(&trace, ent->currentOrigin,
                              ent->mins,
                              ent->maxs,
                              nextOrigin,
                              ent->passEntityNum,
                              clipmask);
        }

        ent->currentOrigin[0] = trace.endpos[0];
        ent->currentOrigin[1] = trace.endpos[1];
        ent->currentOrigin[2] = trace.endpos[2];

        if (trace.startsolid != 0) {
            trace.fraction = 0.0f;
        }

        trap_LinkEntity(ent);
        G_RunThink(ent);

        if (ent->linked != 0 && trace.fraction != 1.0f) {
            int contents = trap_PointContents(ent->currentOrigin,
                                              PASS_ENTITY_NONE,
                                              ITEM_POINT_CONTENTS_MASK);

            if (contents == 0) {
                G_BounceItem(ent, &trace);
            } else {
                G_FreeEntity(ent);
            }
        }
    }
}

/* VERIFIED_DECOMPILER(0x56146, 66146_G_SpawnItem.c, VERIFY-P1-ITEMS2-2026-06-17): DATAFLOW_VERIFIED - spawn vars, registration, model/DObj setup, bounds, flags, callbacks, immediate/delayed link paths, and debug-only diagnostics checked against current decompiler output. */
void G_SpawnItem(gentity_t *ent, gitem_t *item)
{
    int itemIndex = game_compat_item_index_from_def(item);
    itemType_t itemType = game_compat_item_def_for_index(itemIndex)->type;
    const char *noiseAlias;

    G_SpawnFloat("random", "0", &ent->itemRandom);
    G_SpawnFloat("wait", "0", &ent->itemWait);
    RegisterItem(itemIndex, 0);

    ent->itemInfo = item;
    G_SetModel(ent, game_compat_item_def_for_index(itemIndex)->worldModel);

    if (G_SpawnString("noise", 0, &noiseAlias) != 0) {
        ent->itemSoundAlias =
            G_SoundAliasIndex(noiseAlias);
    }

    ent->bounceFactor = 0;
    game_compat_set_item_bounds(ent, itemType);

    ent->svFlags |= SVF_CAPSULE;
    ent->s.eFlags |= EF_CAPSULE;
    ent->scriptContents = ITEM_SPAWN_PACKED_STATE;
    ent->touch = Touch_Item_Auto;
    ent->s.eType = ET_ITEM;
    ent->s.itemIndex = itemIndex;
    G_DObjUpdate(ent);

    ent->s.clientNum = ENTITYNUM_WORLD;
    ent->use = Use_Item;
    ent->flags |= FL_SUPPORTS_LINKTO;

    if (level.spawningMapEntities == 0) {
        if ((ent->spawnflags &
             ITEM_SPAWNFLAG_NO_DROP_TO_FLOOR) == 0) {
            ent->s.groundEntityNum = ENTITYNUM_NONE;
            if (itemType == IT_WEAPON) {
                ent->currentAngles[2] += ITEM_WEAPON_ANGLE_ADJUST;
            }
        }

        G_SetAngle(ent, ent->currentAngles);
        G_SetOrigin(ent, ent->currentOrigin);
        trap_LinkEntity(ent);
    } else {
        G_SetAngle(ent, ent->currentAngles);
        ent->nextthink = level.time + ITEM_DELAYED_SPAWN_MS;
        ent->think = FinishSpawningItem;
    }
}

/* VERIFIED_DECOMPILER(0x53772, 63772_FUN_00063772.c, VERIFY-P1-ITEMS2-2026-06-17): DATAFLOW_VERIFIED - default count, Add_Ammo call, clip-only message branch, remaining pool update, and respawn return checked against current decompiler output. */
int Pickup_Ammo(gentity_t *itemEnt, gentity_t *player)
{
    const gitem_t *itemInfo = itemEnt->itemInfo;
    int weapon = itemInfo->weapon;
    int count = itemEnt->itemCount;
    int added;

    if (count == 0) {
        count = itemInfo->quantity;
    }

    added = Add_Ammo(player, weapon, count, 0);
    if (added == 0) {
        return 0;
    }

    if (BG_WeaponIsClipOnly(weapon) != 0) {
        trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0,
                               va("f \"GAME_PICKUP_CLIPONLY_AMMO\x14%s\"",
                                  ((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->displayName));
    } else {
        trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0,
                               va("f \"GAME_PICKUP_AMMO\x14%s\"",
                                  ((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->displayName));
    }

    Scr_AddEntity(player);
    Scr_Notify(itemEnt, scr_const_trigger, 1);
    return PICKUP_AMMO_RESPAWN_TIME;
}

/* VERIFIED_DECOMPILER(0x54361, 64361_Pickup_Health.c, VERIFY-WAVE3-ITEM-TOUCH-VISIBILITY-2026-06-17): DATAFLOW_VERIFIED - amount selection, overheal limit, percent rounding repair, health mirrors, pickup message, script notify, and no-respawn return checked against current decompiler output. */
int Pickup_Health(gentity_t *itemEnt, gentity_t *player)
{
    const gitem_t *itemInfo = itemEnt->itemInfo;
    int itemQuantity = itemInfo->quantity;
    int maxHealth = player->client->ps.stats[STAT_MAX_HEALTH];
    int healthLimit = maxHealth;
    int amount;
    int oldHealth;

    if (itemQuantity == PICKUP_HEALTH_SMALL ||
        itemQuantity == PICKUP_HEALTH_LARGE) {
        healthLimit = maxHealth * 2;
    }

    amount = itemEnt->itemCount;
    if (amount == 0) {
        amount = itemQuantity;
    }

    oldHealth = player->health;
    /* Retain the live product until the platform's integer conversion: Windows
     * consumes the low dword of a qword result; Linux converts to a dword. */
    player->health += coduo_fp_to_i32_extended((WIDE_FLOAT)amount * (WIDE_FLOAT)maxHealth * 0.01f);

    if (healthLimit < player->health) {
        player->health = healthLimit;
    } else {
        /* Multiply by 100 in integer arithmetic before the live division;
         * preserve the same Windows conversion as the health gain. */
        int currentPercent = coduo_fp_to_i32_extended((WIDE_FLOAT)(player->health * 100) / (WIDE_FLOAT)maxHealth);
        int oldPercent = coduo_fp_to_i32_extended((WIDE_FLOAT)(oldHealth * 100) / (WIDE_FLOAT)maxHealth);

        if (currentPercent < 1) {
            currentPercent = 1;
        } else if (currentPercent > 100) {
            currentPercent = 100;
        }

        if (oldPercent < 1) {
            oldPercent = 1;
        }

        oldPercent += amount;
        if (oldPercent > 100) {
            oldPercent = 100;
        }

        if (currentPercent != oldPercent) {
            player->health = (oldPercent * maxHealth) / 100;
        }
    }

    player->client->ps.stats[STAT_HEALTH] = player->health;
    trap_SendServerCommand((uint32_t)(int)(player - g_entities), 0,
                           va("f \"GAME_PICKUP_HEALTH\x15%i\"", amount));
    Scr_AddEntity(player);
    Scr_Notify(itemEnt, scr_const_trigger, 1);
    return PICKUP_HEALTH_NO_RESPAWN;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Touch_Item generated body. */
static void game_compat_touch_item_send_cant_grab_weapon_message(gentity_t *itemEnt,
                                               gentity_t *player)
{
    int weapon = ((const gitem_t *)itemEnt->itemInfo)->weapon;

    if (Com_BitCheck(player->client->ps.weaponBits, weapon) == 0) {
        switch (((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->slot) {
        case WEAPSLOT_PRIMARY_FIRST:
        case WEAPSLOT_PRIMARY_SECOND:
            trap_SendServerCommand(
                (uint32_t)(int)(player - g_entities), 0,
                va("f \"GAME_CANT_GET_PRIMARY_WEAP_MESSAGE\""));
            break;
        case WEAPSLOT_PISTOL:
            trap_SendServerCommand(
                (uint32_t)(int)(player - g_entities), 0,
                va("f \"GAME_CANT_GET_PISTOL_WEAP_MESSAGE\""));
            break;
        case WEAPSLOT_GRENADE:
            trap_SendServerCommand(
                (uint32_t)(int)(player - g_entities), 0,
                va("f \"GAME_CANT_GET_GRENADE_WEAP_MESSAGE\""));
            break;
        case WEAPSLOT_SMOKE_GRENADE:
            trap_SendServerCommand(
                (uint32_t)(int)(player - g_entities), 0,
                va("f \"GAME_CANT_GET_SMOKER_WEAP_MESSAGE\""));
            break;
        default:
            break;
        }
    } else {
        trap_SendServerCommand(
            (uint32_t)(int)(player - g_entities), 0,
            va("f \"GAME_PICKUP_CANTCARRYMOREAMMO\x14%s\"",
               ((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->displayName));
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Touch_Item generated body. */
static void game_compat_touch_item_log_pickup(gentity_t *itemEnt, gentity_t *player)
{
    const gitem_t *itemInfo = itemEnt->itemInfo;
    char cleanName[CLIENT_CLEAN_NAME_LEN];
    int guid;

    Q_strncpyz(cleanName,
               player->client->clientState.name,
               sizeof(cleanName));
    Q_CleanStr(cleanName);

    guid = trap_GetGuid(player->s.number);
    if (itemInfo->type == IT_WEAPON) {
        int weapon = itemInfo->weapon;

        G_LogPrintf("Weapon;%d;%d;%s;%s\n", guid, player->s.number,
                    cleanName,
                    ((const weaponInfo_t *)BG_GetInfoForWeapon(weapon))->pickupName);
    } else {
        G_LogPrintf("Item;%d;%d;%s;%s\n", guid, player->s.number,
                    cleanName, itemInfo->classname);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Touch_Item generated body. */
static int game_compat_touch_item_dispatch_pickup(gentity_t *itemEnt, gentity_t *player,
                                   int *event, int traceMode)
{
    switch (((const gitem_t *)itemEnt->itemInfo)->type) {
    case IT_WEAPON:
        return Pickup_Weapon(itemEnt, player, event, traceMode);
    case IT_AMMO:
        return Pickup_Ammo(itemEnt, player);
    case IT_HEALTH:
        return Pickup_Health(itemEnt, player);
    default:
        return 0;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from Touch_Item generated body. */
static void game_compat_touch_item_schedule_respawn(gentity_t *itemEnt, int respawnSeconds)
{
    float wait = itemEnt->itemWait;
    float random = itemEnt->itemRandom;

    if (wait == PICKUP_HEALTH_NO_RESPAWN) {
        itemEnt->flags |= FL_NOCLIENT;
        itemEnt->s.eFlags |= EF_NODRAW;
        itemEnt->scriptContents = 0;
        itemEnt->unlinkOnTimeout = 1;
        return;
    }

    if (wait != 0.0f) {
        /* Windows consumes a signed-qword conversion's low dword; Linux
         * truncates directly to a signed dword. Preserve that distinction
         * for both the wait value and the live random product below. */
        respawnSeconds = coduo_fp_to_i32_extended((WIDE_FLOAT)wait);
    }

    if (random != 0.0f) {
        respawnSeconds += coduo_fp_to_i32_extended(
            (WIDE_FLOAT)coduo_server_rand_signed_unit() * (WIDE_FLOAT)random);
        if (respawnSeconds < 1) {
            respawnSeconds = 1;
        }
    }

    if ((itemEnt->flags & DROPPED_ITEM_FLAGS) != 0) {
        itemEnt->skipTypeDispatch = 1;
    }

    itemEnt->svFlags |= ITEM_SVF_RESPAWNING;
    itemEnt->flags |= FL_NOCLIENT;
    itemEnt->scriptContents = 0;

    if (respawnSeconds < 1) {
        itemEnt->nextthink = 0;
        itemEnt->think = 0;
    } else {
        itemEnt->nextthink = level.time + respawnSeconds * 1000;
        itemEnt->think = RespawnItem;
    }

    if ((itemEnt->flags & DROPPED_ITEM_FLAGS) != 0) {
        itemEnt->think = G_FreeEntity;
        itemEnt->nextthink =
            level.time + DROPPED_ITEM_PICKUP_FREE_DELAY_MS;
    }

    trap_LinkEntity(itemEnt);
}

/* VERIFIED_DECOMPILER(0x5473b, 6473b_Touch_Item.c, VERIFY-WAVE3-ITEM-TOUCH-VISIBILITY-2026-06-17): DATAFLOW_VERIFIED - auto-touch gate, manual denial messages, clientState.name logging, pickup dispatch, event selection, visibility/no-respawn stores, dropped-item free path, and link behavior checked against current decompiler output. */
void Touch_Item(gentity_t *itemEnt, gentity_t *player, int traceMode)
{
    int event = EV_ITEM_PICKUP;
    int respawnSeconds;

    if (*game_compat_item_auto_touch_flag(itemEnt) == 0) {
        return;
    }

    *game_compat_item_auto_touch_flag(itemEnt) = 0;

    if (player->client == 0 || player->health <= 0) {
        return;
    }

    if (BG_CanItemBeGrabbed(&itemEnt->s, &player->client->ps,
                            traceMode) == 0 &&
        traceMode != ITEM_TOUCH_AUTOGRAB) {
        if (traceMode == ITEM_TOUCH_MANUAL &&
            itemEnt->s.clientNum != player->s.number &&
            ((const gitem_t *)itemEnt->itemInfo)->type == IT_WEAPON) {
            game_compat_touch_item_send_cant_grab_weapon_message(itemEnt, player);
        }
        return;
    }

    game_compat_touch_item_log_pickup(itemEnt, player);

    respawnSeconds = game_compat_touch_item_dispatch_pickup(itemEnt, player, &event, traceMode);
    if (respawnSeconds == 0) {
        return;
    }

    if (itemEnt->itemSoundAlias != 0) {
        event = EV_ITEM_PICKUP_QUIET;
        G_PlaySoundAlias(player,
                         itemEnt->itemSoundAlias);
    }

    if (player->client->predictItems == 0) {
        G_AddEvent(player, event,
                   itemEnt->s.itemIndex);
    } else {
        G_AddPredictableEvent(player, event,
                              itemEnt->s.itemIndex);
    }

    game_compat_touch_item_schedule_respawn(itemEnt, respawnSeconds);
}

/* VERIFIED_DECOMPILER(0x30610, 40610_FUN_00040610.c, VERIFY-PACKET-WEAPON-INFO-SETUP-2026-06-17): DATAFLOW_VERIFIED - empty-string global alias, low-hunk aligned allocation, strlen+1 size, strcpy, and destination pointer store checked against current decompiler output. */
void BG_CopyWeaponString(const char **dest, const char *src)
{
    char *copy;
    size_t length;

    if (*src == '\0') {
        *dest = bg_weaponEmptyString;
        return;
    }

    length = strlen(src);
    copy = (char *)trap_Hunk_AllocLowAlignInternal(length + 1, 1);
    strcpy(copy, src);
    *dest = copy;
}

/* VERIFIED_DECOMPILER(0x30676, 40676_FUN_00040676.c, VERIFY-PACKET-WEAPON-INFO-SETUP-2026-06-17): DATAFLOW_VERIFIED - parser callback thunk delegates arguments unchanged to BG_CopyWeaponString and returns void as checked against current decompiler output. */
void BG_CopyWeaponStringForParser(char *dest, const char *src)
{
    BG_CopyWeaponString((const char **)(void *)dest, src);
}

/* VERIFIED_DECOMPILER(0x30690, 40690_FUN_00040690.c, VERIFY-PACKET-WEAPON-INFO-SETUP-2026-06-17): DATAFLOW_VERIFIED - low-hunk weaponInfo allocation size, bg_weaponInfos slot store, weaponIndex store, pickupName default, parse-field stride/type check, string-offset defaults, and return pointer checked against current decompiler output. */
weaponInfo_t *BG_AllocWeaponInfoDefaults(int weapon,
                                         const parseField_t *fields,
                                         int fieldCount)
{
    weaponInfo_t *weaponInfo =
        (weaponInfo_t *)trap_Hunk_AllocLowInternal((int)sizeof(*weaponInfo));

    bg_weaponInfos[weapon] = weaponInfo;
    weaponInfo->weaponIndex = weapon;
    BG_CopyWeaponString(&weaponInfo->pickupName, "");

    for (int fieldIndex = 0; fieldIndex < fieldCount; fieldIndex++) {
        if (fields[fieldIndex].type == PARSE_FIELD_STRING_ALLOC) {
            BG_CopyWeaponString((const char **)(void *)
                                    &((uint8_t *)weaponInfo)
                                        [fields[fieldIndex].offset],
                                "");
        }
    }

    return weaponInfo;
}

/* VERIFIED_DECOMPILER(0x3073b, 4073b_FUN_0004073b.c, VERIFY-PACKET-WEAPON-INFO-SETUP-2026-06-17): DATAFLOW_VERIFIED - empty-string hunk setup, default weapon-info allocation, slot-0 pickupName "none", bg_numWeapons increment/decrement behavior, path construction, debug print gate, file open/read/close order, WEAPONFILE magic validation, 8192-byte payload guard, Info_Validate, pickupName copy, ParseConfigStringToStruct arguments/callback order, and failed-parse pointer clear checked against current decompiler output. */
void BG_LoadWeaponFiles(const char **weaponFiles, int weaponFileCount)
{
    const size_t magicLength = strlen(WEAPON_FILE_MAGIC);
    int handle;
    int fileLength;
    char info[WEAPON_FILE_BUFFER_SIZE];
    char path[MAX_QPATH];

    bg_weaponEmptyString = (const char *)trap_Hunk_AllocLowAlignInternal(1, 1);
    ((char *)bg_weaponEmptyString)[0] = '\0';

    BG_AllocWeaponInfoDefaults(0, bg_weaponFieldDefs,
                               BG_WEAPON_FIELD_COUNT);
    BG_CopyWeaponString(&bg_weaponInfos[0]->pickupName, WEAPON_AMMO_NONE_NAME);

    bg_numWeapons = 0;

    for (int weaponFile = 0; weaponFile < weaponFileCount; weaponFile++) {
        weaponInfo_t *weaponInfo;

        bg_numWeapons++;
        weaponInfo = BG_AllocWeaponInfoDefaults(bg_numWeapons,
                                                bg_weaponFieldDefs,
                                                BG_WEAPON_FIELD_COUNT);

        const size_t folderLength = strlen(bg_szWeaponsFolder);
        const size_t fileNameLength = strlen(weaponFiles[weaponFile]);
        /* NOT_FROM_ORIGINAL_SOURCE: require the complete mounted-file path and
         * NUL to fit; never substitute a truncated weapon definition name. */
        if (folderLength >= sizeof(path) ||
            fileNameLength > sizeof(path) - folderLength - 2) {
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER "Weapon file path is too long");
        }
        Com_sprintf(path, sizeof(path), "%s/%s", bg_szWeaponsFolder,
                    weaponFiles[weaponFile]);

        if (bg_debugWeaponMessages.integer != 0 &&
            bg_debugWeaponMessages.integer != 2) {
            Com_DPrintf("Parsing weapon file \"%s\"...\n", path);
        }

        fileLength = trap_FS_FOpenFile(path, &handle, FS_READ);
        if (fileLength < 1) {
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER "Could not load weapon file '%s'",
                      path);
        }

        /* NOT_FROM_ORIGINAL_SOURCE: require the complete file magic before any
         * read or remaining-payload subtraction. */
        if (fileLength < (int)magicLength) {
            trap_FS_FCloseFile(handle);
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER
                      "\"%s\" is too short to be a weapon file",
                      path);
        }
        trap_FS_Read(info, (int)magicLength, handle);
        info[magicLength] = '\0';
        if (strncmp(info, WEAPON_FILE_MAGIC, magicLength) != 0) {
            trap_FS_FCloseFile(handle);
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER
                      "\"%s\" does not appear to be a weapon file",
                      path);
        }

        if (fileLength - (int)magicLength >= WEAPON_FILE_BUFFER_SIZE) {
            trap_FS_FCloseFile(handle);
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER
                      "\"%s\" Is too long of a weapon file to parse",
                      path);
        }

        memset(info, 0, sizeof(info));
        trap_FS_Read(info, fileLength - (int)magicLength, handle);
        info[fileLength - (int)magicLength] = '\0';
        trap_FS_FCloseFile(handle);

        if (Info_Validate(info) == 0) {
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER "\"%s\" is not a valid weapon file",
                      path);
        }

        BG_CopyWeaponString(&weaponInfo->pickupName, weaponFiles[weaponFile]);

        if (ParseConfigStringToStruct(weaponInfo, bg_weaponFieldDefs,
                                      BG_WEAPON_FIELD_COUNT, info,
                                      WEAPON_FIELD_CUSTOM_TYPE_LIMIT,
                                      BG_ParseWeaponInfoSpecificFieldType,
                                      BG_CopyWeaponStringForParser) == 0) {
            bg_weaponInfos[bg_numWeapons] = NULL;
            bg_numWeapons--;
        }
    }
}

/* VERIFIED_DECOMPILER(0x315c3, 415c3_FUN_000415c3.c, VERIFY-PACKET-WEAPON-INFO-SETUP-2026-06-17): DATAFLOW_VERIFIED - 1..bg_numWeapons loop, nonempty hintString gate, G_GetHintStringIndex arguments, failure branch, and max hint string constant checked against current decompiler output. */
void BG_SetupWeaponHintStrings(void)
{
    for (int weapon = 1; weapon <= bg_numWeapons; weapon++) {
        weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];

        if (weaponInfo->hintString[0] != '\0' &&
            G_GetHintStringIndex(&weaponInfo->hintStringIndex,
                                 weaponInfo->hintString) == 0) {
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER
                      "Too many different hintstring values on weapons. "
                      "Max allowed is %i different strings",
                      HINTSTRING_WEAPON_MAX);
        }
    }
}

/* VERIFIED_DECOMPILER(0x31659, 41659_compare_weaponfile_names.c, VERIFY-PACKET-WEAPON-INFO-SETUP-2026-06-17): DATAFLOW_VERIFIED - qsort comparator loads both const char * entries, calls Q_stricmp(left, right), and returns the preserved EAX result; disassembly confirms return behavior where the decompiler inferred void. */
int compare_weaponfile_names(const void *a, const void *b)
{
    const char *const *left = (const char *const *)a;
    const char *const *right = (const char *const *)b;

    return Q_stricmp(*left, *right);
}

/* VERIFIED_DECOMPILER(0x31693, 41693_BG_SetupWeaponInfo.c, VERIFY-PACKET-WEAPON-INFO-SETUP-2026-06-17): DATAFLOW_VERIFIED - debug banners, weapon-info memory acquisition, ammo/clip table initialization, weapon-file list loading/cap checks, filename pointer collection/sort/configstring setup, load-vs-already-loaded paths, setup call order, and final banner checked against current decompiler output; the item definitions retain module lifetime across setup calls. */
void BG_SetupWeaponInfo(void)
{
    char weaponFileConfigstring[WEAPON_FILE_MAX_STRING_CHARS];
    char weaponFileList[WEAPON_FILE_LIST_BUFFER_SIZE];
    const char *weaponFiles[MAX_WEAPON_FILES];
    int alreadyLoaded;

    Com_DPrintf("----------------------\n");
    Com_DPrintf("Game: BG_SetupWeaponInfo\n");

    bg_weaponInfos =
        trap_GetWeaponInfoMemory(
            MAX_WEAPONS * (int)sizeof(*bg_weaponInfos),
            &alreadyLoaded);
    if (bg_weaponInfos == NULL) {
        Com_Error(ERR_DROP,
                  COM_ERROR_MARKER "Could not allocate WeaponInfo array\n");
    }

    memset(bg_ammoTypeNames, 0, sizeof(bg_ammoTypeNames));
    bg_ammoTypeMax[0] = 0;
    /* VERIFIED_ORIGINAL(0x31693 BG_SetupWeaponInfo): the i386 module seeds
     * ammo/clip type slot 0 with the rodata string "none"; the empty string is
     * only the weapon-file list extension and becomes slot 1 when parsed. */
    bg_ammoTypeNames[0] = WEAPON_AMMO_NONE_NAME;
    bg_numAmmoTypes = 1;

    memset(bg_ammoClipNames, 0, sizeof(bg_ammoClipNames));
    bg_ammoClipSizes[0] = 0;
    bg_ammoClipNames[0] = WEAPON_AMMO_NONE_NAME;
    bg_numAmmoClips = 1;

    memset(weaponFiles, 0, sizeof(weaponFiles));

    if (alreadyLoaded == 0) {
        int weaponFileCount =
            trap_FS_GetFileList(bg_szWeaponsFolder,
                                WEAPON_FILE_LIST_EXTENSION,
                                weaponFileList, sizeof(weaponFileList));
        char *weaponFileName = weaponFileList;

        if (weaponFileCount < 1) {
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER "No weapon files found in %s.\n",
                      bg_szWeaponsFolder);
        }
        if (weaponFileCount > MAX_WEAPON_FILES) {
            Com_Error(ERR_DROP,
                      COM_ERROR_MARKER
                      "Max number of weapons allowed is %i, found %i.\n",
                      MAX_WEAPON_FILES, weaponFileCount);
        }

        for (int weaponFile = 0;
             weaponFile < weaponFileCount && weaponFile < MAX_WEAPON_FILES;
             weaponFile++) {
            size_t weaponFileNameLength = strlen(weaponFileName);

            if (bg_debugWeaponMessages.integer != 0 &&
                bg_debugWeaponMessages.integer != 2) {
                Com_DPrintf("Getting weapon file \"%s/%s\" for parsing\n",
                            bg_szWeaponsFolder, weaponFileName);
            }

            weaponFiles[weaponFile] = weaponFileName;
            weaponFileName = &weaponFileName[weaponFileNameLength + 1];
        }

        coduo_qsort(weaponFiles, (size_t)weaponFileCount,
                    sizeof(weaponFiles[0]), compare_weaponfile_names);

        weaponFileConfigstring[0] = '\0';
        for (int weaponFile = 0; weaponFile < weaponFileCount; weaponFile++) {
            if (weaponFile > 0) {
                strcat(weaponFileConfigstring, " ");
            }
            strcat(weaponFileConfigstring, weaponFiles[weaponFile]);
        }

        trap_SetConfigstring(WEAPON_CONFIGSTRING_FILES,
                             weaponFileConfigstring);
        BG_LoadWeaponFiles(weaponFiles, weaponFileCount);
    } else {
        bg_numWeapons = 0;
        for (int weapon = 1;
             weapon < MAX_WEAPONS && bg_weaponInfos[weapon] != NULL;
             weapon++) {
            bg_numWeapons++;
        }
    }

    BG_SetupWeaponADSRates();
    BG_SetupAmmoIndexes();
    BG_SetupSharedAmmoIndexes();
    BG_SetupClipIndexes();
    BG_FillInWeaponItems();
    BG_SetupAltWeaponIndexes();
    BG_SetupWeaponHintStrings();

    Com_DPrintf("----------------------\n");
}

/* VERIFIED_DECOMPILER(0x300e3, 400e3_FUN_000400e3.c, VERIFY-PACKET-WEAPON-GETTERS-2026-06-17): DATAFLOW_VERIFIED - float absolute value promoted to long double return checked against current decompiler output. */
long double BG_FloatAbs(float value)
{
    return (long double)fabsf(value);
}

/* VERIFIED_DECOMPILER(0x33c28, 43c28_BG_WeaponAmmo.c, VERIFY-ITEMS-WEAPON-PM-2026-06-17): DATAFLOW_VERIFIED; ammo/clip index calls and client ammo-plus-clip field loads checked against current decompiler output. */
int BG_WeaponAmmo(gclient_t *client, int weapon)
{
    int ammoIndex = BG_AmmoForWeapon(weapon);
    int clipIndex = BG_ClipForWeapon(weapon);

    return client->ps.ammo[ammoIndex] + client->ps.clips[clipIndex];
}

/* No weapon state or animation has been printed when the module is loaded. */
#define PM_DEBUG_STATE_UNSET (-1)
int32_t PMDebugLastWeaponState = PM_DEBUG_STATE_UNSET;
uint32_t PMDebugLastWeaponAnim = UINT32_MAX;
const char *PMDebugPrefix = "G";
