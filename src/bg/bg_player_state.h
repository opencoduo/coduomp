#ifndef BG_PLAYER_STATE_H
#define BG_PLAYER_STATE_H

#include "qcommon/entity_state_types.h"
#include "qcommon/player_state_types.h"
#include "qcommon/trajectory_types.h"

#include <stdint.h>

/*
 * Append one predictable event to the four-entry player-state event ring.
 * A zero event is a no-op; event and eventParm are stored as zero-extended
 * low bytes before eventIndex advances.
 *
 * The operation graph and field widths agree in all three authoritative
 * module bodies:
 *
 *   uo_cgame_mp_x86.dll  0x30006550..0x30006583
 *   uo_game_mp_x86.dll   0x200062c0..0x200062f3
 *   game.mp.uo.i386.so   RVA 0x000209eb..0x00020a35
 *
 * The normal source contract is the game/Quake-style event, parameter, state
 * order.  The Windows compiler assigned the cgame arguments to ECX, stack,
 * and EAX respectively; that register assignment is not a different source
 * signature.
 */
void BG_AddPredictableEventToPlayerstate(int32_t event, int32_t eventParm,
                                         playerState_t *ps);
/* Project a mutable player state into its network entity state. Both routines
 * advance the player state's event cursors. Linux symbols establish the
 * canonical player-state-first source signature; the Windows register order
 * is a compiler ABI detail. */
void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *es,
                                 qboolean snap);
void BG_PlayerStateToEntityStateExtrapolate(playerState_t *ps,
                                            entityState_t *es, int32_t time,
                                            qboolean snap);
extern const int32_t iSingleClientEvents[7];
extern const int32_t *pEventSingleClientList;
void BG_EvaluateTrajectory(const trajectory_t *trajectory, int32_t atTime,
                           vec3_t result);
void BG_EvaluateTrajectoryDelta(const trajectory_t *trajectory,
                                int32_t atTime, vec3_t result);

#endif
