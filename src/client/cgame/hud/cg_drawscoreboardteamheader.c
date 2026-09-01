// Source: uo_cgame_mp_x86.dll 0x30037090..0x30037417
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30037090_30037417.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>

/*
 * CG_DrawScoreboardTeamHeader (0x30037090)
 *
 * Draws one multiplayer-scoreboard team-section header: the "<team name> (N
 * players)" banner line and, for the AXIS/ALLIES sections, a per-team totals row
 * (the team score in the score column and average ping in the ping column).
 * Returns the Y at which the section's client rows should begin (the input Y
 * advanced past the 24-pixel banner). Called four times per frame by
 * CG_DrawScoreboardBody (0x30037b50), once per team section.
 *
 * NAME: the .mcode mechanical pre-hint "G_Damage" is REJECTED. G_Damage is a
 * SERVER combat/damage routine (game_mp.dll); it does no drawing, touches no
 * cgame traps, and shares nothing with this body. The size-match (win 0x387 vs
 * 0x38c) is exactly the size-based misidentification the contract forbids. This
 * function is pure cgame scoreboard UI: it formats a localized player-count
 * banner via CG_SE_LOCALIZE_MESSAGE, registers/sets a 2D shader (traps 89/72), draws the
 * banner background (CG_DrawPic), the team name and the totals columns
 * (CG_R_TEXT_PAINT text draws), and walks the static cg_scoreboardColumns[] table.
 * The name CG_DrawScoreboardTeamHeader is provisional, chosen by proven role
 * (it is the per-team banner drawer invoked by CG_DrawScoreboardBody, adjacent
 * to CG_DrawScoreboard_ListColumnHeaders / CG_DrawClientScore); the exact CoD:UO
 * symbol is not proven from the allowed inputs.
 *
 * ABI (proven from the four call sites 0x30037c49/c8f/cda/d28 in
 * CG_DrawScoreboardBody):
 *   - drawCtx : arg0, cdecl stack slot [ESP+0xb8] (a cgScoreboardDrawCtx_t *; only
 *               +0x0c, the fade alpha, is read -> MOV ECX,[EAX+0xc]).
 *   - y       : arg1, cdecl stack float [ESP+0xbc] (the section's top Y).
 *   - boardWidth : arg2, cdecl stack float [ESP+0xc0] (the caller's ESI, 374.0f
 *               or 382.0f; the banner-bar pixel width forwarded to CG_DrawPic).
 *   - bannerHeight : arg3, cdecl stack float [ESP+0xc4] (0x42000000 = 32.0f in
 *               every caller; the banner-bar pixel height for CG_DrawPic).
 *   - team    : EBX register argument (0/1/2 = the team_t section selector).
 *   - lineCounter : EDX register argument, `int *` (the running scoreboard line
 *               index; `*lineCounter` is compared/incremented for scroll clipping).
 *   - returns : float, on the x87 stack (y + 24.0f). Caller `FSTP [ESP+0x3c]`.
 *   RET with no immediate (caller-cleaned cdecl for the four stack slots; the two
 *   register args are not on the stack). Modeled as ordered parameters; no
 *   calling-convention attribute is added because the syntax-only build does not
 *   need one.
 *
 * SCROLL / OVERFLOW clipping (shared with CG_DrawClientScore, which reads the same
 * two globals):
 *   - cg_scoreboardOverflowed (0x3048a564): once a drawn line has flowed past the
 *     bottom of the visible area this latch is set; further header/row draws for
 *     the frame early-out. The body drawer clears it to 0 at frame start.
 *   - cg_scoreboardScrollPos (0x3048a560): the top visible line index. Lines whose
 *     running index is below the scroll position are skipped (counter advanced,
 *     nothing drawn).
 *
 * The /GS stack cookie is snapshotted on entry (__security_cookie into a local)
 * and verified on every exit via __security_check_cookie (0x30061639); that is an
 * MSVC calling-convention detail, recorded here but not part of the source logic.
 */

