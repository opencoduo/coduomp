#include "wgl_debug.h"

#include "gl_debug.h"
#include "client/engine/q_shared.h"
#include "../platform/dynamic_library_boundary.h"

/* Source: CoDUOMP.exe 0x004d0070..0x004d05d5. The 29 Win32/WGL logging
 * wrappers write their exact .rdata entry-point names, flush the shared QGL
 * stream, and tail-call the corresponding platform entry. Return values and
 * opaque native handles pass through unchanged. */
int32_t RENDERER_GL_API_CALL WGL_LogCopyContext(renderer_wgl_render_context_t source, renderer_wgl_render_context_t destination,
                                                uint32_t mask)
{
    fprintf(rendererGlLogFile, "wglCopyContext\n");
    fflush(rendererGlLogFile);
    return rendererWglCopyContextDriver(source, destination, mask);
}

renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_LogCreateContext(renderer_wgl_device_context_t deviceContext)
{
    fprintf(rendererGlLogFile, "wglCreateContext\n");
    fflush(rendererGlLogFile);
    return rendererWglCreateContextDriver(deviceContext);
}

renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_LogCreateLayerContext(renderer_wgl_device_context_t deviceContext,
                                                                             int32_t layerPlane)
{
    fprintf(rendererGlLogFile, "wglCreateLayerContext\n");
    fflush(rendererGlLogFile);
    return rendererWglCreateLayerContextDriver(deviceContext, layerPlane);
}

int32_t RENDERER_GL_API_CALL WGL_LogDeleteContext(renderer_wgl_render_context_t renderContext)
{
    fprintf(rendererGlLogFile, "wglDeleteContext\n");
    fflush(rendererGlLogFile);
    return rendererWglDeleteContextDriver(renderContext);
}

renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_LogGetCurrentContext(void)
{
    fprintf(rendererGlLogFile, "wglGetCurrentContext\n");
    fflush(rendererGlLogFile);
    return rendererWglGetCurrentContextDriver();
}

renderer_wgl_device_context_t RENDERER_GL_API_CALL WGL_LogGetCurrentDC(void)
{
    fprintf(rendererGlLogFile, "wglGetCurrentDC\n");
    fflush(rendererGlLogFile);
    return rendererWglGetCurrentDCDriver();
}

renderer_wgl_proc_t RENDERER_GL_API_CALL WGL_LogGetProcAddress(const char *name)
{
    fprintf(rendererGlLogFile, "wglGetProcAddress\n");
    fflush(rendererGlLogFile);
    return rendererWglGetProcAddressDriver(name);
}

int32_t RENDERER_GL_API_CALL WGL_LogMakeCurrent(renderer_wgl_device_context_t deviceContext, renderer_wgl_render_context_t renderContext)
{
    fprintf(rendererGlLogFile, "wglMakeCurrent\n");
    fflush(rendererGlLogFile);
    return rendererWglMakeCurrentDriver(deviceContext, renderContext);
}

int32_t RENDERER_GL_API_CALL WGL_LogShareLists(renderer_wgl_render_context_t source, renderer_wgl_render_context_t destination)
{
    fprintf(rendererGlLogFile, "wglShareLists\n");
    fflush(rendererGlLogFile);
    return rendererWglShareListsDriver(source, destination);
}

int32_t RENDERER_GL_API_CALL WGL_LogUseFontBitmaps(renderer_wgl_device_context_t deviceContext, uint32_t firstGlyph, uint32_t glyphCount,
                                                   uint32_t listBase)
{
    fprintf(rendererGlLogFile, "wglUseFontBitmaps\n");
    fflush(rendererGlLogFile);
    return rendererWglUseFontBitmapsDriver(deviceContext, firstGlyph, glyphCount, listBase);
}

int32_t RENDERER_GL_API_CALL WGL_LogUseFontOutlines(renderer_wgl_device_context_t deviceContext, uint32_t firstGlyph, uint32_t glyphCount,
                                                    uint32_t listBase, float deviation, float extrusion, int32_t format,
                                                    renderer_wgl_glyph_metrics_float_t *metrics)
{
    fprintf(rendererGlLogFile, "wglUseFontOutlines\n");
    fflush(rendererGlLogFile);
    return rendererWglUseFontOutlinesDriver(deviceContext, firstGlyph, glyphCount, listBase, deviation, extrusion, format, metrics);
}

