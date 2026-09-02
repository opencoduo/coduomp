// Source: uo_cgame_mp_x86.dll 0x3002de90..0x3002df22
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002de90_3002df22.mcode
//
// CG_BuildTimeoutHudStrings(void) — rebuild the two "timeout" HUD display values
// from their config strings when the scoreboard is enabled. Reads the timeout
// end-time config string, parses it to an integer time, and (if nonzero) adds the
// current engine milliseconds to make an absolute end time; then formats the
// timeout text config string through the engine and copies it into the display
// buffer. When the scoreboard is disabled it just clears the end-time and returns.
//
// This is one member of the config-string "timeout"/"vote" display cluster. The
// per-field version of exactly this work lives in the config-string dispatcher
// CG_ConfigStringModified (0x30038e70): its case CS number 0x775 (1909) runs the
// end-time block below (into cg_timeoutEndTime / cg_timeoutActive) and its case
// 0x776 (1910) runs the format+strncpy block below (into cg_timeoutString), each
// using the SAME globals, the SAME "timeout string" format literal (0x300779bc),
// the SAME trap ids (6 and 0x39), and the SAME callees (Q_atoi 0x3005b6ce and CRT
// strncpy 0x3005be00). This function performs both together at scoreboard setup.
//
// NAME REJECTED: the .mcode size-guess "G_DObjCalcPose" is wrong — there is no
// DObj, pose, skeleton, or bone math here; the body is config-string parse +
// engine text format + fixed 255-byte string copy into a HUD buffer, gated by the
// scoreboard-enable flag. Named by proven role.
//
// Instruction evidence (every behavior-affecting statement):
//
//   3002de90  CMP dword [0x3045014c],1          ; timescale_vmCvar.integer == 1 ?
//   3002de97  MOV dword [0x30447fd4],0          ; cg_timeoutActive = 0 (always)
//   3002dea1  JNZ 0x3002deae                    ; if enabled, do the work
//   3002dea3  MOV dword [0x30447fd0],0          ; else cg_timeoutEndTime = 0
//   3002dead  RET                               ;      and return
//   3002deae  MOV EAX,[0x304427d4]              ; EAX = timeout-time CS offset
//   3002deb3  LEA ECX,[EAX + 0x30442a00]        ; ECX = &cg_gameState.stringData[off]
//   3002deb9  PUSH ECX
//   3002deba  CALL 0x3005b6ce                   ; EAX = Q_atoi(configString)
//   3002debf  ADD ESP,4                         ; caller-clean 1 arg
//   3002dec2  TEST EAX,EAX
//   3002dec4  MOV [0x30447fd0],EAX              ; cg_timeoutEndTime = parsed time
//   3002dec9  JZ 0x3002deee                     ; if 0, skip the milliseconds add
//   3002decb  PUSH 6                            ; trap id 6 (engine milliseconds)
//   3002decd  MOV dword [0x30447fd4],1          ; cg_timeoutActive = 1
//   3002ded7  CALL [0x30085e9c]                 ; EAX = cgame_syscall(6)
//   3002dedd  MOV ECX,[0x30447fd0]              ; ECX = cg_timeoutEndTime
//   3002dee3  ADD ESP,4                         ; caller-clean the id
//   3002dee6  ADD ECX,EAX                       ; endTime += milliseconds
//   3002dee8  MOV [0x30447fd0],ECX              ; store absolute end time
//   3002deee  MOV EAX,[0x304427d8]              ; EAX = timeout-string CS offset
//   3002def3  LEA EAX,[EAX + 0x30442a00]        ; EAX = &cg_gameState.stringData[off]
//   3002def9  PUSH 0x300779bc                   ; "timeout string" format literal
//   3002defe  PUSH EAX                          ; source config string
//   3002deff  PUSH 0x39                         ; trap id 0x39 (CS text format)
//   3002df01  CALL [0x30085e9c]                 ; EAX = cgame_syscall(0x39, src, fmt)
//   3002df07  PUSH 0xff                         ; strncpy count = 255
//   3002df0c  PUSH EAX                          ; strncpy src = formatted text
//   3002df0d  PUSH 0x30447fd8                   ; strncpy dst = cg_timeoutString
//   3002df12  CALL 0x3005be00                   ; strncpy(cg_timeoutString, txt, 255)
//   3002df17  ADD ESP,0x18                      ; caller-clean 6 dwords (3 trap + 3 strncpy)
//   3002df1a  MOV byte [0x304480d7],0           ; cg_timeoutString[255] = '\0'
//   3002df21  RET
//
// Provisional callee decls (Q_atoi, cgame_syscall) are caller-observed and are
// superseded by each callee's own .mcode reconstruction; strncpy is the statically
// linked MSVC CRT strncpy at 0x3005be00 (dword-scan copy, no forced NUL).

#include <string.h>

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_BuildTimeoutHudStrings(void)
{
    /* 0x3002de97 MOV [cg_timeoutActive],0 executes on BOTH paths (before the branch).
     * 0x3002de90 CMP [timescale],1 / 0x3002dea1 JNE 0x3002deae: the rebuild runs when
     * timescale != 1; the timescale == 1 fall-through clears cg_timeoutEndTime
     * (0x3002dea3) and returns. A prior pass INVERTED the condition (cleared on != 1,
     * rebuilt on == 1), so the timeout HUD was built during normal play and cleared
     * during an actual timeout. */
    cg_timeoutActive = 0;
    if (timescale_vmCvar.integer == 1) {
        cg_timeoutEndTime = 0;
        return;
    }

    /* Config string N = &cg_gameState.stringData[offset]; here the timeout-time
     * config string, parsed to an integer time via Q_atoi. */
    cg_timeoutEndTime = coduo_crt_atoi(&cg_gameState.stringData[cg_gameState.stringOffsets[CS_TIMEOUT_TIME]]);
    if (cg_timeoutEndTime != 0) {
        /* Non-zero timeout: mark active and convert the relative time into an
         * absolute end time by adding the current engine milliseconds. */
        cg_timeoutActive = 1;
        cg_timeoutEndTime = coduo_int32_from_bits((uint32_t)cg_timeoutEndTime + (uint32_t)cgame_syscall(CG_MILLISECONDS));
    }

    /* Format the timeout-text config string through the engine, copy up to 255
     * bytes, then force the final byte of the 256-byte buffer to NUL. */
    {
        char *formatted = (char *)(intptr_t)cgame_syscall(
            CG_SE_LOCALIZE_MESSAGE, &cg_gameState.stringData[cg_gameState.stringOffsets[CS_TIMEOUT_STRING]], "timeout string");
        strncpy(cg_timeoutString, formatted, 255);
    }

    cg_timeoutString[sizeof(cg_timeoutString) - 1] = '\0';
}
