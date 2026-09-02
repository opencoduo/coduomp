#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "renderer_cvars.h"
#include "../animation/dobj.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    R_WAVE_TABLE_SIZE = 1024,
    R_WAVE_TABLE_HALF = 512,
    R_WAVE_TABLE_QUARTER = 256,
    R_IMAGE_HASH_SIZE = 4096,
    R_MIN_POLYS = 4096,
    R_MIN_POLY_VERTS = 16384,
    R_DIFFUSE_SUN_QUALITY_MIN = 0,
    R_DIFFUSE_SUN_QUALITY_MAX = 2,
    R_DIFFUSE_SUN_STEPS_MIN = 1,
    R_DIFFUSE_SUN_STEPS_MAX = 5
};

/* Exact single-precision constants loaded by the Windows x87 sequence. Their
 * mathematical forms are 360/1024, pi, 1/180, 1/1024, and 1/256. Keeping the
 * binary values explicit prevents host headers from selecting a different pi
 * approximation. */
#define R_WAVE_DEGREES_PER_STEP 0.3515625f             /* 0x3eb40000 */
#define R_PI_F                  3.1415927410125732f     /* 0x40490fdb */
#define R_INV_180_F             0.0055555556900799274f /* 0x3bb60b61 */
#define R_INV_WAVE_TABLE_F      0.0009765625f           /* 0x3a800000 */
#define R_INV_WAVE_QUARTER_F    0.00390625f             /* 0x3b800000 */
#define R_DIFFUSE_SUN_HALF_WEIGHT 0.5f                   /* 0x3f000000 */

trGlobals_t tr;
/* Original renderer GL state/config objects at 0x04899e80 and 0x0489a020.
 * Their relative Windows addresses are not a native-layout requirement. */
glstate_t glState;               /* 0x04899e80 */
glconfig_t glConfig;             /* 0x0489a020 */
backEndState_t backEnd;
shaderCommands_t tess; /* 0x03984d40..0x04884da7 */

/* Original surface dispatch table at 0x005ce970. Its 29 entries were read
 * directly from the PE initialized-data image; repeated entries are deliberate
 * aliases for surface formats sharing the same renderer.
 * PE_RELOCATION_VALUES_VERIFIED: all 29 PE pointer targets resolve, in order,
 * to the maintained RB_Surface* functions below. */
renderer_surface_fn_t rb_surfaceTable[] = {
    [R_SURFACE_BAD] = RB_SurfaceBad,
    [R_SURFACE_SKIP] = RB_SurfaceSkip,
    [R_SURFACE_POLY] = RB_SurfacePolychain,
    [R_SURFACE_ENTITY] = RB_SurfaceEntity,
    [R_SURFACE_XMODEL_RIGID] = RB_SurfaceXModelRigid,
    [R_SURFACE_XMODEL_RIGID_SSE] = RB_SurfaceXModelRigidSSE,
    [R_SURFACE_XMODEL_RIGID_ARB] = RB_SurfaceXModelRigidARB,
    [R_SURFACE_XMODEL_RIGID_ATI] = RB_SurfaceXModelRigidATI,
    [R_SURFACE_XMODEL_RIGID_NV] = RB_SurfaceXModelRigidNV,
    [R_SURFACE_XMODEL_WEIGHT] = RB_SurfaceXModelWeight,
    [R_SURFACE_XMODEL_WEIGHT_SSE] = RB_SurfaceXModelWeightSSE,
    [R_SURFACE_STATIC_MODEL] = RB_SurfaceStaticModel,
    [R_SURFACE_CACHED_STATIC_MODEL_GENERIC] = RB_SurfaceStaticModelCached,
    [R_SURFACE_CACHED_STATIC_MODEL_ARB] = RB_SurfaceStaticModelCached,
    [R_SURFACE_CACHED_STATIC_MODEL_ATI] = RB_SurfaceStaticModelCached,
    [R_SURFACE_CACHED_STATIC_MODEL_NV] = RB_SurfaceStaticModelCached,
    [R_SURFACE_STATIC_MODEL_T2V3_GENERIC] =
        RB_SurfaceStaticModelT2V3_Generic,
    [R_SURFACE_STATIC_MODEL_T2V3_ARB] = RB_SurfaceStaticModelT2V3_ARB,
    [R_SURFACE_STATIC_MODEL_T2V3_ATI] = RB_SurfaceStaticModelATI,
    [R_SURFACE_STATIC_MODEL_T2V3_NV] = RB_SurfaceStaticModelT2V3_NV,
    [R_SURFACE_STATIC_MODEL_T2N3V3_GENERIC] =
        RB_SurfaceStaticModelT2N3V3_Generic,
    [R_SURFACE_STATIC_MODEL_T2N3V3_ARB] = RB_SurfaceStaticModelT2N3V3_ARB,
    [R_SURFACE_STATIC_MODEL_T2N3V3_ATI] = RB_SurfaceStaticModelATI,
    [R_SURFACE_STATIC_MODEL_T2N3V3_NV] = RB_SurfaceStaticModelT2N3V3_NV,
    [R_SURFACE_WORLD_UNOPTIMIZED] = RB_SurfaceTriangles,
    [R_SURFACE_OPTIMIZED_GENERIC] = RB_SurfaceOptimized,
    [R_SURFACE_OPTIMIZED_ARB] = RB_SurfaceOptimized,
    [R_SURFACE_OPTIMIZED_ATI] = RB_SurfaceOptimized,
    [R_SURFACE_OPTIMIZED_NV] = RB_SurfaceOptimized,
};

