/*
 * Source reconstruction for player script methods.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <math.h>
#include <string.h>

#include "recovered_game.h"
#include "game_globals.h"
#include "scr_vm.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#define RELIABLE_SERVER_COMMAND 1
#define PLAYER_SAY_TEXT_SIZE 1023
#define PLAYER_SAY_PREFIX 20
#define PLAYER_SAY_ALL 0
#define PLAYER_SAY_TEAM 1
#define PLAYER_SAY_SQUAD 3
#define PLAYER_SPECTATE_DENY_MASK_AXIS 0x02u
#define PLAYER_SPECTATE_DENY_MASK_ALLIES 0x04u
#define PLAYER_SPECTATE_DENY_MASK_NONE 0x01u
#define PLAYER_SPECTATE_DENY_MASK_FREELOOK 0x10u
#define PLAYER_FATIGUE_MIN 0.0f
#define PLAYER_FATIGUE_MAX 1.0f
#define AUTODEMO_START_COMMAND "C"
#define AUTODEMO_STOP_COMMAND "D"
#define AUTOSCREENSHOT_COMMAND "E"
#define PLAYER_SWITCH_WEAPON_COMMAND "a %i"
#define GIVEWEAPON_NO_SLOT_ERROR \
    "Can not give player weapon without having an empty weapon slot\n"
#define PLAYER_NO_WEAPON_NAME "none"
#define PLAYER_PS_FLAG_PING 0x00040000u
#define PLAYER_PING_DURATION_MS 4000
#define PLAYER_COMPLAINT_DISABLED_COMMAND "m -4"
#define PLAYER_COMPLAINT_PROMPT_COMMAND "m %i"
#define PLAYER_DROP_WEAPON_DEFAULT_TAG "tag_weapon_right"
#define ENTITY_FLAG_VEHICLE_OCCUPANT 0x00100000u
#define ENTITY_FLAG_NO_KNOCKBACK 0x00000008u
#define ENTITY_FLAG_GODMODE 0x00000001u
#define DAMAGE_FLAG_NO_KNOCKBACK 0x04
#define PLAYER_DAMAGE_KNOCKBACK_NORMAL 0.3f
#define PLAYER_DAMAGE_KNOCKBACK_CROUCH 0.15f
#define PLAYER_DAMAGE_KNOCKBACK_PRONE 0.02f
#define PLAYER_DAMAGE_KNOCKBACK_MAX 60
#define PLAYER_DAMAGE_KNOCKBACK_PM_TIME_MIN 50
#define PLAYER_DAMAGE_KNOCKBACK_PM_TIME_MAX 200
#define PLAYER_DAMAGE_KNOCKBACK_DIVISOR 250.0f
#define PLAYER_DAMAGE_SURFACE_TYPE_FLESH 7
#define PLAYER_DAMAGE_IMPACT_EVENT_FLAG 0x00002000u
#define PLAYER_DAMAGE_BULLET_EVENT_FLAGS 0x00000800u
#define PLAYER_DAMAGE_MIN_HEALTH -999
#define PLAYER_SUICIDE_DAMAGE 100000
#define PLAYER_OPENMENU_COMMAND "t %i"
#define PLAYER_OPENMENU_NOMOUSE_COMMAND "t %i 1"
#define PLAYER_CLOSEMENU_COMMAND "u"
#define PLAYER_IPRINTLN_COMMAND "f"
#define PLAYER_IPRINTLN_BOLD_COMMAND "g"
#define UNKNOWN_WEAPON_SLOT_ERROR \
    "Unknown weaponslot name %s. Valid weaponslots are \"primary\", \"primaryb\", \"pistol\", \"grenade\", and \"smokegrenade\""
#define PLAYER_CLONE_KEEP_S_FLAG 0x00000008u
#define PLAYER_CLONE_PS_FLAGS_MASK 0xffeffff7u
#define PLAYER_CLONE_TEMP_S_FLAG 0x00000400u
#define PLAYER_CLONE_CONTENTS 0x04000000
#define PLAYER_CLONE_CLIPMASK 0x00010001
#define PLAYER_CLONE_LIFETIME_MS 250
uint8_t G_SoundAliasIndex(const char *name);
void G_Say(gentity_t *ent, gentity_t *target, int mode,
                  const char *message);
int G_ModelIndex(const char *modelName);
gentity_t *G_TempEntity(const float *origin, int event);
gentity_t *G_SpawnPlayerClone(void);
void G_SetOrigin(gentity_t *ent, const float *origin);
void G_SetAngle(gentity_t *ent, const float *angles);
int trap_Cvar_VariableIntegerValue(const char *name);

/* NOT_FROM_ORIGINAL_SOURCE: shared open-menu command handling. */
static void game_compat_script_player_open_menu(uint32_t entityNum, const char *commandFormat)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    if (ent->client->connectedState == CON_CONNECTED) {
        int menuIndex = GScr_GetScriptMenuIndex(Scr_GetString(0));

        trap_SendServerCommand(entityNum, RELIABLE_SERVER_COMMAND,
                               va(commandFormat, menuIndex));
        Scr_AddInt(1);
    } else {
        Scr_AddInt(0);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: shared client cvar message/string argument handling. */
static const char *game_compat_script_player_get_client_cvar_value(
    char messageBuffer[MAX_STRING_CHARS])
{
    if (Scr_GetType(1) == SCRIPT_VAR_LOCALIZED_STRING) {
        Scr_ConstructMessageString(1, messageBuffer,
                                   MAX_STRING_CHARS,
                                   SCRIPT_MESSAGE_MODE_CLIENT_CVAR_VALUE);
        return messageBuffer;
    }

    return Scr_GetString(1);
}

/* NOT_FROM_ORIGINAL_SOURCE: shared client cvar sanitization buffer handling. */
static qboolean game_compat_script_player_clean_client_cvar_value(
    const char *value,
    char cleaned[MAX_STRING_CHARS])
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve every cleaned client-cvar result that
     * fits the fixed command value and report an over-capacity source. */
    memset(cleaned, 0, MAX_STRING_CHARS);

    int32_t index = 0;
    while (index < MAX_STRING_CHARS - 1 && value[index] != '\0') {
        cleaned[index] =
            Q_CleanCharacter((int)(int8_t)value[index]);
        if (cleaned[index] == '"') {
            cleaned[index] = '\'';
        }
        index++;
    }
    return value[index] == '\0' ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: shared say message construction. */
static void game_compat_script_player_say(uint32_t entityNum, int mode)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    char message[MAX_STRING_CHARS];

    Scr_ConstructMessageString(0, &message[1], PLAYER_SAY_TEXT_SIZE,
                               SCRIPT_MESSAGE_MODE_CLIENT_CHAT);
    message[0] = PLAYER_SAY_PREFIX;
    G_Say(ent, NULL, mode, message);
}

/* NOT_FROM_ORIGINAL_SOURCE: shared spectate-team string to deny-mask mapping. */
static uint32_t game_compat_script_player_spectate_team_deny_mask(uint16_t teamName)
{
    /*
     * RESOLVED(UO-GAME-UNK-0012): these script team names select deny bits in the
     * field inverted by G_ClientCanSpectateTeam.
     */
    if (teamName == scr_const_axis) {
        return PLAYER_SPECTATE_DENY_MASK_AXIS;
    }
    if (teamName == scr_const_allies) {
        return PLAYER_SPECTATE_DENY_MASK_ALLIES;
    }
    if (teamName == scr_const_none) {
        return PLAYER_SPECTATE_DENY_MASK_NONE;
    }
    if (teamName == scr_const_freelook) {
        return PLAYER_SPECTATE_DENY_MASK_FREELOOK;
    }

    Scr_ParamError(
        0,
        "team must be \"axis\", \"allies\", \"none\", or \"freelook\"");
    return 0;
}

