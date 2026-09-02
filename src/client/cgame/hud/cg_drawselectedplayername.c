// Source: uo_cgame_mp_x86.dll 0x30031020..0x300310aa
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30031020_300310aa.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Layout guards proving the machine-code offsets/stride this function relies on
 * (verified at 4-byte i386 pointer width). The 0x4d0 element stride matches
 * `IMUL EAX,EAX,0x4d0` at 0x30031042; infoValid +0x00 matches `CMP [EAX],0`
 * at 0x3003104d; the string slot pointer is &state + 0xc (`ADD EAX,0xc` at
 * 0x30031086); obj->y / obj->h match `FADD [ECX+4]` / `FLD [ECX+0xc]`; bits(obj->x)
 * matches `MOV ECX,[ECX]`. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(clientInfo_t) == 0x4d0, "bgs.clientinfo element stride");
#endif
_Static_assert(offsetof(clientInfo_t, infoValid) == 0x00, "infoValid +0x00");
_Static_assert(offsetof(rectDef_t, x) == 0x00, "obj->x +0x00");
_Static_assert(offsetof(rectDef_t, y) == 0x04, "obj->y +0x04");
_Static_assert(offsetof(rectDef_t, h) == 0x0c, "obj->h +0x0c");

/*
 * CG_DrawSelectedPlayerName (0x30031020) — an iterator-driven member of the
 * CG_R_TEXT_PAINT HUD emit family. Instead of using the local player's clientNum (as
 * CG_DrawPlayerLocation, 0x30031280, does), it advances/clamps the shared HUD
 * emit cursor (cg_currentSelectedPlayer_vmCvar.integer), maps it through cg_hudEmitClientTable[] to a
 * per-client bgs.clientinfo[] index, early-outs if that state is empty, then
 * emits one cgame trap 54 whose "string" slot is a POINTER INTO that per-client
 * anim state (&state + 0xc) rather than a config string.
 *
 * Cursor clamp (identical preamble to the two family siblings that share these
 * globals — CG_HudEmitIconOrValue at 0x30030f10 and the
 * config-string emitter at 0x300311f0):
 *   idx = cg_currentSelectedPlayer_vmCvar.integer;                     // MOV EAX,[0x3044f60c]
 *   if (idx < 0 || idx >= cg_hudEmitCount)      // TEST/JL ; CMP [0x305385e0]/JL (signed)
 *       cg_currentSelectedPlayer_vmCvar.integer = idx = 0;             // XOR EAX,EAX ; MOV [0x3044f60c],EAX
 *   state = &bgs.clientinfo[cg_hudEmitClientTable[idx]];
 *                                               // MOV EAX,[EAX*4 + 0x305384c0]
 *                                               // IMUL EAX,EAX,0x4d0 ; ADD EAX,0x305e1f34
 *   if (state->infoValid == 0) return;    // CMP [EAX],0 ; JZ epilogue
 *
 * NOTE ON THE SIGNED-RANGE TEST: the first branch is `TEST EAX,EAX; JL reset`
 * (idx < 0) and the second is `CMP EAX,[cg_hudEmitCount]; JL keep` (idx <
 * count) — a signed compare, so out-of-range (idx < 0 OR idx >= count) collapses
 * to slot 0. cg_hudEmitClientTable[]/cg_currentSelectedPlayer_vmCvar.integer/cg_hudEmitCount are int32.
 *
 * Register-argument ABI (non-default, matches the family): the emit object arrives
 * in ECX (loaded here as `FLD [ECX+0xc]`, `FADD [ECX+4]`, `MOV ECX,[ECX]`), and
 * four cdecl stack words A0..A3 follow. The entry `SUB ESP,0x10` / matching
 * `ADD ESP,0x10` is frame scratch; the `ADD ESP,0x28` before it unwinds the 10
 * dwords pushed for the syscall (caller-cleaned VM trap). The function ends in a
 * plain RET, so ECX is modelled as the leading pointer parameter `obj`.
 *
 * The emitted 10-slot syscall vector (proven by the push order 0x30031066..0x3003109b,
 * lowest syscall arg == last-pushed):
 *   cgame_syscall(CG_R_TEXT_PAINT,   // PUSH 0x36
 *                 bits(obj->x), // stored at 0x30031071 and reloaded at 0x30031094
 *                 sumBits,      // bits of obj->h + obj->y (single x87 FADD)
 *                 arg0, arg1, arg2,
 *                 state->name, // ADD EAX,0xc ; PUSH EAX -> in-place client name
 *                 0, 0,
 *                 arg3);
 *
 * word0 NOTE: at 0x3003105c the code loads bits(obj->x) (MOV ECX,[ECX]) and stores it
 * at S+0xc via MOV [ESP+0x14],ECX at 0x30031071. After six intervening pushes,
 * MOV EAX,[ESP+0x24] at 0x30031094 addresses that same S+0xc slot, and the following
 * PUSH EAX forwards it as the first data argument. The separate scratch-zero slot
 * at S+0x0 supplies one of the two later zero arguments.
 *
 * Name adjudication: the .mcode header's size-matched
 * "trap_XAnimSetCompleteGoalWeightKnobAll" guess is REJECTED — this function issues a
 * single cgame trap id 54 and does no XAnim goal-weight/knob work; it is the
 * selected-player-name member of the text-paint family. Retail UO routes both
 * CG_SELECTEDPLAYER_NAME and CG_VOICE_NAME here, and the macOS owner-draw jump
 * table routes both ids to CG_DrawSelectedPlayerName, establishing the exact name.
 */
