#ifndef QCOMMON_Q_VECTOR_TYPES_H
#define QCOMMON_Q_VECTOR_TYPES_H

#include <stddef.h>

/* Canonical Quake vector scalar, fixed-size vectors, and 3x3 basis. */
typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];
typedef vec3_t axis_t[3];

/* Quake orientation transform shared by the engine and game modules.  The
 * byte-identical Windows conversion cluster in CoDUOMP.exe (0x004506f0),
 * uo_cgame_mp_x86.dll (0x3004f4e0), and uo_game_mp_x86.dll (0x20058d00), plus
 * the Linux engine and game bodies, all use origin at +0x00 and the three
 * basis rows at +0x0c in one 0x30-byte record. */
typedef struct orientation_s {
    vec3_t origin;
    axis_t axis;
} orientation_t;

typedef char q_orientation_origin_offset[
    offsetof(orientation_t, origin) == 0x00 ? 1 : -1];
typedef char q_orientation_origin_extent[
    sizeof(((orientation_t *)0)->origin) == 0x0c ? 1 : -1];
typedef char q_orientation_axis_offset[
    offsetof(orientation_t, axis) == 0x0c ? 1 : -1];
typedef char q_orientation_axis_extent[
    sizeof(((orientation_t *)0)->axis) == 0x24 ? 1 : -1];
typedef char q_orientation_size[sizeof(orientation_t) == 0x30 ? 1 : -1];

#endif
