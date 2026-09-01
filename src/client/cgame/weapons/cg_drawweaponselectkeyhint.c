// Source: uo_cgame_mp_x86.dll 0x30046a40..0x30046ba7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30046a40_30046ba7.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_DrawWeaponSelectKeyHint (0x30046a40)
 *
 * Draws the "which key selects this weapon slot" hint for one entry of the
 * weapon-select HUD: it looks up the key currently bound to the slot's
 * "weaponslot <name>" command and emits two overlapping 2D draw elements
 * (cgame trap 54) that render the key label, then resets the 2D draw color
 * (trap 72 / trap_R_SetColor).
 *
 * Name adjudication: the .mcode "# name Scr_Objective_Current" is REJECTED. That
 * is a server-side game_mp script command (script_objectives.c) taking no args and
 * mutating objective state; it was assigned purely by a byte-size match (win 0x167
 * vs corpus 0x168), which the naming rules forbid. This function does no objective
 * work: it formats va("weaponslot %s", bg_weaponSlotNames[slot]), resolves the bound
 * key string via BindingFromName, and issues cgame draw traps 54/54/72. It is a
 * client cgame HUD helper. The name below reflects that proven behavior; the exact
 * original CoD symbol is unresolved (no cgame syscall-id/name table recovered).
 *
 * ABI (i386), proven from the machine code and the sole caller at 0x30047320:
 *   - params  : a pointer in EDI to a small draw-params struct (only +0x0c is
 *               read, a float forwarded verbatim into both draw blocks); a slot
 *               index in ECX; and two stack floats. The caller sets EDI = &local,
 *               ECX = slot index, then pushes x then 8.0f (so x = stack arg 0,
 *               y = stack arg 1 = 8.0f at the observed call).
 *   - The body runs SUB ESP,0x30 / PUSH ESI at entry and unwinds with
 *     ADD ESP,0x54 / POP ESI / ADD ESP,0x30 / RET (caller-cleaned, no RET imm).
 *
 * Key-string lookup:
 *   name = bg_weaponSlotNames[slot]         (MOV EAX,[ECX*4 + 0x30084310])
 *   text = va(cg_weaponSlotCvarNameFormat, name)        (PUSH name; PUSH fmt; CALL va;
 *                                            ADD ESP,8 cleans only these two)
 *   key  = BindingFromName(text, 1)         (EAX = text in; the leftover PUSH 1 is
 *                                            firstKeyOnly; const char * out in EAX)
 *
 * The two trap-54 draws share the same fixed 10-dword shape (id + 9 args), proven
 * push-by-push:
 *   cgame_syscall(54, xBits, yBits, 5, 0.25f, &block, key, 6.0f, 0, 0)
 * with the float args forwarded as raw 32-bit words (the i386 code FSTPs each into
 * a stack slot and PUSHes the dword; CG_FloatBits reproduces that exactly).
 *
 *   draw 1 (0x30046afb): x = paramX + 1.0f
 *                        y = paramY + 1.0f - 4.0f + 9.6f  ( = paramY + 6.6f )
 *                        block = { 0.0f, 0.0f, 0.0f, params->f0c, 1,1,1,1 }
 *   draw 2 (0x30046b8d): x = paramX               (no +1.0 bias)
 *                        y = paramY - 4.0f + 9.6f  ( = paramY + 5.6f )
 *                        block = { 1.0f, 1.0f, 1.0f, params->f0c, 1,1,1,1 }
 *
 * The block is an 8-dword parameter/color group passed to the trap by address.
 * Only its +0x0c word differs from a constant (it carries params->f0c, read once
 * at entry); slot +0x0c is NOT rewritten between the two draws, so both draws see
 * the same value there. The leading three words differ (0,0,0 vs 1,1,1) — a
 * two-pass "shadow then foreground" style typical of a HUD glyph/key draw. Exact
 * field meaning of the block is unproven (the trap-54 engine service is unknown),
 * so the block is a local float[8] matching the machine-code stores.
 *
 * Float .rdata constants used (dumped at their exact addresses):
 *   0x3007bce0 = 1.0f, 0x3007be40 = 4.0f, 0x3007c080 = 9.600000381f
 * Immediate float words: 0x40c00000 = 6.0f, 0x3e800000 = 0.25f, 0x3f800000 = 1.0f.
 *
 * Final: cgame_syscall(72, params) — trap_R_SetColor (CG_R_SETCOLOR) with the EDI
 * pointer as its single argument, resetting/setting the 2D draw color after the
 * two draws (PUSH EDI; PUSH 0x48; CALL cgame_syscall; ADD ESP,8).
 */