/* VERIFIED_DECOMPILER(0x47f83, 57f83_FUN_00057f83.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; client sessionState +0x4504 zero/playing predicate checked. */
int WeaponSlotsNotValid(gentity_t *ent)
{
    return ent->client->sessionState != SESS_STATE_PLAYING;
}

/* NOT_FROM_ORIGINAL_SOURCE: shared weapon-slot parameter parser. */
static int game_compat_script_player_parse_weapon_slot(uint32_t paramIndex)
{
    uint16_t slotName = Scr_GetConstString(paramIndex);
    const char *slotString = SL_ConvertToString(slotName);
    int slot = BG_GetWeaponSlotForName(slotString);

    if (slot == 0) {
        const char *errorSlotString = SL_ConvertToString(slotName);

        Scr_ParamError(paramIndex,
                       va(UNKNOWN_WEAPON_SLOT_ERROR, errorSlotString));
    }

    return slot;
}

/* NOT_FROM_ORIGINAL_SOURCE: shared auto-demo/autoscreenshot command helper. */
static void game_compat_script_player_auto_client_command(
    uint32_t entityNum,
    const char *usage,
    const char *enableCvar,
    const char *command)
{
    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    if (g_entities[entityNum].client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    if (Scr_GetNumParam() != 0) {
        Scr_Error(usage);
    }

    if (trap_Cvar_VariableIntegerValue(enableCvar) != 0) {
        trap_SendServerCommand(entityNum, RELIABLE_SERVER_COMMAND, command);
        Scr_AddInt(1);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: shared button mask result helper. */
static void game_compat_script_player_add_button_pressed(uint32_t entityNum, uint32_t mask)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    Scr_AddInt((ent->client->currentButtons & mask) != 0);
}

/* VERIFIED_DECOMPILER(0x45ad0, 55ad0_PlayerCmd_giveWeapon.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; entity validation, slot empty/stack guard, BG_GivePlayerWeapon, start-ammo delta, and Add_Ammo ownership flag checked. */
void PlayerCmd_giveWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;
    int alreadyOwned = Com_BitCheck(ent->client->ps.weaponBits, weapon);

    if (BG_GetEmptySlotForWeapon(&ent->client->ps, weapon) == 0 &&
        BG_GetStackSlotForWeapon(&ent->client->ps, weapon, 0) == 0) {
        Scr_ParamError(0, GIVEWEAPON_NO_SLOT_ERROR);
    }

    BG_GivePlayerWeapon(&ent->client->ps, weapon);

    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    int ammoIndex = weaponInfo->ammoIndex;
    int startAmmo = weaponInfo->startAmmo;
    int ammoNeeded = startAmmo - ent->client->ps.ammo[ammoIndex];

    if (ammoNeeded > 0) {
        Add_Ammo(ent, weapon, ammoNeeded, alreadyOwned == 0);
    }
}

/* VERIFIED_DECOMPILER(0x45c6d, 55c6d_PlayerCmd_takeWeapon.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; weapon lookup, ammo/clip zero stores, and BG_TakePlayerWeapon checked. */
void PlayerCmd_takeWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;
    gclient_t *client = ent->client;
    int ammoIndex = BG_AmmoForWeapon(weapon);

    client->ps.ammo[ammoIndex] = 0;
    client = ent->client;
    int clipIndex = BG_ClipForWeapon(weapon);
    client->ps.clips[clipIndex] = 0;
    BG_TakePlayerWeapon(&ent->client->ps, weapon);
}

/* VERIFIED_DECOMPILER(0x45d6b, 55d6b_PlayerCmd_takeAllWeapons.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; currentWeapon zero, 1..BG_GetNumWeapons loop, ammo/clip clears, and BG_TakePlayerWeapon checked. */
void PlayerCmd_takeAllWeapons(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    ent->client->ps.currentWeapon = 0;

    for (int weapon = 1; weapon <= BG_GetNumWeapons(); weapon++) {
        gclient_t *client = ent->client;
        int ammoIndex = BG_AmmoForWeapon(weapon);

        client->ps.ammo[ammoIndex] = 0;
        client = ent->client;
        int clipIndex = BG_ClipForWeapon(weapon);
        client->ps.clips[clipIndex] = 0;
        BG_TakePlayerWeapon(&ent->client->ps, weapon);
    }
}

/* VERIFIED_DECOMPILER(0x45e76, 55e76_PlayerCmd_getCurrentWeapon.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; currentWeapon < 1 none case and pickupName lookup checked. */
void PlayerCmd_getCurrentWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    if (ent->client->ps.currentWeapon < 1) {
        Scr_AddString(PLAYER_NO_WEAPON_NAME);
        return;
    }

    Scr_AddString(((weaponInfo_t *)BG_GetInfoForWeapon(
                      ent->client->ps.currentWeapon))->pickupName);
}

/* VERIFIED_DECOMPILER(0x45f46, 55f46_PlayerCmd_hasWeapon.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; weapon lookup and weaponBits Com_BitCheck bool result checked. */
void PlayerCmd_hasWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;

    Scr_AddBool(Com_BitCheck(ent->client->ps.weaponBits, weapon) != 0);
}

/* VERIFIED_DECOMPILER(0x46027, 56027_PlayerCmd_switchToWeapon.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; ownership branch, reliable switch command, and bool return checked. */
void PlayerCmd_switchToWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;

    if (Com_BitCheck(ent->client->ps.weaponBits, weapon) == 0) {
        Scr_AddBool(0);
        return;
    }

    trap_SendServerCommand(entityNum, RELIABLE_SERVER_COMMAND,
                           va(PLAYER_SWITCH_WEAPON_COMMAND, weapon));
    Scr_AddBool(1);
}

/* VERIFIED_DECOMPILER(0x46145, 56145_PlayerCmd_giveClipAmmo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; owned-weapon gate, clipSize*count reserve delta, and Add_Ammo force-fill flag checked. */
void PlayerCmd_giveClipAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    const char *weaponName = Scr_GetString(0);
    int clipCount = Scr_GetInt(1);
    int weapon = BG_GetWeaponIndexForName(weaponName) & 0xff;

    if (Com_BitCheck(ent->client->ps.weaponBits, weapon) == 0) {
        return;
    }

    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    int ammoIndex = weaponInfo->ammoIndex;
    int clipSize = weaponInfo->clipSize;
    int ammoNeeded =
        clipSize * clipCount - ent->client->ps.ammo[ammoIndex];

    if (ammoNeeded > 0) {
        Add_Ammo(ent, weapon, ammoNeeded, 1);
    }
}

/* VERIFIED_DECOMPILER(0x4627d, 5627d_PlayerCmd_giveStartAmmo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; owned-weapon gate, startAmmo reserve delta, and Add_Ammo flag 0 checked. */
void PlayerCmd_giveStartAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;

    if (Com_BitCheck(ent->client->ps.weaponBits, weapon) == 0) {
        return;
    }

    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    int ammoIndex = weaponInfo->ammoIndex;
    int startAmmo = weaponInfo->startAmmo;
    int ammoNeeded = startAmmo - ent->client->ps.ammo[ammoIndex];

    if (ammoNeeded > 0) {
        Add_Ammo(ent, weapon, ammoNeeded, 0);
    }
}

/* VERIFIED_DECOMPILER(0x463a0, 563a0_PlayerCmd_giveMaxAmmo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; owned-weapon gate, BG_GetAmmoTypeMax delta, and Add_Ammo flag 0 checked. */
void PlayerCmd_giveMaxAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;

    if (Com_BitCheck(ent->client->ps.weaponBits, weapon) == 0) {
        return;
    }

    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    int ammoIndex = weaponInfo->ammoIndex;
    int maxAmmo = BG_GetAmmoTypeMax(ammoIndex);
    int ammoNeeded =
        maxAmmo - ent->client->ps.ammo[weaponInfo->ammoIndex];

    if (ammoNeeded > 0) {
        Add_Ammo(ent, weapon, ammoNeeded, 0);
    }
}