/* .rdata float constants (dumped by address; not inferred from neighbors). */
#define CG_SB_BANNER_HEIGHT      24.0f  /* [0x3007bdd4]: banner line height (Y advance) */
#define CG_SB_BOTTOM_LIMIT      432.0f  /* [0x3007bdc0]: visible-area bottom Y limit */
#define CG_SB_NAME_Y_OFFSET      18.0f  /* [0x3007bf10]: team-name baseline below Y */
#define CG_SB_ZERO                0.0f  /* [0x3007bcec]: 0.0f (non-measured x offset) */
#define CG_SB_BANNER_X          129.0f  /* 0x43010000: banner-bar / totals left X */
#define CG_SB_NAME_X            133.0f  /* 0x43050000: team-name draw X */
#define CG_SB_TEXT_SCALE         0.32f  /* 0x3ea3d70a: text scale for name/totals draws */

/* Fixed trailing mode word in the CG_R_TEXT_PAINT draw vector (PUSH 0x3). */
enum { CG_SB_TRAP54_MODE = 3 };

/* Shader-registration mode passed to CG_R_REGISTERSHADER for the banner bar. */
enum { CG_SB_BANNER_SHADER_MODE = 5 };

/*
 * Per-column value-source selector read at cg_scoreboardColumns[i].valueSelect
 * (+0x0c) — the field the header drawer at 0x30036d60 left `reserved`. This
 * function proves it is meaningful: only columns whose selector is 1 or 3 draw a
 * per-team aggregate value; every other selector (0/2/4) skips the column body.
 * Values from the .rdata table: {4,0,1,2,3} across the standalone pre-array dword
 * (0x30071a60) and cg_scoreboardColumns[0..3]. Provisional names by proven role.
 */
enum {
    CG_SB_VALSEL_TEAMSCORE = 1, /* draw cg_scoreboardTeamScores[team] (score column) */
    CG_SB_VALSEL_TEAMPING  = 3  /* draw cg_scoreboardTeamPings[team]  (ping column) */
};

/*
 * The banner player-count formats, localized via CG_SE_LOCALIZE_MESSAGE. Each has a "\x15"
 * (0x15) control prefix before "%i" — the string-editor width/pad code the CoD
 * localizer consumes. Dumped at their .rdata addresses.
 */
#define CG_SB_FMT_PLAYERS_SINGULAR "CGAME_SB_PLAYER\x15%i"   /* 0x30079cf8 */
#define CG_SB_FMT_PLAYERS_PLURAL   "CGAME_SB_PLAYERS\x15%i"  /* 0x30079ce4 */
#define CG_SB_KEY_BANNER_TEXT      "scoreboard banner text"  /* 0x30079d0c */
#define CG_SB_FMT_INT              "%i"                        /* 0x300769e0 */

/* Section selector encoded as `team - 0` / DEC EAX chain (0x30037171): the caller
 * passes team in EBX; team 0 => "None"/spectator-band banner, 1 => Axis, 2 =>
 * Allies, others => the default (spectators). Named provisionally by role. */
enum {
    CG_SB_SECTION_NONE   = 0,
    CG_SB_SECTION_AXIS   = 1,
    CG_SB_SECTION_ALLIES = 2
};

