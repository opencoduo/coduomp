#include "cgame.h"

enum {
    CL_SNAPSHOT_ENTITY_WARNING_ENTRY = 8,
    CL_SNAPSHOT_ENTITY_WARNING_MSEC = 3000
};

/* Source: CoDUOMP.exe 0x00401240..0x00401253.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401240_00401254.mcode.
 * Name and signature: exact same-module Mac symbol
 * CL_GetCurrentSnapshotNumber. The Windows cgame syscall dispatcher also
 * inlines these two stores for CG_GET_CURRENT_SNAPSHOT_NUMBER. */
void CL_GetCurrentSnapshotNumber(int32_t *snapshotNumber, int32_t *serverTime)
{
    *snapshotNumber = cl.snap.messageNum;
    *serverTime = cl.snap.serverTime;
}

/* Source: CoDUOMP.exe 0x00401260..0x004013f2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401260_004013f2.mcode.
 * Name: exact same-module Mac symbol CL_GetSnapshot. The cgame syscall caller
 * at 0x00403413 proves snapshotNumber followed by the output pointer. */
qboolean CL_GetSnapshot(int32_t snapshotNumber, snapshot_t *snapshot)
{
    if (snapshotNumber > cl.snap.messageNum) {
        Com_Error(ERR_DROP, "\x15"
                            "CL_GetSnapshot: snapshotNumber > cl.snapshot.messageNum");
    }

    if ((int32_t)((uint32_t)cl.snap.messageNum - (uint32_t)snapshotNumber) >= CODUO_SNAPSHOT_BACKUP_COUNT) {
        return qfalse;
    }

    const clSnapshot_t *const source = &cl.snapshots[(uint32_t)snapshotNumber & (CODUO_SNAPSHOT_BACKUP_COUNT - 1)];
    if (source->valid == qfalse) {
        return qfalse;
    }

    if ((int32_t)((uint32_t)cl.parseEntitySequence - (uint32_t)source->firstEntitySequence) >= CODUO_PARSE_RING_COUNT) {
        return qfalse;
    }
    if ((int32_t)((uint32_t)cl.parseClientSequence - (uint32_t)source->firstClientSequence) >= CODUO_PARSE_RING_COUNT) {
        return qfalse;
    }

    snapshot->snapFlags = source->snapFlags;
    snapshot->serverCommandSequence = source->serverCommandSequence;
    snapshot->ping = source->ping;
    snapshot->serverTime = source->serverTime;
    snapshot->ps = source->ps;

    int32_t entityCount = source->numEntities;
    if (entityCount > MAX_ENTITIES_IN_SNAPSHOT) {
        if (com_statmon->integer != 0) {
            StatMon_Warning(CL_SNAPSHOT_ENTITY_WARNING_ENTRY, CL_SNAPSHOT_ENTITY_WARNING_MSEC, "gfx/2d/warning@snapshotents.jpg");
        } else {
            Com_DPrintf("CL_GetSnapshot: truncated %i entities to %i\n", entityCount, MAX_ENTITIES_IN_SNAPSHOT);
        }
        entityCount = MAX_ENTITIES_IN_SNAPSHOT;
    }

    snapshot->numEntities = entityCount;
    for (int32_t entityIndex = 0; entityIndex < entityCount; ++entityIndex) {
        const uint32_t parseIndex = ((uint32_t)source->firstEntitySequence + (uint32_t)entityIndex) & (CODUO_PARSE_RING_COUNT - 1);
        snapshot->entities[entityIndex] = cl.parseEntities[parseIndex];
    }

    int32_t clientCount = source->numClients;
    if (clientCount > MAX_CLIENTS_IN_SNAPSHOT)
        clientCount = MAX_CLIENTS_IN_SNAPSHOT;

    snapshot->numClients = clientCount;
    for (int32_t clientIndex = 0; clientIndex < clientCount; ++clientIndex) {
        const uint32_t parseIndex = ((uint32_t)source->firstClientSequence + (uint32_t)clientIndex) & (CODUO_PARSE_RING_COUNT - 1);
        snapshot->clients[clientIndex] = cl.parseClients[parseIndex];
    }

    return qtrue;
}
