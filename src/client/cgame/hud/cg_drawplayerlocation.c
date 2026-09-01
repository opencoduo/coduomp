// Source: uo_cgame_mp_x86.dll 0x30031280..0x300312fc
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031280_300312fc.mcode

#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Layout guards proving the machine-code offsets/stride this function relies on
 * (verified at 4-byte i386 pointer width). The 0x4d0 element stride matches the
 * `IMUL EAX,EAX,0x4d0` at 0x3003128b; the field offsets match the [EAX]/[EAX+0x38]
 * and [cg_snap+0xe0] accesses. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(clientInfo_t) == 0x4d0, "bgs.clientinfo element stride");
#endif
_Static_assert(offsetof(clientInfo_t, infoValid) == 0x00, "infoValid +0x00");
_Static_assert(offsetof(clientInfo_t, location) == 0x38, "location +0x38");
_Static_assert(offsetof(snapshot_t, ps.psClientNum) == 0xe0, "cg_snap->ps.psClientNum +0xe0");
_Static_assert(offsetof(rectDef_t, x) == 0x00, "obj->x +0x00");
_Static_assert(offsetof(rectDef_t, y) == 0x04, "obj->y +0x04");
_Static_assert(offsetof(rectDef_t, h) == 0x0c, "obj->h +0x0c");

/*
 * CG_DrawPlayerLocation (0x30031280) — a member of the CG_R_TEXT_PAINT emitter family
 * that emits the LOCAL player's current HUD-hint string. Unlike the object-driven
 * area-chat trio (0x30031940/0x300319a0/0x30031a00), this one fetches the local
 * player's per-client anim/player state, and if that state is valid, looks up the
 * hint's config string and forwards it through trap 54 together with a small vector
 * built from the register-passed object and four caller stack words.
 *
 * Behavior (all offsets proven against the .mcode):
 *   ps = &bgs.clientinfo[cg_snap->ps.psClientNum]   // stride 0x4d0, base 0x305e1f34
 *   if (ps->infoValid == 0) return;          // no valid local state -> emit nothing
 *   name = CG_GetTranslatedLocationString(ps->location); // callee 0x300310b0
 *   cgame_syscall(CG_R_TEXT_PAINT,
 *                 bits(obj->x),                       // [ESI+0x00] raw dword
 *                 <bits of (obj->h + obj->y)>,  // FLD [ESI+0xc]; FADD [ESI+4] (float)
 *                 arg0, arg1, arg2,                 // three caller stack words, in order
 *                 name,                             // config-string pointer
 *                 0, 0,
 *                 arg3);                            // fourth caller stack word in the tail slot
 *
 * Register-argument ABI (non-default, proven from the sole caller at 0x30032335):
 * the object pointer arrives in ESI (the caller sets it with `LEA ESI,[ESP+0x1c]`
 * to a local rectDef_t, then pushes four cdecl dwords and cleans them with
 * `ADD ESP,0x10`). The function ends in a plain RET; the `ADD ESP,0x28` before it
 * only unwinds the 10 dwords pushed for the syscall, and the entry `SUB ESP,0x10`
 * plus its matching `ADD ESP,0x10` are this frame's scratch space. ESI is modelled
 * here as the leading pointer parameter `obj`.
 *
 * Name adjudication: the .mcode header's size-matched "PM_WaterEvents" guess is
 * REJECTED — PM_WaterEvents does pmove water-level event bookkeeping and issues no
 * engine syscall; this function makes exactly one cgame trap (id 54) and looks a
 * string up by config-string index. Retail UO assigns this owner-draw case
 * CG_PLAYER_LOCATION, and the macOS owner-draw jump table names its target
 * CG_DrawPlayerLocation. CG_GetTranslatedLocationString (0x300310b0) adds the
 * CS_LOCATIONS base, resolves the config string, and applies the string-editor
 * translation policy.
 *
 * Instruction map (frame base S = ESP right after SUB ESP,0x10; return addr at S+0x10;
 * incoming stack args A0..A3 at S+0x14/S+0x18/S+0x1c/S+0x20):
 *   30031280 MOV  EAX,[0x30459160]       EAX = cg_snap
 *   30031285 MOV  EAX,[EAX+0xe0]         EAX = cg_snap->ps.psClientNum
 *   3003128b IMUL EAX,EAX,0x4d0          EAX = clientNum * sizeof(playerState_t)
 *   30031291 ADD  EAX,0x305e1f34         EAX = &bgs.clientinfo[clientNum]
 *   30031296 MOV  ECX,[EAX]              ECX = ps->infoValid
 *   30031298 SUB  ESP,0x10               (frame scratch)
 *   3003129b TEST ECX,ECX / 3003129d JZ 0x300312f8   if (infoValid == 0) -> return
 *   3003129f MOV  EAX,[EAX+0x38]         EAX = ps->location
 *   300312a2 CALL 0x300310b0             EAX = translated location text
 *   300312a7 FLD  [ESI+0xc]              st0 = obj->h                        (float)
 *   300312ae FADD [ESI+0x4]             st0 = obj->h + obj->y             (float add)
 *   300312b1 MOV  EDX,[ESI]              EDX = bits(obj->x)                      (raw dword)
 *   300312bb FSTP [scratch]             store the float sum
 *   ...  the compiler shuffles A0..A3, bits(obj->x), the sum bits, and the config-string
 *        pointer through scratch slots and pushes them in reverse call order ...
 *   300312ed PUSH 0x36                   command id 54 (CG_R_TEXT_PAINT)
 *   300312ef CALL *0x30085e9c            cgame_syscall(54, ...)
 *   300312f5 ADD  ESP,0x28               unwind the 10 pushed dwords
 *   300312f8 ADD  ESP,0x10 ; RET
 *
 * The float sum is forwarded through a plain 32-bit syscall slot as its bit pattern;
 * it is produced by a single x87 FADD (float precision throughout), so it is
 * reconstructed as `float` and passed as raw bits.
 */
void CG_DrawPlayerLocation(rectDef_t *obj, intptr_t arg0, intptr_t arg1,
                           intptr_t arg2, intptr_t arg3)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t clientNum = cg_snap->ps.psClientNum;
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_DrawPlayerLocation: invalid client number %i",
                  clientNum);
        return;
    }
    clientInfo_t *ps = &bgs.clientinfo[clientNum];

    /* No valid local player state yet -> emit nothing. */
    if (ps->infoValid == 0)
        return;

    const char *hint = CG_GetTranslatedLocationString(ps->location);

    float sum = obj->h + obj->y;
    int32_t sumBits;
    /* the sum is forwarded through a plain 32-bit syscall slot as its bit pattern */
    memcpy(&sumBits, &sum, sizeof(sumBits));

    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(obj->x),
                  sumBits,
                  arg0,
                  arg1,
                  arg2,
                  (intptr_t)hint,
                  0,
                  0,
                  arg3);
}
