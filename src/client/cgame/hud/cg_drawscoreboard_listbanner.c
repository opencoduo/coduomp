// Source: uo_cgame_mp_x86.dll 0x300361d0..0x300368f9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300361d0_300368f9.mcode

#include "../client_recovered.h"
#include "../globals.h"

/*
 * CG_DrawScoreboard_ListBanner (0x300361d0)
 *
 * Draws the header/banner block at the top of the multiplayer scoreboard: a dark
 * background panel, a frame of divider bars, the localized game-type name, the map
 * name, and the server host name. Takes a single float argument `fade`, the global
 * scoreboard alpha (0..1). The panel is drawn at alpha fade*0.55, the bars at
 * fade*0.1; the four text lines are drawn at alpha = fade (the raw argument is
 * copied back into color[3] at 0x3003636b before any text draw).
 *
 * NAMING: the .mcode header's size-matched guess `G_GetActivateEnt` is REJECTED.
 * G_GetActivateEnt is a *server* entity-activation lookup returning gentity_t*;
 * this function does no entity work. It is pure cgame 2D HUD drawing: it registers
 * the "black"/"white" shaders, sets the 2D draw color and issues stretch-pics
 * (trap_R_SetColor / trap_R_DrawStretchPic), then formats/measures/draws text
 * through the cgame text traps (52 width, 53 height, 54 draw). The strings it
 * touches ("scoreboard gametype display", "CGAME_LISTENSERVER", "0.0.0.0:0",
 * "maps"/"mp"/".bsp") and the cgs.gametype/hostname/mapname mirrors it reads prove
 * the scoreboard-banner role. The symbolized Mac cgame confirms the exact source
 * name CG_DrawScoreboard_ListBanner.
 *
 * CoDUOMP.exe's recovered dispatcher identifies the cgame text traps 52/53/54
 * and the banner query traps 128/129. Float args to the
 * variadic traps are forwarded by raw 32-bit bit pattern (FSTP float -> dword PUSH,
 * no double promotion); CG_FloatBits reproduces that. Screen coordinates are the
 * virtual 640x480 UI space scaled by cgs.screenXScale/cgs.screenYScale, the same
 * CG_AdjustFrom640 transform used by CG_DrawStretchPic.
 *
 * ABI: cdecl, one float arg (arg0 at entry+4), plain RET, no return value. The body
 * is /GS-guarded: it snapshots __security_cookie ([0x30081650]) on entry and
 * revalidates it via __security_check_cookie just before RET; that compiler
 * artifact is not modeled.
 */

/* Unbounded limit passed to Q_stricmpn to get a full-string Q_stricmp
 * (EAX = 0x1869f = 99999 at 0x300366f9). */
enum { BANNER_STRICMP_UNBOUNDED = 99999 };

/* Text-fit gate: the (labelWidth + textWidth + 4) sum is compared against 386
 * (CMP ...,0x182) and, on overflow, the scale is shrunk by SHRINK_STEP each pass
 * until it fits or drops to/below MIN_SCALE (FCOMP 0.075f). */
enum { BANNER_FIT_WIDTH_LIMIT = 386, BANNER_TEXT_PAD = 4 };

/* The divider-bar coordinate table (.rdata 0x30071ab0). Each row is {x,y,w,h} in
 * virtual UI units; six rows are drawn with the "white" shader. Values dumped from
 * the DLL; the loop reads row[0..3] via a base pointer at row+8. */
static const vec4_t cg_scoreboardBannerBars[6] = {
    { 123.0f,  25.0f, 394.0f,   2.0f },  /* 0x30071ab0 */
    { 123.0f, 447.0f, 394.0f,   2.0f },  /* 0x30071ac0 */
    { 123.0f,  27.0f,   2.0f, 420.0f },  /* 0x30071ad0 */
    { 515.0f,  27.0f,   2.0f, 420.0f },  /* 0x30071ae0 */
    { 125.0f,  51.0f, 390.0f,   1.0f },  /* 0x30071af0 */
    { 125.0f, 432.0f, 390.0f,   1.0f }   /* 0x30071b00 */
};

/* Inline case-insensitive prefix compare, folding 'a'..'z' to upper-case, exactly
 * as the three unrolled compare loops at 0x30036450 / 0x300364b0 / 0x30036520 do:
 * returns 0 while the first `n` folded characters are equal (or a shared NUL is
 * reached), nonzero otherwise. The DLL open-codes this three times over the
 * map-name buffer; one helper reproduces the identical fold+sign. */
