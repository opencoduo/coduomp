/*
 * Source reconstruction for damage/death helper functions.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "recovered_game.h"
#include "qcommon/info.h"
#include "game_globals.h"
#include "scr_vm.h"
#include "compat/coduo_x87emu.h"
#include "compat/coduo_native_x87.h"
#include "qcommon/q_string.h"

#define HITLOC_TABLE_MAX_INFO_SIZE 8192
void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
void G_Error(const char *format, ...);
int trap_FS_FOpenFile(const char *path, int *handle, fsMode_t mode);
void trap_FS_FCloseFile(int handle);
void trap_FS_Read(void *buffer, int length, int handle);
uint16_t hitLocationConstStrings[HITLOC_COUNT]; /* DAT_000d1300 */
float g_fHitLocDamageMult[HITLOC_COUNT];

static const char *hitLocationNames[HITLOC_COUNT] = {
    "none",
    "helmet",
    "head",
    "neck",
    "torso_upper",
    "torso_lower",
    "right_arm_upper",
    "left_arm_upper",
    "right_arm_lower",
    "left_arm_lower",
    "right_hand",
    "left_hand",
    "right_leg_upper",
    "left_leg_upper",
    "right_leg_lower",
    "left_leg_lower",
    "right_foot",
    "left_foot",
    "gun",
};

/* VERIFIED_DECOMPILER(0x4f4c0, 5f4c0_FUN_0005f4c0.c, VERIFY-DAMAGE-STALE-SMALL-2026-06-17): DATAFLOW_VERIFIED - strcpy wrapper call target, argument order, void return, and absence of side effects checked against current decompiler output. */
/* 0x4f4c0 G_HitLocStrcpy: exact original identity supplied by the UO Mac symbol. */
static void G_HitLocStrcpy(char *dest, const char *src)
{
    strcpy(dest, src);
}

