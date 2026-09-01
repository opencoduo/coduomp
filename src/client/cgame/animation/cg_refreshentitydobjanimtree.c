// Source: uo_cgame_mp_x86.dll 0x30021ea0..0x30021f9d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021ea0_30021f9d.mcode
//
// CG_RefreshEntityDObjAnimTree — refresh the per-entity DObj model registration
// for the client entity whose table index is `entityNum` (register argument ESI).
// Given the entity's eType and its animTreeParam (a registered model handle), it:
//   1. acquires the entity's engine DObj handle (CG_DOBJ_GET_HANDLE / trap 0xa5),
//   2. wraps the model handle into an engine DObj/model object
//      (CG_DOBJ_WRAP_MODEL / trap 0x32),
//   3. if a DObj is already registered for this index but the cached (key,handle)
//      pair differs, releases the stale registration (CG_SAFE_CLIENT_DOBJ_FREE /
//      trap 0xa8) and clears the cached info,
//   4. builds a one-element DObj model set for the new handle
//      (CG_CLIENT_DOBJ_CREATE / trap 0xa7), and for the MG42 eType (11) first
//      constructs the weapon anim tree (CG_CreateMG42WeaponAnimTree, 0x3001e960)
//      and instantiates it (CG_XANIM_CREATE_TREE / trap 134 == 0x86), then
//   5. caches the new (eType, dObjHandle) pair back into the DObj-info table.
// Returns the acquired DObj handle (EAX) on the build path; returns 0 on the
// early-out paths (no handle / already up to date).
//
// NAMING: the .mcode header name "PlaneFromPoints" is a pure size guess
// (win size 0xfd matched a same-size server symbol) and is REJECTED — this
// function contains no floating point at all (no cross product / normalize /
// dot), only integer engine syscalls and a DObj-registration table. The name
// CG_RefreshEntityDObjAnimTree is the caller-observed name already recorded in
// client_recovered.h for 0x30021ea0 (assigned by the CG_General reconstruction),
// adopted here; the DObj-info table names (cg_dObjInfoKeys/cg_dObjInfoHandles)
// follow the same-module cgame PPC accessor cluster CG_SetDObjInfo /
// CG_CheckDObjInfoMatches / CG_Free*DObjInfo. Exact engine symbol unproven (no
// cgame syscall-id table recovered); role-named from behavior + call graph.
//
// ABI (proven from the callers at 0x3001e469 / 0x3001e731 / 0x3001ecd9 /
// 0x3001efb6 / 0x3001f29d / 0x30021699): entityNum in ESI (register argument),
// eType and animTreeParam as two caller-cleaned 32-bit stack args (callers do
// `push <param1>; push <eType>; call`). Callee saves EBX/EBP/EDI/ESI and reserves
// 0x10 bytes of frame (SUB ESP,0x10). Plain RET (caller cleans the two stack
// args). The register+stack mix is documented here, not encoded as a
// calling-convention attribute, for the syntax-only build.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

