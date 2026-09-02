// Source: uo_cgame_mp_x86.dll 0x30022810..0x30023562
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30022810_30023562.mcode
//
// CG_EntityEvent (0x30022810)
// -----------------------------------------------------------------------------
// Run the client-side visual/audio side effects for one entity event id on a
// centity/localEntity `self`. This is the cgame CG_EntityEvent dispatcher.
//
// NAME ADJUDICATION: the .mcode header's mechanical `# name Cmd_CallVote_f` is a
// SIZE-GUESS and is REJECTED. The body is proven CG_EntityEvent by its own
// diagnostic strings, which name the function directly:
//     "CG_EntityEvent:ZERO EVENT\n"        (0x300773fc)
//     "CG_EntityEvent:%s\n"                (0x300773d4)
//     "ent:%3i  event:%3i "                (0x300773e8)
//     "Event %s just for client %i was sent to other clients\n" (0x3007739c)
//     "Unknown event: '%s'"               (0x30077378)
// and confirmed by the same-module PPC bank (cgame_mp.dll::CG_EntityEvent). It is
// a big event-id range/jump-table dispatcher over per-entity sounds and effects,
// not a callvote console command.
//
// ABI (proven from the .mcode prologue and the caller CG_CheckPlayerstateEvents):
//   - self       in ECX  (MOV ESI,ECX)  : subject entity (centity_t *)
//   - event      in EAX  (MOV EDI,EAX)  : event id (already masked)
//   - predicted  on stack ([ESP+0x2c] past the 0x18+0x10 prologue) : a flag word
//   Ends in a plain RET; the single stack arg is caller-cleaned (cdecl with two
//   register-passed leading args). No calling-convention attribute is emitted.
//
// SOUND HELPERS (proven from their machine code and existing header decls):
//   - CG_PlayEntitySoundAliasByName(entNum, name)  (0x3002ca50) plays a registered sound
//     name using &cg_entities[entNum].currentStatePos.trBase (entity base 0x3048c6e0,
//     field +0x18, stride 0x288; absolute base 0x3048c6f8). The dispatcher
//     also INLINES this exact wrapper for the step legs (it computes
//     &cg_entities[self->currentState.number].currentStatePos.trBase, imul 0x288, and calls
//     CG_PlaySoundAliasByName (0x3002ca80) directly); those are expressed here as
//     CG_PlayEntitySoundAliasByName.
//   - CG_PlaySoundAliasByName(channelObj, name, entNum) (0x3002ca80) plays `name` on an
//     explicit channel object; the positional event legs pass &self->lerpOrigin as the
//     channel and the fixed entity number 0x3fe.
//
// GLOBALS: the event-indexed sound-name pools (cg_020/beb0/bf0c/bf68/bfc4 tables),
// the eventParm/weapon/surface tables, the per-weapon cgWeaponInfo_t sndEvent_*
// handles, the per-item registered-visuals cache (cg_items, itemInfo_t), and the
// 0x304879xx prediction/kick state are resolved by role in globals.h /
// client_recovered.h. The overlapping event-sound pools are modelled as separate
// arrays indexed by the absolute event id (matching `*(name **)(BASE + event*4)`);
// their true shared-pool origin/extent needs the sound-registration writer and is
// noted there, but the per-event reads are machine-faithful.

#include "../client_recovered.h"
#include "qcommon/fx_types.h"
#include "../globals.h"

#include <stdint.h>
#include <stddef.h>

/* The vmCvar_t handle at 0x30413100 that the taunt/voice events set via
 * trap_Cvar_SetValue. Kept as its mechanical 32-bit slot; treated as a vmCvar_t at
 * the call boundary. Exact cvar name unresolved. */

/* The event callees below now use their centrally declared recovered names.
 * Their non-default i386 register assignments remain recorded at the calls. */
/* 0x30047be0: eject the weapon's shell-casing effect on tag_brass. thiscall:
 * ECX=self, cdecl stack arg=eventId. Declared in client_recovered.h; reconstructed
 * in src/client/cgame/weapons/cg_ejectweaponbrass.c. */
/* 0x30047d20: EAX = regArg (eventParm for the 0xa3-0xa6/0xad leg, 0 for 0xae/0xaf);
 * stack args (self, self, event, subFlag). */

/* Mechanical globals referenced by role. cg_debugevents_vmCvar.integer gates the event trace;
 * cg_footsteps gates the surface step sounds; cg_clientNum is the local client
 * number; cg_eventDemoGate gates the voice-cvar paths; cg_eventNames[event] is the
 * event-name string table used by the trace/unknown diagnostics. The three
 * fire-mode gates block the local-player low-ammo prediction. Identities resolved
 * by role; kept address-shaped where the exact CoD cvar name is unproven. */
/* cg_debugevents_vmCvar.integer is now a resolved extern in globals.h (was the mechanical
 * g_data_cmd_callvote_f_30456f8c); no local alias needed. */
#define cg_footsteps       cg_footsteps_vmCvar.integer   /* nonzero: play step sounds */
#define cg_clientNum       cg_predictedPlayerState.psClientNum   /* local client number */
#define cg_eventDemoGate   cl_stanceTemp_vmCvar.integer   /* nonzero: suppress voice cvar */
#define cg_fireGateTurretA cg_nopredict_vmCvar.integer
#define cg_fireGateTurretB g_synchronousClients_vmCvar.integer

/* Two runtime floats (0x30539468, 0x30539bc8) read as (hi - lo) in the body-hit
 * envelope. Identity unresolved (multi-consumer, mislabeled owner); the globals
 * are float-typed in globals.h. */
#define cg_envHi bg_fallDamageMaxHeight.value
#define cg_envLo bg_fallDamageMinHeight.value

enum {
    EV_MAX_CLIENTS = 0x40,        /* clientNum clamp */

