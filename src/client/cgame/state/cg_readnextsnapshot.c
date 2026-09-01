// Source: uo_cgame_mp_x86.dll 0x3003d220..0x3003d2cb
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003d220_3003d2cb.mcode
//
// CG_ReadNextSnapshot — read the next available engine snapshot into the free one
// of the two double-buffered cg_activeSnapshots slots and return it, or NULL when
// no newer snapshot is (yet) available.
//
// Name evidence: the function pushes the warning string 0x3007a484
// "WARNING: CG_ReadNextSnapshot: way out of range, %i > %i\n" (globals.mcode:1892)
// to Com_PrintMessage, which names the function directly. Its caller
// CG_ProcessSnapshots (0x3003d2d0) references the sibling string
// "CG_ProcessSnapshots: n < cg.latestSnapshotNum". The mechanical size-matched
// name "KeywordHash_Key" is REJECTED: this reads snapshots, not a hash key.
//
// The two data buffers 0x30459168 / 0x3046e188 (mechanically owner=keywordhash_key)
// are the cg_activeSnapshots[2] double buffer; 0x30447ab4 (owner=g_runframe) is
// cg.processedSnapshotNum; 0x30459158 is cg.latestSnapshotNum; the ring at
// 0x305380a4/0x305382a4/0x305384a4 (owner=vector4scale) is the lagometer snapshot
// ring cg_lagometer. All resolved by machine-code producer/consumer evidence and
// renamed at their globals.h/globals.c definitions.
//
// ABI: standard cdecl, caller-cleaned. ESI is callee-saved (PUSH ESI / POP ESI);
// modelled as ordinary source-level control flow. Signed compares (JLE/JGE/JL) on
// processedSnapshotNum / latestSnapshotNum are preserved as signed int math.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* trap-count guard the "way out of range" warning fires on: cgame prints when the
 * engine's latest snapshot is more than this many ahead of what cgame has processed
 * (CMP latestSnapshotNum, processedSnapshotNum + 0x3e8). */
enum { CG_SNAPSHOT_RANGE_WARN = 1000 };

/* The cgame trap dispatcher (VM syscall pointer at 0x30085e9c) is declared in
 * globals.h. */

