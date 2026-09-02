#include "cgame.h"

#include "cinematic.h"
#include "console.h"
#include "../animation/dobj.h"
#include "qcommon/hunk.h"
#include "../platform/crt_boundary.h"
#include "../renderer/renderer_api.h"
#include "../system_event.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Exact binary float constants. Semantically these are 1.0f / 640.0f and
 * 1.0f / 480.0f; spelling the stored values directly makes cross-platform
 * rounding independent of constant folding. */
#define SCREEN_SCALE_X 0.0015625000232830644f /* 0x3acccccd */
#define SCREEN_SCALE_Y 0.0020833334419876337f /* 0x3b088889 */
#define SCR_SMALL_TEXT_SCALE \
    0.3333333432674408f /* 0x3eaaaaab, semantically 1/3 */
#define SCR_SMALL_TEXT_ADVANCE 8.0f

enum {
    SCR_DEBUG_GRAPH_SAMPLE_COUNT = 1024,
    SCR_DEBUG_GRAPH_SAMPLE_MASK = SCR_DEBUG_GRAPH_SAMPLE_COUNT - 1,
    SCR_SMALL_TEXT_FONT = 5,
    SCR_SMALL_TEXT_BASELINE_OFFSET = 16,
    SCR_SMALL_TEXT_STYLE = 0,
    CL_CUBEMAP_BASENAME_MAX_LENGTH = 40,
    CL_CUBEMAP_MIN_FACE_SIZE = 4,
    CL_CUBEMAP_MAX_FACE_SIZE = 1024,
    CL_CUBEMAP_FACE_COUNT =
        CUBEMAP_FACE_BACK - CUBEMAP_FACE_UP + 1
};

#define CL_CUBEMAP_DEFAULT_FRESNEL_N0 \
    1.0f
#define CL_CUBEMAP_DEFAULT_FRESNEL_N1 \
    1.3329999446868896f /* exact 0x3faa9fbe; approximately 1.333 */
#define CL_CUBEMAP_MIN_REFRACTION_INDEX \
    1.0f

typedef struct scr_debug_graph_sample_s {
    float value;
    int32_t color; /* Original +0x04; written but never read by CoDUOMP.exe. */
} scr_debug_graph_sample_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(scr_debug_graph_sample_t) == 4,
               "i386 debug-graph sample alignment changed");
_Static_assert(offsetof(scr_debug_graph_sample_t, value) == 0x00,
               "original debug-graph value offset");
_Static_assert(offsetof(scr_debug_graph_sample_t, color) == 0x04,
               "original debug-graph color offset");
_Static_assert(sizeof(scr_debug_graph_sample_t) == 0x08,
               "original debug-graph sample extent");
#endif

cvar_t *scr_timegraph;   /* original 0x04957f34 */
cvar_t *scr_debuggraph;  /* original 0x04957f28 */
cvar_t *scr_graphheight; /* original 0x04957f38 */
cvar_t *scr_graphscale;  /* original 0x04957f2c */
cvar_t *scr_graphshift;  /* original 0x04957f30 */
static qboolean scr_initialized; /* original 0x04957f3c */
/* Original graph ring at 0x008ce9a0 and its cursor at 0x008d09a0. The
 * adjacent dword is the sample color accepted by SCR_DebugGraph; the Win32
 * graph renderer itself only reads the value lane. */
static scr_debug_graph_sample_t
    scr_debugGraphSamples[SCR_DEBUG_GRAPH_SAMPLE_COUNT];
static uint32_t scr_debugGraphCursor;
/* Source: CoDUOMP.exe 0x005c49a0..0x005c49b8. The pointer order is consumed
 * with public cubemap face values 1..6.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * six original pointers and proves the ordered suffix strings. */
static const char *const
    cl_cubemapFaceSuffixes[CL_CUBEMAP_FACE_COUNT] = {
        "_up", "_dn", "_lf", "_rt", "_ft", "_bk"
    };
/* SCR_UpdateScreen's recursion guard. Com_Error clears it immediately before
 * leaving the current frame through com_abortFrame. */
qboolean scr_updateScreenRecursionGuard; /* original 0x0389fcf4 */