intptr_t CG_RefreshEntityDObjAnimTree(int32_t entityNum, int32_t eType,
                                      int32_t animTreeParam)
{
    /* One DObjModel descriptor handed to CG_CLIENT_DOBJ_CREATE. Recovered
     * from the 0x10-byte frame reservation (SUB ESP,0x10) written at
     * 0x30021f51..0x30021f62: +0x00 model, +0x04 tagName = NULL,
     * +0x08 modelIndex as a signed 16-bit word, +0x0c ignoreCollision = 1.
     * The original leaves abiGap_00a untouched; the engine skips that word. */
    DObjModel element;

    /* 30021ea5 MOV EBP,[ESP+0x20] -> arg1 (animTreeParam), the model handle to wrap. */
    int32_t modelHandle = animTreeParam;

    /* 30021eaa PUSH ESI saves entityNum immediately before 30021eab pushes
     * syscall 0xa5. The original variadic i386 syscall therefore observes that
     * saved ESI word as argument 1 even though there is no second explicit PUSH
     * at this call site. Spell out the implicit stack argument in portable
     * source; otherwise a native 64-bit build passes unrelated stack contents
     * to CG_DOBJ_GET_HANDLE. 30021eb9 then saves the returned handle in EBX. */
    intptr_t dObjHandle =
        cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);

    /* 30021eb6 PUSH EBP / PUSH 0x32 / CALL -> CG_DOBJ_WRAP_MODEL(modelHandle);
     *   30021ec6 MOV EDI,EAX. Both syscall arg blocks cleaned by ADD ESP,0x10. */
    XModel *wrappedModel = (XModel *)(intptr_t)cgame_syscall(
        CG_DOBJ_WRAP_MODEL, modelHandle);

    /* Register-value aliasing in the original:
     *   EBX = dObjHandle (from trap 0xa5), EDI = wrappedModel (from trap 0x32). */

    /* 30021ec4 TEST EBX,EBX / JZ 0x30021f09: no DObj handle -> skip the
     *   compare/release, but still fall through to the wrappedModel handling. */
    if (dObjHandle != 0) {
        /* 30021eca TEST EDI,EDI / JZ 0x30021ee8: wrappedModel == 0 forces the
         *   release+clear path. Otherwise compare the cached info; when the cached
         *   (key,handle) already equals (arg0, wrappedModel) there is nothing to do.
         * 30021ece MOV EAX,[ESP+0x20] -> arg0 (eType), the cached key.
         * 30021ed2 CMP cg_dObjInfoKeys[entityNum],EAX
         * 30021edb CMP cg_dObjInfoHandles[entityNum],EDI */
        if (wrappedModel == 0 ||
            cg_dObjInfoKeys[entityNum] != (uint32_t)eType ||
            cg_dObjInfoHandles[entityNum] != wrappedModel) {
            /* 30021ee8 PUSH 0x1 / PUSH ESI / PUSH 0xa8 / CALL -> release the stale
             *   DObj registration for this index (flag 1). ADD ESP,0xc.
             * 30021ef9 XOR EBX,EBX ; store 0 into both cached table slots. */
            cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, entityNum, 1);
            dObjHandle = 0;
            cg_dObjInfoKeys[entityNum] = 0;
            cg_dObjInfoHandles[entityNum] = 0;
        } else {
            /* 30021ee2 JZ 0x30021f94: cached info matches -> return the cached
             *   handle (EAX = EBX = dObjHandle) unchanged. */
            return dObjHandle;
        }
    }

    /* 30021f09 TEST EDI,EDI / JZ 0x30021f94: no wrapped model -> return
     *   dObjHandle (0 here, since the only way in with a live handle returned
     *   above). */
    if (wrappedModel == 0) {
        /* 30021f96 MOV EAX,EBX -> return the (now zero) dObjHandle. */
        return dObjHandle;
    }

    /* 30021f11 MOV EBX,[ESP+0x20] -> reload arg0 (eType), reused as the cached key
     *   and (for eType 11) the MG42 branch selector. */
    intptr_t animTreeInstance = 0; /* 30021f41 XOR EAX,EAX default */

    /* 30021f15 CMP EBX,0xb / JNZ 0x30021f41: only the turret/MG42 eType (11)
     *   builds a weapon anim tree. */
    if (eType == ET_TURRET) {
        /* 30021f1a MOV EAX,ESI / IMUL EAX,EAX,0x288 / ADD EAX,0x3048c6e0 ->
         *   &cg_effectSlots[entityNum] (centity_t, stride 0x288), passed in
         *   EAX (register argument) to CG_CreateMG42WeaponAnimTree. Index the typed
         *   cg_entities[] base by entity number, matching the other centity consumers. */
        intptr_t masterTree = CG_CreateMG42WeaponAnimTree(
            cg_entities + entityNum);

        /* 30021f2c TEST EAX,EAX / JZ 0x30021f41: only instantiate a non-zero tree.
         * 30021f30 PUSH EAX / PUSH 0x86 / CALL -> CG_XANIM_CREATE_TREE(masterTree)
         *   -> per-entity tree instance handle (kept in EAX). ADD ESP,8. */
        if (masterTree != 0) {
            animTreeInstance = cgame_syscall(CG_XANIM_CREATE_TREE, masterTree);
        }
    }

    /* 30021f43.. build the single-element DObj model set and commit it.
     *   element = { model=wrappedModel, 0, modelIndex=(int16_t)animTreeParam, 1 }.
     * 30021f51 MOV [ESP+0x20],EDI       -> element.model = wrappedModel
     * 30021f55 MOV WORD [ESP+0x28],BP   -> element.modelIndex = (int16_t)modelHandle
     * 30021f5a MOV [ESP+0x24],0         -> element.tagName = NULL
     * 30021f62 MOV [ESP+0x2c],1         -> element.ignoreCollision = 1 */
    element.model = wrappedModel;
    element.tagName = NULL;
    element.modelIndex = (int16_t)modelHandle;
    element.ignoreCollision = 1;

    /* 30021f43 PUSH ESI / PUSH EAX / PUSH 0x1 / LEA ECX,[ESP+0x18] / PUSH ECX /
     *   PUSH 0xa7 / CALL -> CG_CLIENT_DOBJ_CREATE(&element, 1, animTreeInstance,
     *   entityNum). ADD ESP shared with the following syscall's cleanup. */
    cgame_syscall(CG_CLIENT_DOBJ_CREATE, (intptr_t)&element, 1,
                  animTreeInstance, entityNum);

    /* 30021f70..0x30021f84 publishes both cache words between pushing the
     * entity-number argument and invoking CG_DOBJ_GET_HANDLE. In logical source
     * order, both stores therefore precede the re-acquisition callback. */
    cg_dObjInfoKeys[entityNum] = (uint32_t)eType;
    cg_dObjInfoHandles[entityNum] = wrappedModel;
    intptr_t reacquired = cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum);

    /* 30021f8d.. epilogue: the build path returns via 0x30021f93 (RET) WITHOUT the
     *   `MOV EAX,EBX` that the 0x30021f94 early-return path uses, so EAX still holds
     *   the result of the final CG_DOBJ_GET_HANDLE call. */
    return reacquired;
}
