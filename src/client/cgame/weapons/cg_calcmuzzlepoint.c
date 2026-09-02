// Source: uo_cgame_mp_x86.dll 0x30048b60..0x30048d53
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30048b60_30048d53.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_CalcMuzzlePoint(weaponName, entityNum, muzzle) -> qboolean
 *
 * The assigned .mcode "# name PM_Weapon_CheckForChangeWeapon" is a pure
 * corpus/size guess (win 0x1f3 vs matched 0x1f4) and is REJECTED. The function's
 * own error string proves the identity: on the non-client / DObj-less path it
 * calls Com_DPrintf("No %s in CG_CalcMuzzlePoint on entity %d.\n", weaponName,
 * entityNum) (the format literal at 0x3007a824). The same-module PPC bank lists
 * CG_CalcMuzzlePoint in cgame_mp.dll, so the name is adopted directly.
 *
 * Purpose: compute the world-space muzzle point used to spawn a weapon's muzzle
 * flash / tracer origin for entity `entityNum`, writing the vec3 result into
 * `muzzle`. There are three distinct sources for the point:
 *   1. The LOCAL PREDICTED PLAYER: when the current snapshot's playerState is the
 *      first-person view (playerStateFlags has one of the two 0xc0000 view bits
 *      set) and entityNum == the local client number, the muzzle is built from
 *      cg_snap->ps.origin + a per-stance viewheight, optionally nudged forward for
 *      an LMG fired while aiming down sights, then spread/spun by the
 *      view-anchored effect helper.
 *   2. The entity's animated DObj TAG: for a remote/other entity that owns a DObj
 *      (slot->currentValid != NULL), the engine bone/tag matrix is fetched and its
 *      translation row copied out as the muzzle.
 *   3. The entity's LERP ORIGIN with a per-stance viewheight: fallback when the
 *      entity number is below MAX_CLIENTS but has no DObj tag — a diagnostic is
 *      printed and the muzzle is the entity's interpolated origin plus the stance
 *      viewheight offset.
 *
 * ABI (custom register-argument, proven from the caller at 0x30048d88 and the
 * prologue): weaponName arrives in EAX (MOV EBX,EAX), entityNum in ECX (MOV
 * EDI,ECX), and `muzzle` is a single caller-pushed stack pointer read as
 * [ESP+0x64] after the SUB ESP,0x58 + four register pushes (MOV EBP,[ESP+0x64]).
 * Returns qboolean in EAX (1 whenever a point was produced, 0 only on the
 * no-DObj-and-null-bone early out). The callee cleans its own stack (SUB/ADD
 * ESP,0x58, plain RET; the pushed float args to the two helpers are cleaned with
 * ADD ESP,0x10). Modeled with ordered parameters and no calling-convention
 * attribute (the syntax-only build does not require one).
 *
 * Per-instruction proof of the behavior-affecting statements:
 *   30048b6c MOV EAX,[cg_snap]
 *   30048b74 TEST [EAX+0x18],0xc0000 ; JZ 0x30048c68   ps.playerStateFlags view bits
 *   30048b81 CMP EDI,[EAX+0xe0] ; JNZ 0x30048c68        entityNum == ps.clientNum ?
 *   30048b8d MOV EAX,[EAX+0x20] ; MOV [EBP],EAX         muzzle[0] = ps.origin[0]
 *   30048b99 MOV EDX,[cg_snap+0x24] ; MOV [EBP+4],EDX   muzzle[1] = ps.origin[1]
 *   30048ba4 FLD [cg_snap+0x28] ; FST [EBP+8]           muzzle[2] = ps.origin[2]
 *   30048bb0 FADD [cg_snap+0x104] ; FSTP [EBP+8]        muzzle[2] += ps.viewHeightCurrent
 *   30048bb6 MOV ECX,[bg_weaponInfos]
 *   30048bc4 MOV EDX,[cg_snap+0xe4]                      currentWeapon index
 *   30048bca MOV EDX,[ECX+EDX*4]                         w = bg_weaponInfos[currentWeapon]
 *   30048bcd CMP [EDX+0x80],3 ; JNZ 0x30048c3c           w->weaponClass == LMG ?
 *   30048bd6 TEST [cg_snap+0x18],0x20 ; JZ 0x30048c3c    ps.playerStateFlags & PMF_ADS ?
 *   30048bdc MOV EAX,[cg_snap+0x5b0]                      ps.proneDirection (yaw, float bits)
 *   30048bee angles = {0.0f, proneDirection, 0.0f}
 *   30048c02 CALL AngleVectors(angles, forward=&fwd, right=NULL, up=NULL)
 *   30048c07 FLD fwd[0] ; FMUL 19.0f ; FADD [EBP]   ; FSTP [EBP]   muzzle[0]+=19*fwd[0]
 *   30048c17 FLD fwd[1] ; FMUL 19.0f ; FADD [EBP+4] ; FSTP [EBP+4] muzzle[1]+=19*fwd[1]
 *   30048c27 FLD fwd[2] ; FMUL 19.0f ; FADD [EBP+8] ; FSTP [EBP+8] muzzle[2]+=19*fwd[2]
 *   30048c37 MOV EAX,[cg_snap]
 *   30048c3c MOV ECX,[EAX+0x50]                          ps.leanFraction (float bits)
 *   30048c3f MOV EDX,[cg_refdefViewAngles[1]]
 *   30048c45 PUSH 20.0f ; PUSH 16.0f ; PUSH leanFraction ; PUSH spinAngle
 *   30048c51 MOV EDX,EBP (out=muzzle) ; CALL AddLeanToPosition
 *   30048c5e MOV EAX,1 ; RET                             return qtrue
 *
 *   30048c68 (view/local-player test failed) ESI = &cg_entities[entityNum]
 *   30048c76 MOV EAX,[ESI+0x1e8] ; TEST ; JZ 0x30048c97 slot->currentValid present ?
 *   30048c80 PUSH slot->currentState.number ; PUSH 0xa5 ; CALL cgame_syscall
 *                                                         handle = trap(CG_DOBJ_GET_HANDLE, num)
 *   30048c93 TEST handle ; JZ 0x30048ca1 -> if 0 fall to origin fallback; nonzero:
 *   30048ca1 CALL CG_DObjGetWorldTagMatrix(handle, slot, boneMat[16])
 *   30048cb1 TEST EAX ; JZ 0x30048cd7 -> on success copy translation row:
 *   30048cb5 muzzle[2]=boneMat[14] ; muzzle[0]=boneMat[12] ; muzzle[1]=boneMat[13]
 *            ([ESP+0x60]/[ESP+0x58]/[ESP+0x5c] over the buffer based at ESP+0x28)
 *   30048ccd MOV EAX,1 ; RET
 *
 *   30048cd7 (no DObj tag) CMP entityNum,0x40 (MAX_CLIENTS)
 *   30048cda muzzle = slot->currentState.pos.trBase
 *            (dword copies of +0x18/+0x1c/+0x20)
 *   30048cec JGE 0x30048d46 -> entityNum >= MAX_CLIENTS: done, return qtrue
 *   30048cee PUSH entityNum ; PUSH weaponName ; PUSH fmt ; CALL Com_DPrintf
 *   30048cfa MOV EAX,slot->currentState.eFlags(+0x8) ; add per-stance viewheight to muzzle[2]:
 *   30048d00 TEST AL,0x40 -> muzzle[2] += bg_viewheight_prone_vmCvar.integer   (int, FILD)
 *   30048d1d TEST AL,0x20 -> muzzle[2] += bg_viewheight_crouched_vmCvar.integer    (int, FILD)
 *   30048d3a else         -> muzzle[2] += bg_viewheight_standing.integer (int, FILD)
 *   30048d49 MOV EAX,1 ; RET
 *
 *   30048c97 slot has no dobj and trap returned 0 -> XOR EAX,EAX ; RET (return qfalse)
 *
 * Widths/signedness: muzzle[0]/[1] on the local-player path and the whole
 * currentState.pos.trBase fallback are plain 32-bit dword copies (float bit
 * patterns carried through unchanged); muzzle[2] work is x87 single-precision
 * (FLD/FST/FADD DWORD).
 * The stance viewheight adds are FILD DWORD (signed int -> float) of three .bss
 * int globals. The 0xc0000 / 0x20 flag tests are on the same ps.playerStateFlags
 * dword; the 0x40/0x20 stance tests are on the entity slot's eFlags byte.
 */

