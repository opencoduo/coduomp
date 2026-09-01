#ifndef QCOMMON_STATMON_TYPES_H
#define QCOMMON_STATMON_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "q_shared_types.h"

enum { STATMON_ENTRY_CAPACITY = 64 };

/* Engine-owned warning icon returned to cgame by
 * CG_GET_EXPIRING_ICON_LIST.  CoDUOMP.exe StatMon_Warning writes the signed
 * expiration deadline and renderer shader handle at these two offsets; the
 * Windows cgame consumer reads the same 0x08-byte rows. */
typedef struct statmon_entry_s {
    int32_t expireTime;
    qhandle_t shaderHandle;
} statmon_entry_t;

typedef char q_statmon_expire_time_offset[
    offsetof(statmon_entry_t, expireTime) == 0x00 ? 1 : -1];
typedef char q_statmon_shader_handle_offset[
    offsetof(statmon_entry_t, shaderHandle) == 0x04 ? 1 : -1];
typedef char q_statmon_entry_size[
    sizeof(statmon_entry_t) == 0x08 ? 1 : -1];

#endif
