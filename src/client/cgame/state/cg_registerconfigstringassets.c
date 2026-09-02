// Source: uo_cgame_mp_x86.dll 0x30038830..0x30038e69
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30038830_30038e69.mcode
//
// CG_RegisterConfigStringAssets — precache the config-string-backed HUD assets at
// map load: seed three signed HUD-stat integers from config strings, parse the fog
// config, register the script-menu files, then register the HUD shader, HUD
// material, and server shader config-string ranges. Every shader-range entry is
// preceded by an inlined loading-screen "pump" (the loading-half of CG_DrawActive,
// 0x3001c120, replicated in-place at 0x300389a6..0x30038b33 and
// 0x30038c40..0x30038e32) so the progress screen animates while assets stream in.
//
// The former BG_Player_DoControllersInternal name was a size-only server match and
// is rejected: this function issues no player-controller work; it drives cgame
// asset registration traps.
//
// Machine-code facts and adjudications:
//   * The body NEVER calls CG_DrawInformation (0x3002a530). It INLINES that
//     function's loading pump twice. Each pump: (a) resolves the CS_SERVERINFO
//     "mapname" via Info_ValueForKey and registers "levelshots/<map>.tga" (or the
//     "menu/art/unknownmap" fallback) as a 2D-UI shader via
//     trap_R_RegisterShaderNoMip(name, R_IMAGE_TRACK_UI); (b) resets the 2D color
//     with cgame_syscall(CG_R_SETCOLOR, 0); (c) draws the levelshot full-screen via
//     cgame_syscall(CG_R_DRAWSTRETCHPIC, ...) with x = cgs_screenXScale*0,
//     y = cgs_screenYScale*0, w = cgs_screenXScale*640, h = cgs_screenYScale*480,
//     s/t = (0,0)-(1,1); (d) reads "com_expectedhunkusage" via
//     cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, ...) into a 64-byte buffer and
//     atoi's it, and when positive draws the styled loading bar at (200,468,240,10)
//     with fraction = trap(CG_HUNK_USED)/expected clamped to 1.0; (e) forces a
//     present with cgame_syscall(CG_UPDATE_SCREEN). The pump only runs when
//     cg_snap == NULL AND the reentry latch cg_updateScreenActive == 0, exactly the
//     CG_DrawActive loading-screen gate. The first pump calls the wrapper
//     CG_DrawFilledBarStyled (0x3001c860); the second inlines that wrapper's body
//     (same {0.5,0.5,0.5,0.3}/{1,1,1,0.3} colors, flags 0x18, rect 200,468,240,10)
//     into a direct CG_FilledBar (0x3001c5d0) call — semantically identical.
//   * Config-string reads are inlined as
//     &cg_gameState.stringData[cg_gameState.stringOffsets[cfg]] (offset table
//     0x30440a00, data base 0x30442a00), carrying CG_ConfigString's out-of-range
//     Com_ErrorMessage("CG_ConfigString: bad index: %i", cfg) diagnostic (bounds
//     [0, MAX_CONFIGSTRINGS); the error is emitted but the lookup still proceeds).
//     Same inline idiom as sibling CG_SetConfigValues (0x30038430).
//   * The HUD-stat seeds store into the parallel signed-int trio
//     cg_hudStat5Value/cg_hudStat6Value/cg_hudStat14Value from config strings
//     5, 6, 14.
//
// i386/ABI facts recorded but not source behavior: /GS stack cookie
// (__security_cookie 0x30081650 snapshotted into the frame at entry, verified by
// __security_check_cookie 0x30061639 on exit), the Q_atoi JMP-thunk (0x3005b6ce ->
// 0x3005b646, modeled as coduo_crt_atoi), the interleaved x87 stretch-pic frame
// build, and the register-argument ABIs of Info_ValueForKey (ECX/EBX),
// CG_DrawFilledBarStyled/CG_FilledBar (EBX/ECX/EDX). Omitted from the body.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

#include <stddef.h>
#include <stdint.h>

