// Source: uo_cgame_mp_x86.dll 0x3003b470..0x3003b4a6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003b470_3003b4a6.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_ExecuteNewServerCommands (0x3003b470) — the id-Tech cgame server-command drain.
 *
 * NAMING: the mechanical .mcode header guessed `Q_rint` PURELY by a size match
 * (win 0x36 == corpus 0x36). That is rejected — the real Q_rint (a float-to-int
 * round) is already reconstructed at a DIFFERENT address (0x3006be3c) and does no
 * cvar/trap/global work. This body performs no rounding whatsoever: it advances a
 * global command counter and issues a syscall per step. The behavior is the exact
 * stock-Q3 CG_ExecuteNewServerCommands loop.
 *
 * ARGUMENT (register convention): `latestSequence` arrives in ESI (the caller at
 * 0x3003cc79 does `mov esi,[edi+0x1501c]` immediately before `call 0x3003b470`, and
 * this function only reads ESI, never writes/saves it). It is the highest reliable
 * server-command sequence number the client has received.
 *
 * BODY, instruction by instruction:
 *   3003b470  push ecx / (3003b4a4) pop ecx : one scratch dword of stack; the value
 *                                             is never used — pure frame padding,
 *                                             not an argument. Modeled as nothing.
 *   3003b471  call 0x3003a810   : CG_CheckOpenWaitingScriptMenu() — reconcile the
 *                                 pending ui_waitingScriptMenu/ui_newScriptMenu cvars
 *                                 once before draining commands (CoD:UO addition; not
 *                                 in stock Q3, but placed here in the same routine).
 *   3003b476  mov eax,[0x30447ab0] : eax = cgs.serverCommandSequence (the highest cmd
 *                                    already executed).
 *   3003b47b  cmp eax,esi / 3003b47d jge 0x3003b4a4 : SIGNED compare; if the executed
 *                                    sequence is already >= latestSequence, nothing
 *                                    new to do — return.
 *   loop (3003b480..3003b4a2):
 *     3003b480  inc eax               : ++seq (pre-increment before fetch).
 *     3003b481  push eax              : syscall arg = the seq to fetch.
 *     3003b482  push 0x52             : CG_GET_SERVER_COMMAND (trap_GetServerCommand).
 *     3003b484  mov [0x30447ab0],eax  : cgs.serverCommandSequence = seq (store BEFORE
 *                                       the call — so a re-entrant/error path sees it).
 *     3003b489  call [0x30085e9c]     : cgame_syscall(0x52, seq) -> qboolean in EAX.
 *     3003b48f  add esp,8             : cdecl caller cleanup of the 2 pushed dwords.
 *     3003b492  test eax,eax / 3003b494 jz 0x3003b49b : if the command was available,
 *     3003b496  call 0x3003ac90       :   CG_ServerCommand() dispatches it (it reads
 *                                          the fetched argv itself via trap 0xd).
 *     3003b49b  mov eax,[0x30447ab0]  : reload seq (the two calls clobber EAX).
 *     3003b4a0  cmp eax,esi / 3003b4a2 jl 0x3003b480 : SIGNED; keep looping while
 *                                       seq < latestSequence.
 *   3003b4a5  ret : plain RET (no imm) — caller-cleaned/register-arg convention.
 *
 * Signedness: both the initial gate (JGE) and the loop-back test (JL) are signed, so
 * the sequence numbers are treated as signed int32 (matching cgs.serverCommandSequence,
 * an int32). The store-before-call ordering is preserved exactly.
 *
 * cgame_syscall returns int32; here it is used only as a truthiness gate, so the
 * result is compared against zero (qboolean semantics for CG_GET_SERVER_COMMAND).
 */
void CG_ExecuteNewServerCommands(int32_t latestSequence)
{
    /* 0x3003b471: run the pending-script-menu reconciliation once up front. */
    CG_CheckOpenWaitingScriptMenu();

    /* 0x3003b476..0x3003b4a2: drain every command newer than the one already
     * executed, up to latestSequence. The gate and the loop test are both signed. */
    while (cgs_serverCommandSequence < latestSequence) {
        /* 0x3003b480/0x3003b484: advance and commit the sequence BEFORE fetching. */
        cgs_serverCommandSequence = coduo_int32_from_bits((uint32_t)cgs_serverCommandSequence + 1u);

        /* 0x3003b489: fetch this reliable command into the engine's argv buffer;
         * nonzero means the command is present and ready to dispatch. */
        if (cgame_syscall(CG_GET_SERVER_COMMAND, cgs_serverCommandSequence)) {
            /* 0x3003b496: dispatch the fetched command. */
            CG_ServerCommand();
        }
    }
}
