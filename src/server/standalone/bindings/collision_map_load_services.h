#ifndef CODUO_COLLISION_MAP_LOAD_SERVICES_H
#define CODUO_COLLISION_MAP_LOAD_SERVICES_H

void Sys_LoadingKeepAlive(void);

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated original retains an out-of-line
 * empty platform hook where the Windows client inlines a message pump. */
#define COLLISION_MAP_LOADING_KEEPALIVE() Sys_LoadingKeepAlive()

#ifdef CODUO_COLLISION_DIGEST
void coduo_engine_emit_collision_digest(const char *mapName);
void coduo_engine_emit_collision_parity(const char *mapName);
void coduo_engine_emit_capsule_patch_digest(const char *mapName);
void coduo_engine_emit_core_math_digest(void);
void coduo_engine_collision_load_timing_begin(void);
void coduo_engine_collision_load_timing_end(const char *mapName);

#define COLLISION_MAP_LOAD_DIAGNOSTICS_BEGIN(mapName) \
    do { \
        (void)(mapName); \
        coduo_engine_collision_load_timing_begin(); \
    } while (0)
#define COLLISION_MAP_LOAD_DIAGNOSTICS_END(mapName) \
    do { \
        coduo_engine_collision_load_timing_end(mapName); \
        coduo_engine_emit_collision_digest(mapName); \
        coduo_engine_emit_collision_parity(mapName); \
        coduo_engine_emit_capsule_patch_digest(mapName); \
        coduo_engine_emit_core_math_digest(); \
    } while (0)
#else
#define COLLISION_MAP_LOAD_DIAGNOSTICS_BEGIN(mapName) ((void)(mapName))
#define COLLISION_MAP_LOAD_DIAGNOSTICS_END(mapName) ((void)(mapName))
#endif

#endif
