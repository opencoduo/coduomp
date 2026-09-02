/*
 * Source reconstruction for script entity interaction methods.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "recovered_game.h"
#include "game_globals.h"
#include "scr_vm.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#define ENTITY_FLAG_HIDDEN 0x00001000u
#define ENTITY_FLAG_GRENADE_TOUCH_DAMAGE 0x00008000u
#define ENTITY_FLAG_NO_GRENADE_BOUNCE 0x00010000u
#define CLEARVEHICLEPOSITION_MAX_ENTITIES 10
#define CLEARVEHICLEPOSITION_CONTENTS_MASK MASK_CLEAR_VEHICLE_POSITION
#define VERIFYPOSITION_MAX_ENTITIES 10
#define VERIFYPOSITION_EXTRA_CONTENTS 0x00800000u
#define VERIFYPOSITION_CONTENTS_MASK \
    (CONTENTS_BODY | VERIFYPOSITION_EXTRA_CONTENTS)
#define VERIFYPOSITION_VEHICLE_BOUNDS_SCALE 1.2f
#define VERIFYPOSITION_VEHICLE_BOTTOM_ADJUST 32.0f
#define HINT_STRING_CONFIGSTRING_BASE CS_HINTSTRINGS
#define HINT_STRING_CONFIGSTRING_COUNT CS_HINTSTRINGS_COUNT
#define CURSOR_HINT_TABLE_LIMIT 267
#define HINTSTRING_INHERIT 255
#define HINT_INHERIT_STRING "HINT_INHERIT"
#define SET_HINTSTRING_TRIGGER_USE_ERROR \
    "The setHintString command only works on trigger_use entities.\n"
#define HINTSTRING_LIMIT_ERROR \
    "Too many different hintstring values. Max allowed is %i different strings"
#define SOUND_EVENT_DURATION_MSEC 300

qboolean trap_EntityContact(const float *mins, const float *maxs,
                            gentity_t *ent);
qboolean trap_EntityContactCapsule(const float *mins, const float *maxs,
                                   gentity_t *ent);
int trap_EntitiesInBox(const float *mins, const float *maxs,
                              int *entityList, int maxCount, int contentMask);
void trap_Trace(trace_t *trace, const float *start, const float *mins,
                       const float *maxs, const float *end, int passEntityNum,
                       int contentMask);
/* NOT_FROM_ORIGINAL_SOURCE: local predicate extracted from door method bodies. */
static qboolean game_compat_script_entity_is_door(gentity_t *ent)
{
    return ent->scriptClassname == scr_const_func_door ||
           ent->scriptClassname == scr_const_func_door_rotating;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper for repeated currentOrigin + bounds math. */
static void game_compat_script_entity_add_abs_bounds(gentity_t *ent, vec3_t mins, vec3_t maxs)
{
    mins[0] = ent->currentOrigin[0] + ent->mins[0];
    mins[1] = ent->currentOrigin[1] + ent->mins[1];
    mins[2] = ent->currentOrigin[2] + ent->mins[2];

    maxs[0] = ent->currentOrigin[0] + ent->maxs[0];
    maxs[1] = ent->currentOrigin[1] + ent->maxs[1];
    maxs[2] = ent->currentOrigin[2] + ent->maxs[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
static qboolean game_compat_script_entity_contents_bounds_touch(const vec3_t aMins,
                                                 const vec3_t aMaxs,
                                                 gentity_t *b)
{
    vec3_t bMins;
    vec3_t bMaxs;

    game_compat_script_entity_add_abs_bounds(b, bMins, bMaxs);

    return aMins[0] < bMaxs[0] ||
           aMins[1] < bMaxs[1] ||
           aMins[2] < bMaxs[2] ||
           bMins[0] < aMaxs[0] ||
           bMins[1] < aMaxs[1] ||
           bMins[2] < aMaxs[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper extracted from damage direction setup. */
static const float *game_compat_script_entity_get_damage_dir(gentity_t *target,
                                              const vec3_t source,
                                              vec3_t dir)
{
    const float *origin;

    if (target->client == 0) {
        origin = target->currentOrigin;
    } else {
        origin = target->client->ps.psOrigin;
    }

    dir[0] = origin[0] - source[0];
    dir[1] = origin[1] - source[1];
    dir[2] = origin[2] - source[2];

    if (VectorNormalize(dir) == 0.0f) {
        return 0;
    }

    return dir;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper for delete/clearvehicleposition death cleanup. */
static void game_compat_script_entity_schedule_death_free(gentity_t *ent)
{
    Scr_AddEntity(ent);
    Scr_Notify(ent, scr_const_death, 1);
    trap_UnlinkEntity(ent);
    ent->use = 0;
    ent->touch = 0;
    ent->think = G_FreeEntity;
    ent->nextthink = level.time + 100;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper for vehicle bounds translation. */
static void game_compat_script_entity_offset_bounds(vec3_t mins, vec3_t maxs,
                                      const vec3_t origin)
{
    mins[0] += origin[0];
    mins[1] += origin[1];
    mins[2] += origin[2];
    maxs[0] += origin[0];
    maxs[1] += origin[1];
    maxs[2] += origin[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local predicate extracted from hint methods. */
static qboolean game_compat_script_entity_is_trigger_use(gentity_t *ent)
{
    return ent->scriptClassname == scr_const_trigger_use;
}

/* NOT_FROM_ORIGINAL_SOURCE: local predicate extracted from turret heat methods. */
static qboolean game_compat_script_entity_is_turret(gentity_t *ent)
{
    return ent->scriptClassname == scr_const_misc_mg42 ||
           ent->scriptClassname == scr_const_misc_turret;
}

/* NOT_FROM_ORIGINAL_SOURCE: local validation helper for turret script methods. */
static void game_compat_script_entity_require_turret(gentity_t *ent)
{
    if (!game_compat_script_entity_is_turret(ent)) {
        Scr_Error("Can only call turret script functions on misc_mg42 or misc_turret");
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local validation helper for grenade touch damage methods. */
static void game_compat_script_entity_require_damage_trigger(gentity_t *ent)
{
    if (ent->scriptClassname != scr_const_trigger_damage) {
        Scr_Error("Currently on supported on damage triggers");
    }
}

/* VERIFIED_DECOMPILER(0x698e0, 798e0_G_GetHintStringIndex.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - configstring scan limit, empty-slot allocation, strcmp reuse path, out index stores, and failure -1/0 return checked against current decompiler output. */
qboolean G_GetHintStringIndex(int *outIndex, const char *value)
{
    char configString[MAX_STRING_CHARS];
    int index;

    for (index = 0; index < HINT_STRING_CONFIGSTRING_COUNT; index++) {
        trap_GetConfigstring(HINT_STRING_CONFIGSTRING_BASE + index,
                             configString, MAX_STRING_CHARS);

        if (configString[0] == '\0') {
            trap_SetConfigstring(HINT_STRING_CONFIGSTRING_BASE + index, value);
            *outIndex = index;
            return 1;
        }

        if (strcmp(value, configString) == 0) {
            *outIndex = index;
            return 1;
        }
    }

    *outIndex = -1;
    return 0;
}

/* VERIFIED_DECOMPILER(0x6e801, 7e801_Scr_SetOrigin.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - vector argument, G_SetOrigin call, linked-state branch, and relink side effect checked against current decompiler output. */
void Scr_SetOrigin(gentity_t *ent)
{
    vec3_t origin;

    Scr_GetVector(0, origin);
    G_SetOrigin(ent, origin);
    if (ent->linkedState != 0) {
        trap_LinkEntity(ent);
    }
}

/* VERIFIED_DECOMPILER(0x6e855, 7e855_Scr_SetAngles.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - vector argument and G_SetAngle call checked against current decompiler output. */
void Scr_SetAngles(gentity_t *ent)
{
    vec3_t angles;

    Scr_GetVector(0, angles);
    G_SetAngle(ent, angles);
}

/* VERIFIED_DECOMPILER(0x6e892, 7e892_Scr_SetHealth.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - integer argument, non-client maxHealth/health stores, and client health/health stores checked against current decompiler output. */
void Scr_SetHealth(gentity_t *ent)
{
    int health = Scr_GetInt(0);

    if (ent->client == 0) {
        ent->maxHealth = health;
        ent->health = health;
    } else {
        ent->health = health;
        ent->client->ps.stats[STAT_HEALTH] = health;
    }
}

/* VERIFIED_DECOMPILER(0x6e8fd, 7e8fd_GScr_AddVector.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - null branch, Scr_AddUndefined fallback, and Scr_AddVector argument checked against current decompiler output. */
void GScr_AddVector(const float *value)
{
    if (value == 0) {
        Scr_AddUndefined();
    } else {
        Scr_AddVector(value);
    }
}

/* VERIFIED_DECOMPILER(0x6e92d, 7e92d_GScr_AddEntity.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - null branch, Scr_AddUndefined fallback, and Scr_AddEntity argument checked against current decompiler output. */
void GScr_AddEntity(gentity_t *ent)
{
    if (ent == 0) {
        Scr_AddUndefined();
    } else {
        Scr_AddEntity(ent);
    }
}

/* VERIFIED_DECOMPILER(0x68698, 78698_script_method_scriptbuiltin_useby.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - entity/user resolution, trigger notify argument order, use callback null branch, and callback arguments checked against current decompiler output. */
void ScrCmd_UseBy(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    gentity_t *user = Scr_GetEntity(0);

    Scr_AddEntity(user);
    Scr_Notify(ent, scr_const_trigger, 1);

    if (ent->use != 0) {
        ent->use(ent, user, user);
    }
}

/* VERIFIED_DECOMPILER(0x68727, 78727_script_method_scriptbuiltin_istouching.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - contents-based entity swap, abs bounds math, capsule/contact branch, decompiler-emitted OR bounds predicate, and Scr_AddInt bool result checked against current decompiler output. */
void ScrCmd_IsTouching(uint32_t scriptObject)
{
    gentity_t *boundsEnt = script_object_to_gentity(scriptObject);
    gentity_t *contactEnt;
    vec3_t mins;
    vec3_t maxs;
    qboolean touching = 0;

    if (boundsEnt->contents == 0) {
        contactEnt = Scr_GetEntity(0);
    } else {
        contactEnt = boundsEnt;
        boundsEnt = Scr_GetEntity(0);
    }

    game_compat_script_entity_add_abs_bounds(boundsEnt, mins, maxs);

    if (boundsEnt->contents == 0 || contactEnt->contents == 0) {
        if ((boundsEnt->svFlags & SVF_CAPSULE) != 0) {
            touching = trap_EntityContactCapsule(mins, maxs, contactEnt) != 0;
        } else {
            touching = trap_EntityContact(mins, maxs, contactEnt) != 0;
        }
    } else {
        touching = game_compat_script_entity_contents_bounds_touch(mins, maxs, contactEnt);
    }

    Scr_AddInt(touching ? 1 : 0);
}

/* VERIFIED_DECOMPILER(0x6897f, 7897f_script_method_scriptbuiltin_lockdoor.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - door/rotating-door classname predicate and locked store checked against current decompiler output. */
void ScrCmd_LockDoor(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    if (game_compat_script_entity_is_door(ent)) {
        ent->doorLocked = 1;
    }
}

/* VERIFIED_DECOMPILER(0x689e0, 789e0_script_method_scriptbuiltin_unlockdoor.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - door/rotating-door classname predicate and locked clear checked against current decompiler output. */
void ScrCmd_UnlockDoor(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    if (game_compat_script_entity_is_door(ent)) {
        ent->doorLocked = 0;
    }
}

/* VERIFIED_DECOMPILER(0x68a41, 78a41_script_method_scriptbuiltin_isdoorlocked.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - door/rotating-door predicate, locked test, and Scr_AddInt result checked against current decompiler output. */
void ScrCmd_IsDoorLocked(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    Scr_AddInt(game_compat_script_entity_is_door(ent) && ent->doorLocked != 0 ? 1 : 0);
}

/* VERIFIED_DECOMPILER(0x68abb, 78abb_script_method_scriptbuiltin_playsound.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - sound string lookup, level.time+300 soundTime store, and G_PlaySoundAlias argument order checked against current decompiler output. */
void ScrCmd_PlaySound(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    uint8_t soundAlias = G_SoundAliasIndex(Scr_GetString(0));

    ent->soundTime = level.time + SOUND_EVENT_DURATION_MSEC;
    G_PlaySoundAlias(ent, (uint8_t)soundAlias);
}

/* VERIFIED_DECOMPILER(0x68b2e, 78b2e_script_method_scriptbuiltin_playloopsound.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - sound string lookup, -1 soundTime store, byte-masked clientSound store checked against current decompiler output. */
void ScrCmd_PlayLoopSound(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    ent->soundTime = -1;
    ent->s.clientSound = G_SoundAliasIndex(Scr_GetString(0)) & 0xff;
}

/* VERIFIED_DECOMPILER(0x68b8d, 78b8d_script_method_scriptbuiltin_stoploopsound.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - level.time+300 soundTime store and clientSound clear checked against current decompiler output. */
void ScrCmd_StopLoopSound(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    ent->soundTime = level.time + SOUND_EVENT_DURATION_MSEC;
    ent->s.clientSound = 0;
}

/* VERIFIED_DECOMPILER(0x68bda, 78bda_script_method_scriptbuiltin_delete.c, VERIFY-P1-FREEPATH-2026-06-17): DATAFLOW_VERIFIED - entity validation helper, client rejection, add/notify/unlink, callback clears, G_FreeEntity think assignment, and level.time+100 scheduling checked against current decompiler output. */
void ScrCmd_Delete(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    if (ent->client != 0) {
        Scr_Error("Cannot delete a client entity");
    }

    game_compat_script_entity_schedule_death_free(ent);
}

/* VERIFIED_DECOMPILER(0x68c92, 78c92_script_method_scriptbuiltin_setmodel.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - model string argument, G_SetModel, G_DObjUpdate, and trap_LinkEntity sequence checked against current decompiler output. */
void ScrCmd_SetModel(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    G_SetModel(ent, Scr_GetString(0));
    G_DObjUpdate(ent);
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x68cef, 78cef_script_method_scriptbuiltin_getnormalhealth.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - non-client raw health, zero health branch, client normalMaxHealth divisor, and Scr_AddFloat results checked against current decompiler output. */
void ScrCmd_GetNormalHealth(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    if (ent->client == 0) {
        Scr_AddFloat((float)ent->health);
    } else if (ent->health == 0) {
        Scr_AddFloat(0.0f);
    } else {
        /* 0x68d2a..0x68d41: fild health, fild normalMaxHealth, fdivp, one
         * float rounding at the argument store — neither integer is rounded
         * to float before the divide. */
#if EMULATE_X87
        Scr_AddFloat(x87f_store_f32(x87f_div(
            x87f_load_i32(ent->health),
            x87f_load_i32(ent->client->normalMaxHealth))));
#else
        Scr_AddFloat((float)((long double)ent->health /
                             (long double)ent->client->normalMaxHealth));
#endif
    }
}

/* VERIFIED_DECOMPILER(0x68d70, 78d70_script_method_scriptbuiltin_setnormalhealth.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - float clamp, maxHealth/normalMaxHealth scaling, ROUND behavior, nonpositive Com_Printf path, and health store checked against current decompiler output. */
void ScrCmd_SetNormalHealth(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    float value = Scr_GetFloat(0);
    int health;

    if (value > 1.0f) {
        value = 1.0f;
    }

    /* fild maxHealth/normalMaxHealth * value(float), fistp-direct truncate. */
    if (ent->client == 0) {
        if (ent->maxHealth == 0) {
            health = game_compat_int32_from_float_trunc(value);
        } else {
#if EMULATE_X87
            health = x87f_store_i32_trunc(x87f_mul(
                x87f_load_i32(ent->maxHealth), x87f_load_f32(value)));
#else
            health = game_compat_int32_from_long_double_trunc(
                (long double)ent->maxHealth * (long double)value);
#endif
        }
    } else {
#if EMULATE_X87
        health = x87f_store_i32_trunc(x87f_mul(
            x87f_load_i32(ent->client->normalMaxHealth), x87f_load_f32(value)));
#else
        health = game_compat_int32_from_long_double_trunc(
            (long double)ent->client->normalMaxHealth * (long double)value);
#endif
    }

    if (health < 1) {
        Com_Printf("ERROR: Cannot setnormalhealth to 0 or below.\n");
    } else {
        ent->health = health;
    }
}

/* VERIFIED_DECOMPILER(0x68e67, 78e67_script_method_scriptbuiltin_dodamage.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - param-count error, damage/source parsing, client/non-client origin selection, normalized dir/null dir path, optional attacker/inflictor/mod parsing, ROUND damage, and G_Damage arguments checked against current decompiler output. */
void ScrCmd_DoDamage(uint32_t scriptObject)
{
    gentity_t *target;
    gentity_t *attacker;
    gentity_t *inflictor;
    float damageAmount;
    vec3_t source;
    vec3_t point;
    vec3_t dir;
    const float *dirPtr;
    int meansOfDeath = 0;

    if (Scr_GetNumParam() < 2) {
        Scr_Error("DoDamage damageAmount sourceVec [attacker] [inflictor] [mod]\n");
    }

    target = script_object_to_gentity(scriptObject);
    damageAmount = Scr_GetFloat(0);
    Scr_GetVector(1, source);

    point[0] = source[0];
    point[1] = source[1];
    point[2] = source[2];
    dirPtr = game_compat_script_entity_get_damage_dir(target, source, dir);

    attacker = Scr_GetNumParam() < 3 ? 0 : Scr_GetEntity(2);
    inflictor = Scr_GetNumParam() < 4 ? attacker : Scr_GetEntity(3);

    if (Scr_GetNumParam() > 4) {
        meansOfDeath = G_IndexForMeansOfDeath(Scr_GetString(4));
    }

    G_Damage(target, inflictor, attacker, dirPtr, point,
             game_compat_int32_from_float_trunc(damageAmount),
             0, meansOfDeath, 0);
}

/* VERIFIED_DECOMPILER(0x69042, 79042_script_method_scriptbuiltin_dodamagemod.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - param-count error, damage/source/mod parsing, client/non-client origin selection, normalized dir/null dir path, ROUND damage, and world-inflicted G_Damage arguments checked against current decompiler output. */
void ScrCmd_DoDamageMod(uint32_t scriptObject)
{
    gentity_t *target;
    float damageAmount;
    vec3_t source;
    vec3_t point;
    vec3_t dir;
    const float *dirPtr;
    int meansOfDeath;

    if (Scr_GetNumParam() < 3) {
        Scr_Error("DoDamageMod damageAmount sourceVec mod\n");
    }

    target = script_object_to_gentity(scriptObject);
    damageAmount = Scr_GetFloat(0);
    Scr_GetVector(1, source);

    point[0] = source[0];
    point[1] = source[1];
    point[2] = source[2];
    dirPtr = game_compat_script_entity_get_damage_dir(target, source, dir);
    meansOfDeath = G_IndexForMeansOfDeath(Scr_GetString(2));

    G_Damage(target, 0, 0, dirPtr, point,
             game_compat_int32_from_float_trunc(damageAmount),
             0, meansOfDeath, 0);
}

/* VERIFIED_DECOMPILER(0x691d2, 791d2_script_method_scriptbuiltin_settakedamage.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - exact one-parameter validation, bool argument, and takeDamage byte store checked against current decompiler output. */
void ScrCmd_SetTakeDamage(uint32_t scriptObject)
{
    gentity_t *ent;

    if (Scr_GetNumParam() != 1) {
        Scr_Error("SetTakeDamage true/false\n");
    }

    ent = script_object_to_gentity(scriptObject);
    ent->takeDamage = (uint8_t)Scr_GetBool(0);
}

/* VERIFIED_DECOMPILER(0x6922c, 7922c_script_method_scriptbuiltin_show.c, VERIFY-SCRIPT-ENTITY-WORLD-CONTENTS-2026-06-17): DATAFLOW_VERIFIED - entity resolution and +0x18c flag clear of ENTITY_FLAG_HIDDEN checked against current decompiler output. */
void ScrCmd_Show(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    ent->flags &= ~ENTITY_FLAG_HIDDEN;
}

/* VERIFIED_DECOMPILER(0x69259, 79259_script_method_scriptbuiltin_hide.c, VERIFY-SCRIPT-ENTITY-WORLD-CONTENTS-2026-06-17): DATAFLOW_VERIFIED - entity resolution and +0x18c flag set of ENTITY_FLAG_HIDDEN checked against current decompiler output. */
void ScrCmd_Hide(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    ent->flags |= ENTITY_FLAG_HIDDEN;
}

/* VERIFIED_DECOMPILER(0x69286, 79286_script_method_scriptbuiltin_unlinkfromworld.c, VERIFY-SCRIPT-ENTITY-WORLD-CONTENTS-2026-06-17): DATAFLOW_VERIFIED - entity resolution and trap_UnlinkEntity argument checked against current decompiler output. */
void ScrCmd_UnlinkFromWorld(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    trap_UnlinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x692b7, 792b7_script_method_scriptbuiltin_linkintoworld.c, VERIFY-SCRIPT-ENTITY-WORLD-CONTENTS-2026-06-17): DATAFLOW_VERIFIED - entity resolution and trap_LinkEntity argument checked against current decompiler output. */
void ScrCmd_LinkIntoWorld(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x692e8, 792e8_ScrCmd_VerifyPosition.c, VERIFY-SCRIPT-ENTITY-WORLD-CONTENTS-2026-06-17): DATAFLOW_VERIFIED - vehicle type branch, VEH_GetMinsMaxs scaling, 0x2800000 contents mask, optional VEH_InitPhysics, trace arguments, and allsolid/startsolid result path checked against current decompiler output. */
void ScrCmd_VerifyPosition(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    if (ent->s.eType == ET_VEHICLE) {
        int entityNumbers[VERIFYPOSITION_MAX_ENTITIES];
        vec3_t mins;
        vec3_t maxs;
        int count;

        VEH_GetMinsMaxs(ent, mins, maxs);

        /*
         * 0x6933b..0x693f4: all six components are scaled by 1.2f in place
         * (rounded to float), then the origin is added in place (rounded
         * again), then mins[2] alone gets the 32.0f bottom adjust as a third
         * separate rounding.
         */
        mins[0] *= VERIFYPOSITION_VEHICLE_BOUNDS_SCALE;
        mins[1] *= VERIFYPOSITION_VEHICLE_BOUNDS_SCALE;
        mins[2] *= VERIFYPOSITION_VEHICLE_BOUNDS_SCALE;
        maxs[0] *= VERIFYPOSITION_VEHICLE_BOUNDS_SCALE;
        maxs[1] *= VERIFYPOSITION_VEHICLE_BOUNDS_SCALE;
        maxs[2] *= VERIFYPOSITION_VEHICLE_BOUNDS_SCALE;
        mins[0] += ent->currentOrigin[0];
        mins[1] += ent->currentOrigin[1];
        mins[2] += ent->currentOrigin[2];
        maxs[0] += ent->currentOrigin[0];
        maxs[1] += ent->currentOrigin[1];
        maxs[2] += ent->currentOrigin[2];
        mins[2] -= VERIFYPOSITION_VEHICLE_BOTTOM_ADJUST;

        count = trap_EntitiesInBox(mins, maxs, entityNumbers,
                                   VERIFYPOSITION_MAX_ENTITIES,
                                   VERIFYPOSITION_CONTENTS_MASK);
        if (count == 0 ||
            (count == 1 && entityNumbers[0] == (int)scriptObject)) {
            if (Scr_GetNumParam() == 1 && Scr_GetInt(0) != 0) {
                VEH_InitPhysics(ent);
            }
            Scr_AddInt(1);
        } else {
            Scr_AddInt(0);
        }
    } else {
        trace_t trace;

        trap_Trace(&trace, ent->currentOrigin, ent->mins, ent->maxs,
                   ent->currentOrigin, ent->s.number, ent->clipmask);
        if (trace.allsolid == 0 && trace.startsolid == 0) {
            Scr_AddInt(1);
        } else {
            Scr_AddInt(0);
        }
    }
}

/* VERIFIED_DECOMPILER(0x6950e, 7950e_script_method_scriptbuiltin_clearvehicleposition.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - vehicle type gate, VEH_GetMinsMaxs origin offset, EntitiesInBox arguments, occupant resolution loop, death notify/unlink, callback clears, G_FreeEntity think, and level.time+100 scheduling checked against current decompiler output. */
void ScrCmd_ClearVehiclePosition(uint32_t scriptObject)
{
    gentity_t *vehicle = script_object_to_gentity(scriptObject);
    int entityNumbers[CLEARVEHICLEPOSITION_MAX_ENTITIES];
    vec3_t mins;
    vec3_t maxs;
    int count;
    int i;

    if (vehicle->s.eType != ET_VEHICLE) {
        return;
    }

    VEH_GetMinsMaxs(vehicle, mins, maxs);
    game_compat_script_entity_offset_bounds(mins, maxs, vehicle->currentOrigin);

    count = trap_EntitiesInBox(mins, maxs, entityNumbers,
                               CLEARVEHICLEPOSITION_MAX_ENTITIES,
                               CLEARVEHICLEPOSITION_CONTENTS_MASK);

    for (i = 0; i < count; i++) {
        gentity_t *ent = script_object_to_gentity((uint32_t)entityNumbers[i]);
        game_compat_script_entity_schedule_death_free(ent);
    }
}

/* VERIFIED_DECOMPILER(0x696b9, 796b9_script_method_scriptbuiltin_setcontents.c, VERIFY-SCRIPT-ENTITY-WORLD-CONTENTS-2026-06-17): DATAFLOW_VERIFIED - Scr_GetInt argument, prior +0x120 contents return, contents store, and relink side effect checked against current decompiler output. */
void ScrCmd_SetContents(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    int contents = Scr_GetInt(0);

    Scr_AddInt(ent->scriptContents);
    ent->scriptContents = contents;
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x69716, 79716_script_method_scriptbuiltin_setbounds.c, VERIFY-SCRIPT-ENTITY-WORLD-CONTENTS-2026-06-17): DATAFLOW_VERIFIED - Scr_GetVector calls to +0x108/+0x114 bounds and relink side effect checked against current decompiler output. */
void ScrCmd_SetBounds(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    Scr_GetVector(0, ent->mins);
    Scr_GetVector(1, ent->maxs);
    trap_LinkEntity(ent);
}

/* VERIFIED_DECOMPILER(0x69777, 79777_script_method_scriptbuiltin_setcursorhint.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - trigger_use HINT_INHERIT path, hintStrings scan bounds, case-insensitive match, diagnostic list printing, va error text, and cursorHint stores checked against current decompiler output. */
void GScr_SetCursorHint(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    const char *hint = Scr_GetString(0);
    int index;

    if (game_compat_script_entity_is_trigger_use(ent) &&
        Q_strcasecmp(hint, HINT_INHERIT_STRING) == 0) {
        ent->s.cursorHint = CURSOR_HINT_INHERIT;
    } else {
        for (index = CURSOR_HINT_NONE;
             index < CURSOR_HINT_TABLE_LIMIT && hintStrings[index] != 0;
             index++) {
            if (Q_strcasecmp(hint, hintStrings[index]) == 0) {
                ent->s.cursorHint = index;
                return;
            }
        }

        Com_Printf("List of valid hint type strings\n");
        if (game_compat_script_entity_is_trigger_use(ent)) {
            Com_Printf("HINT_INHERIT (for trigger_use entities only)\n");
        }

        for (index = CURSOR_HINT_NONE;
             index < CURSOR_HINT_TABLE_LIMIT && hintStrings[index] != 0;
             index++) {
            Com_Printf("%s\n", hintStrings[index]);
        }

        Scr_Error(va("%s is not a valid hint type. See above for list of valid hint types\n",
                     hint));
    }
}

/* VERIFIED_DECOMPILER(0x699b6, 799b6_script_method_scriptbuiltin_sethintstring.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - trigger_use validation, string-type HINT_INHERIT path, Scr_ConstructMessageString arguments, hintstring index lookup/error, and byte index store checked against current decompiler output. */
void GScr_SetHintString(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);
    char message[MAX_STRING_CHARS];
    int index;

    if (!game_compat_script_entity_is_trigger_use(ent)) {
        Scr_Error(SET_HINTSTRING_TRIGGER_USE_ERROR);
    }

    if (Scr_GetType(0) == SCRIPT_VAR_STRING &&
        Q_stricmp(Scr_GetString(0), HINT_INHERIT_STRING) == 0) {
        ent->s.hintStringIndex = HINTSTRING_INHERIT;
        return;
    }

    Scr_ConstructMessageString(0, message, MAX_STRING_CHARS,
                               SCRIPT_MESSAGE_MODE_HINT_STRING);

    if (!G_GetHintStringIndex(&index, message)) {
        Scr_Error(va(HINTSTRING_LIMIT_ERROR,
                     HINT_STRING_CONFIGSTRING_COUNT));
    }

    ent->s.hintStringIndex = index & 0xff;
}

/* VERIFIED_DECOMPILER(0x69ab9, 79ab9_script_method_scriptbuiltin_getentitynumber.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - entity resolution and state number Scr_AddInt checked against current decompiler output. */
void GScr_GetEntityNumber(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    Scr_AddInt(ent->s.number);
}

/* VERIFIED_DECOMPILER(0x69aec, 79aec_script_method_scriptbuiltin_getturretheat.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - misc_mg42/misc_turret validation, error text, turretState heat field, and Scr_AddFloat call checked against current decompiler output. */
void GScr_GetTurretHeat(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    game_compat_script_entity_require_turret(ent);
    Scr_AddFloat(ent->turretState->heat);
}

/* VERIFIED_DECOMPILER(0x69b60, 79b60_script_method_scriptbuiltin_getturretoverheating.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - misc_mg42/misc_turret validation, error text, turretState overheating field, and Scr_AddBool call checked against current decompiler output. */
void GScr_GetTurretOverheating(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    game_compat_script_entity_require_turret(ent);
    Scr_AddBool(ent->turretState->overheating);
}

/* VERIFIED_DECOMPILER(0x69bd4, 79bd4_script_method_scriptbuiltin_enablegrenadetouchdamage.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - trigger_damage validation, error text, and 0x8000 flag set checked against current decompiler output. */
void GScr_EnableGrenadeTouchDamage(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    game_compat_script_entity_require_damage_trigger(ent);
    ent->flags |= ENTITY_FLAG_GRENADE_TOUCH_DAMAGE;
}

/* VERIFIED_DECOMPILER(0x69c39, 79c39_script_method_scriptbuiltin_disablegrenadetouchdamage.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - trigger_damage validation, error text, and 0x8000 flag clear checked against current decompiler output. */
void GScr_DisableGrenadeTouchDamage(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    game_compat_script_entity_require_damage_trigger(ent);
    ent->flags &= ~ENTITY_FLAG_GRENADE_TOUCH_DAMAGE;
}

/* VERIFIED_DECOMPILER(0x69c9e, 79c9e_script_method_scriptbuiltin_enablegrenadebounce.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - entity resolution and 0x10000 no-grenade-bounce flag clear checked against current decompiler output. */
void GScr_EnableGrenadeBounce(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    ent->flags &= ~ENTITY_FLAG_NO_GRENADE_BOUNCE;
}

/* VERIFIED_DECOMPILER(0x69ccb, 79ccb_script_method_scriptbuiltin_disablegrenadebounce.c, VERIFY-SCRIPT-ENTITY-METHODS-PACKET-2026-06-17): DATAFLOW_VERIFIED - entity resolution and 0x10000 no-grenade-bounce flag set checked against current decompiler output. */
void GScr_DisableGrenadeBounce(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    ent->flags |= ENTITY_FLAG_NO_GRENADE_BOUNCE;
}
