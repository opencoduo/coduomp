#ifndef CODUO_SHARED_Q_MATRIX_TYPES_H
#define CODUO_SHARED_Q_MATRIX_TYPES_H

#include <stddef.h>

#include "qcommon/q_vector_types.h"

/* Compact affine transform: three binary32 basis rows followed immediately by
 * one binary32 translation row. */
typedef struct matrix43_s {
    axis_t axis;
    vec3_t origin;
} matrix43_t;

/* Padded DObj skeleton transform: four 16-byte rows.  The fourth lane of each
 * row is retained because the original DObj code indexes these records with a
 * 0x40-byte stride. */
typedef struct DObjSkelMat_s {
    float axis[3][4];
    float origin[4];
} DObjSkelMat;

/* Per-part DObj animation accumulator.  The Windows client evaluator
 * (CoDUOMP.exe 0x00497a10) and Linux server evaluator (coduo_lnxded
 * 0x080baa8a) both use a 0x20-byte stride and clear these eight binary32
 * lanes in the same order.  The Linux evaluator adds each contribution's
 * weight at +0x10 and translation stream lanes at +0x14..+0x1c; the Windows
 * cgame local-tag producer at 0x3001fbb0, Windows game producer at
 * 0x20052610, and Linux game producer at RVA 0x00078cf6 write zero weight
 * followed by origin XYZ at those same offsets.  The retained Mac engine,
 * cgame, and game bodies and DObjAnimMat_s symbol corroborate the layout and
 * original type name. */
typedef struct DObjAnimMat_s {
    vec4_t quat;
    float accumulatedWeight;
    vec3_t translation;
} DObjAnimMat;

typedef char q_matrix43_axis_offset[offsetof(matrix43_t, axis) == 0x00 ? 1 : -1];
typedef char q_matrix43_axis_extent[sizeof(((matrix43_t *)0)->axis) == 0x24 ? 1 : -1];
typedef char q_matrix43_origin_offset[offsetof(matrix43_t, origin) == 0x24 ? 1 : -1];
typedef char q_matrix43_origin_extent[sizeof(((matrix43_t *)0)->origin) == 0x0c ? 1 : -1];
typedef char q_matrix43_size[sizeof(matrix43_t) == 0x30 ? 1 : -1];
struct q_matrix43_alignment_probe_s {
    unsigned char byte;
    matrix43_t value;
};
typedef char q_matrix43_alignment[offsetof(struct q_matrix43_alignment_probe_s, value) == 0x04 ? 1 : -1];

typedef char q_dobj_skel_matrix_axis_offset[offsetof(DObjSkelMat, axis) == 0x00 ? 1 : -1];
typedef char q_dobj_skel_matrix_axis_extent[sizeof(((DObjSkelMat *)0)->axis) == 0x30 ? 1 : -1];
typedef char q_dobj_skel_matrix_origin_offset[offsetof(DObjSkelMat, origin) == 0x30 ? 1 : -1];
typedef char q_dobj_skel_matrix_origin_extent[sizeof(((DObjSkelMat *)0)->origin) == 0x10 ? 1 : -1];
typedef char q_dobj_skel_matrix_size[sizeof(DObjSkelMat) == 0x40 ? 1 : -1];
struct q_dobj_skel_matrix_alignment_probe_s {
    unsigned char byte;
    DObjSkelMat value;
};
typedef char q_dobj_skel_matrix_alignment[offsetof(struct q_dobj_skel_matrix_alignment_probe_s, value) == 0x04 ? 1 : -1];

typedef char q_dobj_anim_matrix_quat_offset[offsetof(DObjAnimMat, quat) == 0x00 ? 1 : -1];
typedef char q_dobj_anim_matrix_quat_extent[sizeof(((DObjAnimMat *)0)->quat) == 0x10 ? 1 : -1];
typedef char q_dobj_anim_matrix_accumulated_weight_offset[offsetof(DObjAnimMat, accumulatedWeight) == 0x10 ? 1 : -1];
typedef char q_dobj_anim_matrix_translation_offset[offsetof(DObjAnimMat, translation) == 0x14 ? 1 : -1];
typedef char q_dobj_anim_matrix_translation_extent[sizeof(((DObjAnimMat *)0)->translation) == 0x0c ? 1 : -1];
typedef char q_dobj_anim_matrix_size[sizeof(DObjAnimMat) == 0x20 ? 1 : -1];
struct q_dobj_anim_matrix_alignment_probe_s {
    unsigned char byte;
    DObjAnimMat value;
};
typedef char q_dobj_anim_matrix_alignment[offsetof(struct q_dobj_anim_matrix_alignment_probe_s, value) == 0x04 ? 1 : -1];

#endif
