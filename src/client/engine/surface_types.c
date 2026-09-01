#include "client/cgame.h"

#include "qcommon/q_string.h"
#include "surface_types.h"

#include <stddef.h>
#include <stdint.h>

#define SURFACE_TYPE_INFO(type_, name_, hasContents_, contents_) \
    {                                                            \
        (name_), (hasContents_),                                  \
        (uint32_t)(type_) << SURFACE_TYPE_SHIFT, (contents_)      \
    }

/* Original Win32 0x005ce4c8..0x005ce837. This is one sentinel-terminated
 * infoParms table: the 22 material surface-type rows are followed immediately
 * by the 32 general surface-parm rows. ParseSurfaceParm walks the complete
 * table, while surface-type conversion and RE_PickShader use their respective
 * proven subranges. The second field is set precisely for rows that clear the
 * default solid contents classification, but no CoDUOMP.exe table consumer
 * reads that column. PE_RELOCATION_VALUES_VERIFIED: all 54 name pointers and
 * the terminating null match the PE. */
const surfaceParm_t surfaceParms[] = {
    SURFACE_TYPE_INFO(SURFACE_TYPE_BARK, "bark", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_BRICK, "brick", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_CARPET, "carpet", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_CLOTH, "cloth", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_CONCRETE, "concrete", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_DIRT, "dirt", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_FLESH, "flesh", qfalse, 0),
    SURFACE_TYPE_INFO(
        SURFACE_TYPE_FOLIAGE, "foliage", qtrue,
        CONTENTS_FOLIAGE),
    SURFACE_TYPE_INFO(
        SURFACE_TYPE_GLASS, "glass", qtrue,
        CONTENTS_GLASS),
    SURFACE_TYPE_INFO(SURFACE_TYPE_GRASS, "grass", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_GRAVEL, "gravel", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_ICE, "ice", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_METAL, "metal", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_MUD, "mud", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_PAPER, "paper", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_PLASTER, "plaster", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_ROCK, "rock", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_SAND, "sand", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_SNOW, "snow", qfalse, 0),
    SURFACE_TYPE_INFO(
        SURFACE_TYPE_WATER, "water", qtrue,
        CONTENTS_WATER),
    SURFACE_TYPE_INFO(SURFACE_TYPE_WOOD, "wood", qfalse, 0),
    SURFACE_TYPE_INFO(SURFACE_TYPE_ASPHALT, "asphalt", qfalse, 0),
    /* These general surface-parm names are source data rather than synthesized
     * diagnostic labels: RE_PickShader reports a row whenever its corresponding
     * surfaceFlags or contents mask is present in the collision trace. */
    {"opaqueglass", qfalse,
     (uint32_t)SURFACE_TYPE_GLASS << SURFACE_TYPE_SHIFT, 0},
    {"clipmissile", qtrue, 0, CONTENTS_CLIPMISSILE},
    {"ai_nosight", qtrue, 0, CONTENTS_AI_NOSIGHT},
    {"clipshot", qtrue, 0, CONTENTS_CLIPSHOT},
    {"playerclip", qtrue, 0, CONTENTS_PLAYERCLIP},
    {"monsterclip", qtrue, 0, CONTENTS_MONSTERCLIP},
    {"vehicleclip", qtrue, 0, CONTENTS_VEHICLECLIP},
    {"itemclip", qtrue, 0, CONTENTS_ITEMCLIP},
    {"nodrop", qtrue, 0, CONTENTS_NODROP},
    {"nonsolid", qtrue, SURF_NONSOLID, 0},
    {"origin", qtrue, 0, CONTENTS_ORIGIN},
    {"detail", qfalse, 0, CONTENTS_DETAIL},
    {"structural", qfalse, 0, CONTENTS_STRUCTURAL},
    {"portal", qtrue, SURF_PORTAL, 0},
    {"canshootclip", qfalse, 0, CONTENTS_CANSHOOTCLIP},
    {"sky", qfalse, SURF_SKY, CONTENTS_SKY},
    {"lightfilter", qfalse, SURF_LIGHTFILTER, 0},
    {"alphashadow", qfalse, SURF_ALPHASHADOW, 0},
    {"castshadow", qfalse, SURF_CASTSHADOW, 0},
    {"hint", qfalse, SURF_HINT, 0},
    {"slick", qfalse, SURF_SLICK, 0},
    {"noimpact", qfalse, SURF_NOIMPACT, 0},
    {"nomarks", qfalse, SURF_NOMARKS, 0},
    {"ladder", qfalse, SURF_LADDER, 0},
    {"nodamage", qfalse, SURF_NODAMAGE, 0},
    {"nosteps", qfalse, SURF_NOSTEPS, 0},
    {"nodraw", qfalse, SURF_NODRAW, 0},
    {"pointlight", qfalse, SURF_POINTLIGHT, 0},
    {"nolightmap", qfalse, SURF_NOLIGHTMAP, 0},
    {"nodlight", qfalse, SURF_NODLIGHT, 0},
    {"hmgstanding", qtrue, 0, CONTENTS_HMG_STANDING},
    {"hmgcrouching", qtrue, 0, CONTENTS_HMG_CROUCHING},
    {NULL, qfalse, 0, 0}
};