/* VERIFIED_DECOMPILER(0x4f4ea, 5f4ea_G_ParseHitLocDmgTable.c, VERIFY-DAMAGE-STALE-SMALL-2026-06-17): DATAFLOW_VERIFIED - 19-entry field table initialization, float defaults, Scr_AllocString writes, gun multiplier zero, file/magic/length validation, close point, Info_Validate branch, and ParseConfigStringToStruct arguments checked against current decompiler output. */
/* 0x4f4ea G_ParseHitLocDmgTable */
void G_ParseHitLocDmgTable(void)
{
    parseField_t fields[HITLOC_COUNT];
    char buffer[HITLOC_TABLE_MAX_INFO_SIZE];
    const char *path = "info/mp_lochit_dmgtable";
    const char *magic = "LOCDMGTABLE";
    int handle;
    int fileLength;
    int magicLength = (int)strlen(magic);
    int payloadLength;

    for (int index = 0; index < HITLOC_COUNT; index++) {
        g_fHitLocDamageMult[index] = 1.0f;
        fields[index].key = hitLocationNames[index];
        fields[index].offset = index * (int32_t)sizeof(float);
        fields[index].type = PARSE_FIELD_FLOAT;
        hitLocationConstStrings[index] = Scr_AllocString(hitLocationNames[index], 1);
    }

    g_fHitLocDamageMult[HITLOC_GUN] = 0.0f;

    fileLength = trap_FS_FOpenFile(path, &handle, FS_READ);
    if (fileLength < 1) {
        Com_Error(1, COM_ERROR_MARKER "Could not load hitloc damage table %s\n",
                  path);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (fileLength < magicLength) {
        Com_Error(1,
                  COM_ERROR_MARKER
                  "\"%s\" does not appear to be a hitloc damage table\n",
                  path);
        return;
    }

    memset(buffer, 0, (size_t)magicLength + 1u);
    trap_FS_Read(buffer, magicLength, handle);
    buffer[magicLength] = '\0';
    if (strncmp(buffer, magic, (size_t)magicLength) != 0) {
        Com_Error(1,
                  COM_ERROR_MARKER
                  "\"%s\" does not appear to be a hitloc damage table\n",
                  path);
        return;
    }

    payloadLength = fileLength - magicLength;
    if (payloadLength >= HITLOC_TABLE_MAX_INFO_SIZE) {
        Com_Error(1,
                  COM_ERROR_MARKER
                  "\"%s\" Is too long of a hitloc damage table to parse\n",
                  path);
        return;
    }

    memset(buffer, 0, (size_t)payloadLength + 1u);
    trap_FS_Read(buffer, payloadLength, handle);
    buffer[payloadLength] = '\0';
    trap_FS_FCloseFile(handle);

    if (Info_Validate(buffer) == 0) {
        Com_Error(1,
                  COM_ERROR_MARKER
                  "\"%s\" is not a valid hitloc damage table\n",
                  path);
        return;
    }

    if (ParseConfigStringToStruct(g_fHitLocDamageMult, fields,
                                  HITLOC_COUNT, buffer, 0, 0,
                                  G_HitLocStrcpy) == 0) {
        G_Error("Error parsing hitloc damage table %s\n", path);
    }
}

/* VERIFIED_DECOMPILER(0x4f7c7, 5f7c7_AddScore.c, VERIFY-DAMAGE-STALE-SMALL-2026-06-17): DATAFLOW_VERIFIED - empty void stub and return-only behavior checked against current decompiler output. */
/* 0x4f7c7 AddScore */
void AddScore(void)
{
}

/* VERIFIED_DECOMPILER(0x4f7cc, 5f7cc_LookAtKiller.c, VERIFY-DAMAGE-STALE-SMALL-2026-06-17): DATAFLOW_VERIFIED - attacker-first branch, inflictor fallback, self/current yaw fallback, currentOrigin/currentAngles/deathYaw offsets, C-cast truncation conversions, and duplicate vectoyaw call checked against current decompiler output. */
/* 0x4f7cc LookAtKiller */
void LookAtKiller(gentity_t *self, gentity_t *inflictor, gentity_t *attacker)
{
    vec3_t dir;

    if (attacker != 0 && attacker != self) {
        dir[0] = attacker->currentOrigin[0] - self->currentOrigin[0];
        dir[1] = attacker->currentOrigin[1] - self->currentOrigin[1];
        dir[2] = attacker->currentOrigin[2] - self->currentOrigin[2];
    } else if (inflictor != 0 && inflictor != self) {
        dir[0] = inflictor->currentOrigin[0] - self->currentOrigin[0];
        dir[1] = inflictor->currentOrigin[1] - self->currentOrigin[1];
        dir[2] = inflictor->currentOrigin[2] - self->currentOrigin[2];
    } else {
#if EMULATE_X87
        self->client->ps.stats[STAT_DEAD_YAW] = x87f_store_i32_trunc(
            x87f_load_f32(self->currentAngles[1]));
#elif defined(__x86_64__)
        self->client->ps.stats[STAT_DEAD_YAW] =
            CODUO_X87_TRUNCATE_I32((long double)self->currentAngles[1]);
#else
        self->client->ps.stats[STAT_DEAD_YAW] =
            (int32_t)self->currentAngles[1];
#endif
        return;
    }

#if EMULATE_X87
    {
        const float killerYaw = vectoyaw(dir);
        self->client->ps.stats[STAT_DEAD_YAW] =
            x87f_store_i32_trunc(x87f_load_f32(killerYaw));
    }
#elif defined(__x86_64__)
    {
        const float killerYaw = vectoyaw(dir);
        self->client->ps.stats[STAT_DEAD_YAW] =
            CODUO_X87_TRUNCATE_I32((long double)killerYaw);
    }
#else
    self->client->ps.stats[STAT_DEAD_YAW] = (int32_t)vectoyaw(dir);
#endif
    vectoyaw(dir);
}

/* VERIFIED_DECOMPILER(0x4f8fe, 5f8fe_G_IndexForMeansOfDeath.c, VERIFY-DAMAGE-STALE-SMALL-2026-06-17): DATAFLOW_VERIFIED - modNames linear search over indices 0..26, Q_stricmp argument order, unknown-name print, and zero fallback checked against current decompiler output. */
/* 0x4f8fe G_IndexForMeansOfDeath */
int G_IndexForMeansOfDeath(const char *name)
{
    for (int index = 0; index < MOD_COUNT; index++) {
        if (Q_stricmp(name, modNames[index]) == 0) {
            return index;
        }
    }

    Com_Printf("Unknown means of death string '%s'\n", name);
    return 0;
}

/* VERIFIED_DECOMPILER(0x511dc, 611dc_G_GetHitLocationString.c, VERIFY-DAMAGE-STALE-SMALL-2026-06-17): DATAFLOW_VERIFIED - 16-bit hit-location string table load with index scaling checked against current decompiler output. */
/* 0x511dc G_GetHitLocationString */
uint16_t G_GetHitLocationString(int hitLocation)
{
    return hitLocationConstStrings[hitLocation];
}

/* VERIFIED_DECOMPILER(0x511f9, 611f9_G_GetHitLocationIndexFromString.c, VERIFY-DAMAGE-STALE-SMALL-2026-06-17): DATAFLOW_VERIFIED - 16-bit table comparison loop over indices 0..18 and zero fallback checked against current decompiler output. */
/* 0x511f9 G_GetHitLocationIndexFromString */
int G_GetHitLocationIndexFromString(uint16_t hitLocationName)
{
    for (int index = 0; index < HITLOC_COUNT; index++) {
        if (hitLocationConstStrings[index] == hitLocationName) {
            return index;
        }
    }

    return 0;
}
