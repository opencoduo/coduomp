// Source: uo_cgame_mp_x86.dll 0x300178c0..0x3001791b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300178c0_3001791b.mcode

#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_ConsoleCommand — dispatch a console command directed at the cgame module.
 *
 * The engine hands cgame a command line; this routine fetches token 0 (the
 * command word) and looks it up case-insensitively in cg_consoleCommands[]. On a
 * match it runs the registered handler (if any) and reports the command handled;
 * otherwise it reports not-handled so the engine can try elsewhere.
 *
 * Name: same-module PPC bank cgame_mp!CG_ConsoleCommand. The .mcode's
 * size-matched guess "script_func_vectordot" is rejected — this reads a console
 * command table and has no vector-dot arithmetic. Its neighbor CG_InitConsoleCommands
 * (0x30017920) registers/removes the same table's commands via the engine.
 *
 * Machine-code trace (0x300178c0..0x3001791a):
 *   push esi                              ; save
 *   push 0x400 / push buf / push 0 / push 0xd
 *   call [cgame_syscall]                  ; trap_Argv(0, g_textScratchBuffer, 1024)
 *   mov  ecx,[cg_consoleCommands[0].name] ; first command name
 *   add  esp,0x10 ; xor esi,esi           ; i = 0
 *   test ecx,ecx ; jz  ret0               ; empty table -> return qfalse
 * loop (0x300178e4):
 *   mov  eax,99999 ; mov edx,buf ; call Q_stricmpn   ; Q_stricmpn(name(ECX), buf(EDX), 99999)
 *   test eax,eax ; jz  found                          ; match -> handle it
 *   mov  ecx,[cg_consoleCommands[i+1].name]           ; next name (base+8, pre-inc esi)
 *   inc  esi
 *   test ecx,ecx ; jnz loop                           ; NULL name terminates
 *   xor  eax,eax ; pop esi ; ret                       ; return qfalse
 * found (0x30017907):
 *   mov  esi,[cg_consoleCommands[i].function]          ; handler (base+4, post-inc esi)
 *   test esi,esi ; jz  skip
 *   call esi                                           ; run handler (no args)
 * skip (0x30017914):
 *   mov  eax,1 ; pop esi ; ret                         ; return qtrue
 *
 * The pre-increment name load uses base+8 (0x30071788) with the not-yet-incremented
 * index and the post-increment handler load uses base+4 (0x30071784) with the
 * incremented index; both resolve to the same table entry i, so this is a plain
 * for-scan over cg_consoleCommands[]. Q_stricmpn's limit of 99999 makes it a full
 * (unbounded) case-insensitive string compare; == 0 means the names are equal.
 */
qboolean CG_ConsoleCommand(void)
{
    int32_t i;

    /* trap_Argv(0, buffer, 1024): copy argv[0] (the command word) into the shared
     * 1KB text scratch buffer. Length is the raw 0x400 = 1024 pushed at 0x300178c1. */
    trap_Argv(0, g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));

    for (i = 0; cg_consoleCommands[i].name != NULL; i++) {
        /* EAX=99999 limit, ECX=name (s1), EDX=buffer (s2); 0 => equal. */
        if (Q_stricmpn(cg_consoleCommands[i].name, g_textScratchBuffer, 99999) == 0) {
            void (*handler)(void) = cg_consoleCommands[i].function;
            if (handler != NULL) {
                handler();
            }
            return qtrue;
        }
    }

    return qfalse;
}
