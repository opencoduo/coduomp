#include "dobj.h"

#include "qcommon/q_memory.h"
#include "qcommon/qcommon_runtime_types.h"

#include <stdint.h>

int32_t dobj_skelCacheKey;
int32_t dobjLastAllocatedIndex;
uint16_t serverDObjHandleByEntity[DOBJ_SERVER_HANDLE_COUNT];
uint8_t dobjAllocState[DOBJ_ALLOC_STATE_BYTES];
DObj dobjPool[DOBJ_POOL_COUNT];
uint16_t clientDObjHandleByEntity[DOBJ_CLIENT_HANDLE_COUNT];
qboolean comDObjInitialized;

enum {
    DOBJ_CLIENT_OWNERSHIP = 1,
    DOBJ_SERVER_OWNERSHIP = 2,
    DOBJ_ANY_OWNERSHIP = DOBJ_CLIENT_OWNERSHIP | DOBJ_SERVER_OWNERSHIP
};

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Complete common DObj pool subsystem.  Windows CoDUOMP.exe retains the
 * accessors at 0x0043cf50..0x0043cf8b and the allocator/lifecycle cluster at
 * 0x0043cf90..0x0043d293.  Linux coduo_lnxded retains the same ten operations
 * at 0x08072593..0x08072a35.  Its former primary/secondary reconstruction
 * names correspond exactly to the original client/server tables: 1152 client
 * handles, 1024 server handles, and two ownership bits per 1024 pool slots.
 */

/* NOT_FROM_ORIGINAL_SOURCE: typed factoring of the packed two-bit ownership
 * arithmetic repeated by the original DObj pool functions. */
static uint8_t coduomp_dobj_ownership_mask(int32_t slot, uint8_t ownership)
{
    return (uint8_t)(ownership << ((slot & 3) * 2));
}

/* NOT_FROM_ORIGINAL_SOURCE: typed factoring of the repeated packed-bit test. */
static qboolean coduomp_dobj_slot_is_owned(int32_t slot)
{
    return (dobjAllocState[slot >> 2] & coduomp_dobj_ownership_mask(slot, DOBJ_ANY_OWNERSHIP)) != 0 ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed factoring of the repeated packed-bit set. */
static void coduomp_dobj_set_ownership(int32_t slot, uint8_t ownership)
{
    dobjAllocState[slot >> 2] = (uint8_t)(dobjAllocState[slot >> 2] | coduomp_dobj_ownership_mask(slot, ownership));
}

/* NOT_FROM_ORIGINAL_SOURCE: typed factoring of the repeated packed-bit clear. */
static void coduomp_dobj_clear_ownership(int32_t slot, uint8_t ownership)
{
    dobjAllocState[slot >> 2] = (uint8_t)(dobjAllocState[slot >> 2] & (uint8_t)~coduomp_dobj_ownership_mask(slot, ownership));
}

void Com_InitDObj(void)
{
    Com_Memset(dobjAllocState, 0, sizeof(dobjAllocState));
    Com_Memset(clientDObjHandleByEntity, 0, sizeof(clientDObjHandleByEntity));
    Com_Memset(serverDObjHandleByEntity, 0, sizeof(serverDObjHandleByEntity));
    dobjLastAllocatedIndex = 1;
    comDObjInitialized = qtrue;
}

void Com_ShutdownDObj(void)
{
    if (comDObjInitialized != qfalse) {
        comDObjInitialized = qfalse;
    }
}

int32_t Com_GetFreeDObjIndex(void)
{
    int32_t slot;

    for (slot = dobjLastAllocatedIndex + 1; slot < DOBJ_POOL_COUNT; ++slot) {
        if (coduomp_dobj_slot_is_owned(slot) == qfalse) {
            dobjLastAllocatedIndex = slot;
            return slot;
        }
    }

    for (slot = 1; slot <= dobjLastAllocatedIndex; ++slot) {
        if (coduomp_dobj_slot_is_owned(slot) == qfalse) {
            dobjLastAllocatedIndex = slot;
            return slot;
        }
    }

    Com_Error(ERR_DROP, "\x15"
                        "No free DObjs");
    return 0;
}

void Com_ClientDObjCreate(const DObjModel *models, uint16_t modelCount, XAnimTree *runtimeTree, int32_t entityNum, uint16_t gameId)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)entityNum >= DOBJ_CLIENT_HANDLE_COUNT) {
        return;
    }

    const int32_t slot = Com_GetFreeDObjIndex();

    coduomp_dobj_set_ownership(slot, DOBJ_CLIENT_OWNERSHIP);
    clientDObjHandleByEntity[entityNum] = (uint16_t)slot;
    DObjCreate(models, modelCount, runtimeTree, &dobjPool[slot], gameId);
}

