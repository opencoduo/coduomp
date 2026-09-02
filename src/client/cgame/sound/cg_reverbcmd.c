// Source: uo_cgame_mp_x86.dll 0x3003aa70..0x3003ab6d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003aa70_3003ab6d.mcode
//
// CG_ReverbCmd — the "cg_reverb"/"reverb"-style console command handler that
// forwards an environmental-reverb request to the engine sound system.
//
// Name evidence: the embedded .rdata error string decides it outright —
//   0x30079f58 "ERROR: CG_ReverbCmd called with %i args (should be 4)\n"
// so the mechanical size-matched guess `script_func_getweaponclassname`
// (win size 0xfd, matched size 0xfd) is REJECTED: this function parses console
// argv floats and issues a sound trap — it is not a weapon-class-name lookup and
// touches no weaponInfo_t/weaponClass table or string return. A same-module PPC row
// (cgame_mp.dll!CG_ReverbCmd) corroborates the symbol.
//
// Machine-code notes (all behavior below is traced to the .mcode / objdump):
//   - trap_Argc()  == cgame_syscall(12)          -> EAX = console argument count
//   - trap_Argv()  == cgame_syscall(13, n, buf, len) -> copy argv[n] into buffer
//   - atof (0x3005b969)                          -> parse a numeric substring
//   - Com_PrintMessage (0x3002b2b0)              -> variadic print backend
//   - cgame_syscall(CG_MSS_SET_ENVIRONMENT_EFFECTS == 0xdd, name, a, b) -> 3-arg cgame sound trap
//   - g_textScratchBuffer == 0x300da488[1024]    -> shared 1024-byte text buffer
//
// The three argv tokens are consumed in the order 2, 3, 1 (the exact push order
// of the trap_Argv id operands: 0x2 at 0x3003aa9f, 0x3 at 0x3003aac4, 0x1 at
// 0x3003aae9). Each trap_Argv overwrites g_textScratchBuffer, so argv[2] and
// argv[3] must be converted to float BEFORE the next token is fetched; only
// argv[1]'s raw string survives in the buffer to be handed to the trap as the
// reverb-preset name.
//
//   - argv[2] -> float, kept as its raw bit pattern (stored via FSTP float at
//     frame+0xc, then reloaded as an int32 with MOV ECX and forwarded to the
//     trap; the engine reinterprets arg 2 as a float). Modeled with CG_FloatBits.
//   - argv[3] -> float, multiplied by 1000.0f (.rdata 0x3007be88 = 0x447a0000;
//     the adjacent 0x3007be84 holds 0.85f, which this function does NOT load),
//     then rounded to
//     the nearest int with the pervasive x87 FISTP idiom (FADD double 2^-30 =
//     CG_FTOL_EPSILON at .rdata 0x3007be50, then FISTP). Negative results are
//     clamped to 0 (TEST EAX,EAX / JGE at 0x3003ab20..0x3003ab22; the JL arm does
//     XOR EAX,EAX). The >=0 arm at 0x3003ab28 re-rounds the identical value from
//     frame+0x8 (a duplicated-tail codegen of the same round) and yields the same
//     integer.
//   - argv[1] -> left in g_textScratchBuffer and passed as the trap's name arg.
//
// Final trap (0x3003ab60): cgame_syscall(0xdd, g_textScratchBuffer,
//   CG_FloatBits(argv2), roundedArgv3). The two RETs each restore the 0x1c-byte
// local frame (ADD ESP,0x1c) after cleaning their own pushed args (caller-clean).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_ReverbCmd(void)
{
    /* PUSH 0xc / CALL *cgame_syscall / ADD ESP,4 ; CMP EAX,4 / JZ 0x3003aa95 */
    int32_t argc = trap_Argc();
    if (argc != 4) {
        /* PUSH EAX (argc) / PUSH fmt / CALL Com_PrintMessage / ADD ESP,8 / RET */
        Com_PrintMessage("ERROR: CG_ReverbCmd called with %i args (should be 4)\n",
                         argc);
        return;
    }

    /* trap_Argv(2, g_textScratchBuffer, 1024) then atof -> narrowed to float by
     * FSTP DWORD [ESP+0x20] (frame+0xc). The float's raw bits are what the trap
     * ultimately receives as its second argument. */
    trap_Argv(2, g_textScratchBuffer, sizeof(g_textScratchBuffer));
    float reverbParam = (float)atof(g_textScratchBuffer);

    /* trap_Argv(3, g_textScratchBuffer, 1024) then atof -> narrowed to float by
     * FSTP DWORD [ESP+0x14] (frame+0x0). */
    trap_Argv(3, g_textScratchBuffer, sizeof(g_textScratchBuffer));
    float argv3 = (float)atof(g_textScratchBuffer);

    /* trap_Argv(1, g_textScratchBuffer, 1024): argv[1] stays in the buffer as the
     * reverb-preset name string handed to the trap below. No atof for this one. */
    trap_Argv(1, g_textScratchBuffer, sizeof(g_textScratchBuffer));

    /* FLD float [frame+0x0] (argv3) ; FMUL float 1000.0f (.rdata 0x3007be88 =
     * 0x447a0000) ; FST/FSTP into frame+0x8 and frame+0x0. */
    float scaled = argv3 * 1000.0f;

    /* FLD double 2^-30 ; FADD ; FISTP -> round-to-nearest int, then clamp <0 to 0
     * (TEST EAX,EAX / JGE ; JL arm: XOR EAX,EAX). The >=0 arm re-rounds the same
     * value; both arms produce CG_RoundToNearest(scaled). */
    int32_t roundedGain = CG_RoundToNearest(scaled);
    if (roundedGain < 0) {
        roundedGain = 0;
    }

    /* MOV ECX,[frame+0xc] (argv[2]'s float bits) ; PUSH roundedGain ; PUSH ECX ;
     * PUSH g_textScratchBuffer ; PUSH 0xdd ; CALL *cgame_syscall ; ADD ESP,0x10.
     * The engine reads arg 2 as a float, so we forward argv[2]'s bit pattern. */
    cgame_syscall(CG_MSS_SET_ENVIRONMENT_EFFECTS,
                  (intptr_t)g_textScratchBuffer,
                  CG_FloatBits(reverbParam),
                  roundedGain);
}
