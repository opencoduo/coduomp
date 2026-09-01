#include "bg_player_state.h"

#include "compat/coduo_int32_bits.h"

#include <stdint.h>

void BG_AddPredictableEventToPlayerstate(int32_t event, int32_t eventParm,
                                         playerState_t *ps)
{
    if (event == 0) {
        return;
    }

    ps->events[(uint32_t)ps->eventIndex & (MAX_PS_EVENTS - 1u)] =
        (uint8_t)event;
    ps->eventParms[(uint32_t)ps->eventIndex & (MAX_PS_EVENTS - 1u)] =
        (uint8_t)eventParm;
    ps->eventIndex = coduo_int32_from_bits((uint32_t)ps->eventIndex + 1u);
}