backEndData_t *rendererBackendData;
int32_t rendererMaxPolys;
int32_t rendererMaxPolyVerts;

image_t *imageHashTable[R_IMAGE_HASH_SIZE];
/* Original 0x0488bb20/0x0488bb24. Image creation accumulates the first;
 * the renderer export boundary lets the effects system manage the second. */
uint32_t rendererTextureMinFilter = GL_LINEAR_MIPMAP_NEAREST;
uint32_t rendererTextureMagFilter = GL_LINEAR;
uint8_t rendererGammaTable[256];
uint8_t rendererGammaOverbrightTable[256];
uint8_t rendererOverbrightTable[256];
uint8_t rendererInverseOverbrightTable[256];
uint8_t rendererIntensityTable[256];

/* Original 0x04884dac..0x04884dd3. R_Init clears the complete scene-frame
 * state; entity-list producers now resolve its count and first-entity words. */
renderer_scene_frame_state_t rendererSceneFrameState;

/* Original 0x0389feb0/0x0389feb4. RE_RegisterFont owns the eight-entry font
 * count, while R_LoadAsianFont and R_Init own the Asian-page loaded flag. */
int32_t rendererRegisteredFontCount;
qboolean rendererAsianFontLoaded;

/* R_ClearLightVisCache storage. The four statistics are discontiguous in the
 * original image (0x00d92e9c, 0x00d92ea0, 0x00d92fb8, and 0x00d92fbc). The
 * typed cache begins at 0x00d92fc0 and is exactly 2 MiB. R_GetCachedVisibility
 * proves each statistic's role and the cache's 8192x32x8 layout. */
int32_t rendererLightVisMaxAssociativity;
int32_t rendererLightVisUsedEntryCount;
int32_t rendererLightVisFlushedEntryCount;
int32_t rendererLightVisRuntimeFillCount;
renderer_light_vis_cache_entry_t
    rendererLightVisCache[R_LIGHT_VIS_BUCKET_COUNT]
                         [R_LIGHT_VIS_ENTRIES_PER_BUCKET]; /* 0x00d92fc0 */

/* Optional light-vis history and its sorted coordinate index. Original
 * pointers/counts: 0x00d92fa8/0x00d92fb0 and 0x00d92fac/0x00d92fb4. */
renderer_light_vis_history_entry_t *rendererLightVisHistory;
int32_t rendererLightVisHistoryCount;
renderer_light_vis_sort_entry_t *rendererLightVisSortedHistory;
int32_t rendererLightVisSortedHistoryCount;

/* Source: CoDUOMP.exe 0x004c55b0..0x004c55d4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c55b0_004c55d5.mcode.
 * Name: same-module Mac symbol R_ClearLightVisCache. The standalone Windows
 * body clears the complete 2 MiB byte cache and the two discontiguous pairs
 * of control words. The compiler emits the same source operation inline in
 * R_Init at 0x004c5012..0x004c5037. */
void R_ClearLightVisCache(void)
{
    memset(rendererLightVisCache, 0, sizeof(rendererLightVisCache));
    rendererLightVisMaxAssociativity = 0;
    rendererLightVisUsedEntryCount = 0;
    rendererLightVisFlushedEntryCount = 0;
    rendererLightVisRuntimeFillCount = 0;
}

/* Source: CoDUOMP.exe 0x004c55e0..0x004c56a7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c55e0_004c56a8.mcode.
 * Name: same-module Mac symbol R_SetHwLightGlobals. R_Register's exact
 * Cvar_Get return stores prove all three cvar identities. The four destination
 * addresses are tr +0xa9c..+0xaa8; their roles are independently confirmed by
 * the diffuse-sun sampling and entity-light selection consumers. */
