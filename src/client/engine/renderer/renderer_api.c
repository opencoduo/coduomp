#include "renderer_api.h"

#include "../client/debug_lines.h"
#include "../effects/fx_api.h"

#include <string.h>

void RE_ClearScene(void);
void RE_AddPolysToScene(int32_t shaderHandle, int32_t vertexCount, const polyVert_t *vertices, int32_t polyCount);
void RE_AddCoronaToScene(const vec3_t origin, float red, float green, float blue, float scale, int32_t id, int32_t flags);
void RE_RenderScene(const refdef_t *refdef);
void RE_ClearFlares(void);
void RE_SetColor(const float *rgba);
void RE_StretchPic(float x, float y, float width, float height, float s1, float t1, float s2, float t2, int32_t shaderHandle);
void RE_StretchPicGradient(float x, float y, float width, float height, float s1, float t1, float s2, float t2, int32_t shaderHandle,
                           const float *gradientColor, int32_t gradientType);
void RE_StretchPicRotate(float x, float y, float width, float height, float s1, float t1, float s2, float t2, float angleDegrees,
                         int32_t shaderHandle);
void RE_DrawQuadPic(const vec2_t positions[4], const vec2_t texCoords[4], int32_t shaderHandle);
void RE_BeginFrame(stereoFrame_t stereoFrame);
void RE_EndFrame(int32_t *frontEndMsec, int32_t *backEndMsec);
void RE_SaveScreen(void);
void RE_BlendSavedScreen(int32_t duration);
void RE_TrackStatistics(renderer_frame_statistics_t *statistics);
void R_ResetImageAllocations(void);
void R_FreeImageAllocations(void);
refimport_t ri;
refexport_t rendererExports;
/* Renderer-owned return storage at original 0x00d92db0. CL_InitRef copies
 * this table into the client-owned rendererExports at original 0x049580a0. */
static refexport_t rendererModuleExports;

/* Source: CoDUOMP.exe 0x004c5280..0x004c550d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5280_004c550e.mcode.
 * The Windows body proves the 38-entry import copy, 59-entry export clear,
 * API version 14, diagnostic, every export-table offset, and every target
 * address. Same-module Mac GetRefAPI corroborates the conventional source ABI
 * order (version, imports) and supplies the exact 59 source function names;
 * Windows machine code remains authoritative for behavior. */
refexport_t *GetRefAPI(int32_t apiVersion, const refimport_t *imports)
{
    memcpy(&ri, imports, sizeof(ri));
    memset(&rendererModuleExports, 0, sizeof(rendererModuleExports));

    if (apiVersion != RENDERER_API_VERSION) {
        ri.Printf(R_PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", RENDERER_API_VERSION, apiVersion);
        return NULL;
    }

    rendererModuleExports.Shutdown = RE_Shutdown;
    rendererModuleExports.BeginRegistration = RE_BeginRegistration;
    rendererModuleExports.GetXModelByHandle = RE_GetXModelByHandle;
    rendererModuleExports.RegisterModel = RE_RegisterModel;
    rendererModuleExports.RegisterShader = RE_RegisterShader;
    rendererModuleExports.RegisterShaderNoMip = RE_RegisterShaderNoMip;
    rendererModuleExports.LoadWorldMap = RE_LoadWorldMap;
    rendererModuleExports.FinishLoadingModels = RE_FinishLoadingModels;
    rendererModuleExports.SetIgnorePrecacheErrors = RE_SetIgnorePrecacheErrors;
    rendererModuleExports.GetIgnorePrecacheErrors = RE_GetIgnorePrecacheErrors;
    rendererModuleExports.GetShaderFromModel = RE_GetShaderFromModel;
    rendererModuleExports.SetFXImageMemory = RE_SetFXImageMemory;
    rendererModuleExports.GetFXImageMemory = RE_GetFXImageMemory;
    rendererModuleExports.GetImageMemory = RE_GetImageMemory;
    rendererModuleExports.GetShaderName = RE_GetShaderName;
    rendererModuleExports.GetFarPlaneDist = RE_GetFarPlaneDist;
    rendererModuleExports.EndRegistration = RE_EndRegistration;
    rendererModuleExports.ClearScene = RE_ClearScene;
    rendererModuleExports.AddRefEntityToScene = RE_AddRefEntityToScene;
    rendererModuleExports.AddPolyToScene = RE_AddPolyToScene;
    rendererModuleExports.AddPolysToScene = RE_AddPolysToScene;
    rendererModuleExports.AddLightToScene = RE_AddLightToScene;
    rendererModuleExports.SetCullDist = RE_SetCullDist;
    rendererModuleExports.AddCoronaToScene = RE_AddCoronaToScene;
    rendererModuleExports.SetFog = R_SetFog;
    rendererModuleExports.SaveFogState = RE_SaveFogState;
    rendererModuleExports.RestoreFogState = RE_RestoreFogState;
    rendererModuleExports.RenderScene = RE_RenderScene;
    rendererModuleExports.ClearFlares = RE_ClearFlares;
    rendererModuleExports.SetColor = RE_SetColor;
    rendererModuleExports.StretchPic = RE_StretchPic;
    rendererModuleExports.StretchPicGradient = RE_StretchPicGradient;
    rendererModuleExports.StretchPicRotate = RE_StretchPicRotate;
    rendererModuleExports.DrawQuadPic = RE_DrawQuadPic;
    rendererModuleExports.StretchRaw = RE_StretchRaw;
    rendererModuleExports.UploadCinematic = RE_UploadCinematic;
    rendererModuleExports.BeginFrame = RE_BeginFrame;
    rendererModuleExports.EndFrame = RE_EndFrame;
    rendererModuleExports.SaveScreen = RE_SaveScreen;
    rendererModuleExports.BlendSavedScreen = RE_BlendSavedScreen;
    rendererModuleExports.MarkFragments = RE_MarkFragments;
    rendererModuleExports.ModelBounds = R_ModelBounds;
    rendererModuleExports.TrackStatistics = RE_TrackStatistics;
    rendererModuleExports.PickShader = RE_PickShader;
    rendererModuleExports.RegisterFont = RE_RegisterFont;
    rendererModuleExports.GetEntityToken = R_GetEntityToken;
    rendererModuleExports.ResetImageAllocations = R_ResetImageAllocations;
    rendererModuleExports.FreeImageAllocations = R_FreeImageAllocations;
    rendererModuleExports.CubemapShot = RE_CubemapShot;
    rendererModuleExports.CubemapWaterShot = RE_CubemapWaterShot;
    rendererModuleExports.LocateDebugStrings = RE_LocateDebugStrings;
    rendererModuleExports.LocateDebugLines = RE_LocateDebugLines;
    rendererModuleExports.AddPlume = RE_AddPlume;
    rendererModuleExports.TextWidth = RE_Text_Width;
    rendererModuleExports.TextHeight = RE_Text_Height;
    rendererModuleExports.TextPaint = RE_Text_Paint;
    rendererModuleExports.TextConsoleWidth = RE_Text_ConsoleWidth;
    rendererModuleExports.TextConsolePaint = RE_Text_ConsolePaint;
    rendererModuleExports.TextPaintWithCursor = RE_Text_PaintWithCursor;

    return &rendererModuleExports;
}