void Com_ServerDObjCreate(const DObjModel *models, uint16_t modelCount, XAnimTree *runtimeTree, int32_t entityNum, uint16_t gameId)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)entityNum >= DOBJ_SERVER_HANDLE_COUNT) {
        return;
    }

    const int32_t slot = Com_GetFreeDObjIndex();

    coduomp_dobj_set_ownership(slot, DOBJ_SERVER_OWNERSHIP);
    serverDObjHandleByEntity[entityNum] = (uint16_t)slot;
    DObjCreate(models, modelCount, runtimeTree, &dobjPool[slot], gameId);
}

void Com_SafeClientDObjFree(int32_t entityNum, qboolean releaseRuntimeTree)
{
    if ((uint32_t)entityNum >= DOBJ_CLIENT_HANDLE_COUNT) {
        return;
    }

    const int32_t slot = (int16_t)clientDObjHandleByEntity[entityNum];

    if (slot == 0) {
        return;
    }

    clientDObjHandleByEntity[entityNum] = 0;
    coduomp_dobj_clear_ownership(slot, DOBJ_CLIENT_OWNERSHIP);
    if (coduomp_dobj_slot_is_owned(slot) == qfalse) {
        DObjFree(&dobjPool[slot], releaseRuntimeTree);
    }
}

void Com_SafeServerDObjFree(int32_t entityNum, qboolean releaseRuntimeTree)
{
    if ((uint32_t)entityNum >= DOBJ_SERVER_HANDLE_COUNT) {
        return;
    }

    const int32_t slot = (int16_t)serverDObjHandleByEntity[entityNum];

    if (slot == 0) {
        return;
    }

    serverDObjHandleByEntity[entityNum] = 0;
    coduomp_dobj_clear_ownership(slot, DOBJ_SERVER_OWNERSHIP);
    if (coduomp_dobj_slot_is_owned(slot) == qfalse) {
        DObjFree(&dobjPool[slot], releaseRuntimeTree);
    }
}

void Com_MirrorServerDObjsToClient(void)
{
    for (int32_t entityNum = 0; entityNum < DOBJ_SERVER_HANDLE_COUNT; ++entityNum) {
        Com_SafeClientDObjFree(entityNum, qfalse);

        const int32_t slot = (int16_t)serverDObjHandleByEntity[entityNum];
        clientDObjHandleByEntity[entityNum] = (uint16_t)slot;
        if (slot != 0) {
            coduomp_dobj_set_ownership(slot, DOBJ_CLIENT_OWNERSHIP);
        }
    }
}

DObj *Com_GetClientDObj(int32_t entityNum)
{
    if ((uint32_t)entityNum >= DOBJ_CLIENT_HANDLE_COUNT) {
        return NULL;
    }

    const int16_t handle = (int16_t)clientDObjHandleByEntity[entityNum];
    return handle != 0 ? &dobjPool[handle] : NULL;
}

DObj *Com_GetServerDObj(int32_t entityNum)
{
    if ((uint32_t)entityNum >= DOBJ_SERVER_HANDLE_COUNT) {
        return NULL;
    }

    const int16_t handle = (int16_t)serverDObjHandleByEntity[entityNum];
    return handle != 0 ? &dobjPool[handle] : NULL;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(serverDObjHandleByEntity) == 0x800, "server DObj handle table size changed");
_Static_assert(sizeof(dobjAllocState) == 0x100, "packed DObj allocation-state size changed");
_Static_assert(sizeof(clientDObjHandleByEntity) == 0x900, "client DObj handle table size changed");
#endif
