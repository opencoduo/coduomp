// Source: uo_cgame_mp_x86.dll 0x3002df30..0x3002e385
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002df30_3002e385.mcode

#include "../client_recovered.h"

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
    cgame_compat_reset_recovered_cgs_state();
    cgame_compat_reset_recovered_cg_state();
    memset(cg_entities, 0, sizeof(cg_entities));
    memset(cg_weaponInfos, 0, sizeof(cg_weaponInfos));
    memset(cg_items, 0, sizeof(cg_items));
    memset(&bgs, 0, sizeof(bgs));

    cg_effectTime = 0;
    cg_effectAnimTime = 0;
    cg_effectFrameTime = 0;

    bgs.soundAliasCallback = trap_Com_SoundAliasString;
    bgs.soundEventCallback = CG_PlayEntitySoundAliasByName;

    cg_clientNum = clientNum;
    cg_processedSnapshotNum = serverMessageNum;
    cgs_serverCommandSequence = serverCommandSequence;

    cgs_media_whiteShader =
        CG_RegisterMaterial("white", R_IMAGE_TRACK_HUD);
    cgs_media_hudSoftLineShader =
        CG_RegisterMaterial("hudSoftLine", R_IMAGE_TRACK_HUD);
    cgs_media_hudSoftLineHShader =
        CG_RegisterMaterial("hudSoftLineH", R_IMAGE_TRACK_HUD);

    CG_RegisterCvars();
    CG_InitConsoleCommands();

    cgame_syscall(CG_GET_GLCONFIG, &cgs_glconfig);
    cgs_screenXScale = (float)(
        (long double)cgs_glconfig.vidWidth * (long double)(1.0f / 640.0f));
    cgs_screenYScale = (float)(
        (long double)cgs_glconfig.vidHeight * (long double)(1.0f / 480.0f));

    cgame_syscall(CG_GET_GAME_STATE, &cg_gameState);

    {
        const char *serverGame =
            &cg_gameState.stringData[cg_gameState.stringOffsets[2]];
        if (memcmp("cod", serverGame, CG_INIT_GAME_VERSION_BYTES) != 0) {
            Com_ErrorMessage("Client/Server game mismatch: %s/%s",
                             "cod", serverGame);
        }
    }

    cg_hudStat14Value = coduo_crt_atoi(
        &cg_gameState.stringData[
            cg_gameState.stringOffsets[CG_INIT_HUD_STAT_CONFIGSTRING]]);

    /* 0x3002e080..0x3002e09b: MSVC rand returns 0..32767. The x87 code
     * multiplies by 1/32768, doubles, then subtracts 1. */
    cg_initialRandomValue = (float)(
        (long double)coduo_crt_rand() *
        (long double)(1.0f / 32768.0f) * 2.0L - 1.0L);

    CG_ParseServerinfo();
    CG_SetConfigValues();
    InitWeaponInfo();
    CGScr_LoadAnimTrees();

    CG_LoadingString("collision map");
    cgame_syscall(CG_CM_LOAD_MAP, cgs_mapname);

    String_Init();

    CG_LoadingString("graphics");
    memset(&cg_refdef, 0, sizeof(cg_refdef));
    cgame_syscall(CG_R_CLEAR_SCENE);

    CG_LoadingString(cgs_mapname);
    cgame_syscall(CG_R_LOAD_WORLD_MAP, cgs_mapname);

    CG_LoadingString("sound aliases");
    cgame_syscall(CG_COM_LOAD_SOUND_ALIASES, cgs_mapname);

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
    cg_loadingScratch[0] = '\0';
    CG_RegisterConfigStringAssets();

    CG_LoadingString("");

    cg_hudSpinBaseTime = (float)atof(
        &cg_gameState.stringData[cg_gameState.stringOffsets[11]]);

    cgame_syscall(CG_R_FINISH_LOADING_MODELS);
    cgame_syscall(CG_MSS_STOP_SOUNDS, 0);
    CG_ConfigString3Modified();
    CG_BuildVoteHudStrings();
    CG_BuildTimeoutHudStrings();
}
