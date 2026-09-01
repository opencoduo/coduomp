#ifndef QCOMMON_FX_TYPES_H
#define QCOMMON_FX_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* The scheduler key and SEffectTemplate name field both hold at most 63
 * bytes plus NUL. Effect configstrings use this same key prefix up to the
 * first '.', so the server and client can enforce the proven consumer limit
 * before crossing their module boundaries. */
enum { FX_EFFECT_TEMPLATE_NAME_CAPACITY = 64 };

/* Exact engine/cgame effect-bolt boundary.  CoDUOMP.exe consumes entityNum
 * and boneIndex as the two consecutive dwords used to resolve one DObj bone;
 * the Windows cgame passes the same record to both FX-on-tag syscalls. */
typedef struct sfx_bolt_info_s {
    int32_t entityNum;
    int32_t boneIndex;
} sfx_bolt_info_t;

typedef char q_fx_bolt_entity_offset[
    offsetof(sfx_bolt_info_t, entityNum) == 0x00 ? 1 : -1];
typedef char q_fx_bolt_bone_offset[
    offsetof(sfx_bolt_info_t, boneIndex) == 0x04 ? 1 : -1];
typedef char q_fx_bolt_size[
    sizeof(sfx_bolt_info_t) == 0x08 ? 1 : -1];

#endif
