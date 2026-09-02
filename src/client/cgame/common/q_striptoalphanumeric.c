// Source: uo_cgame_mp_x86.dll 0x3003aa20..0x3003aa65
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003aa20_3003aa65.mcode

#include "client/cgame/client_recovered.h"

/*
 * Q_StripToAlphanumeric (0x3003aa20)
 *
 * Filters a NUL-terminated string in place, keeping only the ASCII
 * alphanumeric bytes 'a'..'z', 'A'..'Z' and '0'..'9'; every other byte
 * (spaces, punctuation, color-escape carets, control bytes, high-bit bytes,
 * etc.) is dropped. The kept bytes are compacted toward the front of the same
 * buffer and the result is NUL-terminated. Returns void.
 *
 * The .mcode size-guess name "Cmd_NextVehSlot_f" is REJECTED: this is a pure
 * character-class string filter — no command dispatch, no globals, no traps.
 * It is a sibling of Q_StripControl0x19 (0x3003a9f0) directly above it: same
 * in-place read/write-index compaction shape, but the keep predicate is
 * "is alphanumeric" instead of "!= 0x19". It is NOT id's Q_CleanStr (which
 * strips "^<digit>" color escapes and non-printable ranges yet keeps spaces
 * and punctuation, and returns char*); this one drops everything that is not
 * [A-Za-z0-9] and returns void. Descriptive q_shared-style name by proven
 * behavior; the exact source symbol is unconfirmed.
 *
 * Role (caller evidence): CG_ServerCommand (0x3003ac90) fetches the local
 * player's "name" cvar into a 0x20-byte stack buffer via
 * trap_Cvar_VariableStringBuffer (syscall 0xb), calls this to reduce the name
 * to alphanumeric characters only, then uses the cleaned name to build a demo
 * filename ("record %s-%s") and a screenshot filename ("screenshotJPEG %s-%s").
 * Stripping to alphanumeric makes the player name safe to embed in a filename.
 *
 * ABI: the string pointer arrives in EDI (the caller does `LEA EDI,[ESP+..]`
 * then a bare `CALL` with nothing pushed). The routine saves/restores ESI
 * (its write index) around the body and returns with a plain RET; there are no
 * stack args to clean and no return value (the final AL holds the terminating
 * NUL from the last peek, which the caller ignores).
 *
 * Machine-code facts proven for every behavior-affecting statement:
 *   3003aa20  AL = str[0]                    (MOV AL,[EDI])
 *   3003aa22  PUSH ESI                        (save caller ESI)
 *   3003aa23  ESI = 0                         (write index dst)   (XOR ESI,ESI)
 *   3003aa25  TEST AL,AL ; JZ 0x3003aa5f      => empty string: skip straight to
 *                                                the terminator write, dst == 0
 *   3003aa29  EDX = EDI                        (read cursor src = &str[0])
 *   loop @3003aa30:
 *   3003aa30  CL = *src                        (MOV CL,[EDX])
 *   3003aa32  EAX = (int)(signed char)CL       (MOVSX EAX,CL)  sign-extend
 *   3003aa35  CMP EAX,0x61 ; JL  0x3003aa3f    c <  'a' -> not lowercase
 *   3003aa3a  CMP EAX,0x7a ; JLE 0x3003aa53    'a'<=c<='z' -> keep
 *   3003aa3f  CMP EAX,0x41 ; JL  0x3003aa49    c <  'A' -> not uppercase
 *   3003aa44  CMP EAX,0x5a ; JLE 0x3003aa53    'A'<=c<='Z' -> keep
 *   3003aa49  CMP EAX,0x30 ; JL  0x3003aa57    c <  '0' -> drop
 *   3003aa4e  CMP EAX,0x39 ; JG  0x3003aa57    c >  '9' -> drop
 *   3003aa53  str[dst] = CL                    (MOV [ESI+EDI],CL)  keep+compact
 *   3003aa56  dst++                            (INC ESI)
 *   3003aa57  AL = src[1] ; src++              (MOV AL,[EDX+1] ; INC EDX) peek+adv
 *   3003aa5b  TEST AL,AL ; JNZ 0x3003aa30      loop while the next byte is non-NUL
 *   3003aa5f  str[dst] = 0                     (MOV [ESI+EDI],0)  NUL-terminate
 *   3003aa63  POP ESI ; RET
 *
 * Because MOVSX sign-extends, bytes with the high bit set become negative and
 * are < 0x30, so they fall through all the range checks and are dropped — the
 * predicate is exactly the signed comparison the hardware performs.
 *
 * The read cursor (src) advances one byte per iteration regardless of whether
 * the byte was kept; the write index (dst) advances only on kept bytes, so
 * dst <= src always and the in-place compaction never overwrites unread input.
 */
void Q_StripToAlphanumeric(char *str)
{
    int dst = 0;                 /* ESI: next write slot in str */

    if (str[0] == '\0') {
        str[0] = '\0';           /* empty: terminator write with dst == 0 */
        return;
    }

    /* EDX read cursor over the same buffer; the loop peeks str[i+1] to decide
     * when to stop, matching `MOV AL,[EDX+1]; INC EDX; TEST; JNZ`. */
    for (int i = 0; str[i] != '\0'; i++) {
        int c = (signed char)str[i];

        /* 0x3003aa35..0x3003aa52: the original routine performs this signed
         * three-range comparison chain in its own body. Keep it here rather
         * than introducing a non-original helper call on unoptimized builds. */
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
            str[dst] = str[i];   /* keep the byte; compact toward the front */
            dst++;
        }
    }

    str[dst] = '\0';
}
