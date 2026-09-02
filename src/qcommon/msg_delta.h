#ifndef QCOMMON_MSG_DELTA_H
#define QCOMMON_MSG_DELTA_H

#include "msg.h"
#include "net_field_types.h"
#include "snapshot_types.h"

#include <stdint.h>

typedef void (*msg_delta_extra_writer_t)(msg_t *message, const void *record);

#ifdef __cplusplus
extern "C" {
#endif

void MSG_WriteDeltaValue(msg_t *message, int32_t oldValue, int32_t newValue, int32_t bitCount);
int32_t MSG_ReadDeltaValue(msg_t *message, int32_t oldValue, int32_t bitCount);
void MSG_WriteDeltaKey(msg_t *message, uint32_t key, int32_t oldValue, int32_t newValue, int32_t bitCount);
int32_t MSG_ReadDeltaKey(msg_t *message, uint32_t key, int32_t oldValue, int32_t bitCount);
void MSG_WriteKey(msg_t *message, uint32_t key, int32_t value, int32_t bitCount);
int32_t MSG_ReadKey(msg_t *message, uint32_t key, int32_t bitCount);
void MSG_WriteDeltaKeyByte(msg_t *message, uint32_t key, int8_t oldValue, uint32_t newValue);
uint32_t MSG_ReadDeltaKeyByte(msg_t *message, uint8_t key, uint32_t oldValue);
void MSG_WriteDeltaKeyShort(msg_t *message, uint32_t key, int16_t oldValue, uint32_t newValue);
int32_t MSG_ReadDeltaKeyShort(msg_t *message, uint16_t key, int32_t oldValue);
void MSG_WriteReliableCommandToBuffer(const char *input, char *output, int32_t outputSize);
void MSG_SetDefaultUserCmd(const playerState_t *playerState, usercmd_t *command);
uint32_t MSG_HorMoveFrom(int32_t forwardMove, int32_t rightMove);
uint32_t MSG_VertMoveFrom(int32_t upMove);
void MSG_HorMoveTo(uint32_t packedMove, int8_t *forwardMove, int8_t *rightMove);
void MSG_VertMoveTo(uint32_t packedMove, int8_t *upMove);
void MSG_WriteDeltaUsercmdKey(msg_t *message, uint32_t key, const usercmd_t *from, const usercmd_t *to);
void MSG_ReadDeltaUsercmdKey(msg_t *message, uint32_t key, const usercmd_t *from, usercmd_t *to);
void MSG_WriteDeltaField(msg_t *message, const void *from, const void *to, const netField_t *field);
void MSG_WriteDeltaFields(msg_t *message, const void *from, const void *to, qboolean force, int32_t count, const netField_t *fields);
void MSG_WriteDeltaStruct(msg_t *message, const void *from, const void *to, qboolean force, int32_t count, int32_t numberBits,
                          const netField_t *fields, msg_delta_extra_writer_t extraWriter, qboolean writeForceBit);
void MSG_WriteDeltaEntity_ChangedCallback(msg_t *message, const void *entityState);
void MSG_WriteDeltaEntity(msg_t *message, const entityState_t *from, const entityState_t *to, qboolean force);
void MSG_WriteDeltaArchivedEntity(msg_t *message, const archivedEntity_t *from, const archivedEntity_t *to, qboolean force);
void MSG_WriteDeltaClient(msg_t *message, const clientState_t *from, const clientState_t *to, qboolean force);
void MSG_ReadDeltaField(msg_t *message, const void *from, void *to, const netField_t *field, qboolean print);
void MSG_ReadDeltaFields(msg_t *message, const void *from, void *to, int32_t count, const netField_t *fields);
qboolean MSG_ReadDeltaStruct(msg_t *message, const void *from, void *to, int32_t number, int32_t count, const netField_t *fields);
qboolean MSG_ReadDeltaEntity(msg_t *message, const entityState_t *from, entityState_t *to, int32_t number);
qboolean MSG_ReadDeltaArchivedEntity(msg_t *message, const archivedEntity_t *from, archivedEntity_t *to, int32_t number);
qboolean MSG_ReadDeltaClient(msg_t *message, const clientState_t *from, clientState_t *to, int32_t clientNum);
void MSG_WriteDeltaHudElems(msg_t *message, const hudElem_t *from, const hudElem_t *to, int32_t maxHudElems);
void MSG_ReadDeltaHudElems(msg_t *message, const hudElem_t *from, hudElem_t *to, int32_t maxHudElems);
void MSG_WriteDeltaPlayerstate(msg_t *message, const playerState_t *from, const playerState_t *to);
void MSG_ReadDeltaPlayerstate(msg_t *message, const playerState_t *from, playerState_t *to);

#ifdef __cplusplus
}
#endif

#endif