/* Trap-54 fixed draw parameters, from the pushed immediates. */
enum { CG_WSHINT_STYLE = 5 };          /* PUSH 5 (int)                       */
#define CG_WSHINT_SCALE  0.25f         /* 0x3e800000                         */
#define CG_WSHINT_SIZE   6.0f          /* 0x40c00000                         */

/* Draw-params struct pointed to by EDI. Only +0x0c is read by this function
 * (forwarded into both draw blocks); the surrounding fields are opaque here. */
void CG_DrawWeaponSelectKeyHint(const vec4_t params,
                                int32_t slot,
                                float x,
                                float y)
{
    const char *name;
    const char *key;
    float block[8];
    float shared0c;

    /* Saved once at entry (MOV EAX,[EDI+0xc]; MOV [ESP+0x20],EAX). Both draw
     * blocks read this value at block[+0x0c]; it is not rewritten between draws. */
    shared0c = params[3];

    /* name = bg_weaponSlotNames[slot]; text = va(cg_weaponSlotCvarNameFormat, name). */
    name = bg_weaponSlotNames[slot];
    key = va(cg_weaponSlotCvarNameFormat, name);

    /* key display string; EAX = text in, const char * out. The trailing PUSH 1 at
     * the call site is BindingFromName's firstKeyOnly flag (show only key 1). */
    key = BindingFromName(key, qtrue);

    /* ---- draw 1: shadow pass (leading three block words = 0) ---- */
    block[0] = 0.0f;
    block[1] = 0.0f;
    block[2] = 0.0f;
    block[3] = shared0c;
    block[4] = 1.0f;
    block[5] = 1.0f;
    block[6] = 1.0f;
    block[7] = 1.0f;

    float shadowX = (float)((long double)x + (long double)1.0f);
    float shadowY = (float)((((long double)y + (long double)1.0f) -
                              (long double)4.0f) +
                             (long double)9.600000381469727f);

    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(shadowX),
                  CG_FloatBits(shadowY),
                  CG_WSHINT_STYLE,
                  CG_FloatBits(CG_WSHINT_SCALE),
                  (intptr_t)block,
                  (intptr_t)key,
                  CG_FloatBits(CG_WSHINT_SIZE),
                  0,
                  0);

    /* ---- draw 2: foreground pass (leading three block words = 1.0) ----
     * block[+0x0c] (index 3) is left holding shared0c from the first pass. */
    block[0] = 1.0f;
    block[1] = 1.0f;
    block[2] = 1.0f;
    block[4] = 1.0f;
    block[5] = 1.0f;
    block[6] = 1.0f;
    block[7] = 1.0f;

    float foregroundY = (float)(((long double)y - (long double)4.0f) +
                                (long double)9.600000381469727f);

    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(x),
                  CG_FloatBits(foregroundY),
                  CG_WSHINT_STYLE,
                  CG_FloatBits(CG_WSHINT_SCALE),
                  (intptr_t)block,
                  (intptr_t)key,
                  CG_FloatBits(CG_WSHINT_SIZE),
                  0,
                  0);

    /* trap_R_SetColor(params): reset the 2D draw color after the draws. */
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)params);
}
