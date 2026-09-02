// Source: uo_cgame_mp_x86.dll 0x300311f0..0x3003127f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300311f0_3003127f.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Layout guards proving the machine-code offsets/stride this function relies on
 * (verified at 4-byte i386 pointer width). The 0x4d0 element stride matches
 * `IMUL EAX,EAX,0x4d0` at 0x30031212; infoValid +0x00 matches `CMP [EAX],0`
 * at 0x3003121d; location +0x38 matches `MOV EAX,[EAX+0x38]` at 0x30031222;
 * obj->h / obj->y match `FLD [ESI+0xc]` / `FADD [ESI+0x4]`; bits(obj->x) matches
 * `MOV EDX,[ESI]`. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(clientInfo_t) == 0x4d0, "bgs.clientinfo element stride");
#endif
_Static_assert(offsetof(clientInfo_t, infoValid) == 0x00, "infoValid +0x00");
_Static_assert(offsetof(clientInfo_t, location) == 0x38, "location +0x38");
_Static_assert(offsetof(rectDef_t, x) == 0x00, "obj->x +0x00");
_Static_assert(offsetof(rectDef_t, y) == 0x04, "obj->y +0x04");
_Static_assert(offsetof(rectDef_t, h) == 0x0c, "obj->h +0x0c");

/*
 * CG_DrawSelectedPlayerLocation (0x300311f0) — iterator-driven member of the
 * CG_R_TEXT_PAINT HUD emit family and the exact sibling of
 * CG_DrawSelectedPlayerName (0x30031020).
 * It shares that sibling's cursor/table preamble but, unlike it, forwards bits(obj->x)
 * (not a constant 0) in the first data slot and resolves the "string" slot through
 * CG_GetTranslatedLocationString(state->location) instead of pushing a name pointer
 * into the per-client anim state. Its body from the FLD onward is byte-for-byte identical to
 * CG_DrawPlayerLocation (0x30031280); the difference from that local-player member
 * is only the preamble (shared HUD emit cursor/table vs. cg_snap->ps.psClientNum).
 *
 * Cursor clamp (identical preamble to CG_DrawSelectedPlayerName 0x30031020 and the
 * precacheHeadIcon script function at 0x30030f10, which share these globals):
 *   idx = cg_currentSelectedPlayer_vmCvar.integer;                     // MOV EAX,[0x3044f60c]
 *   if (idx < 0 || idx >= cg_hudEmitCount)      // TEST/JL ; CMP [0x305385e0]/JL (signed)
 *       cg_currentSelectedPlayer_vmCvar.integer = idx = 0;             // XOR EAX,EAX ; MOV [0x3044f60c],EAX
 *   state = &bgs.clientinfo[cg_hudEmitClientTable[idx]];
 *                                               // MOV EAX,[EAX*4 + 0x305384c0]
 *                                               // IMUL EAX,EAX,0x4d0 ; ADD EAX,0x305e1f34
 *   if (state->infoValid == 0) return;    // CMP [EAX],0 ; JZ epilogue
 *   name = CG_GetTranslatedLocationString(state->location);
 *                                               // MOV EAX,[EAX+0x38] ; CALL 0x300310b0
 *
 * SIGNED-RANGE TEST: the first branch is `TEST EAX,EAX; JL reset` (idx < 0) and the
 * second is `CMP EAX,[cg_hudEmitCount]; JL keep` (idx < count, signed), so an
 * out-of-range cursor (idx < 0 OR idx >= count) collapses to slot 0.
 * cg_hudEmitClientTable[]/cg_currentSelectedPlayer_vmCvar.integer/cg_hudEmitCount are int32.
 *
 * Register-argument ABI (non-default, matches the family): the emit object arrives
 * in ESI (loaded here as `FLD [ESI+0xc]`, `FADD [ESI+4]`, `MOV EDX,[ESI]`), and four
 * cdecl stack words A0..A3 follow. The entry `SUB ESP,0x10` / matching `ADD ESP,0x10`
 * is frame scratch; the `ADD ESP,0x28` before it unwinds the 10 dwords pushed for the
 * syscall (caller-cleaned VM trap). The function ends in a plain RET, so ESI is
 * modelled as the leading pointer parameter `obj`.
 *
 * The emitted 10-slot syscall vector (proven by the push order 0x30031242..0x30031270,
 * lowest syscall arg == last-pushed; the shuffle is byte-identical to
 * CG_DrawPlayerLocation, whose slot mapping this reuses):
 *   cgame_syscall(CG_R_TEXT_PAINT,   // PUSH 0x36
 *                 bits(obj->x),   // [ESI+0x00] raw dword, stored to scratch and pushed
 *                 sumBits,      // bits of obj->h + obj->y (single x87 FADD)
 *                 arg0, arg1, arg2,
 *                 name,         // CG_GetTranslatedLocationString(state->location)
 *                 0, 0,
 *                 arg3);
 *
 * DIFFERENCE FROM 0x30031020: both siblings push bits(obj->x) in the first data
 * slot. That sibling pushes &state[+0xc] in the string slot, whereas this function
 * resolves the string slot through CG_ConfigStringHint.
 *
 * Name adjudication: the .mcode header's size-matched "G_EntLinkToWithOffset" guess is
 * REJECTED — this function issues a single cgame trap id 54 and does no entity
 * linking/offset work; it is the selected-player-location member of the text-paint
 * family. Retail UO assigns this case CG_SELECTEDPLAYER_LOCATION, and the macOS
 * owner-draw jump table names its target CG_DrawSelectedPlayerLocation.
 * CG_GetTranslatedLocationString (0x300310b0) adds the CS_LOCATIONS base,
 * resolves the config string, and applies the string-editor translation policy.
 */
void CG_DrawSelectedPlayerLocation(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
    int32_t idx = cg_currentSelectedPlayer_vmCvar.integer;

    /* Out-of-range (idx < 0 OR idx >= count, signed) -> reset to slot 0 and store
     * back. */
    if (idx < 0 || idx >= cg_hudEmitCount) {
        idx = 0;
        cg_currentSelectedPlayer_vmCvar.integer = 0;
    }

    int32_t clientNum = cgame_compat_read_target_i32_index(cg_hudEmitClientTable, idx);
    clientInfo_t *state = cgame_compat_unchecked_clientinfo(&bgs.clientinfo[0], clientNum);

    /* No valid per-client state at this table slot -> emit nothing. */
    if (state->infoValid == 0)
        return;

    const char *hint = CG_GetTranslatedLocationString(state->location);

    float sum = obj->h + obj->y;
    int32_t sumBits;
    /* the float sum is forwarded through a plain 32-bit syscall slot as its bit
     * pattern (single-precision throughout: one x87 FADD). */
    memcpy(&sumBits, &sum, sizeof(sumBits));

    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(obj->x), sumBits, arg0, arg1, arg2, (intptr_t)hint, 0, 0, arg3);
}