    /* half-open surface/impact sound ranges */
    EV_STEP_A_LO = 0x01, EV_STEP_A_HI = 0x18,
    EV_STEP_B_LO = 0x18, EV_STEP_B_HI = 0x2f,
    EV_STEP_C_LO = 0x2f, EV_STEP_C_HI = 0x46,
    EV_STEP_D_LO = 0x46, EV_STEP_D_HI = 0x5d,
    EV_FLESH_LO  = 0x5d, EV_FLESH_HI  = 0x74,
    EV_BODY_LO   = 0x74, EV_BODY_HI   = 0x8b,

    EV_TABLE_LO  = 0x8b, EV_TABLE_HI  = 0xd3,  /* jump-table window [0x8b, 0xd3) */

    KICK_LOOP_MAX = 0x18,          /* body-hit kick clamp */

    CS_WEAPON_SOUND_BASE = 0x295,  /* eventParm + 661 -> config string (event 0xb1) */
    LOCAL_SOUND_ENTITY   = 0x3fe,  /* fixed entityNum for positional plays */

    WEAPON_FIRE_FLAGS = 0xc0000,   /* cg_snap->ps.playerStateFlags bits gating fire tick */
    EFLAGS_PROJECTILE_HIT_VEHICLE = 0x100000,
    EFLAGS_STEP_ALT = 0xc,         /* self->currentState.eType bits selecting event 0xd0 variant */

    PROJECTILE_EXPLOSION_GRENADE = 0,
    PROJECTILE_EXPLOSION_SMOKE = 1,
    PROJECTILE_EXPLOSION_ROCKET = 2,
    PROJECTILE_EXPLOSION_MOLOTOV = 3,
    PROJECTILE_EXPLOSION_ARTILLERY = 4,
    PROJECTILE_EXPLOSION_MORTAR = 5,
    PROJECTILE_EXPLOSION_TANK = 6,
    PROJECTILE_EXPLOSION_B17 = 7,
    PROJECTILE_EXPLOSION_NONE = 8,
    PROJECTILE_EXPLOSION_SOUND_BANK_COUNT = PROJECTILE_EXPLOSION_NONE,
    CG_EVENT_SOUND_NONE = 8,
    CG_SURFACE_SOUND_COUNT =
        sizeof(cg_grenadeExplodeSurfaceSounds) /
        sizeof(cg_grenadeExplodeSurfaceSounds[0]),
    CG_SHELL_FLASH_SOUND_COUNT =
        sizeof(cg_shellFlashSounds) / sizeof(cg_shellFlashSounds[0]),
    CG_BARRAGE_SOUND_COUNT =
        sizeof(cg_barrageIncomingSounds) /
        sizeof(cg_barrageIncomingSounds[0]),
};

_Static_assert(CG_SURFACE_SOUND_COUNT ==
                   sizeof(cg_grenadeBounceSurfaceSounds) /
                       sizeof(cg_grenadeBounceSurfaceSounds[0]),
               "grenade surface sound rows differ in size");
_Static_assert(CG_SURFACE_SOUND_COUNT <= CG_IMPACT_SURFACE_TYPES,
               "surface sound domain exceeds impact effect domain");
_Static_assert(CG_SHELL_FLASH_SOUND_COUNT == CG_BARRAGE_SOUND_COUNT,
               "event parameter sound rows differ in size");
_Static_assert(CG_EVENT_SOUND_NONE == CG_SHELL_FLASH_SOUND_COUNT,
               "event sound sentinel no longer follows the sound rows");

/* NOT_FROM_ORIGINAL_SOURCE: the original i386 globals at
 * 0x3044bc30..0x3044bf10 are one contiguous
 * [projectileExplosionType][surfaceType] pointer table. Keep the separately
 * named recovered rows, but express their proven adjacency explicitly for host
 * builds where pointers are wider and the linker may place globals anywhere. */
static const char *(*const cg_projectileExplosionSurfaceSoundBanks
                    [PROJECTILE_EXPLOSION_SOUND_BANK_COUNT])
                   [CG_SURFACE_SOUND_COUNT] = {
    [PROJECTILE_EXPLOSION_GRENADE] = &cg_grenadeExplodeSurfaceSounds,
    [PROJECTILE_EXPLOSION_SMOKE] = &cg_unusedSurfaceSoundSet0,
    [PROJECTILE_EXPLOSION_ROCKET] = &cg_rocketExplodeSurfaceSounds,
    [PROJECTILE_EXPLOSION_MOLOTOV] = &cg_unusedSurfaceSoundSet1,
    [PROJECTILE_EXPLOSION_ARTILLERY] = &cg_artilleryExplodeSurfaceSounds,
    [PROJECTILE_EXPLOSION_MORTAR] = &cg_mortarExplodeSurfaceSounds,
    [PROJECTILE_EXPLOSION_TANK] = &cg_tankExplodeSurfaceSounds,
    [PROJECTILE_EXPLOSION_B17] = &cg_eventSurfaceSounds,
};