snapshot_t *CG_ReadNextSnapshot(void)
{
    /* 0x3003d220 / 0x3003d225: snapshot the two counters into locals. */
    int32_t processed = cg_processedSnapshotNum;      /* [0x30447ab4] */
    int32_t latest = cg_latestSnapshotNum;            /* [0x30459158] */

    /* 0x3003d22b..0x3003d24c: warn if the engine has run far ahead of us.
     * CMP latestSnapshotNum, processedSnapshotNum+0x3e8 ; JLE skips the warning,
     * so the warning fires when latest > processed + 1000. */
    int32_t warningLimit = coduo_int32_from_bits(
        (uint32_t)processed + (uint32_t)CG_SNAPSHOT_RANGE_WARN);
    if (latest > warningLimit) {
        Com_PrintMessage("WARNING: CG_ReadNextSnapshot: way out of range, %i > %i\n",
                         latest, processed);
        /* 0x3003d241/0x3003d247: reload both counters after the call. */
        latest = cg_latestSnapshotNum;
        processed = cg_processedSnapshotNum;
    }

    /* 0x3003d24f/0x3003d252: if we have already processed everything the engine has,
     * there is no next snapshot — return NULL. Signed CMP EAX,ECX ; JGE. */
    while (processed < latest) {
        /* 0x3003d260..0x3003d276: pick the free double-buffer slot — the one NOT
         * currently installed as cg_snap.
         *   CMP [cg_snap], &cg_activeSnapshots0 ; JZ keeps ESI = &cg_activeSnapshots1. */
        snapshot_t *dest = (cg_snap == &cg_activeSnapshots0)
                                 ? &cg_activeSnapshots1
                                 : &cg_activeSnapshots0;

        /* 0x3003d276/0x3003d27b: ++processedSnapshotNum, written back to the global. */
        processed = coduo_int32_from_bits((uint32_t)processed + 1u);
        cg_processedSnapshotNum = processed;

        /* 0x3003d277..0x3003d286: trap_GetSnapshot(processedSnapshotNum, dest).
         * Three dwords pushed (id, number, dest), caller-cleaned (ADD ESP,0xc). */
        int32_t got = (int32_t)cgame_syscall(CG_GET_SNAPSHOT, processed,
                                    (intptr_t)dest);

        /* 0x3003d289/0x3003d28b: nonzero => the snapshot was delivered. */
        if (got != 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            if ((uint32_t)dest->ps.psClientNum >=
                (uint32_t)MAX_CLIENTS) {
                Com_Error(
                    ERR_DROP,
                    "\x15" "CG_ReadNextSnapshot: "
                    "invalid player client number %i",
                    dest->ps.psClientNum);
                return NULL;
            }

            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            {
                const int32_t currentWeapon = dest->ps.currentWeapon;
                if ((uint32_t)currentWeapon >= (uint32_t)MAX_WEAPONS ||
                    currentWeapon > bg_numWeapons ||
                    (currentWeapon != 0 &&
                     bg_weaponInfos[currentWeapon] == NULL)) {
                    Com_Error(
                        ERR_DROP,
                        "\x15" "CG_ReadNextSnapshot: "
                        "invalid player weapon %i",
                        currentWeapon);
                    return NULL;
                }
            }
            for (int32_t eventSlot = 0;
                 eventSlot < MAX_PS_EVENTS;
                 ++eventSlot) {
                if ((uint32_t)dest->ps.events[eventSlot] >=
                    (uint32_t)EV_MAX_EVENTS) {
                    Com_Error(
                        ERR_DROP,
                        "\x15" "CG_ReadNextSnapshot: "
                        "invalid player event %i",
                        dest->ps.events[eventSlot]);
                    return NULL;
                }
            }
            for (int32_t entityIndex = 0;
                 entityIndex < dest->numEntities;
                 ++entityIndex) {
                const entityState_t *const entity =
                    &dest->entities[entityIndex];
                if ((entity->eType == ET_PLAYER ||
                     entity->eType == ET_PLAYER_CORPSE) &&
                    (uint32_t)entity->clientNum >=
                        (uint32_t)MAX_CLIENTS) {
                    Com_Error(
                        ERR_DROP,
                        "\x15" "CG_ReadNextSnapshot: "
                        "invalid entity client number %i",
                        entity->clientNum);
                    return NULL;
                }

                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                if (entity->eType == ET_PLAYER ||
                    entity->eType == ET_PLAYER_CORPSE) {
                    const int32_t weapon = entity->weapon;
                    if ((uint32_t)weapon >= (uint32_t)MAX_WEAPONS ||
                        weapon > bg_numWeapons ||
                        (weapon != 0 && bg_weaponInfos[weapon] == NULL)) {
                        Com_Error(ERR_DROP,
                                  "\x15" "CG_ReadNextSnapshot: invalid entity weapon %i",
                                  weapon);
                        return NULL;
                    }
                }

                if (entity->eType > ET_EVENTS &&
                    (uint32_t)(entity->eType - ET_EVENTS) >=
                        (uint32_t)EV_MAX_EVENTS) {
                    Com_Error(
                        ERR_DROP,
                        "\x15" "CG_ReadNextSnapshot: "
                        "invalid event entity type %i",
                        entity->eType);
                    return NULL;
                }
                for (int32_t eventSlot = 0;
                     eventSlot < MAX_ENTITY_EVENTS;
                     ++eventSlot) {
                    if ((uint32_t)entity->events[eventSlot] >=
                        (uint32_t)EV_MAX_EVENTS) {
                        Com_Error(
                            ERR_DROP,
                            "\x15" "CG_ReadNextSnapshot: "
                            "invalid entity event %i",
                            entity->events[eventSlot]);
                        return NULL;
                    }
                }
            }

            /* 0x3003d2c0..0x3003d2ca: log it in the lagometer and return the slot. */
            CG_AddLagometerSnapshotInfo(dest);   /* CALL 0x30018a40 with EAX=dest */
            return dest;
        }

        /* 0x3003d28d..0x3003d2ba: the read failed (snapshot dropped). This is the
         * NULL branch of CG_AddLagometerSnapshotInfo inlined:
         *   cg_lagometer.snapshotSamples[snapshotCount & 0x7f] = -1;
         *   ++cg_lagometer.snapshotCount;
         * Then reload processed/latest and continue the loop while processed < latest
         * (JL 0x3003d260). The index mask is LAG_SAMPLES-1 (128-entry ring). */
        int32_t snapshotCount = cg_lagometer.snapshotCount; /* 0x3003d28d */
        latest = cg_latestSnapshotNum;                    /* 0x3003d292 reload */
        cg_lagometer.snapshotSamples[
            (uint32_t)snapshotCount & (uint32_t)(LAG_SAMPLES - 1)] = -1;
        int32_t nextSnapshotCount = coduo_int32_from_bits(
            (uint32_t)cg_lagometer.snapshotCount + 1u);    /* 0x3003d2a6/0x3003d2b1 */
        processed = cg_processedSnapshotNum;              /* 0x3003d2ac reload */
        cg_lagometer.snapshotCount = nextSnapshotCount;    /* 0x3003d2b4 store */
    }

    /* 0x3003d2bc: XOR EAX,EAX ; RET — no next snapshot available. */
    return (snapshot_t *)0;
}