/* VERIFIED_DECOMPILER(0x464cd, 564cd_PlayerCmd_getFractionStartAmmo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; unowned/startAmmo<=0 one result, reserve zero, and reserve/start ratio checked. */
void PlayerCmd_getFractionStartAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;

    if (Com_BitCheck(ent->client->ps.weaponBits, weapon) == 0) {
        Scr_AddFloat(1.0f);
        return;
    }

    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    int startAmmo = weaponInfo->startAmmo;

    if (startAmmo < 1) {
        Scr_AddFloat(1.0f);
        return;
    }

    int reserveAmmo =
        ent->client->ps.ammo[weaponInfo->ammoIndex];

    if (reserveAmmo < 1) {
        Scr_AddFloat(0.0f);
        return;
    }

    /* 0x465f6..0x46608: fild reserve, fild start, fdivp, one float rounding
     * at the result store — neither integer is rounded to float first. */
#if EMULATE_X87
    Scr_AddFloat(x87f_store_f32(
        x87f_div(x87f_load_i32(
                     ent->client->ps.ammo[weaponInfo->ammoIndex]),
                 x87f_load_i32(weaponInfo->startAmmo))));
#else
    Scr_AddFloat((float)((long double)
                             ent->client->ps.ammo[weaponInfo->ammoIndex] /
                         (long double)weaponInfo->startAmmo));
#endif
}

/* VERIFIED_DECOMPILER(0x4662a, 5662a_PlayerCmd_getFractionMaxAmmo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; unowned/maxAmmo<=0 one result, reserve zero, and reserve/max ratio checked. */
void PlayerCmd_getFractionMaxAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;

    if (Com_BitCheck(ent->client->ps.weaponBits, weapon) == 0) {
        Scr_AddFloat(1.0f);
        return;
    }

    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    int ammoIndex = weaponInfo->ammoIndex;

    if (BG_GetAmmoTypeMax(ammoIndex) < 1) {
        Scr_AddFloat(1.0f);
        return;
    }

    int reserveAmmo =
        ent->client->ps.ammo[weaponInfo->ammoIndex];

    if (reserveAmmo < 1) {
        Scr_AddFloat(0.0f);
        return;
    }

    /* 0x4675c..0x46782: the reserve count IS rounded to a float slot (fild;
     * fstp DWORD) before the divide, while the max-ammo divisor is fild'd
     * directly with no float rounding.  So load reserve via (float) (rounds),
     * maxAmmo via fild (exact). */
    float reserveAmmoAsFloat =
        (float)ent->client->ps.ammo[weaponInfo->ammoIndex];
    int maxAmmo = BG_GetAmmoTypeMax(weaponInfo->ammoIndex);
#if EMULATE_X87
    Scr_AddFloat(x87f_store_f32(x87f_div(
        x87f_load_f32(reserveAmmoAsFloat), x87f_load_i32(maxAmmo))));
#else
    Scr_AddFloat((float)((long double)reserveAmmoAsFloat /
                         (long double)maxAmmo));
#endif
}

/* VERIFIED_DECOMPILER(0x467a4, 567a4_script_method_player_setorigin.c, VERIFY-NEXT-003-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue, Scr_GetVector(0), unlink, SetClientOrigin helper side effects, and relink checked against current decompiler output. */
void PlayerCmd_setOrigin(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    vec3_t origin;

    Scr_GetVector(0, origin);
    trap_UnlinkEntity(ent);
    SetClientOrigin(ent, origin);
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x46920, 56920_script_method_player_setplayerangles.c, VERIFY-NEXT-003-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue, Scr_GetVector(0), and SetClientViewAngle argument order checked against current decompiler output. */
void PlayerCmd_setAngles(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    vec3_t angles;

    Scr_GetVector(0, angles);
    SetClientViewAngle(ent, angles);
}

/* VERIFIED_DECOMPILER(0x469cb, 569cb_PlayerCmd_useButtonPressed.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; currentButtons use mask 0x40 result checked. */
void PlayerCmd_useButtonPressed(uint32_t entityNum)
{
    game_compat_script_player_add_button_pressed(entityNum, PM_BUTTON_ACTIVATE);
}

/* VERIFIED_DECOMPILER(0x46a81, 56a81_PlayerCmd_attackButtonPressed.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; currentButtons attack mask 0x1 result checked. */
void PlayerCmd_attackButtonPressed(uint32_t entityNum)
{
    game_compat_script_player_add_button_pressed(entityNum, PM_BUTTON_FIRE);
}

/* VERIFIED_DECOMPILER(0x46b37, 56b37_PlayerCmd_meleeButtonPressed.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; currentButtons melee mask 0x20 result checked. */
void PlayerCmd_meleeButtonPressed(uint32_t entityNum)
{
    game_compat_script_player_add_button_pressed(entityNum, PM_BUTTON_MELEE);
}

/* VERIFIED_DECOMPILER(0x46bed, 56bed_PlayerCmd_isAds.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; playerStateFlags ADS mask 0x20 result checked. */
void PlayerCmd_isAds(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    Scr_AddInt((ent->client->ps.playerStateFlags & PMF_ADS) != 0);
}

/* VERIFIED_DECOMPILER(0x46ca0, 56ca0_PlayerCmd_isInVehicle.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; entity s_flags vehicle occupant mask 0x100000 result checked. */
void PlayerCmd_isInVehicle(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    Scr_AddInt((ent->s.eFlags & ENTITY_FLAG_VEHICLE_OCCUPANT) != 0);
}

/* VERIFIED_DECOMPILER(0x46d4f, 56d4f_PlayerCmd_isOnGround.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; groundEntityNum != ENTITYNUM_NONE result checked. */
void PlayerCmd_isOnGround(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    Scr_AddInt(ent->client->ps.groundEntityNum != ENTITYNUM_NONE);
}

/* VERIFIED_DECOMPILER(0x46e01, 56e01_PlayerCmd_pingPlayer.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; entityStateFlags ping OR and pingEndTime level.time+4000 checked. */
void PlayerCmd_pingPlayer(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    ent->client->ps.entityStateFlags |= PLAYER_PS_FLAG_PING;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ent->client->pingEndTime = level.time + PLAYER_PING_DURATION_MS;
}

/* VERIFIED_DECOMPILER(0x46eca, 56eca_script_method_player_setviewmodel.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; Scr_GetString, G_ModelIndex, and viewModelIndex store checked. */
void PlayerCmd_SetViewmodel(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    int modelIndex = G_ModelIndex(Scr_GetString(0));

    ent->client->viewModelIndex = modelIndex;
}

/* VERIFIED_DECOMPILER(0x46f7f, 56f7f_script_method_player_getviewmodel.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; viewModelIndex load, G_ModelName, and Scr_AddString checked. */
void PlayerCmd_GetViewmodel(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    Scr_AddString(G_ModelName(ent->client->viewModelIndex));
}

/* VERIFIED_DECOMPILER(0x4702a, 5702a_script_method_player_allowcomplaint.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; self no-op, disabled command, prompt command, and pending complaint fields checked. */
void PlayerCmd_allowComplaint(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    gentity_t *target = Scr_GetEntity(0);

    if (ent == target) {
        return;
    }

    if (target->client->complaintDisabled != 0) {
        trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                               RELIABLE_SERVER_COMMAND,
                               PLAYER_COMPLAINT_DISABLED_COMMAND);
        return;
    }

    trap_SendServerCommand((uint32_t)(int)(ent - g_entities),
                           RELIABLE_SERVER_COMMAND,
                           va(PLAYER_COMPLAINT_PROMPT_COMMAND,
                              target->s.number));
    ent->client->pendingComplaintClient = target->s.clientNum;
    /* NOT_FROM_ORIGINAL_SOURCE: retain the target-width wrapping deadline;
     * the consumer orders this short interval by modulo-32-bit difference. */
    ent->client->pendingComplaintTime = coduo_int32_from_bits(
        (uint32_t)level.time + (uint32_t)GAME_COMPLAINT_WINDOW_MSEC);
}

