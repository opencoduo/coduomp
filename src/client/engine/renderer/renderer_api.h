#ifndef CODUOMP_RENDERER_API_H
#define CODUOMP_RENDERER_API_H

#include <stddef.h>
#include <stdint.h>

#include "qcommon/asset_type_names.h"
#include "../effects/fx_render_types.h"
#include "backend.h"
#include "gl_state.h"

struct client_debug_line_s;
struct client_debug_string_s;

enum {
    RENDERER_API_VERSION = 14
};

typedef enum stereoFrame_e {
    STEREO_CENTER = 0,
    STEREO_LEFT = 1,
    STEREO_RIGHT = 2
} stereoFrame_t;

/* Q3 supplies the inherited refexport_t identity; Windows and Mac independently
 * prove this expanded CoD table's 59 consecutive slots and ordering. */
typedef struct refexport_s {
    void (*Shutdown)(qboolean destroyWindow);                         /* 0 */
    void (*BeginRegistration)(glconfig_t *config);                    /* 1 */
    XModel *(*GetXModelByHandle)(int32_t modelHandle);                 /* 2 */
    int32_t (*RegisterModel)(const char *name, int32_t loadMode);      /* 3 */
    int32_t (*RegisterShader)(const char *name, int32_t loadMode);     /* 4 */
    int32_t (*RegisterShaderNoMip)(const char *name, int32_t loadMode);/* 5 */
    void (*LoadWorldMap)(const char *name, int32_t *checksum);         /* 6 */
    void (*FinishLoadingModels)(void);                                /* 7 */
    void (*SetIgnorePrecacheErrors)(qboolean ignore);                  /* 8 */
    qboolean (*GetIgnorePrecacheErrors)(void);                         /* 9 */
    int32_t (*GetShaderFromModel)(int32_t modelHandle, int32_t surfaceIndex);               /* 10 */
    void (*SetFXImageMemory)(uint32_t imageMemory);                    /* 11 */
    uint32_t (*GetFXImageMemory)(void);                                /* 12 */
    uint32_t (*GetImageMemory)(void);                                  /* 13 */
    const char *(*GetShaderName)(int32_t shaderHandle);                /* 14 */
    float (*GetFarPlaneDist)(void);                                    /* 15 */
    void (*EndRegistration)(void);                                     /* 16 */
    void (*ClearScene)(void);                                          /* 17 */
    void (*AddRefEntityToScene)(const refEntity_t *entity, renderer_static_model_t *staticLighting); /* 18 */
    void (*AddPolyToScene)(int32_t shaderHandle, int32_t vertexCount, const polyVert_t *vertices); /* 19 */
    void (*AddPolysToScene)(int32_t shaderHandle, int32_t vertexCount, const polyVert_t *vertices, int32_t polyCount); /* 20 */
    void (*AddLightToScene)(const vec3_t origin, float radius, float red, float green, float blue); /* 21 */
    void (*SetCullDist)(float distance); /* 22 */
    void (*AddCoronaToScene)(const vec3_t origin, float red, float green, float blue, float scale, int32_t id, int32_t flags); /* 23 */
    void (*SetFog)(int32_t fogIndex, int32_t fogStart, int32_t fogEnd, float red, float green, float blue, float density); /* 24 */
    int32_t (*SaveFogState)(void *buffer, uint32_t bufferSize); /* 25 */
    int32_t (*RestoreFogState)(const void *buffer, uint32_t bufferSize); /* 26 */
    void (*RenderScene)(const refdef_t *refdef); /* 27 */
    void (*ClearFlares)(void); /* 28 */
    void (*SetColor)(const float *rgba); /* 29 */
    void (*StretchPic)(float x, float y, float width, float height, float s1, float t1, float s2, float t2, int32_t shaderHandle); /* 30 */
    void (*StretchPicGradient)(float x, float y, float width, float height, float s1, float t1, float s2, float t2, int32_t shaderHandle,
                               const float *gradientColor, int32_t gradientType); /* 31 */
    void (*StretchPicRotate)(float x, float y, float width, float height, float s1, float t1, float s2, float t2, float angleDegrees,
                             int32_t shaderHandle); /* 32 */
    void (*DrawQuadPic)(const vec2_t positions[4], const vec2_t texCoords[4], int32_t shaderHandle); /* 33 */
    void (*StretchRaw)(int32_t x, int32_t y, int32_t width, int32_t height, int32_t columns, int32_t rows, const uint8_t *data,
                       int32_t client, qboolean dirty); /* 34 */
    void (*UploadCinematic)(int32_t width, int32_t height, int32_t columns, int32_t rows, const uint8_t *data, int32_t client,
                            qboolean dirty); /* 35 */
    void (*BeginFrame)(stereoFrame_t stereoFrame); /* 36 */
    void (*EndFrame)(int32_t *frontEndMsec, int32_t *backEndMsec); /* 37 */
    void (*SaveScreen)(void); /* 38 */
    void (*BlendSavedScreen)(int32_t duration); /* 39 */
    int32_t (*MarkFragments)(int32_t pointCount, const vec3_t *points, const vec3_t projectionOrigin, const axis_t projectionAxis,
                             float projectionRadius, int32_t maxPoints, polyVert_t *pointBuffer, int32_t maxFragments,
                             markFragment_t *fragmentBuffer, int32_t shaderHandle); /* 40 */
    void (*ModelBounds)(int32_t modelHandle, vec3_t mins, vec3_t maxs); /* 41 */
    void (*TrackStatistics)(renderer_frame_statistics_t *statistics); /* 42 */
    qboolean (*PickShader)(const vec3_t start, const vec3_t direction, char *materialName, char *surfaceFlags, char *contents,
                           int32_t bufferSize); /* 43 */
    void (*RegisterFont)(const char *name, int32_t pointSize, fontInfo_t *font, int32_t loadMode); /* 44 */
    qboolean (*GetEntityToken)(char *buffer, int32_t bufferSize); /* 45 */
    void (*ResetImageAllocations)(void); /* 46 */
    void (*FreeImageAllocations)(void); /* 47 */
    void (*CubemapShot)(const char *fileName, int32_t faceSize, cubemap_face_t face, float fresnelN0, float fresnelN1); /* 48 */
    void (*CubemapWaterShot)(const char *fileName, int32_t faceSize, cubemap_face_t face, const vec3_t horizonColor,
                             const vec3_t zenithColor); /* 49 */
    void (*LocateDebugStrings)(const struct client_debug_string_s *strings, int32_t stringCount); /* 50 */
    void (*LocateDebugLines)(const struct client_debug_line_s *lines, int32_t lineCount); /* 51 */
    void (*AddPlume)(const vec3_t origin, int32_t labelValue, const vec3_t color, int32_t duration); /* 52 */
    int32_t (*TextWidth)(const char *text, int32_t fontHandle, float scale, float fixedAdvance, int32_t limit); /* 53 */
    int32_t (*TextHeight)(int32_t fontHandle, float scale); /* 54 */
    void (*TextPaint)(float x, float y, int32_t fontHandle, float scale, const float color[4], const char *text, float fixedAdvance,
                      int32_t limit, int32_t textStyle); /* 55 */
    int32_t (*TextConsoleWidth)(const uint16_t *encoded, int32_t fontHandle, float scale, float fixedAdvance,
                                int32_t encodedCount); /* 56 */
    void (*TextConsolePaint)(float x, float y, int32_t fontHandle, float scale, const float color[4], const uint16_t *encoded,
                             float fixedAdvance, int32_t encodedCount, int32_t textStyle); /* 57 */
    void (*TextPaintWithCursor)(float x, float y, int32_t fontHandle, float scale, const float color[4], const char *text,
                                int32_t cursorPosition, uint8_t cursorCharacter, float fixedAdvance, int32_t limit,
                                int32_t textStyle); /* 58 */
} refexport_t;