int32_t RENDERER_GL_API_CALL WGL_LogDescribeLayerPlane(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat, int32_t layerPlane,
                                                       uint32_t descriptorBytes, renderer_wgl_layer_plane_descriptor_t *descriptor)
{
    fprintf(rendererGlLogFile, "wglDescribeLayerPlane\n");
    fflush(rendererGlLogFile);
    return rendererWglDescribeLayerPlaneDriver(deviceContext, pixelFormat, layerPlane, descriptorBytes, descriptor);
}

int32_t RENDERER_GL_API_CALL WGL_LogSetLayerPaletteEntries(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                           int32_t firstEntry, int32_t entryCount, const uint32_t *colors)
{
    fprintf(rendererGlLogFile, "wglSetLayerPaletteEntries\n");
    fflush(rendererGlLogFile);
    return rendererWglSetLayerPaletteEntriesDriver(deviceContext, layerPlane, firstEntry, entryCount, colors);
}

int32_t RENDERER_GL_API_CALL WGL_LogGetLayerPaletteEntries(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                           int32_t firstEntry, int32_t entryCount, uint32_t *colors)
{
    fprintf(rendererGlLogFile, "wglGetLayerPaletteEntries\n");
    fflush(rendererGlLogFile);
    return rendererWglGetLayerPaletteEntriesDriver(deviceContext, layerPlane, firstEntry, entryCount, colors);
}

int32_t RENDERER_GL_API_CALL WGL_LogRealizeLayerPalette(renderer_wgl_device_context_t deviceContext, int32_t layerPlane, int32_t realize)
{
    fprintf(rendererGlLogFile, "wglRealizeLayerPalette\n");
    fflush(rendererGlLogFile);
    return rendererWglRealizeLayerPaletteDriver(deviceContext, layerPlane, realize);
}

int32_t RENDERER_GL_API_CALL WGL_LogSwapLayerBuffers(renderer_wgl_device_context_t deviceContext, uint32_t planes)
{
    fprintf(rendererGlLogFile, "wglSwapLayerBuffers\n");
    fflush(rendererGlLogFile);
    return rendererWglSwapLayerBuffersDriver(deviceContext, planes);
}

const char *RENDERER_GL_API_CALL WGL_LogGetExtensionsStringEXT(void)
{
    fprintf(rendererGlLogFile, "wglGetExtensionsStringEXT\n");
    fflush(rendererGlLogFile);
    return rendererWglGetExtensionsStringEXTDriver();
}

int32_t RENDERER_GL_API_CALL WGL_LogSwapIntervalEXT(int32_t interval)
{
    fprintf(rendererGlLogFile, "wglSwapIntervalEXT\n");
    fflush(rendererGlLogFile);
    return rendererWglSwapIntervalEXTDriver(interval);
}

int32_t RENDERER_GL_API_CALL WGL_LogGetPixelFormatAttribivARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                              int32_t layerPlane, uint32_t attributeCount, const int32_t *attributes,
                                                              int32_t *values)
{
    fprintf(rendererGlLogFile, "wglGetPixelFormatAttribivARB\n");
    fflush(rendererGlLogFile);
    return rendererWglGetPixelFormatAttribivARBDriver(deviceContext, pixelFormat, layerPlane, attributeCount, attributes, values);
}

int32_t RENDERER_GL_API_CALL WGL_LogGetPixelFormatAttribfvARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                              int32_t layerPlane, uint32_t attributeCount, const int32_t *attributes,
                                                              float *values)
{
    fprintf(rendererGlLogFile, "wglGetPixelFormatAttribfvARB\n");
    fflush(rendererGlLogFile);
    return rendererWglGetPixelFormatAttribfvARBDriver(deviceContext, pixelFormat, layerPlane, attributeCount, attributes, values);
}