/* Source: CoDUOMP.exe 0x00419b60..0x00419b8b, recovered from an exporter
 * function-boundary gap. Exact same-module Mac symbol SCR_DebugGraph. MSVC
 * also inlines it at 0x0040ca2e..0x0040ca57 and
 * 0x004139f2..0x00413a19; all three instruction sequences prove the two-field
 * ring write and wrapping 1024-entry index. */
void SCR_DebugGraph(float value, int32_t color)
{
    scr_debug_graph_sample_t *const sample =
        &scr_debugGraphSamples[
            scr_debugGraphCursor & SCR_DEBUG_GRAPH_SAMPLE_MASK];
    ++scr_debugGraphCursor;
    sample->value = value;
    sample->color = color;
}

/* Source: CoDUOMP.exe 0x00419b90..0x00419ce3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419b90_00419ce4.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * SCR_DrawDebugGraph. */
void SCR_DrawDebugGraph(void)
{
    vec4_t color;
    const int32_t width = cls.rendererConfig.vidWidth;
    const int32_t screenHeight = cls.rendererConfig.vidHeight;
    const int32_t graphHeight = scr_graphheight->integer;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (graphHeight == 0)
        return;

    CL_LookupColor('0', color);
    rendererExports.SetColor(color);
    rendererExports.StretchPic(
        0.0f, (float)(screenHeight - graphHeight),
        (float)width, (float)graphHeight,
        0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader);
    rendererExports.SetColor(NULL);

    for (int32_t x = 0; x < width; ++x) {
        const uint32_t sampleIndex =
            (scr_debugGraphCursor - (uint32_t)x - 1u) &
            SCR_DEBUG_GRAPH_SAMPLE_MASK;
        float value =
            scr_debugGraphSamples[sampleIndex].value *
                (float)scr_graphscale->integer +
            (float)scr_graphshift->integer;

        if (value < 0.0f) {
            const int32_t quotient =
                (int32_t)(value / (float)graphHeight);
            value +=
                (float)((1 - quotient) * graphHeight);
        }

        const int32_t barHeight =
            (int32_t)value % graphHeight;
        rendererExports.StretchPic(
            (float)x, (float)(screenHeight - barHeight),
            1.0f, (float)barHeight,
            0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader);
    }
}

/* Source: CoDUOMP.exe 0x00419d80..0x00419f9f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419d80_00419fa0.mcode and the two
 * state-dispatch tables at 0x00419fa0/0x00419fc4.
 * Name and stereo-frame argument: exact same-module Mac symbol
 * SCR_DrawScreenField. */