/* VERIFIED_DECOMPILER(0x4718a, 5718a_script_method_player_showscoreboard.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; entity validation and Cmd_Score_f target checked. */
void PlayerCmd_showScoreboard(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    Cmd_Score_f(ent);
}

/* VERIFIED_DECOMPILER(0x4721b, 5721b_script_method_player_setspawnweapon.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; owned-weapon branch, currentWeapon store, and weaponState zero checked. */
void PlayerCmd_setSpawnWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(0)) & 0xff;

    if (Com_BitCheck(ent->client->ps.weaponBits, weapon) != 0) {
        ent->client->ps.currentWeapon = weapon;
        ent->client->ps.weaponState = WEAPON_STATE_IDLE;
    }
}

/* VERIFIED_DECOMPILER(0x47307, 57307_script_method_player_dropitem.c, VERIFY-NEXT-003-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue, weapon-vs-item branch, optional tag argument, Drop_Weapon/Drop_Item calls, and GScr_AddEntity result checked against current decompiler output. */
void PlayerCmd_dropItem(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    const char *itemName = Scr_GetString(0);
    int weapon = BG_GetWeaponIndexForName(itemName) & 0xff;
    gentity_t *dropped;

    if (weapon != 0) {
        const char *tagName = PLAYER_DROP_WEAPON_DEFAULT_TAG;

        if (Scr_GetNumParam() >= 2) {
            tagName = Scr_GetString(1);
        }

        dropped = Drop_Weapon(ent, weapon, tagName);
    } else {
        gitem_t *item = BG_FindItem(itemName);

        if (item == 0) {
            dropped = 0;
        } else {
            dropped = Drop_Item(ent, item, 0.0f, 0);
        }
    }

    GScr_AddEntity(dropped);
}

