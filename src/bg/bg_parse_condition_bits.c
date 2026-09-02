// Source: uo_cgame_mp_x86.dll 0x30001920..0x30001cc7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30001920_30001cc7.mcode
//
// BG_ParseConditionBits — parse one animation-script condition clause from the
// shared Com parse cursor and fold its condition-value bit masks into the
// caller's 64-bit result bitset.
//
// NAME RESOLUTION: the .mcode's mechanical name "Cmd_Give_f" is a pure size guess
// (win 0x3a7 ~= corpus 0x3aa) and is REJECTED — this body has no give/item/weapon
// logic at all. The real identity is proven two ways: (1) it references the two
// diagnostic strings "BG_ParseConditionBits: unexpected '%s'" (0x300727dc) and
// "BG_ParseConditionBits: unexpected end of condition" (0x30072804); (2) the
// same-module PPC bank (cgame_mp.dll) lists BG_ParseConditionBits. Its two callers
// (0x30001e35, 0x3000282f, the per-condition parse loop in BG_AnimParseAnimScript)
// pass a Com parse cursor, a fallback name table, a condition index, and an
// int[2] output — matching the four cdecl args recovered below.
//
// ABI: __cdecl, 4 dword args, caller-cleaned (each call site does `ADD ESP,0x10`).
//   arg0 [entry+0x04] char **text_pp      Com parse text cursor
//   arg1 [entry+0x08] bg_indexed_string_t *stringTable  fallback bit-index table
//   arg2 [entry+0x0c] int condIndex       which animation condition
//   arg3 [entry+0x10] int *result         in/out int[2] 64-bit mask accumulator
// The prologue `SUB ESP,0x54; PUSH EBX,EBP,ESI,EDI` puts args at [ESP+0x68..0x74]
// once all four callee-saved registers are pushed. A /GS stack cookie
// (__security_cookie snapshotted at 0x30001923, checked via
// __security_check_cookie 0x30061639 at both RETs) is a compiler artifact and is
// not modeled here.
//
// The parser walks the flat per-condition tables:
//   bgAnimConditionAliases[condIndex*16 .. +16)  (0x3008bf38 window, NULL-terminated
//                                                  bg_indexed_string_t lookup table)
//   bgAnimConditionAliasBits[condIndex*16 + v]      (0x3008c4e8 parallel int[2] masks)
// both proven above (globals.h) with stride/extent from the anim registration
// routine.
//
// Machine-code self-check performed over every branch, memory access, call
// (Com_Parse, Com_ScriptError, Com_Error, Q_stricmpn, Q_strcat, BG_IndexForString),
// argument order, constant, and wrapping shift.

#include "bg_animation.h"
#include "bg_animation_services.h"
#include "qcommon/com_parse.h"
#include "qcommon/q_string.h"

#include <string.h>

// Condition-clause keyword literals compared with Q_stricmpn(token, lit, 99999).
// (Q_stricmpn(s1, s2, limit): EAX=limit, EDX=s1=token, ECX=s2=literal; ==0 == match.)
#define BG_STRICMP_LIMIT 99999   /* 0x1869f — the universal Q_stricmp compare cap */

