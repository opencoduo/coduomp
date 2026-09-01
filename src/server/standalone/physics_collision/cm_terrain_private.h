#ifndef CODUO_CM_TERRAIN_PRIVATE_H
#define CODUO_CM_TERRAIN_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

#include "cm_trace_core_private.h"
#include "collision/collision_terrain_dispatch.h"
#include "collision/collision_terrain_trace.h"
#include "collision/collision_static_models.h"
#include "collision/collision_triangle_soup.h"

int32_t CM_LittleShort(int16_t value);
uint32_t CM_LittleLong(uint32_t value);
long double CM_LittleFloat(float value);
#ifdef CODUO_COLLISION_DIGEST
/* Collision-parity harness only (see cm_collision_digest.c) — not part of the
 * reconstruction. coduo_engine_digest_curve_collide remains in the engine's
 * collision-build support source. */
void coduo_engine_collision_digest_bytes_external(const void *data, size_t length, uint64_t *accum);
void coduo_engine_digest_curve_collide(const void *curveCollide, uint64_t *accum,
                                       int32_t *planeCountOut,
                                       int32_t *facetCountOut);
#endif

#endif