/* Config-string indices this function touches. The three HUD-stat seeds and the
 * asset-registration ranges are all fixed immediates in the machine code. */
enum {
    /* Signed-int HUD stat parsed once from config string 14. */
    CS_HUD_STAT_14 = 14,
};

/* Loading-screen fallback levelshot when no map name is set (dumped byte-exact from
 * .rdata 0x30077858). */
static const char cg_unknownMapShader[] = "menu/art/unknownmap";

/* The fixed loading progress-bar rectangle in virtual 640x480 UI coordinates
 * (float bit patterns 0x43480000/0x43ea0000/0x43700000/0x41200000). */
enum {
    LOADBAR_UNUSED = 0  /* kept as named float constants at the call sites */
};

#if defined(_MSC_VER)
#define CG_REGISTER_ASSETS_COMPAT_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CG_REGISTER_ASSETS_COMPAT_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CG_REGISTER_ASSETS_COMPAT_ALWAYS_INLINE inline
#endif

/*
 * Resolve config string `cfg` (the inlined CG_ConfigString body): report the shared
 * out-of-range diagnostic and still proceed with the unbounded offset lookup, then
 * return &cg_gameState.stringData[cg_gameState.stringOffsets[cfg]]. Matches the
 * machine's inlined bounds check at every config-string reference in this function.
 */
/* NOT_FROM_ORIGINAL_SOURCE: readable factoring of the repeated inlined
 * config-string lookup graph in CG_RegisterConfigStringAssets. */
static CG_REGISTER_ASSETS_COMPAT_ALWAYS_INLINE const char *cgame_compat_config_string_inline(int32_t cfg)
{
    if (cfg < 0 || cfg >= MAX_CONFIGSTRINGS) {          /* TEST/JL + CMP 0x800/JL */
        Com_ErrorMessage(cg_configStringBadIndexFmt, cfg);
    }
    return &cg_gameState.stringData[cg_gameState.stringOffsets[cfg]];
}

/*
 * The inlined loading-screen pump (the loading-half of CG_DrawActive), replicated
 * verbatim at 0x300389a6..0x30038b33 (first shader loop) and
 * 0x30038c40..0x30038e32 (server shader loop). Runs only while no snapshot is
 * installed and no synchronous redraw is already in progress.
 */
/* NOT_FROM_ORIGINAL_SOURCE: readable factoring of the two duplicated loading
 * pump instruction regions in CG_RegisterConfigStringAssets. */
