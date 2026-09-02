// Source: uo_cgame_mp_x86.dll 0x30001cd0..0x30001e84
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30001cd0_30001e84.mcode
//
// BG_ParseConditions — parse the condition clauses that qualify one animation
// script and fill script->conditions[]. Resolved by behavior + call graph and the
// server name bank (game_mp_uo animation.c: `qboolean BG_ParseConditions(char
// **parse, bg_anim_script_t *script)`); the two diagnostics it emits are
// "BG_ParseConditions: expected condition value, found end of line" (0x30072790)
// and "BG_ParseConditions: no conditions found" (0x30072768). The .mcode
// size-guess name "CG_SoundBlend" is REJECTED: this reads no sound state, it
// tokenizes a parse cursor and builds an anim-script condition array.
//
// Custom register ABI (proven at the call site and by the body): the script is
// passed in ESI (used as the accumulator base at [ESI]=conditionCount, with
// 0x0c-byte condition slots at +0x04), and text_pp is the single caller-cleaned
// stack dword ([ESP+0x18] after the prologue). No compiler calling-convention
// attribute is applied; the register/stack split is recorded here as an ABI note.
// The function always returns qtrue (MOV EAX,1 at the single RET).
//
// Loop token acquisition (0x30001ce0 / repeated at 0x30001d80) is the Com_Parse
// unget/backup handling inlined by the compiler: if com_parseSession->ungetToken
// is set it either returns the buffered token in place (when NOT spaceDelimited —
// it returns session->token) or restores the saved cursor and re-parses;
// otherwise it just calls Com_Parse. Both copies remain explicit below because
// they take independent com_parseSession snapshots and are not calls in retail.

#include <stdint.h>
#include <stddef.h>

#include "bg_animation.h"
#include "bg_animation_services.h"
#include "qcommon/com_parse.h"
#include "qcommon/q_string.h"

/* Case-insensitive length-limited compare uses the universal 99999 (0x1869f)
 * scan limit that all Q_stricmpn call sites in this module pass. */
enum { Q_STRICMPN_UNBOUNDED = 99999 };

qboolean BG_ParseConditions(char **text_pp, bg_anim_script_t *script)
{
    /* The two condition-value words. Zeroed ONCE before the loop (the single
     * [ESP+0xc]/[ESP+0x10] = 0 at the prologue) and NOT re-zeroed per iteration.
     * bits[0] is fully overwritten each iteration; bits[1] persists — the
     * BITMASK path passes the bitset to BG_ParseConditionBits, which zeroes the
     * first word on entry but OR-accumulates into the second. */
    bg_condition_bits_t value;
    value.bits[0] = 0;
    value.bits[1] = 0;

    for (;;) {
        com_parse_session_t *session = com_parseSession; /* 0x30001ce0 */
        char *token;

        if (session->ungetToken != 0) {
            int32_t wasSpaceDelimited = session->spaceDelimited;

            session->ungetToken = 0;
            if (wasSpaceDelimited != 0) {
                *text_pp = session->savedParse;
                session->line = session->savedLine;
                token = Com_ParseExt(text_pp, qfalse);
            } else {
                token = session->token;
            }
        } else {
            token = Com_ParseExt(text_pp, qfalse);
        }

        if (token == NULL || token[0] == '\0') {
            /* End of the condition list. */
            if (script->conditionCount == 0) {
                BG_AnimParseError("BG_ParseConditions: no conditions found");
            }
            return qtrue;
        }

        /* "default" means an unconditional script: stop with no condition added. */
        if (Q_stricmpn(token, "default", Q_STRICMPN_UNBOUNDED) == 0) {
            return qtrue;
        }

        {
            int32_t condType = BG_IndexForString(token, bgAnimConditionTypeStrings, qfalse);
            int32_t mode = bgAnimConditionTypes[condType].mode;

            if (mode == ANIM_CONDMODE_BITMASK) {
                BG_ParseConditionBits(
                    text_pp,
                    bgAnimConditionTypes[condType].values,
                    condType, &value);
            } else if (mode == ANIM_CONDMODE_EQUAL) {
                if (bgAnimConditionTypes[condType].values == NULL) {
                    /* No value table for this condition type: assume value 1. */
                    value.bits[0] = 1;
                } else {
                    char *valueToken;
                    size_t valueLength;

                    /* The second retail inlined parse block takes a fresh global
                     * session snapshot after the condition-table load above. */
                    session = com_parseSession;                          /* 0x30001d80 */
                    if (session->ungetToken != 0) {
                        int32_t wasSpaceDelimited = session->spaceDelimited;

                        session->ungetToken = 0;
                        if (wasSpaceDelimited != 0) {
                            *text_pp = session->savedParse;
                            session->line = session->savedLine;
                            valueToken = Com_ParseExt(text_pp, qfalse);
                        } else {
                            valueToken = session->token;
                        }
                    } else {
                        valueToken = Com_ParseExt(text_pp, qfalse);
                    }

                    if (valueToken == NULL || valueToken[0] == '\0') {
                        BG_AnimParseError("BG_ParseConditions: expected condition value, "
                                  "found end of line");
                    }

                    /* Two independent strlen scans surround the optional comma
                     * removal at 0x30001dd9..0x30001dfe. */
                    valueLength = strlen(valueToken);
                    if (valueToken[valueLength - 1] == ',') {
                        valueLength = strlen(valueToken);
                        valueToken[valueLength - 1] = '\0';
                    }
                    value.bits[0] = BG_IndexForString(
                        valueToken, bgAnimConditionTypes[condType].values,
                        qfalse);
                }
            }
            /* Any other mode: leave value[0]/value[1] as-is (a pass-through). */

            /* NOT_FROM_ORIGINAL_SOURCE: enforce the fixed condition capacity
             * before writing the type or either value word. */
            if ((uint32_t)script->conditionCount >=
                (uint32_t)ANIM_COND_COUNT) {
                BG_AnimParseError(
                    "BG_ParseConditions: exceeded maximum conditions (%i)",
                    ANIM_COND_COUNT);
                return qfalse;
            }
            {
                int32_t conditionIndex = script->conditionCount;        /* 0x30001e3d */
                script->conditions[conditionIndex].type = condType;

                conditionIndex = script->conditionCount;                /* 0x30001e4a */
                script->conditions[conditionIndex].value[0] = value.bits[0];

                conditionIndex = script->conditionCount;                /* 0x30001e53 */
                script->conditions[conditionIndex].value[1] = value.bits[1];
                script->conditionCount =
                    coduo_int32_from_bits((uint32_t)script->conditionCount + 1u); /* INC [ESI] */
            }
        }
    }
}
