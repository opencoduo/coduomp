// Source: uo_cgame_mp_x86.dll 0x30023690..0x3002388f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30023690_3002388f.mcode
//
// CG_EntityPreEvent -- the DObj / corpse-model analogue of CG_EntityEvent. It runs
// the client-side visual/audio side effects for one event id that has been queued on
// a client entity's embedded model sub-entity (cent->corpseModelInfo, the
// entityState_t at cent+0xf4). Called from CG_CheckPreEvents
// (0x300239e0) as it walks that model record's own event ring.
//
// NAME: proven by the function's own debug-trace strings --
//   "CG_EntityPreEvent:ZERO EVENT\n"                 (.rdata 0x30077358)
//   "ent:%3i  preevent:%3i CG_EntityPreEvent:%s\n"   (.rdata 0x3007732c)
// The mechanical pre-hint "CG_DrawField" (a pure size guess) is REJECTED: this body
// draws nothing -- it dispatches event effects.
//
// ABI (proven from the two call sites 0x30023a1b / 0x30023abd in
// CG_CheckPreEvents): the parent centity `cent` arrives in EAX (0x3002369d
// MOV EDI,EAX), `event` is the first caller-cleaned stack arg (0x30023695 reads
// [ESP+0x28] before the last two register pushes), and `eventParm` is the second
// stack arg (only read on the event-163..166 path at 0x30023810, [ESP+0x34] after the
// four register pushes). Callee ends in a plain RET; the caller cleans both slots.
//
// ESI throughout the body is `model = &cent->corpseModelInfo` (LEA ESI,[EDI+0xf4]);
// EDI+0x208 is `cent->lerpOrigin`, the world-origin/angles context handed to
// CG_AddCameraShake and the effect spawners.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>

/*
 * Event ids CG_EntityPreEvent decodes. The dispatch is a jump table over
 * `event - 0xA3` (index byte table at 0x300238b8, target table at 0x30023890), so
 * only ids 0xA3..0xC9 (163..201) reach a handler; every other id, and the grouped
 * ids that map to the default entry, are no-ops. The shared entityEvent_t names
 * come from the complete original event-name tables; the handler roles below
 * remain proven directly from the index/target tables. */

/* The four weapon-effect "modes" the target[4]/target[5] handlers pass as the last
 * arg of CG_FireWeapon. Role-named (exact meaning unproven). */
enum {
    MODEL_WEAPON_EFFECT_MODE_0 = 0,
    MODEL_WEAPON_EFFECT_MODE_1 = 1,
    MODEL_WEAPON_EFFECT_MODE_2 = 2,
    MODEL_WEAPON_EFFECT_MODE_3 = 3
};

/* Flag bit CG_FireWeapon looks for in its EAX flags word (it does
 * `and eax,0xffffff7f` after extracting bit 0x80). The target[3] handler passes it
 * set; the eventParm-driven handlers pass eventParm through unmasked. */
#define MODEL_WEAPON_EFFECT_FLAG_ALT ((uint32_t)0x00000080)

/* Camera-shake constants for the EV_FIRE_WEAPON_MG42 handler (target[3]).
 * Immediates 0x3d4ccccd / 0x64 / 0x42c80000 in the .mcode. */
#define MODEL_SHAKE_AMPLITUDE 0.05f  /* 0x3d4ccccd */
enum { MODEL_SHAKE_DURATION = 100 }; /* 0x64 */
#define MODEL_SHAKE_RADIUS    100.0f /* 0x42c80000 */