static CG_REGISTER_ASSETS_COMPAT_ALWAYS_INLINE void cgame_compat_register_assets_loading_pump(void)
{
    const char *mapName;
    qhandle_t levelShotShader;
    char hunkUsageStr[64];   /* [ESP+0x94] region, CvarVariableStringBuffer dst */
    int32_t expectedHunkUsage;
    float loadFraction;

    /* cg_snap != NULL -> a snapshot is installed, skip the loading pump.
     * (0x30038911/0x3003891c MOV/TEST/JNZ 0x30038b38.) */
    if (cg_snap != NULL) {
        return;
    }
    /* Reentry latch: bail if a synchronous redraw is already in progress.
     * (0x30038924/0x30038929 MOV/TEST/JNZ 0x30038b38.) */
    if (cg_updateScreenActive != 0) {
        return;
    }
    cg_updateScreenActive = 1;                          /* 0x30038938 MOV [..cf8],1 */

    /* Clear each server-set load cvar that is currently flagged as set. The value
     * string is the empty literal ""; the key is the cvar name. (0x30038931..0x30038992.) */
    if (cl_serverloadmap.string[0] != '\0') {           /* 0x30038931 MOV AL,[..0030] */
        cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadmap", (intptr_t)"");
    }
    if (cl_serverloadgametype.string[0] != '\0') {      /* 0x30038959 MOV AL,[..8970] */
        cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadgametype", (intptr_t)"");
    }
    if (cl_serverloadwaiting.integer != 0) {            /* 0x30038977 MOV EAX,[..346c] */
        cgame_syscall(CG_CVAR_SET, (intptr_t)"cl_serverloadwaiting", (intptr_t)"0");
    }

    /* Resolve the map name from config string 0 (CS_SERVERINFO): register its
     * "levelshots/<map>.tga" shader, or fall back to "menu/art/unknownmap".
     * (0x30038995..0x300389e2.) Info_ValueForKey takes info in ECX, key in EBX. */
    mapName = Info_ValueForKey(&cg_gameState.stringData[cg_gameState.stringOffsets[0]], "mapname");

    levelShotShader = 0;
    if (mapName != NULL && mapName[0] != '\0') {        /* 0x300389ab..0x300389b2 */
        const char *shaderName = va("levelshots/%s.tga", mapName); /* 0x300389bc CALL va */
        levelShotShader = trap_R_RegisterShaderNoMip(shaderName, R_IMAGE_TRACK_UI); /* 0x300389c5 */
    }
    if (levelShotShader == 0) {                         /* 0x300389cf TEST ESI,ESI / JNZ */
        levelShotShader = trap_R_RegisterShaderNoMip(cg_unknownMapShader, R_IMAGE_TRACK_UI); /* 0x300389da */
    }

    /* Reset the 2D draw color to white (R_SetColor(NULL)). 0x300389e4..0x300389e8. */
    cgame_syscall(CG_R_SETCOLOR, 0);

    /* Draw the levelshot full-screen. Coordinates are virtual->real scaled and
     * forwarded as raw float bit patterns to the stretch-pic trap:
     *   x = cgs_screenXScale * 0.0f,   y = cgs_screenYScale * 0.0f,
     *   w = cgs_screenXScale * 640.0f, h = cgs_screenYScale * 480.0f,  s/t (0,0)-(1,1).
     * (0x300389ee..0x30038a79, FMULs against 0x3007bcec=0.0f / 0x3007bf34=640.0f /
     * 0x3007c148=480.0f; the heavily-interleaved x87 frame is identical to
     * CG_DrawActive's.) */
    {
        float x = (float)((long double)cgs_screenXScale * 0.0L);
        float y = (float)((long double)cgs_screenYScale * 0.0L);
        float w = (float)((long double)cgs_screenXScale * 640.0L);
        float h = (float)((long double)cgs_screenYScale * 480.0L);
        trap_R_DrawStretchPic(CG_FloatBits(x), CG_FloatBits(y), CG_FloatBits(w), CG_FloatBits(h), CG_FloatBits(0.0f),
                              CG_FloatBits(0.0f),  /* s1, t1 */
                              CG_FloatBits(1.0f), CG_FloatBits(1.0f),  /* s2, t2 */
                              levelShotShader);
    }

    /* Progress fraction: read "com_expectedhunkusage" and atoi it; if positive,
     * divide the current hunk usage (trap CG_HUNK_USED) by it, clamp to 1.0, and
     * draw the styled loading bar. (0x30038a7f..0x30038b1f.) */
    cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, (intptr_t)"com_expectedhunkusage", (intptr_t)hunkUsageStr,
                  (int32_t)sizeof(hunkUsageStr));       /* size 0x40 = 64 */
    expectedHunkUsage = coduo_crt_atoi(hunkUsageStr);

    /* The four scratch slots pre-filled with 0.8f (0x3f4ccccd) are dead on every
     * path — loadFraction is always assigned from the division before use — but the
     * write is kept for fidelity. (0x30038aaf..0x30038ac7.) */
    loadFraction = 0.8f;

    if (expectedHunkUsage > 0) {                        /* 0x30038aa9 TEST + 0x30038acf JLE */
        int32_t currentHunkUsage = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_HUNK_USED)); /* 0x30038ad1 trap(0x3f) */
        /* 0x30038add FILD currentHunkUsage; 0x30038ae4 FIDIV expectedHunkUsage keep
         * both byte counts exact (they exceed 2^24, so (float) casts would round --
         * Class 4). 0x30038ae8 FST(float) stores the rounded fraction but *keeps*
         * the 80-bit quotient, which 0x30038aec FCOMPs against 1.0f (Class 8), so
         * the clamp test must read the unrounded value. */
        long double frac = (long double)currentHunkUsage / (long double)expectedHunkUsage;
        loadFraction = (float)frac;                     /* 0x30038ae8 FST DWORD */
        if (frac > 1.0f) {                              /* 0x30038aec FCOMP 1.0; TEST 0x41 */
            loadFraction = 1.0f;                        /* 0x30038af9 store 0x3f800000 */
        }
        CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f, loadFraction); /* 0x30038b1a */
    }

    /* Force an out-of-frame present so the load screen animates, then release the
     * reentry latch. (0x30038b22..0x30038b33.) */
    cgame_syscall(CG_UPDATE_SCREEN);
    cg_updateScreenActive = coduo_int32_from_bits((uint32_t)cg_updateScreenActive - 1u);
}