extern refexport_t rendererExports;

/* Packed renderer copies selected by ^8 and ^9 after CL_UpdateColor updates
 * the client-side float colors. */

#ifdef __cplusplus
extern "C" {
#endif

/* The Windows CL_UpdateColor body calls this conversion directly because
 * MSVC inlines RB_UpdateColor. */
void RB_UpdateColorInternal(const float color[4], uint8_t outColor[4]);
void RB_UpdateColor(const float color8[4], const float color9[4]);

refexport_t *GetRefAPI(int32_t apiVersion, const refimport_t *imports);
void RE_BeginRegistration(glconfig_t *config);
XModel *RE_GetXModelByHandle(int32_t modelHandle);
int32_t RE_RegisterModel(const char *name, int32_t loadMode);
void RE_FinishLoadingModels(void);
void RE_SetIgnorePrecacheErrors(qboolean ignorePrecacheErrors);
qboolean RE_GetIgnorePrecacheErrors(void);
void R_ModelBounds(int32_t modelHandle, vec3_t mins, vec3_t maxs);
int32_t RE_MarkFragments(int32_t pointCount, const vec3_t *points, const vec3_t projectionOrigin, const axis_t projectionAxis,
                         float projectionRadius, int32_t maxPoints, polyVert_t *pointBuffer, int32_t maxFragments,
                         markFragment_t *fragmentBuffer, int32_t shaderHandle);
qboolean RE_PickShader(const vec3_t start, const vec3_t direction, char *materialName, char *surfaceFlags, char *contents,
                       int32_t bufferSize);
void RE_AddPlume(const vec3_t origin, int32_t labelValue, const vec3_t color, int32_t duration);
void *PbGlQuery(int32_t query);

#ifdef __cplusplus
}
#endif

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(refimport_t) == 0x98, "i386 renderer import-table size changed");
#define RENDERER_ASSERT_I386_EXPORT_SLOT(member, slot) \
    _Static_assert(offsetof(refexport_t, member) == (slot) * 4, "i386 renderer export slot " #member " moved"); \
    _Static_assert(sizeof(((refexport_t *)0)->member) == 4, "i386 renderer export slot " #member " extent changed")
_Static_assert(_Alignof(refexport_t) == 0x04, "i386 renderer export-table alignment changed");
RENDERER_ASSERT_I386_EXPORT_SLOT(Shutdown, 0);
RENDERER_ASSERT_I386_EXPORT_SLOT(BeginRegistration, 1);
RENDERER_ASSERT_I386_EXPORT_SLOT(GetXModelByHandle, 2);
RENDERER_ASSERT_I386_EXPORT_SLOT(RegisterModel, 3);
RENDERER_ASSERT_I386_EXPORT_SLOT(RegisterShader, 4);
RENDERER_ASSERT_I386_EXPORT_SLOT(RegisterShaderNoMip, 5);
RENDERER_ASSERT_I386_EXPORT_SLOT(LoadWorldMap, 6);
RENDERER_ASSERT_I386_EXPORT_SLOT(FinishLoadingModels, 7);
RENDERER_ASSERT_I386_EXPORT_SLOT(SetIgnorePrecacheErrors, 8);
RENDERER_ASSERT_I386_EXPORT_SLOT(GetIgnorePrecacheErrors, 9);
RENDERER_ASSERT_I386_EXPORT_SLOT(GetShaderFromModel, 10);
RENDERER_ASSERT_I386_EXPORT_SLOT(SetFXImageMemory, 11);
RENDERER_ASSERT_I386_EXPORT_SLOT(GetFXImageMemory, 12);
RENDERER_ASSERT_I386_EXPORT_SLOT(GetImageMemory, 13);
RENDERER_ASSERT_I386_EXPORT_SLOT(GetShaderName, 14);
RENDERER_ASSERT_I386_EXPORT_SLOT(GetFarPlaneDist, 15);
RENDERER_ASSERT_I386_EXPORT_SLOT(EndRegistration, 16);
RENDERER_ASSERT_I386_EXPORT_SLOT(ClearScene, 17);
RENDERER_ASSERT_I386_EXPORT_SLOT(AddRefEntityToScene, 18);
RENDERER_ASSERT_I386_EXPORT_SLOT(AddPolyToScene, 19);
RENDERER_ASSERT_I386_EXPORT_SLOT(AddPolysToScene, 20);
RENDERER_ASSERT_I386_EXPORT_SLOT(AddLightToScene, 21);
RENDERER_ASSERT_I386_EXPORT_SLOT(SetCullDist, 22);
RENDERER_ASSERT_I386_EXPORT_SLOT(AddCoronaToScene, 23);
RENDERER_ASSERT_I386_EXPORT_SLOT(SetFog, 24);
RENDERER_ASSERT_I386_EXPORT_SLOT(SaveFogState, 25);
RENDERER_ASSERT_I386_EXPORT_SLOT(RestoreFogState, 26);
RENDERER_ASSERT_I386_EXPORT_SLOT(RenderScene, 27);
RENDERER_ASSERT_I386_EXPORT_SLOT(ClearFlares, 28);
RENDERER_ASSERT_I386_EXPORT_SLOT(SetColor, 29);
RENDERER_ASSERT_I386_EXPORT_SLOT(StretchPic, 30);
RENDERER_ASSERT_I386_EXPORT_SLOT(StretchPicGradient, 31);
RENDERER_ASSERT_I386_EXPORT_SLOT(StretchPicRotate, 32);
RENDERER_ASSERT_I386_EXPORT_SLOT(DrawQuadPic, 33);
RENDERER_ASSERT_I386_EXPORT_SLOT(StretchRaw, 34);
RENDERER_ASSERT_I386_EXPORT_SLOT(UploadCinematic, 35);
RENDERER_ASSERT_I386_EXPORT_SLOT(BeginFrame, 36);
RENDERER_ASSERT_I386_EXPORT_SLOT(EndFrame, 37);
RENDERER_ASSERT_I386_EXPORT_SLOT(SaveScreen, 38);
RENDERER_ASSERT_I386_EXPORT_SLOT(BlendSavedScreen, 39);
RENDERER_ASSERT_I386_EXPORT_SLOT(MarkFragments, 40);
RENDERER_ASSERT_I386_EXPORT_SLOT(ModelBounds, 41);
RENDERER_ASSERT_I386_EXPORT_SLOT(TrackStatistics, 42);
RENDERER_ASSERT_I386_EXPORT_SLOT(PickShader, 43);
RENDERER_ASSERT_I386_EXPORT_SLOT(RegisterFont, 44);
RENDERER_ASSERT_I386_EXPORT_SLOT(GetEntityToken, 45);
RENDERER_ASSERT_I386_EXPORT_SLOT(ResetImageAllocations, 46);
RENDERER_ASSERT_I386_EXPORT_SLOT(FreeImageAllocations, 47);
RENDERER_ASSERT_I386_EXPORT_SLOT(CubemapShot, 48);
RENDERER_ASSERT_I386_EXPORT_SLOT(CubemapWaterShot, 49);
RENDERER_ASSERT_I386_EXPORT_SLOT(LocateDebugStrings, 50);
RENDERER_ASSERT_I386_EXPORT_SLOT(LocateDebugLines, 51);
RENDERER_ASSERT_I386_EXPORT_SLOT(AddPlume, 52);
RENDERER_ASSERT_I386_EXPORT_SLOT(TextWidth, 53);
RENDERER_ASSERT_I386_EXPORT_SLOT(TextHeight, 54);
RENDERER_ASSERT_I386_EXPORT_SLOT(TextPaint, 55);
RENDERER_ASSERT_I386_EXPORT_SLOT(TextConsoleWidth, 56);
RENDERER_ASSERT_I386_EXPORT_SLOT(TextConsolePaint, 57);
RENDERER_ASSERT_I386_EXPORT_SLOT(TextPaintWithCursor, 58);
#undef RENDERER_ASSERT_I386_EXPORT_SLOT
_Static_assert(offsetof(refexport_t, ClearScene) == 0x44, "i386 renderer ClearScene export offset changed");
_Static_assert(offsetof(refexport_t, SaveFogState) == 0x64, "i386 renderer fog-save export offset changed");
_Static_assert(offsetof(refexport_t, RenderScene) == 0x6c, "i386 renderer RenderScene export offset changed");
_Static_assert(offsetof(refexport_t, BeginFrame) == 0x90, "i386 renderer BeginFrame export offset changed");
_Static_assert(offsetof(refexport_t, CubemapShot) == 0xc0, "i386 renderer CubemapShot export offset changed");
_Static_assert(offsetof(refexport_t, TextPaintWithCursor) == 0xe8, "i386 renderer final export offset changed");
_Static_assert(sizeof(refexport_t) == 0xec, "i386 renderer export-table size changed");
#endif

#endif