/* VERIFIED_DECOMPILER(0x47442, 57442_script_method_player_finishplayerdamage.c, VERIFY-NEXT-003-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - positive-damage gate, parameter entity tests, optional vector reads, ROUND knockback, godmode gate, bullet temp events, feedback fields, notify, death/pain callbacks, linked return, and health mirror checked against current decompiler output. */
void PlayerCmd_finishPlayerDamage(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    gentity_t *world = &g_entities[ENTITYNUM_WORLD];
    gentity_t *inflictor = world;
    gentity_t *attacker = world;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    vec3_t point = {0.0f, 0.0f, 0.0f};
    vec3_t rawDir;
    vec3_t dir;
    const float *pointPtr = 0;
    const float *rawDirPtr = 0;
    int damage = Scr_GetInt(2);

    if (damage <= 0) {
        return;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (Scr_GetType(0) != 0 &&
        Scr_GetPointerType(0) == SCRIPT_VAR_ENTITY) {
        inflictor = Scr_GetEntity(1);
    }
    if (Scr_GetType(1) != 0 &&
        Scr_GetPointerType(1) == SCRIPT_VAR_ENTITY) {
        attacker = Scr_GetEntity(1);
    }

    int damageFlags = Scr_GetInt(3);
    int meansOfDeath = G_IndexForMeansOfDeath(Scr_GetString(4));
    int weapon = BG_GetWeaponIndexForName(Scr_GetString(5)) & 0xff;

    if (Scr_GetType(6) != 0) {
        Scr_GetVector(6, point);
        pointPtr = point;
    }
    if (Scr_GetType(7) != 0) {
        Scr_GetVector(7, rawDir);
        rawDirPtr = rawDir;
    }

    int hitLocation =
        G_GetHitLocationIndexFromString(Scr_GetConstString(8));

    if (rawDirPtr == 0) {
        dir[0] = 0.0f;
        dir[1] = 0.0f;
        dir[2] = 0.0f;
    } else {
        VectorNormalize2(rawDirPtr, dir);
    }

    int knockback = 0;
    if ((ent->flags & ENTITY_FLAG_NO_KNOCKBACK) == 0 &&
        (damageFlags & DAMAGE_FLAG_NO_KNOCKBACK) == 0) {
        float knockbackScale = PLAYER_DAMAGE_KNOCKBACK_NORMAL;

        if ((ent->client->ps.playerStateFlags &
             PMF_PRONE) != 0) {
            knockbackScale = PLAYER_DAMAGE_KNOCKBACK_PRONE;
        } else if ((ent->client->ps.playerStateFlags &
                    PMF_DUCKED) != 0) {
            knockbackScale = PLAYER_DAMAGE_KNOCKBACK_CROUCH;
        }

        /* Stock 0x476ba: (int)(fild(damage) * knockbackScale) fistp-direct. */
#if EMULATE_X87
        knockback = x87f_store_i32_trunc(
            x87f_mul(x87f_load_i32(damage), x87f_load_f32(knockbackScale)));
#else
        knockback = game_compat_int32_from_long_double_trunc(
            (long double)damage * (long double)knockbackScale);
#endif
        if (knockback > PLAYER_DAMAGE_KNOCKBACK_MAX) {
            knockback = PLAYER_DAMAGE_KNOCKBACK_MAX;
        }

        if (knockback != 0) {
            vec3_t impulseDelta;

            /* Stock 0x4770e/0x4772b/0x47748: dir[k] * ((fild(knockback) *
             * g_knockback.value) / 250.0f) all 80-bit, one store per component.
             * Stock recomputes the base per k; deterministic, so one reused
             * x87f base is value-identical. */
#if EMULATE_X87
            x87f impulseBase = x87f_div(
                x87f_mul(x87f_load_i32(knockback),
                         x87f_load_f32(g_knockback.value)),
                x87f_load_f32(PLAYER_DAMAGE_KNOCKBACK_DIVISOR));
            impulseDelta[0] =
                x87f_store_f32(x87f_mul(x87f_load_f32(dir[0]), impulseBase));
            impulseDelta[1] =
                x87f_store_f32(x87f_mul(x87f_load_f32(dir[1]), impulseBase));
            impulseDelta[2] =
                x87f_store_f32(x87f_mul(x87f_load_f32(dir[2]), impulseBase));
#else
            impulseDelta[0] = (float)(
                (long double)dir[0] *
                (((long double)knockback *
                  (long double)g_knockback.value) /
                 (long double)PLAYER_DAMAGE_KNOCKBACK_DIVISOR));
            impulseDelta[1] = (float)(
                (long double)dir[1] *
                (((long double)knockback *
                  (long double)g_knockback.value) /
                 (long double)PLAYER_DAMAGE_KNOCKBACK_DIVISOR));
            impulseDelta[2] = (float)(
                (long double)dir[2] *
                (((long double)knockback *
                  (long double)g_knockback.value) /
                 (long double)PLAYER_DAMAGE_KNOCKBACK_DIVISOR));
#endif

            ent->client->ps.velocity[0] += impulseDelta[0];
            ent->client->ps.velocity[1] += impulseDelta[1];
            ent->client->ps.velocity[2] += impulseDelta[2];

            if (ent->client->ps.pmTime == 0) {
                int pmTime = knockback * 2;

                if (pmTime < PLAYER_DAMAGE_KNOCKBACK_PM_TIME_MIN) {
                    pmTime = PLAYER_DAMAGE_KNOCKBACK_PM_TIME_MIN;
                }
                if (pmTime > PLAYER_DAMAGE_KNOCKBACK_PM_TIME_MAX) {
                    pmTime = PLAYER_DAMAGE_KNOCKBACK_PM_TIME_MAX;
                }

                ent->client->ps.pmTime =
                    pmTime;
                ent->client->ps.playerStateFlags |=
                    PMF_NO_GROUNDFRICTION;
            }
        }
    }

    if ((ent->flags & ENTITY_FLAG_GODMODE) != 0) {
        return;
    }

    if (weapon != 0) {
        const weaponInfo_t *weaponInfo =
            (const weaponInfo_t *)BG_GetInfoForWeapon(weapon);

        if (weaponInfo->weaponType == WEAPTYPE_BULLET) {
            gentity_t *temp = G_TempEntity(point, EV_BULLET_HIT);

            temp->s.tempEffectId = DirToByte(dir) & 0xff;
            temp->s.hintStringIndex = DirToByte(dir) & 0xff;
            temp->s.surfType = PLAYER_DAMAGE_SURFACE_TYPE_FLESH;
            temp->s.weapon = attacker->s.weapon;
            temp->s.vehicleEntityNum = attacker->s.number;
            temp->svFlags |= PLAYER_DAMAGE_IMPACT_EVENT_FLAG;
            temp->tempClientNumFc = ent->client->ps.psClientNum;

            weaponInfo = BG_GetInfoForWeapon(weapon);
            int tempEvent =
                (weaponInfo->ricochet == 0)
                    ? EV_BULLET_HIT_CLIENT_SMALL
                    : EV_BULLET_HIT_CLIENT_LARGE;

            temp = G_TempEntity(point, tempEvent);
            temp->s.surfType = PLAYER_DAMAGE_SURFACE_TYPE_FLESH;
            temp->s.weapon = attacker->s.weapon;
            temp->s.vehicleEntityNum = attacker->s.number;
            temp->s.clientNum = ent->client->ps.psClientNum;
            temp->svFlags = PLAYER_DAMAGE_BULLET_EVENT_FLAGS;
            temp->tempClientNumFc = ent->client->ps.psClientNum;
        }
    }

    ent->client->damageTaken += damage;

    if (rawDirPtr == 0) {
        ent->client->damageFrom[0] = ent->currentOrigin[0];
        ent->client->damageFrom[1] = ent->currentOrigin[1];
        ent->client->damageFrom[2] = ent->currentOrigin[2];
        ent->client->damageFromWorld = 1;
    } else {
        ent->client->damageFrom[0] = dir[0];
        ent->client->damageFrom[1] = dir[1];
        ent->client->damageFrom[2] = dir[2];
        ent->client->damageFromWorld = 0;
    }

    ent->health -= damage;
    Scr_AddEntity(attacker);
    Scr_AddInt(damage);
    Scr_Notify(ent, scr_const_damage, 2);

    if (ent->health < 1) {
        if (ent->health < PLAYER_DAMAGE_MIN_HEALTH) {
            ent->health = PLAYER_DAMAGE_MIN_HEALTH;
        }

        ent->attacker = attacker;

        gentity_die_t die = ent->die;
        if (die != 0) {
            die(ent, inflictor, attacker, damage, meansOfDeath, weapon, dir,
                hitLocation);
        }
        if (ent->linked == 0) {
            return;
        }
    } else {
        gentity_pain_t pain = ent->pain;
        if (pain != 0) {
            pain(ent, attacker, damage, pointPtr, meansOfDeath, dir, hitLocation);
        }
    }

    ent->client->ps.stats[STAT_HEALTH] = ent->health;
}

/* VERIFIED_DECOMPILER(0x47c68, 57c68_script_method_player_suicide.c, VERIFY-NEXT-003-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue, godmode clear, health/health zero stores, and player_die self/self/self arguments with MOD_SUICIDE checked against current decompiler output. */
void PlayerCmd_Suicide(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    ent->flags &= ~ENTITY_FLAG_GODMODE;
    ent->health = 0;
    ent->client->ps.stats[STAT_HEALTH] = 0;
    player_die(ent, ent, ent, PLAYER_SUICIDE_DAMAGE, MOD_SUICIDE,
               0, 0, 0);
}

/* VERIFIED_DECOMPILER(0x47d64, 57d64_script_method_player_openmenu.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; connected-state gate, script menu index, open command, and int result checked. */
void PlayerCmd_OpenMenu(uint32_t entityNum)
{
    game_compat_script_player_open_menu(entityNum, PLAYER_OPENMENU_COMMAND);
}

/* VERIFIED_DECOMPILER(0x47e59, 57e59_script_method_player_openmenunomouse.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; connected-state gate, script menu index, no-mouse command, and int result checked. */
void PlayerCmd_OpenMenuNoMouse(uint32_t entityNum)
{
    game_compat_script_player_open_menu(entityNum, PLAYER_OPENMENU_NOMOUSE_COMMAND);
}

/* VERIFIED_DECOMPILER(0x47f4e, 57f4e_script_method_player_closemenu.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; close menu server command checked. */
void PlayerCmd_CloseMenu(uint32_t entityNum)
{
    trap_SendServerCommand(entityNum, RELIABLE_SERVER_COMMAND,
                           PLAYER_CLOSEMENU_COMMAND);
}

/* VERIFIED_DECOMPILER(0x47f9e, 57f9e_script_method_player_getweaponslotweapon.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; invalid session none result, slot parse, slot byte, and pickupName/none result checked. */
void PlayerCmd_GetWeaponSlotWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    if (WeaponSlotsNotValid(ent) != 0) {
        Scr_AddConstString(scr_const_none);
        return;
    }

    int slot = game_compat_script_player_parse_weapon_slot(0);
    int weapon = (int)(int8_t)ent->client->ps.weaponSlots[slot];

    if (weapon == 0) {
        Scr_AddConstString(scr_const_none);
        return;
    }

    Scr_AddString(((weaponInfo_t*)BG_GetInfoForWeapon(weapon))->pickupName);
}

/* VERIFIED_DECOMPILER(0x480f9, 580f9_script_method_player_setweaponslotweapon.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; slot parse, none/unknown weapon handling, primary slot compatibility, old take, give, and primaryB move checked. */
void PlayerCmd_SetWeaponSlotWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int movePrimaryToPrimaryB = 0;
    int slot = game_compat_script_player_parse_weapon_slot(0);
    const char *weaponName = Scr_GetString(1);
    int weapon;

    if (Q_stricmp(weaponName, PLAYER_NO_WEAPON_NAME) == 0) {
        weapon = 0;
    } else {
        weapon = BG_GetWeaponIndexForName(weaponName) & 0xff;
        if (weapon == 0) {
            Scr_ParamError(1, va("Unknown weapon %s.", weaponName));
        }

        const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
        int weaponSlot = weaponInfo->slot;

        if (weaponSlot != slot &&
            !((weaponSlot == WEAPSLOT_PRIMARY ||
               weaponSlot == WEAPSLOT_PRIMARYB) &&
              (slot == WEAPSLOT_PRIMARY ||
               slot == WEAPSLOT_PRIMARYB))) {
            const char *requestedSlotName =
                BG_GetWeaponSlotNameForIndex(slot);
            const char *weaponSlotName =
                BG_GetWeaponSlotNameForIndex(weaponSlot);

            Scr_ParamError(
                1,
                va("Weapon %s goes in the %s weaponslot, not the %s weaponslot.",
                   weaponName, weaponSlotName, requestedSlotName));
        }
    }

    int oldWeapon =
        (int)(int8_t)ent->client->ps.weaponSlots[slot];
    if (oldWeapon != 0) {
        BG_TakePlayerWeapon(&ent->client->ps, oldWeapon);
    }

    if (weapon != 0) {
        if (slot == WEAPSLOT_PRIMARYB &&
            (int)ent->client->ps.weaponSlots[WEAPSLOT_PRIMARY] == 0) {
            movePrimaryToPrimaryB = 1;
        }

        BG_GivePlayerWeapon(&ent->client->ps, weapon);

        if (movePrimaryToPrimaryB != 0) {
            ent->client->ps.weaponSlots[WEAPSLOT_PRIMARYB] =
                ent->client->ps.weaponSlots[WEAPSLOT_PRIMARY];
            ent->client->ps.weaponSlots[WEAPSLOT_PRIMARY] = 0;
        }
    }
}

