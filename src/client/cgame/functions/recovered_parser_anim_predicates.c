// Complete parser-stack and animation-state predicates recovered from mcode.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

enum recoveredPlayerStance_e {
    RECOVERED_STANCE_STAND = 0,
    RECOVERED_STANCE_PRONE = 1,
    RECOVERED_STANCE_CROUCH = 2
};

// Source RVA: 0x300083e0
int32_t StanceForViewHeight(const playerState_t *ps)
{
    if (ps->viewHeightTarget == ps->crouchViewHeight)
        return RECOVERED_STANCE_CROUCH;
    if (ps->viewHeightTarget == ps->proneViewHeight)
        return RECOVERED_STANCE_PRONE;
    return RECOVERED_STANCE_STAND;
}

// Source RVA: 0x30008440
double PmTimeScale(void)
{
    const int32_t pmTime = pm->ps->pmTime;
    if (pmTime >= 1700)
        return 2.5f;
    /* 0x3000845d FILD; 0x30008460 FMUL const: pmTime enters the multiply via a
     * bare FILD with no FSTP DWORD, so no (float) cast (it would round under
     * -std=c11). 1.5f/1700.0f folds to the single FMUL const at 0x3007be64. */
    return (double)pmTime * (double)(1.5f / 1700.0f) +
           (double)1.0f;
}