void CG_EntityPreEvent(centity_t *cent, int32_t event, int32_t eventParm)
{
    /* ESI: the model sub-entity (entityState_t) embedded at cent+0xf4. */
    entityState_t *model = &cent->corpseModelInfo;

    if (event == 0) {
        /* 0x300236a6: a queued event id of 0 is invalid. Only report it (once) when
         * cg_debugevents_vmCvar.integer is on, then return. */
        if (cg_debugevents_vmCvar.integer != 0) {
            Com_PrintMessage("CG_EntityPreEvent:ZERO EVENT\n");
        }
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if ((uint32_t)event >= (uint32_t)EV_MAX_EVENTS) {
        Com_Printf("WARNING: CG_EntityPreEvent: invalid event %i\n", event);
        return;
    }

    /* 0x300236c3: with debug tracing on, log the model entity number, the event id,
     * and the event's name. cg_eventNames is the CS-independent event-name table
     * (0x30082774). Reads model->number ([ESI]) as the "ent" number. */
    if (cg_debugevents_vmCvar.integer != 0) {
        Com_PrintMessage("ent:%3i  preevent:%3i CG_EntityPreEvent:%s\n",
                         (int)model->numberBits, (int)event,
                         cg_eventNames[event]);
    }

    /* 0x300236e7: dispatch. The disassembler's jump table only covers ids
     * 0xA3..0xC9; anything else falls straight through to the shared return. */
    switch (event) {

    /* target[0] (0x30023810): ids 163..166. Weapon effect with the event's parm as
     * the flags word and mode 0. eventParm is the second caller stack arg (read at
     * [ESP+0x34]). */
    case EV_FIRE_WEAPON:
    case EV_FIRE_WEAPONB:
    case EV_FIRE_WEAPONC:
    case EV_FIRE_WEAPON_LASTSHOT:
        CG_FireWeapon((uint32_t)eventParm, cent, model, event,
                                  MODEL_WEAPON_EFFECT_MODE_0);
        return;

    /* target[1] (0x30023829): id 167. Play the held weapon's model sound on this
     * entity. weaponIndex indexes cg_weaponInfos (stride 0x1c4); the sound name is
     * cgWeaponInfo.rechamberSound (+0xf0). A null sound name skips the call. Reads
     * model->number ([ESI]) as the sound entity number. */
    case EV_RECHAMBER_WEAPON: {
        const char *sound = (const char *)(uintptr_t)
            cg_weaponInfos[model->weaponIndex].rechamberSound;
        if (sound != 0) {
            CG_PlayEntitySoundAliasByName((int)model->numberBits, sound);
        }
        return;
    }

    /* target[2] (0x30023853): id 168. Eject weapon brass for the model record;
     * passes that entityState-shaped record in ECX and the event id on the stack. */
    case EV_EJECT_BRASS:
        CG_EjectWeaponBrass(model, event);
        return;

    /* target[3] (0x300237dc): id 173. Add a fixed-size camera shake at the entity's
     * weapon-origin context, then run a weapon effect with the alt flag set. */
    case EV_FIRE_WEAPON_MG42:
        CG_AddCameraShake(cent->lerpOrigin, MODEL_SHAKE_AMPLITUDE,
                          MODEL_SHAKE_DURATION, MODEL_SHAKE_RADIUS);
        CG_FireWeapon(MODEL_WEAPON_EFFECT_FLAG_ALT, cent, model, event,
                                  MODEL_WEAPON_EFFECT_MODE_0);
        return;

    /* target[4] (0x30023796): id 174. Weapon effect run twice, modes 0 then 1, with
     * a zero flags word. */
    case EV_FIRE_QUADBARREL_1:
        CG_FireWeapon(0u, cent, model, event, MODEL_WEAPON_EFFECT_MODE_0);
        CG_FireWeapon(0u, cent, model, event, MODEL_WEAPON_EFFECT_MODE_1);
        return;

    /* target[5] (0x300237b9): id 175. Weapon effect run twice, modes 2 then 3. */
    case EV_FIRE_QUADBARREL_2:
        CG_FireWeapon(0u, cent, model, event, MODEL_WEAPON_EFFECT_MODE_2);
        CG_FireWeapon(0u, cent, model, event, MODEL_WEAPON_EFFECT_MODE_3);
        return;

    /* target[6] (0x30023704): id 178. Spawn a bullet/tracer trail between two
     * byte-direction vectors expanded from model->eventParm and
     * model->scale. The low-3-bit effect selector comes from
     * model->legsAnim. */
    case EV_BULLET_HIT: {
        vec3_t dirA, dirB;
        int32_t effectSelect;

        ByteToDir((int32_t)model->eventParmBits, dirA); /* 0x3002370e */
        ByteToDir((int32_t)model->scaleBits, dirB);     /* 0x3002371d */

        /* 0x30023722: emitterParam0 ? (emitterParam0 & 7) : 0 */
        if (model->legsAnimWord != 0) {
            effectSelect = (int32_t)(model->legsAnimWord & 7u);
        } else {
            effectSelect = 0;
        }

        /* The model-event consumer validates the seven-bit weapon and eight-bit
         * surface selectors before its table lookups. */
        CG_BulletHitEvent((int32_t)model->vehicleEntityNumBits,
                          cent->lerpOrigin, dirA, dirB,
                          model->weaponIndex, model->surfTypeBits,
                          model->torsoAnimWord, effectSelect);
        return;
    }

    /* target[7] (0x3002376e): ids 179 and 180. Weapon-fire / tracer spawn. */
    case EV_BULLET_HIT_CLIENT_SMALL:
    case EV_BULLET_HIT_CLIENT_LARGE:
        /* The model-event consumer validates both selectors before lookup. */
        CG_ModelEventFireWeapon(event, model->surfTypeBits, model->weaponIndex,
                                cent->lerpOrigin,
                                (int32_t)model->vehicleEntityNumBits);
        return;

    /* target[8] (0x30023866): id 201. Camera shake whose amplitude/radius/duration
     * come from the model record. duration is model->earthquake.durationMs TRUNCATED to int
     * via _ftol2 (0x30023873 CALL 0x3006be3c, no round-to-nearest FISTP and no
     * FNSTCW/FLDCW dance), i.e. coduo_fp_to_i32_extended -- NOT a round; amplitude/radius
     * are read straight as floats. */
    case EV_EARTHQUAKE: {
        float radius = model->earthquake.radius;       /* 0x30023866, before FLD */
        long double duration = model->earthquake.durationMs;
        int32_t durationMsec = coduo_fp_to_i32_extended(duration);
        float amplitude = model->earthquake.scale; /* 0x30023878, after _ftol2 */
        CG_AddCameraShake(cent->lerpOrigin, amplitude, durationMsec, radius);
        return;
    }

    default:
        /* target[9] (0x30023887): every remaining id is a no-op. */
        return;
    }
}