#undef SURFACE_TYPE_INFO

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(surfaceParms) ==
                   55 * sizeof(surfaceParms[0]),
               "surface-parm table row count changed");
#endif

/* Source: CoDUOMP.exe 0x00401db0..0x00401e03.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401db0_00401e04.mcode.
 * Name: exact same-module Mac symbol CL_SurfaceTypeFromName. */
surfaceType_t CL_SurfaceTypeFromName(const char *name)
{
    if (Q_strcasecmp(name, "default") == 0)
        return SURFACE_TYPE_DEFAULT;

    for (size_t index = 0;
         index < SURFACE_TYPE_INFO_COUNT;
         ++index) {
        if (Q_strcasecmp(name, surfaceParms[index].name) == 0) {
            return (surfaceType_t)(
                (surfaceParms[index].surfaceFlags >>
                 SURFACE_TYPE_SHIFT) &
                SURFACE_TYPE_MASK);
        }
    }

    return SURFACE_TYPE_INVALID;
}

/* Source: CoDUOMP.exe 0x00401e10..0x00401e28.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401e10_00401e29.mcode.
 * Name: exact same-module Mac symbol CL_SurfaceTypeToName. */
const char *CL_SurfaceTypeToName(int32_t surfaceType)
{
    if (surfaceType > SURFACE_TYPE_DEFAULT &&
        surfaceType < SURFACE_TYPE_COUNT) {
        return surfaceParms[surfaceType - 1].name;
    }
    return "default";
}

/* Source: CoDUOMP.exe 0x0043d760..0x0043d7b3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d760_0043d7b4.mcode.
 * Name: exact same-module Mac symbol Com_SurfaceTypeFromName. This is a
 * distinct original entry point with the same table walk as the client
 * module-service version above. */
surfaceType_t Com_SurfaceTypeFromName(const char *name)
{
    if (Q_strcasecmp(name, "default") == 0)
        return SURFACE_TYPE_DEFAULT;

    for (size_t index = 0;
         index < SURFACE_TYPE_INFO_COUNT;
         ++index) {
        if (Q_strcasecmp(name, surfaceParms[index].name) == 0) {
            return (surfaceType_t)(
                (surfaceParms[index].surfaceFlags >>
                 SURFACE_TYPE_SHIFT) &
                SURFACE_TYPE_MASK);
        }
    }

    return SURFACE_TYPE_INVALID;
}

/* Source: CoDUOMP.exe 0x0043d7c0..0x0043d7d8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043d7c0_0043d7d9.mcode.
 * Name: exact same-module Mac symbol Com_SurfaceTypeToName. */
const char *Com_SurfaceTypeToName(int32_t surfaceType)
{
    if (surfaceType > SURFACE_TYPE_DEFAULT &&
        surfaceType < SURFACE_TYPE_COUNT) {
        return surfaceParms[surfaceType - 1].name;
    }
    return "default";
}
