// Source: uo_cgame_mp_x86.dll 0x30002470..0x30002ecc
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30002470_30002ecc.mcode
//
// BG_AnimParseAnimScript(bg_static_animation_table_t *table,
//                        bg_runtime_animation_t *runtimeArr,
//                        int32_t *runtimeCount)
//
// Top-level player animation-script parser of the CoD BG animation system. It
// loads "mp/playeranim.script" (bgPlayerAnimScriptPath) once into a static text
// buffer, then drives the Com script tokenizer over it, filling the per-anim-tree
// script/statechange/event/command tables. Every diagnostic it emits is prefixed
// "BG_AnimParseAnimScript: ..." (e.g. 0x30072470 "expected condition type string",
// 0x30072434 "can not make a define of type '%s'", 0x30072350 "expected 'state'",
// 0x300722fc "expected '{'", 0x30072210 "expected 'statechange', got '%s'"), which
// proves the name; the .mcode's size-matched "CG_DrawCrosshair" guess is REJECTED
// (no crosshair drawing occurs).
//
// The five script sections are dispatched by a jump table at 0x30002ecc keyed on a
// keyword index resolved from the section-name table 0x300823f8, in this order:
//   0 "defines"  1 "animations"  2 "canned_animations"  3 "statechanges"  4 "events"
// Within each section body a brace-nesting sub-state machine (state in EBX, 0..3)
// tracks depth; at the deepest level BG_ParseConditions fills a 0x10c-byte command
// scratch that is appended to the anim-tree's global command pool and linked into
// the current script slot, or BG_ParseCommands parses a full command body.
//
// NAMING NOTE: the globals.h notes attribute the runtime-animation state setup
// (0x300a7820/0x300a5108) to "CGScr_InitAnimTreeParse"; that is this function's
// script-loading caller path (0x30016360 CGScr_LoadAnimTrees passes a non-NULL
// runtime array/count), while the other caller (0x300018f7) passes NULLs to parse
// the static player script. The diagnostic strings make BG_AnimParseAnimScript the
// authoritative name for THIS body.
//
// ABI: standard cdecl, three stack args at [EBP+0x8]/[EBP+0xc]/[EBP+0x10]. The
// i386 /GS stack-cookie prologue/epilogue (MOV EAX,[__security_cookie]; ...;
// __security_check_cookie at 0x30061639) and the callee-saved EBX/ESI/EDI pushes
// are calling-convention/codegen detail expressed here as ordinary C.
//
// Frame map (post-prologue ESP), proven from the [ESP+X] accesses:
//   +0x0c data_p (char* parse cursor)   +0x10 state (EBX, brace depth)
//   +0x14 currentSlot (bg_anim_script_list_t* script slot base)
//   +0x18..+0x20 idx[3] (per-depth parsed index; idx[state], -1 = unset)
//   +0x24 lastCommandBlock (init 0/NULL at 0x30002492; the append tail at
//         0x30002dfb saves the just-appended global block pointer here, and both
//         BG_ParseCommands call sites 0x30002b10/0x30002e58 pass this slot)
//   +0x2c currentSection (ANIM_SECTION_*)   +0x30.. block[0x10c] command scratch

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bg_animation.h"
#include "bg_animation_services.h"
#include "qcommon/com_parse.h"
#include "qcommon/q_string.h"

/* The parsed regions (animations/canned/statechanges/events/globalItems) and the
 * globalItemCount are now named fields/arrays of bg_static_animation_table_t
 * (bg_animation_types.h); the parser indexes those arrays directly instead of adding
 * the old proven byte offsets (0xb804 / 0x14924 / 0x1da44 / 0x1fa84 / 0x21ac4 /
 * 0xa7ac4) to a char* base. Each script-list region is one bg_anim_script_list_t
 * (0x204 on i386); each global command block is one bg_anim_script_t (0x10c). */

enum {
    ANIM_SCRIPT_TEXT_MAX = 0x1869f /* file size cap / Q_stricmpn limit */
};

/* The animation-state fan-out (slot = idx[0]*18 + animState, LEA x9, x2) is now the
 * animations[4][18] / canned[4][18] array shape: the 18-wide inner dimension is the
 * multiplier, so indexing region[idx[0]][animState] encodes it directly. */

/* bg_anim_script_t (the 0x10c-byte i386 parsed command block, bulk-cleared and
 * bulk-copied as a unit) and bg_anim_script_list_t are defined in the shared BG
 * animation-type boundary. The table's globalItems[] pool stores the blocks. */

