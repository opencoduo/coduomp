#ifndef QCOMMON_Q_COLLISION_TYPES_H
#define QCOMMON_Q_COLLISION_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "q_vector_types.h"

/*
 * Public collision/content domain shared by the engine and native modules.
 * The low surface bits and packed material field are read unchanged from BSP
 * shader records into trace_t.surfaceFlags.  CoDUOMP.exe's surface-parameter
 * table and the Windows cgame movement bodies agree on the named bits; for
 * example PM_CrashLand tests SURF_NODAMAGE at 0x3000a477, PM_Friction tests
 * SURF_SLICK at 0x3000b3fd, and the landing/footstep family tests SURF_NOSTEPS
 * and extracts bits 20..24.  The Windows and Linux server collision paths use
 * the same content bits and the 0x00ffffff inline-brush-model sentinel.
 */
enum {
    SOLID_BMODEL = 0x00ffffff,

    SURFACE_TYPE_SHIFT = 20,
    SURFACE_TYPE_MASK = 31,

    SURF_NODAMAGE = 0x00000001,
    SURF_SLICK = 0x00000002,
    SURF_SKY = 0x00000004,
    SURF_LADDER = 0x00000008,
    SURF_NOIMPACT = 0x00000010,
    SURF_NOMARKS = 0x00000020,
    SURF_NODRAW = 0x00000080,
    SURF_HINT = 0x00000100,
    SURF_NOLIGHTMAP = 0x00000400,
    SURF_POINTLIGHT = 0x00000800,
    SURF_NOSTEPS = 0x00002000,
    SURF_NONSOLID = 0x00004000,
    SURF_LIGHTFILTER = 0x00008000,
    SURF_ALPHASHADOW = 0x00010000,
    SURF_NODLIGHT = 0x00020000,
    SURF_CASTSHADOW = 0x00040000,

    CONTENTS_SOLID = 0x00000001,
    CONTENTS_FOLIAGE = 0x00000002,
    CONTENTS_GLASS = 0x00000010,
    CONTENTS_WATER = 0x00000020,
    CONTENTS_CANSHOOTCLIP = 0x00000040,
    CONTENTS_CLIPMISSILE = 0x00000080,
    CONTENTS_VEHICLECLIP = 0x00000200,
    CONTENTS_ITEMCLIP = 0x00000400,
    CONTENTS_SKY = 0x00000800,
    CONTENTS_AI_NOSIGHT = 0x00001000,
    CONTENTS_CLIPSHOT = 0x00002000,
    CONTENTS_HMG_STANDING = 0x00004000,
    CONTENTS_PLAYERCLIP = 0x00010000,
    CONTENTS_MONSTERCLIP = 0x00020000,
    CONTENTS_HMG_CROUCHING = 0x00400000,
    CONTENTS_ORIGIN = 0x01000000,
    CONTENTS_BODY = 0x02000000,
    CONTENTS_DETAIL = 0x08000000,
    CONTENTS_STRUCTURAL = 0x10000000,

    /* Canonical player movement masks. Windows cgame prediction and the
     * Windows/Linux game modules use these exact live/dead trace values. */
    MASK_PLAYERSOLID = 0x02810011,
    MASK_DEADSOLID = 0x00810011
};

#define SURF_PORTAL UINT32_C(0x80000000)
#define CONTENTS_NODROP UINT32_C(0x80000000)
#define MASK_ALL UINT32_C(0xffffffff)

/* Complete material-id domain used by the surface-name syscalls and packed in
 * trace surfaceFlags.  The engine's original name table has 23 entries, while
 * cgame consumers independently use the same water value (20) and count (23). */
typedef enum surfaceType_e {
    SURFACE_TYPE_INVALID = -1,
    SURFACE_TYPE_DEFAULT = 0,
    SURFACE_TYPE_BARK = 1,
    SURFACE_TYPE_BRICK = 2,
    SURFACE_TYPE_CARPET = 3,
    SURFACE_TYPE_CLOTH = 4,
    SURFACE_TYPE_CONCRETE = 5,
    SURFACE_TYPE_DIRT = 6,
    SURFACE_TYPE_FLESH = 7,
    SURFACE_TYPE_FOLIAGE = 8,
    SURFACE_TYPE_GLASS = 9,
    SURFACE_TYPE_GRASS = 10,
    SURFACE_TYPE_GRAVEL = 11,
    SURFACE_TYPE_ICE = 12,
    SURFACE_TYPE_METAL = 13,
    SURFACE_TYPE_MUD = 14,
    SURFACE_TYPE_PAPER = 15,
    SURFACE_TYPE_PLASTER = 16,
    SURFACE_TYPE_ROCK = 17,
    SURFACE_TYPE_SAND = 18,
    SURFACE_TYPE_SNOW = 19,
    SURFACE_TYPE_WATER = 20,
    SURFACE_TYPE_WOOD = 21,
    SURFACE_TYPE_ASPHALT = 22,
    SURFACE_TYPE_COUNT = 23
} surfaceType_t;