void CG_RegisterConfigStringAssets(void)
{
    int32_t index;

    /* Seed the three signed HUD-stat integers from config strings 5, 6, 14 (inlined
     * config-string lookups + Q_atoi). (0x30038836..0x30038888.) */
    cg_hudStat5Value = coduo_crt_atoi(&cg_gameState.stringData[cg_gameState.stringOffsets[CS_TEAM_SCORE_AXIS]]);
    cg_hudStat6Value = coduo_crt_atoi(&cg_gameState.stringData[cg_gameState.stringOffsets[CS_TEAM_SCORE_ALLIES]]);
    cg_hudStat14Value = coduo_crt_atoi(&cg_gameState.stringData[cg_gameState.stringOffsets[CS_HUD_STAT_14]]);

    /* Parse the map's fog config string. (0x3003888d CALL CG_ParseFog.) */
    CG_ParseFog();

    /* Register the script-menu files. (0x30038892..0x300388e8, EDI 0x535..0x555.)
     * Each nonempty name is loaded via trap(CG_R_REGISTERMENU); a zero (failed)
     * return reports "Could not load script menu file '%s'\n". */
    for (index = CS_SCRIPTMENUS; index < CS_SCRIPTMENUS + CS_SCRIPTMENUS_COUNT; index++) {
        const char *name = cgame_compat_config_string_inline(index);
        if (name[0] != '\0' && cgame_syscall(CG_R_REGISTERMENU, (intptr_t)name) == 0) {
            Com_ErrorMessage("Could not load script menu file '%s'\n", name);
        }
    }

    /* HUD 2D shaders [22, 38): pump the loading screen, then register each config
     * string (including empty ones) as a 2D shader. (0x300388ea..0x30038b4a.) */
    for (index = CS_STATUS_ICONS; index < CS_HEAD_ICONS; index++) {
        const char *name = cgame_compat_config_string_inline(index);
        cgame_compat_register_assets_loading_pump();
        (void)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)name, R_IMAGE_TRACK_HUD);
    }

    /* HUD materials [38, 53): register each via CG_RegisterMaterial (no pump).
     * (0x30038b50..0x30038b8b.) */
    for (index = CS_HEAD_ICONS; index < CS_LOCATIONS; index++) {
        (void)CG_RegisterMaterial(cgame_compat_config_string_inline(index), R_IMAGE_TRACK_HUD);
    }

    /* Server 2D shaders [1654, 1909): skip empty entries; for each nonempty name
     * pump the loading screen, then register it as a 2D shader.
     * (0x30038b8d..0x30038e4c.) */
    for (index = CS_SHADERS + 1; index < CS_TIMEOUT_TIME; index++) {
        const char *name = cgame_compat_config_string_inline(index);
        if (name[0] == '\0') {                          /* 0x30038bb9 CMP byte,0; JZ 0x30038e45 */
            continue;
        }
        cgame_compat_register_assets_loading_pump();
        (void)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)name, R_IMAGE_TRACK_HUD);
    }
}

#undef CG_REGISTER_ASSETS_COMPAT_ALWAYS_INLINE