void BG_ParseConditionBits(char **text_pp, bg_indexed_string_t *stringTable, int condIndex, bg_condition_bits_t *result)
{
    char keyBuffer[64];         // [ESP+0x20]: accumulated space-joined condition key
    /* The original fallback bit store indexes from [ESP+0x10] without bounding
     * the index to the two-word condition mask (0x30001c26..0x30001c40).
     * Its next two stack dwords are notFlag at +0x18 and a disposable compiler
     * scratch at +0x1c.  Model that four-dword window explicitly so weapon
     * indexes 64..127 retain the original x86 side effects without indexing
     * into an unrelated native-width pointer in the host compiler's frame. */
    int32_t localWords[4] = {0, 0, 0, 0};
    qboolean endOfClause = qfalse;// EBP: a terminator (empty token or trailing ',') seen
    com_parse_session_t *session = com_parseSession; /* 0x3000193e: EDI snapshot */

    // localWords[0]/[1] are [ESP+0x10]/[ESP+0x14], the current-clause mask.
    // localWords[2] is [ESP+0x18], MINUS/NOT -> AND-NOT instead of OR.
    keyBuffer[0] = '\0';          // 0x30001948: BYTE [ESP+0x20] = 0

    result->bits[0] = 0;          // 0x3000194c: *(int*)arg3 = 0

    for (;;) {
        // --- Read the next token (0x30001952) ---------------------------------
        // Honor a pushed-back token: if com_parseSession->ungetToken is set, either
        // re-run Com_Parse from the saved cursor (space-delimited mode) or reuse the
        // token already sitting in com_parseSession->token.
        const char *token;

        if (session->ungetToken != 0) {
            int wasSpaceDelimited = session->spaceDelimited;
            session->ungetToken = 0;                         // 0x30001960
            if (wasSpaceDelimited != 0) {
                *text_pp = session->savedParse;               // 0x30001976
                session->line = session->savedLine;           // 0x3000197e
                token = Com_ParseExt(text_pp, qfalse);          // 0x3000198a
                session = com_parseSession;                  // 0x3000198f
            } else {
                token = session->token;                      // 0x30001968: ESI = EDI
            }
        } else {
            token = Com_ParseExt(text_pp, qfalse);              // 0x3000198a
            session = com_parseSession;                      // 0x3000198f
        }

        // --- End-of-text handling (0x3000199a) --------------------------------
        if (token == NULL || token[0] == '\0') {
            // Push the (empty) token back so the caller's outer loop can see EOF.
            if (session->ungetToken != 0) {                  // 0x300019a2
                Com_ScriptError("UngetToken called twice");  // 0x300019af
                session = com_parseSession;                  // 0x300019b4
            }
            com_tokenStart = com_lastTokenStart;             // 0x300019bd/0x300019c2
            endOfClause = qtrue;                             // EBP = 1
            session->ungetToken = 1;                         // 0x300019d0
            // strlen(keyBuffer)==0 here (empty token path) -> return (0x300019ed).
            if (keyBuffer[0] == '\0')
                return;                                       // JZ 0x30001cb6
            // (fallthrough into token processing is unreachable for an empty token
            //  but preserved as in the machine code)
        }

        // --- Token classification (0x300019f3) --------------------------------
        // Note: after the empty-token branch above, `token` may be NULL/empty; the
        // machine code guards each Q_stricmpn with a NULL check (CMP ESI,EBX).
        if (token != NULL) {
            if (Q_stricmpn(token, ",", BG_STRICMP_LIMIT) == 0)       // 0x300019fc ","
                endOfClause = qtrue;                                  // 0x30001a0c

            if (Q_stricmpn(token, "none", BG_STRICMP_LIMIT) == 0) {   // 0x30001a16 "none"
                result->bits[0] |= 1;                                 // 0x30001a2a
                goto loop_tail;                                       // 0x30001a2d
            }
            if (Q_stricmpn(token, "none,", BG_STRICMP_LIMIT) == 0) {  // 0x30001a37 "none,"
                result->bits[0] |= 1;                                 // 0x30001cb3
                return;                                               // 0x30001caf path
            }
            if (Q_stricmpn(token, "NOT", BG_STRICMP_LIMIT) == 0)      // 0x30001a50 "NOT"
                token = "MINUS";                                      // 0x30001a60
        }

        // --- Accumulate real words into keyBuffer (0x30001a65) ----------------
        // Only when the clause has not already terminated. Operators AND/MINUS are
        // not appended.
        if (endOfClause == qfalse) {
            qboolean isOperator = qfalse;
            if (token != NULL) {
                if (Q_stricmpn(token, "AND", BG_STRICMP_LIMIT) == 0 ||   // 0x30001a76
                    Q_stricmpn(token, "MINUS", BG_STRICMP_LIMIT) == 0)   // 0x30001a8f
                    isOperator = qtrue;                                  // -> skip append
            }
            if (!isOperator) {
                // Strip a trailing ',' (clause terminator glued onto the word).
                size_t tlen = strlen(token);                             // 0x30001a9f loop
                if (token[tlen - 1] == ',') {                            // 0x30001aad ','
                    endOfClause = qtrue;                                 // 0x30001ab6
                    // token[strlen(token)-1] = 0  (0x30001ac9); token is a mutable
                    // Com token buffer here (com_parseSession->token / *text_pp view).
                    ((char *)token)[strlen(token) - 1] = '\0';
                }
                if (keyBuffer[0] != '\0')                                // 0x30001acd strlen != 0
                    Q_strcat(keyBuffer, 0x40, " ");                      // 0x30001af1 append " "
                Q_strcat(keyBuffer, 0x40, token);                       // 0x30001b03 append word
                session = com_parseSession;                             // 0x30001b08
            }
        }

        // --- Decide whether to finalize the clause (0x30001b13) ---------------
        // If the token is the AND or MINUS operator, finalize the accumulated key
        // now; otherwise (a plain word) keep reading unless the clause terminated.
        {
            qboolean tokenIsAndOrMinus = qfalse;
            if (token != NULL) {
                if (Q_stricmpn(token, "AND", BG_STRICMP_LIMIT) == 0 ||   // 0x30001b1c
                    Q_stricmpn(token, "MINUS", BG_STRICMP_LIMIT) == 0)   // 0x30001b31
                    tokenIsAndOrMinus = qtrue;
            }
            if (!tokenIsAndOrMinus && endOfClause == qfalse)             // 0x30001b41
                continue;                                                // JZ 0x30001952
        }

        // --- Finalize the accumulated condition key (0x30001b49) --------------
        if (keyBuffer[0] == '\0') {                                      // strlen==0
            if (endOfClause != qfalse) {                                 // 0x30001b5f
                BG_AnimParseError("BG_ParseConditionBits: unexpected end of condition"); // 0x30001b68
            } else if (token != NULL && Q_stricmpn(token, "MINUS", BG_STRICMP_LIMIT) == 0) { // 0x30001b7b
                localWords[2] = 1;                                       // 0x30001b8b
                continue;                                                // JMP 0x30001952
            } else {
                BG_AnimParseError("BG_ParseConditionBits: unexpected '%s'", token); // 0x30001b9e
            }
            session = com_parseSession;                                 // 0x30001ba6
        }

        // --- Resolve the key to a mask (0x30001bac) ---------------------------
        if (Q_stricmpn(keyBuffer, "default", BG_STRICMP_LIMIT) == 0) {   // 0x30001bb6
            localWords[0] = ~0;                                          // 0x30001bc3
            localWords[1] = ~0;                                          // 0x30001bc6
        } else {
            // The binary forms this per-condition table pointer here, after every
            // token/error callback above, rather than snapshotting it at entry.
            bg_indexed_string_t *valueNames = &bgAnimConditionAliases[(uint32_t)condIndex * 16u];
            int valueIndex = BG_IndexForString(keyBuffer, valueNames, qtrue); // 0x30001be8
            if (valueIndex >= 0) {                                       // 0x30001bf2 (JL)
                // masks from the parallel per-condition value table
                bg_condition_bits_t *bv = &bgAnimConditionAliasBits[condIndex * 16 + valueIndex]; // 0x30001bf4..bfd
                localWords[0] = bv->bits[0];                             // 0x30001c00
                localWords[1] = bv->bits[1];                             // 0x30001c06
            } else {
                // Fallback: treat the key as a single bit in the caller-supplied
                // string table's index space. The original address calculation
                // reaches localWords[2]/[3] for valid weapon indexes >= 64.
                int bitIndex = BG_IndexForString(keyBuffer, stringTable, qfalse); // 0x30001c1f
                // localWords[bitIndex>>5] |= (1u << (bitIndex & 31)) (0x30001c26..c40)
                localWords[bitIndex >> 5] |= (int32_t)(1u << (bitIndex & 31));
            }
            session = com_parseSession;                                  // 0x30001c4a
        }

        // --- Apply localBits to result[] (0x30001c50) -------------------------
        if (localWords[2] != 0) {                                       // 0x30001c50
            result->bits[0] &= ~localWords[0];                           // 0x30001c5a/0x30001c5c
            result->bits[1] &= ~localWords[1];                           // 0x30001c5e..0x30001c63
        } else {
            result->bits[0] |= localWords[0];                            // 0x30001c67
            result->bits[1] |= localWords[1];                            // 0x30001c69/0x30001c6c
        }
        // result[1] store (0x30001c70) is folded into the assignments above.

        keyBuffer[0] = '\0';                                            // 0x30001c73 reset key

        // A MINUS immediately following the value inverts the NEXT clause.
        if (token != NULL && Q_stricmpn(token, "MINUS", BG_STRICMP_LIMIT) == 0)          // 0x30001c7e "MINUS"
            localWords[2] = 1;                                          // 0x30001c8e

    loop_tail:
        if (endOfClause != qfalse)                                     // 0x30001c96 (JZ 0x30001952)
            return;                                                     // epilogue 0x30001c9e
        // else fall back to the top of the loop and read the next token.
    }
}