typedef char q_collision_surface_type_abi_size[
    sizeof(surfaceType_t) == 4 ? 1 : -1];

/* Canonical Quake collision-plane record.  All authoritative CoD:UO engine
 * and module bodies use this 0x14-byte layout and the signbits byte at +0x11. */
typedef struct cplane_s {
    vec3_t normal;
    float dist;
    uint8_t type;
    uint8_t signbits;
    uint8_t pad[2];
} cplane_t;

typedef char q_collision_cplane_normal_offset[
    offsetof(cplane_t, normal) == 0x00 ? 1 : -1];
typedef char q_collision_cplane_dist_offset[
    offsetof(cplane_t, dist) == 0x0c ? 1 : -1];
typedef char q_collision_cplane_type_offset[
    offsetof(cplane_t, type) == 0x10 ? 1 : -1];
typedef char q_collision_cplane_signbits_offset[
    offsetof(cplane_t, signbits) == 0x11 ? 1 : -1];
typedef char q_collision_cplane_size[sizeof(cplane_t) == 0x14 ? 1 : -1];

/* Four-float plane used by terrain collision and renderer geometry. */
typedef union plane_u {
    struct planeComponents_s {
        vec3_t normal;
        float distance;
    } components;
    vec4_t equation;
} plane_t;

typedef char q_collision_plane_normal_offset[
    offsetof(plane_t, components.normal) == 0x00 ? 1 : -1];
typedef char q_collision_plane_distance_offset[
    offsetof(plane_t, components.distance) == 0x0c ? 1 : -1];
typedef char q_collision_plane_size[sizeof(plane_t) == 0x10 ? 1 : -1];

/* Collision result shared by engine collision, server tracing, and the game
 * modules.  CoDUOMP.exe SV_PointTraceToEntity (0x00467670) constructs the
 * complete record at these offsets and copies twelve dwords.  The Windows
 * cgame PM_trace (0x30008280), Windows game ground trace (0x2000a230), Linux
 * engine SV_Trace (0x0809afc9), and Linux game ground trace (RVA 0x0007fc99)
 * independently use the same i386 ABI. */
typedef struct trace_s {
    float fraction;
    vec3_t endpos;
    vec3_t normal;
    int32_t surfaceFlags;
    int32_t contents;
    const char *material;
    uint16_t entityNum;
    uint16_t partName;
    uint16_t partGroup;
    uint8_t allsolid;
    uint8_t startsolid;
} trace_t;

typedef char q_collision_trace_fraction_offset[
    offsetof(trace_t, fraction) == 0x00 ? 1 : -1];
typedef char q_collision_trace_endpos_offset[
    offsetof(trace_t, endpos) == 0x04 ? 1 : -1];
typedef char q_collision_trace_normal_offset[
    offsetof(trace_t, normal) == 0x10 ? 1 : -1];
typedef char q_collision_trace_surface_flags_offset[
    offsetof(trace_t, surfaceFlags) == 0x1c ? 1 : -1];
typedef char q_collision_trace_contents_offset[
    offsetof(trace_t, contents) == 0x20 ? 1 : -1];

#if UINTPTR_MAX == UINT32_MAX
typedef char q_collision_trace_material_offset[
    offsetof(trace_t, material) == 0x24 ? 1 : -1];
typedef char q_collision_trace_entity_num_offset[
    offsetof(trace_t, entityNum) == 0x28 ? 1 : -1];
typedef char q_collision_trace_part_name_offset[
    offsetof(trace_t, partName) == 0x2a ? 1 : -1];
typedef char q_collision_trace_part_group_offset[
    offsetof(trace_t, partGroup) == 0x2c ? 1 : -1];
typedef char q_collision_trace_allsolid_offset[
    offsetof(trace_t, allsolid) == 0x2e ? 1 : -1];
typedef char q_collision_trace_startsolid_offset[
    offsetof(trace_t, startsolid) == 0x2f ? 1 : -1];
typedef char q_collision_trace_size[sizeof(trace_t) == 0x30 ? 1 : -1];
#elif UINTPTR_MAX == UINT64_MAX
typedef char q_collision_trace_material_offset[
    offsetof(trace_t, material) == 0x28 ? 1 : -1];
typedef char q_collision_trace_entity_num_offset[
    offsetof(trace_t, entityNum) == 0x30 ? 1 : -1];
typedef char q_collision_trace_part_name_offset[
    offsetof(trace_t, partName) == 0x32 ? 1 : -1];
typedef char q_collision_trace_part_group_offset[
    offsetof(trace_t, partGroup) == 0x34 ? 1 : -1];
typedef char q_collision_trace_allsolid_offset[
    offsetof(trace_t, allsolid) == 0x36 ? 1 : -1];
typedef char q_collision_trace_startsolid_offset[
    offsetof(trace_t, startsolid) == 0x37 ? 1 : -1];
typedef char q_collision_trace_size[sizeof(trace_t) == 0x38 ? 1 : -1];
#endif

#endif
