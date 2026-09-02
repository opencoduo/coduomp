/*
 * Source reconstruction for trigger_damage system.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "scr_vm.h"
#include "game_functions.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#define TRIGGER_HURT_DEFAULT_SOUND "world_hurt_me"
#define TRIGGER_HURT_LIFE_KEY "life"
#define TRIGGER_HURT_DEFAULT_DAMAGE 5
#define TRIGGER_HURT_REPEAT_MS 100
#define TRIGGER_HURT_SLOW_REPEAT_MS 1000
#define TRIGGER_HURT_TIMER_THINK_MS 50
#define TRIGGER_HURT_DURATION_SCALE 1000.0f

#define TRIGGER_HURT_SPAWNFLAG_START_OFF 0x01u
#define TRIGGER_HURT_SPAWNFLAG_SILENT 0x04u
#define TRIGGER_HURT_SPAWNFLAG_NO_PROTECTION 0x08u
#define TRIGGER_HURT_SPAWNFLAG_SLOW 0x10u
#define TRIGGER_HURT_SPAWNFLAG_ONCE 0x20u

#define TRIGGER_DAMAGE_HEALTH_SENTINEL 32000
#define TRIGGER_DAMAGE_FREE_DELAY_MS 100
#define TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_PISTOL 0x01u
#define TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_RIFLE 0x02u
#define TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_GRENADE_PROJECTILE_ARTILLERY 0x04u
#define TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_EXPLOSIVE 0x08u
#define TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_SPLASH 0x10u
#define TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_MELEE 0x20u
#define TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_WORLD 0x100u
#define TRIGGER_DAMAGE_DEFAULT_WAIT "0.5"
#define TRIGGER_DAMAGE_DEFAULT_RANDOM "0"
#define TRIGGER_DAMAGE_GRENADE_TOUCH_FLAG 0x8000u
#define TRIGGER_DAMAGE_SCAN_MAX_ENTITIES 1024

#define TRIGGER_LOOKAT_CONTENTS 0x20000000u
#define TRIGGER_ENTITY_STATE_FLAG 0x00000002u
#define TRIGGER_MOUNT_SPAWNFLAG_LARGE 0x01u
#define TRIGGER_MOUNT_NO_BRUSH_BASE_CONTENTS 0x401c0008u
#define TRIGGER_MOUNT_POINT_EPSILON 0.1f
#define TRIGGER_MOUNT_CONTENTS 0x00004000u
#define TRIGGER_MOUNT_CONTENTS_LARGE 0x00400000u
#define EXPLOSIVE_INDICATOR_THINK_MS 100

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint16_t game_compat_trigger_damage_classname(const gentity_t *ent)
{
    return ent->scriptClassname;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint16_t game_compat_trigger_damage_grenade_touch_flags(const gentity_t *ent)
{
    return (uint16_t)ent->flags;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float *game_compat_trigger_hurt_end_time(gentity_t *ent)
{
    return &ent->itemWait;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static float *game_compat_trigger_hurt_duration_seconds(gentity_t *ent)
{
    return &ent->concussiveFxEndTime;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint8_t game_compat_trigger_hurt_toucher_active(const gentity_t *ent)
{
    return ent->takeDamage;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_trigger_hurt_set_toucher_active(gentity_t *ent, uint8_t active)
{
    ent->takeDamage = active;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int game_compat_trigger_damage_team(gentity_t *ent, int fallbackTeam)
{
    if (ent->client == NULL) {
        return fallbackTeam;
    }

    return ent->client->sessionTeam;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int game_compat_trigger_damage_trigger_team(gentity_t *ent)
{
    return game_compat_trigger_damage_team(ent, ent->droppedClipCount);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int game_compat_trigger_damage_min_damage(gentity_t *ent)
{
    return ent->doorLocked;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static int game_compat_trigger_damage_health_threshold(gentity_t *ent)
{
    return ent->itemCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static uint16_t game_compat_trigger_damage_team_string(gentity_t *ent)
{
    return (uint16_t)ent->teamName;
}

/* ------------------------------------------------------------------ */
/*  0x7672b  hurt_touch                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7672b, 8672b_hurt_touch.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - active toucher byte, repeat timers, sound alias point, no-protection damage flag, G_Damage argument order, MOD_TRIGGER_HURT, and once touch clear match current decompiler output. */
void hurt_touch(gentity_t *ent, gentity_t *other, int traceMode)
{
    int damageFlags;

    (void)traceMode;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (game_compat_trigger_hurt_toucher_active(other) == 0 || ent->dynaSinkEndTime > level.time) {
        return;
    }

    G_Trigger(ent, other);

    if ((ent->spawnflags & TRIGGER_HURT_SPAWNFLAG_SLOW) == 0) {
        ent->dynaSinkEndTime = level.time + TRIGGER_HURT_REPEAT_MS;
    } else {
        ent->dynaSinkEndTime = level.time + TRIGGER_HURT_SLOW_REPEAT_MS;
    }

    if ((ent->spawnflags & TRIGGER_HURT_SPAWNFLAG_SILENT) == 0) {
        G_PlaySoundAliasAtPoint(other->currentOrigin, ent->itemSoundAlias);
    }

    if ((ent->spawnflags & TRIGGER_HURT_SPAWNFLAG_NO_PROTECTION) == 0) {
        damageFlags = 0;
    } else {
        damageFlags = DAMAGE_NO_PROTECTION;
    }

    G_Damage(other, ent, ent, NULL, NULL, ent->damage, damageFlags, MOD_TRIGGER_HURT, 0);

    if ((ent->spawnflags & TRIGGER_HURT_SPAWNFLAG_ONCE) != 0) {
        ent->touch = NULL;
    }
}