/* 19.0f — the forward muzzle offset applied to an LMG while PMF_ADS is set.
 * .rdata float at 0x3007c190. */
#define CG_MUZZLE_LMG_ADS_FORWARD 19.0f

/* Spread/spin helper scales, pushed as 16.0f and 20.0f (0x41800000/0x41a00000). */
#define CG_MUZZLE_SPIN_SCALE_A 16.0f
#define CG_MUZZLE_SPIN_SCALE_B 20.0f

/* MAX_CLIENTS — entity numbers below this are player clients; the muzzle
 * fallback prints a diagnostic and applies a stance viewheight only for them. */
enum {
    CG_MAX_CLIENTS = 64
};

/* Entity currentState.eFlags stance bits selecting the viewheight offset added
 * to a non-DObj entity's muzzle. */
/* AddLeanToPosition (0x3004f370) offsets `out` (EDX) by a view-anchored
 * direction from a yaw angle, a lean fraction that early-outs at 0.0, and two
 * float scales. */

/* The diagnostic callee 0x3002b470 is Com_DPrintf (the developer-gated variadic
 * printer, reconstructed in functions/FUN_3002b470_3002b4ca.c), declared in
 * client_recovered.h — NOT the unconditional Com_Printf. The caller cleans the
 * stack (ADD ESP,0xc after fmt + two int args here). */

