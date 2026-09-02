// Source: uo_cgame_mp_x86.dll 0x30037420..0x300377fd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30037420_300377fd.mcode

#include "../client_recovered.h"

#include <stdint.h>
#include <string.h>

/*
 * CG_DrawClientScore draws one visible row in the multiplayer scoreboard and
 * returns the next row Y. The .mcode's PM_FootstepEvent label is rejected: it was
 * assigned from a near-size match, while this body walks cg_scoreboardColumns,
 * formats cgScore_t fields, registers the "white"/"black" row shaders, and emits
 * the scoreboard's trap-52/trap-54 text draws. The sole caller at 0x30037810 is
 * CG_DrawScoreboard_ScoresList; the behavioral name is also present in the
 * same-module PPC bank.
 *
 * ABI: color/y/entry/boardWidth/alternateShade are five cdecl stack words. The
 * running line counter arrives in EDX and is modeled as the trailing parameter.
 * The caller cleans 0x14 bytes and consumes the float result from ST0.
 */

#define CG_SB_ROW_X_START 129.0f
#define CG_SB_ROW_HEIGHT 12.0f
#define CG_SB_ROW_BOTTOM_LIMIT 432.0f
#define CG_SB_ROW_TEXT_Y_OFFSET 10.5f
#define CG_SB_ROW_TEXT_SCALE 0.22f
#define CG_SB_ROW_ICON_SIZE 13.0f
#define CG_SB_ROW_LOCAL_ALPHA_SCALE 0.2f
#define CG_SB_ROW_ALT_ALPHA_SCALE 0.15f

enum {
    CG_SB_ROW_SHADER_SORT = 5,
    CG_SB_ROW_TRAP54_MODE = 3
};

/* The selector lane begins one dword before cg_scoreboardColumns and is read at
 * 0x300375b0 once per 0x10-byte column. */
typedef enum cgScoreboardRowValueSelect_e {
    CG_SB_ROW_VALUE_NAME = 0,
    CG_SB_ROW_VALUE_SCORE = 1,
    CG_SB_ROW_VALUE_DEATHS = 2,
    CG_SB_ROW_VALUE_PING = 3,
    CG_SB_ROW_VALUE_STATUS_ICON = 4
} cgScoreboardRowValueSelect_t;