/* VERIFIED_DECOMPILER(0x4838a, 5838a_script_method_player_getweaponslotammo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; invalid session/empty slot zero, clip-only branch, clip/reserve index, and Scr_AddInt checked. */
void PlayerCmd_GetWeaponSlotAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    if (WeaponSlotsNotValid(ent) != 0) {
        Scr_AddInt(0);
        return;
    }

    int slot = game_compat_script_player_parse_weapon_slot(0);
    int weapon = (int)(int8_t)ent->client->ps.weaponSlots[slot];

    if (weapon == 0) {
        Scr_AddInt(0);
        return;
    }

    if (BG_WeaponIsClipOnly(weapon) != 0) {
        int clipIndex = BG_ClipForWeapon(weapon);

        Scr_AddInt(ent->client->ps.clips[clipIndex]);
    } else {
        int ammoIndex = BG_AmmoForWeapon(weapon);

        Scr_AddInt(ent->client->ps.ammo[ammoIndex]);
    }
}

/* VERIFIED_DECOMPILER(0x4851a, 5851a_script_method_player_setweaponslotammo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; slot parse, empty slot no-op, clip-only branch, clamp call order, and ammo/clip stores checked. */
void PlayerCmd_SetWeaponSlotAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int slot = game_compat_script_player_parse_weapon_slot(0);
    int amount = Scr_GetInt(1);
    int weapon = (int)(int8_t)ent->client->ps.weaponSlots[slot];

    if (weapon == 0) {
        return;
    }

    if (BG_WeaponIsClipOnly(weapon) != 0) {
        int clipIndex = BG_ClipForWeapon(weapon);

        if (clipIndex != 0) {
            if (amount < 0) {
                amount = 0;
            } else if (BG_GetAmmoClipSize(clipIndex) < amount) {
                amount = BG_GetAmmoClipSize(clipIndex);
            }
            ent->client->ps.clips[clipIndex] = amount;
        }
    } else {
        int ammoIndex = BG_AmmoForWeapon(weapon);

        if (ammoIndex != 0) {
            if (amount < 0) {
                amount = 0;
            } else if (BG_GetAmmoTypeMax(ammoIndex) < amount) {
                amount = BG_GetAmmoTypeMax(ammoIndex);
            }
            ent->client->ps.ammo[ammoIndex] = amount;
        }
    }
}

/* VERIFIED_DECOMPILER(0x486f3, 586f3_script_method_player_getweaponslotclipammo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; invalid session/empty slot/zero clip index zero results and clip load checked. */
void PlayerCmd_GetWeaponSlotClipAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    if (WeaponSlotsNotValid(ent) != 0) {
        Scr_AddInt(0);
        return;
    }

    int slot = game_compat_script_player_parse_weapon_slot(0);
    int weapon = (int)(int8_t)ent->client->ps.weaponSlots[slot];

    if (weapon == 0) {
        Scr_AddInt(0);
        return;
    }

    int clipIndex = BG_ClipForWeapon(weapon);
    if (clipIndex == 0) {
        Scr_AddInt(0);
        return;
    }

    Scr_AddInt(ent->client->ps.clips[clipIndex]);
}

/* VERIFIED_DECOMPILER(0x4885d, 5885d_script_method_player_setweaponslotclipammo.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; slot parse, empty slot zero result, clip index branch, clamp call order, and clip store checked. */
void PlayerCmd_SetWeaponSlotClipAmmo(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    int slot = game_compat_script_player_parse_weapon_slot(0);
    int amount = Scr_GetInt(1);
    int weapon = (int)(int8_t)ent->client->ps.weaponSlots[slot];

    if (weapon == 0) {
        Scr_AddInt(0);
        return;
    }

    int clipIndex = BG_ClipForWeapon(weapon);
    if (clipIndex != 0) {
        if (amount < 0) {
            amount = 0;
        }
        if (BG_GetAmmoClipSize(clipIndex) < amount) {
            amount = BG_GetAmmoClipSize(clipIndex);
        }
        ent->client->ps.clips[clipIndex] = amount;
    }
}

/* VERIFIED_DECOMPILER(0x489d0, 589d0_script_method_player_iprintln.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; Scr_MakeGameMessage iprintln command checked. */
void iclientprintln(uint32_t entityNum)
{
    Scr_MakeGameMessage(entityNum, PLAYER_IPRINTLN_COMMAND);
}

/* VERIFIED_DECOMPILER(0x489fd, 589fd_script_method_player_iprintlnbold.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; Scr_MakeGameMessage bold command checked. */
void iclientprintlnbold(uint32_t entityNum)
{
    Scr_MakeGameMessage(entityNum, PLAYER_IPRINTLN_BOLD_COMMAND);
}

/* VERIFIED_DECOMPILER(0x48a2a, 58a2a_script_method_player_spawn.c, VERIFY-P1-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue, Scr_GetVector(0/1), and ClientSpawn argument order checked against current decompiler output. */
void PlayerCmd_spawn(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    vec3_t origin;
    vec3_t angles;

    Scr_GetVector(0, origin);
    Scr_GetVector(1, angles);
    ClientSpawn(ent, origin, angles);
}

/* VERIFIED_DECOMPILER(0x48aef, 58aef_script_method_player_setentertime.c, VERIFY-P1-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue and Scr_GetInt(0) store to client enterTime checked against current decompiler output. */
void PlayerCmd_setEnterTime(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    gclient_t *client = ent->client;

    client->enterTime = Scr_GetInt(0);
}

/* VERIFIED_DECOMPILER(0x48b92, 58b92_FUN_00058b92.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; temporary corpse clone s_flags clear checked. */
void BodyEnd(gentity_t *clone)
{
    clone->s.eFlags &= ~PLAYER_CLONE_TEMP_S_FLAG;
}

/* VERIFIED_DECOMPILER(0x48ba9, 58ba9_script_method_player_cloneplayer.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; clone spawn, flags, origin/angle, trajectory, contents, svFlags, bounds, anims, link, lifetime, think, and result entity checked. */
void PlayerCmd_ClonePlayer(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    gclient_t *client = ent->client;
    gentity_t *clone = G_SpawnPlayerClone();

    clone->s.clientNum = client->ps.psClientNum;
    clone->s.eFlags =
        (clone->s.eFlags & PLAYER_CLONE_KEEP_S_FLAG) |
        (client->ps.entityStateFlags & PLAYER_CLONE_PS_FLAGS_MASK) |
        PLAYER_CLONE_TEMP_S_FLAG;

    G_SetOrigin(clone, ent->currentOrigin);
    G_SetAngle(clone, ent->currentAngles);
    clone->currentOrigin[2] += 1.0f;
    clone->s.pos.trBase[2] +=
        1.0f;

    clone->s.pos.trType =
        TR_GRAVITY;
    clone->s.pos.trTime =
        level.time;
    clone->s.pos.trDelta[0] =
        client->ps.velocity[0];
    clone->s.pos.trDelta[1] =
        client->ps.velocity[1];
    clone->s.pos.trDelta[2] =
        client->ps.velocity[2];

    clone->s.eType = ET_PLAYER_CORPSE;
    clone->scriptContents =
        PLAYER_CLONE_CONTENTS;
    clone->linkedByte16d = 1;
    clone->s.groundEntityNum =
        ENTITYNUM_NONE;
    /* 0x48d3e writes 0x200 to gentity +0x0f8, the server svFlags slot. */
    clone->svFlags =
        SVF_CAPSULE;

    memcpy(clone->mins, ent->mins, sizeof(clone->mins));
    memcpy(clone->maxs, ent->maxs, sizeof(clone->maxs));
    /* VERIFIED_DECOMPILER(0x48ba9, 58ba9_script_method_player_cloneplayer.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; clone contents at +0x120 stay PLAYER_CLONE_CONTENTS and source entity copy resumes at absMin +0x124. */
    memcpy(clone->absMin, ent->absMin, sizeof(clone->absMin));
    memcpy(clone->absMax, ent->absMax, sizeof(clone->absMax));

    clone->s.legsAnim =
        client->ps.legsAnim;
    clone->s.torsoAnim =
        client->ps.torsoAnim;
    clone->clipmask =
        PLAYER_CLONE_CLIPMASK;

    trap_LinkEntity(clone);

    clone->nextthink =
        level.time + PLAYER_CLONE_LIFETIME_MS;
    clone->think = BodyEnd;

    GScr_AddEntity(clone);
}