#if defined(_MSC_VER)
#define CG_BANNER_COMPAT_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CG_BANNER_COMPAT_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CG_BANNER_COMPAT_ALWAYS_INLINE inline
#endif

/* NOT_FROM_ORIGINAL_SOURCE: source spelling of the three inlined signed-byte
 * comparison loops; forced inline so it cannot create a recovered function. */
static CG_BANNER_COMPAT_ALWAYS_INLINE int
cgame_compat_banner_casecmpn(const char *a, const char *b, int32_t n)
{
    while (n-- > 0) {
        int32_t ca = (int32_t)(int8_t)*a++;
        int32_t cb = (int32_t)(int8_t)*b++;
        if (ca != cb) {
            if (ca >= 'a' && ca <= 'z')
                ca -= ('a' - 'A');
            if (cb >= 'a' && cb <= 'z')
                cb -= ('a' - 'A');
            if (ca != cb)
                return (ca >= cb) ? 1 : -1;
        }
        if (ca == 0)
            break;
    }
    return 0;
}

void CG_DrawScoreboard_ListBanner(float fade)
{
    vec4_t color;                 /* {r,g,b,a} draw color, reused across draws */
    qhandle_t blackShader;
    qhandle_t whiteShader;
    const char *gametypeToken;
    const char *gametypeText;
    const char *serverAddr;
    char mapName[64];             /* local copy of cgs.mapname (stack buffer) */
    char *mapBase;                /* base map name after "maps/"/"mp/" prefix strip */
    int labelWidth, textWidth;    /* trap-52 width returns for the fit test */
    int metric;                   /* trap-53 height return, for vertical centering */
    float scale;                  /* running text scale in the shrink loops */
    int i;

    /* --- background panel: SetColor({1,1,1, fade*0.55}); draw "black" full rect. */
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = fade * 0.55f;
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);
    CG_DrawInformation(0);
    blackShader = (qhandle_t)cgame_syscall(CG_R_REGISTERSHADER,
                                           (intptr_t)"black", 5);
    trap_R_DrawStretchPic(CG_FloatBits(120.0f * cgs_screenXScale),
                          CG_FloatBits(22.0f * cgs_screenYScale),
                          CG_FloatBits(400.0f * cgs_screenXScale),
                          CG_FloatBits(430.0f * cgs_screenYScale),
                          CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                          CG_FloatBits(1.0f), CG_FloatBits(1.0f),
                          blackShader);

    /* --- divider bars: SetColor({1,1,1, fade*0.1}); draw the "white" bar table. */
    color[3] = fade * 0.1f;
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);
    CG_DrawInformation(0);
    whiteShader = (qhandle_t)cgame_syscall(CG_R_REGISTERSHADER,
                                           (intptr_t)"white", 5);
    for (i = 0; i < 6; i++) {
        trap_R_DrawStretchPic(
            CG_FloatBits(cg_scoreboardBannerBars[i][0] * cgs_screenXScale),
            CG_FloatBits(cg_scoreboardBannerBars[i][1] * cgs_screenYScale),
            CG_FloatBits(cg_scoreboardBannerBars[i][2] * cgs_screenXScale),
            CG_FloatBits(cg_scoreboardBannerBars[i][3] * cgs_screenYScale),
            CG_FloatBits(0.0f), CG_FloatBits(0.0f),
            CG_FloatBits(1.0f), CG_FloatBits(1.0f),
            whiteShader);
    }

    /* --- text alpha: 0x3003635a/0x3003636b — the RAW fade argument is copied back
     * into color[3] before the gametype query; all four text lines draw at
     * alpha = fade, not the bar pass's fade*0.1. */
    color[3] = fade;

    /* --- game-type line: resolve + localize cgs.gametype, center vertically, draw.
     * gametypeToken = trap(128, cgs.gametype); gametypeText = trap(57, token,
     * "scoreboard gametype display"). metric = trap(53, 0, scale 0.41) is the text
     * height; y = 51 - (24 - metric)*0.5. Drawn at x=129, scale 0.41.
     * NB: every text-position integer here (24-metric, 14-metric, textWidth+4) is
     * FILDed straight into its FMUL/FSUBR (e.g. 0x300363c8 FILD; 0x300363d7 FMUL 0.5f;
     * 0x30036666 FILD; 0x3003666f FSUBR 511.0f) with no float store, so it stays exact
     * in 80-bit -- do NOT re-add a (float) cast, which would round the integer. */
    gametypeToken = (const char *)(intptr_t)cgame_syscall(CG_UI_GET_GAMETYPE_DISPLAY_NAME,
                                    (intptr_t)cgs_gametype);
    gametypeText = (const char *)(intptr_t)cgame_syscall(CG_SE_LOCALIZE_MESSAGE,
                                    (intptr_t)gametypeToken,
                                    (intptr_t)"scoreboard gametype display");
    metric = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_HEIGHT, 0, CG_FloatBits(0.41f)));
    int32_t gametypeCenterDelta = coduo_int32_from_bits(
        24u - (uint32_t)metric);
    float gametypeY = (float)(
        (long double)51.0f -
        (long double)gametypeCenterDelta * (long double)0.5f);
    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(129.0f),                              /* x */
                  CG_FloatBits(gametypeY),                           /* y */
                  0,
                  CG_FloatBits(0.41f),                               /* scale */
                  (intptr_t)color,
                  (intptr_t)gametypeText,
                  0, 0, 3);

    /* --- extract the base map name from cgs.mapname ("maps/mp/<name>.bsp"). */
    for (i = 0; ; i++) {
        char c = cgs_mapname[i];
        mapName[i] = c;
        if (c == '\0')
            break;
    }
    /* strip a trailing ".bsp" (case-insensitive) */
    {
        int len = 0;
        while (mapName[len] != '\0')
            len++;
        /* Stock subtracts four without a local length guard. The direct and
         * sole producer, CG_ParseServerinfo, always writes
         * "maps/mp/%s.bsp" (including when the server mapname is empty), so the
         * supported call graph guarantees len >= 12 before this subtraction.
         * Keep the target-dword displacement rather than inventing a redundant
         * branch that is absent from the machine code. */
        int32_t suffixOffset = coduo_int32_from_bits((uint32_t)len - 4u);
        char *suffix = (char *)(
            (uintptr_t)(void *)&mapName[0] +
            (uintptr_t)(intptr_t)suffixOffset);
        if (cgame_compat_banner_casecmpn(suffix, ".bsp", 4) == 0)
            *suffix = '\0';
    }
    /* strip a leading "maps/" or "maps\" — the separator test runs ONLY when the
     * 4-char case-folded compare matched (a mismatch at 0x300364fb jumps past the
     * check at 0x300364fd entirely). */
    mapBase = mapName;
    if (cgame_compat_banner_casecmpn(mapName, "maps", 4) == 0 &&
        (mapName[4] == '/' || mapName[4] == '\\'))
        mapBase = mapName + 5;
    /* strip a further leading "mp/" or "mp\" (same gate: mismatch at 0x30036566
     * skips the [EBP+2] separator check). */
    if (cgame_compat_banner_casecmpn(mapBase, "mp", 2) == 0 &&
        (mapBase[2] == '/' || mapBase[2] == '\\'))
        mapBase += 3;

    /* --- map-name line. The shared scale slot (entry+4) is re-seeded with 0.41f
     * (MOV dword [ESP+0x94],0x3ed1eb85 at 0x3003658a) before the fit test; measure
     * mapBase and gametypeText at 0.41, and if the pair overflows the row, shrink
     * the scale by 0.025 each pass (guarding on scale <= 0.075) until it fits. */
    scale = 0.41f;                                 /* 0x3ed1eb85 */
    labelWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH, (intptr_t)mapBase, 0, CG_FloatBits(0.41f), 0));
    textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH, (intptr_t)gametypeText, 0,
        CG_FloatBits(0.41f), 0));
    int32_t combinedWidth = coduo_int32_from_bits(
        (uint32_t)textWidth + (uint32_t)labelWidth +
        (uint32_t)BANNER_TEXT_PAD);
    if (combinedWidth > BANNER_FIT_WIDTH_LIMIT) {
        for (;;) {
            if (!(scale > 0.075f))
                break;
            scale = (float)((long double)scale - (long double)0.025f);
            labelWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_R_TEXT_WIDTH, (intptr_t)mapBase, 0,
                CG_FloatBits(scale), 0));
            /* gametypeText is measured at the FIXED initial 0.41 scale (the loop
             * only shrinks the map-name label); the machine reads a separate,
             * never-updated slot for this width. */
            textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_R_TEXT_WIDTH, (intptr_t)gametypeText, 0,
                CG_FloatBits(0.41f), 0));
            combinedWidth = coduo_int32_from_bits(
                (uint32_t)textWidth + (uint32_t)labelWidth +
                (uint32_t)BANNER_TEXT_PAD);
            if (combinedWidth <= BANNER_FIT_WIDTH_LIMIT)
                break;
        }
    }
    /* draw mapBase right-aligned: measure at the final scale, x = 511 - (width+4). */
    textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH, (intptr_t)mapBase, 0, CG_FloatBits(scale), 0));
    metric = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_HEIGHT, 0, CG_FloatBits(scale)));
    int32_t paddedMapWidth = coduo_int32_from_bits(
        (uint32_t)textWidth + (uint32_t)BANNER_TEXT_PAD);
    float mapX = (float)(
        (long double)511.0f - (long double)paddedMapWidth);
    int32_t mapCenterDelta = coduo_int32_from_bits(24u - (uint32_t)metric);
    float mapY = (float)(
        (long double)51.0f -
        (long double)mapCenterDelta * (long double)0.5f);
    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(mapX),                          /* x */
                  CG_FloatBits(mapY),                          /* y */
                  0,
                  CG_FloatBits(scale),
                  (intptr_t)color,
                  (intptr_t)mapBase,
                  0, 0, 3);

    /* --- host name line: query the server address, substitute local-listen text. */
    serverAddr = (const char *)(intptr_t)cgame_syscall(CG_CL_GET_SERVER_IP_ADDRESS);
    if (serverAddr != 0 &&
        Q_stricmpn("0.0.0.0:0", serverAddr, BANNER_STRICMP_UNBOUNDED) == 0) {
        serverAddr = CG_SafeTranslateString_Internal("cgame", "CGAME_LISTENSERVER");
    }

    /* fit: the shared scale slot is re-seeded with 0.2f (MOV dword
     * [ESP+0x94],0x3e4ccccd at 0x30036733); measure serverAddr + cgs.hostname at
     * 0.2, and on overflow shrink by 0.01 (guard scale <= 0.075) until the pair
     * fits. Unlike the map loop, BOTH widths are re-measured at the shrinking
     * scale each pass: the FST at 0x300367aa writes the new scale into the slot
     * ([ESP+0x1c] = entry+8-relative slot) that the hostname width call reads
     * back at 0x300367c6. */
    scale = 0.2f;                                  /* 0x3e4ccccd */
    labelWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH, (intptr_t)serverAddr, 0,
        CG_FloatBits(0.2f), 0));
    textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH, (intptr_t)cgs_hostname, 0,
        CG_FloatBits(0.2f), 0));
    combinedWidth = coduo_int32_from_bits(
        (uint32_t)textWidth + (uint32_t)labelWidth +
        (uint32_t)BANNER_TEXT_PAD);
    if (combinedWidth > BANNER_FIT_WIDTH_LIMIT) {
        for (;;) {
            if (!(scale > 0.075f))
                break;
            scale = (float)((long double)scale - (long double)0.01f);
            labelWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_R_TEXT_WIDTH, (intptr_t)serverAddr, 0,
                CG_FloatBits(scale), 0));
            textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_R_TEXT_WIDTH, (intptr_t)cgs_hostname, 0,
                CG_FloatBits(scale), 0));
            combinedWidth = coduo_int32_from_bits(
                (uint32_t)textWidth + (uint32_t)labelWidth +
                (uint32_t)BANNER_TEXT_PAD);
            if (combinedWidth <= BANNER_FIT_WIDTH_LIMIT)
                break;
        }
    }
    /* draw cgs.hostname left at x=129, then serverAddr right at x = 511 - width. */
    metric = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_HEIGHT, 0, CG_FloatBits(scale)));
    int32_t hostCenterDelta = coduo_int32_from_bits(14u - (uint32_t)metric);
    float hostY = (float)(
        (long double)447.0f -
        (long double)hostCenterDelta * (long double)0.5f);
    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(129.0f),                                /* x */
                  CG_FloatBits(hostY),                          /* y */
                  0,
                  CG_FloatBits(scale),
                  (intptr_t)color,
                  (intptr_t)cgs_hostname,
                  0, 0, 3);
    textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH, (intptr_t)serverAddr, 0, CG_FloatBits(scale), 0));
    int32_t paddedAddressWidth = coduo_int32_from_bits(
        (uint32_t)textWidth + (uint32_t)BANNER_TEXT_PAD);
    float addressX = (float)(
        (long double)511.0f - (long double)paddedAddressWidth);
    cgame_syscall(CG_R_TEXT_PAINT,
                  CG_FloatBits(addressX),                       /* x */
                  CG_FloatBits(hostY),                          /* y */
                  0,
                  CG_FloatBits(scale),
                  (intptr_t)color,
                  (intptr_t)serverAddr,
                  0, 0, 3);
}

#undef CG_BANNER_COMPAT_ALWAYS_INLINE