int32_t RENDERER_GL_API_CALL WGL_LogChoosePixelFormatARB(renderer_wgl_device_context_t deviceContext, const int32_t *integerAttributes,
                                                         const float *floatAttributes, uint32_t maximumFormats, int32_t *formats,
                                                         uint32_t *formatCount)
{
    fprintf(rendererGlLogFile, "wglChoosePixelFormatARB\n");
    fflush(rendererGlLogFile);
    return rendererWglChoosePixelFormatARBDriver(deviceContext, integerAttributes, floatAttributes, maximumFormats, formats, formatCount);
}

renderer_wgl_pbuffer_t RENDERER_GL_API_CALL WGL_LogCreatePbufferARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                                    int32_t width, int32_t height, const int32_t *attributes)
{
    fprintf(rendererGlLogFile, "wglCreatePbufferARB\n");
    fflush(rendererGlLogFile);
    return rendererWglCreatePbufferARBDriver(deviceContext, pixelFormat, width, height, attributes);
}

renderer_wgl_device_context_t RENDERER_GL_API_CALL WGL_LogGetPbufferDCARB(renderer_wgl_pbuffer_t pbuffer)
{
    fprintf(rendererGlLogFile, "wglGetPbufferDCARB\n");
    fflush(rendererGlLogFile);
    return rendererWglGetPbufferDCARBDriver(pbuffer);
}

int32_t RENDERER_GL_API_CALL WGL_LogReleasePbufferDCARB(renderer_wgl_pbuffer_t pbuffer, renderer_wgl_device_context_t deviceContext)
{
    fprintf(rendererGlLogFile, "wglReleasePbufferDCARB\n");
    fflush(rendererGlLogFile);
    return rendererWglReleasePbufferDCARBDriver(pbuffer, deviceContext);
}

int32_t RENDERER_GL_API_CALL WGL_LogDestroyPbufferARB(renderer_wgl_pbuffer_t pbuffer)
{
    fprintf(rendererGlLogFile, "wglDestroyPbufferARB\n");
    fflush(rendererGlLogFile);
    return rendererWglDestroyPbufferARBDriver(pbuffer);
}

int32_t RENDERER_GL_API_CALL WGL_LogQueryPbufferARB(renderer_wgl_pbuffer_t pbuffer, int32_t attribute, int32_t *value)
{
    fprintf(rendererGlLogFile, "wglQueryPbufferARB\n");
    fflush(rendererGlLogFile);
    return rendererWglQueryPbufferARBDriver(pbuffer, attribute, value);
}

int32_t RENDERER_GL_API_CALL WGL_LogBindTexImageARB(renderer_wgl_pbuffer_t pbuffer, int32_t buffer)
{
    fprintf(rendererGlLogFile, "wglBindTexImageARB\n");
    fflush(rendererGlLogFile);
    return rendererWglBindTexImageARBDriver(pbuffer, buffer);
}

int32_t RENDERER_GL_API_CALL WGL_LogReleaseTexImageARB(renderer_wgl_pbuffer_t pbuffer, int32_t buffer)
{
    fprintf(rendererGlLogFile, "wglReleaseTexImageARB\n");
    fflush(rendererGlLogFile);
    return rendererWglReleaseTexImageARBDriver(pbuffer, buffer);
}

int32_t RENDERER_GL_API_CALL WGL_LogSetPbufferAttribARB(renderer_wgl_pbuffer_t pbuffer, const int32_t *attributes)
{
    fprintf(rendererGlLogFile, "wglSetPbufferAttribARB\n");
    fflush(rendererGlLogFile);
    return rendererWglSetPbufferAttribARBDriver(pbuffer, attributes);
}

/* Source: CoDUOMP.exe 0x004d6910..0x004d7000. The checked WGL layer preserves
 * each native return value across the unconditional GetLastError call, prints
 * a diagnostic only for a nonzero error, and then returns the saved value. */
int32_t RENDERER_GL_API_CALL WGL_CheckedCopyContext(renderer_wgl_render_context_t source, renderer_wgl_render_context_t destination,
                                                    uint32_t mask)
{
    int32_t result = rendererWglCopyContextDriver(source, destination, mask);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglCopyContext: GetLastError() = 0x%04x\n", error);
    return result;
}

renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_CheckedCreateContext(renderer_wgl_device_context_t deviceContext)
{
    renderer_wgl_render_context_t result = rendererWglCreateContextDriver(deviceContext);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglCreateContext: GetLastError() = 0x%04x\n", error);
    return result;
}

renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_CheckedCreateLayerContext(renderer_wgl_device_context_t deviceContext,
                                                                                 int32_t layerPlane)
{
    renderer_wgl_render_context_t result = rendererWglCreateLayerContextDriver(deviceContext, layerPlane);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglCreateLayerContext: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedDeleteContext(renderer_wgl_render_context_t renderContext)
{
    int32_t result = rendererWglDeleteContextDriver(renderContext);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglDeleteContext: GetLastError() = 0x%04x\n", error);
    return result;
}

renderer_wgl_render_context_t RENDERER_GL_API_CALL WGL_CheckedGetCurrentContext(void)
{
    renderer_wgl_render_context_t result = rendererWglGetCurrentContextDriver();
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglGetCurrentContext: GetLastError() = 0x%04x\n", error);
    return result;
}

renderer_wgl_device_context_t RENDERER_GL_API_CALL WGL_CheckedGetCurrentDC(void)
{
    renderer_wgl_device_context_t result = rendererWglGetCurrentDCDriver();
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglGetCurrentDC: GetLastError() = 0x%04x\n", error);
    return result;
}

renderer_wgl_proc_t RENDERER_GL_API_CALL WGL_CheckedGetProcAddress(const char *name)
{
    renderer_wgl_proc_t result = rendererWglGetProcAddressDriver(name);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglGetProcAddress: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedMakeCurrent(renderer_wgl_device_context_t deviceContext,
                                                    renderer_wgl_render_context_t renderContext)
{
    int32_t result = rendererWglMakeCurrentDriver(deviceContext, renderContext);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglMakeCurrent: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedShareLists(renderer_wgl_render_context_t source, renderer_wgl_render_context_t destination)
{
    int32_t result = rendererWglShareListsDriver(source, destination);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglShareLists: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedUseFontBitmaps(renderer_wgl_device_context_t deviceContext, uint32_t firstGlyph,
                                                       uint32_t glyphCount, uint32_t listBase)
{
    int32_t result = rendererWglUseFontBitmapsDriver(deviceContext, firstGlyph, glyphCount, listBase);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglUseFontBitmaps: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedUseFontOutlines(renderer_wgl_device_context_t deviceContext, uint32_t firstGlyph,
                                                        uint32_t glyphCount, uint32_t listBase, float deviation, float extrusion,
                                                        int32_t format, renderer_wgl_glyph_metrics_float_t *metrics)
{
    int32_t result =
        rendererWglUseFontOutlinesDriver(deviceContext, firstGlyph, glyphCount, listBase, deviation, extrusion, format, metrics);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglUseFontOutlines: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedDescribeLayerPlane(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                           int32_t layerPlane, uint32_t descriptorBytes,
                                                           renderer_wgl_layer_plane_descriptor_t *descriptor)
{
    int32_t result = rendererWglDescribeLayerPlaneDriver(deviceContext, pixelFormat, layerPlane, descriptorBytes, descriptor);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglDescribeLayerPlane: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedSetLayerPaletteEntries(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                               int32_t firstEntry, int32_t entryCount, const uint32_t *colors)
{
    int32_t result = rendererWglSetLayerPaletteEntriesDriver(deviceContext, layerPlane, firstEntry, entryCount, colors);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglSetLayerPaletteEntries: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedGetLayerPaletteEntries(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                               int32_t firstEntry, int32_t entryCount, uint32_t *colors)
{
    int32_t result = rendererWglGetLayerPaletteEntriesDriver(deviceContext, layerPlane, firstEntry, entryCount, colors);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglGetLayerPaletteEntries: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedRealizeLayerPalette(renderer_wgl_device_context_t deviceContext, int32_t layerPlane,
                                                            int32_t realize)
{
    int32_t result = rendererWglRealizeLayerPaletteDriver(deviceContext, layerPlane, realize);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglRealizeLayerPalette: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedSwapLayerBuffers(renderer_wgl_device_context_t deviceContext, uint32_t planes)
{
    int32_t result = rendererWglSwapLayerBuffersDriver(deviceContext, planes);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglSwapLayerBuffers: GetLastError() = 0x%04x\n", error);
    return result;
}

const char *RENDERER_GL_API_CALL WGL_CheckedGetExtensionsStringEXT(void)
{
    const char *result = rendererWglGetExtensionsStringEXTDriver();
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglGetExtensionsStringEXT: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedSwapIntervalEXT(int32_t interval)
{
    int32_t result = rendererWglSwapIntervalEXTDriver(interval);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglSwapIntervalEXT: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedGetPixelFormatAttribivARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                                  int32_t layerPlane, uint32_t attributeCount, const int32_t *attributes,
                                                                  int32_t *values)
{
    int32_t result = rendererWglGetPixelFormatAttribivARBDriver(deviceContext, pixelFormat, layerPlane, attributeCount, attributes, values);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglGetPixelFormatAttribivARB: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedGetPixelFormatAttribfvARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                                  int32_t layerPlane, uint32_t attributeCount, const int32_t *attributes,
                                                                  float *values)
{
    int32_t result = rendererWglGetPixelFormatAttribfvARBDriver(deviceContext, pixelFormat, layerPlane, attributeCount, attributes, values);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglGetPixelFormatAttribfvARB: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedChoosePixelFormatARB(renderer_wgl_device_context_t deviceContext, const int32_t *integerAttributes,
                                                             const float *floatAttributes, uint32_t maximumFormats, int32_t *formats,
                                                             uint32_t *formatCount)
{
    int32_t result =
        rendererWglChoosePixelFormatARBDriver(deviceContext, integerAttributes, floatAttributes, maximumFormats, formats, formatCount);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglChoosePixelFormatARB: GetLastError() = 0x%04x\n", error);
    return result;
}

renderer_wgl_pbuffer_t RENDERER_GL_API_CALL WGL_CheckedCreatePbufferARB(renderer_wgl_device_context_t deviceContext, int32_t pixelFormat,
                                                                        int32_t width, int32_t height, const int32_t *attributes)
{
    renderer_wgl_pbuffer_t result = rendererWglCreatePbufferARBDriver(deviceContext, pixelFormat, width, height, attributes);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglCreatePbufferARB: GetLastError() = 0x%04x\n", error);
    return result;
}

renderer_wgl_device_context_t RENDERER_GL_API_CALL WGL_CheckedGetPbufferDCARB(renderer_wgl_pbuffer_t pbuffer)
{
    renderer_wgl_device_context_t result = rendererWglGetPbufferDCARBDriver(pbuffer);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglGetPbufferDCARB: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedReleasePbufferDCARB(renderer_wgl_pbuffer_t pbuffer, renderer_wgl_device_context_t deviceContext)
{
    int32_t result = rendererWglReleasePbufferDCARBDriver(pbuffer, deviceContext);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglReleasePbufferDCARB: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedDestroyPbufferARB(renderer_wgl_pbuffer_t pbuffer)
{
    int32_t result = rendererWglDestroyPbufferARBDriver(pbuffer);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglDestroyPbufferARB: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedQueryPbufferARB(renderer_wgl_pbuffer_t pbuffer, int32_t attribute, int32_t *value)
{
    int32_t result = rendererWglQueryPbufferARBDriver(pbuffer, attribute, value);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglQueryPbufferARB: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedBindTexImageARB(renderer_wgl_pbuffer_t pbuffer, int32_t buffer)
{
    int32_t result = rendererWglBindTexImageARBDriver(pbuffer, buffer);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglBindTexImageARB: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedReleaseTexImageARB(renderer_wgl_pbuffer_t pbuffer, int32_t buffer)
{
    int32_t result = rendererWglReleaseTexImageARBDriver(pbuffer, buffer);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglReleaseTexImageARB: GetLastError() = 0x%04x\n", error);
    return result;
}

int32_t RENDERER_GL_API_CALL WGL_CheckedSetPbufferAttribARB(renderer_wgl_pbuffer_t pbuffer, const int32_t *attributes)
{
    int32_t result = rendererWglSetPbufferAttribARBDriver(pbuffer, attributes);
    uint32_t error = coduomp_platform_last_error();
    if (error != 0)
        Com_Printf("^3wglSetPbufferAttribARB: GetLastError() = 0x%04x\n", error);
    return result;
}