/* ------------------------------------------------------------------ */
/*  0x7687f  hurt_think                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7687f, 8687f_hurt_think.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - nextthink reschedule, float end-time compare against level time, and free path match current decompiler output. */
void hurt_think(gentity_t *ent)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ent->nextthink = level.time + TRIGGER_HURT_REPEAT_MS;

    /* 0x768b2: fild level.time — the integer time is compared in extended
     * precision, never rounded to float. */
    if (*game_compat_trigger_hurt_end_time(ent) < level.time) {
        G_FreeEntity(ent);
    }
}

/* ------------------------------------------------------------------ */
/*  0x768da  hurt_use                                                 */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x768da, 868da_hurt_use.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - touch toggle, duration nonzero/NaN behavior, timer think install, and end-time float store match current decompiler output. */
void hurt_use(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)other;
    (void)activator;

    if (ent->touch == NULL) {
        ent->touch = hurt_touch;
    } else {
        ent->touch = NULL;
    }

    if (*game_compat_trigger_hurt_duration_seconds(ent) != 0.0f) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ent->nextthink = level.time + TRIGGER_HURT_TIMER_THINK_MS;
        ent->think = hurt_think;
        /* 0x7695f..0x76978: fild level.time joins the 80-bit chain directly;
         * the only rounding is the final store to the end-time slot. */
        *game_compat_trigger_hurt_end_time(ent) =
            *game_compat_trigger_hurt_duration_seconds(ent) * TRIGGER_HURT_DURATION_SCALE + level.time;
    }
}

