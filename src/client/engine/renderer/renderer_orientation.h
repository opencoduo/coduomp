#ifndef CODUOMP_RENDERER_ORIENTATION_H
#define CODUOMP_RENDERER_ORIENTATION_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"

/* Quake III's renderer-local orientationr_t is retained unchanged: the CoD
 * clients use the same origin/axis/viewOrigin/modelMatrix roles and extent. */
typedef struct orientationr_s {
    vec3_t origin;                          /* original +0x00 */
    vec3_t axis[3];                         /* original +0x0c */
    vec3_t viewOrigin;                      /* original +0x30 */
    float modelMatrix[16];                  /* original +0x3c */
} orientationr_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(orientationr_t) == 4,
               "i386 renderer orientation alignment changed");
_Static_assert(offsetof(orientationr_t, origin) == 0x00,
               "original renderer orientation origin offset");
_Static_assert(sizeof(((orientationr_t *)0)->origin) == 0x0c,
               "original renderer orientation origin extent");
_Static_assert(offsetof(orientationr_t, axis) == 0x0c,
               "original renderer orientation axis offset");
_Static_assert(sizeof(((orientationr_t *)0)->axis) == 0x24,
               "original renderer orientation axis extent");
_Static_assert(offsetof(orientationr_t, viewOrigin) == 0x30,
               "original renderer local-view-origin offset");
_Static_assert(sizeof(((orientationr_t *)0)->viewOrigin) == 0x0c,
               "original renderer local-view-origin extent");
_Static_assert(offsetof(orientationr_t, modelMatrix) == 0x3c,
               "original renderer model-matrix offset");
_Static_assert(sizeof(((orientationr_t *)0)->modelMatrix) == 0x40,
               "original renderer model-matrix extent");
_Static_assert(sizeof(orientationr_t) == 0x7c,
               "original renderer orientation extent");
#endif

#endif
