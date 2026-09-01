#ifndef CODUOMP_SERVER_GAME_SYSCALL_SERVICES_H
#define CODUOMP_SERVER_GAME_SYSCALL_SERVICES_H

#include "client/engine/server/server.h"

#include "client/engine/animation/dobj.h"
#include "client/engine/animation/xanim_pool.h"
#include "client/engine/client/debug_lines.h"
#include "client/engine/math/vector_math.h"
#include "client/engine/networking/net_address.h"
#include "client/engine/physics/cm_trace.h"
#include "qcommon/q_string.h"
#include "sound/alias/sound_alias.h"
#include "client/engine/ui/ui_module_loader.h"
#include "animation/xanim_eval.h"

#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: bind the shared dispatcher to the listen-server
 * debug presentation that is present in CoDUOMP.exe and absent from the
 * dedicated engine. Keeping the raw syscall vector here prevents the
 * dedicated target from evaluating arguments its original handler ignored. */
static inline void server_compat_game_debug_string(
    const intptr_t *arguments)
{
    const uint32_t scaleBits = (uint32_t)arguments[3];
    float scale;

    memcpy(&scale, &scaleBits, sizeof(scale));
    CL_AddDebugString((const float *)arguments[1],
                      (const float *)arguments[2], scale,
                      (const char *)arguments[4], qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: second target binding for the same client-only
 * debug-presentation edge. */
static inline void server_compat_game_debug_line(
    const intptr_t *arguments)
{
    CL_AddDebugLine((const float *)arguments[1],
                    (const float *)arguments[2],
                    (const float *)arguments[3],
                    (qboolean)arguments[4], (int32_t)arguments[5], qtrue);
}

/* NOT_FROM_ORIGINAL_SOURCE: bind the common syscall IDs to the client
 * engine's renderer-owned surface table. */
static inline int32_t server_compat_surface_type_from_name(const char *name)
{
    return (int32_t)Com_SurfaceTypeFromName(name);
}

/* NOT_FROM_ORIGINAL_SOURCE: reverse lookup binding for the same renderer-owned
 * surface table. */
static inline const char *server_compat_surface_type_to_name(int32_t type)
{
    return Com_SurfaceTypeToName(type);
}

/* NOT_FROM_ORIGINAL_SOURCE: bind the common ownership protocol to the
 * client's aligned, reusable weapon-info allocation. */
static inline void *server_compat_get_weapon_info_memory(
    int32_t byteCount, int32_t *previousOwner, int32_t callerOwner)
{
    return Com_GetWeaponInfoMemory(byteCount, previousOwner, callerOwner);
}

/* NOT_FROM_ORIGINAL_SOURCE: release binding for the same client-owned weapon
 * allocation protocol. */
static inline void server_compat_free_weapon_info_memory(
    int32_t callerOwner, qboolean preserveAllocation)
{
    Com_FreeWeaponInfoMemory(callerOwner, preserveAllocation);
}

#endif
