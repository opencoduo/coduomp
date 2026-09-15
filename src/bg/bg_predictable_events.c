#include "bg_player_state.h"

#include "compat/coduo_int32_bits.h"

#include <stdint.h>

/* NOT_FROM_ORIGINAL_SOURCE: client predictable-event provenance observer; server modules leave this unset. */
void (*coduomp_predictable_event_observer)(const playerState_t *ps, int32_t event, int32_t eventParm);

void BG_AddPredictableEventToPlayerstate(int32_t event, int32_t eventParm,
                                         playerState_t *ps)
{
    if (event == 0) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: capture the producing command before the event sequence advances. */
    if (coduomp_predictable_event_observer != NULL) {
        coduomp_predictable_event_observer(ps, (uint8_t)event, (uint8_t)eventParm);
    }

    ps->events[(uint32_t)ps->eventIndex & (MAX_PS_EVENTS - 1u)] =
        (uint8_t)event;
    ps->eventParms[(uint32_t)ps->eventIndex & (MAX_PS_EVENTS - 1u)] =
        (uint8_t)eventParm;
    ps->eventIndex = coduo_int32_from_bits((uint32_t)ps->eventIndex + 1u);
}
