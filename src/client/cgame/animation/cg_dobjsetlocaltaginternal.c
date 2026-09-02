// Source: uo_cgame_mp_x86.dll 0x3001fbb0..0x3001fcf1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001fbb0_3001fcf1.mcode
//
// CG_DObjSetLocalTagInternal — write one DObj local-tag rot/trans slot from Euler
// angles + an origin. Assigned .mcode name "G_FreeEntity" was a size-match guess
// and is REJECTED: this body converts angles to a quaternion (via FSINCOS +
// QuatMultiply) and stores a DObjAnimMat, which is not an entity-free routine. The
// resolved name is the same-module PPC name CG_DObjSetLocalTagInternal (cgame_mp),
// corroborated by the call graph: BG_Player_DoControllers (0x30005730) resolves
// a bone index via trap CG_DOBJ_GET_BONE_INDEX (0xb2), binds a rot/trans slot via
// trap 0xa2/0xa3, then calls here to fill the slot.
//
// Register/stack ABI (internal __usercall), proven from the caller at 0x300057b2:
//   EAX = self (entity), pushed as the single arg to trap 0xa1
//   ECX = rotTransIndex (bone's bound rot/trans slot; << 5 = *0x20 stride)
//   EBX = angles (const vec3_t*, NULL => identity quaternion)
//   one stack arg (loaded via MOV EBP,[ESP+0x50]) = origin (const vec3_t*)
// Non-default frame: SUB ESP,0x48 prologue; caller-cleaned; no return value used
// (the machine code leaves EAX = origin[2] at exit but the callers ignore it).
//
// EBX is a live input register that the function never saves/restores — it is a
// register-passed parameter, not a callee-saved local.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#include <stdint.h>
#include <string.h>

void CG_DObjSetLocalTagInternal(void *self, int rotTransIndex, const vec3_t angles, const vec3_t origin)
{
    /* trap CG_DOBJ_GET_ROT_TRANS_ARRAY (0xa1): fetch the entity's rot/trans matrix
     * array, then select this slot (SHL EDI,0x5 == index * sizeof(DObjAnimMat)). */
    DObjAnimMat *base = (DObjAnimMat *)(intptr_t)cgame_syscall(CG_DOBJ_GET_ROT_TRANS_ARRAY, (intptr_t)self);
    /* SHL EDI,5 is a target-width modulo-2^32 byte offset.  Apply that
     * resolved offset to the native engine pointer so a 64-bit base is not
     * truncated and the original null-base-plus-offset behavior is retained. */
    uint32_t matrixOffset = (uint32_t)rotTransIndex << 5;
    DObjAnimMat *mat = (DObjAnimMat *)((uintptr_t)base + (uintptr_t)matrixOffset);

    if (angles != NULL) {
        /* Build three per-axis unit quaternions from the half-angle-scaled angles.
         * The three FMULs at 0x3001fbe7, 0x3001fc29, and 0x3001fc6c all read
         * 0x3007be78 = 0x3c0efa35 = pi/360, not the pi/180 `deg2rad` constant at
         * 0x3007bd70. Thus the degrees-to-radians conversion and quaternion
         * half-angle are folded into one exact float multiplication.
         * FSINCOS yields ST0=cos, ST1=sin; the setter stores cos into the w slot
         * (+0x0c) and sin into the axis slot. quat layout is [x,y,z,w].
         *
         * Composition order (two QuatMultiply calls): q = (qz * qy) * qx, where
         *   qz = angles[1] about Z : {0, 0, sin, cos}
         *   qy = angles[0] about Y : {0, sin, 0, cos}
         *   qx = angles[2] about X : {sin, 0, 0, cos}
         * QuatMultiply(first, second) returns second * first, so each formal
         * operand pair is written in the reverse of the product above.
         */
        float sc;

        float qz[4]; /* [ESP+0x14..0x20], from angles[1] */
        sc = angles[1] * DEG_TO_HALF_RAD;
        qz[0] = 0.0f;
        qz[1] = 0.0f;
        coduo_x87_sincosf(sc, &qz[2], &qz[3]);

        float qy[4]; /* [ESP+0x24..0x30], from angles[0] */
        sc = angles[0] * DEG_TO_HALF_RAD;
        qy[0] = 0.0f;
        qy[2] = 0.0f;
        coduo_x87_sincosf(sc, &qy[1], &qy[3]);

        float qx[4]; /* [ESP+0x34..0x40], from angles[2] */
        sc = angles[2] * DEG_TO_HALF_RAD;
        qx[1] = 0.0f;
        qx[2] = 0.0f;
        coduo_x87_sincosf(sc, &qx[0], &qx[3]);

        float tmp[4]; /* [ESP+0x44]: qz * qy */
        QuatMultiply(qy, qz, tmp);
        QuatMultiply(qx, tmp, mat->quat);
    } else {
        /* angles == NULL: identity quaternion {0,0,0,1} (MOV [ESI+0xc],0x3f800000). */
        mat->quat[0] = 0.0f;
        mat->quat[1] = 0.0f;
        mat->quat[2] = 0.0f;
        mat->quat[3] = 1.0f;
    }

    /* The local-tag producer starts a fresh contribution: zero weight at
     * +0x10, followed by origin XYZ at +0x14..+0x1c. */
    mat->accumulatedWeight = 0.0f;
    memcpy(&mat->translation[0], &origin[0], sizeof(mat->translation[0]));
    memcpy(&mat->translation[1], &origin[1], sizeof(mat->translation[1]));
    memcpy(&mat->translation[2], &origin[2], sizeof(mat->translation[2]));
}