/* BG_ParseConditions (0x30001cd0), Com_ParseExt (0x3004d6b0), Com_ParseOnLine
 * (0x3004da60), Com_Parse (0x3004da20) and the animation-script scratch
 * globals (bgAnimScriptFileBuffer/bgAnimScriptLoaded/
 * bgAnimConditionAliasStringUsed/bgAnimConditionAliasStringBuffer/
 * bgAnimConditionAliasCounts) are declared in bg_animation.h. */

/* Exact .rdata literals. Source addresses are retained in comments, but runtime
 * source uses native string objects rather than original-image absolute pointers. */
#define STR_SET "set" /* 0x300724a8 */
#define STR_STATE "state" /* 0x3007237c */
#define STR_STATECHANGE "statechange" /* 0x3007224c */
#define STR_OPEN_BRACE "{" /* 0x30072384 */
#define STR_CLOSE_BRACE "}" /* 0x30072764 */
#define STR_EQUALS "=" /* 0x300723bc */
#define STR_SESSION_NAME "BG_AnimParseAnimScript" /* 0x30072508 */
#define STR_COULDNT_LOAD \
    "\x15" \
    "Couldn't load player animation script %s\n" /* 0x30072520 */
#define STR_UNEXPECTED "BG_AnimParseAnimScript: unexpected '%s'" /* 0x300724ac */
#define STR_END_OF_FILE "BG_AnimParseAnimScript: unexpected end of file: %s" /* 0x300724d4 */
#define STR_COND_TYPE "BG_AnimParseAnimScript: expected condition type string" /* 0x30072470 */
#define STR_MAKE_DEFINE "BG_AnimParseAnimScript: can not make a define of type '%s'" /* 0x30072434 */
#define STR_COND_DEFINE "BG_AnimParseAnimScript: expected condition define string" /* 0x300723f8 */
#define STR_MAX_COND_DEFINES "BG_AnimParseAnimScript: exceeded maximum condition defines (%i)"
#define STR_EQ_EOL "BG_AnimParseAnimScript: expected '=', found end of line" /* 0x300723c0 */
#define STR_EQ_FOUND "BG_AnimParseAnimScript: expected '=', found '%s'" /* 0x30072388 */
#define STR_EXPECT_STATE "BG_AnimParseAnimScript: expected 'state'" /* 0x30072350 */
#define STR_STATE_TYPE "BG_AnimParseAnimScript: expected state type" /* 0x30072324 */
#define STR_EXPECT_OPEN "BG_AnimParseAnimScript: expected '{'" /* 0x300722fc */
#define STR_INTERNAL "BG_AnimParseAnimScript: internal error" /* 0x300722d4 */
#define STR_MAX_ITEMS "BG_AnimParseAnimScript: exceeded maximum items per script (%i)" /* 0x30072294 */
#define STR_MAX_GLOBAL "BG_AnimParseAnimScript: exceeded maximum global items (%i)" /* 0x30072258 */
#define STR_EXPECT_SCHANGE "BG_AnimParseAnimScript: expected 'statechange', got '%s'" /* 0x30072210 */
#define STR_STATE_TYPE2 "BG_AnimParseAnimScript: expected <state type>" /* 0x300721e0 */

/* section-keyword and sub-keyword indexed-string tables. */
#define ANIM_SECTION_TABLE bgAnimParseSectionStrings
#define ANIM_STATETYPE_TABLE animStateStr
#define ANIM_ANIMSTATE_TABLE bgAnimGroupStrings
#define ANIM_EVENTTYPE_TABLE bgAnimEventStrings

/* Rewind data_p back over the just-parsed token so it is re-read by the condition/
 * command parser, then verify the rewound cursor still spells the token (else
 * "internal error"). Mirrors the two strlen scans + Q_strncmp at 0x30002a00. */
/* The unget-restoring token fetch inlined by the MAIN LOOP (0x300025a3): if
 * com_parseSession has an ungotten token, restore the saved cursor/line and clear
 * the flag unconditionally, then Com_ParseExt(data_p, 1). */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of repeated retail inlined
 * parser-state plumbing; no additional recovered binary function is claimed. */
static char *bg_compat_anim_get_token(char **data_p, qboolean allowLineBreaks)
{
    com_parse_session_t *sess = com_parseSession;
    if (sess->ungetToken != 0) {
        *data_p = sess->savedParse;
        sess->ungetToken = 0;
        sess->line = sess->savedLine;
    }
    return Com_ParseExt(data_p, allowLineBreaks);
}

