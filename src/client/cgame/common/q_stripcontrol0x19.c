// Source: uo_cgame_mp_x86.dll 0x3003a9f0..0x3003aa17
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003a9f0_3003aa17.mcode

#include "client/cgame/client_recovered.h"

/*
 * Q_StripControl0x19 (0x3003a9f0)
 *
 * Removes every occurrence of the control byte 0x19 (25, ASCII EM) from a
 * NUL-terminated string, in place, and returns nothing.
 *
 * The .mcode size-guess name "G_ModelName" is REJECTED: this is a pure string
 * routine — it takes a single char* (in ESI, register-passed by the caller),
 * walks it to the NUL, compacts every byte != 0x19 toward the front of the same
 * buffer, and NUL-terminates. There is no model table, no global read, and no
 * system call. It is also NOT id's Q_CleanStr (0x88 bytes on PPC; that filters
 * "^<digit>" color escapes and non-printable ranges and returns the string
 * pointer). This one strips exactly one byte value (0x19), returns void, and is
 * 0x27 bytes. Descriptive q_shared-style name by proven behavior; the exact
 * source symbol is unconfirmed.
 *
 * Role (caller evidence): CG_ServerCommand (0x3003ac90) copies a server-command
 * argument into a 0x95-byte local buffer via Q_strncpyz, then calls this on the
 * buffer to strip the 0x19 marker byte, then hands the cleaned string to
 * CG_AddToTeamChat (0x30039390). So this sanitizes an incoming chat/print line
 * before it is appended to the team-chat log.
 *
 * ABI: the string pointer arrives in ESI (the caller does `LEA ESI,[ESP+..]`
 * then a bare `CALL` with nothing pushed); plain RET (no stack args to clean).
 * The final RET leaves EAX holding the terminating NUL byte (the last
 * `MOV AL,[EDX+1]` read), which the caller ignores — this is a void function.
 *
 * Machine-code facts proven for every behavior-affecting statement:
 *   3003a9f0  AL = str[0]                 (MOV AL,[ESI])
 *   3003a9f2  ECX = 0                      (write index dst)   (XOR ECX,ECX)
 *   3003a9f4  TEST AL,AL ; JZ 0x3003aa12   => empty string: jump straight to
 *                                             the terminator write, dst stays 0
 *   3003a9f8  EDX = ESI                    (read cursor src = &str[0])
 *   3003a9fa  LEA EBX,[EBX]                (6-byte NOP alignment padding; no-op)
 *   loop @3003aa00:
 *   3003aa00  AL = *src                    (MOV AL,[EDX])
 *   3003aa02  CMP AL,0x19 ; JZ 0x3003aa0a  => when the byte is 0x19, skip the
 *                                             copy+increment (drop the byte)
 *   3003aa06  str[dst] = AL                (MOV [ECX+ESI],AL)  — in-place compact
 *   3003aa09  dst++                        (INC ECX)
 *   3003aa0a  AL = src[1] ; src++          (MOV AL,[EDX+1] ; INC EDX)  peek+advance
 *   3003aa0e  TEST AL,AL ; JNZ 0x3003aa00  => loop while the next byte is non-NUL
 *   3003aa12  str[dst] = 0                 (MOV [ECX+ESI],0)  NUL-terminate
 *   3003aa16  RET
 *
 * Note: the read cursor (src) advances one byte per iteration regardless of
 * whether the byte was kept; the write index (dst) advances only on kept bytes.
 * dst <= src always, so the in-place compaction never overwrites unread input.
 */
#define Q_STRIP_CONTROL_CHAR ((char)0x19)

void Q_StripControl0x19(char *str)
{
    int dst = 0;                 /* ECX: next write slot in str */

    if (str[0] == '\0') {
        str[0] = '\0';           /* empty: terminator write with dst == 0 */
        return;
    }

    /* EDX read cursor over the same buffer; loop peeks str[i+1] to decide when
     * to stop, matching the machine's `MOV AL,[EDX+1]; INC EDX; TEST; JNZ`. */
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] != Q_STRIP_CONTROL_CHAR) {
            str[dst] = str[i];   /* keep the byte; compact toward the front */
            dst++;
        }
    }

    str[dst] = '\0';
}