void CG_EntityEvent(centity_t *self, int event, int predicted)
{
    int eventParm;
    int clientNum;
    int entNum;

    /* --- ZERO EVENT trace ------------------------------------------------- */
    if (event == EV_NONE) {
        if (cg_debugevents_vmCvar.integer) {
            Com_PrintMessage("CG_EntityEvent:ZERO EVENT\n");
        }
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)event >= (uint32_t)EV_MAX_EVENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_EntityEvent: invalid event %i",
                  event);
        return;
    }

    eventParm = coduo_int32_from_bits(self->currentState.eventParm); /* [ESP+0x10] = self->currentState.eventParm */

    /* --- debug event trace ------------------------------------------------ */
    if (cg_debugevents_vmCvar.integer) {
        /* 0x3002285a..0x30022863: PUSH EDI(event); PUSH ECX(entityNum); PUSH fmt
         * — entityNum is the first vararg, event the second. */
        Com_PrintMessage("ent:%3i  event:%3i ", self->currentState.number, event);
        /* 0x30022868 reloads the cvar after the first print. Preserve that
         * second gate in case the callback changes the cached value. */
        if (cg_debugevents_vmCvar.integer) {
            const char *eventName = cg_eventNames[event];
            Com_PrintMessage("CG_EntityEvent:%s\n", eventName);
        }
    }

    /* clientNum = self->currentState.clientNum, clamped to [0, MAX_CLIENTS); else 0. */
    clientNum = self->currentState.clientNum;
    if (clientNum < 0 || clientNum >= EV_MAX_CLIENTS) {
        clientNum = 0;
    }

    entNum = self->currentState.number;

    /* ===================================================================== */
    /* surface / material step-and-impact sound ranges                        */
    /* Each leg: optionally play the surface table sound (gated by cg_footsteps */
    /* for the step ranges), then the fixed impact sound — both on entNum's     */
    /* per-client channel.                                                      */
    /* ===================================================================== */

    if (event >= EV_STEP_A_LO && event < EV_STEP_A_HI) {          /* [1, 0x18) */
        if (cg_footsteps) {
            CG_PlayEntitySoundAliasByName(entNum, (const char *)(intptr_t)
                                cg_stepRunSurfaceSounds[event - EV_STEP_A_LO]);
        }
        CG_PlayEntitySoundAliasByName(entNum, cg_soundGearRattleRun);
        return;
    }

    if (event >= EV_STEP_D_LO && event < EV_STEP_D_HI) {          /* [0x46, 0x5d) */
        if (cg_footsteps) {
            CG_PlayEntitySoundAliasByName(entNum, (const char *)(intptr_t)
                                cg_stepSprintSurfaceSounds[event - EV_STEP_D_LO]);
        }
        CG_PlayEntitySoundAliasByName(entNum, cg_soundGearRattleSprint);
        return;
    }

    if (event >= EV_STEP_B_LO && event < EV_STEP_B_HI) {          /* [0x18, 0x2f) */
        if (cg_footsteps) {
            CG_PlayEntitySoundAliasByName(entNum, (const char *)(intptr_t)
                                cg_stepWalkSurfaceSounds[event - EV_STEP_B_LO]);
        }
        CG_PlayEntitySoundAliasByName(entNum, cg_soundGearRattleWalk);
        return;
    }

    if (event >= EV_STEP_C_LO && event < EV_STEP_C_HI) {          /* [0x2f, 0x46) */
        if (cg_footsteps) {
            CG_PlayEntitySoundAliasByName(entNum, (const char *)(intptr_t)
                                cg_stepProneSurfaceSounds[event - EV_STEP_C_LO]);
        }
        CG_PlayEntitySoundAliasByName(entNum, cg_soundGearRattleWalk);
        return;
    }

    /* The compiler's range-dispatch tree contains a second [0x46, 0x5d) leg at
     * 0x300229aa that would play the 0x3044bf0c pool with impact 0x3044c1d8, but it
     * is UNREACHABLE: every event in [0x46, 0x5d) is consumed and returned by the
     * first D leg above (0x300228e8). The reference is preserved here in a comment
     * rather than as dead C. Its shifted base also resolves to
     * cg_stepRunSurfaceSounds[event - EV_STEP_D_LO]. */

    /* [0x5d, 0x74): bullet-flesh events. Play the flesh pool; then, only for the
     * local client, set the weapon-movement float state. */
    if (event >= EV_FLESH_LO && event < EV_FLESH_HI) {
        CG_PlayEntitySoundAliasByName(entNum, (const char *)(intptr_t)
                            cg_shellFlashSurfaceSounds[event - EV_FLESH_LO]);
        if (clientNum == (int)cg_clientNum) {
            cg_impactViewKickTime = coduo_int32_from_bits(cg_time);
            cg_impactViewKick = -(long double)eventParm; /* FILD raw; FCHS; FSTP (0x30022a0a..0x30022a1c): no pre-store round */
        }
        return;
    }

    /* [0x74, 0x8b): body-hit events. Play the body pool and the shared impact
     * sound 0x3044bbbc; then, for the local client, run the flesh envelope and
     * clamp its rounded result before stamping the kick state. */
    if (event >= EV_BODY_LO && event < EV_BODY_HI) {
        CG_PlayEntitySoundAliasByName(entNum, (const char *)(intptr_t)
                            cg_shellFlashSurfaceSounds[event - EV_BODY_LO]);
        CG_PlayEntitySoundAliasByName(entNum, cg_soundLandDamage);
        if (clientNum == (int)cg_clientNum) {
            /* envelope = (hi - lo) * eventParm * 0.01 + lo; FCOM against 12.0.
             * TEST AH,0x41 followed by JNP at 0x30022a97 skips the kick only for
             * less-than/equal; greater-than and unordered both continue. */
            /* eventParm enters via a raw FILD (0x30022a7a) and the whole chain --
             * the mul-add, the >12.0f FCOM (0x30022a8c) and the (x-12)*(1/26)+1)*4
             * remap -- runs in st0 with no float store until _ftol2 truncates it
             * (0x30022ab5). So envelope is long double (coduo_fp_to_i32_extended takes
             * long double) and eventParm is not (float)-cast. */
            long double envelope =
                ((long double)cg_envHi - (long double)cg_envLo) *
                    ((long double)eventParm * (long double)0.01f) +
                (long double)cg_envLo;
            /* TEST AH,0x41 / JNP skips only less-than and equal. An unordered
             * x87 result follows the remap/_ftol2 path alongside greater-than. */
            if (!(envelope <= (long double)12.0f)) {
                int n;
                /* (envelope - 12.0) * (1/26) + 1.0, then * 4.0; consts 0x3007c180,
                 * 0x3007bce0=1.0, 0x3007be40=4.0. */
                envelope =
                    ((envelope - (long double)12.0f) *
                         (long double)(1.0f / 26.0f) +
                     (long double)1.0f) *
                    (long double)4.0f;
                n = coduo_fp_to_i32_extended(envelope);
                if (n > KICK_LOOP_MAX) {
                    eventParm = KICK_LOOP_MAX;
                } else {
                    if (n <= 0) {
                        return;
                    }
                    eventParm = n;
                }
                cg_impactViewKickTime = coduo_int32_from_bits(cg_time);
                cg_impactViewKick = -(long double)eventParm; /* FILD raw; FCHS; FSTP (0x30022ad5..0x30022ae5): no pre-store round */
            }
        }
        return;
    }

    /* ===================================================================== */
    /* jump-table window [0x8b, 0xd3)                                          */
    /* ===================================================================== */

    if (event < EV_TABLE_LO || event >= EV_TABLE_HI) {
        /* 0x30022af9..0x30022afc JA -> 0x30023539: CALL 0x3002b300, the fatal
         * Com_ErrorMessage (CG_ERROR trap) — NOT a benign print. */
        Com_ErrorMessage("Unknown event: '%s'", cg_eventNames[event]);
        return;
    }

    switch (event) {

    case EV_FOLIAGE_SOUND:  /* 0x8b, 0x30022b09 */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundMovementFoliage);
        return;

    case EV_FATIGUE_SOUND:  /* 0x8d, 0x30022b23 */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundSprintBreathLast);
        return;

    case EV_FATIGUE_LAST_SOUND:  /* 0x8c, 0x30022b3c */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundFatigueBreath);
        return;

    /* 0x8e/0x8f/0x90 (0x30022b56/b92/bcd): taunt/voice. For a non-local client,
     * print the "just for client" DPrintf; otherwise, when the demo gate is clear,
     * set the voice cvar to "0"/"1"/"2" (pushes 0x30076cc8/0x30077398/0x30077394
     * at 0x30022b8b/0x30022bc6/0x30022c02). */
    case EV_STANCE_FORCE_STAND:
    case EV_STANCE_FORCE_CROUCH:
    case EV_STANCE_FORCE_PRONE: {
        const char *voice = (event == EV_STANCE_FORCE_STAND) ? "0"
                          : (event == EV_STANCE_FORCE_CROUCH) ? "1"
                                             : "2";
        if (clientNum != (int)cg_clientNum) {
            Com_DPrintf("Event %s just for client %i was sent to other clients\n",
                        cg_eventNames[event], clientNum);
            return;
        }
        if (cg_eventDemoGate != 0) {
            return;
        }
        trap_Cvar_SetValue(&cl_stance_vmCvar, voice);
        return;
    }

    /* 0x91 (0x30022c1f): weapon-changed / low-ammo warning. For a non-local client
     * this behaves as the 0x8e voice-diagnostic path (JNZ 0x30022b5e). */
    case EV_STEP_VIEW:
        if (clientNum != (int)cg_clientNum) {
            Com_DPrintf("Event %s just for client %i was sent to other clients\n",
                        cg_eventNames[event], clientNum);
            return;
        }
        if (cg_demoPlayback != 0 || cg_fireGateTurretA != 0 || cg_fireGateTurretB != 0) {
            return;
        }
        {
            int32_t now = coduo_int32_from_bits(cg_time);
            int32_t sinceLast = coduo_int32_from_bits(
                (uint32_t)now - (uint32_t)cg_weaponChangeViewOffsetTime);
            /* base is never stored: both legs leave it in st (the product at
             * 0x30022c72..0x30022c82, or FLD 0.0f at 0x30022c8a) and it is added to
             * the raw-FILD (eventParm-128) straight out of st1 (0x30022c9f), with the
             * single FSTP at 0x30022ca1. So base is long double and neither integer
             * operand is (float)-cast (both are raw FILD: 0x30022c72 / 0x30022c9b). */
            long double base;

            if (sinceLast < 100) {
                int32_t remaining = coduo_int32_from_bits(
                    100u - (uint32_t)sinceLast);
                base =
                    ((long double)remaining *
                         (long double)cg_weaponChangeViewOffset) *
                    (long double)0.01f * (long double)0.9f;
            } else {
                base = (long double)0.0f;
                /* 0x30022c8a: FLD [0x3007bcec] = 0.0f */
            }

            int32_t eventOffset = coduo_int32_from_bits(
                (uint32_t)eventParm - 128u);
            cg_weaponChangeViewOffset = (long double)eventOffset + base;

            /* FCOMP 24.0; TEST AH,0x41; JNZ -> the >24.0 fall-through clamps DOWN to
             * 24.0 and returns; when <= 24.0 the code proceeds to the -16.0 clamp. */
            if (cg_weaponChangeViewOffset > 24.0f) {
                cg_weaponChangeViewOffset = 24.0f;   /* 0x41c00000 */
                cg_weaponChangeViewOffsetTime = now;
                return;
            }
            /* FCOMP -16.0; TEST AH,0x5; JP -> clamp UP to -16.0 when below it. */
            if (cg_weaponChangeViewOffset < -16.0f) {
                cg_weaponChangeViewOffset = -16.0f;  /* 0xc1800000 */
            }
            cg_weaponChangeViewOffsetTime = now;
            return;
        }

    case EV_WATER_TOUCH:  /* 0x92, 0x30022cff */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundPlayerWaterIn);
        return;

    case EV_WATER_LEAVE:  /* 0x93, 0x30022d19 */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundPlayerWaterOut);
        return;

    /* 0x94/0x95/0x96 (0x30022d32): weapon-fire foley from the item-indexed
     * cg_items registered-visuals cache (itemInfo_t, stride 0x24) at +0x1c
     * (pickupSound, event 0x94) or +0x20 (pickupSoundAlt, event 0x96); then, if the
     * local player owns the event and a fire flag is set, tick the weapon model
     * change. */
    case EV_ITEM_PICKUP:
    case EV_AMMO_PICKUP: {
        int32_t wparm = coduo_int32_from_bits(self->currentState.eventParm);
        itemInfo_t *rec;
        if (wparm < 1 || wparm >= 0x86) {
            return;
        }
        rec = &cg_items[wparm];
        if (event == EV_ITEM_PICKUP) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, rec->pickupSound);
        } else {
            CG_PlayEntitySoundAliasByName(self->currentState.number, rec->pickupSoundAlt);
        }
        if ((cg_snap->ps.playerStateFlags & WEAPON_FIRE_FLAGS) != 0
            && (uint32_t)self->currentState.number == (uint32_t)cg_snap->ps.psClientNum) {
            CG_SetSelectedWeapon(wparm);
        }
        return;
    }

    /* 0x95 (0x30022d32 with the +0x24 index bounds ok but neither +0x1c nor +0x20
     * store path taken): only the fire-model tick, no foley (both JNZ arms skip). */
    case EV_ITEM_PICKUP_QUIET: {
        int32_t wparm = coduo_int32_from_bits(self->currentState.eventParm);
        if (wparm < 1 || wparm >= 0x86) {
            return;
        }
        if ((cg_snap->ps.playerStateFlags & WEAPON_FIRE_FLAGS) != 0
            && (uint32_t)self->currentState.number == (uint32_t)cg_snap->ps.psClientNum) {
            CG_SetSelectedWeapon(wparm);
        }
        return;
    }

    /* 0x99 (0x30022dae): play the weapon's reloadSound when set (the JNZ jumps
     * INTO the shared play tail at 0x300231ee with ECX = reloadSound; only the
     * 0xc8 entry at 0x300231e8 loads the 0x3044c264 shared handle); otherwise
     * fall back to reloadEmptySound when present. */
    case EV_RELOAD: {
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->reloadSound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->reloadSound);
            return;
        }
        if (w->reloadEmptySound != 0) {
                CG_PlayEntitySoundAliasByName(self->currentState.number, w->reloadEmptySound);
        }
        return;
    }

    /* 0x9a (0x30022dea): play reloadEmptySound when set; otherwise (0x30022e14)
     * fall back to reloadSound when present. */
    case EV_RELOAD_FROM_EMPTY: {
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->reloadEmptySound != 0) {
                CG_PlayEntitySoundAliasByName(self->currentState.number, w->reloadEmptySound);
                return;
        }
        if (w->reloadSound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->reloadSound);
        }
        return;
    }

    case EV_RELOAD_START: {  /* 0x9b, 0x30022e36 */
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->reloadStartSound != 0) {
                CG_PlayEntitySoundAliasByName(self->currentState.number, w->reloadStartSound);
        }
        return;
    }

    case EV_RELOAD_END: {  /* 0x9c, 0x30022e64 */
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->reloadEndSound != 0) {
                CG_PlayEntitySoundAliasByName(self->currentState.number, w->reloadEndSound);
        }
        return;
    }

    /* EV_NOAMMO (0x97) and EV_DROPWEAPON (0xcb) use different sound gates, then
     * share the local-player out-of-ammo-change tail described below. */
    /* 0x97 (0x30022e92) and 0xcb (0x30022eb7) share the use-check tail at 0x30022ed9.
     * 0x97: if the weapon descriptor has an alt-mode sound (desc+0x340 != 0) it
     * plays NO sound; otherwise it plays the shared cg_soundOutOfAmmo. 0xcb: plays
     * the weapon's +0x10c sound when present. Both then, for the local player with
     * a fire flag set, call CG_OutOfAmmoChange. */
    case EV_NOAMMO:
    case EV_DROPWEAPON: {
        if (event == EV_NOAMMO) {
            const int32_t weaponIndex = self->currentState.weapon;
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            if (weaponIndex <= 0 || weaponIndex > bg_numWeapons ||
                (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS ||
                bg_weaponInfos[weaponIndex] == NULL) {
                Com_Printf(
                    "WARNING: CG_EntityEvent: invalid weapon index %i "
                    "for event %i\n",
                    weaponIndex, event);
                return;
            }
            weaponInfo_t *weaponInfo = bg_weaponInfos[weaponIndex];
            if (weaponInfo->clipRequired == 0) {
                CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundOutOfAmmo);
            }
        } else {
            cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
            if (w->putawaySound != 0) {
                CG_PlayEntitySoundAliasByName(self->currentState.number, w->putawaySound);
            }
        }
        if ((cg_snap->ps.playerStateFlags & WEAPON_FIRE_FLAGS) != 0
            && (uint32_t)self->currentState.number == (uint32_t)cg_snap->ps.psClientNum) {
            CG_OutOfAmmoChange();
        }
        return;
    }

    case EV_RAISE_WEAPON: {  /* 0x9d, 0x30022f06 */
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->raiseSound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->raiseSound);
        }
        return;
    }

    case EV_PUTAWAY_WEAPON: {  /* 0x9e, 0x30022f34 */
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->putawaySound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->putawaySound);
        }
        return;
    }

    case EV_WEAPON_ALT: {  /* 0x9f, 0x30022f62 */
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->altSwitchSound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->altSwitchSound);
        }
        return;
    }

    case EV_DEPLOY_WEAPON: {  /* 0xa0, 0x30022f90 */
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->deploySound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->deploySound);
        }
        return;
    }

    case EV_BREAKDOWN_WEAPON: {  /* 0xa1, 0x30022fbe */
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->breakdownSound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->breakdownSound);
        }
        return;
    }

    /* 0xa2 (0x3002304a): play the weapon's +0xe0 sound. */
    case EV_PULLBACK_WEAPON: {
        cgWeaponInfo_t *w = &cg_weaponInfos[self->currentState.weapon];
        if (w->pullbackSound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->pullbackSound);
        }
        return;
    }

    /* 0xa3/0xa4/0xa5/0xa6/0xad (0x30023078): if predicted, play weapon foley set 0. */
    case EV_FIRE_WEAPON:
    case EV_FIRE_WEAPONB:
    case EV_FIRE_WEAPONC:
    case EV_FIRE_WEAPON_LASTSHOT:
    case EV_FIRE_WEAPON_MG42:
        if (predicted == 0) {
            return;
        }
        CG_FireWeapon((uint32_t)self->currentState.eventParm, self,
                      (entityState_t *)self, event, 0); /* EAX=eventParm */
        return;

    /* 0xae (0x30022fec): if predicted, play weapon foley sets 0 and 1. */
    case EV_FIRE_QUADBARREL_1:
        if (predicted == 0) {
            return;
        }
        CG_FireWeapon(0, self, (entityState_t *)self, event, 0);
        CG_FireWeapon(0, self, (entityState_t *)self, event, 1);
        return;

    /* 0xaf (0x3002301b): if predicted, play weapon foley sets 2 and 3. */
    case EV_FIRE_QUADBARREL_2:
        if (predicted == 0) {
            return;
        }
        CG_FireWeapon(0, self, (entityState_t *)self, event, 2);
        CG_FireWeapon(0, self, (entityState_t *)self, event, 3);
        return;

    /* 0xa7 (0x3002309d): only when predicted ([ESP+0x2c] gate at 0x300230a1),
     * play the weapon's rechamberSound. */
    case EV_RECHAMBER_WEAPON: {
        cgWeaponInfo_t *w;
        if (predicted == 0) {
            return;
        }
        w = &cg_weaponInfos[self->currentState.weapon];
        if (w->rechamberSound != 0) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, w->rechamberSound);
        }
        return;
    }

    /* 0xa8 (0x300230d7): if predicted, eject the weapon's brass for this event. */
    case EV_EJECT_BRASS:
        if (predicted == 0) {
            return;
        }
        CG_EjectWeaponBrass((entityState_t *)self, event);
        return;

    /* EV_MELEE_SWIPE (0xa9, 0x300230f6): dry-fire; if the weapon descriptor
     * (+0x31c) is unset AND its type (+0x7c) != 2, play 0x3044c1ec, else play
     * 0x3044c1e8. */
    case EV_MELEE_SWIPE: {
        const int32_t weaponIndex = self->currentState.weapon;
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (weaponIndex <= 0 || weaponIndex > bg_numWeapons ||
            (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS ||
            bg_weaponInfos[weaponIndex] == NULL) {
            Com_Printf(
                "WARNING: CG_EntityEvent: invalid weapon index %i "
                "for event %i\n",
                weaponIndex, event);
            return;
        }
        weaponInfo_t *weaponInfo = bg_weaponInfos[weaponIndex];
        if (weaponInfo->ricochet == 0 &&
            weaponInfo->weaponType != WEAPTYPE_PROJECTILE) {
            CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundMeleeSwingSmall);
            return;
        }
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundMeleeSwingLarge);
        return;
    }

    /* 0xab (0x30023148): resolve the event origin, then play 0x3044c1f0 on the
     * weapon's leg/torso sound channel (self->currentState.vehicleEntityNum). */
    case EV_MELEE_HIT: {
        vec3_t evOrigin;
        ByteToDir(coduo_int32_from_bits(self->currentState.eventParm), evOrigin);
        CG_PlayEntitySoundAliasByName((int)self->currentState.vehicleEntityNum, cg_soundMeleeHit);
        return;
    }

    /* 0xb0 (0x30023172): weapon bullet/impact effect between the two origins. */
    case EV_BULLET_TRACER:
        CG_SpawnTracerLine(self->currentState.effectEndOrigin, self->currentState.origin,
                           coduo_int32_from_bits(self->currentState.eventParm));
        return;

    /* 0xb1 (0x30023492): fire-loop config-string sound. index = eventParm + 0x295
     * -> CG_ConfigString; play it on the currentState.pos.trBase channel with
     * entNum. */
    case EV_SOUND_ALIAS: {
        int32_t configStringIndex = coduo_int32_from_bits(
            self->currentState.eventParm + (uint32_t)CS_WEAPON_SOUND_BASE);
        const char *name = CG_ConfigString(configStringIndex);
        CG_PlaySoundAliasByName(self->currentState.number, &self->currentState.origin, name);
        return;
    }

    /* EV_GRENADE_BOUNCE (0xb5, 0x3002321b): resolve the event origin, play the
     * muzzle sound (either 0x3044c200 or the per-weapon 0x3044bbd4 entry,
     * selected by the weapon descriptor +0x33c), then the per-weapon
     * muzzle-flash effect (0x3044c6f4). */
    case EV_GRENADE_BOUNCE: {
        vec3_t evOrigin;
        const char *muzzle;
        uint32_t flashHandle;
        const int32_t weaponIndex = self->currentState.weapon;
        const int32_t surfaceType = self->currentState.surfType;

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (weaponIndex <= 0 || weaponIndex > bg_numWeapons ||
            (uint32_t)weaponIndex >= (uint32_t)MAX_WEAPONS ||
            bg_weaponInfos[weaponIndex] == NULL) {
            Com_Printf(
                "WARNING: CG_EntityEvent: invalid weapon index %i "
                "for event %i\n",
                weaponIndex, event);
            return;
        }
        if ((uint32_t)surfaceType >= (uint32_t)CG_IMPACT_SURFACE_TYPES) {
            Com_Printf(
                "WARNING: CG_EntityEvent: invalid surface type %i "
                "for event %i\n",
                surfaceType, event);
            return;
        }

        ByteToDir(coduo_int32_from_bits(self->currentState.eventParm), evOrigin);
        weaponInfo_t *weaponInfo = bg_weaponInfos[weaponIndex];
        if (weaponInfo->cloth != 0) {
            muzzle = cg_soundSatchelBounce;
        } else {
            if ((uint32_t)surfaceType >=
                (uint32_t)CG_SURFACE_SOUND_COUNT) {
                Com_Printf(
                    "WARNING: CG_EntityEvent: invalid sound surface type %i "
                    "for event %i\n",
                    surfaceType, event);
                return;
            }
            muzzle = cg_grenadeBounceSurfaceSounds[surfaceType];
        }
        CG_PlaySoundAliasByName(LOCAL_SOUND_ENTITY, &self->lerpOrigin, muzzle);

        /* Row 12 of cg_impactEffects (0x3044c6f4 ==
         * &cg_impactEffects[12][0]), indexed by surfType. */
        flashHandle = (uint32_t)cgame_compat_read_target_i32_index(
            &cg_impactEffects[0][0],
            coduo_int32_from_bits(12u * CG_IMPACT_SURFACE_TYPES +
                             (uint32_t)surfaceType));
        if (flashHandle == 0) {
            return;
        }
        cgame_syscall(CG_PLAY_EFFECT_ORIENTED,
                      coduo_int32_from_bits(flashHandle),
                      (intptr_t)&self->lerpOrigin,
                      (intptr_t)evOrigin);
        return;
    }

    case EV_GRENADE_SPOON:  /* 0xb6, 0x30023298 */
        CG_PlaySoundAliasByName(LOCAL_SOUND_ENTITY, &self->lerpOrigin,
                                cg_soundUsGrenadeLever);
        return;

    /* 0xd0 (0x300232b4): flag-selected sound at &self->lerpOrigin. */
    case EV_OVERHEATING:
        if ((self->currentState.eType & EFLAGS_STEP_ALT) != 0) {
            CG_PlaySoundAliasByName(LOCAL_SOUND_ENTITY, &self->lerpOrigin,
                                    cg_soundMgOverheatVehicle);
            return;
        }
        CG_PlaySoundAliasByName(LOCAL_SOUND_ENTITY, &self->lerpOrigin,
                                cg_soundMgOverheat);
        return;

    /* EV_PROJECTILE_LAUNCH (0xb7, 0x300232eb): eventParm-indexed sound
     * 0x3044c194; skip when parm==8 or the handle is null. */
    case EV_PROJECTILE_LAUNCH: {
        int32_t p = coduo_int32_from_bits(self->currentState.eventParm);
        const char *name;
        if (p == CG_EVENT_SOUND_NONE) {
            return;
        }
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)p >= (uint32_t)CG_SHELL_FLASH_SOUND_COUNT) {
            Com_Printf(
                "WARNING: CG_EntityEvent: invalid sound selector %i "
                "for event %i\n",
                p, event);
            return;
        }
        name = cg_shellFlashSounds[p];
        if (name == NULL) {
            return;
        }
        CG_PlaySoundAliasByName(LOCAL_SOUND_ENTITY, &self->lerpOrigin, name);
        return;
    }

    /* EV_PROJECTILE_INCOMING (0xb8, 0x30023320): eventParm-indexed sound
     * 0x3044c1b4; skip when parm==8 or the handle is null. */
    case EV_PROJECTILE_INCOMING: {
        int32_t p = coduo_int32_from_bits(self->currentState.eventParm);
        const char *name;
        if (p == CG_EVENT_SOUND_NONE) {
            return;
        }
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)p >= (uint32_t)CG_BARRAGE_SOUND_COUNT) {
            Com_Printf(
                "WARNING: CG_EntityEvent: invalid sound selector %i "
                "for event %i\n",
                p, event);
            return;
        }
        name = cg_barrageIncomingSounds[p];
        if (name == NULL) {
            return;
        }
        CG_PlaySoundAliasByName(LOCAL_SOUND_ENTITY, &self->lerpOrigin, name);
        return;
    }

    /* EV_PROJECTILE_EXPLODE / EV_PROJECTILE_EXPLODE_NOMARKS
     * (0xb9/0xba, 0x30023355): resolve the event origin, play the
     * explosion-type/surface-selected effect (unless explosionType is "none"),
     * then its surface sound, then the per-weapon-model gate effect and
     * impact-name sound. */
    case EV_PROJECTILE_EXPLODE:
    case EV_PROJECTILE_EXPLODE_NOMARKS: {
        vec3_t evOrigin;
        int explosionType;
        int surfaceType;
        cgWeaponInfo_t *w;

        ByteToDir(coduo_int32_from_bits(self->currentState.eventParm), evOrigin);
        explosionType = self->currentState.scale;
        surfaceType = self->currentState.surfType;

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)explosionType >
            (uint32_t)PROJECTILE_EXPLOSION_NONE) {
            Com_Printf(
                "WARNING: CG_EntityEvent: invalid explosion type %i "
                "for event %i\n",
                explosionType, event);
            return;
        }
        if (explosionType != PROJECTILE_EXPLOSION_NONE &&
            (uint32_t)surfaceType >= (uint32_t)CG_SURFACE_SOUND_COUNT) {
            Com_Printf(
                "WARNING: CG_EntityEvent: invalid surface type %i "
                "for event %i\n",
                surfaceType, event);
            return;
        }

        if (explosionType == PROJECTILE_EXPLOSION_SMOKE) {
            /* Smoke explosion: resolve the "g_body" tag on this object; on success
             * play the surface-specific smoke effect there. The 0xe9 trap's
             * trailing argument is an 8-byte { entityNum, boneIndex } record at
             * [ESP+0x14]..[ESP+0x1b].
             * 0x30023385 (MOV [ESP+0x20],EAX during the trap-0xe3 pushes)
             * writes self->currentState.number to its first word; 0x30023394 writes the
             * CG_RESOLVE_TAG result to the adjacent second word. */
            sfx_bolt_info_t boltInfo;
            boltInfo.entityNum = self->currentState.number;
            boltInfo.boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_RESOLVE_TAG, self->currentState.number, (intptr_t)"g_body"));
            if (boltInfo.boneIndex >= 0) {
                /* Row 14 of cg_impactEffects
                 * (0x3044c7b4 == &cg_impactEffects[14][0]) is the
                 * smoke_grenade_explode effect family, indexed by surface type. */
                uint32_t handle =
                    (uint32_t)cgame_compat_read_target_i32_index(
                        &cg_impactEffects[0][0],
                        coduo_int32_from_bits(
                            14u * CG_IMPACT_SURFACE_TYPES +
                            (uint32_t)surfaceType));
                cgame_syscall(CG_PLAY_EFFECT_ON_TAG,
                              coduo_int32_from_bits(handle),
                              (intptr_t)&self->lerpOrigin,
                              0,
                              (intptr_t)&boltInfo);
            }
        } else if (explosionType != PROJECTILE_EXPLOSION_NONE) {
            int handled = 0;
            if ((self->currentState.eFlags & EFLAGS_PROJECTILE_HIT_VEHICLE) != 0) {
                /* 0x3044c7b0 == &cg_impactEffects[13][23]; the flat
                 * explosionType*24 walk selects column 23 (the vehicle surface)
                 * in the explosion-type effect row. */
                uint32_t flatIndex =
                    13u * CG_IMPACT_SURFACE_TYPES + 23u +
                    (uint32_t)explosionType * CG_IMPACT_SURFACE_TYPES;
                uint32_t handle =
                    (uint32_t)cgame_compat_read_target_i32_index(
                        &cg_impactEffects[0][0],
                        coduo_int32_from_bits(flatIndex));
                if (handle != 0) {
                    cgame_syscall(CG_PLAY_EFFECT_ORIENTED,
                                  coduo_int32_from_bits(handle),
                                  (intptr_t)&self->lerpOrigin,
                                  (intptr_t)&evOrigin);
                    handled = 1;
                }
            }
            if (!handled) {
                /* 0x3044c754 == &cg_impactEffects[13][0]; the flat
                 * surfaceType+explosionType*24 index selects the ordinary surface
                 * column in the explosion-type effect row. */
                uint32_t flatIndex =
                    13u * CG_IMPACT_SURFACE_TYPES +
                    (uint32_t)explosionType * CG_IMPACT_SURFACE_TYPES +
                    (uint32_t)surfaceType;
                uint32_t handle =
                    (uint32_t)cgame_compat_read_target_i32_index(
                        &cg_impactEffects[0][0],
                        coduo_int32_from_bits(flatIndex));
                if (handle != 0) {
                    cgame_syscall(CG_PLAY_EFFECT_ORIENTED,
                                  coduo_int32_from_bits(handle),
                                  (intptr_t)&self->lerpOrigin,
                                  (intptr_t)&evOrigin);
                }
            }
        }

        /* Explosion foley sound (skipped for projectileExplosionType "none").
         * 0x3002340c..0x30023417 proves the flattened
         * [explosionType][surfaceType] access with a 23-pointer row stride. */
        if (explosionType != PROJECTILE_EXPLOSION_NONE) {
            const char *const *soundBank =
                *cg_projectileExplosionSurfaceSoundBanks[explosionType];
            const char *foley = soundBank[surfaceType];
            CG_PlaySoundAliasByName(LOCAL_SOUND_ENTITY, &self->lerpOrigin, foley);
        }

        /* per-weapon-model gate effect + impact name sound. */
        w = &cg_weaponInfos[self->currentState.weapon];
        if (w->projectileExplosionEffect != 0) {
            cgame_syscall(CG_PLAY_EFFECT_ORIENTED,
                          coduo_int32_from_bits(w->projectileExplosionEffect),
                          (intptr_t)&self->lerpOrigin,
                          (intptr_t)&evOrigin);
        }
        {
            cgWeaponInfo_t *w2 = &cg_weaponInfos[self->currentState.weapon];
            const char *nm =
                w2->projectileExplosionSound;
            if (nm == NULL || nm[0] == '\0') {
                return;
            }
            CG_PlaySoundAliasByName(LOCAL_SOUND_ENTITY, &self->lerpOrigin, nm);
        }
        return;
    }

    /* 0xbd (0x30023202): blood/missile impact for the companion entity. */
    case EV_RAILTRAIL:
        CG_RailTrail(self->currentState.cursorHint, self->currentState.origin, self->currentState.effectEndOrigin);
        return;

    case EV_DEATH:  /* 0xc1, 0x300234bc */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundDeath);
        return;

    case EV_DEBUG_LINE:  /* 0xc2, 0x300234e6 */
        CG_AddLightningBeam(self);
        return;

    case EV_PLAY_FX:  /* 0xc3, 0x300234f5 */
        CG_PlayFx(self, NULL);
        return;

    case EV_PLAY_FX_DIR: {  /* 0xc4, 0x30023506 */
        vec3_t evOrigin;
        ByteToDir(coduo_int32_from_bits(self->currentState.scale), evOrigin);
        /* 0x30023515: ECX still holds &evOrigin — ByteToDir (0x30049400)
         * provably preserves ECX — so the decoded direction IS passed; only
         * the 0xc3 leg (0x300234f5) zeroes ECX for a NULL direction. */
        CG_PlayFx(self, evOrigin);
        return;
    }

    case EV_PLAY_FX_ON_TAG:  /* 0xc5, 0x30023524 */
        CG_PlayFxOnTag(self, coduo_int32_from_bits(self->currentState.eventParm));
        return;

    case EV_FLAMEBARREL_BOUNCE:  /* 0xc8, 0x300231e8 */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundFlameBarrelBounce);
        return;

    /* 0xcc (0x300231c2): stamp cg.time into self->miscTime, then play 0x3044bbcc. */
    case EV_ITEM_RESPAWN:
        self->miscTime = coduo_int32_from_bits(cg_time);
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundItemRespawn);
        return;

    case EV_ITEM_POP:  /* 0xcd, 0x300231ce */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundItemRespawn);
        return;

    case EV_PLAYER_TELEPORT_IN:  /* 0xce, 0x3002318f */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundPlayerTeleportIn);
        return;

    case EV_PLAYER_TELEPORT_OUT:  /* 0xcf, 0x300231a9 */
        CG_PlayEntitySoundAliasByName(self->currentState.number, cg_soundPlayerTeleportOut);
        return;

    case EV_OBITUARY:  /* 0xd1, 0x300234d5 */
        CG_Obituary(self);
        return;

    /* 0xbb/0xbc/0xbe/0xc6/0xc7: in-window jump-table slots wired to the
     * unknown-event error leg (0x30023539), same fatal Com_ErrorMessage as the
     * out-of-window path. */
    case EV_MOLOTOV_EXPLODE:
    case EV_MOLOTOV_EXPLODE_NOMARKS:
    case EV_BULLET:
    case EV_PLAY_FX_ON_PLAYER:
    case EV_STOP_ATTACHED_FX:
        Com_ErrorMessage("Unknown event: '%s'", cg_eventNames[event]);
        return;

    /* 0x98, 0xaa, 0xac, 0xb2, 0xb3, 0xb4, 0xbf, 0xc0, 0xc9, 0xca, 0xd2
     * (0x3002355a): no-op events (jump-table slots at the shared epilogue). */
    default:
        return;
    }
}
