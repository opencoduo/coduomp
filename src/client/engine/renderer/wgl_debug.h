#ifndef CODUOMP_RENDERER_WGL_DEBUG_H
#define CODUOMP_RENDERER_WGL_DEBUG_H

#include <stddef.h>
#include <stdint.h>

#include "gl_api.h"

/* Platform-neutral carriers for Win32's opaque WGL handles. They remain real
 * native pointers on 64-bit hosts; no Windows handle is narrowed to a PE32
 * word in maintained source. */
typedef struct renderer_wgl_device_context_s *renderer_wgl_device_context_t;
typedef struct renderer_wgl_render_context_s *renderer_wgl_render_context_t;
typedef struct renderer_wgl_pbuffer_s *renderer_wgl_pbuffer_t;
typedef struct renderer_wgl_glyph_metrics_float_s renderer_wgl_glyph_metrics_float_t;
typedef struct renderer_wgl_layer_plane_descriptor_s renderer_wgl_layer_plane_descriptor_t;
typedef void(RENDERER_GL_API_CALL *renderer_wgl_proc_t)(void);
typedef intptr_t(RENDERER_GL_API_CALL *renderer_win32_window_proc_t)(void *window, uint32_t message, uintptr_t wParam, intptr_t lParam);

/* Platform-neutral spelling of the Win32 PIXELFORMATDESCRIPTOR. This is an
 * operating-system ABI record rather than a host-pointer-bearing game struct,
 * so its 40-byte layout is identical for 32- and 64-bit Windows. */
typedef struct renderer_pixel_format_descriptor_s {
    uint16_t size;
    uint16_t version;
    uint32_t flags;
    uint8_t pixelType;
    uint8_t colorBits;
    uint8_t redBits;
    uint8_t redShift;
    uint8_t greenBits;
    uint8_t greenShift;
    uint8_t blueBits;
    uint8_t blueShift;
    uint8_t alphaBits;
    uint8_t alphaShift;
    uint8_t accumBits;
    uint8_t accumRedBits;
    uint8_t accumGreenBits;
    uint8_t accumBlueBits;
    uint8_t accumAlphaBits;
    uint8_t depthBits;
    uint8_t stencilBits;
    uint8_t auxBuffers;
    uint8_t layerType;
    uint8_t reserved;
    uint32_t layerMask;
    uint32_t visibleMask;
    uint32_t damageMask;
} renderer_pixel_format_descriptor_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_pixel_format_descriptor_t) == 4, "i386 pixel-format descriptor alignment changed");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, size) == 0x00, "i386 pixel-format descriptor size word moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, version) == 0x02, "i386 pixel-format descriptor version moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, flags) == 0x04, "i386 pixel-format descriptor flags moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, pixelType) == 0x08, "i386 pixel-format descriptor pixel type moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, colorBits) == 0x09, "i386 pixel-format descriptor color bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, redBits) == 0x0a, "i386 pixel-format descriptor red bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, redShift) == 0x0b, "i386 pixel-format descriptor red shift moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, greenBits) == 0x0c, "i386 pixel-format descriptor green bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, greenShift) == 0x0d, "i386 pixel-format descriptor green shift moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, blueBits) == 0x0e, "i386 pixel-format descriptor blue bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, blueShift) == 0x0f, "i386 pixel-format descriptor blue shift moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, alphaBits) == 0x10, "i386 pixel-format descriptor alpha bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, alphaShift) == 0x11, "i386 pixel-format descriptor alpha shift moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, accumBits) == 0x12, "i386 pixel-format descriptor accumulation bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, accumRedBits) == 0x13,
               "i386 pixel-format descriptor accumulation red bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, accumGreenBits) == 0x14,
               "i386 pixel-format descriptor accumulation green bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, accumBlueBits) == 0x15,
               "i386 pixel-format descriptor accumulation blue bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, accumAlphaBits) == 0x16,
               "i386 pixel-format descriptor accumulation alpha bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, depthBits) == 0x17, "i386 pixel-format descriptor depth bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, stencilBits) == 0x18, "i386 pixel-format descriptor stencil bits moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, auxBuffers) == 0x19, "i386 pixel-format descriptor auxiliary count moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, layerType) == 0x1a, "i386 pixel-format descriptor layer type moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, reserved) == 0x1b, "i386 pixel-format descriptor reserved byte moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, layerMask) == 0x1c, "i386 pixel-format descriptor layer mask moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, visibleMask) == 0x20, "i386 pixel-format descriptor visible mask moved");
