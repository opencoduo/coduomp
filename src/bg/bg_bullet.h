#ifndef CODUO_SHARED_BG_BULLET_H
#define CODUO_SHARED_BG_BULLET_H

#include "qcommon/q_vector_types.h"

void gunrandom(float *x, float *y);
/* Base of the forward/right/up/origin packet.  Original cgame callers pass a
 * matrix43_t basis while game callers pass the identical 0x30-byte prefix of
 * weapon_muzzle_t, so this is intentionally a flat packet pointer rather than
 * a C array parameter with a misleading single-object bound. */
void BG_Bullet_Endpos(float spread, vec3_t end,
                      const float *muzzlePoints);

#endif