/* ------------------------------------------------------------------ */
/*  0x76981  SP_trigger_hurt                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76981, 86981_SP_trigger_hurt.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - InitTrigger order, sound/default damage spawns, 0x405c0008 contents store, use/touch callbacks, start-off flag, and duration atof store match current decompiler output. */
void SP_trigger_hurt(gentity_t *ent)
{
    const char *soundName;
    const char *duration;

    InitTrigger(ent);

    G_SpawnString("sound", TRIGGER_HURT_DEFAULT_SOUND, &soundName);
    ent->itemSoundAlias = G_SoundAliasIndex(soundName);

    if (ent->damage == 0) {
        ent->damage = TRIGGER_HURT_DEFAULT_DAMAGE;
    }

    ent->scriptContents = MASK_TRIGGER;
    ent->use = hurt_use;

    if ((ent->spawnflags & TRIGGER_HURT_SPAWNFLAG_START_OFF) == 0) {
        ent->touch = hurt_touch;
    }

    G_SpawnString(TRIGGER_HURT_LIFE_KEY, "0", &duration);
    *game_compat_trigger_hurt_duration_seconds(ent) = (float)atof(duration);
}

/* ------------------------------------------------------------------ */
/*  0x76a66  SP_trigger_once                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76a66, 86a66_SP_trigger_once.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - itemWait -1.0, Touch_Multi/Use_Multi callbacks, InitTrigger, InitSentientTrigger, and link call match current decompiler output. */
void SP_trigger_once(gentity_t *ent)
{
    ent->itemWait = -1.0f;
    ent->touch = Touch_Multi;
    ent->use = Use_Multi;

    InitTrigger(ent);
    InitSentientTrigger(ent);
    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x76aca  Respond_trigger_damage                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76aca, 86aca_Respond_trigger_damage.c, VERIFY-NEXT-007-TRIGGER-DAMAGE-2026-06-17): DATAFLOW_VERIFIED - spawnflags MOD filters at +0x188, return values, and all pistol/rifle/explosive/splash/melee/world branches match current decompiler output. */
