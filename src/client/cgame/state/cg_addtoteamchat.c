// Source: uo_cgame_mp_x86.dll 0x30039390..0x300394f6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30039390_300394f6.mcode
//
// CG_AddToTeamChat(const char *str)
//
// Appends one team-chat message to the team-chat scroll ring
// (globals.h: teamChatMsgs / teamChatMsgTimes / teamChatPos / teamChatLastPos),
// stripping/normalizing Quake3 "^x" color codes and word-wrapping long lines at
// TEAMCHAT_WIDTH. When a line wraps, the active color code is re-emitted at the
// start of the continuation line so the color carries across.
//
// Naming: the .mcode header carried a mechanical "# name CG_Corpse" derived from
// a size match, which the machine code contradicts — this touches the team-chat
// ring buffer, the caret color escape ('^' = 0x5e), the cg_chatHeight_vmCvar.integer /
// cg_chatTime_vmCvar.integer cvar snapshots and cg.time, exactly the CG_AddToTeamChat body
// (CG_AddToTeamChat is in the same-module PPC bank at cgame_mp.dll 0x471e0). The
// size-based CG_Corpse guess is rejected.
//
// ABI: the string argument arrives in ECX (register / __fastcall-style first
// arg); the function takes no stack arguments and returns with a plain RET.
// This is recorded here rather than via a calling-convention attribute per the
// workflow's ABI guidance.
//
// Machine-code facts checked against the .mcode:
//  - chatHeight clamp: cg_chatHeight_vmCvar.integer capped to TEAMCHAT_HEIGHT (CMP ..,8 /
//    JL); if <=0 (signed JLE) the ring is flushed and the function returns.
//  - cg_chatTime_vmCvar.integer <=0 (signed JLE) also flushes the ring and returns.
//  - ring index is (teamChatPos % chatHeight) via CDQ/IDIV (signed), line stride
//    TEAMCHAT_LINE_BYTES (IMUL ..,0x10f).
//  - wrap trigger is len > TEAMCHAT_WIDTH-1 (CMP EBP,0x59 / JLE), i.e. len >= 90.
//  - color-string test: str[0]=='^' && str[1]!=0 && str[1]!='^' &&
//    str[1] in ['0'..'9'] (CMP AL,0x30 JL / CMP AL,0x39 JG).
//  - lastColor tracks the most recent color char (MOVSX -> [ESP+0x10]); firstColor
//    ([ESP+0x14], init -1) is latched to the first color char only while still
//    negative (TEST/JGE). The wrap continuation re-emits "^<firstColor>^<lastColor>".

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* '^' color escape and the wrap-time line-width limit, proven by the loop's
 * `len > 89` compare (TEAMCHAT_WIDTH-1). */
enum {
    Q_COLOR_ESCAPE = '^',   /* 0x5e */
    TEAMCHAT_WRAP_LEN = TEAMCHAT_WIDTH - 1  /* 89 = 0x59 */
};

