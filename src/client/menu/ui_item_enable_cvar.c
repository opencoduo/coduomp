// Sources: uo_cgame_mp_x86.dll 0x300527e0..0x30052918 and
//          uo_ui_mp_x86.dll    0x40014330..0x40014468
//
// Item_EnableShowViaCvar — ui_shared.c menu-item show/hide (or enable/disable)
// predicate. The .mcode header's mechanical size-guess "CG_DrawPlayerWeaponModeIcon"
// (win size 0x138 == matched size 0x138) is REJECTED per the no-size-matching rule:
// this function draws nothing. It reads the item's linked cvar string value and
// matches it against a list of accepted tokens, exactly the same-module PPC-bank
// role Item_EnableShowViaCvar. Proven by:
//   - item arrives in ECX (thiscall this); `flag` is a single stack arg at [EBP+0x8],
//     caller-cleaned.
//   - item->enableCvar (+0x11c) names a cvar whose current string is fetched via
//     DC->getCVarString (DC+0x68) into a 0x400 buffer (0x3005284c).
//   - item->cvarTest (+0x120) is a whitespace/';'-separated value list, copied into a
//     zeroed 0x400 buffer via Q_strcat (0x3004e740) and tokenized with String_Parse
//     (0x300505a0); each token is compared case-insensitively to the cvar value with
//     Q_stricmpn (0x3004e620) using the 99999 (0x1869f) limit (i.e. Q_stricmp).
//   - item->cvarFlags (+0x124) ANDed with `flag` (0x30052893 TEST) selects polarity.
//
// Register/ABI notes recorded here, not modeled as source:
//   - MSVC /GS prologue: snapshot __security_cookie [0x30081650] into a frame slot
//     ([ESP+0x81c]) and verify it via __security_check_cookie (0x30061639, ECX=cookie)
//     before each RET.
//   - AND ESP,~7 frame alignment and callee register saves are ABI, not behavior.

#include "ui_runtime.h"

#include <string.h>

extern displayContextDef_t *DC;

void Q_strcat(char *destination, int32_t destinationSize,
              const char *source);
int32_t Q_stricmpn(const char *left, const char *right, int32_t count);
qboolean String_Parse(char **data, const char **string);

/*
 * A menu-script token separator: an isolated ";" token (0x30052884: CMP byte,0x3b
 * then CMP byte[+1],0) is skipped, matching the ui_shared.c list-separator handling.
 */
enum { CVARTEST_SEPARATOR_CHAR = ';' };

/*
 * Q_stricmpn is called with the constant 99999 (0x1869f) limit at 0x300528a0, i.e.
 * an effectively-unbounded case-insensitive compare (Q_stricmp behavior).
 */
enum { STRICMP_UNBOUNDED_LIMIT = 99999 };

qboolean Item_EnableShowViaCvar(itemDef_t *item, int32_t flag)
{
    char cvarValue[MAX_STRING_CHARS];      /* getCVarString destination (ESP0+0x418) */
    char cvarTestBuf[MAX_STRING_CHARS];    /* zeroed, then Q_strcat(cvarTest) (ESP0+0x18) */
    char *parsePtr;             /* String_Parse handle (ESP0+0x10) -> cvarTestBuf */
    const char *token;          /* current parsed token (ESP0+0x14) */

    /* 0x300527fd..0x3005280a: zero the 0x400-byte token buffer (REP STOSD, 0x100 dwords). */
    memset(cvarTestBuf, 0, sizeof(cvarTestBuf));

    /* 0x300527ff/0x30052812/0x30052829: bail (return qtrue) if the item is NULL or
     * either linked-cvar string is NULL or empty. */
    if (item == NULL) {
        return qtrue;
    }
    if (item->cvarTest == NULL || item->cvarTest[0] == '\0') {
        return qtrue;
    }
    if (item->enableCvar == NULL || item->enableCvar[0] == '\0') {
        return qtrue;
    }

    /* 0x3005284c: cvarTest (+0x11c) is the cvar name. */
    DC->getCVarString(item->cvarTest, cvarValue, (int32_t)sizeof(cvarValue));

    /* 0x3005285f: build the tokenizer source buffer = cvarTest (Q_strcat into the
     * freshly-zeroed buffer; EAX=destSize 0x400, EBX=dest, src pushed). */
    Q_strcat(cvarTestBuf, (int)sizeof(cvarTestBuf), item->enableCvar);

    /* 0x30052871: seed the String_Parse handle to point at the copied list. */
    parsePtr = cvarTestBuf;

    /* 0x30052875: first token. If there is no token at all, fall through to default. */
    if (String_Parse(&parsePtr, &token)) {
        do {
            /* 0x30052884: skip a bare ";" list separator token. */
            if (token[0] == CVARTEST_SEPARATOR_CHAR && token[1] == '\0') {
                continue;
            }

            /* 0x30052893: select polarity from cvarFlags & flag, then compare the
             * token to the cvar's current value (case-insensitive, unbounded). */
            if ((item->cvarFlags & flag) != 0) {
                /* show/enable list: a match means "enabled" -> qtrue. */
                if (Q_stricmpn(token, cvarValue, STRICMP_UNBOUNDED_LIMIT) == 0) {
                    return qtrue;
                }
            } else {
                /* hide/disable list: a match means "disabled" -> qfalse. */
                if (Q_stricmpn(token, cvarValue, STRICMP_UNBOUNDED_LIMIT) == 0) {
                    return qfalse;
                }
            }
            /* 0x300528d1: advance to the next token; loop while parsing succeeds. */
        } while (String_Parse(&parsePtr, &token));
    }

    /* 0x300528e2: no token matched. Default = !(cvarFlags & flag):
     *   NEG/SBB/INC turns (cvarFlags & flag)!=0 into 0 and ==0 into 1. */
    return ((item->cvarFlags & flag) == 0) ? qtrue : qfalse;
}