_Static_assert(offsetof(renderer_pixel_format_descriptor_t, damageMask) == 0x24, "i386 pixel-format descriptor damage mask moved");
_Static_assert(sizeof(renderer_pixel_format_descriptor_t) == 0x28, "i386 pixel-format descriptor size changed");
#endif

typedef enum renderer_wgl_setup_result_e {
    R_WGL_SETUP_SUCCESS = 0,
    R_WGL_SETUP_PIXEL_FORMAT_FAILED = 1,
    R_WGL_SETUP_CONTEXT_FAILED = 2
} renderer_wgl_setup_result_t;

typedef enum renderer_mode_set_result_e {
    R_MODE_SET_SUCCESS = 0,
    R_MODE_SET_FULLSCREEN_UNAVAILABLE = 1,
    R_MODE_SET_INVALID = 2
} renderer_mode_set_result_t;

/* Original 0x039826e4. The Win32 GL window setup owns this HDC; retained as
 * an opaque native pointer so it widens normally on 64-bit Windows. */
extern renderer_wgl_device_context_t rendererWin32DeviceContext;
extern renderer_wgl_render_context_t rendererWin32RenderContext;
extern qboolean rendererWin32PixelFormatSet;
extern int32_t rendererWin32DesktopColorBits;
extern int32_t rendererWin32DesktopWidth;
extern int32_t rendererWin32DesktopHeight;
extern renderer_win32_window_proc_t rendererWin32WindowProcedure;
/* Original 0x0389f6e0..0x0389fce0, captured by GLimp_InitGamma and restored
 * by GLimp_Shutdown. */
extern uint16_t rendererOriginalGammaRamp[3][256];

void GLW_BuildPFD(renderer_pixel_format_descriptor_t *descriptor, int32_t colorBits, int32_t depthBits, int32_t stencilBits,
                  qboolean stereo);
int32_t GLW_ChoosePFD(renderer_wgl_device_context_t deviceContext, renderer_pixel_format_descriptor_t *preferred);
renderer_wgl_setup_result_t GLW_SetPFD(renderer_pixel_format_descriptor_t *descriptor);
qboolean GLW_InitDriver(int32_t colorBits);
qboolean GLW_CreateWindow(const char *driverName, int32_t width, int32_t height, int32_t colorBits, qboolean fullscreen);
void GLW_PrintDisplayChangeError(int32_t result);
renderer_mode_set_result_t GLW_SetMode(const char *driverName, int32_t mode, int32_t colorBits, qboolean fullscreen);
qboolean GLW_CheckOpenGLVersion(int32_t requiredMajor, int32_t requiredMinor);
qboolean GLW_HasExtension(const char *extensionString, const char *extensionName);
void GLW_MissingFeatureError(void);
void QGL_LoadARBVertexProgramFunctions(void);
qboolean GLW_CheckOSVersion(void);
qboolean GLW_StartDriverAndSetMode(const char *driverName);
qboolean GLW_TryFallbackMode(int32_t mode, const char *resolutionName);
qboolean GLW_LoadOpenGL(void);
void GLW_ApplyRendererAutoConfig(void);

typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_copy_context_func_t)(renderer_wgl_render_context_t source,
                                                                        renderer_wgl_render_context_t destination, uint32_t mask);
typedef renderer_wgl_render_context_t(RENDERER_GL_API_CALL *renderer_wgl_create_context_func_t)(
    renderer_wgl_device_context_t deviceContext);
typedef renderer_wgl_render_context_t(RENDERER_GL_API_CALL *renderer_wgl_create_layer_context_func_t)(
    renderer_wgl_device_context_t deviceContext, int32_t layerPlane);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_delete_context_func_t)(renderer_wgl_render_context_t renderContext);