void CG_DrawSelectedPlayerName(rectDef_t *obj, intptr_t arg0, intptr_t arg1,
                               intptr_t arg2, intptr_t arg3)
{
    int32_t idx = cg_currentSelectedPlayer_vmCvar.integer;

    /* Out-of-range (idx < 0 OR idx >= count, signed) -> reset to slot 0 and store
     * back. */
    if (idx < 0 || idx >= cg_hudEmitCount) {
        idx = 0;
        cg_currentSelectedPlayer_vmCvar.integer = 0;
    }

    int32_t clientNum = cgame_compat_read_target_i32_index(
        cg_hudEmitClientTable, idx);
    clientInfo_t *state = cgame_compat_unchecked_clientinfo(
        &bgs.clientinfo[0], clientNum);

    /* No valid per-client state at this table slot -> emit nothing. */
    if (state->infoValid == 0)
        return;

    /* bits(obj->x) (0x3003105c MOV ECX,[ECX]) is the FIRST data argument (a1) of the
     * CG_R_TEXT_PAINT syscall below: it is stashed to a stack slot at 0x30031071 and
     * reloaded as the last data push (0x30031094 MOV EAX,[ESP+0x24] / 0x3003109a PUSH
     * EAX, i.e. a1). A prior pass treated it as a dead read and passed literal 0 as a1
     * (that literal 0 actually belongs to a7, where 0 is also passed). */
    int32_t xBits = CG_FloatBits(obj->x);

    float sum = obj->h + obj->y;
    int32_t sumBits;
    /* the float sum is forwarded through a plain 32-bit syscall slot as its bit
     * pattern (single-precision throughout: one x87 FADD). */
    memcpy(&sumBits, &sum, sizeof(sumBits));

    /* String slot is the ADDRESS of the iterated player's in-place name buffer
     * (state->name at +0xc; ADD EAX,0xc; PUSH EAX), not a config string.
     * This is the one genuine address-of-a-subobject-through-an-int-slot idiom: the
     * pointer is handed through a 32-bit syscall slot, so it is laundered to int. */
    const char *name = state->name;

    cgame_syscall(CG_R_TEXT_PAINT,
                  xBits,
                  sumBits,
                  arg0,
                  arg1,
                  arg2,
                  (intptr_t)name,
                  0,
                  0,
                  arg3);
}
