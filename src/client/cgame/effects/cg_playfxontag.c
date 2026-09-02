// Source: uo_cgame_mp_x86.dll 0x30022780..0x3002280b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30022780_3002280b.mcode

#include "client/cgame/client_recovered.h"
#include "qcommon/fx_types.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <string.h>

/*
 * CG_PlayFxOnTag (0x30022780)
 *
 * Plays a tag-bound effect described by an effect config string. The effect
 * spawner `self` (in EAX) selects a config string by self->currentState.eventParm (passed
 * separately in ECX by the dispatcher at 0x30022810, MOV ECX,[ESI+0xa4]);
 * cfgIndex = fxId + CS_FX. The config string's first two bytes encode
 * a small handle-table index and the rest is a tag name. The tag is resolved on
 * self->currentState.number via CG_RESOLVE_TAG; if it resolves, the indexed effect handle is
 * played at self->lerpOrigin via CG_PLAY_EFFECT_ON_TAG.
 *
 * Name: role name; the .mcode's size-matched "G_DObjSetLocalTag" guess is REJECTED
 * — that is a game-server DObj tag setter, whereas this is cgame code that reads a
 * config string, resolves a tag, and fires an effect-play trap. The bad-index path
 * reuses CG_ConfigString's inlined Com_ErrorMessage("CG_ConfigString: bad index:
 * %i", n) (string at 0x30077d90), which anchors the config-string lookup identity.
 * The Mac CG_PlayFxOnTag performs the corresponding config-string, bone-index,
 * and entity-effect play sequence, resolving the source name.
 *
 * Register/stack ABI (proven from the .mcode and the sole caller 0x3002352c):
 *   - EAX = self (centity_t *); EDI := EAX.
 *   - ECX = fxId; ESI := ECX + 0x3e5 (CS_FX) = cfgIndex.
 *   - SUB ESP,8 reserves two stack dwords; the traps are cdecl and clean their own
 *     pushed args (ADD ESP,0xc / ADD ESP,0x14 after each call); RET (no immediate).
 *
 * Instruction-by-instruction facts:
 *   - 30022786 ADD ESI,0x3e5           cfgIndex = fxId + 997
 *   - 3002278f JS  (bad if cfgIndex<0) 30022797 JL 0x800 (skip error if <2048),
 *     else PUSH cfgIndex; PUSH "CG_ConfigString: bad index: %i"; CALL
 *     Com_ErrorMessage (0x3002b300); the lookup still proceeds afterwards (matches
 *     CG_ConfigString's inlined behaviour).
 *   - 300227a7 MOV EAX,[ESI*4 + 0x30440a00]  offset = cg_gameState.stringOffsets[cfgIndex]
 *   - 300227ae ADD EAX,0x30442a00            EAX = &cg_gameState.stringData[offset] = cfg
 *   - 300227b3 MOVSX ECX,byte [EAX]          d0 = (signed char)cfg[0]
 *   - 300227b6 MOVSX EDX,byte [EAX+1]        d1 = (signed char)cfg[1]
 *   - 300227ba LEA ECX,[ECX+ECX*4]  (ECX*5)
 *   - 300227bd LEA ECX,[EDX+ECX*2]           biasedId = d1 + d0*10
 *   - 300227c0 MOV ESI,[ECX*4 + 0x30447ca4]  handle = cg_effectDefs[id]
 *     because 0x304484e4 - 0x30447ca4 == 528*4 and biasedId == id + 48*11.
 *     Mac 0x1003451c..0x10034538 instead subtracts ASCII '0' explicitly.
 *   - 300227c7 MOV ECX,[EDI]                 objId = self->currentState.number
 *   - 300227c9 ADD EAX,2                      tagName = cfg + 2 (past the two id bytes)
 *   - PUSH tagName; PUSH objId; PUSH 0xe3; MOV [ESP+0x14],ECX (spill objId to
 *     local slot A); CALL cgame_syscall  => result = cgame_syscall(0xe3, objId, tagName)
 *   - 300227e2 MOV [ESP+0xc],EAX (local slot B = result); JL 0x30022805 if result<0
 *   - else LEA EDX,[ESP+8] (&slot A, holding objId); PUSH &slotA; PUSH 0; ADD
 *     EDI,0x208 (&self->lerpOrigin); PUSH &origin; PUSH handle; PUSH 0xe9; CALL
 *     cgame_syscall => cgame_syscall(0xe9, handle, &self->lerpOrigin, 0, &objIdSlot)
 *
 * Signedness: both config bytes are sign-extended (MOVSX), so a high-bit byte
 * contributes negatively to `id`; preserved with an explicit (signed char) read.
 * The id -> handle multiply and add are ordinary 32-bit integer arithmetic.
 */

void CG_PlayFxOnTag(centity_t *self, int fxId)
{
    int cfgIndex;
    const char *cfg;
    int d0, d1, id;
    uint32_t handle;
    const char *tagName;
    sfx_bolt_info_t boltInfo;

    /* ADD ESI,997 wraps modulo 2^32 and JS consumes the resulting sign bit. */
    uint32_t cfgIndexBits = (uint32_t)fxId + (uint32_t)CS_FX;
    memcpy(&cfgIndex, &cfgIndexBits, sizeof(cfgIndex));

    /* Inlined CG_ConfigString bounds check: report out-of-range, then still look
     * up (matches the machine code, which falls through into the lookup). The
     * signed compare uses JS then JL against 2048, i.e. cfgIndex < 0 || >= 2048. */
    if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS) {
        Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex);
    }

    int32_t stringOffset = cgame_compat_read_target_i32_index(
        cg_gameState.stringOffsets, cfgIndex);
    cfg = (const char *)(
        (uintptr_t)(const void *)cg_gameState.stringData +
        (uintptr_t)(intptr_t)stringOffset);

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (cfg[0] == '\0' || cfg[1] == '\0' ||
        (unsigned char)cfg[0] < (unsigned char)'0' ||
        (unsigned char)cfg[0] > (unsigned char)'9' ||
        (unsigned char)cfg[1] < (unsigned char)'0' ||
        (unsigned char)cfg[1] > (unsigned char)'9') {
        Com_Printf(
            "WARNING: CG_PlayFxOnTag: invalid effect config string %i\n",
            cfgIndex);
        return;
    }

    d0 = (signed char)cfg[0] - '0';
    d1 = (signed char)cfg[1] - '0';
    id = d1 + d0 * 10;
    if (id < 1 || id >= CS_EFFECTS_COUNT) {
        Com_Printf(
            "WARNING: CG_PlayFxOnTag: invalid effect id %i in config string %i\n",
            id, cfgIndex);
        return;
    }
    handle = cg_effectDefs[id];
    tagName = cfg + 2;

    boltInfo.entityNum = self->currentState.number;

    /* Resolve the named tag on the owning object; negative means "no such tag". */
    boltInfo.boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_RESOLVE_TAG, self->currentState.number, (intptr_t)tagName));
    if (boltInfo.boneIndex < 0) {
        return;
    }

    /* Play the tag-bound effect at the spawner's origin. The trailing by-address
     * argument is the 8-byte { entityNum, boneIndex } record at [ESP+8]; the
     * engine consumes both words. */
    cgame_syscall(CG_PLAY_EFFECT_ON_TAG,
                  coduo_int32_from_bits(handle),
                  (intptr_t)self->lerpOrigin,
                  0,
                  (intptr_t)&boltInfo);
}