typedef renderer_wgl_render_context_t(RENDERER_GL_API_CALL *renderer_wgl_get_current_context_func_t)(void);
typedef renderer_wgl_device_context_t(RENDERER_GL_API_CALL *renderer_wgl_get_current_dc_func_t)(void);
typedef renderer_wgl_proc_t(RENDERER_GL_API_CALL *renderer_wgl_get_proc_address_func_t)(const char *name);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_make_current_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                        renderer_wgl_render_context_t renderContext);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_share_lists_func_t)(renderer_wgl_render_context_t source,
                                                                       renderer_wgl_render_context_t destination);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_use_font_bitmaps_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                            uint32_t firstGlyph, uint32_t glyphCount, uint32_t listBase);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_use_font_outlines_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                             uint32_t firstGlyph, uint32_t glyphCount, uint32_t listBase,
                                                                             float deviation, float extrusion, int32_t format,
                                                                             renderer_wgl_glyph_metrics_float_t *metrics);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_describe_layer_plane_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                                int32_t pixelFormat, int32_t layerPlane,
                                                                                uint32_t descriptorBytes,
                                                                                renderer_wgl_layer_plane_descriptor_t *descriptor);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_set_layer_palette_entries_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                                     int32_t layerPlane, int32_t firstEntry,
                                                                                     int32_t entryCount, const uint32_t *colors);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_get_layer_palette_entries_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                                     int32_t layerPlane, int32_t firstEntry,
                                                                                     int32_t entryCount, uint32_t *colors);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_realize_layer_palette_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                                 int32_t layerPlane, int32_t realize);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_swap_layer_buffers_func_t)(renderer_wgl_device_context_t deviceContext, uint32_t planes);
typedef const char *(RENDERER_GL_API_CALL *renderer_wgl_get_extensions_string_ext_func_t)(void);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_swap_interval_ext_func_t)(int32_t interval);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_get_pixel_format_attribiv_arb_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                                         int32_t pixelFormat, int32_t layerPlane,
                                                                                         uint32_t attributeCount, const int32_t *attributes,
                                                                                         int32_t *values);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_get_pixel_format_attribfv_arb_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                                         int32_t pixelFormat, int32_t layerPlane,
                                                                                         uint32_t attributeCount, const int32_t *attributes,
                                                                                         float *values);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_choose_pixel_format_arb_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                                   const int32_t *integerAttributes,
                                                                                   const float *floatAttributes, uint32_t maximumFormats,
                                                                                   int32_t *formats, uint32_t *formatCount);
typedef renderer_wgl_pbuffer_t(RENDERER_GL_API_CALL *renderer_wgl_create_pbuffer_arb_func_t)(renderer_wgl_device_context_t deviceContext,
                                                                                             int32_t pixelFormat, int32_t width,
                                                                                             int32_t height, const int32_t *attributes);
typedef renderer_wgl_device_context_t(RENDERER_GL_API_CALL *renderer_wgl_get_pbuffer_dc_arb_func_t)(renderer_wgl_pbuffer_t pbuffer);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_release_pbuffer_dc_arb_func_t)(renderer_wgl_pbuffer_t pbuffer,
                                                                                  renderer_wgl_device_context_t deviceContext);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_destroy_pbuffer_arb_func_t)(renderer_wgl_pbuffer_t pbuffer);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_query_pbuffer_arb_func_t)(renderer_wgl_pbuffer_t pbuffer, int32_t attribute,
                                                                             int32_t *value);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_bind_tex_image_arb_func_t)(renderer_wgl_pbuffer_t pbuffer, int32_t buffer);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_release_tex_image_arb_func_t)(renderer_wgl_pbuffer_t pbuffer, int32_t buffer);
typedef int32_t(RENDERER_GL_API_CALL *renderer_wgl_set_pbuffer_attrib_arb_func_t)(renderer_wgl_pbuffer_t pbuffer,
                                                                                  const int32_t *attributes);

#define QGL_WGL_ENTRY(type_, name_) \
    extern type_ rendererWgl##name_##Driver; \
    extern type_ qwgl##name_;
#include "qgl_wgl_entries.h"
#undef QGL_WGL_ENTRY