int Respond_trigger_damage(gentity_t *trigger, int mod)
{
    if ((trigger->spawnflags & TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_PISTOL) != 0 && mod == MOD_PISTOL_BULLET) {
        return 0;
    }

    if ((trigger->spawnflags & TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_RIFLE) != 0 && mod == MOD_RIFLE_BULLET) {
        return 0;
    }

    if ((trigger->spawnflags & TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_GRENADE_PROJECTILE_ARTILLERY) != 0) {
        switch (mod) {
        case MOD_GRENADE:
        case MOD_GRENADE_SPLASH:
        case MOD_PROJECTILE:
        case MOD_PROJECTILE_SPLASH:
        case MOD_ARTILLERY:
        case MOD_ARTILLERY_SPLASH:
            return 0;
        default:
            break;
        }
    }

    if ((trigger->spawnflags & TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_EXPLOSIVE) != 0) {
        switch (mod) {
        case MOD_GRENADE:
        case MOD_GRENADE_SPLASH:
        case MOD_PROJECTILE:
        case MOD_PROJECTILE_SPLASH:
        case MOD_MORTAR:
        case MOD_MORTAR_SPLASH:
        case MOD_DYNAMITE:
        case MOD_DYNAMITE_SPLASH:
        case MOD_ARTILLERY:
        case MOD_ARTILLERY_SPLASH:
        case MOD_EXPLOSIVE:
            return 0;
        default:
            break;
        }
    }

    if ((trigger->spawnflags & TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_SPLASH) != 0) {
        switch (mod) {
        case MOD_GRENADE_SPLASH:
        case MOD_PROJECTILE_SPLASH:
        case MOD_MORTAR_SPLASH:
        case MOD_DYNAMITE_SPLASH:
        case MOD_ARTILLERY_SPLASH:
            return 0;
        default:
            break;
        }
    }

    if ((trigger->spawnflags & TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_MELEE) != 0 && mod == MOD_MELEE) {
        return 0;
    }

    if ((trigger->spawnflags & TRIGGER_DAMAGE_SPAWNFLAG_IGNORE_WORLD) != 0) {
        switch (mod) {
        case MOD_UNKNOWN:
        case MOD_WATER:
        case MOD_CRUSH:
        case MOD_TELEFRAG:
        case MOD_FALLING:
        case MOD_SUICIDE:
        case MOD_TRIGGER_HURT:
            return 0;
        default:
            break;
        }
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/*  0x76c2f  Activate_trigger_damage                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76c2f, 86c2f_Activate_trigger_damage.c, VERIFY-NEXT-007-TRIGGER-DAMAGE-2026-06-17): DATAFLOW_VERIFIED - trigger/client team gates, nextthink/Think_GeneralLink gate, min/threshold checks, G_Trigger argument order, wait/random scheduling, free-delay path, and health sentinel store match current decompiler output. */
void Activate_trigger_damage(gentity_t *trigger, gentity_t *activator, int damage, int mod)
{
    int triggerTeam = game_compat_trigger_damage_trigger_team(trigger);
    int activatorTeam = game_compat_trigger_damage_team(activator, triggerTeam);

    if (((triggerTeam == activatorTeam || triggerTeam == 0 || activatorTeam == 0) &&
         (trigger->nextthink == 0 || trigger->think == Think_GeneralLink) &&
         (game_compat_trigger_damage_min_damage(trigger) < 1 || game_compat_trigger_damage_min_damage(trigger) <= damage) &&
         Respond_trigger_damage(trigger, mod) != 0 &&
         (game_compat_trigger_damage_health_threshold(trigger) == 0 ||
          game_compat_trigger_damage_health_threshold(trigger) <= TRIGGER_DAMAGE_HEALTH_SENTINEL - trigger->health))) {
        trigger->triggerActivator = activator;

        if (mod != -1) {
            G_Trigger(trigger, trigger->triggerActivator);
        }

        if (trigger->think != Think_GeneralLink) {
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (trigger->itemWait > 0.0f) {
                trigger->think = multi_wait;
                /* Stock 0x76dd0..0x76e14: (randSigned*itemRandom + itemWait) *
                 * 1000.0f(float const) kept 80-bit, fistp-direct truncate. */
#if EMULATE_X87
                trigger->nextthink =
                    level.time + x87f_store_i32_trunc(x87f_mul(
                                     x87f_add(x87f_mul(x87f_load_f64(coduo_server_rand_signed_unit()), x87f_load_f32(trigger->itemRandom)),
                                              x87f_load_f32(trigger->itemWait)),
                                     x87f_load_f32(1000.0f)));
#else
                trigger->nextthink = level.time + (int32_t)(((long double)trigger->itemWait + (long double)coduo_server_rand_signed_unit() *
                                                                                                  (long double)trigger->itemRandom) *
                                                            1000.0L);
#endif
            } else {
                trigger->touch = NULL;
                trigger->nextthink = level.time + TRIGGER_DAMAGE_FREE_DELAY_MS;
                trigger->think = G_FreeEntity;
            }
        }

        trigger->health = TRIGGER_DAMAGE_HEALTH_SENTINEL;
    }
}

/* ------------------------------------------------------------------ */
/*  0x76e77  Use_trigger_damage                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76e77, 86e77_Use_trigger_damage.c, VERIFY-NEXT-007-TRIGGER-DAMAGE-2026-06-17): DATAFLOW_VERIFIED - wrapper passes ent, other, itemCount + 1, and MOD -1 to Activate_trigger_damage as in current decompiler output. */
void Use_trigger_damage(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
    (void)activator;

    Activate_trigger_damage(ent, other, game_compat_trigger_damage_health_threshold(ent) + 1, -1);
}

/* ------------------------------------------------------------------ */
/*  0x76eb7  Pain_trigger_damage                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76eb7, 86eb7_Pain_trigger_damage.c, VERIFY-NEXT-007-TRIGGER-DAMAGE-2026-06-17): DATAFLOW_VERIFIED - pain wrapper argument order and zero-threshold health sentinel restore match current decompiler output. */
void Pain_trigger_damage(gentity_t *ent, gentity_t *attacker, int damage, const float *point, int mod, const float *dir, int hitLocation)
{
    (void)point;
    (void)dir;
    (void)hitLocation;

    Activate_trigger_damage(ent, attacker, damage, mod);

    if (game_compat_trigger_damage_health_threshold(ent) == 0) {
        ent->health = TRIGGER_DAMAGE_HEALTH_SENTINEL;
    }
}

/* ------------------------------------------------------------------ */
/*  0x76f08  Die_trigger_damage                                       */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76f08, 86f08_Die_trigger_damage.c, VERIFY-NEXT-007-TRIGGER-DAMAGE-2026-06-17): DATAFLOW_VERIFIED - death wrapper uses attacker/damage/mod and zero-threshold health sentinel restore exactly as current decompiler output. */
void Die_trigger_damage(gentity_t *ent, gentity_t *inflictor, gentity_t *attacker, int damage, int mod, int weapon, const float *dir,
                        int hitLocation)
{
    (void)inflictor;
    (void)weapon;
    (void)dir;
    (void)hitLocation;

    Activate_trigger_damage(ent, attacker, damage, mod);

    if (game_compat_trigger_damage_health_threshold(ent) == 0) {
        ent->health = TRIGGER_DAMAGE_HEALTH_SENTINEL;
    }
}

/* ------------------------------------------------------------------ */
/*  0x76f59  SP_trigger_damage                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x76f59, 86f59_SP_trigger_damage.c, VERIFY-NEXT-007-TRIGGER-DAMAGE-2026-06-17): DATAFLOW_VERIFIED - team token string conversion, wait/random defaults and clamp, accumulate/threshold stores, team parsing, takeDamage byte, callbacks, InitTrigger, and link call match current decompiler output. */
void SP_trigger_damage(gentity_t *ent)
{
    const char *teamString;

    teamString = SL_ConvertToString(game_compat_trigger_damage_team_string(ent));

    G_SpawnFloat("wait", TRIGGER_DAMAGE_DEFAULT_WAIT, &ent->itemWait);
    G_SpawnFloat("random", TRIGGER_DAMAGE_DEFAULT_RANDOM, &ent->itemRandom);

    if (ent->itemWait >= 0.0f && ent->itemWait <= ent->itemRandom) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ent->itemRandom = ent->itemWait - 100.0f;
        G_Printf("trigger_damage has random >= wait\n");
    }

    G_SpawnInt("accumulate", TRIGGER_DAMAGE_DEFAULT_RANDOM, &ent->itemCount);
    G_SpawnInt("threshold", TRIGGER_DAMAGE_DEFAULT_RANDOM, &ent->doorLocked);

    ent->droppedClipCount = TEAM_FREE;
    if (teamString != NULL) {
        if (strcasecmp(teamString, "axis") == 0) {
            ent->droppedClipCount = TEAM_AXIS;
        } else if (strcasecmp(teamString, "allies") == 0) {
            ent->droppedClipCount = TEAM_ALLIES;
        }
    }

    ent->health = TRIGGER_DAMAGE_HEALTH_SENTINEL;
    game_compat_trigger_hurt_set_toucher_active(ent, 1);
    ent->use = Use_trigger_damage;
    ent->pain = Pain_trigger_damage;
    ent->die = Die_trigger_damage;

    InitTrigger(ent);
    trap_LinkEntity(ent);
}