/* VERIFIED_DECOMPILER(0x48e90, 58e90_script_method_player_setclientcvar.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; string/message value path, strlen, 1024-byte memset, 8192-byte clean loop, quote replacement, and v command checked. */
void PlayerCmd_SetClientCvar(uint32_t entityNum)
{
    const char *name;
    char messageValue[MAX_STRING_CHARS];
    char cleaned[MAX_STRING_CHARS];
    const char *value;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    if (g_entities[entityNum].client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    name = Scr_GetString(0);
    value = game_compat_script_player_get_client_cvar_value(messageValue);

    (void)strlen(value);
    if (game_compat_script_player_clean_client_cvar_value(value, cleaned) ==
        qfalse) {
        Scr_ParamError(1, "setclientcvar value is too long");
        return;
    }

    trap_SendServerCommand(
        entityNum,
        RELIABLE_SERVER_COMMAND,
        va("v %s \"%s\"", name, cleaned));
}

/* VERIFIED_DECOMPILER(0x49043, 59043_script_method_player_freezecontrols.c, VERIFY-P1-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue and Scr_GetBool(0) store to controlsFrozen checked against current decompiler output. */
void PlayerCmd_FreezeControls(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    gclient_t *client = ent->client;

    client->controlsFrozen = Scr_GetBool(0);
}

/* VERIFIED_DECOMPILER(0x490e6, 590e6_script_method_player_disableweapon.c, VERIFY-P1-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue and OR of PMF_WEAPON_DISABLED into playerStateFlags checked against current decompiler output. */
void PlayerCmd_DisableWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    ent->client->ps.playerStateFlags |= PMF_WEAPON_DISABLED;
}

/* VERIFIED_DECOMPILER(0x49189, 59189_script_method_player_enableweapon.c, VERIFY-P1-SCRIPTPLAYER-2026-06-17): DATAFLOW_VERIFIED - entity validation prologue and AND clear of PMF_WEAPON_DISABLED from playerStateFlags checked against current decompiler output. */
void PlayerCmd_EnableWeapon(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    ent->client->ps.playerStateFlags &= ~PMF_WEAPON_DISABLED;
}

