#ifndef GAME_ENTITY_DISPATCH_PRIVATE_H
#define GAME_ENTITY_DISPATCH_PRIVATE_H

#include "recovered_game.h"

typedef enum binary_mover_state_e {
    MOVER_STATE_DOOR_POS1 = 0,
    MOVER_STATE_DOOR_POS2 = 1,
    MOVER_STATE_DOOR_POS3 = 2,
    MOVER_STATE_DOOR_MOVING_TO_POS2 = 3,
    MOVER_STATE_DOOR_MOVING_TO_POS1 = 4,
    MOVER_STATE_DOOR_MOVING_TO_POS3 = 5,
    MOVER_STATE_DOOR_MOVING_TO_POS2_FROM_POS3 = 6,
    MOVER_STATE_ROTATE_POS1 = 7,
    MOVER_STATE_ROTATE_POS2 = 8,
    MOVER_STATE_ROTATE_MOVING_TO_POS2 = 9,
    MOVER_STATE_ROTATE_MOVING_TO_POS1 = 10
} binary_mover_state_t;

typedef struct mover_push_record_s {
    gentity_t *ent;
    vec3_t origin;
    vec3_t angles; /* idTech pushed_t slot; UO binary does not read/write it. */
    float yawDelta;
} mover_push_record_t;

extern mover_push_record_t *moverPushStackCursor;

#endif
