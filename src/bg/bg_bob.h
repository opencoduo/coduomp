#ifndef BG_BOB_H
#define BG_BOB_H

#include "qcommon/player_state_types.h"

/* Windows leaves each result live in ST0.  Linux explicitly narrows each
 * public result to binary32 before returning it. */
#if defined(WINDOWS_BEHAVIOR)
long double BG_GetBobCycle(const playerState_t *ps);
long double BG_GetVerticalBobFactor(const playerState_t *ps, float phase,
                                    float amplitude, float maxAmplitude);
long double BG_GetHorizontalBobFactor(const playerState_t *ps, float phase,
                                      float amplitude, float maxAmplitude);
#else
float BG_GetBobCycle(const playerState_t *ps);
float BG_GetVerticalBobFactor(const playerState_t *ps, float phase,
                              float amplitude, float maxAmplitude);
float BG_GetHorizontalBobFactor(const playerState_t *ps, float phase,
                                float amplitude, float maxAmplitude);
#endif

#endif