#ifdef __cplusplus
extern "C" {
#endif

int32_t RENDERER_GL_API_CALL WGL_LogCopyContext(renderer_wgl_render_context_t source, renderer_wgl_render_context_t destination,
                                                uint32_t mask);
renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_LogCreateContext(renderer_wgl_device_context_t deviceContext);
renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_LogCreateLayerContext(renderer_wgl_device_context_t deviceContext,
                                                                             int32_t layerPlane);
int32_t RENDERER_GL_API_CALL WGL_LogDeleteContext(renderer_wgl_render_context_t renderContext);
renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_LogGetCurrentContext(void);
renderer_wgl_device_context_t RENDERER_GL_API_CALL WGL_LogGetCurrentDC(void);
renderer_wgl_proc_t RENDERER_GL_API_CALL WGL_LogGetProcAddress(const char *name);
int32_t RENDERER_GL_API_CALL WGL_LogMakeCurrent(renderer_wgl_device_context_t deviceContext, renderer_wgl_render_context_t renderContext);
int32_t RENDERER_GL_API_CALL WGL_LogShareLists(renderer_wgl_render_context_t source, renderer_wgl_render_context_t destination);
int32_t RENDERER_GL_API_CALL WGL_LogUseFontBitmaps(renderer_wgl_device_context_t deviceContext, uint32_t firstGlyph, uint32_t glyphCount,
                                                   uint32_t listBase);
int32_t RENDERER_GL_API_CALL WGL_LogUseFontOutlines(renderer_wgl_device_context_t deviceContext, uint32_t firstGlyph, uint32_t glyphCount,
                                                    uint32_t listBase, float deviation, float extrusion, int32_t format,
                                                    renderer_wgl_glyph_metrics_float_t *metrics);
int32_t RENDERER_GL_API_CALL WGL_LogDescribeLayerPlane(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat, int32_t layerPlane,
                                                       uint32_t descriptorBytes, renderer_wgl_layer_plane_descriptor_t *descriptor);
int32_t RENDERER_GL_API_CALL WGL_LogSetLayerPaletteEntries(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                           int32_t firstEntry, int32_t entryCount, const uint32_t *colors);
int32_t RENDERER_GL_API_CALL WGL_LogGetLayerPaletteEntries(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                           int32_t firstEntry, int32_t entryCount, uint32_t *colors);
int32_t RENDERER_GL_API_CALL WGL_LogRealizeLayerPalette(renderer_wgl_device_context_t deviceContext, int32_t layerPlane, int32_t realize);
int32_t RENDERER_GL_API_CALL WGL_LogSwapLayerBuffers(renderer_wgl_device_context_t deviceContext, uint32_t planes);
const char *RENDERER_GL_API_CALL WGL_LogGetExtensionsStringEXT(void);
int32_t RENDERER_GL_API_CALL WGL_LogSwapIntervalEXT(int32_t interval);
int32_t RENDERER_GL_API_CALL WGL_LogGetPixelFormatAttribivARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                              int32_t layerPlane, uint32_t attributeCount, const int32_t *attributes,
                                                              int32_t *values);
int32_t RENDERER_GL_API_CALL WGL_LogGetPixelFormatAttribfvARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                              int32_t layerPlane, uint32_t attributeCount, const int32_t *attributes,
                                                              float *values);
int32_t RENDERER_GL_API_CALL WGL_LogChoosePixelFormatARB(renderer_wgl_device_context_t deviceContext, const int32_t *integerAttributes,
                                                         const float *floatAttributes, uint32_t maximumFormats, int32_t *formats,
                                                         uint32_t *formatCount);
renderer_wgl_pbuffer_t RENDERER_GL_API_CALL WGL_LogCreatePbufferARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                                    int32_t width, int32_t height, const int32_t *attributes);
renderer_wgl_device_context_t RENDERER_GL_API_CALL WGL_LogGetPbufferDCARB(renderer_wgl_pbuffer_t pbuffer);
int32_t RENDERER_GL_API_CALL WGL_LogReleasePbufferDCARB(renderer_wgl_pbuffer_t pbuffer, renderer_wgl_device_context_t deviceContext);
int32_t RENDERER_GL_API_CALL WGL_LogDestroyPbufferARB(renderer_wgl_pbuffer_t pbuffer);
int32_t RENDERER_GL_API_CALL WGL_LogQueryPbufferARB(renderer_wgl_pbuffer_t pbuffer, int32_t attribute, int32_t *value);
int32_t RENDERER_GL_API_CALL WGL_LogBindTexImageARB(renderer_wgl_pbuffer_t pbuffer, int32_t buffer);
int32_t RENDERER_GL_API_CALL WGL_LogReleaseTexImageARB(renderer_wgl_pbuffer_t pbuffer, int32_t buffer);
int32_t RENDERER_GL_API_CALL WGL_LogSetPbufferAttribARB(renderer_wgl_pbuffer_t pbuffer, const int32_t *attributes);
int32_t RENDERER_GL_API_CALL WGL_CheckedCopyContext(renderer_wgl_render_context_t source, renderer_wgl_render_context_t destination,
                                                    uint32_t mask);
renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_CheckedCreateContext(renderer_wgl_device_context_t deviceContext);
renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_CheckedCreateLayerContext(renderer_wgl_device_context_t deviceContext,
                                                                                 int32_t layerPlane);