/* NOT_FROM_ORIGINAL_SOURCE: maintained source helper; no standalone original body. */
static void game_compat_trigger_damage_scan(gentity_t *activator, const float *start, const float *end, int damage, int mod,
                                            qboolean requireGrenadeTouchFlag)
{
    vec3_t mins;
    vec3_t maxs;
    int entityList[TRIGGER_DAMAGE_SCAN_MAX_ENTITIES];
    int entityCount;
    int i;

    mins[0] = start[0];
    mins[1] = start[1];
    mins[2] = start[2];
    maxs[0] = start[0];
    maxs[1] = start[1];
    maxs[2] = start[2];

    AddPointToBounds(end, mins, maxs);

    entityCount = trap_EntitiesInBox(mins, maxs, entityList, TRIGGER_DAMAGE_SCAN_MAX_ENTITIES, CONTENTS_TRIGGER_DAMAGE);

    for (i = 0; i < entityCount; i++) {
        gentity_t *trigger = &g_entities[entityList[i]];

        if (game_compat_trigger_damage_classname(trigger) != scr_const_trigger_damage) {
            continue;
        }

        if (requireGrenadeTouchFlag && (game_compat_trigger_damage_grenade_touch_flags(trigger) & TRIGGER_DAMAGE_GRENADE_TOUCH_FLAG) == 0) {
            continue;
        }

        if (trap_SightTraceToEntity(start, vec3_origin, vec3_origin, end, trigger->s.number, (int32_t)MASK_ALL) == 0) {
            continue;
        }

        Scr_AddEntity(activator);
        Scr_AddInt(damage);
        Scr_Notify(trigger, scr_const_damage, 2);
        Activate_trigger_damage(trigger, activator, damage, mod);

        if (game_compat_trigger_damage_health_threshold(trigger) == 0) {
            trigger->health = TRIGGER_DAMAGE_HEALTH_SENTINEL;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x77130  G_CheckHitTriggerDamage                                  */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x77130, 87130_G_CheckHitTriggerDamage.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - helper-extracted scan matches current decompiler: bounds setup, 0x400000 contents query, trigger_damage classname gate, sight trace args, notify args, Activate_trigger_damage args, and zero-threshold health reset. */
void G_CheckHitTriggerDamage(gentity_t *activator, const float *start, const float *end, int damage, int mod)
{
    game_compat_trigger_damage_scan(activator, start, end, damage, mod, qfalse);
}

/* ------------------------------------------------------------------ */
/*  0x773bf  G_GrenadeTouchTriggerDamage                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x773bf, 873bf_G_GrenadeTouchTriggerDamage.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - helper-extracted scan matches current decompiler, including grenade touch 0x8000 flag gate before sight trace and the same notify/activate/health-reset side effects. */
void G_GrenadeTouchTriggerDamage(gentity_t *grenade, const float *grenadePos, const float *explosionPos, int damage, int mod)
{
    game_compat_trigger_damage_scan(grenade, grenadePos, explosionPos, damage, mod, qtrue);
}

/* ------------------------------------------------------------------ */
/*  0x775b8  explosive_indicator_think                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x775b8, 875b8_explosive_indicator_think.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - passEntityNum owner lookup, linked/classname invalid free-think install, and unconditional 100 ms reschedule match current decompiler output. */
void explosive_indicator_think(gentity_t *ent)
{
    gentity_t *owner = &g_entities[ent->passEntityNum];

    if (owner->linked == 0 || game_compat_trigger_damage_classname(owner) != scr_const_trigger_objective_info) {
        ent->think = G_FreeEntity;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ent->nextthink = level.time + EXPLOSIVE_INDICATOR_THINK_MS;
}

/* ------------------------------------------------------------------ */
/*  0x77654  SP_trigger_lookat                                        */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x77654, 87654_SP_trigger_lookat.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - brush model call, lookat contents, svFlags store, s_flags bit set, and link call match current decompiler output. */
void SP_trigger_lookat(gentity_t *ent)
{
    trap_SetBrushModel(ent);
    ent->scriptContents = TRIGGER_LOOKAT_CONTENTS;
    ent->svFlags = SVF_NOCLIENT;
    ent->s.eFlags |= TRIGGER_ENTITY_STATE_FLAG;
    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x776ab  Touch_trigger_mount                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x776ab, 876ab_Touch_trigger_mount.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - wrapper calls G_Trigger with ent and other in decompiler order and has no additional side effects. */
void Touch_trigger_mount(gentity_t *ent, gentity_t *other, int traceMode)
{
    (void)traceMode;

    G_Trigger(ent, other);
}

/* ------------------------------------------------------------------ */
/*  0x776d5  SP_trigger_mount                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x776d5, 876d5_SP_trigger_mount.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - touch callback, InitTrigger order, spawnflag large branch, scriptContents OR masks, and link call match current decompiler output. */
void SP_trigger_mount(gentity_t *ent)
{
    ent->touch = Touch_trigger_mount;

    InitTrigger(ent);

    if ((ent->spawnflags & TRIGGER_MOUNT_SPAWNFLAG_LARGE) == 0) {
        ent->scriptContents |= TRIGGER_MOUNT_CONTENTS;
    } else {
        ent->scriptContents |= TRIGGER_MOUNT_CONTENTS_LARGE;
    }

    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x77754  SP_trigger_mount_no_brush                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x77754, 87754_SP_trigger_mount_no_brush.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - no-brush contents/svFlags/s_flags stores, largeTrigger spawnflag set/clear, contents OR masks, and link call match current decompiler output. */
void SP_trigger_mount_no_brush(gentity_t *ent, qboolean largeTrigger)
{
    ent->touch = Touch_trigger_mount;
    ent->scriptContents = TRIGGER_MOUNT_NO_BRUSH_BASE_CONTENTS;
    ent->svFlags = SVF_NOCLIENT;
    ent->s.eFlags |= TRIGGER_ENTITY_STATE_FLAG;

    if (!largeTrigger) {
        ent->spawnflags &= ~TRIGGER_MOUNT_SPAWNFLAG_LARGE;
        ent->scriptContents |= TRIGGER_MOUNT_CONTENTS;
    } else {
        ent->spawnflags |= TRIGGER_MOUNT_SPAWNFLAG_LARGE;
        ent->scriptContents |= TRIGGER_MOUNT_CONTENTS_LARGE;
    }

    trap_LinkEntity(ent);
}

/* ------------------------------------------------------------------ */
/*  0x7780f  G_CheckPointInsideTriggerMount                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7780f, 8780f_G_CheckPointInsideTriggerMount.c, VERIFY-TRIGGER-DAMAGE-REMAINING-2026-06-17): DATAFLOW_VERIFIED - point +/-0.1 bounds, 0x405c0008 query, trigger_mount classname scan, nullable large-trigger hint store, and 0/1 returns match current decompiler output. */
int G_CheckPointInsideTriggerMount(gentity_t *ent, const float *point, int *mountHintData)
{
    vec3_t mins;
    vec3_t maxs;
    int entityList[TRIGGER_DAMAGE_SCAN_MAX_ENTITIES];
    int entityCount;

    (void)ent;

    mins[0] = point[0] - TRIGGER_MOUNT_POINT_EPSILON;
    mins[1] = point[1] - TRIGGER_MOUNT_POINT_EPSILON;
    mins[2] = point[2] - TRIGGER_MOUNT_POINT_EPSILON;
    maxs[0] = point[0] + TRIGGER_MOUNT_POINT_EPSILON;
    maxs[1] = point[1] + TRIGGER_MOUNT_POINT_EPSILON;
    maxs[2] = point[2] + TRIGGER_MOUNT_POINT_EPSILON;

    entityCount = trap_EntitiesInBox(mins, maxs, entityList, TRIGGER_DAMAGE_SCAN_MAX_ENTITIES, MASK_TRIGGER);

    for (int i = 0; i < entityCount; i++) {
        gentity_t *trigger = &g_entities[entityList[i]];

        if (game_compat_trigger_damage_classname(trigger) != scr_const_trigger_mount) {
            continue;
        }

        if (mountHintData != NULL) {
            *mountHintData = (trigger->spawnflags & TRIGGER_MOUNT_SPAWNFLAG_LARGE) != 0;
        }

        return 1;
    }

    return 0;
}
