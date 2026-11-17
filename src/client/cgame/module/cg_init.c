// Source: uo_cgame_mp_x86.dll 0x3002df30..0x3002e385
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002df30_3002e385.mcode

#include "../client_recovered.h"
#include "compat/crt/atof_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * CG_Init is the cgame connection/map-load initializer. The assigned G_RunFrame
 * label is rejected: this body consumes the three CGVM_INIT values, clears the
 * cgs/cg/entity/weapon/item/animation state, obtains glconfig and gameState,
 * precaches map media, then builds the initial vote/timeout HUD strings. The
 * same-module PPC bank names this initialization entry CG_Init, and vmMain's
 * command-1 dispatch proves the call role independently of size.
 *
 * The original i386 ABI carries serverMessageNum in EDX and the remaining two
 * values on the stack. The portable recovered signature keeps their semantic
 * source order.
 */

enum {
    CG_INIT_GAME_VERSION_BYTES = 4,
    CG_INIT_HUD_STAT_CONFIGSTRING = 14
};

void CG_Init(int32_t serverMessageNum, int32_t serverCommandSequence,
             int32_t clientNum)
{
    memset(&cgs, 0, sizeof(cgs));
    memset(&cg, 0, sizeof(cg));
    memset(cg_entities, 0, sizeof(cg_entities));
    memset(cg_weaponInfos, 0, sizeof(cg_weaponInfos));
    memset(cg_items, 0, sizeof(cg_items));
    memset(&bgs, 0, sizeof(bgs));

    cg_effectTime = 0;
    cg_effectAnimTime = 0;
    cg_effectFrameTime = 0;

    bgs.soundAliasCallback = trap_Com_SoundAliasString;
    bgs.soundEventCallback = CG_PlayEntitySoundAliasByName;

    cg.clientNum = clientNum;
    cgs.processedSnapshotNum = serverMessageNum;
    cgs.serverCommandSequence = serverCommandSequence;

    cgs.media.whiteShader =
        CG_RegisterMaterial("white", R_IMAGE_TRACK_HUD);
    cgs.media.hudSoftLineShader =
        CG_RegisterMaterial("hudSoftLine", R_IMAGE_TRACK_HUD);
    cgs.media.hudSoftLineHShader =
        CG_RegisterMaterial("hudSoftLineH", R_IMAGE_TRACK_HUD);

    CG_RegisterCvars();
    CG_InitConsoleCommands();

    cgame_syscall(CG_GET_GLCONFIG, &cgs.glconfig);
    cgs.screenXScale = (float)(
        (long double)cgs.glconfig.vidWidth * (long double)(1.0f / 640.0f));
    cgs.screenYScale = (float)(
        (long double)cgs.glconfig.vidHeight * (long double)(1.0f / 480.0f));

    trap_GetGameState(&cgs.gameState);

    {
        const char *serverGame =
            &cgs.gameState.stringData[cgs.gameState.stringOffsets[2]];
        if (memcmp("cod", serverGame, CG_INIT_GAME_VERSION_BYTES) != 0) {
            CG_Error("Client/Server game mismatch: %s/%s",
                             "cod", serverGame);
        }
    }

    cgs.levelStartTime = coduo_crt_atoi(
        &cgs.gameState.stringData[
            cgs.gameState.stringOffsets[CG_INIT_HUD_STAT_CONFIGSTRING]]);

    /* 0x3002e080..0x3002e09b: MSVC rand returns 0..32767. The x87 code
     * multiplies by 1/32768, doubles, then subtracts 1. */
    cgs.initialRandomValue = (float)(
        (long double)coduo_crt_rand() *
        (long double)(1.0f / 32768.0f) * 2.0L - 1.0L);

    CG_ParseServerinfo();
    CG_ParseWolfinfo();
    InitWeaponInfo();
    CGScr_LoadAnimTrees();

    CG_LoadingString("collision map");
    trap_CM_LoadMap(cgs.mapname);

    String_Init();

    CG_LoadingString("graphics");
    memset(&cg.refdef, 0, sizeof(cg.refdef));
    cgame_syscall(CG_R_CLEAR_SCENE);

    CG_LoadingString(cgs.mapname);
    trap_R_LoadWorldMap(cgs.mapname);

    CG_LoadingString("sound aliases");
    trap_Com_LoadSoundAliases(cgs.mapname);

    CG_LoadingString("sounds");
    CG_RegisterSounds();

    CG_LoadingString("game media");
    CG_RegisterGraphics();

    CG_LoadingString("clients");
    CG_RegisterMenuAssets();
    CG_UIDisplayContextInit();
    CG_InitFlameChunks();
    CG_InitLocalEntities();
    CG_InitMarkPolys();
    /* 0x3002e2eb clears the visible loading label before config-string assets
     * begin registering and pumping the screen. This is distinct from the full
     * empty-label CG_LoadingString operation that follows. */
    cg.infoScreenText[0] = '\0';
    CG_SetConfigValues();

    CG_LoadingString("");

    cg.hudSpinBaseTime = (float)coduo_compat_atof(
        &cgs.gameState.stringData[cgs.gameState.stringOffsets[11]]);

    trap_R_FinishLoadingModels();
    trap_MSS_StopSounds(0);
    CG_StartAmbient();
    CG_BuildVoteHudStrings();
    CG_BuildTimeoutHudStrings();
}