int32_t RENDERER_GL_API_CALL WGL_CheckedDeleteContext(renderer_wgl_render_context_t renderContext);
renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_CheckedGetCurrentContext(void);
renderer_wgl_device_context_t RENDERER_GL_API_CALL WGL_CheckedGetCurrentDC(void);
renderer_wgl_proc_t RENDERER_GL_API_CALL WGL_CheckedGetProcAddress(const char *name);
int32_t RENDERER_GL_API_CALL WGL_CheckedMakeCurrent(renderer_wgl_device_context_t deviceContext,
                                                    renderer_wgl_render_context_t renderContext);
int32_t RENDERER_GL_API_CALL WGL_CheckedShareLists(renderer_wgl_render_context_t source, renderer_wgl_render_context_t destination);
int32_t RENDERER_GL_API_CALL WGL_CheckedUseFontBitmaps(renderer_wgl_device_context_t deviceContext, uint32_t firstGlyph,
                                                       uint32_t glyphCount, uint32_t listBase);
int32_t RENDERER_GL_API_CALL WGL_CheckedUseFontOutlines(renderer_wgl_device_context_t deviceContext, uint32_t firstGlyph,
                                                        uint32_t glyphCount, uint32_t listBase, float deviation, float extrusion,
                                                        int32_t format, renderer_wgl_glyph_metrics_float_t *metrics);
int32_t RENDERER_GL_API_CALL WGL_CheckedDescribeLayerPlane(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                           int32_t layerPlane, uint32_t descriptorBytes,
                                                           renderer_wgl_layer_plane_descriptor_t *descriptor);
int32_t RENDERER_GL_API_CALL WGL_CheckedSetLayerPaletteEntries(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                               int32_t firstEntry, int32_t entryCount, const uint32_t *colors);
int32_t RENDERER_GL_API_CALL WGL_CheckedGetLayerPaletteEntries(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                               int32_t firstEntry, int32_t entryCount, uint32_t *colors);
int32_t RENDERER_GL_API_CALL WGL_CheckedRealizeLayerPalette(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                            int32_t realize);
int32_t RENDERER_GL_API_CALL WGL_CheckedSwapLayerBuffers(renderer_wgl_device_context_t deviceContext, uint32_t planes);
const char *RENDERER_GL_API_CALL WGL_CheckedGetExtensionsStringEXT(void);
int32_t RENDERER_GL_API_CALL WGL_CheckedSwapIntervalEXT(int32_t interval);
int32_t RENDERER_GL_API_CALL WGL_CheckedGetPixelFormatAttribivARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                                  int32_t layerPlane, uint32_t attributeCount, const int32_t *attributes,
                                                                  int32_t *values);
int32_t RENDERER_GL_API_CALL WGL_CheckedGetPixelFormatAttribfvARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                                  int32_t layerPlane, uint32_t attributeCount, const int32_t *attributes,
                                                                  float *values);
int32_t RENDERER_GL_API_CALL WGL_CheckedChoosePixelFormatARB(renderer_wgl_device_context_t deviceContext, const int32_t *integerAttributes,
                                                             const float *floatAttributes, uint32_t maximumFormats, int32_t *formats,
                                                             uint32_t *formatCount);
renderer_wgl_pbuffer_t RENDERER_GL_API_CALL WGL_CheckedCreatePbufferARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                                        int32_t width, int32_t height, const int32_t *attributes);
renderer_wgl_device_context_t RENDERER_GL_API_CALL WGL_CheckedGetPbufferDCARB(renderer_wgl_pbuffer_t pbuffer);
int32_t RENDERER_GL_API_CALL WGL_CheckedReleasePbufferDCARB(renderer_wgl_pbuffer_t pbuffer, renderer_wgl_device_context_t deviceContext);
int32_t RENDERER_GL_API_CALL WGL_CheckedDestroyPbufferARB(renderer_wgl_pbuffer_t pbuffer);
int32_t RENDERER_GL_API_CALL WGL_CheckedQueryPbufferARB(renderer_wgl_pbuffer_t pbuffer, int32_t attribute, int32_t *value);
int32_t RENDERER_GL_API_CALL WGL_CheckedBindTexImageARB(renderer_wgl_pbuffer_t pbuffer, int32_t buffer);
int32_t RENDERER_GL_API_CALL WGL_CheckedReleaseTexImageARB(renderer_wgl_pbuffer_t pbuffer, int32_t buffer);
int32_t RENDERER_GL_API_CALL WGL_CheckedSetPbufferAttribARB(renderer_wgl_pbuffer_t pbuffer, const int32_t *attributes);

#ifdef __cplusplus
}
#endif

#endif