/* VERIFIED_DECOMPILER(0x4922c, 5922c_script_method_player_setreverb.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; param-count branches, wet/fade defaults, usage error, and reverb command checked. */
void PlayerCmd_SetReverb(uint32_t entityNum)
{
    uint32_t paramCount;
    float wetLevel = 0.5f;
    float fadeTime = 0.0f;
    const char *roomType;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    if (g_entities[entityNum].client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    paramCount = Scr_GetNumParam();
    if (paramCount == 2) {
        wetLevel = Scr_GetFloat(1);
    } else if (paramCount == 3) {
        fadeTime = Scr_GetFloat(2);
        wetLevel = Scr_GetFloat(1);
    } else if (paramCount != 1) {
        Scr_Error(
            "USAGE: player setReverb(\"roomtype\", wetlevel = 0.5, fadetime = 1);\n"
            "wetlevel is a float from 0 (no effect) to 1 (full effect), "
            "fadetime is in sec and just modifies wetlevel\n");
        return;
    }

    roomType = Scr_GetString(0);
    trap_SendServerCommand(
        entityNum,
        RELIABLE_SERVER_COMMAND,
        va("r \"%s\" %g %g", roomType, (double)wetLevel, (double)fadeTime));
}

/* VERIFIED_DECOMPILER(0x4935b, 5935b_script_method_player_islookingat.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; Scr_GetEntity and lookAtEntity pointer comparison checked. */
void ScrCmd_IsLookingAt(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    gclient_t *client = ent->client;
    gentity_t *target = Scr_GetEntity(0);

    Scr_AddInt(client->lookAtEntity == target);
}

/* VERIFIED_DECOMPILER(0x4940c, 5940c_script_method_player_playlocalsound.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; sound alias index and local sound command checked. */
void ScrCmd_PlayLocalSound(uint32_t entityNum)
{
    const char *name;
    uint8_t aliasIndex;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    if (g_entities[entityNum].client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    name = Scr_GetString(0);
    aliasIndex = G_SoundAliasIndex(name);
    trap_SendServerCommand(entityNum, 0, va("s %i", (int)aliasIndex));
}

/* VERIFIED_DECOMPILER(0x494d8, 594d8_script_method_player_playlocalannouncersound.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; sound alias index and announcer command checked. */
void ScrCmd_PlayLocalAnnouncerSound(uint32_t entityNum)
{
    const char *name;
    uint8_t aliasIndex;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    if (g_entities[entityNum].client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    name = Scr_GetString(0);
    aliasIndex = G_SoundAliasIndex(name);
    trap_SendServerCommand(entityNum, 0, va("S %i", (int)aliasIndex));
}

/* VERIFIED_DECOMPILER(0x495a4, 595a4_script_method_player_sayall.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; message construction, prefix byte, G_Say target, and all mode checked. */
void PlayerCmd_SayAll(uint32_t entityNum)
{
    game_compat_script_player_say(entityNum, PLAYER_SAY_ALL);
}

/* VERIFIED_DECOMPILER(0x4968f, 5968f_script_method_player_sayteam.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; message construction, prefix byte, G_Say target, and team mode checked. */
void PlayerCmd_SayTeam(uint32_t entityNum)
{
    game_compat_script_player_say(entityNum, PLAYER_SAY_TEAM);
}

/* VERIFIED_DECOMPILER(0x4977a, 5977a_script_method_player_saysquad.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; message construction, prefix byte, G_Say target, and squad mode checked. */
void PlayerCmd_SaySquad(uint32_t entityNum)
{
    game_compat_script_player_say(entityNum, PLAYER_SAY_SQUAD);
}

/* VERIFIED_DECOMPILER(0x49865, 59865_script_method_player_allowspectateteam.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; team const mask mapping, param error, bool branch, and deny-mask set/clear checked. */
void PlayerCmd_AllowSpectateTeam(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    uint16_t teamName = Scr_GetConstString(0);
    uint32_t denyMask = game_compat_script_player_spectate_team_deny_mask(teamName);

    if (Scr_GetBool(1) == 0) {
        ent->client->spectateTeamDenyMask |= denyMask;
    } else {
        ent->client->spectateTeamDenyMask &= ~denyMask;
    }
}

/* VERIFIED_DECOMPILER(0x499dd, 599dd_script_method_player_getguid.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; no-param validation, trap_GetGuid, and Scr_AddInt checked. */
void PlayerCmd_GetGuid(uint32_t entityNum)
{
    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    if (g_entities[entityNum].client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    if (Scr_GetNumParam() != 0) {
        Scr_Error("USAGE: self getGuid()\n");
    }

    Scr_AddInt(trap_GetGuid(entityNum));
}

/* VERIFIED_DECOMPILER(0x49a8d, 59a8d_script_method_player_getfatigue.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; no-param validation and fatigueScale return checked. */
void PlayerCmd_GetFatigue(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }

    if (Scr_GetNumParam() != 0) {
        Scr_Error("USAGE: self getFatigue()\n");
    }

    Scr_AddFloat(ent->client->ps.fatigueScale);
}

/* VERIFIED_DECOMPILER(0x49b41, 59b41_script_method_player_setfatigue.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; one-param validation, float clamp to [0,1], and fatigueScale store checked. */
void PlayerCmd_SetFatigue(uint32_t entityNum)
{
    gentity_t *ent;

    if (entityNum > ENTITYNUM_NONE) {
        Scr_Error(va("%i is not a valid entity number", (int)entityNum));
    }

    ent = &g_entities[entityNum];
    if (ent->client == NULL) {
        Scr_Error(va("entity %i is not a player", (int)entityNum));
    }
    float fatigue;

    if (Scr_GetNumParam() != 1) {
        Scr_Error("USAGE: self setFatigue([0.0,1.0])\n");
    }

    fatigue = Scr_GetFloat(0);
    if (fatigue < PLAYER_FATIGUE_MIN) {
        fatigue = PLAYER_FATIGUE_MIN;
    } else if (fatigue > PLAYER_FATIGUE_MAX) {
        fatigue = PLAYER_FATIGUE_MAX;
    }

    ent->client->ps.fatigueScale = fatigue;
}

/* VERIFIED_DECOMPILER(0x49c48, 59c48_script_method_player_autodemostart.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; no-param validation, g_autodemo gate, start command, and enabled-only int result checked. */
void PlayerCmd_AutoDemoStart(uint32_t entityNum)
{
    game_compat_script_player_auto_client_command(
        entityNum,
        "USAGE: self autoDemoStart()\n",
        "g_autodemo",
        AUTODEMO_START_COMMAND);
}

/* VERIFIED_DECOMPILER(0x49d22, 59d22_script_method_player_autodemostop.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; no-param validation, g_autodemo gate, stop command, and enabled-only int result checked. */
void PlayerCmd_AutoDemoStop(uint32_t entityNum)
{
    game_compat_script_player_auto_client_command(
        entityNum,
        "USAGE: self autoDemoStop()\n",
        "g_autodemo",
        AUTODEMO_STOP_COMMAND);
}

/* VERIFIED_DECOMPILER(0x49dfc, 59dfc_script_method_player_autoscreenshot.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; no-param validation, g_autoscreenshot gate, screenshot command, and enabled-only int result checked. */
void PlayerCmd_ClientAutoScreenshot(uint32_t entityNum)
{
    game_compat_script_player_auto_client_command(
        entityNum,
        "USAGE: self autoScreenshot()\n",
        "g_autoscreenshot",
        AUTOSCREENSHOT_COMMAND);
}

static const script_method_t playerMethods[] = {
    {"giveweapon", PlayerCmd_giveWeapon},
    {"takeweapon", PlayerCmd_takeWeapon},
    {"takeallweapons", PlayerCmd_takeAllWeapons},
    {"getcurrentweapon", PlayerCmd_getCurrentWeapon},
    {"hasweapon", PlayerCmd_hasWeapon},
    {"switchtoweapon", PlayerCmd_switchToWeapon},
    {"givestartammo", PlayerCmd_giveStartAmmo},
    {"giveclipammo", PlayerCmd_giveClipAmmo},
    {"givemaxammo", PlayerCmd_giveMaxAmmo},
    {"getfractionstartammo", PlayerCmd_getFractionStartAmmo},
    {"getfractionmaxammo", PlayerCmd_getFractionMaxAmmo},
    {"setorigin", PlayerCmd_setOrigin},
    {"setplayerangles", PlayerCmd_setAngles},
    {"usebuttonpressed", PlayerCmd_useButtonPressed},
    {"attackbuttonpressed", PlayerCmd_attackButtonPressed},
    {"meleebuttonpressed", PlayerCmd_meleeButtonPressed},
    {"isads", PlayerCmd_isAds},
    {"isinvehicle", PlayerCmd_isInVehicle},
    {"isonground", PlayerCmd_isOnGround},
    {"pingplayer", PlayerCmd_pingPlayer},
    {"setviewmodel", PlayerCmd_SetViewmodel},
    {"getviewmodel", PlayerCmd_GetViewmodel},
    {"sayall", PlayerCmd_SayAll},
    {"sayteam", PlayerCmd_SayTeam},
    {"saysquad", PlayerCmd_SaySquad},
    {"allowcomplaint", PlayerCmd_allowComplaint},
    {"showscoreboard", PlayerCmd_showScoreboard},
    {"setspawnweapon", PlayerCmd_setSpawnWeapon},
    {"dropitem", PlayerCmd_dropItem},
    {"finishplayerdamage", PlayerCmd_finishPlayerDamage},
    {"suicide", PlayerCmd_Suicide},
    {"openmenu", PlayerCmd_OpenMenu},
    {"openmenunomouse", PlayerCmd_OpenMenuNoMouse},
    {"closemenu", PlayerCmd_CloseMenu},
    {"freezecontrols", PlayerCmd_FreezeControls},
    {"disableweapon", PlayerCmd_DisableWeapon},
    {"enableweapon", PlayerCmd_EnableWeapon},
    {"setreverb", PlayerCmd_SetReverb},
    {"getweaponslotweapon", PlayerCmd_GetWeaponSlotWeapon},
    {"setweaponslotweapon", PlayerCmd_SetWeaponSlotWeapon},
    {"getweaponslotammo", PlayerCmd_GetWeaponSlotAmmo},
    {"setweaponslotammo", PlayerCmd_SetWeaponSlotAmmo},
    {"getweaponslotclipammo", PlayerCmd_GetWeaponSlotClipAmmo},
    {"setweaponslotclipammo", PlayerCmd_SetWeaponSlotClipAmmo},
    {"iprintln", iclientprintln},
    {"iprintlnbold", iclientprintlnbold},
    {"spawn", PlayerCmd_spawn},
    {"setentertime", PlayerCmd_setEnterTime},
    {"cloneplayer", PlayerCmd_ClonePlayer},
    {"setclientcvar", PlayerCmd_SetClientCvar},
    {"islookingat", ScrCmd_IsLookingAt},
    {"playlocalsound", ScrCmd_PlayLocalSound},
    {"playlocalannouncersound", ScrCmd_PlayLocalAnnouncerSound},
    {"allowspectateteam", PlayerCmd_AllowSpectateTeam},
    {"getguid", PlayerCmd_GetGuid},
    {"getfatigue", PlayerCmd_GetFatigue},
    {"setfatigue", PlayerCmd_SetFatigue},
    {"autodemostart", PlayerCmd_AutoDemoStart},
    {"autodemostop", PlayerCmd_AutoDemoStop},
    {"autoscreenshot", PlayerCmd_ClientAutoScreenshot},
};

/* VERIFIED_DECOMPILER(0x49ed6, 59ed6_Player_GetMethod.c, VERIFY-SCRIPT-PLAYER-CMDS-2026-06-17): DATAFLOW_VERIFIED; entry-time requested-name capture, 60-entry table lookup, strcmp loop, canonical name writeback, callback return, and null miss checked. */
script_method_callback_t Player_GetMethod(const char **name)
{
    const char *requestedName = *name;

    for (uint32_t index = 0; index < sizeof(playerMethods) / sizeof(playerMethods[0]);
         index++) {
        if (strcmp(requestedName, playerMethods[index].name) == 0) {
            *name = playerMethods[index].name;
            return playerMethods[index].callback;
        }
    }

    return 0;
}
