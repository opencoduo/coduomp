#ifndef QCOMMON_NET_FIELD_TYPES_H
#define QCOMMON_NET_FIELD_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Descriptor for one four-byte member of a delta-compressed native record.
 * The offset addresses the owning source object; bits selects the field's
 * proven wire encoding. The pointer and descriptor stride widen natively on
 * 64-bit hosts and are not themselves serialized. */
typedef struct netField_s {
    const char *name;
    int32_t offset;
    int32_t bits;
} netField_t;

/* The Windows client and Linux server carry the same complete delta-table
 * domains. These are table extents, not configurable protocol limits. */
enum {
    MSG_BITMASK_TABLE_COUNT = 33,
    MSG_ENTITY_NETFIELD_COUNT = 60,
    MSG_ARCHIVED_ENTITY_NETFIELD_COUNT = 68,
    MSG_CLIENT_NETFIELD_COUNT = 22,
    MSG_HUD_ELEM_NETFIELD_COUNT = 30,
    MSG_PLAYERSTATE_NETFIELD_COUNT = 114,
    MSG_OBJECTIVE_NETFIELD_COUNT = 6
};

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(netField_t) == 0x0c, "netField_t size mismatch");
_Static_assert(offsetof(netField_t, name) == 0x00, "netField_t.name offset mismatch");
_Static_assert(offsetof(netField_t, offset) == 0x04, "netField_t.offset offset mismatch");
_Static_assert(offsetof(netField_t, bits) == 0x08, "netField_t.bits offset mismatch");
#endif
#endif

#endif