float CG_DrawScoreboardTeamHeader(const cgScoreboardDrawCtx_t *drawCtx, float y,
                                  float boardWidth, float bannerHeight, int team,
                                  int *lineCounter)
{
    /* 0x300370a2: if the scoreboard has already overflowed the visible area this
     * frame, draw nothing and return the input Y unchanged (JNZ 0x300370b8, the
     * return-arg1 epilogue — the scroll/bottom checks are skipped entirely). */
    if (cg_scoreboardOverflowed) {
        return y; /* 0x300370b8: FLD arg1; cookie check; RET */
    }

    /* 0x300370ab: lines above the current scroll position are skipped; the running
     * line counter is still advanced so the scroll math stays in step. */
    if (*lineCounter < cg_scoreboardScrollPos) {
        *lineCounter = coduo_int32_from_bits((uint32_t)*lineCounter + 1u);
        return y; /* 0x300370b8: FLD arg1; cookie check; RET */
    }

    /* 0x300370d2: banner bottom = Y + 24; if it has flowed past the bottom limit,
     * latch the overflow flag and stop. (FCOMP 432.0; TEST AH,0x41 => st0<=432.) */
    long double bannerBottomWide =
        (long double)y + (long double)CG_SB_BANNER_HEIGHT;
    float bannerBottom = (float)bannerBottomWide; /* 0x300370df FST retained */
    if (bannerBottomWide > (long double)CG_SB_BOTTOM_LIMIT) {
        cg_scoreboardOverflowed = 1;                /* 0x300370f0 MOV [..],1 */
        return y;                                   /* 0x300370fa: return arg1 */
    }

    /* 0x30037114: this line is visible and drawn — advance the line counter. */
    *lineCounter = coduo_int32_from_bits((uint32_t)*lineCounter + 1u);

    /* 0x30037118: fade alpha from drawCtx->color[3] at +0x0c. */
    float alpha = drawCtx->color[3]; /* MOV ECX,[EAX+0xc] */

    /* 0x30037121: banner text uses the singular "PLAYER" form when this team has
     * exactly one row, else the plural "PLAYERS" form. cg_scoreboardTeamCount is
     * indexed by team. va("...\x15%i", count) formats the count, then CG_SE_LOCALIZE_MESSAGE
     * localizes it under the "scoreboard banner text" key. */
    int32_t teamCount = cg_scoreboardTeamCount[team];
    const char *countFmt = (teamCount == 1) ? CG_SB_FMT_PLAYERS_SINGULAR
                                            : CG_SB_FMT_PLAYERS_PLURAL;
    const char *countText = va(countFmt, teamCount);
    const char *bannerText =
        (const char *)(intptr_t)cgame_syscall(CG_SE_LOCALIZE_MESSAGE,
                                              (intptr_t)countText,
                                              (intptr_t)CG_SB_KEY_BANNER_TEXT);

    /* The single color vector passed to every 2D draw starts as
     * {1,1,1,alpha}. The RGB stores are at 0x3003712d..0x30037144 and the
     * context-alpha store is at 0x30037145. After the banner draw,
     * 0x3003729c..0x300372a1 passes this same stack address to
     * CG_DrawScoreboard_GetTeamColor, which overwrites only RGB and deliberately
     * preserves the alpha lane for the team-name and totals text draws. */
    vec4_t sectionColor;
    sectionColor[0] = 1.0f;
    sectionColor[1] = 1.0f;
    sectionColor[2] = 1.0f;
    sectionColor[3] = alpha; /* [ESP+0x2c] = drawCtx->color[3] (raw dword copy) */

    /* 0x30037171: section dispatch by team (the SUB/DEC EAX chain). Each branch
     * localizes the team name into `teamName` and reads the section banner icon
     * cvar into a local scratch buffer. char[0x40] scratch mirrors the LEA/PUSH
     * 0x40 buffers the traps fill. */
    char iconCvarValue[64];    /* [ESP+0x3c] region: banner-icon cvar value
                                  *   (fed to CG_R_REGISTERSHADER) */
    char nameCvarValue[64];    /* [ESP+0x8c] region: team-name cvar value
                                  *   (fed to CG_SE_LOCALIZE_MESSAGE to localize) */
    const char *teamName;

    if (team == CG_SB_SECTION_NONE) {
        /* 0x3003723f: spectator/none banner — read only the banner-icon cvar; the
         * team name is the localized banner text itself. */
        trap_Cvar_VariableStringBuffer(cg_scoreboardNoneBannerCvarName, iconCvarValue,
                                       sizeof(iconCvarValue));
        teamName = bannerText;   /* EDI retained from the banner localize */
    } else if (team == CG_SB_SECTION_ALLIES) {
        /* 0x30037200: Allies. Read banner-icon cvar and the team-name cvar, then
         * localize the team name. */
        trap_Cvar_VariableStringBuffer(cg_scoreboardAlliesBannerCvarName, iconCvarValue,
                                       sizeof(iconCvarValue));
        trap_Cvar_VariableStringBuffer(cg_scoreboardAlliesTeamNameCvarName, nameCvarValue,
                                       sizeof(nameCvarValue));
        const char *localized =
            (const char *)(intptr_t)cgame_syscall(CG_SE_LOCALIZE_MESSAGE,
                                                  (intptr_t)nameCvarValue,
                                                  (intptr_t)cg_scoreboardTeamNameLocalizationContext);
        teamName = va(cg_scoreboardTeamNameFormat, localized, bannerText);
    } else if (team == CG_SB_SECTION_AXIS) {
        /* 0x300371c4: Axis. Same shape as Allies with the Axis cvars. */
        trap_Cvar_VariableStringBuffer(cg_scoreboardAxisBannerCvarName, iconCvarValue,
                                       sizeof(iconCvarValue));
        trap_Cvar_VariableStringBuffer(cg_scoreboardAxisTeamNameCvarName, nameCvarValue,
                                       sizeof(nameCvarValue));
        const char *localized =
            (const char *)(intptr_t)cgame_syscall(CG_SE_LOCALIZE_MESSAGE,
                                                  (intptr_t)nameCvarValue,
                                                  (intptr_t)cg_scoreboardTeamNameLocalizationContext);
        teamName = va(cg_scoreboardTeamNameFormat, localized, bannerText);
    } else {
        /* 0x30037184: default (spectators). Read the CGAME_SPECTATORS-band cvar,
         * localize CGAME_SPECTATORS as the section name. */
        trap_Cvar_VariableStringBuffer(cg_scoreboardSpectatorsBannerCvarName, iconCvarValue,
                                       sizeof(iconCvarValue));
        const char *localized =
            (const char *)(intptr_t)cgame_syscall(CG_SE_LOCALIZE_MESSAGE,
                                                  (intptr_t)cg_scoreboardSpectatorsLocalizationKey,
                                                  (intptr_t)cg_scoreboardTeamNameLocalizationContext);
        teamName = va(cg_scoreboardTeamNameFormat, localized, bannerText);
    }

    /* 0x30037254: pump the loading-information screen once. */
    CG_DrawInformation(0);

    /* 0x3003725b: register the banner-icon shader named by `iconCvarValue`
     * (CG_R_REGISTERSHADER, mode 5) and set it as the current 2D draw color source
     * (CG_R_SETCOLOR forwards the {1,1,1,alpha} color). */
    qhandle_t bannerShader =
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER,
            (intptr_t)iconCvarValue,
            CG_SB_BANNER_SHADER_MODE));
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)sectionColor);

    /* 0x30037279..0x30037297: draw the banner bar (CG_DrawPic). The registered
     * shader fills x=129, width=boardWidth, y=arg1, height=32. */
    CG_DrawPic(CG_SB_BANNER_X, y, boardWidth, bannerHeight, bannerShader);

    /* 0x3003729c: overwrite the section color's RGB lanes while retaining the
     * fade alpha already stored in lane 3. */
    CG_DrawScoreboard_GetTeamColor(team, sectionColor);

    /* 0x300372a6..0x300372f3: draw the team-name text (CG_R_TEXT_PAINT) at x=133,
     * y = arg1 + 18, scale 0.32, using the section color and the localized name. */
    float nameY = (float)(
        (long double)y + (long double)CG_SB_NAME_Y_OFFSET);
    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(CG_SB_NAME_X),
                  CG_FloatBits(nameY),
                  0,
                  CG_FloatBits(CG_SB_TEXT_SCALE),
                  (intptr_t)sectionColor,
                  (intptr_t)teamName,
                  0,
                  0,
                  CG_SB_TRAP54_MODE);

    /* 0x300372fc: only the AXIS and ALLIES sections draw the per-team totals row.
     * Every other section returns the banner Y advance now. */
    if (team == CG_SB_SECTION_AXIS || team == CG_SB_SECTION_ALLIES) {
        /* 0x3003730a: totals row X cursor starts at 129.0f. */
        float xCursor = CG_SB_BANNER_X;
        int32_t i;

        for (i = 0; i < CG_SCOREBOARD_COLUMN_COUNT; ++i) {
            const cgScoreboardColumn_t *col = CG_SCOREBOARD_COLUMN(i);

            /* 0x30037314: the value-source selector (the cg_scoreboardColumns
             * next-column selector lane read one entry early, base 0x30071a60).
             * Selectors 1
             * and 3 draw a per-team aggregate; all others skip. */
            int32_t valueSelect = CG_SCOREBOARD_VALUE_SELECT(i);

            if (valueSelect == CG_SB_VALSEL_TEAMSCORE ||
                valueSelect == CG_SB_VALSEL_TEAMPING) {
                /* 0x3003732d/0x30037336: choose the per-team value array. */
                int32_t value = (valueSelect == CG_SB_VALSEL_TEAMSCORE)
                                    ? cg_scoreboardTeamScores[team]
                                    : cg_scoreboardTeamPings[team];

                /* 0x3003733d: format the integer value (va("%i", value)). */
                const char *valueText = va(CG_SB_FMT_INT, value);

                /* 0x3003734a: right-align measured columns (mode==2). Neither branch
                 * rounds: the measured branch leaves FMUL/FISUB in st(0) (0x30037383,
                 * no FSTP) and the zero branch FLDs 0.0f (0x3003738c); both merge at
                 * the shared FADD xCursor (0x30037396), whose FSTP DWORD at
                 * 0x300373a6 is the single rounding. Same shape as the sibling in
                 * cg_drawscoreboard_listcolumnheaders.c. */
                long double xInCell;
                if (col->mode == CG_SB_COLUMN_MODE_MEASURED) {
                    /* 0x30037358 trap 52: measure the text width. Args
                     * (push order): valueText, 0, bits(0.32f), 0 -> int32 width.
                     * 0x30037372 FLD boardWidth; FMUL col->widthFraction;
                     * 0x30037383 FISUB dword (int)width => right-aligned in-cell x.
                     * FISUB is an INTEGER subtract, so textWidth stays exact in 80-bit
                     * (no (float) cast, which would round it first). */
                    int32_t textWidth =
                        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_TEXT_WIDTH,
                                      (intptr_t)valueText,
                                      0,
                                      CG_FloatBits(CG_SB_TEXT_SCALE),
                                      0));
                    xInCell =
                        (long double)boardWidth *
                            (long double)col->widthFraction -
                        (long double)textWidth;
                } else {
                    /* 0x3003738c FLD [0x3007bcec] = 0.0f. */
                    xInCell = CG_SB_ZERO;
                }

                /* 0x30037396 FADD xCursor => absolute draw X. */
                float drawX = xInCell + xCursor;

                /* 0x300373d2 trap 54: draw the value text in the section color.
                 * 10-slot vector: (54, bits(drawX), bits(nameY), 0, bits(0.32f),
                 * sectionColor, valueText, 0, 0, 3). The Y reuses the value stored
                 * at the totals row baseline (MOV EAX,[ESP+0x34] = bits(nameY)). */
                cgame_syscall(CG_R_TEXT_PAINT,
                              CG_FloatBits(drawX),
                              CG_FloatBits(nameY),
                              0,
                              CG_FloatBits(CG_SB_TEXT_SCALE),
                              (intptr_t)sectionColor,
                              (intptr_t)valueText,
                              0,
                              0,
                              CG_SB_TRAP54_MODE);
            }

            /* 0x300373dd: advance the running cursor by this column's width
             * (FLD boardWidth; FMUL col->widthFraction via the pre-incremented
             * base-0x10 displacement == the current entry). */
            xCursor = (float)(
                (long double)boardWidth *
                    (long double)col->widthFraction +
                (long double)xCursor);
        }
    }

    /* 0x300373fe: verify the /GS cookie and return the banner Y advance. The
     * returned value is the slot written at entry (arg1 + 24.0f). */
    return bannerBottom;
}
