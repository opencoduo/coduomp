#ifndef CODUO_SERVER_GAME_SYSCALL_SERVICES_H
#define CODUO_SERVER_GAME_SYSCALL_SERVICES_H

#include "coduo_engine_structs.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"
#include "collision/collision_server_entity.h"
#include "collision/collision_server_trace.h"
#include "qcommon/com_time.h"
#include "../core_memory/core_memory_private.h"
#include "animation/dobj.h"
#include "math/q_math.h"
#include "server/engine/server_configstrings.h"
#include "server/engine/server_client_message.h"
#include "server/engine/server_dobj.h"
#include "server/engine/server_game_data.h"
#include "server/engine/server_game_bridge.h"
#include "server/engine/server_game_hunk.h"
#include "server/engine/server_game_lifecycle.h"
#include "server/engine/server_snapshot_archive.h"
#include "sound/alias/sound_alias.h"
#include "../server/surface_types.h"
#include "animation/xanim.h"
#include "animation/xanim_eval.h"
#include "animation/xmodel.h"

#include <stddef.h>
#include <stdint.h>

extern cvar_t *sv_maxclients;
extern serverStatic_t svs;
extern char *sv_entityParsePoint;

void Com_Printf(const char *format, ...);
void Com_Error(errorParm_t code, const char *format, ...);
int32_t Sys_Milliseconds(void);
void Cvar_Register(vmCvar_t *vmCvar, const char *name, const char *value,
                   uint32_t flags);
void Cvar_Update(vmCvar_t *vmCvar);
void Cvar_Set(const char *name, const char *value);
int32_t Cvar_VariableIntegerValue(const char *name);
float Cvar_VariableValue(const char *name);
void Cvar_VariableStringBuffer(const char *name, char *buffer,
                               int32_t bufferLength);
int32_t FS_FOpenFileByMode(const char *path, int32_t *handleOut,
                           fsMode_t mode);
int32_t FS_Read(void *buffer, int32_t length, int32_t handle);
int32_t FS_Write(const void *buffer, int32_t length, int32_t handle);
void FS_Rename(const char *from, const char *to);
void FS_FCloseFile(int32_t handle);
void Cbuf_ExecuteText(cbufExec_t execWhen, const char *text);
qboolean NET_IsLocalAddress(netadr_t address);
void CM_BoxTrace(trace_t *trace, const vec3_t start, const vec3_t end,
                 const vec3_t mins, const vec3_t maxs, int32_t model,
                 int32_t brushMask, qboolean capsule);
int32_t CM_BoxSightTrace(int32_t result, const vec3_t start,
                         const vec3_t end, const vec3_t mins,
                         const vec3_t maxs, int32_t model,
                         int32_t brushMask, qboolean capsule);
int32_t CM_AreaEntities(const vec3_t mins, const vec3_t maxs,
                        int32_t *entityList, int32_t maxCount,
                        int32_t contentMask);
char *Com_Parse(char **data);
void Q_strncpyz(char *destination, const char *source,
                int32_t destinationSize);
int32_t FS_GetFileList(const char *path, const char *extension,
                       char *listBuffer, int32_t bufferSize);
void Sys_SnapVector(vec3_t vector);
void *Z_MallocInternal(size_t size);
void Z_FreeInternal(void *memory);
XAnim *Scr_GetAnims(uint32_t treeIndex);
void SV_XModelDebugBoxes(int32_t entityNum);
void SV_FreeClientScriptPers(void);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right,
                  vec3_t up);
void PerpendicularVector(vec3_t destination, const vec3_t source);
/* NOT_FROM_ORIGINAL_SOURCE: the dedicated handlers return zero without
 * inspecting their debug-draw arguments. */
static inline void server_compat_game_debug_string(
    const intptr_t *arguments)
{
    (void)arguments;
}

/* NOT_FROM_ORIGINAL_SOURCE: second no-op binding for the dedicated engine's
 * absent debug-presentation handler. */
static inline void server_compat_game_debug_line(
    const intptr_t *arguments)
{
    (void)arguments;
}

/* NOT_FROM_ORIGINAL_SOURCE: bind the common syscall IDs to the dedicated
 * engine's original default-only surface implementation. */
static inline int32_t server_compat_surface_type_from_name(const char *name)
{
    return SurfaceTypeFromName(name);
}

/* NOT_FROM_ORIGINAL_SOURCE: reverse lookup binding for the same dedicated
 * default-only surface implementation. */
static inline const char *server_compat_surface_type_to_name(int32_t type)
{
    return SurfaceTypeToName(type);
}

/* NOT_FROM_ORIGINAL_SOURCE: bind the common ownership protocol to the
 * dedicated engine's low-hunk weapon-info allocation. */
static inline void *server_compat_get_weapon_info_memory(
    int32_t byteCount, int32_t *previousOwner, int32_t callerOwner)
{
    return GetWeaponInfoMemory(byteCount, previousOwner, callerOwner);
}

/* NOT_FROM_ORIGINAL_SOURCE: release binding for the same dedicated low-hunk
 * ownership protocol. */
static inline void server_compat_free_weapon_info_memory(
    int32_t callerOwner, qboolean preserveAllocation)
{
    FreeWeaponInfoMemory(callerOwner, preserveAllocation);
}

#endif