/* The DEFINES-SECTION token fetch inlined three times (0x30002663 / 0x300026f4 /
 * 0x30002790): unlike the main-loop fetch it gates the unget restore on
 * sess->spaceDelimited (+0x408). When a token was ungotten and the session is NOT
 * space-delimited, the buffered sess->token is returned directly without
 * re-parsing; only in space-delimited mode does it restore savedParse/savedLine
 * and re-tokenize with allowLineBreaks==0. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of three repeated retail
 * inlined parser-state blocks. */
static char *bg_compat_anim_get_token_on_line(char **data_p)
{
    com_parse_session_t *sess = com_parseSession;
    if (sess->ungetToken != 0) {
        int32_t spaceDelimited = sess->spaceDelimited;
        sess->ungetToken = 0;
        if (spaceDelimited == 0) {
            return sess->token;
        }
        *data_p = sess->savedParse;
        sess->line = sess->savedLine;
    }
    return Com_ParseExt(data_p, qfalse);
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the repeated retail
 * rewind-and-verify instruction sequence. */
static void bg_compat_unget_token_and_verify(char **data_p, const char *token)
{
    int32_t len = (int32_t)strlen(token);
    *data_p -= len;
    /* Q_strncmp compares the rewound source in EDX/left against the token in
     * ECX/right; EAX at 0x30002a2d supplies the token length. */
    if (Q_strncmp(*data_p, token, len) != 0) {
        BG_AnimParseError(STR_INTERNAL);
    }
}

/* Append the parsed command scratch to the anim-tree global pool and link it into
 * the current slot's list; returns the appended block. Shared tail at 0x30002dd8;
 * the bounds checks precede it at 0x30002a68 / 0x30002d80. */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the shared retail append
 * tail at 0x30002dd8, not an additional binary function. */
static bg_anim_script_t *bg_compat_append_command_block(bg_static_animation_table_t *table, bg_anim_script_list_t *list,
                                                        const bg_anim_script_t *scratch)
{
    int32_t *globalCount = &table->globalItemCount; /* +0xa7ac4 */
    bg_anim_script_t *dst;

    if (list->count >= BG_ANIM_MAX_LIST_ITEMS) {
        BG_AnimParseError(STR_MAX_ITEMS, BG_ANIM_MAX_LIST_ITEMS);
    }
    if (*globalCount >= BG_ANIM_MAX_GLOBAL_SCRIPTS) {
        BG_AnimParseError(STR_MAX_GLOBAL, BG_ANIM_MAX_GLOBAL_SCRIPTS);
    }
    dst = &table->globalItems[*globalCount]; /* +0x21ac4 + count*0x10c */
    list->scripts[list->count] = dst;   /* [ESI + count*4 + 4] = dst */
    *globalCount = coduo_int32_from_bits((uint32_t)*globalCount + 1u);
                                        /* tree->globalItemCount INC */
    dst = list->scripts[list->count];   /* reload the just-stored pointer */
    list->count = coduo_int32_from_bits((uint32_t)list->count + 1u); /* [ESI] INC */
    memcpy(dst, scratch, sizeof(*scratch)); /* MOVSD.REP 0x43 dwords */
    return dst;
}

/* NOT_FROM_ORIGINAL_SOURCE: the original event-list region is contiguous but
 * the recovered table gives its first ten and last six entries distinct names.
 * This preserves every valid event address without out-of-bounds C indexing. */
static bg_anim_script_list_t *bg_compat_anim_parser_event_list(bg_static_animation_table_t *table, int32_t event)
{
    if (event < ANIM_EVENT_RELOAD) {
        return &table->events[event];
    }
    return &table->scriptLists[event - ANIM_EVENT_RELOAD];
}

void BG_AnimParseAnimScript(bg_static_animation_table_t *table, bg_runtime_animation_t *runtimeArr, int32_t *runtimeCount)
{
    char *data_p;
    int32_t state = 0;
    bg_anim_script_list_t *currentSlot = NULL;
    int32_t idx[3] = {-1, -1, -1};
    /* [ESP+0x24]: NULL-initialized (0x30002492); tracks the most recently
     * appended global command block, which the command-body parses target. */
    bg_anim_script_t *lastCommandBlock = NULL;
    int32_t currentSection = 0;
    bg_anim_script_t block; /* [ESP+0x30]: command scratch */
    char *token;

    /* ---- one-time load of the animation script file ---- */
    if (bgAnimScriptLoaded == 0) {
        int32_t fileHandle;
        int32_t fileLen;

        fileLen = bg_compat_anim_script_open(bgPlayerAnimScriptPath, &fileHandle);
        if (fileLen <= 0) {
            Com_Error(ERR_DROP, STR_COULDNT_LOAD, bgPlayerAnimScriptPath);
        }
        if ((uint32_t)fileLen >= (uint32_t)ANIM_SCRIPT_TEXT_MAX) {
            Com_Error(ERR_DROP, STR_COULDNT_LOAD, bgPlayerAnimScriptPath);
        }
        bg_compat_anim_script_read(bgAnimScriptFileBuffer, fileLen, fileHandle);
        bgAnimScriptFileBuffer[fileLen] = 0;
        bg_compat_anim_script_close(fileHandle);
        bgAnimScriptLoaded = 1;
    }

    /* ---- publish runtime-animation state and reset the condition tables ---- */
    bgAnimStaticTable = table;
    bgRuntimeAnimations = runtimeArr;
    bgRuntimeAnimationCount = runtimeCount;

    BG_InitWeaponStrings();

    memset(bgAnimConditionAliases, 0, sizeof(bgAnimConditionAliases));
    memset(bgAnimConditionAliasStringBuffer, 0, sizeof(bgAnimConditionAliasStringBuffer));
    memset(bgAnimConditionAliasCounts, 0, sizeof(bgAnimConditionAliasCounts));
    bgAnimConditionAliasStringUsed = 0;

    data_p = bgAnimScriptFileBuffer;
    Com_BeginParseSession(STR_SESSION_NAME);

    /* ---- main parse loop (0x300025a3) ---- */
    for (;;) {
        int32_t s;

        token = bg_compat_anim_get_token(&data_p, qtrue); /* main-loop unget-restore + Com_ParseExt(,1) */
        if (token == NULL || token[0] == 0) {
            break; /* end of file (0x30002e70) */
        }

        s = BG_IndexForString(token, ANIM_SECTION_TABLE, qtrue);
        if (s >= 0) {
            /* a section keyword: only valid at brace depth 0 */
            if (state != 0) {
                BG_AnimParseError(STR_UNEXPECTED, token);
            }
            currentSection = s;
            bgAnimParseCurrentAnimGroup = ANIM_MT_UNUSED;
            bgAnimParseCurrentEvent = ANIM_EVENT_NONE;
            continue;
        }

        if ((uint32_t)currentSection > 4) {
            continue;
        }

        switch (currentSection) {
        case ANIM_SECTION_DEFINES: {
            /* ---- 0x3000264a: "set <condtype> = <bits>" ---- */
            int32_t condType;
            char *value;
            int32_t base;

            if (Q_stricmpn(STR_SET, token, ANIM_SCRIPT_TEXT_MAX) != 0) {
                continue; /* ignore non-"set" lines */
            }
            token = bg_compat_anim_get_token_on_line(&data_p);
            if (token == NULL || token[0] == 0) {
                BG_AnimParseError(STR_COND_TYPE);
            }
            condType = BG_IndexForString(token, bgAnimConditionTypeStrings, qfalse);
            if (bgAnimConditionTypes[condType].mode != 0) {
                BG_AnimParseError(STR_MAKE_DEFINE, token);
            }
            value = bg_compat_anim_get_token_on_line(&data_p);
            if (value == NULL || value[0] == 0) {
                BG_AnimParseError(STR_COND_DEFINE);
            }
            /* NOT_FROM_ORIGINAL_SOURCE: enforce the per-condition definition
             * capacity before copying a name or indexing the hash and mask rows. */
            if ((uint32_t)bgAnimConditionAliasCounts[condType] >= (uint32_t)BG_ANIM_CONDITION_VALUE_COUNT) {
                BG_AnimParseError(STR_MAX_COND_DEFINES, BG_ANIM_CONDITION_VALUE_COUNT);
                return;
            }
            {
                const char *copiedName = BG_CopyStringIntoBuffer(
                    value, bgAnimConditionAliasStringBuffer, BG_ANIM_CONDITION_ALIAS_STRING_BUFFER_SIZE, &bgAnimConditionAliasStringUsed);

                base = coduo_int32_from_bits((uint32_t)bgAnimConditionAliasCounts[condType] + ((uint32_t)condType << 4));
                bgAnimConditionAliases[base].name = copiedName;
            }
            {
                uint32_t valueHash = BG_StringHashValue(value);

                /* Retail reloads the live count after the hash call instead of
                 * retaining the name-store index from above. */
                base = coduo_int32_from_bits((uint32_t)bgAnimConditionAliasCounts[condType] + ((uint32_t)condType << 4));
                bgAnimConditionAliases[base].hash = valueHash;
            }

            token = bg_compat_anim_get_token_on_line(&data_p);
            if (token == NULL) {
                BG_AnimParseError(STR_EQ_EOL);
            } else if (Q_stricmpn(STR_EQUALS, token, ANIM_SCRIPT_TEXT_MAX) != 0) {
                BG_AnimParseError(STR_EQ_FOUND, token);
            }
            /* stringTable arg is bgAnimConditionTypes[condType].values (0x3008238c[c]),
             * not the value-names window; result is &bgAnimConditionAliasBits[base]. */
            BG_ParseConditionBits(&data_p, bgAnimConditionTypes[condType].values, condType, &bgAnimConditionAliasBits[base]);
            bgAnimConditionAliasCounts[condType] = coduo_int32_from_bits((uint32_t)bgAnimConditionAliasCounts[condType] + 1u);
            continue;
        }

        case ANIM_SECTION_ANIMATIONS:
        case ANIM_SECTION_CANNED_ANIMATIONS:
            /* ---- 0x3000284f ---- */
            if (Q_stricmpn(STR_OPEN_BRACE, token, ANIM_SCRIPT_TEXT_MAX) == 0) {
                /* shared open-brace handler (0x30002b50): descend one level. */
                if (state >= 3) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                if (idx[state] < 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                state = coduo_int32_from_bits((uint32_t)state + 1u);
                continue;
            }
            if (Q_stricmpn(STR_CLOSE_BRACE, token, ANIM_SCRIPT_TEXT_MAX) == 0) {
                state = coduo_int32_from_bits((uint32_t)state - 1u);
                if (state < 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                if (state == 1) {
                    currentSlot = NULL;
                }
                idx[state] = -1;
                continue;
            }
            if (state == 0) {
                if (idx[0] >= 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                if (Q_stricmpn(STR_STATE, token, ANIM_SCRIPT_TEXT_MAX) != 0) {
                    BG_AnimParseError(STR_EXPECT_STATE);
                }
                token = Com_ParseOnLine(&data_p);
                if (token == NULL) {
                    BG_AnimParseError(STR_STATE_TYPE);
                }
                idx[0] = BG_IndexForString(token, ANIM_STATETYPE_TABLE, qfalse);
                token = Com_Parse(&data_p);
                if (token == NULL || Q_stricmpn(STR_OPEN_BRACE, token, ANIM_SCRIPT_TEXT_MAX) != 0) {
                    BG_AnimParseError(STR_EXPECT_OPEN);
                }
                state = 1;
                continue;
            }
            if (state == 1) {
                int32_t animState;
                /* 0x3000295e: a body part naming while idx[1] is already set
                 * (no braces in between) is an error, mirroring the
                 * statechanges/events guard at 0x30002d0b. */
                if (idx[1] >= 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                animState = BG_IndexForString(token, ANIM_ANIMSTATE_TABLE, qfalse);
                idx[1] = animState;
                /* slot = (idx[0]*18 + animState)*0x204 into the animations/canned
                 * region == region[idx[0]][animState] (LEA x9, x2 fan-out). */
                if (currentSection == ANIM_SECTION_ANIMATIONS) {
                    currentSlot = &table->animations[idx[0]][animState];
                    bgAnimParseCurrentAnimGroup = (bg_anim_move_type_t)animState; /* only "animations" sets this */
                } else { /* ANIM_SECTION_CANNED_ANIMATIONS */
                    currentSlot = &table->canned[idx[0]][animState];
                }
                memset(currentSlot, 0, sizeof(*currentSlot));
                continue;
            }
            if (state == 2) {
                if (idx[2] >= 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                bg_compat_unget_token_and_verify(&data_p, token);
                memset(&block, 0, sizeof(block));
                idx[2] = BG_ParseConditions(&data_p, &block); /* returns qtrue */
                lastCommandBlock = bg_compat_append_command_block(table, currentSlot, &block);
                continue;
            }
            /* state == 3: command body (no idx guard at 0x30002ac5). 0x30002b10
             * passes the [ESP+0x24] slot — the LAST APPENDED command block the
             * conditions pass just linked — not currentSlot. */
            bg_compat_unget_token_and_verify(&data_p, token);
            BG_ParseCommands(&data_p, lastCommandBlock, (bg_static_animation_t *)table);
            continue;

        case ANIM_SECTION_STATECHANGES:
        case ANIM_SECTION_EVENTS:
            /* ---- 0x30002b3b ---- */
            if (Q_stricmpn(STR_OPEN_BRACE, token, ANIM_SCRIPT_TEXT_MAX) == 0) {
                if (state >= 3) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                if (idx[state] < 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                state = coduo_int32_from_bits((uint32_t)state + 1u);
                continue;
            }
            if (Q_stricmpn(STR_CLOSE_BRACE, token, ANIM_SCRIPT_TEXT_MAX) == 0) {
                state = coduo_int32_from_bits((uint32_t)state - 1u);
                if (state < 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                if (state == 0) {
                    currentSlot = NULL;
                }
                idx[state] = -1;
                continue;
            }
            if (state == 0) {
                if (idx[0] >= 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                if (currentSection == ANIM_SECTION_STATECHANGES) {
                    int32_t toState, fromState;
                    char *fromToken;
                    if (Q_stricmpn(STR_STATECHANGE, token, ANIM_SCRIPT_TEXT_MAX) != 0) {
                        BG_AnimParseError(STR_EXPECT_SCHANGE, token);
                    }
                    token = Com_ParseOnLine(&data_p);
                    if (token == NULL) {
                        BG_AnimParseError(STR_STATE_TYPE2);
                    }
                    toState = BG_IndexForString(token, ANIM_STATETYPE_TABLE, qfalse);
                    fromToken = Com_ParseOnLine(&data_p);
                    if (fromToken == NULL) {
                        BG_AnimParseError(STR_STATE_TYPE2);
                    }
                    fromState = BG_IndexForString(fromToken, ANIM_STATETYPE_TABLE, qfalse);
                    /* 0x30002c57..0x30002c67 computes
                     * (toState * 4 + fromState) * 0x204. Linux retail
                     * 0x1bb39..0x1bb68 proves the same token order and layout. */
                    idx[0] = fromState;
                    currentSlot = &table->statechanges[toState][fromState];
                    token = Com_Parse(&data_p);
                    if (token == NULL || Q_stricmpn(STR_OPEN_BRACE, token, ANIM_SCRIPT_TEXT_MAX) != 0) {
                        BG_AnimParseError(STR_EXPECT_OPEN);
                    }
                    state = 1;
                    memset(currentSlot, 0, sizeof(*currentSlot));
                    continue;
                } else { /* events */
                    int32_t eventType = BG_IndexForString(token, ANIM_EVENTTYPE_TABLE, qfalse);
                    idx[0] = eventType;
                    /* eventType*0x204 into the events region == events[eventType]. */
                    currentSlot = bg_compat_anim_parser_event_list(table, eventType);
                    bgAnimParseCurrentEvent = (bg_anim_event_t)eventType;
                    memset(currentSlot, 0, sizeof(*currentSlot));
                    continue;
                }
            }
            if (state == 1) {
                if (idx[1] >= 0) {
                    BG_AnimParseError(STR_UNEXPECTED, token);
                }
                bg_compat_unget_token_and_verify(&data_p, token);
                memset(&block, 0, sizeof(block));
                idx[1] = BG_ParseConditions(&data_p, &block);
                lastCommandBlock = bg_compat_append_command_block(table, currentSlot, &block);
                continue;
            }
            /* 0x30002e06: only state == 2 reaches the command body; state == 3
             * (structurally unreachable here, idx[2] is set only in the
             * animations section) errors out. */
            if (state != 2) {
                BG_AnimParseError(STR_UNEXPECTED, token);
            }
            /* 0x30002e58 passes the last appended command block ([ESP+0x24]),
             * not currentSlot. */
            bg_compat_unget_token_and_verify(&data_p, token);
            BG_ParseCommands(&data_p, lastCommandBlock, (bg_static_animation_t *)table);
            continue;

        default:
            continue;
        }
    }

    /* ---- end of file (0x30002e70) ---- */
    if (state != 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: EOF has no current token, so supply an
         * empty suffix for the diagnostic's required string argument. */
        BG_AnimParseError(STR_END_OF_FILE, "");
    }
    bgPlayerAnimScriptPath = NULL;
    Com_EndParseSession();
    /* i386 /GS __security_check_cookie + register-restore epilogue omitted. */
}