void R_SetHwLightGlobals(void)
{
    tr.diffuseSunQuality = r_diffuseSunQuality->integer;
    if (tr.diffuseSunQuality < R_DIFFUSE_SUN_QUALITY_MIN)
        tr.diffuseSunQuality = R_DIFFUSE_SUN_QUALITY_MIN;
    else if (tr.diffuseSunQuality > R_DIFFUSE_SUN_QUALITY_MAX)
        tr.diffuseSunQuality = R_DIFFUSE_SUN_QUALITY_MAX;

    tr.diffuseSunSteps = r_diffuseSunSteps->integer;
    if (tr.diffuseSunSteps < R_DIFFUSE_SUN_STEPS_MIN)
        tr.diffuseSunSteps = R_DIFFUSE_SUN_STEPS_MIN;
    else if (tr.diffuseSunSteps > R_DIFFUSE_SUN_STEPS_MAX)
        tr.diffuseSunSteps = R_DIFFUSE_SUN_STEPS_MAX;

    tr.diffuseSunSampleScale =
        R_DIFFUSE_SUN_HALF_WEIGHT /
        (float)(tr.diffuseSunSteps * tr.diffuseSunSteps);

    tr.maxEntityLights = r_maxEntLights->integer;
    if (tr.maxEntityLights < tr.diffuseSunQuality + 1)
        tr.maxEntityLights = tr.diffuseSunQuality + 1;
    if (tr.maxEntityLights > glConfig.maxLights)
        tr.maxEntityLights = glConfig.maxLights;

    if (r_diffuseSunSteps->modified != qfalse) {
        r_diffuseSunSteps->modified = qfalse;
        R_ClearLightVisCache();
    }
}

/* Source: CoDUOMP.exe 0x004c4de0..0x004c507a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c4de0_004c507b.mcode.
 * Name and the source-level subsystem calls are corroborated by the exact
 * same-module Mac R_Init symbol. Windows instructions remain authoritative:
 * they prove all clear spans, table values, allocation sizes, call order,
 * model registration, GL error handling, and final state writes below. */
void R_Init(void)
{
    size_t backendAllocationSize;
    uint32_t glError;

    ri.Printf(R_PRINT_ALL, "----- R_Init -----\n");

    /* 0x004c4df1..0x004c4dfd clears the contiguous original range
     * 0x04885020..0x04898bf7: tr followed by rendererDebugState. */
    memset(&tr, 0, sizeof(tr));
    memset(&rendererDebugState, 0, sizeof(rendererDebugState));
    memset(&backEnd, 0, sizeof(backEnd));
    memset(&tess, 0, sizeof(tess));

    Swap_Init();

    if (((uintptr_t)tess.xyz & 15U) != 0U)
        Com_Printf("WARNING: tess.xyz not 16 byte aligned\n");

    memset(tess.constantColor255, 0xff, sizeof(tess.constantColor255));

    for (int32_t waveIndex = 0;
         waveIndex < R_WAVE_TABLE_SIZE;
         ++waveIndex) {
        const float waveIndexFloat = (float)waveIndex;
        const long double angle =
            (long double)waveIndexFloat *
            (long double)R_WAVE_DEGREES_PER_STEP *
            (long double)R_PI_F *
            (long double)R_INV_180_F;

        tr.sinTable[waveIndex] = (float)sinl(angle);
        tr.squareTable[waveIndex] =
            waveIndex < R_WAVE_TABLE_HALF ? 1.0f : -1.0f;
        const long double sawToothRaw =
            (long double)waveIndexFloat *
            (long double)R_INV_WAVE_TABLE_F;
        tr.sawToothTable[waveIndex] =
            (float)sawToothRaw;
        /* 0x4c4e98..0x4c4ead stores sawToothRaw but subtracts its retained
         * value from one for the inverse table. */
        tr.inverseSawToothTable[waveIndex] =
            (float)(1.0L - sawToothRaw);

        if (waveIndex < R_WAVE_TABLE_QUARTER) {
            tr.triangleTable[waveIndex] =
                waveIndexFloat * R_INV_WAVE_QUARTER_F;
        } else if (waveIndex < R_WAVE_TABLE_HALF) {
            tr.triangleTable[waveIndex] =
                1.0f - tr.triangleTable[
                    waveIndex - R_WAVE_TABLE_QUARTER];
        } else {
            tr.triangleTable[waveIndex] =
                -tr.triangleTable[waveIndex - R_WAVE_TABLE_HALF];
        }
    }

    Com_NoiseInit();
    R_Register();

    rendererMaxPolys = r_maxpolys->integer;
    if (rendererMaxPolys < R_MIN_POLYS)
        rendererMaxPolys = R_MIN_POLYS;

    rendererMaxPolyVerts = r_maxpolyverts->integer;
    if (rendererMaxPolyVerts < R_MIN_POLY_VERTS)
        rendererMaxPolyVerts = R_MIN_POLY_VERTS;

    backendAllocationSize =
        sizeof(*rendererBackendData) +
        (size_t)rendererMaxPolys * sizeof(srfPoly_t) +
        (size_t)rendererMaxPolyVerts * sizeof(polyVert_t);
    rendererBackendData = ri.Hunk_Alloc(backendAllocationSize);
    rendererBackendData->commandUsed = 0;

    /* The Windows build emits the startup frame-state reset inline. The ten
     * stores are exact at 0x004c4f62..0x004c4f9d. */
    memset(&rendererSceneFrameState, 0, sizeof(rendererSceneFrameState));

    InitOpenGL();
    R_InitAllocators();

    /* Same-module Mac R_InitImages, emitted inline by the Windows compiler. */
    R_InitImages();

    R_InitVertexPrograms();
    R_InitShaders();
    R_ModelInit();
    R_InitFreeType();
    R_SetHwLightGlobals();
    R_ClearLightVisCache();

    glError = qglGetError();
    if (glError != 0)
        ri.Printf(R_PRINT_ALL, "glGetError() = 0x%x\n", glError);

    R_InitDebug();
    XModelSetOptimize(qfalse);

    /* Same-module Mac R_InitWater, emitted inline by the Windows compiler. */
    R_InitWater();

    ri.Printf(R_PRINT_ALL, "----- finished R_Init -----\n");
}