qboolean CG_CalcMuzzlePoint(const char *weaponName, int32_t entityNum, vec3_t muzzle)
{
    /* 30048b6c: the current snapshot / local playerState. */
    snapshot_t *snap = cg_snap;

    /* 30048b74/0x30048b81: local first-person predicted-player muzzle path — the
     * view is first-person (one of the 0xc0000 flags) and this is the local
     * client. Everything else falls through to the entity paths at 0x30048c68. */
    if ((snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0 && entityNum == snap->ps.psClientNum) {

        /* 30048b8d-0x30048bbc: muzzle = ps.origin, then muzzle.z += ps.viewHeightCurrent. */
        muzzle[0] = snap->ps.psOrigin[0];
        muzzle[1] = snap->ps.psOrigin[1];
        muzzle[2] = snap->ps.psOrigin[2];
        muzzle[2] = muzzle[2] + snap->ps.viewHeightCurrent;

        /* 30048bb6-0x30048bd4: for an LMG fired while PMF_ADS is set, nudge
         * the muzzle forward along the aim direction. */
        weaponInfo_t *w = bg_weaponInfos[snap->ps.currentWeapon];
        if (w->weaponClass == WEAPCLASS_LMG && (snap->ps.playerStateFlags & PMF_ADS) != 0) {

            /* 30048bdc-0x30048bfa: angles = (0, proneDirection, 0); the yaw is the
             * ps float at +0x5b0 copied as a dword. */
            vec3_t angles;
            angles[0] = 0.0f;
            angles[1] = snap->ps.proneDirection;
            angles[2] = 0.0f;

            /* 30048c02: AngleVectors with only forward requested (right/up NULL). */
            vec3_t forward;
            AngleVectors(angles, forward, NULL, NULL);

            /* 30048c07-0x30048c34: muzzle += 19.0f * forward. */
            muzzle[0] = muzzle[0] + forward[0] * CG_MUZZLE_LMG_ADS_FORWARD;
            muzzle[1] = muzzle[1] + forward[1] * CG_MUZZLE_LMG_ADS_FORWARD;
            muzzle[2] = muzzle[2] + forward[2] * CG_MUZZLE_LMG_ADS_FORWARD;
        }

        /* 30048c3c-0x30048c58: apply the view-anchored spin offset. leanFraction
         * and the spin angle are ps/cg floats read as dwords. */
        AddLeanToPosition(muzzle, cg_refdefViewAngles[1], snap->ps.leanFraction, CG_MUZZLE_SPIN_SCALE_A, CG_MUZZLE_SPIN_SCALE_B);

        /* 30048c5e: MOV EAX,1 ; RET. */
        return qtrue;
    }

    /* 30048c68-0x30048c70: &cg_entities[entityNum] (stride 0x288). */
    centity_t *slot = cgame_compat_unchecked_cgentity(entityNum);

    /* 30048c76-0x30048c95: only an entity that owns an animated DObj whose engine
     * handle is nonzero can supply a tag muzzle; a null dobj (JZ 0x30048c7e) or a
     * zero engine handle (JZ 0x30048c95) both fail out with no muzzle. */
    if (slot->currentValid != 0) {
        /* 30048c80-0x30048c88: query the entity's DObj handle from the engine. */
        struct DObj_s *dobj = (struct DObj_s *)(intptr_t)cgame_syscall(CG_DOBJ_GET_HANDLE, (int32_t)slot->currentState.number);

        if (dobj != NULL) {
            /* 30048ca1-0x30048cb1: build the entity's DObj bone/tag world matrix.
             * On success the muzzle is the matrix translation row. EAX=weaponName
             * is set before this call (0x30048ca7 MOV EAX,EBX): it is the tagName
             * argument the callee forwards to trap(0xb2, self, tagName) to resolve
             * the named bone — NOT a dead load (the earlier annotation was wrong;
             * see the callee body at 0x3001fdf8). */
            DObjSkelMat boneMatrix;
            if (CG_DObjGetWorldTagMatrix(dobj, weaponName, slot, &boneMatrix)) {
                /* 30048cb5-0x30048cc9: muzzle = the DObjSkelMat origin row read
                 * at [ESP+0x58/0x5c/0x60] over the buffer based at ESP+0x28. */
                muzzle[0] = boneMatrix.origin[0];
                muzzle[1] = boneMatrix.origin[1];
                muzzle[2] = boneMatrix.origin[2];
                /* 30048ccd: MOV EAX,1 ; RET. */
                return qtrue;
            }
            /* 30048cb3: JZ -> the bone build failed; drop to the origin fallback
             * below rather than returning. */
        } else {
            /* 30048c95/0x30048c97: zero engine handle -> XOR EAX,EAX ; RET. */
            return qfalse;
        }
    } else {
        /* 30048c7e/0x30048c97: no DObj at all -> XOR EAX,EAX ; RET. */
        return qfalse;
    }

    /* 30048cd7-0x30048ce9: origin fallback (reached only when a valid DObj handle
     * produced no bone matrix) — muzzle = entity lerp origin. */
    muzzle[0] = slot->currentState.origin[0];
    muzzle[1] = slot->currentState.origin[1];
    muzzle[2] = slot->currentState.origin[2];

    /* 30048cec: player-client entities (< MAX_CLIENTS) get a diagnostic and a
     * stance viewheight; higher entity numbers are done. */
    if (entityNum >= CG_MAX_CLIENTS) {
        /* 30048d46: MOV EAX,1 ; RET. */
        return qtrue;
    }

    /* 30048cee-0x30048cf5: developer diagnostic for a missing weapon DObj on a
     * client (Com_DPrintf, the developer-gated printer at 0x3002b470). */
    Com_DPrintf("No %s in CG_CalcMuzzlePoint on entity %d.\n", weaponName, entityNum);

    /* 30048cfa-0x30048d43: add the per-stance viewheight (int cvar-like globals,
     * FILD to float) to muzzle.z based on the entity's eFlags stance bits. */
    uint32_t eFlags = slot->currentState.eFlags;
    /* Each viewheight integer enters via a bare FILD fed straight into FADD muzzle[2]
     * (0x30048d04 FILD; 0x30048d11 FADD; 0x30048d14 FSTP — no FSTP DWORD between), so
     * the DLL keeps the converted integer 80-bit. An explicit (float) cast would round
     * it first (Class 4); drop it and let the implicit conversion stay exact. */
    if ((eFlags & EF_PRONE) != 0) {
        muzzle[2] = muzzle[2] + bg_viewheight_prone_vmCvar.integer;
    } else if ((eFlags & EF_CROUCHING) != 0) {
        muzzle[2] = muzzle[2] + bg_viewheight_crouched_vmCvar.integer;
    } else {
        muzzle[2] = muzzle[2] + bg_viewheight_standing.integer;
    }

    /* 30048d49: MOV EAX,1 ; RET. */
    return qtrue;
}