void CG_AddToTeamChat(const char *str)
{
    int chatHeight;
    int len;
    char *p;     /* current write cursor into a ring line (EDX) */
    char *ls;    /* last-space position in the line, for word wrap (EDI); 0 == none */
    int lastColor;   /* [ESP+0x10]: most recent color char, initial '7' */
    int firstColor;  /* [ESP+0x14]: first color char seen, latched once; init -1 */

    /* esi = cg_chatHeight_vmCvar.integer, clamped to at most TEAMCHAT_HEIGHT. */
    chatHeight = cg_chatHeight_vmCvar.integer;
    if (chatHeight >= TEAMCHAT_HEIGHT) {
        chatHeight = TEAMCHAT_HEIGHT;
    } else if (chatHeight <= 0) {
        /* CMP ESI,EDI(0) / JLE 0x300394e4 */
        teamChatLastPos = 0;
        teamChatPos = 0;
        return;
    }

    if (cg_chatTime_vmCvar.integer <= 0) {
        /* JLE 0x300394e4 */
        teamChatLastPos = 0;
        teamChatPos = 0;
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (teamChatPos < 0 || teamChatLastPos < 0 || teamChatLastPos > teamChatPos) {
        teamChatPos = 0;
        teamChatLastPos = 0;
    }

    len = 0; /* EBP */
    lastColor = '7'; /* MOV dword [ESP+0x10],0x37 */
    firstColor = -1; /* OR EBX,0xffffffff -> MOV [ESP+0x14],EBX */
    ls = (char *)0; /* XOR EDI,EDI */

    p = teamChatMsgs[teamChatPos % chatHeight];
    *p = '\0'; /* MOV byte [EDX],0x0 */

    /* CMP byte [ECX],0x0 / JZ finalize: empty input still writes an empty line. */
    while (*str != '\0') {
        if (len > TEAMCHAT_WRAP_LEN) {
            /* Line is full: close the current line and start a continuation. */
            if (ls != (char *)0) {
                /* Rewind src to just after the last space and back up the write
                 * cursor to that space (word wrap):
                 *   str += (ls - p) + 1;  p = ls;                                */
                str = str + ((ls - p) + 1);
                p = ls;
            }
            *p = '\0'; /* terminate the closed line */

            teamChatMsgTimes[teamChatPos % chatHeight] = coduo_int32_from_bits((uint32_t)cg_time);
            if (teamChatPos == INT32_MAX) {
                int32_t retainedRows = teamChatPos - teamChatLastPos;
                if (retainedRows < chatHeight) {
                    retainedRows++;
                } else {
                    retainedRows = chatHeight;
                }
                teamChatPos = chatHeight + (int32_t)(((uint32_t)INT32_MAX + 1u) % (uint32_t)chatHeight);
                teamChatLastPos = teamChatPos - retainedRows;
            } else {
                teamChatPos++;
            }

            /* Open the next ring line and re-emit the active color so it carries
             * across the wrap: "^<firstColor>^<lastColor>". firstColor is BL of
             * the latched value (0xff-low when no color was seen yet). */
            p = teamChatMsgs[teamChatPos % chatHeight];
            *p++ = Q_COLOR_ESCAPE;
            *p++ = (char)firstColor;
            *p++ = Q_COLOR_ESCAPE;
            *p++ = (char)lastColor;

            len = 0;
            ls = (char *)0;
        }

        if (str != (char *)0 && str[0] == Q_COLOR_ESCAPE && str[1] != '\0' && str[1] != Q_COLOR_ESCAPE && (unsigned char)str[1] >= '0' &&
            (unsigned char)str[1] <= '9') {
            /* Color code "^d": copy both bytes through, tracking the color char. */
            char c;
            *p++ = Q_COLOR_ESCAPE; /* MOV byte [EDX],0x5e ; INC EDX */
            c = str[1]; /* re-read str[1] (MOV AL,[ECX+1]); INC ECX */
            str++;
            lastColor = (int)(signed char)c; /* MOVSX EBX,AL ; MOV [ESP+0x10],EBX */
            *p++ = c; /* MOV byte [EDX],AL ; INC EDX */
            str++;
            if (firstColor < 0) { /* TEST EBX,EBX / JGE skips the latch */
                firstColor = lastColor;
            }
        } else {
            char c = *str; /* MOV AL,[ECX] */
            if (c == ' ') { /* CMP AL,0x20 / JNZ */
                ls = p; /* MOV EDI,EDX: remember last space */
            }
            *p++ = c; /* MOV byte [EDX],AL ; INC EDX */
            str++; /* INC ECX */
            len = coduo_int32_from_bits((uint32_t)len + 1u); /* INC EBP */
        }
    }

    /* Finalize the last line (0x300394a4). */
    *p = '\0';
    teamChatMsgTimes[teamChatPos % chatHeight] = coduo_int32_from_bits((uint32_t)cg_time);

    if (teamChatPos == INT32_MAX) {
        int32_t retainedRows = teamChatPos - teamChatLastPos;
        if (retainedRows < chatHeight) {
            retainedRows++;
        } else {
            retainedRows = chatHeight;
        }
        teamChatPos = chatHeight + (int32_t)(((uint32_t)INT32_MAX + 1u) % (uint32_t)chatHeight);
        teamChatLastPos = teamChatPos - retainedRows;
    } else {
        teamChatPos++;
    }
    if (coduo_int32_from_bits((uint32_t)teamChatPos - (uint32_t)teamChatLastPos) > chatHeight) {
        /* CMP EDX,ESI / JLE skips; else teamChatLastPos = teamChatPos - chatHeight */
        teamChatLastPos = coduo_int32_from_bits((uint32_t)teamChatPos - (uint32_t)chatHeight);
    }
}