float CG_DrawClientScore(const vec_t *color, float y, const cgScore_t *entry, float boardWidth, int alternateShade, int *counter)
{
    /* 0x30037420..0x30037440: once overflow is latched, or while this line is
     * above the scroll position, leave y unchanged. Skipped scroll lines still
     * advance the logical line counter. */
    if (cg_scoreboardOverflowed) {
        return y;
    }
    if (*counter < cg_scoreboardScrollPos) {
        *counter = coduo_int32_from_bits((uint32_t)*counter + 1u);
        return y;
    }

    /* 0x30037441..0x3003746d: TEST AH,0x41 accepts <= 432.0f. A strict > test
     * also preserves the machine's unordered/NaN path (which continues).
     * The sum stays UNROUNDED in st0 for the limit test: FST [ESP+0xc]
     * (0x3003744b, no pop) stores the float copy, FCOMP 432.0 (0x3003744f)
     * compares the 80-bit value. */
    long double nextYSum = (long double)y + (long double)CG_SB_ROW_HEIGHT;
    float nextY = (float)nextYSum;
    if (nextYSum > CG_SB_ROW_BOTTOM_LIMIT) {
        cg_scoreboardOverflowed = qtrue;
        return y;
    }
    *counter = coduo_int32_from_bits((uint32_t)*counter + 1u);

    /* CG_ParseScores normalizes every stored entry->client to 0..63 before
     * publishing the scoreboard row, which supplies the domain used by the
     * unchecked IMUL-by-0x4d0 lookup at 0x30037476..0x30037486. */
    const clientInfo_t *clientState = &bgs.clientinfo[entry->client];
    if (clientState->infoValid == 0) {
        return y;
    }

    /* 0x30037498..0x30037587: the selected/local HUD client gets a white row
     * highlight; other alternating rows get a dimmer black background. */
    const char *backgroundShaderName = (const char *)0;
    float backgroundAlphaScale = 0.0f;
    if (entry->client == cg_clientNum) {
        backgroundShaderName = cg_whiteMaterialName;
        backgroundAlphaScale = CG_SB_ROW_LOCAL_ALPHA_SCALE;
    } else if (alternateShade != 0) {
        backgroundShaderName = cg_blackMaterialName;
        backgroundAlphaScale = CG_SB_ROW_ALT_ALPHA_SCALE;
    }

    if (backgroundShaderName != (const char *)0) {
        vec4_t backgroundColor = {color[0], color[1], color[2], (float)((long double)color[3] * (long double)backgroundAlphaScale)};

        CG_DrawInformation(0);
        qhandle_t shader =
            coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)backgroundShaderName, CG_SB_ROW_SHADER_SORT));
        cgame_syscall(CG_R_SETCOLOR, (intptr_t)backgroundColor);
        CG_DrawPic(CG_SB_ROW_X_START, y, boardWidth, CG_SB_ROW_HEIGHT, shader);
    }

    /* 0x3003758b..0x300377eb: five fixed columns, with the running cursor
     * advanced by boardWidth * widthFraction after every column.
     * NOTE: the cell width boardWidth * column->widthFraction is NEVER stored
     * as a float by the machine code -- every consumer recomputes the product
     * inline and keeps it in st0 (0x300375cd, 0x300376c5, 0x3003770e,
     * 0x30037741, 0x300377d0) -- so no cellWidth temporary may exist here. */
    float xCursor = CG_SB_ROW_X_START;
    for (int32_t i = 0; i < CG_SCOREBOARD_COLUMN_COUNT; ++i) {
        const cgScoreboardColumn_t *column = CG_SCOREBOARD_COLUMN(i);
        cgScoreboardRowValueSelect_t valueSelect = (cgScoreboardRowValueSelect_t)CG_SCOREBOARD_VALUE_SELECT(i);

        if (valueSelect == CG_SB_ROW_VALUE_STATUS_ICON) {
            /* 0x300375bc..0x3003763e: an icon occupies a 13x13 box. Measured
             * columns right-align it to the cell's right edge. */
            if (entry->statusIcon != 0) {
                float iconOffset =
                    (column->mode == CG_SB_COLUMN_MODE_MEASURED)
                        ? (float)((long double)boardWidth * (long double)column->widthFraction - (long double)CG_SB_ROW_ICON_SIZE)
                        : 0.0f; /* 0x300375cd..0x300375da: one FSTP */
                vec4_t iconColor = {1.0f, 1.0f, 1.0f, color[3]};

                cgame_syscall(CG_R_SETCOLOR, (intptr_t)iconColor);
                CG_DrawPic((float)((long double)xCursor + (long double)iconOffset), y, CG_SB_ROW_ICON_SIZE, CG_SB_ROW_ICON_SIZE,
                           entry->statusIcon);
            }
        } else {
            const char *text = g_str_empty;

            /* 0x30037651 jump table: four text-producing column cases. */
            switch (valueSelect) {
            case CG_SB_ROW_VALUE_NAME:
                text = clientState->name;
                break;
            case CG_SB_ROW_VALUE_SCORE:
                /* 0x30037661..0x30037668: spectators do not have a score
                 * cell. */
                if (entry->team != TEAM_SPECTATOR) {
                    text = va("%i", entry->score);
                }
                break;
            case CG_SB_ROW_VALUE_DEATHS:
                /* 0x3003766f..0x30037676: spectators do not have a deaths
                 * cell. */
                if (entry->team != TEAM_SPECTATOR) {
                    text = va("%i", entry->deaths);
                }
                break;
            case CG_SB_ROW_VALUE_PING:
                /* 0x30037680..0x30037687: ping is formatted for every team,
                 * including spectators. */
                text = va("%i", entry->ping);
                break;
            case CG_SB_ROW_VALUE_STATUS_ICON:
            default:
                break;
            }

            if (text[0] != '\0') {
                /* Trap 52 returns the integer width for the requested character
                 * limit. A zero limit means the complete string. If it does not
                 * fit, 0x300376d5..0x3003771c decrements strlen(text) until it
                 * finds a fitting prefix. */
                int32_t charLimit = 0;
                int32_t textWidth = coduo_int32_from_bits(
                    (uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)text, 0, CG_FloatBits(CG_SB_ROW_TEXT_SCALE), charLimit));

                /* 0x300376c5/0x3003770e: the fit tests compare the UNROUNDED
                 * boardWidth*widthFraction product (FMUL; FCOMPP) against the
                 * FILD'd width -- 0x300376be FILD feeds the compare with no
                 * intervening FSTP DWORD, so the width is NOT cast to float. */
                if ((long double)boardWidth * (long double)column->widthFraction < (long double)textWidth) {
                    charLimit = coduo_int32_from_bits((uint32_t)strlen(text));
                    /* The stock loop has no explicit zero bound. Its fixed
                     * scoreboard columns are all wider than one glyph, so the
                     * producer/table domain guarantees a fitting nonnegative
                     * prefix before DEC could cross zero. */
                    do {
                        charLimit = coduo_int32_from_bits((uint32_t)charLimit - 1u);
                        textWidth = coduo_int32_from_bits(
                            (uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)text, 0, CG_FloatBits(CG_SB_ROW_TEXT_SCALE), charLimit));
                    } while ((long double)boardWidth * (long double)column->widthFraction < (long double)textWidth);
                }

                /* 0x30037741..0x3003774f: the measured offset stays UNROUNDED
                 * in st0 (FLD boardWidth; FMUL [EBP]; FISUB width -- no store)
                 * until the x argument is rounded once at 0x3003778d. */
                long double textOffset = 0.0f;
                if (column->mode == CG_SB_COLUMN_MODE_MEASURED) {
                    textWidth = coduo_int32_from_bits(
                        (uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)text, 0, CG_FloatBits(CG_SB_ROW_TEXT_SCALE), charLimit));
                    textOffset = (long double)boardWidth * (long double)column->widthFraction -
                                 (long double)textWidth; /* 0x3003774f FISUB: int, no float cast */
                }

                /* 0x3003775e..0x300377c7: white RGB with the section alpha,
                 * drawn at y+10.5 using the fitting character limit. */
                vec4_t textColor = {1.0f, 1.0f, 1.0f, color[3]};
                cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits((float)((long double)xCursor + textOffset)),
                              CG_FloatBits((float)((long double)y + (long double)CG_SB_ROW_TEXT_Y_OFFSET)), 0,
                              CG_FloatBits(CG_SB_ROW_TEXT_SCALE), (intptr_t)textColor, (intptr_t)text, 0, charLimit, CG_SB_ROW_TRAP54_MODE);
            }
        }

        /* 0x300377d0..0x300377e7: FLD boardWidth; FMUL [EBP]; FADD xCursor;
         * FSTP xCursor -- the product is never rounded separately. */
        xCursor = (float)((long double)xCursor + (long double)boardWidth * (long double)column->widthFraction);
    }

    return nextY;
}