void SCR_DrawScreenField(stereoFrame_t stereoFrame)
{
    rendererExports.BeginFrame(stereoFrame);


    if (coduo_uiVm == NULL) {
        Com_DPrintf("draw screen without UI loaded\n");
        return;
    }

    const qboolean uiFullscreen = (qboolean)VM_Call(
        coduo_uiVm, UIVM_IS_FULLSCREEN,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    if (uiFullscreen == qfalse) {
        switch (cls.state) {
        case CA_DISCONNECTED:
            MSS_StopSounds(0);
            (void)VM_Call(
                coduo_uiVm, UIVM_SET_ACTIVE_MENU,
                UI_MENU_MAIN,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            break;

        case CA_CONNECTING:
        case CA_CHALLENGING:
        case CA_CONNECTED:
            (void)VM_Call(
                coduo_uiVm, UIVM_REFRESH, cls.realTime,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            (void)VM_Call(
                coduo_uiVm, UIVM_DRAW_CONNECT_SCREEN, qfalse,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            break;

        case CA_LOADING:
        case CA_PRIMED:
            CL_CGameRendering(stereoFrame, qtrue);
            (void)VM_Call(
                coduo_uiVm, UIVM_REFRESH, cls.realTime,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            /* 0x00419eca pushes the overlay flag 1 after rendering the
             * loading/primed cgame frame. */
            (void)VM_Call(
                coduo_uiVm, UIVM_DRAW_CONNECT_SCREEN, qtrue,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            break;

        case CA_ACTIVE:
            CL_CGameRendering(stereoFrame, qtrue);
            SCR_DrawDemoRecording();
            break;

        case CA_CINEMATIC:
            SCR_DrawCinematic();
            break;

        case CA_LOGO:
            CL_DrawLogo();
            if (cls.state != CA_LOGO)
                return;
            break;

        default:
            Com_Error(
                ERR_FATAL,
                "SCR_DrawScreenField: bad cls.state");
        }
    } else if (cls.state == CA_LOADING ||
               cls.state == CA_PRIMED ||
               cls.state == CA_ACTIVE) {
        CL_CGameRendering(stereoFrame, qfalse);
    }

    if ((cls.keyCatchers & KEYCATCH_UI) != 0) {
        (void)VM_Call(
            coduo_uiVm, UIVM_REFRESH, cls.realTime,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    Con_DrawConsole();
    if (scr_debuggraph->integer != 0 ||
        scr_timegraph->integer != 0 ||
        cl_debugMove->integer != 0) {
        SCR_DrawDebugGraph();
    }

    if (net_showprofile->integer != 0)
        Net_DisplayProfile();
}

/* Source: CoDUOMP.exe 0x00419fe0..0x0041a0a6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419fe0_0041a0a7.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * SCR_UpdateScreen. The Windows loading-state path directly pumps platform
 * events before testing the screen guards. */
void SCR_UpdateScreen(void)
{
    if (cls.state == CA_LOADING)
        Sys_PumpEvents();

    if (scr_initialized == qfalse ||
        scr_updateScreenRecursionGuard != qfalse) {
        return;
    }

    scr_updateScreenRecursionGuard = qtrue;
    if (cls.state == CA_ACTIVE) {
        dobj_skelCacheKey =
            (int32_t)((uint32_t)dobj_skelCacheKey + 1u);
        if (dobj_skelCacheKey == 0)
            dobj_skelCacheKey = 1;
        cl_frameRunning = qtrue;
    }

    if (cls.rendererConfig.stereoEnabled != qfalse) {
        SCR_DrawScreenField(STEREO_LEFT);
        SCR_DrawScreenField(STEREO_RIGHT);
    } else {
        SCR_DrawScreenField(STEREO_CENTER);
    }

    if (com_speeds->integer != 0) {
        rendererExports.EndFrame(
            &com_timeFrontend, &com_timeBackend);
    } else {
        rendererExports.EndFrame(NULL, NULL);
    }


    if (cls.state == CA_ACTIVE) {
        cl_frameRunning = qfalse;
        Hunk_ClearTempMemory();
    }
    scr_updateScreenRecursionGuard = qfalse;
}

/* Source: CoDUOMP.exe 0x0041a120..0x0041a12e, recovered from the executable
 * gap left by Ghidra's missing function boundary.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_UpdateScreen_f. */
void CL_UpdateScreen_f(void)
{
    if (cls.state == CA_LOADING)
        SCR_UpdateScreen();
}

/* Source: CoDUOMP.exe 0x0041a0b0..0x0041a119.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041a0b0_0041a11a.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_CubemapShotUsage. */
void CL_CubemapShotUsage(void)
{
    Com_Printf(
        "Syntax: cubemapShot size basefilename "
        "[water r0 g0 b0 r90 g90 b90 | fresnel n0 n1]\n");
    Com_Printf("size must be a power of 2 >= 4 and <= 1024\n");
    Com_Printf(
        "screenshots will be written to 'env/basefilename_*.tga'\n");
    Com_Printf("basefilename must not exceed %i chars\n",
               CL_CUBEMAP_BASENAME_MAX_LENGTH);
    Com_Printf(
        "If 'water' is specified, a diffuse water color cubemap is "
        "generated using local lighting.\n");
    Com_Printf(
        "The water has the given colors at the given angles, and blends "
        "between them in the middle.\n");
    Com_Printf(
        "If 'fresnel' is specified, the alpha channel of the cubemap "
        "contains the reflection factor.\n");
    Com_Printf(
        "n0 and n1 are the index of refraction of the 'air' and 'water' "
        "surfaces, respectively.\n");
    Com_Printf(
        "The index of refraction must always be 1 or greater.\n");
    Com_Printf(
        "This is always calculated, and defaults to air-water interface "
        "(n0 = 1, n1 = 1.333).\n");
}

/* Source: CoDUOMP.exe 0x0041a130..0x0041a45a, recovered from the executable
 * gap left by Ghidra's missing function boundary.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_CubemapShot_f. The renderer export slots and direct Mac call sequence
 * prove the water/fresnel argument layouts and the six face passes. */
void CL_CubemapShot_f(void)
{
    cvar_t *const cheats = Cvar_Get(
        "sv_cheats", "0", CVAR_SYSTEMINFO | CVAR_ROM);
    if (cheats->integer == 0) {
        Com_Printf("Must have cheats enabled to use this command\n");
        return;
    }
    if (coduo_cgameVm == NULL) {
        Com_Printf("must be in a map to use this command\n");
        return;
    }

    const int32_t argumentCount = Cmd_Argc();
    if (argumentCount < 3 ||
        strlen(Cmd_Argv(2)) >
            (size_t)CL_CUBEMAP_BASENAME_MAX_LENGTH) {
        CL_CubemapShotUsage();
        return;
    }

    /* The original 64-byte local occupies 0x40 bytes immediately below the
     * stack cookie; the smaller command limit leaves room for the suffix. */
    char baseFilename[MAX_QPATH];
    strcpy(baseFilename, Cmd_Argv(2));

    const int32_t faceSize = coduo_crt_atoi(Cmd_Argv(1));
    if (faceSize < CL_CUBEMAP_MIN_FACE_SIZE ||
        faceSize > CL_CUBEMAP_MAX_FACE_SIZE ||
        (faceSize & (faceSize - 1)) != 0) {
        CL_CubemapShotUsage();
        return;
    }

    if (argumentCount == 10) {
        vec3_t horizonColor;
        vec3_t zenithColor;

        if (coduo_crt_stricmp(Cmd_Argv(3), "water") != 0) {
            CL_CubemapShotUsage();
            return;
        }

        horizonColor[0] = (float)atof(Cmd_Argv(4));
        horizonColor[1] = (float)atof(Cmd_Argv(5));
        horizonColor[2] = (float)atof(Cmd_Argv(6));
        zenithColor[0] = (float)atof(Cmd_Argv(7));
        zenithColor[1] = (float)atof(Cmd_Argv(8));
        zenithColor[2] = (float)atof(Cmd_Argv(9));

        for (int32_t faceIndex = CUBEMAP_FACE_UP;
             faceIndex <= CUBEMAP_FACE_BACK; ++faceIndex) {
            const cubemap_face_t face =
                (cubemap_face_t)faceIndex;
            rendererExports.CubemapWaterShot(
                va("env/%s%s.tga", baseFilename,
                   cl_cubemapFaceSuffixes[
                       faceIndex - CUBEMAP_FACE_UP]),
                faceSize, face, horizonColor, zenithColor);
        }
        return;
    }

    float fresnelN0 = CL_CUBEMAP_DEFAULT_FRESNEL_N0;
    float fresnelN1 = CL_CUBEMAP_DEFAULT_FRESNEL_N1;
    if (argumentCount == 6) {
        if (coduo_crt_stricmp(Cmd_Argv(3), "fresnel") != 0) {
            CL_CubemapShotUsage();
            return;
        }
        fresnelN0 = (float)atof(Cmd_Argv(4));
        fresnelN1 = (float)atof(Cmd_Argv(5));
        if (fresnelN0 < CL_CUBEMAP_MIN_REFRACTION_INDEX ||
            fresnelN1 < CL_CUBEMAP_MIN_REFRACTION_INDEX) {
            CL_CubemapShotUsage();
            return;
        }
    } else if (argumentCount != 3) {
        CL_CubemapShotUsage();
        return;
    }

    dobj_skelCacheKey =
        (int32_t)((uint32_t)dobj_skelCacheKey + 1u);
    if (dobj_skelCacheKey == 0)
        dobj_skelCacheKey = 1;
    cl_frameRunning = qtrue;

    for (int32_t faceIndex = CUBEMAP_FACE_UP;
         faceIndex <= CUBEMAP_FACE_BACK; ++faceIndex) {
        const cubemap_face_t face =
            (cubemap_face_t)faceIndex;

        rendererExports.BeginFrame(STEREO_CENTER);
        (void)VM_Call(
            coduo_cgameVm, CGVM_DRAW_ACTIVE_FRAME,
            cl.serverTime, STEREO_CENTER, clc.demoPlayback,
            face, faceSize, qtrue,
            0, 0, 0, 0, 0, 0);
        rendererExports.EndFrame(NULL, NULL);
        rendererExports.CubemapShot(
            va("env/%s%s.tga", baseFilename,
               cl_cubemapFaceSuffixes[
                   faceIndex - CUBEMAP_FACE_UP]),
            faceSize, face, fresnelN0, fresnelN1);
    }

    cl_frameRunning = qfalse;
    Hunk_ClearTempMemory();
}

/* Source: CoDUOMP.exe 0x00419cf0..0x00419d7a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419cf0_00419d7b.mcode.
 * Name and no-argument signature: exact same-module Mac symbol SCR_Init. */
void SCR_Init(void)
{
    scr_timegraph = Cvar_Get("timegraph", "0", CVAR_CHEAT);
    scr_debuggraph = Cvar_Get("debuggraph", "0", CVAR_CHEAT);
    scr_graphheight = Cvar_Get("graphheight", "32", CVAR_CHEAT);
    scr_graphscale = Cvar_Get("graphscale", "1", CVAR_CHEAT);
    scr_graphshift = Cvar_Get("graphshift", "0", CVAR_CHEAT);
    scr_initialized = qtrue;
}

/* Source: CoDUOMP.exe 0x00419870..0x004198c0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419870_004198c1.mcode.
 * Name and signature: exact same-module Mac symbol SCR_AdjustTo640. MSVC
 * emitted this standalone body even though the live Win32 callers inline it. */
void SCR_AdjustTo640(float *x, float *y, float *width, float *height)
{
    const float inverseXScale =
        640.0f / (float)cls.rendererConfig.vidWidth;
    const float inverseYScale =
        480.0f / (float)cls.rendererConfig.vidHeight;

    if (x != NULL)
        *x *= inverseXScale;
    if (y != NULL)
        *y *= inverseYScale;
    if (width != NULL)
        *width *= inverseXScale;
    if (height != NULL)
        *height *= inverseYScale;
}

/* Source: CoDUOMP.exe 0x00419740..0x00419799.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419740_0041979a.mcode.
 * Name and signature: exact same-module Mac symbol SCR_AdjustFrom640. */
void SCR_AdjustFrom640(float *x, float *y, float *width, float *height)
{
    const float xScale = (float)cls.rendererConfig.vidWidth * SCREEN_SCALE_X;
    const float yScale = (float)cls.rendererConfig.vidHeight * SCREEN_SCALE_Y;

    if (x != NULL)
        *x *= xScale;
    if (y != NULL)
        *y *= yScale;
    if (width != NULL)
        *width *= xScale;
    if (height != NULL)
        *height *= yScale;
}

/* Source: CoDUOMP.exe 0x004197a0..0x00419808.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004197a0_00419809.mcode.
 * Name: exact same-module Mac symbol SCR_FillRect. The original carries color
 * in EAX and inlines SCR_AdjustFrom640. */
void SCR_FillRect(float x, float y, float width, float height,
                  const vec4_t color)
{
    const float xScale =
        (float)cls.rendererConfig.vidWidth * SCREEN_SCALE_X;
    const float yScale =
        (float)cls.rendererConfig.vidHeight * SCREEN_SCALE_Y;

    rendererExports.SetColor(color);
    rendererExports.StretchPic(
        x * xScale, y * yScale, width * xScale, height * yScale,
        0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader);
    rendererExports.SetColor(NULL);
}

/* Source: CoDUOMP.exe 0x00419810..0x00419866.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419810_00419867.mcode.
 * Name: exact same-module Mac symbol SCR_DrawPic. The Windows body inlines
 * SCR_AdjustFrom640 before forwarding to the renderer. */
void SCR_DrawPic(float x, float y, float width, float height,
                 int32_t shaderHandle)
{
    const float xScale =
        (float)cls.rendererConfig.vidWidth * SCREEN_SCALE_X;
    const float yScale =
        (float)cls.rendererConfig.vidHeight * SCREEN_SCALE_Y;

    rendererExports.StretchPic(
        x * xScale, y * yScale, width * xScale, height * yScale,
        0.0f, 0.0f, 1.0f, 1.0f, shaderHandle);
}

/* Source: CoDUOMP.exe 0x004196d0..0x00419731.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004196d0_00419732.mcode.
 * Role name: SCR_DrawNamedPic, matching the equivalent UI-module helper. The
 * original receives name and shader usage in registers, registers the shader,
 * and draws it with full texture coordinates after applying the 640x480
 * virtual-screen scale. No Win32 instruction directly calls the emitted body. */
void SCR_DrawNamedPic(float x, float y, float width, float height,
                      const char *name, int32_t shaderUsage)
{
    const float xScale =
        (float)cls.rendererConfig.vidWidth * SCREEN_SCALE_X;
    const float yScale =
        (float)cls.rendererConfig.vidHeight * SCREEN_SCALE_Y;
    const int32_t shaderHandle =
        rendererExports.RegisterShader(name, shaderUsage);

    rendererExports.StretchPic(
        x * xScale, y * yScale, width * xScale, height * yScale,
        0.0f, 0.0f, 1.0f, 1.0f, shaderHandle);
}

/* Source: CoDUOMP.exe 0x004198d0..0x00419970.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004198d0_00419971.mcode.
 * Name: exact same-module Mac symbol SCR_DrawSmallChar. The original receives
 * character in ECX and x/y on the stack. */
void SCR_DrawSmallChar(int32_t x, int32_t y, int32_t character)
{
    const vec4_t color = {1.0f, 1.0f, 1.0f, 1.0f};
    const float inverseXScale =
        640.0f / (float)cls.rendererConfig.vidWidth;
    const float inverseYScale =
        480.0f / (float)cls.rendererConfig.vidHeight;

    rendererExports.TextPaint(
        (float)x * inverseXScale,
        (float)(y + SCR_SMALL_TEXT_BASELINE_OFFSET) * inverseYScale,
        SCR_SMALL_TEXT_FONT, inverseYScale * SCR_SMALL_TEXT_SCALE,
        color, va("%c", character),
        inverseXScale * SCR_SMALL_TEXT_ADVANCE,
        0, SCR_SMALL_TEXT_STYLE);
}

/* Source: CoDUOMP.exe 0x00419980..0x004199e1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00419980_004199e2.mcode.
 * Name: exact same-module Mac symbol SCR_DrawSmallStringExt. The Win32 body
 * receives text/color in EAX/ECX and x/y on the stack. */
void SCR_DrawSmallStringExt(int32_t x, int32_t y, const char *text,
                            const vec4_t color)
{
    const float inverseXScale =
        640.0f / (float)cls.rendererConfig.vidWidth;
    const float inverseYScale =
        480.0f / (float)cls.rendererConfig.vidHeight;

    rendererExports.TextPaint(
        (float)x * inverseXScale,
        (float)(y + SCR_SMALL_TEXT_BASELINE_OFFSET) * inverseYScale,
        SCR_SMALL_TEXT_FONT, inverseYScale * SCR_SMALL_TEXT_SCALE,
        color, text, inverseXScale * SCR_SMALL_TEXT_ADVANCE,
        0, SCR_SMALL_TEXT_STYLE);
}

/* Source: CoDUOMP.exe 0x004199f0..0x00419a50.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004199f0_00419a51.mcode.
 * Name: exact same-module Mac symbol SCR_DrawConsoleString. The Windows LTCG
 * body carries encodedCount, encodedText, and color in EAX, ECX, and EDX. */
void SCR_DrawConsoleString(int32_t x, int32_t y,
                           const uint16_t *encodedText,
                           int32_t encodedCount, const vec4_t color)
{
    const float inverseXScale =
        640.0f / (float)cls.rendererConfig.vidWidth;
    const float inverseYScale =
        480.0f / (float)cls.rendererConfig.vidHeight;

    rendererExports.TextConsolePaint(
        (float)x * inverseXScale,
        (float)(y + SCR_SMALL_TEXT_BASELINE_OFFSET) * inverseYScale,
        SCR_SMALL_TEXT_FONT, inverseYScale * SCR_SMALL_TEXT_SCALE,
        color, encodedText, inverseXScale * SCR_SMALL_TEXT_ADVANCE,
        encodedCount, SCR_SMALL_TEXT_STYLE);
}
