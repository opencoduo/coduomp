#ifndef QCOMMON_TRAJECTORY_TYPES_H
#define QCOMMON_TRAJECTORY_TYPES_H

#include "q_vector_types.h"

#include <stddef.h>
#include <stdint.h>

/* Entity-motion trajectory kinds shared by the engine, cgame, and game
 * modules. TR_DECCELERATE retains the spelling used by every recovered tree. */
typedef enum trType_e {
    TR_STATIONARY = 0,
    TR_INTERPOLATE = 1,
    TR_LINEAR = 2,
    TR_LINEAR_STOP = 3,
    TR_SINE = 4,
    TR_GRAVITY = 5,
    TR_GRAVITY_LOW = 6,
    TR_GRAVITY_FLOAT = 7,
    TR_LINKED = 8,
    TR_ACCELERATE = 9,
    TR_DECCELERATE = 10
} trType_t;

/* The retained entity-state and BG trajectory consumers agree on this
 * pointer-free 0x24-byte layout on Windows, Linux, and macOS. */
typedef struct trajectory_s {
    trType_t trType;       /* +0x00 */
    int32_t trTime;        /* +0x04 */
    int32_t trDuration;    /* +0x08 */
    vec3_t trBase;         /* +0x0c */
    vec3_t trDelta;        /* +0x18 */
} trajectory_t;

#define TRAJECTORY_LAYOUT_ASSERT(name_, expression_) \
    typedef char name_[(expression_) ? 1 : -1]

#if defined(_MSC_VER)
#define TRAJECTORY_ALIGNOF(type_) __alignof(type_)
#elif defined(__GNUC__) || defined(__clang__)
#define TRAJECTORY_ALIGNOF(type_) __alignof__(type_)
#elif defined(__cplusplus)
#define TRAJECTORY_ALIGNOF(type_) alignof(type_)
#else
#define TRAJECTORY_ALIGNOF(type_) _Alignof(type_)
#endif

TRAJECTORY_LAYOUT_ASSERT(q_trajectory_type_size, sizeof(trType_t) == 4);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_alignment,
                         TRAJECTORY_ALIGNOF(trajectory_t) == 4);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_type_offset,
                         offsetof(trajectory_t, trType) == 0x00);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_time_offset,
                         offsetof(trajectory_t, trTime) == 0x04);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_duration_offset,
                         offsetof(trajectory_t, trDuration) == 0x08);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_base_offset,
                         offsetof(trajectory_t, trBase) == 0x0c);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_base_extent,
                         sizeof(((trajectory_t *)0)->trBase) == 0x0c);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_delta_offset,
                         offsetof(trajectory_t, trDelta) == 0x18);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_delta_extent,
                         sizeof(((trajectory_t *)0)->trDelta) == 0x0c);
TRAJECTORY_LAYOUT_ASSERT(q_trajectory_size, sizeof(trajectory_t) == 0x24);

#undef TRAJECTORY_ALIGNOF
#undef TRAJECTORY_LAYOUT_ASSERT

#endif
