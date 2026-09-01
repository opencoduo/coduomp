#ifndef CODUOMP_SURFACE_TYPES_H
#define CODUOMP_SURFACE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "q_shared.h"

enum {
    SURFACE_TYPE_INFO_COUNT = SURFACE_TYPE_COUNT - 1,
    SURFACE_PARM_GENERAL_FIRST = SURFACE_TYPE_INFO_COUNT
};

/* The original infoParms table begins with the named material surface types,
 * continues directly into the general map surface parms, and ends in one null
 * sentinel. The +0x04 initializer is one exactly for material/clip/origin-style
 * rows that replace the default solid classification, establishing its
 * clearSolid role. CoDUOMP.exe retains that metadata but never reads it; its
 * table consumers read name, surfaceFlags, and contents only. */
typedef struct surfaceParm_s {
    const char *name;
    qboolean clearSolid;
    uint32_t surfaceFlags;
    uint32_t contents;
} surfaceParm_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(surfaceParm_t) == 4,
               "i386 surface-parm alignment changed");
_Static_assert(offsetof(surfaceParm_t, name) == 0x00,
               "i386 surface-parm name moved");
_Static_assert(offsetof(surfaceParm_t, clearSolid) == 0x04,
               "i386 surface-parm clear-solid flag moved");
_Static_assert(offsetof(surfaceParm_t, surfaceFlags) == 0x08,
               "i386 surface-parm surface flags moved");
_Static_assert(offsetof(surfaceParm_t, contents) == 0x0c,
               "i386 surface-parm contents moved");
_Static_assert(sizeof(surfaceParm_t) == 0x10,
               "i386 surface-parm stride changed");
#endif

extern const surfaceParm_t surfaceParms[];

#endif