/* Source: CoDUOMP.exe 0x004c50e0..0x004c5223.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c50e0_004c5224.mcode.
 * Name, destroyWindow argument, and subsystem names are corroborated by the
 * exact same-module Mac RE_Shutdown symbol. The Windows calls and registered
 * gate independently prove the command removal and teardown order. */
void RE_Shutdown(qboolean destroyWindow)
{
    ri.Printf(R_PRINT_ALL, "RE_Shutdown( %i )\n", destroyWindow);

    ri.Cmd_RemoveCommand("modellist");
    ri.Cmd_RemoveCommand("screenshotJPEG");
    ri.Cmd_RemoveCommand("screenshot");
    ri.Cmd_RemoveCommand("imagelist");
    ri.Cmd_RemoveCommand("shaderlist");
    ri.Cmd_RemoveCommand("skinlist");
    ri.Cmd_RemoveCommand("gfxinfo");
    ri.Cmd_RemoveCommand("modelist");
    ri.Cmd_RemoveCommand("shaderstate");
    ri.Cmd_RemoveCommand("taginfo");
    ri.Cmd_RemoveCommand("cropimages");
    ri.Cmd_RemoveCommand("r_meminfo");
    ri.Cmd_RemoveCommand("r_vc_stats");
    ri.Cmd_RemoveCommand("r_smc_stats");
    ri.Cmd_RemoveCommand("r_smc_flush");
    ri.Cmd_RemoveCommand("r_loadsun");
    ri.Cmd_RemoveCommand("r_savesun");
    ri.Cmd_RemoveCommand("r_sunhelp");
    ri.Cmd_RemoveCommand("r_vbo_refresh");

    if (tr.registered != qfalse) {
        R_DeleteTextures();
        R_DeleteVertexPrograms();
        R_DeleteFragmentShaders();
        R_DeleteBuffersARB();
    }

    R_SaveLightVisHistory();
    R_DoneFreeType();
    R_ShutdownAllocators();
    R_ShutdownStaticModels();

    if (destroyWindow != qfalse) {
        GLimp_Shutdown();
    }

    R_ShutdownDebug();
    tr.registered = qfalse;
}

/* Source: CoDUOMP.exe 0x004c5230..0x004c527d, exporter-gap recovery.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5230_004c527e.mcode.
 * Exact same-module Mac symbol RE_EndRegistration supplies the three source
 * calls. Windows emits R_SyncRenderThread and Sys_LowPhysicalMemory inline:
 * the latter compares the MB-valued detected-memory global with the
 * 96-MiB byte constant before the tail jump to RB_ShowImages. */
void RE_EndRegistration(void)
{
    R_SyncRenderThread();
    if (Sys_LowPhysicalMemory() == qfalse)
        RB_ShowImages();
}
