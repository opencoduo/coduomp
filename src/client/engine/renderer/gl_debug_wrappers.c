#include "gl_debug.h"

#include "gl_state.h"

#include <string.h>

/* Original 0x03982704 and 0x04899704. QGL_EnableLogging opens the stream and
 * installs the logging wrappers; the platform GL loader owns the real driver
 * entry points. */
FILE *rendererGlLogFile;

/* Source: CoDUOMP.exe 0x004f70d0..0x004f70e8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f70d0_004f70e9.mcode.
 * Name and argument: exact same-module Mac symbol GLimp_LogComment. */
void GLimp_LogComment(const char *comment)
{
    if (rendererGlLogFile != NULL)
        fprintf(rendererGlLogFile, "%s", comment);
}

/* Source: CoDUOMP.exe 0x004c9b40..0x004c9b86.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9b40_004c9b86.mcode. The explicit
 * float-to-double promotion is the normal variadic promotion used by the
 * original fprintf call. Provisional name by exact role. */
void RENDERER_GL_API_CALL GL_LogAlphaFunc(uint32_t func, float reference)
{
    fprintf(rendererGlLogFile, "glAlphaFunc( %s, %f )\n", GL_DepthFuncToString(func), (double)reference);
    fflush(rendererGlLogFile);
    rendererGlAlphaFuncDriver(func, reference);
}

/* Source: CoDUOMP.exe 0x004c9b90..0x004c9bc7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9b90_004c9bc7.mcode. */
void RENDERER_GL_API_CALL GL_LogBegin(uint32_t mode)
{
    fprintf(rendererGlLogFile, "glBegin( %s )\n", GL_PrimitiveModeToString(mode));
    fflush(rendererGlLogFile);
    rendererGlBeginDriver(mode);
}

/* Source: CoDUOMP.exe 0x004c9bd0..0x004c9c67.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9bd0_004c9c67.mcode. The load from
 * original tr.images at 0x04889b20 proves that image_t begins with the name
 * string passed to fprintf. Texture zero and unregistered texture numbers use
 * va("%u") exactly as the original wrapper does. */
void RENDERER_GL_API_CALL GL_LogBindTexture(uint32_t target, uint32_t texture)
{
    const char *textureName;

    if (texture != 0 && tr.images[texture] != NULL)
        textureName = tr.images[texture]->imgName;
    else
        textureName = va("%u", texture);

    if (target == GL_TEXTURE_2D) {
        fprintf(rendererGlLogFile, "glBindTexture( GL_TEXTURE_2D, %s )\n", textureName);
    } else if (target == GL_TEXTURE_CUBE_MAP_ARB) {
        fprintf(rendererGlLogFile, "glBindTexture( GL_TEXTURE_CUBE_MAP_ARB, %s )\n", textureName);
    } else {
        fprintf(rendererGlLogFile, "glBindTexture( 0x%x, %s )\n", target, textureName);
    }

    fflush(rendererGlLogFile);
    rendererGlBindTextureDriver(target, texture);
}

/* Source: CoDUOMP.exe 0x004c9c70..0x004c9caf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9c70_004c9caf.mcode. */
void RENDERER_GL_API_CALL GL_LogBindProgramARB(uint32_t target, uint32_t program)
{
    fprintf(rendererGlLogFile, "glBindProgramARB( %s, %u )\n", GL_EnumToString(target), program);
    fflush(rendererGlLogFile);
    rendererGlBindProgramARBDriver(target, program);
}

/* Source: CoDUOMP.exe 0x004c9cb0..0x004c9cde.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9cb0_004c9cde.mcode. */
void RENDERER_GL_API_CALL GL_LogBindFragmentShaderATI(uint32_t shader)
{
    fprintf(rendererGlLogFile, "glBindFragmentShaderATI( %u )\n", shader);
    fflush(rendererGlLogFile);
    rendererGlBindFragmentShaderATIDriver(shader);
}

/* Source: CoDUOMP.exe 0x004c9ce0..0x004c9d1f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9ce0_004c9d1f.mcode. */
void RENDERER_GL_API_CALL GL_LogBindBufferARB(uint32_t target, uint32_t buffer)
{
    fprintf(rendererGlLogFile, "glBindBufferARB( %s, %u )\n", GL_EnumToString(target), buffer);
    fflush(rendererGlLogFile);
    rendererGlBindBufferARBDriver(target, buffer);
}

/* Source: CoDUOMP.exe 0x004c9d20..0x004c9d76.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9d20_004c9d76.mcode. The original
 * i386 format uses %u for its 32-bit GLsizeiptrARB. GLsizeiptrARB is signed
 * pointer-width on native hosts; converting only for `%zu` preserves the
 * original unsigned rendering of its bits without changing the API type. */
void RENDERER_GL_API_CALL GL_LogBufferDataARB(uint32_t target, intptr_t size, const void *data, uint32_t usage)
{
    fprintf(rendererGlLogFile, "glBufferDataARB( %s, %zu, %p, %s )\n", GL_EnumToString(target), (size_t)size, data, GL_EnumToString(usage));
    fflush(rendererGlLogFile);
    rendererGlBufferDataARBDriver(target, size, data, usage);
}

/* Source: CoDUOMP.exe 0x004c9d80..0x004c9dc6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9d80_004c9dc6.mcode. */
void *RENDERER_GL_API_CALL GL_LogMapBufferARB(uint32_t target, uint32_t access)
{
    fprintf(rendererGlLogFile, "glMapBufferARB( %s, %s )\n", GL_EnumToString(target), GL_EnumToString(access));
    fflush(rendererGlLogFile);
    return rendererGlMapBufferARBDriver(target, access);
}

/* Source: CoDUOMP.exe 0x004c9dd0..0x004c9e07.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9dd0_004c9e07.mcode. */
uint8_t RENDERER_GL_API_CALL GL_LogUnmapBufferARB(uint32_t target)
{
    fprintf(rendererGlLogFile, "glUnmapBufferARB( %s )\n", GL_EnumToString(target));
    fflush(rendererGlLogFile);
    return rendererGlUnmapBufferARBDriver(target);
}

/* Source: CoDUOMP.exe 0x004c9f20..0x004c9fa6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9f20_004c9fa6.mcode. The Windows
 * stack-cookie sequence is compiler security instrumentation, not source
 * behavior; the two original 128-byte name buffers are retained. */
void RENDERER_GL_API_CALL GL_LogBlendFunc(uint32_t sourceFactor, uint32_t destinationFactor)
{
    char sourceName[128];
    char destinationName[128];

    GL_BlendFactorToString(sourceFactor, sourceName);
    GL_BlendFactorToString(destinationFactor, destinationName);
    fprintf(rendererGlLogFile, "glBlendFunc( %s, %s )\n", sourceName, destinationName);
    fflush(rendererGlLogFile);
    rendererGlBlendFuncDriver(sourceFactor, destinationFactor);
}

/* Source: CoDUOMP.exe 0x004c9fb0..0x004c9fde. */
void RENDERER_GL_API_CALL GL_LogCallList(uint32_t list)
{
    fprintf(rendererGlLogFile, "glCallList( %u )\n", list);
    fflush(rendererGlLogFile);
    rendererGlCallListDriver(list);
}

/* Source: CoDUOMP.exe 0x004c9fe0..0x004ca092. The four flag tests and their
 * output order follow the original instruction sequence exactly. */
void RENDERER_GL_API_CALL GL_LogClear(uint32_t mask)
{
    fprintf(rendererGlLogFile, "glClear( 0x%x = ", mask);
    if ((mask & GL_COLOR_BUFFER_BIT) != 0)
        fprintf(rendererGlLogFile, "GL_COLOR_BUFFER_BIT ");
    if ((mask & GL_DEPTH_BUFFER_BIT) != 0)
        fprintf(rendererGlLogFile, "GL_DEPTH_BUFFER_BIT ");
    if ((mask & GL_STENCIL_BUFFER_BIT) != 0)
        fprintf(rendererGlLogFile, "GL_STENCIL_BUFFER_BIT ");
    if ((mask & GL_ACCUM_BUFFER_BIT) != 0)
        fprintf(rendererGlLogFile, "GL_ACCUM_BUFFER_BIT ");
    fprintf(rendererGlLogFile, ")\n");
    fflush(rendererGlLogFile);
    rendererGlClearDriver(mask);
}

/* Source: CoDUOMP.exe 0x004ca0a0..0x004ca0d9. */
void RENDERER_GL_API_CALL GL_LogClearDepth(double depth)
{
    fprintf(rendererGlLogFile, "glClearDepth( %f )\n", depth);
    fflush(rendererGlLogFile);
    rendererGlClearDepthDriver(depth);
}

/* Source: CoDUOMP.exe 0x004ca0e0..0x004ca10e. */
void RENDERER_GL_API_CALL GL_LogClearStencil(int32_t stencil)
{
    fprintf(rendererGlLogFile, "glClearStencil( %d )\n", stencil);
    fflush(rendererGlLogFile);
    rendererGlClearStencilDriver(stencil);
}

/* Source: CoDUOMP.exe 0x004ca110..0x004ca157. */
void RENDERER_GL_API_CALL GL_LogColor4f(float red, float green, float blue, float alpha)
{
    fprintf(rendererGlLogFile, "glColor4f( %f,%f,%f,%f )\n", (double)red, (double)green, (double)blue, (double)alpha);
    fflush(rendererGlLogFile);
    rendererGlColor4fDriver(red, green, blue, alpha);
}

/* Source: CoDUOMP.exe 0x004ca160..0x004ca1ac. */
void RENDERER_GL_API_CALL GL_LogColor4fv(const float *color)
{
    fprintf(rendererGlLogFile, "glColor4fv( %f,%f,%f,%f )\n", (double)color[0], (double)color[1], (double)color[2], (double)color[3]);
    fflush(rendererGlLogFile);
    rendererGlColor4fvDriver(color);
}

/* Source: CoDUOMP.exe 0x004ca1b0..0x004ca206. */
void RENDERER_GL_API_CALL GL_LogColorPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer)
{
    fprintf(rendererGlLogFile, "glColorPointer( %d, %s, %d, %s )\n", size, GL_TypeToString(type), stride,
            GL_MemoryPointerToString(pointer));
    fflush(rendererGlLogFile);
    rendererGlColorPointerDriver(size, type, stride, pointer);
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
void RENDERER_GL_API_CALL GL_LogCullFace(uint32_t mode)
{
    const char *name = mode == GL_FRONT ? "GL_FRONT" : "GL_BACK";
    fprintf(rendererGlLogFile, "glCullFace( %s )\n", name);
    fflush(rendererGlLogFile);
    rendererGlCullFaceDriver(mode);
}

/* Source: CoDUOMP.exe 0x004ca260..0x004ca297. */
void RENDERER_GL_API_CALL GL_LogDepthFunc(uint32_t func)
{
    fprintf(rendererGlLogFile, "glDepthFunc( %s )\n", GL_DepthFuncToString(func));
    fflush(rendererGlLogFile);
    rendererGlDepthFuncDriver(func);
}

/* Source: CoDUOMP.exe 0x004ca2a0..0x004ca2ea. */
void RENDERER_GL_API_CALL GL_LogDepthMask(uint8_t enabled)
{
    const char *name;
    if (enabled == 0)
        name = "GL_FALSE";
    else if (enabled == 1)
        name = "GL_TRUE";
    else
        name = "OUT OF RANGE FOR BOOLEAN";

    fprintf(rendererGlLogFile, "glDepthMask( %s )\n", name);
    fflush(rendererGlLogFile);
    rendererGlDepthMaskDriver(enabled);
}

/* Source: CoDUOMP.exe 0x004ca2f0..0x004ca339. */
void RENDERER_GL_API_CALL GL_LogDepthRange(double nearValue, double farValue)
{
    fprintf(rendererGlLogFile, "glDepthRange( %f, %f )\n", nearValue, farValue);
    fflush(rendererGlLogFile);
    rendererGlDepthRangeDriver(nearValue, farValue);
}

/* Source: CoDUOMP.exe 0x004ca340..0x004ca377. */
void RENDERER_GL_API_CALL GL_LogDisable(uint32_t capability)
{
    fprintf(rendererGlLogFile, "glDisable( %s )\n", GL_EnumToString(capability));
    fflush(rendererGlLogFile);
    rendererGlDisableDriver(capability);
}

/* Source: CoDUOMP.exe 0x004ca380..0x004ca3b7. */
void RENDERER_GL_API_CALL GL_LogDisableClientState(uint32_t capability)
{
    fprintf(rendererGlLogFile, "glDisableClientState( %s )\n", GL_EnumToString(capability));
    fflush(rendererGlLogFile);
    rendererGlDisableClientStateDriver(capability);
}

/* Source: CoDUOMP.exe 0x004ca3c0..0x004ca41d. */
void RENDERER_GL_API_CALL GL_LogDrawElements(uint32_t mode, int32_t count, uint32_t type, const void *indices)
{
    fprintf(rendererGlLogFile, "glDrawElements( %s, %d, %s, %s )\n", GL_PrimitiveModeToString(mode), count, GL_TypeToString(type),
            GL_MemoryPointerToString(indices));
    fflush(rendererGlLogFile);
    rendererGlDrawElementsDriver(mode, count, type, indices);
}

/* Source: CoDUOMP.exe 0x004ca420..0x004ca490. */
void RENDERER_GL_API_CALL GL_LogDrawRangeElementsEXT(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type,
                                                     const void *indices)
{
    fprintf(rendererGlLogFile, "glDrawRangeElementsEXT( %s, %u, %u, %d, %s, %s )\n", GL_PrimitiveModeToString(mode), start, end, count,
            GL_TypeToString(type), GL_MemoryPointerToString(indices));
    fflush(rendererGlLogFile);
    rendererGlDrawRangeElementsEXTDriver(mode, start, end, count, type, indices);
}

/* Source: CoDUOMP.exe 0x004ca490..0x004ca4c7. */
void RENDERER_GL_API_CALL GL_LogEnable(uint32_t capability)
{
    fprintf(rendererGlLogFile, "glEnable( %s )\n", GL_EnumToString(capability));
    fflush(rendererGlLogFile);
    rendererGlEnableDriver(capability);
}

/* Source: CoDUOMP.exe 0x004ca4d0..0x004ca507. */
void RENDERER_GL_API_CALL GL_LogEnableClientState(uint32_t capability)
{
    fprintf(rendererGlLogFile, "glEnableClientState( %s )\n", GL_EnumToString(capability));
    fflush(rendererGlLogFile);
    rendererGlEnableClientStateDriver(capability);
}

/* Source: CoDUOMP.exe 0x004ca510..0x004ca548. The original logger deliberately
 * prints both GLenum values numerically instead of using GL_EnumToString. */
void RENDERER_GL_API_CALL GL_LogHint(uint32_t target, uint32_t mode)
{
    fprintf(rendererGlLogFile, "glHint( 0x%x, 0x%x )\n", target, mode);
    fflush(rendererGlLogFile);
    rendererGlHintDriver(target, mode);
}

/* Source: CoDUOMP.exe 0x004ca550..0x004ca58f and compiler jump table
 * 0x004ca590..0x004ca5a4. The hexadecimal fallback is produced through the
 * engine's rotating va() buffers exactly as in the original helper. */
const char *GL_FogParameterToString(uint32_t parameter)
{
    switch (parameter) {
    case GL_FOG_DENSITY:
        return "GL_FOG_DENSITY";
    case GL_FOG_START:
        return "GL_FOG_START";
    case GL_FOG_END:
        return "GL_FOG_END";
    case GL_FOG_MODE:
        return "GL_FOG_MODE";
    case GL_FOG_COLOR:
        return "GL_FOG_COLOR";
    default:
        return va("0x%4x", parameter);
    }
}

/* Source: CoDUOMP.exe 0x004ca5b0..0x004ca5ea. This wrapper does not flush the
 * log stream before forwarding; that omission is observable original behavior. */
void RENDERER_GL_API_CALL GL_LogFogf(uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glFogf( %s, %g )\n", GL_FogParameterToString(parameter), (double)value);
    rendererGlFogfDriver(parameter, value);
}

/* Source: CoDUOMP.exe 0x004ca5f0..0x004ca670. GL_FOG_COLOR logs all four
 * components; every other parameter logs only values[0]. The original wrapper
 * forwards without an explicit fflush in either branch. */
void RENDERER_GL_API_CALL GL_LogFogfv(uint32_t parameter, const float *values)
{
    if (parameter == GL_FOG_COLOR) {
        fprintf(rendererGlLogFile, "glFogfv( GL_FOG_COLOR, { %g, %g, %g, %g } )\n", (double)values[0], (double)values[1], (double)values[2],
                (double)values[3]);
    } else {
        fprintf(rendererGlLogFile, "glFogfv( %s, %g )\n", GL_FogParameterToString(parameter), (double)values[0]);
    }
    rendererGlFogfvDriver(parameter, values);
}

/* Source: CoDUOMP.exe 0x004ca670..0x004ca6a3. Like the adjacent fog wrappers,
 * this function forwards without an explicit fflush. */
void RENDERER_GL_API_CALL GL_LogFogi(uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glFogi( %s, %i )\n", GL_FogParameterToString(parameter), value);
    rendererGlFogiDriver(parameter, value);
}

/* Source: CoDUOMP.exe 0x004ca6b0..0x004ca733. GL_EnumToString uses one shared
 * fallback buffer, so the original copies the first result before asking for
 * the second. The 1024-byte local and stack-cookie instrumentation are compiler
 * shape; retaining the local preserves the observable two-result behavior. */
void RENDERER_GL_API_CALL GL_LogPolygonMode(uint32_t face, uint32_t mode)
{
    char faceName[MAX_STRING_CHARS];

    strcpy(faceName, GL_EnumToString(face));
    fprintf(rendererGlLogFile, "glPolygonMode( %s, %s )\n", faceName, GL_EnumToString(mode));
    fflush(rendererGlLogFile);
    rendererGlPolygonModeDriver(face, mode);
}

/* Source: CoDUOMP.exe 0x004ca740..0x004ca788. */
void RENDERER_GL_API_CALL GL_LogScissor(int32_t x, int32_t y, int32_t width, int32_t height)
{
    fprintf(rendererGlLogFile, "glScissor( %d, %d, %d, %d )\n", x, y, width, height);
    fflush(rendererGlLogFile);
    rendererGlScissorDriver(x, y, width, height);
}

/* Source: CoDUOMP.exe 0x004ca790..0x004ca7e6. */
void RENDERER_GL_API_CALL GL_LogTexCoordPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer)
{
    fprintf(rendererGlLogFile, "glTexCoordPointer( %d, %s, %d, %s )\n", size, GL_TypeToString(type), stride,
            GL_MemoryPointerToString(pointer));
    fflush(rendererGlLogFile);
    rendererGlTexCoordPointerDriver(size, type, stride, pointer);
}

/* Source: CoDUOMP.exe 0x004ca7f0..0x004ca888. The target is copied because
 * GL_EnumToString's fallback buffer is shared with the parameter conversion. */
void RENDERER_GL_API_CALL GL_LogTexEnvf(uint32_t target, uint32_t parameter, float value)
{
    char targetName[MAX_STRING_CHARS];

    strcpy(targetName, GL_EnumToString(target));
    fprintf(rendererGlLogFile, "glTexEnvf( %s, %s, %g )\n", targetName, GL_EnumToString(parameter), (double)value);
    fflush(rendererGlLogFile);
    rendererGlTexEnvfDriver(target, parameter, value);
}

/* Source: CoDUOMP.exe 0x004ca890..0x004ca946. Two values are copied before
 * converting the third because all three can use the same fallback buffer. */
void RENDERER_GL_API_CALL GL_LogTexEnvi(uint32_t target, uint32_t parameter, int32_t value)
{
    char targetName[MAX_STRING_CHARS];
    char parameterName[MAX_STRING_CHARS];

    strcpy(targetName, GL_EnumToString(target));
    strcpy(parameterName, GL_EnumToString(parameter));
    fprintf(rendererGlLogFile, "glTexEnvi( %s, %s, %s )\n", targetName, parameterName, GL_EnumToString((uint32_t)value));
    fflush(rendererGlLogFile);
    rendererGlTexEnviDriver(target, parameter, value);
}

/* Source: CoDUOMP.exe 0x004ca950..0x004ca997. */
void RENDERER_GL_API_CALL GL_LogTexParameterf(uint32_t target, uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glTexParameterf( 0x%x, 0x%x, %f )\n", target, parameter, (double)value);
    fflush(rendererGlLogFile);
    rendererGlTexParameterfDriver(target, parameter, value);
}

/* Source: CoDUOMP.exe 0x004ca9a0..0x004ca9e0. */
void RENDERER_GL_API_CALL GL_LogTexParameteri(uint32_t target, uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glTexParameteri( 0x%x, 0x%x, 0x%x )\n", target, parameter, (uint32_t)value);
    fflush(rendererGlLogFile);
    rendererGlTexParameteriDriver(target, parameter, value);
}

/* Source: CoDUOMP.exe 0x004ca9e0..0x004caa2e. */
void RENDERER_GL_API_CALL GL_LogNormalPointer(uint32_t type, int32_t stride, const void *pointer)
{
    fprintf(rendererGlLogFile, "glNormalPointer( %s, %d, %s )\n", GL_TypeToString(type), stride, GL_MemoryPointerToString(pointer));
    fflush(rendererGlLogFile);
    rendererGlNormalPointerDriver(type, stride, pointer);
}

/* Source: CoDUOMP.exe 0x004caa30..0x004caa86. */
void RENDERER_GL_API_CALL GL_LogVertexPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer)
{
    fprintf(rendererGlLogFile, "glVertexPointer( %d, %s, %d, %s )\n", size, GL_TypeToString(type), stride,
            GL_MemoryPointerToString(pointer));
    fflush(rendererGlLogFile);
    rendererGlVertexPointerDriver(size, type, stride, pointer);
}

/* Source: CoDUOMP.exe 0x004caa90..0x004caad8. */
void RENDERER_GL_API_CALL GL_LogViewport(int32_t x, int32_t y, int32_t width, int32_t height)
{
    fprintf(rendererGlLogFile, "glViewport( %d, %d, %d, %d )\n", x, y, width, height);
    fflush(rendererGlLogFile);
    rendererGlViewportDriver(x, y, width, height);
}

/* Source: CoDUOMP.exe 0x004caae0..0x004cab15. The logger intentionally omits
 * the condition argument from its text, but forwards it to the driver. */
void RENDERER_GL_API_CALL GL_LogSetFenceNV(uint32_t fence, uint32_t condition)
{
    fprintf(rendererGlLogFile, "glSetFenceNV( %d )\n", (int32_t)fence);
    fflush(rendererGlLogFile);
    rendererGlSetFenceNVDriver(fence, condition);
}

/* Source: CoDUOMP.exe 0x004cab20..0x004cab50. */
void RENDERER_GL_API_CALL GL_LogFinishFenceNV(uint32_t fence)
{
    fprintf(rendererGlLogFile, "glFinishFenceNV( %d )\n", (int32_t)fence);
    fflush(rendererGlLogFile);
    rendererGlFinishFenceNVDriver(fence);
}

/* Source: CoDUOMP.exe 0x004cab50..0x004cab87. The text reports the zero-based
 * texture-unit index, while the original GLenum is forwarded unchanged. */
void RENDERER_GL_API_CALL GL_LogActiveTextureARB(uint32_t texture)
{
    fprintf(rendererGlLogFile, "glActiveTextureARB( GL_TEXTURE%i_ARB )\n", (int32_t)(texture - GL_TEXTURE0_ARB));
    fflush(rendererGlLogFile);
    rendererGlActiveTextureARBDriver(texture);
}

/* Source: CoDUOMP.exe 0x004cab90..0x004cabc7. */
void RENDERER_GL_API_CALL GL_LogClientActiveTextureARB(uint32_t texture)
{
    fprintf(rendererGlLogFile, "glClientActiveTextureARB( GL_TEXTURE%i_ARB )\n", (int32_t)(texture - GL_TEXTURE0_ARB));
    fflush(rendererGlLogFile);
    rendererGlClientActiveTextureARBDriver(texture);
}

/* Source: CoDUOMP.exe 0x004cabd0..0x004cabf5. The original logger prints only
 * the entry-point name and forwards all three arguments unchanged. */
void RENDERER_GL_API_CALL GL_LogMultiTexCoord2fARB(uint32_t target, float s, float t)
{
    fprintf(rendererGlLogFile, "glMultiTexCoord2fARB\n");
    fflush(rendererGlLogFile);
    rendererGlMultiTexCoord2fARBDriver(target, s, t);
}

/* Source: CoDUOMP.exe 0x004cac00..0x004cae95. These extension wrappers log
 * only the entry-point name, flush, and forward the complete original API
 * signature. Separate functions are retained because those signatures are an
 * important part of the renderer/platform boundary. */
void RENDERER_GL_API_CALL GL_LogLockArraysEXT(int32_t first, int32_t count)
{
    fprintf(rendererGlLogFile, "glLockArraysEXT\n");
    fflush(rendererGlLogFile);
    rendererGlLockArraysEXTDriver(first, count);
}

void RENDERER_GL_API_CALL GL_LogUnlockArraysEXT(void)
{
    fprintf(rendererGlLogFile, "glUnlockArraysEXT\n");
    fflush(rendererGlLogFile);
    rendererGlUnlockArraysEXTDriver();
}

void RENDERER_GL_API_CALL GL_LogPNTrianglesiATI(uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glPNTrianglesiATI\n");
    fflush(rendererGlLogFile);
    rendererGlPNTrianglesiATIDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogPNTrianglesfATI(uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glPNTrianglesfATI\n");
    fflush(rendererGlLogFile);
    rendererGlPNTrianglesfATIDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogCompressedTexImage3DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                        int32_t height, int32_t depth, int32_t border, int32_t imageSize, const void *data)
{
    fprintf(rendererGlLogFile, "glCompressedTexImage3DARB\n");
    fflush(rendererGlLogFile);
    rendererGlCompressedTexImage3DARBDriver(target, level, internalFormat, width, height, depth, border, imageSize, data);
}

void RENDERER_GL_API_CALL GL_LogCompressedTexImage2DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                        int32_t height, int32_t border, int32_t imageSize, const void *data)
{
    fprintf(rendererGlLogFile, "glCompressedTexImage2DARB\n");
    fflush(rendererGlLogFile);
    rendererGlCompressedTexImage2DARBDriver(target, level, internalFormat, width, height, border, imageSize, data);
}

void RENDERER_GL_API_CALL GL_LogCompressedTexImage1DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                        int32_t border, int32_t imageSize, const void *data)
{
    fprintf(rendererGlLogFile, "glCompressedTexImage1DARB\n");
    fflush(rendererGlLogFile);
    rendererGlCompressedTexImage1DARBDriver(target, level, internalFormat, width, border, imageSize, data);
}

void RENDERER_GL_API_CALL GL_LogCompressedTexSubImage3DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
                                                           int32_t zOffset, int32_t width, int32_t height, int32_t depth, uint32_t format,
                                                           int32_t imageSize, const void *data)
{
    fprintf(rendererGlLogFile, "glCompressedTexSubImage3DARB\n");
    fflush(rendererGlLogFile);
    rendererGlCompressedTexSubImage3DARBDriver(target, level, xOffset, yOffset, zOffset, width, height, depth, format, imageSize, data);
}

void RENDERER_GL_API_CALL GL_LogCompressedTexSubImage2DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset, int32_t width,
                                                           int32_t height, uint32_t format, int32_t imageSize, const void *data)
{
    fprintf(rendererGlLogFile, "glCompressedTexSubImage2DARB\n");
    fflush(rendererGlLogFile);
    rendererGlCompressedTexSubImage2DARBDriver(target, level, xOffset, yOffset, width, height, format, imageSize, data);
}

void RENDERER_GL_API_CALL GL_LogCompressedTexSubImage1DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t width, uint32_t format,
                                                           int32_t imageSize, const void *data)
{
    fprintf(rendererGlLogFile, "glCompressedTexSubImage1DARB\n");
    fflush(rendererGlLogFile);
    rendererGlCompressedTexSubImage1DARBDriver(target, level, xOffset, width, format, imageSize, data);
}

void RENDERER_GL_API_CALL GL_LogGetCompressedTexImageARB(uint32_t target, int32_t level, void *image)
{
    fprintf(rendererGlLogFile, "glGetCompressedTexImageARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetCompressedTexImageARBDriver(target, level, image);
}

void RENDERER_GL_API_CALL GL_LogDeleteBuffersARB(int32_t count, const uint32_t *buffers)
{
    fprintf(rendererGlLogFile, "glDeleteBuffersARB\n");
    fflush(rendererGlLogFile);
    rendererGlDeleteBuffersARBDriver(count, buffers);
}

void RENDERER_GL_API_CALL GL_LogGenBuffersARB(int32_t count, uint32_t *buffers)
{
    fprintf(rendererGlLogFile, "glGenBuffersARB\n");
    fflush(rendererGlLogFile);
    rendererGlGenBuffersARBDriver(count, buffers);
}

uint8_t RENDERER_GL_API_CALL GL_LogIsBufferARB(uint32_t buffer)
{
    fprintf(rendererGlLogFile, "glIsBufferARB\n");
    fflush(rendererGlLogFile);
    return rendererGlIsBufferARBDriver(buffer);
}

/* Source: CoDUOMP.exe 0x004caea0..0x004cb045. These are the remaining ARB
 * buffer queries followed by the first ATI object-buffer operations. The ARB
 * offset/size arguments are native pointer-width API scalars; ATI object-buffer
 * sizes remain GLsizei (signed 32-bit) on every host. */
void RENDERER_GL_API_CALL GL_LogBufferSubDataARB(uint32_t target, intptr_t offset, intptr_t size, const void *data)
{
    fprintf(rendererGlLogFile, "glBufferSubDataARB\n");
    fflush(rendererGlLogFile);
    rendererGlBufferSubDataARBDriver(target, offset, size, data);
}

void RENDERER_GL_API_CALL GL_LogGetBufferSubDataARB(uint32_t target, intptr_t offset, intptr_t size, void *data)
{
    fprintf(rendererGlLogFile, "glGetBufferSubDataARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetBufferSubDataARBDriver(target, offset, size, data);
}

void RENDERER_GL_API_CALL GL_LogGetBufferParameterivARB(uint32_t target, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetBufferParameterivARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetBufferParameterivARBDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetBufferPointervARB(uint32_t target, uint32_t parameter, void **pointer)
{
    fprintf(rendererGlLogFile, "glGetBufferPointervARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetBufferPointervARBDriver(target, parameter, pointer);
}

uint32_t RENDERER_GL_API_CALL GL_LogNewObjectBufferATI(int32_t size, const void *data, uint32_t usage)
{
    fprintf(rendererGlLogFile, "glNewObjectBufferATI\n");
    fflush(rendererGlLogFile);
    return rendererGlNewObjectBufferATIDriver(size, data, usage);
}

uint8_t RENDERER_GL_API_CALL GL_LogIsObjectBufferATI(uint32_t buffer)
{
    fprintf(rendererGlLogFile, "glIsObjectBufferATI\n");
    fflush(rendererGlLogFile);
    return rendererGlIsObjectBufferATIDriver(buffer);
}

void RENDERER_GL_API_CALL GL_LogUpdateObjectBufferATI(uint32_t buffer, uint32_t offset, int32_t size, const void *data,
                                                      uint32_t preserveMode)
{
    fprintf(rendererGlLogFile, "glUpdateObjectBufferATI\n");
    fflush(rendererGlLogFile);
    rendererGlUpdateObjectBufferATIDriver(buffer, offset, size, data, preserveMode);
}

void RENDERER_GL_API_CALL GL_LogGetObjectBufferfvATI(uint32_t buffer, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetObjectBufferfvATI\n");
    fflush(rendererGlLogFile);
    rendererGlGetObjectBufferfvATIDriver(buffer, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetObjectBufferivATI(uint32_t buffer, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetObjectBufferivATI\n");
    fflush(rendererGlLogFile);
    rendererGlGetObjectBufferivATIDriver(buffer, parameter, values);
}

/* Source: CoDUOMP.exe 0x004cb050..0x004cb225. ATI object/element-array logging
 * continues the same name-only, flush, then forward pattern. */
void RENDERER_GL_API_CALL GL_LogFreeObjectBufferATI(uint32_t buffer)
{
    fprintf(rendererGlLogFile, "glFreeObjectBufferATI\n");
    fflush(rendererGlLogFile);
    rendererGlFreeObjectBufferATIDriver(buffer);
}

void RENDERER_GL_API_CALL GL_LogArrayObjectATI(uint32_t array, int32_t size, uint32_t type, int32_t stride, uint32_t buffer,
                                               uint32_t offset)
{
    fprintf(rendererGlLogFile, "glArrayObjectATI\n");
    fflush(rendererGlLogFile);
    rendererGlArrayObjectATIDriver(array, size, type, stride, buffer, offset);
}

void RENDERER_GL_API_CALL GL_LogGetArrayObjectfvATI(uint32_t array, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetArrayObjectfvATI\n");
    fflush(rendererGlLogFile);
    rendererGlGetArrayObjectfvATIDriver(array, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetArrayObjectivATI(uint32_t array, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetArrayObjectivATI\n");
    fflush(rendererGlLogFile);
    rendererGlGetArrayObjectivATIDriver(array, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogVariantArrayObjectATI(uint32_t id, uint32_t type, int32_t stride, uint32_t buffer, uint32_t offset)
{
    fprintf(rendererGlLogFile, "glVariantArrayObjectATI\n");
    fflush(rendererGlLogFile);
    rendererGlVariantArrayObjectATIDriver(id, type, stride, buffer, offset);
}

void RENDERER_GL_API_CALL GL_LogGetVariantArrayObjectfvATI(uint32_t id, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetVariantArrayObjectfvATI\n");
    fflush(rendererGlLogFile);
    rendererGlGetVariantArrayObjectfvATIDriver(id, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetVariantArrayObjectivATI(uint32_t id, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetVariantArrayObjectivATI\n");
    fflush(rendererGlLogFile);
    rendererGlGetVariantArrayObjectivATIDriver(id, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogElementPointerATI(uint32_t type, const void *pointer)
{
    fprintf(rendererGlLogFile, "glElementPointerATI\n");
    fflush(rendererGlLogFile);
    rendererGlElementPointerATIDriver(type, pointer);
}

void RENDERER_GL_API_CALL GL_LogDrawElementArrayATI(uint32_t mode, int32_t count)
{
    fprintf(rendererGlLogFile, "glDrawElementArrayATI\n");
    fflush(rendererGlLogFile);
    rendererGlDrawElementArrayATIDriver(mode, count);
}

void RENDERER_GL_API_CALL GL_LogDrawRangeElementArrayATI(uint32_t mode, uint32_t start, uint32_t end, int32_t count)
{
    fprintf(rendererGlLogFile, "glDrawRangeElementArrayATI\n");
    fflush(rendererGlLogFile);
    rendererGlDrawRangeElementArrayATIDriver(mode, start, end, count);
}

/* Source: CoDUOMP.exe 0x004cb230..0x004cb3d5. NV vertex-array memory and fence
 * wrappers retain their true return and pointer types while logging only the
 * entry-point name. The Windows-specific wgl names are exposed here without
 * making the allocator storage itself platform-specific. */
void RENDERER_GL_API_CALL GL_LogFlushVertexArrayRangeNV(void)
{
    fprintf(rendererGlLogFile, "glFlushVertexArrayRangeNV\n");
    fflush(rendererGlLogFile);
    rendererGlFlushVertexArrayRangeNVDriver();
}

void RENDERER_GL_API_CALL GL_LogVertexArrayRangeNV(int32_t length, const void *pointer)
{
    fprintf(rendererGlLogFile, "glVertexArrayRangeNV\n");
    fflush(rendererGlLogFile);
    rendererGlVertexArrayRangeNVDriver(length, pointer);
}

void *RENDERER_GL_API_CALL GL_LogAllocateMemoryNV(int32_t size, float readFrequency, float writeFrequency, float priority)
{
    fprintf(rendererGlLogFile, "wglAllocateMemoryNV\n");
    fflush(rendererGlLogFile);
    return rendererGlAllocateMemoryNVDriver(size, readFrequency, writeFrequency, priority);
}

void RENDERER_GL_API_CALL GL_LogFreeMemoryNV(void *memory)
{
    fprintf(rendererGlLogFile, "wglFreeMemoryNV\n");
    fflush(rendererGlLogFile);
    rendererGlFreeMemoryNVDriver(memory);
}

void RENDERER_GL_API_CALL GL_LogDeleteFencesNV(int32_t count, const uint32_t *fences)
{
    fprintf(rendererGlLogFile, "glDeleteFencesNV\n");
    fflush(rendererGlLogFile);
    rendererGlDeleteFencesNVDriver(count, fences);
}

void RENDERER_GL_API_CALL GL_LogGenFencesNV(int32_t count, uint32_t *fences)
{
    fprintf(rendererGlLogFile, "glGenFencesNV\n");
    fflush(rendererGlLogFile);
    rendererGlGenFencesNVDriver(count, fences);
}

uint8_t RENDERER_GL_API_CALL GL_LogIsFenceNV(uint32_t fence)
{
    fprintf(rendererGlLogFile, "glIsFenceNV\n");
    fflush(rendererGlLogFile);
    return rendererGlIsFenceNVDriver(fence);
}

uint8_t RENDERER_GL_API_CALL GL_LogTestFenceNV(uint32_t fence)
{
    fprintf(rendererGlLogFile, "glTestFenceNV\n");
    fflush(rendererGlLogFile);
    return rendererGlTestFenceNVDriver(fence);
}

void RENDERER_GL_API_CALL GL_LogGetFenceivNV(uint32_t fence, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetFenceivNV\n");
    fflush(rendererGlLogFile);
    rendererGlGetFenceivNVDriver(fence, parameter, values);
}

/* Source: CoDUOMP.exe 0x004cb3e0..0x004cb585. NV register-combiner wrappers
 * log only their names, then forward their complete typed APIs. */
void RENDERER_GL_API_CALL GL_LogCombinerParameterfvNV(uint32_t parameter, const float *values)
{
    fprintf(rendererGlLogFile, "glCombinerParameterfvNV\n");
    fflush(rendererGlLogFile);
    rendererGlCombinerParameterfvNVDriver(parameter, values);
}

void RENDERER_GL_API_CALL GL_LogCombinerParameterfNV(uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glCombinerParameterfNV\n");
    fflush(rendererGlLogFile);
    rendererGlCombinerParameterfNVDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogCombinerParameterivNV(uint32_t parameter, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glCombinerParameterivNV\n");
    fflush(rendererGlLogFile);
    rendererGlCombinerParameterivNVDriver(parameter, values);
}

void RENDERER_GL_API_CALL GL_LogCombinerParameteriNV(uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glCombinerParameteriNV\n");
    fflush(rendererGlLogFile);
    rendererGlCombinerParameteriNVDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogCombinerInputNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t input, uint32_t mapping,
                                                uint32_t componentUsage)
{
    fprintf(rendererGlLogFile, "glCombinerInputNV\n");
    fflush(rendererGlLogFile);
    rendererGlCombinerInputNVDriver(stage, portion, variable, input, mapping, componentUsage);
}

void RENDERER_GL_API_CALL GL_LogCombinerOutputNV(uint32_t stage, uint32_t portion, uint32_t abOutput, uint32_t cdOutput, uint32_t sumOutput,
                                                 uint32_t scale, uint32_t bias, uint8_t abDotProduct, uint8_t cdDotProduct, uint8_t muxSum)
{
    fprintf(rendererGlLogFile, "glCombinerOutputNV\n");
    fflush(rendererGlLogFile);
    rendererGlCombinerOutputNVDriver(stage, portion, abOutput, cdOutput, sumOutput, scale, bias, abDotProduct, cdDotProduct, muxSum);
}

void RENDERER_GL_API_CALL GL_LogFinalCombinerInputNV(uint32_t variable, uint32_t input, uint32_t mapping, uint32_t componentUsage)
{
    fprintf(rendererGlLogFile, "glFinalCombinerInputNV\n");
    fflush(rendererGlLogFile);
    rendererGlFinalCombinerInputNVDriver(variable, input, mapping, componentUsage);
}

void RENDERER_GL_API_CALL GL_LogGetCombinerInputParameterfvNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
                                                              float *values)
{
    fprintf(rendererGlLogFile, "glGetCombinerInputParameterfvNV\n");
    fflush(rendererGlLogFile);
    rendererGlGetCombinerInputParameterfvNVDriver(stage, portion, variable, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetCombinerInputParameterivNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
                                                              int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetCombinerInputParameterivNV\n");
    fflush(rendererGlLogFile);
    rendererGlGetCombinerInputParameterivNVDriver(stage, portion, variable, parameter, values);
}

/* Source: CoDUOMP.exe 0x004cb590..0x004cb915. The remaining NV register-
 * combiner queries and ATI fragment-shader wrappers log only their names and
 * forward the complete extension APIs. */
void RENDERER_GL_API_CALL GL_LogGetCombinerOutputParameterfvNV(uint32_t stage, uint32_t portion, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetCombinerOutputParameterfvNV\n");
    fflush(rendererGlLogFile);
    rendererGlGetCombinerOutputParameterfvNVDriver(stage, portion, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetCombinerOutputParameterivNV(uint32_t stage, uint32_t portion, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetCombinerOutputParameterivNV\n");
    fflush(rendererGlLogFile);
    rendererGlGetCombinerOutputParameterivNVDriver(stage, portion, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetFinalCombinerInputParameterfvNV(uint32_t variable, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetFinalCombinerInputParameterfvNV\n");
    fflush(rendererGlLogFile);
    rendererGlGetFinalCombinerInputParameterfvNVDriver(variable, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetFinalCombinerInputParameterivNV(uint32_t variable, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetFinalCombinerInputParameterivNV\n");
    fflush(rendererGlLogFile);
    rendererGlGetFinalCombinerInputParameterivNVDriver(variable, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogCombinerStageParameterfvNV(uint32_t stage, uint32_t parameter, const float *values)
{
    fprintf(rendererGlLogFile, "glCombinerStageParameterfvNV\n");
    fflush(rendererGlLogFile);
    rendererGlCombinerStageParameterfvNVDriver(stage, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetCombinerStageParameterfvNV(uint32_t stage, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetCombinerStageParameterfvNV\n");
    fflush(rendererGlLogFile);
    rendererGlGetCombinerStageParameterfvNVDriver(stage, parameter, values);
}

uint32_t RENDERER_GL_API_CALL GL_LogGenFragmentShadersATI(uint32_t range)
{
    fprintf(rendererGlLogFile, "glGenFragmentShadersATI\n");
    fflush(rendererGlLogFile);
    return rendererGlGenFragmentShadersATIDriver(range);
}

void RENDERER_GL_API_CALL GL_LogDeleteFragmentShaderATI(uint32_t shader)
{
    fprintf(rendererGlLogFile, "glDeleteFragmentShaderATI\n");
    fflush(rendererGlLogFile);
    rendererGlDeleteFragmentShaderATIDriver(shader);
}

void RENDERER_GL_API_CALL GL_LogBeginFragmentShaderATI(void)
{
    fprintf(rendererGlLogFile, "glBeginFragmentShaderATI\n");
    fflush(rendererGlLogFile);
    rendererGlBeginFragmentShaderATIDriver();
}

void RENDERER_GL_API_CALL GL_LogEndFragmentShaderATI(void)
{
    fprintf(rendererGlLogFile, "glEndFragmentShaderATI\n");
    fflush(rendererGlLogFile);
    rendererGlEndFragmentShaderATIDriver();
}

void RENDERER_GL_API_CALL GL_LogPassTexCoordATI(uint32_t destination, uint32_t coordinate, uint32_t swizzle)
{
    fprintf(rendererGlLogFile, "glPassTexCoordATI\n");
    fflush(rendererGlLogFile);
    rendererGlPassTexCoordATIDriver(destination, coordinate, swizzle);
}

void RENDERER_GL_API_CALL GL_LogSampleMapATI(uint32_t destination, uint32_t interpolation, uint32_t swizzle)
{
    fprintf(rendererGlLogFile, "glSampleMapATI\n");
    fflush(rendererGlLogFile);
    rendererGlSampleMapATIDriver(destination, interpolation, swizzle);
}

void RENDERER_GL_API_CALL GL_LogColorFragmentOp1ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                    uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                    uint32_t argument1Modifier)
{
    fprintf(rendererGlLogFile, "glColorFragmentOp1ATI\n");
    fflush(rendererGlLogFile);
    rendererGlColorFragmentOp1ATIDriver(operation, destination, destinationMask, destinationModifier, argument1, argument1Replication,
                                        argument1Modifier);
}

void RENDERER_GL_API_CALL GL_LogColorFragmentOp2ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                    uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                    uint32_t argument1Modifier, uint32_t argument2, uint32_t argument2Replication,
                                                    uint32_t argument2Modifier)
{
    fprintf(rendererGlLogFile, "glColorFragmentOp2ATI\n");
    fflush(rendererGlLogFile);
    rendererGlColorFragmentOp2ATIDriver(operation, destination, destinationMask, destinationModifier, argument1, argument1Replication,
                                        argument1Modifier, argument2, argument2Replication, argument2Modifier);
}

void RENDERER_GL_API_CALL GL_LogColorFragmentOp3ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                    uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                    uint32_t argument1Modifier, uint32_t argument2, uint32_t argument2Replication,
                                                    uint32_t argument2Modifier, uint32_t argument3, uint32_t argument3Replication,
                                                    uint32_t argument3Modifier)
{
    fprintf(rendererGlLogFile, "glColorFragmentOp3ATI\n");
    fflush(rendererGlLogFile);
    rendererGlColorFragmentOp3ATIDriver(operation, destination, destinationMask, destinationModifier, argument1, argument1Replication,
                                        argument1Modifier, argument2, argument2Replication, argument2Modifier, argument3,
                                        argument3Replication, argument3Modifier);
}

void RENDERER_GL_API_CALL GL_LogAlphaFragmentOp1ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                    uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier)
{
    fprintf(rendererGlLogFile, "glAlphaFragmentOp1ATI\n");
    fflush(rendererGlLogFile);
    rendererGlAlphaFragmentOp1ATIDriver(operation, destination, destinationModifier, argument1, argument1Replication, argument1Modifier);
}

void RENDERER_GL_API_CALL GL_LogAlphaFragmentOp2ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                    uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier,
                                                    uint32_t argument2, uint32_t argument2Replication, uint32_t argument2Modifier)
{
    fprintf(rendererGlLogFile, "glAlphaFragmentOp2ATI\n");
    fflush(rendererGlLogFile);
    rendererGlAlphaFragmentOp2ATIDriver(operation, destination, destinationModifier, argument1, argument1Replication, argument1Modifier,
                                        argument2, argument2Replication, argument2Modifier);
}

void RENDERER_GL_API_CALL GL_LogAlphaFragmentOp3ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                    uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier,
                                                    uint32_t argument2, uint32_t argument2Replication, uint32_t argument2Modifier,
                                                    uint32_t argument3, uint32_t argument3Replication, uint32_t argument3Modifier)
{
    fprintf(rendererGlLogFile, "glAlphaFragmentOp3ATI\n");
    fflush(rendererGlLogFile);
    rendererGlAlphaFragmentOp3ATIDriver(operation, destination, destinationModifier, argument1, argument1Replication, argument1Modifier,
                                        argument2, argument2Replication, argument2Modifier, argument3, argument3Replication,
                                        argument3Modifier);
}

void RENDERER_GL_API_CALL GL_LogSetFragmentShaderConstantATI(uint32_t destination, const float *value)
{
    fprintf(rendererGlLogFile, "glSetFragmentShaderConstantATI\n");
    fflush(rendererGlLogFile);
    rendererGlSetFragmentShaderConstantATIDriver(destination, value);
}

/* Source: CoDUOMP.exe 0x004cb920..0x004cbbe5. ARB vertex-attribute scalar
 * wrappers log only their names and then forward the exact extension API.
 * The 1d/2d/3d/4d machine bodies copy each 64-bit argument through x87 before
 * the stdcall driver call; the typed double parameters preserve that ABI-level
 * forwarding without embedding 32-bit stack arithmetic in maintained source. */
void RENDERER_GL_API_CALL GL_LogVertexAttrib1sARB(uint32_t index, int16_t x)
{
    fprintf(rendererGlLogFile, "glVertexAttrib1sARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib1sARBDriver(index, x);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib1fARB(uint32_t index, float x)
{
    fprintf(rendererGlLogFile, "glVertexAttrib1fARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib1fARBDriver(index, x);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib1dARB(uint32_t index, double x)
{
    fprintf(rendererGlLogFile, "glVertexAttrib1dARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib1dARBDriver(index, x);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib2sARB(uint32_t index, int16_t x, int16_t y)
{
    fprintf(rendererGlLogFile, "glVertexAttrib2sARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib2sARBDriver(index, x, y);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib2fARB(uint32_t index, float x, float y)
{
    fprintf(rendererGlLogFile, "glVertexAttrib2fARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib2fARBDriver(index, x, y);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib2dARB(uint32_t index, double x, double y)
{
    fprintf(rendererGlLogFile, "glVertexAttrib2dARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib2dARBDriver(index, x, y);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib3sARB(uint32_t index, int16_t x, int16_t y, int16_t z)
{
    fprintf(rendererGlLogFile, "glVertexAttrib3sARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib3sARBDriver(index, x, y, z);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib3fARB(uint32_t index, float x, float y, float z)
{
    fprintf(rendererGlLogFile, "glVertexAttrib3fARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib3fARBDriver(index, x, y, z);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib3dARB(uint32_t index, double x, double y, double z)
{
    fprintf(rendererGlLogFile, "glVertexAttrib3dARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib3dARBDriver(index, x, y, z);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4sARB(uint32_t index, int16_t x, int16_t y, int16_t z, int16_t w)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4sARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4sARBDriver(index, x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4fARB(uint32_t index, float x, float y, float z, float w)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4fARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4fARBDriver(index, x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4dARB(uint32_t index, double x, double y, double z, double w)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4dARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4dARBDriver(index, x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4NubARB(uint32_t index, uint8_t x, uint8_t y, uint8_t z, uint8_t w)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4NubARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4NubARBDriver(index, x, y, z, w);
}

/* Source: CoDUOMP.exe 0x004cbbf0..0x004cc0c5. ARB vertex-attribute vector,
 * array-pointer, and array-toggle wrappers; each body is the same proven
 * log/flush/tail-forward shape with the extension's exact typed signature. */
void RENDERER_GL_API_CALL GL_LogVertexAttrib1svARB(uint32_t index, const int16_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib1svARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib1svARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib1fvARB(uint32_t index, const float *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib1fvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib1fvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib1dvARB(uint32_t index, const double *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib1dvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib1dvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib2svARB(uint32_t index, const int16_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib2svARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib2svARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib2fvARB(uint32_t index, const float *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib2fvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib2fvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib2dvARB(uint32_t index, const double *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib2dvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib2dvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib3svARB(uint32_t index, const int16_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib3svARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib3svARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib3fvARB(uint32_t index, const float *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib3fvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib3fvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib3dvARB(uint32_t index, const double *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib3dvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib3dvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4bvARB(uint32_t index, const int8_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4bvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4bvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4svARB(uint32_t index, const int16_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4svARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4svARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4ivARB(uint32_t index, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4ivARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4ivARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4ubvARB(uint32_t index, const uint8_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4ubvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4ubvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4usvARB(uint32_t index, const uint16_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4usvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4usvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4uivARB(uint32_t index, const uint32_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4uivARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4uivARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4fvARB(uint32_t index, const float *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4fvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4fvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4dvARB(uint32_t index, const double *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4dvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4dvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4NbvARB(uint32_t index, const int8_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4NbvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4NbvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4NsvARB(uint32_t index, const int16_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4NsvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4NsvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4NivARB(uint32_t index, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4NivARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4NivARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4NubvARB(uint32_t index, const uint8_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4NubvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4NubvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4NusvARB(uint32_t index, const uint16_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4NusvARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4NusvARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttrib4NuivARB(uint32_t index, const uint32_t *values)
{
    fprintf(rendererGlLogFile, "glVertexAttrib4NuivARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttrib4NuivARBDriver(index, values);
}

void RENDERER_GL_API_CALL GL_LogVertexAttribPointerARB(uint32_t index, int32_t size, uint32_t type, uint8_t normalized, int32_t stride,
                                                       const void *pointer)
{
    fprintf(rendererGlLogFile, "glVertexAttribPointerARB\n");
    fflush(rendererGlLogFile);
    rendererGlVertexAttribPointerARBDriver(index, size, type, normalized, stride, pointer);
}

void RENDERER_GL_API_CALL GL_LogEnableVertexAttribArrayARB(uint32_t index)
{
    fprintf(rendererGlLogFile, "glEnableVertexAttribArrayARB\n");
    fflush(rendererGlLogFile);
    rendererGlEnableVertexAttribArrayARBDriver(index);
}

void RENDERER_GL_API_CALL GL_LogDisableVertexAttribArrayARB(uint32_t index)
{
    fprintf(rendererGlLogFile, "glDisableVertexAttribArrayARB\n");
    fflush(rendererGlLogFile);
    rendererGlDisableVertexAttribArrayARBDriver(index);
}

/* Source: CoDUOMP.exe 0x004cc0d0..0x004cc545. ARB program creation,
 * parameter, and query wrappers log only their names before forwarding the
 * exact extension APIs. The two scalar-double setters have explicit x87
 * forwarding in the original and clean 40 bytes of stdcall arguments,
 * proving two 32-bit selectors followed by four 64-bit values. */
void RENDERER_GL_API_CALL GL_LogProgramStringARB(uint32_t target, uint32_t format, int32_t length, const void *string)
{
    fprintf(rendererGlLogFile, "glProgramStringARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramStringARBDriver(target, format, length, string);
}

void RENDERER_GL_API_CALL GL_LogDeleteProgramsARB(int32_t count, const uint32_t *programs)
{
    fprintf(rendererGlLogFile, "glDeleteProgramsARB\n");
    fflush(rendererGlLogFile);
    rendererGlDeleteProgramsARBDriver(count, programs);
}

void RENDERER_GL_API_CALL GL_LogGenProgramsARB(int32_t count, uint32_t *programs)
{
    fprintf(rendererGlLogFile, "glGenProgramsARB\n");
    fflush(rendererGlLogFile);
    rendererGlGenProgramsARBDriver(count, programs);
}

void RENDERER_GL_API_CALL GL_LogProgramEnvParameter4fARB(uint32_t target, uint32_t index, float x, float y, float z, float w)
{
    fprintf(rendererGlLogFile, "glProgramEnvParameter4fARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramEnvParameter4fARBDriver(target, index, x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogProgramEnvParameter4dARB(uint32_t target, uint32_t index, double x, double y, double z, double w)
{
    fprintf(rendererGlLogFile, "glProgramEnvParameter4dARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramEnvParameter4dARBDriver(target, index, x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogProgramEnvParameter4fvARB(uint32_t target, uint32_t index, const float *values)
{
    fprintf(rendererGlLogFile, "glProgramEnvParameter4fvARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramEnvParameter4fvARBDriver(target, index, values);
}

void RENDERER_GL_API_CALL GL_LogProgramEnvParameter4dvARB(uint32_t target, uint32_t index, const double *values)
{
    fprintf(rendererGlLogFile, "glProgramEnvParameter4dvARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramEnvParameter4dvARBDriver(target, index, values);
}

void RENDERER_GL_API_CALL GL_LogProgramLocalParameter4fARB(uint32_t target, uint32_t index, float x, float y, float z, float w)
{
    fprintf(rendererGlLogFile, "glProgramLocalParameter4fARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramLocalParameter4fARBDriver(target, index, x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogProgramLocalParameter4dARB(uint32_t target, uint32_t index, double x, double y, double z, double w)
{
    fprintf(rendererGlLogFile, "glProgramLocalParameter4dARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramLocalParameter4dARBDriver(target, index, x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogProgramLocalParameter4fvARB(uint32_t target, uint32_t index, const float *values)
{
    fprintf(rendererGlLogFile, "glProgramLocalParameter4fvARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramLocalParameter4fvARBDriver(target, index, values);
}

void RENDERER_GL_API_CALL GL_LogProgramLocalParameter4dvARB(uint32_t target, uint32_t index, const double *values)
{
    fprintf(rendererGlLogFile, "glProgramLocalParameter4dvARB\n");
    fflush(rendererGlLogFile);
    rendererGlProgramLocalParameter4dvARBDriver(target, index, values);
}

void RENDERER_GL_API_CALL GL_LogGetProgramEnvParameterfvARB(uint32_t target, uint32_t index, float *values)
{
    fprintf(rendererGlLogFile, "glGetProgramEnvParameterfvARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetProgramEnvParameterfvARBDriver(target, index, values);
}

void RENDERER_GL_API_CALL GL_LogGetProgramEnvParameterdvARB(uint32_t target, uint32_t index, double *values)
{
    fprintf(rendererGlLogFile, "glGetProgramEnvParameterdvARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetProgramEnvParameterdvARBDriver(target, index, values);
}

void RENDERER_GL_API_CALL GL_LogGetProgramLocalParameterfvARB(uint32_t target, uint32_t index, float *values)
{
    fprintf(rendererGlLogFile, "glGetProgramLocalParameterfvARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetProgramLocalParameterfvARBDriver(target, index, values);
}

void RENDERER_GL_API_CALL GL_LogGetProgramLocalParameterdvARB(uint32_t target, uint32_t index, double *values)
{
    fprintf(rendererGlLogFile, "glGetProgramLocalParameterdvARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetProgramLocalParameterdvARBDriver(target, index, values);
}

void RENDERER_GL_API_CALL GL_LogGetProgramivARB(uint32_t target, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetProgramivARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetProgramivARBDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetProgramStringARB(uint32_t target, uint32_t parameter, void *string)
{
    fprintf(rendererGlLogFile, "glGetProgramStringARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetProgramStringARBDriver(target, parameter, string);
}

void RENDERER_GL_API_CALL GL_LogGetVertexAttribdvARB(uint32_t index, uint32_t parameter, double *values)
{
    fprintf(rendererGlLogFile, "glGetVertexAttribdvARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetVertexAttribdvARBDriver(index, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetVertexAttribfvARB(uint32_t index, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetVertexAttribfvARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetVertexAttribfvARBDriver(index, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetVertexAttribivARB(uint32_t index, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetVertexAttribivARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetVertexAttribivARBDriver(index, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetVertexAttribPointervARB(uint32_t index, uint32_t parameter, void **pointer)
{
    fprintf(rendererGlLogFile, "glGetVertexAttribPointervARB\n");
    fflush(rendererGlLogFile);
    rendererGlGetVertexAttribPointervARBDriver(index, parameter, pointer);
}

uint8_t RENDERER_GL_API_CALL GL_LogIsProgramARB(uint32_t program)
{
    fprintf(rendererGlLogFile, "glIsProgramARB\n");
    fflush(rendererGlLogFile);
    return rendererGlIsProgramARBDriver(program);
}

/* Source: CoDUOMP.exe 0x004cc550..0x004cc6f5. Core OpenGL accumulation,
 * bitmap/list, clear, and clipping wrappers. Each logs its API name, flushes
 * the log stream, and tail-forwards the complete typed call. */
void RENDERER_GL_API_CALL GL_LogAccum(uint32_t operation, float value)
{
    fprintf(rendererGlLogFile, "glAccum\n");
    fflush(rendererGlLogFile);
    rendererGlAccumDriver(operation, value);
}

uint8_t RENDERER_GL_API_CALL GL_LogAreTexturesResident(int32_t count, const uint32_t *textures, uint8_t *residences)
{
    fprintf(rendererGlLogFile, "glAreTexturesResident\n");
    fflush(rendererGlLogFile);
    return rendererGlAreTexturesResidentDriver(count, textures, residences);
}

void RENDERER_GL_API_CALL GL_LogArrayElement(int32_t index)
{
    fprintf(rendererGlLogFile, "glArrayElement\n");
    fflush(rendererGlLogFile);
    rendererGlArrayElementDriver(index);
}

void RENDERER_GL_API_CALL GL_LogBitmap(int32_t width, int32_t height, float xOrigin, float yOrigin, float xMove, float yMove,
                                       const uint8_t *bitmap)
{
    fprintf(rendererGlLogFile, "glBitmap\n");
    fflush(rendererGlLogFile);
    rendererGlBitmapDriver(width, height, xOrigin, yOrigin, xMove, yMove, bitmap);
}

void RENDERER_GL_API_CALL GL_LogCallLists(int32_t count, uint32_t type, const void *lists)
{
    fprintf(rendererGlLogFile, "glCallLists\n");
    fflush(rendererGlLogFile);
    rendererGlCallListsDriver(count, type, lists);
}

void RENDERER_GL_API_CALL GL_LogClearAccum(float red, float green, float blue, float alpha)
{
    fprintf(rendererGlLogFile, "glClearAccum\n");
    fflush(rendererGlLogFile);
    rendererGlClearAccumDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogClearColor(float red, float green, float blue, float alpha)
{
    fprintf(rendererGlLogFile, "glClearColor\n");
    fflush(rendererGlLogFile);
    rendererGlClearColorDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogClearIndex(float index)
{
    fprintf(rendererGlLogFile, "glClearIndex\n");
    fflush(rendererGlLogFile);
    rendererGlClearIndexDriver(index);
}

void RENDERER_GL_API_CALL GL_LogClipPlane(uint32_t plane, const double *equation)
{
    fprintf(rendererGlLogFile, "glClipPlane\n");
    fflush(rendererGlLogFile);
    rendererGlClipPlaneDriver(plane, equation);
}

/* Source: CoDUOMP.exe 0x004cc700..0x004cca05. Complete core OpenGL Color3
 * scalar/vector family. glColor3d's x87 forwarding and `ret 0x18` prove three
 * 64-bit scalar arguments; all other variants tail-forward their typed APIs. */
void RENDERER_GL_API_CALL GL_LogColor3b(int8_t red, int8_t green, int8_t blue)
{
    fprintf(rendererGlLogFile, "glColor3b\n");
    fflush(rendererGlLogFile);
    rendererGlColor3bDriver(red, green, blue);
}

void RENDERER_GL_API_CALL GL_LogColor3bv(const int8_t *values)
{
    fprintf(rendererGlLogFile, "glColor3bv\n");
    fflush(rendererGlLogFile);
    rendererGlColor3bvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor3d(double red, double green, double blue)
{
    fprintf(rendererGlLogFile, "glColor3d\n");
    fflush(rendererGlLogFile);
    rendererGlColor3dDriver(red, green, blue);
}

void RENDERER_GL_API_CALL GL_LogColor3dv(const double *values)
{
    fprintf(rendererGlLogFile, "glColor3dv\n");
    fflush(rendererGlLogFile);
    rendererGlColor3dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor3f(float red, float green, float blue)
{
    fprintf(rendererGlLogFile, "glColor3f\n");
    fflush(rendererGlLogFile);
    rendererGlColor3fDriver(red, green, blue);
}

void RENDERER_GL_API_CALL GL_LogColor3fv(const float *values)
{
    fprintf(rendererGlLogFile, "glColor3fv\n");
    fflush(rendererGlLogFile);
    rendererGlColor3fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor3i(int32_t red, int32_t green, int32_t blue)
{
    fprintf(rendererGlLogFile, "glColor3i\n");
    fflush(rendererGlLogFile);
    rendererGlColor3iDriver(red, green, blue);
}

void RENDERER_GL_API_CALL GL_LogColor3iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glColor3iv\n");
    fflush(rendererGlLogFile);
    rendererGlColor3ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor3s(int16_t red, int16_t green, int16_t blue)
{
    fprintf(rendererGlLogFile, "glColor3s\n");
    fflush(rendererGlLogFile);
    rendererGlColor3sDriver(red, green, blue);
}

void RENDERER_GL_API_CALL GL_LogColor3sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glColor3sv\n");
    fflush(rendererGlLogFile);
    rendererGlColor3svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor3ub(uint8_t red, uint8_t green, uint8_t blue)
{
    fprintf(rendererGlLogFile, "glColor3ub\n");
    fflush(rendererGlLogFile);
    rendererGlColor3ubDriver(red, green, blue);
}

void RENDERER_GL_API_CALL GL_LogColor3ubv(const uint8_t *values)
{
    fprintf(rendererGlLogFile, "glColor3ubv\n");
    fflush(rendererGlLogFile);
    rendererGlColor3ubvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor3ui(uint32_t red, uint32_t green, uint32_t blue)
{
    fprintf(rendererGlLogFile, "glColor3ui\n");
    fflush(rendererGlLogFile);
    rendererGlColor3uiDriver(red, green, blue);
}

void RENDERER_GL_API_CALL GL_LogColor3uiv(const uint32_t *values)
{
    fprintf(rendererGlLogFile, "glColor3uiv\n");
    fflush(rendererGlLogFile);
    rendererGlColor3uivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor3us(uint16_t red, uint16_t green, uint16_t blue)
{
    fprintf(rendererGlLogFile, "glColor3us\n");
    fflush(rendererGlLogFile);
    rendererGlColor3usDriver(red, green, blue);
}

void RENDERER_GL_API_CALL GL_LogColor3usv(const uint16_t *values)
{
    fprintf(rendererGlLogFile, "glColor3usv\n");
    fflush(rendererGlLogFile);
    rendererGlColor3usvDriver(values);
}

/* Source: CoDUOMP.exe 0x004cca10..0x004cccc5. Remaining core OpenGL Color4
 * scalar/vector family; Color4f and Color4fv are recovered with the earlier
 * argument-aware wrappers. GL_LogColor4d's x87 forwarding and `ret 0x20`
 * prove four 64-bit scalar arguments. Every entry logs, flushes, and forwards
 * the complete typed OpenGL call. */
void RENDERER_GL_API_CALL GL_LogColor4b(int8_t red, int8_t green, int8_t blue, int8_t alpha)
{
    fprintf(rendererGlLogFile, "glColor4b\n");
    fflush(rendererGlLogFile);
    rendererGlColor4bDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogColor4bv(const int8_t *values)
{
    fprintf(rendererGlLogFile, "glColor4bv\n");
    fflush(rendererGlLogFile);
    rendererGlColor4bvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor4d(double red, double green, double blue, double alpha)
{
    fprintf(rendererGlLogFile, "glColor4d\n");
    fflush(rendererGlLogFile);
    rendererGlColor4dDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogColor4dv(const double *values)
{
    fprintf(rendererGlLogFile, "glColor4dv\n");
    fflush(rendererGlLogFile);
    rendererGlColor4dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor4i(int32_t red, int32_t green, int32_t blue, int32_t alpha)
{
    fprintf(rendererGlLogFile, "glColor4i\n");
    fflush(rendererGlLogFile);
    rendererGlColor4iDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogColor4iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glColor4iv\n");
    fflush(rendererGlLogFile);
    rendererGlColor4ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor4s(int16_t red, int16_t green, int16_t blue, int16_t alpha)
{
    fprintf(rendererGlLogFile, "glColor4s\n");
    fflush(rendererGlLogFile);
    rendererGlColor4sDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogColor4sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glColor4sv\n");
    fflush(rendererGlLogFile);
    rendererGlColor4svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor4ub(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    fprintf(rendererGlLogFile, "glColor4ub\n");
    fflush(rendererGlLogFile);
    rendererGlColor4ubDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogColor4ubv(const uint8_t *values)
{
    fprintf(rendererGlLogFile, "glColor4ubv\n");
    fflush(rendererGlLogFile);
    rendererGlColor4ubvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor4ui(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha)
{
    fprintf(rendererGlLogFile, "glColor4ui\n");
    fflush(rendererGlLogFile);
    rendererGlColor4uiDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogColor4uiv(const uint32_t *values)
{
    fprintf(rendererGlLogFile, "glColor4uiv\n");
    fflush(rendererGlLogFile);
    rendererGlColor4uivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogColor4us(uint16_t red, uint16_t green, uint16_t blue, uint16_t alpha)
{
    fprintf(rendererGlLogFile, "glColor4us\n");
    fflush(rendererGlLogFile);
    rendererGlColor4usDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogColor4usv(const uint16_t *values)
{
    fprintf(rendererGlLogFile, "glColor4usv\n");
    fflush(rendererGlLogFile);
    rendererGlColor4usvDriver(values);
}

/* Source: CoDUOMP.exe 0x004cccd0..0x004cd245. Core OpenGL name-only logging
 * wrappers from the secondary QGL dispatch set. These are distinct from the
 * earlier argument-formatting wrappers: for example, 0x004cceb0 and
 * 0x004ca490 are separate glDrawBuffer loggers installed into separate
 * dispatch slots. Every entry here logs only the API name, flushes, and
 * forwards the complete typed call. */
void RENDERER_GL_API_CALL GL_LogColorMask(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    fprintf(rendererGlLogFile, "glColorMask\n");
    fflush(rendererGlLogFile);
    rendererGlColorMaskDriver(red, green, blue, alpha);
}

void RENDERER_GL_API_CALL GL_LogColorMaterial(uint32_t face, uint32_t mode)
{
    fprintf(rendererGlLogFile, "glColorMaterial\n");
    fflush(rendererGlLogFile);
    rendererGlColorMaterialDriver(face, mode);
}

void RENDERER_GL_API_CALL GL_LogCopyPixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t type)
{
    fprintf(rendererGlLogFile, "glCopyPixels\n");
    fflush(rendererGlLogFile);
    rendererGlCopyPixelsDriver(x, y, width, height, type);
}

void RENDERER_GL_API_CALL GL_LogCopyTexImage1D(uint32_t target, int32_t level, uint32_t internalFormat, int32_t x, int32_t y, int32_t width,
                                               int32_t border)
{
    fprintf(rendererGlLogFile, "glCopyTexImage1D\n");
    fflush(rendererGlLogFile);
    rendererGlCopyTexImage1DDriver(target, level, internalFormat, x, y, width, border);
}

void RENDERER_GL_API_CALL GL_LogCopyTexImage2D(uint32_t target, int32_t level, uint32_t internalFormat, int32_t x, int32_t y, int32_t width,
                                               int32_t height, int32_t border)
{
    fprintf(rendererGlLogFile, "glCopyTexImage2D\n");
    fflush(rendererGlLogFile);
    rendererGlCopyTexImage2DDriver(target, level, internalFormat, x, y, width, height, border);
}

void RENDERER_GL_API_CALL GL_LogCopyTexSubImage1D(uint32_t target, int32_t level, int32_t xOffset, int32_t x, int32_t y, int32_t width)
{
    fprintf(rendererGlLogFile, "glCopyTexSubImage1D\n");
    fflush(rendererGlLogFile);
    rendererGlCopyTexSubImage1DDriver(target, level, xOffset, x, y, width);
}

void RENDERER_GL_API_CALL GL_LogCopyTexSubImage2D(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset, int32_t x, int32_t y,
                                                  int32_t width, int32_t height)
{
    fprintf(rendererGlLogFile, "glCopyTexSubImage2D\n");
    fflush(rendererGlLogFile);
    rendererGlCopyTexSubImage2DDriver(target, level, xOffset, yOffset, x, y, width, height);
}

void RENDERER_GL_API_CALL GL_LogDeleteLists(uint32_t list, int32_t range)
{
    fprintf(rendererGlLogFile, "glDeleteLists\n");
    fflush(rendererGlLogFile);
    rendererGlDeleteListsDriver(list, range);
}

void RENDERER_GL_API_CALL GL_LogDeleteTextures(int32_t count, const uint32_t *textures)
{
    fprintf(rendererGlLogFile, "glDeleteTextures\n");
    fflush(rendererGlLogFile);
    rendererGlDeleteTexturesDriver(count, textures);
}

void RENDERER_GL_API_CALL GL_LogDrawArrays(uint32_t mode, int32_t first, int32_t count)
{
    fprintf(rendererGlLogFile, "glDrawArrays\n");
    fflush(rendererGlLogFile);
    rendererGlDrawArraysDriver(mode, first, count);
}

/* Source: CoDUOMP.exe 0x004cceb0..0x004cced5. The original generic logger
 * prints only the entry-point name and deliberately omits the buffer value. */
void RENDERER_GL_API_CALL GL_LogDrawBuffer(uint32_t buffer)
{
    fprintf(rendererGlLogFile, "glDrawBuffer\n");
    fflush(rendererGlLogFile);
    rendererGlDrawBufferDriver(buffer);
}

void RENDERER_GL_API_CALL GL_LogDrawPixels(int32_t width, int32_t height, uint32_t format, uint32_t type, const void *pixels)
{
    fprintf(rendererGlLogFile, "glDrawPixels\n");
    fflush(rendererGlLogFile);
    rendererGlDrawPixelsDriver(width, height, format, type, pixels);
}

void RENDERER_GL_API_CALL GL_LogEdgeFlag(uint8_t flag)
{
    fprintf(rendererGlLogFile, "glEdgeFlag\n");
    fflush(rendererGlLogFile);
    rendererGlEdgeFlagDriver(flag);
}

void RENDERER_GL_API_CALL GL_LogEdgeFlagPointer(int32_t stride, const void *pointer)
{
    fprintf(rendererGlLogFile, "glEdgeFlagPointer\n");
    fflush(rendererGlLogFile);
    rendererGlEdgeFlagPointerDriver(stride, pointer);
}

void RENDERER_GL_API_CALL GL_LogEdgeFlagv(const uint8_t *flag)
{
    fprintf(rendererGlLogFile, "glEdgeFlagv\n");
    fflush(rendererGlLogFile);
    rendererGlEdgeFlagvDriver(flag);
}

void RENDERER_GL_API_CALL GL_LogEnd(void)
{
    fprintf(rendererGlLogFile, "glEnd\n");
    fflush(rendererGlLogFile);
    rendererGlEndDriver();
}

void RENDERER_GL_API_CALL GL_LogEndList(void)
{
    fprintf(rendererGlLogFile, "glEndList\n");
    fflush(rendererGlLogFile);
    rendererGlEndListDriver();
}

void RENDERER_GL_API_CALL GL_LogEvalCoord1d(double u)
{
    fprintf(rendererGlLogFile, "glEvalCoord1d\n");
    fflush(rendererGlLogFile);
    rendererGlEvalCoord1dDriver(u);
}

void RENDERER_GL_API_CALL GL_LogEvalCoord1dv(const double *u)
{
    fprintf(rendererGlLogFile, "glEvalCoord1dv\n");
    fflush(rendererGlLogFile);
    rendererGlEvalCoord1dvDriver(u);
}

void RENDERER_GL_API_CALL GL_LogEvalCoord1f(float u)
{
    fprintf(rendererGlLogFile, "glEvalCoord1f\n");
    fflush(rendererGlLogFile);
    rendererGlEvalCoord1fDriver(u);
}

void RENDERER_GL_API_CALL GL_LogEvalCoord1fv(const float *u)
{
    fprintf(rendererGlLogFile, "glEvalCoord1fv\n");
    fflush(rendererGlLogFile);
    rendererGlEvalCoord1fvDriver(u);
}

void RENDERER_GL_API_CALL GL_LogEvalCoord2d(double u, double v)
{
    fprintf(rendererGlLogFile, "glEvalCoord2d\n");
    fflush(rendererGlLogFile);
    rendererGlEvalCoord2dDriver(u, v);
}

void RENDERER_GL_API_CALL GL_LogEvalCoord2dv(const double *values)
{
    fprintf(rendererGlLogFile, "glEvalCoord2dv\n");
    fflush(rendererGlLogFile);
    rendererGlEvalCoord2dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogEvalCoord2f(float u, float v)
{
    fprintf(rendererGlLogFile, "glEvalCoord2f\n");
    fflush(rendererGlLogFile);
    rendererGlEvalCoord2fDriver(u, v);
}

void RENDERER_GL_API_CALL GL_LogEvalCoord2fv(const float *values)
{
    fprintf(rendererGlLogFile, "glEvalCoord2fv\n");
    fflush(rendererGlLogFile);
    rendererGlEvalCoord2fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogEvalMesh1(uint32_t mode, int32_t i1, int32_t i2)
{
    fprintf(rendererGlLogFile, "glEvalMesh1\n");
    fflush(rendererGlLogFile);
    rendererGlEvalMesh1Driver(mode, i1, i2);
}

void RENDERER_GL_API_CALL GL_LogEvalMesh2(uint32_t mode, int32_t i1, int32_t i2, int32_t j1, int32_t j2)
{
    fprintf(rendererGlLogFile, "glEvalMesh2\n");
    fflush(rendererGlLogFile);
    rendererGlEvalMesh2Driver(mode, i1, i2, j1, j2);
}

void RENDERER_GL_API_CALL GL_LogEvalPoint1(int32_t i)
{
    fprintf(rendererGlLogFile, "glEvalPoint1\n");
    fflush(rendererGlLogFile);
    rendererGlEvalPoint1Driver(i);
}

void RENDERER_GL_API_CALL GL_LogEvalPoint2(int32_t i, int32_t j)
{
    fprintf(rendererGlLogFile, "glEvalPoint2\n");
    fflush(rendererGlLogFile);
    rendererGlEvalPoint2Driver(i, j);
}

/* Source: CoDUOMP.exe 0x004cd250..0x004cd7e5. Continuation of the core
 * OpenGL name-only dispatch logger. GL_LogFrustum's six x87 loads and
 * `ret 0x30` prove six double arguments; GL_LogGenLists and GL_LogGetError
 * preserve their OpenGL return values. */
void RENDERER_GL_API_CALL GL_LogFeedbackBuffer(int32_t size, uint32_t type, float *buffer)
{
    fprintf(rendererGlLogFile, "glFeedbackBuffer\n");
    fflush(rendererGlLogFile);
    rendererGlFeedbackBufferDriver(size, type, buffer);
}

void RENDERER_GL_API_CALL GL_LogFinish(void)
{
    fprintf(rendererGlLogFile, "glFinish\n");
    fflush(rendererGlLogFile);
    rendererGlFinishDriver();
}

void RENDERER_GL_API_CALL GL_LogFlush(void)
{
    fprintf(rendererGlLogFile, "glFlush\n");
    fflush(rendererGlLogFile);
    rendererGlFlushDriver();
}

void RENDERER_GL_API_CALL GL_LogFogiv(uint32_t parameter, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glFogiv\n");
    fflush(rendererGlLogFile);
    rendererGlFogivDriver(parameter, values);
}

void RENDERER_GL_API_CALL GL_LogFrontFace(uint32_t mode)
{
    fprintf(rendererGlLogFile, "glFrontFace\n");
    fflush(rendererGlLogFile);
    rendererGlFrontFaceDriver(mode);
}

void RENDERER_GL_API_CALL GL_LogFrustum(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    fprintf(rendererGlLogFile, "glFrustum\n");
    fflush(rendererGlLogFile);
    rendererGlFrustumDriver(left, right, bottom, top, nearValue, farValue);
}

uint32_t RENDERER_GL_API_CALL GL_LogGenLists(int32_t range)
{
    fprintf(rendererGlLogFile, "glGenLists\n");
    fflush(rendererGlLogFile);
    return rendererGlGenListsDriver(range);
}

void RENDERER_GL_API_CALL GL_LogGenTextures(int32_t count, uint32_t *textures)
{
    fprintf(rendererGlLogFile, "glGenTextures\n");
    fflush(rendererGlLogFile);
    rendererGlGenTexturesDriver(count, textures);
}

void RENDERER_GL_API_CALL GL_LogGetBooleanv(uint32_t parameter, uint8_t *values)
{
    fprintf(rendererGlLogFile, "glGetBooleanv\n");
    fflush(rendererGlLogFile);
    rendererGlGetBooleanvDriver(parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetClipPlane(uint32_t plane, double *equation)
{
    fprintf(rendererGlLogFile, "glGetClipPlane\n");
    fflush(rendererGlLogFile);
    rendererGlGetClipPlaneDriver(plane, equation);
}

void RENDERER_GL_API_CALL GL_LogGetDoublev(uint32_t parameter, double *values)
{
    fprintf(rendererGlLogFile, "glGetDoublev\n");
    fflush(rendererGlLogFile);
    rendererGlGetDoublevDriver(parameter, values);
}

uint32_t RENDERER_GL_API_CALL GL_LogGetError(void)
{
    fprintf(rendererGlLogFile, "glGetError\n");
    fflush(rendererGlLogFile);
    return rendererGlGetErrorDriver();
}

void RENDERER_GL_API_CALL GL_LogGetFloatv(uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetFloatv\n");
    fflush(rendererGlLogFile);
    rendererGlGetFloatvDriver(parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetIntegerv(uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetIntegerv\n");
    fflush(rendererGlLogFile);
    rendererGlGetIntegervDriver(parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetLightfv(uint32_t light, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetLightfv\n");
    fflush(rendererGlLogFile);
    rendererGlGetLightfvDriver(light, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetLightiv(uint32_t light, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetLightiv\n");
    fflush(rendererGlLogFile);
    rendererGlGetLightivDriver(light, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetMapdv(uint32_t target, uint32_t query, double *values)
{
    fprintf(rendererGlLogFile, "glGetMapdv\n");
    fflush(rendererGlLogFile);
    rendererGlGetMapdvDriver(target, query, values);
}

void RENDERER_GL_API_CALL GL_LogGetMapfv(uint32_t target, uint32_t query, float *values)
{
    fprintf(rendererGlLogFile, "glGetMapfv\n");
    fflush(rendererGlLogFile);
    rendererGlGetMapfvDriver(target, query, values);
}

void RENDERER_GL_API_CALL GL_LogGetMapiv(uint32_t target, uint32_t query, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetMapiv\n");
    fflush(rendererGlLogFile);
    rendererGlGetMapivDriver(target, query, values);
}

void RENDERER_GL_API_CALL GL_LogGetMaterialfv(uint32_t face, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetMaterialfv\n");
    fflush(rendererGlLogFile);
    rendererGlGetMaterialfvDriver(face, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetMaterialiv(uint32_t face, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetMaterialiv\n");
    fflush(rendererGlLogFile);
    rendererGlGetMaterialivDriver(face, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetPixelMapfv(uint32_t map, float *values)
{
    fprintf(rendererGlLogFile, "glGetPixelMapfv\n");
    fflush(rendererGlLogFile);
    rendererGlGetPixelMapfvDriver(map, values);
}

void RENDERER_GL_API_CALL GL_LogGetPixelMapuiv(uint32_t map, uint32_t *values)
{
    fprintf(rendererGlLogFile, "glGetPixelMapuiv\n");
    fflush(rendererGlLogFile);
    rendererGlGetPixelMapuivDriver(map, values);
}

void RENDERER_GL_API_CALL GL_LogGetPixelMapusv(uint32_t map, uint16_t *values)
{
    fprintf(rendererGlLogFile, "glGetPixelMapusv\n");
    fflush(rendererGlLogFile);
    rendererGlGetPixelMapusvDriver(map, values);
}

void RENDERER_GL_API_CALL GL_LogGetPointerv(uint32_t parameter, void **value)
{
    fprintf(rendererGlLogFile, "glGetPointerv\n");
    fflush(rendererGlLogFile);
    rendererGlGetPointervDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogGetPolygonStipple(uint8_t *mask)
{
    fprintf(rendererGlLogFile, "glGetPolygonStipple\n");
    fflush(rendererGlLogFile);
    rendererGlGetPolygonStippleDriver(mask);
}

const uint8_t *RENDERER_GL_API_CALL GL_LogGetString(uint32_t name)
{
    fprintf(rendererGlLogFile, "glGetString\n");
    fflush(rendererGlLogFile);
    return rendererGlGetStringDriver(name);
}

void RENDERER_GL_API_CALL GL_LogGetTexEnvfv(uint32_t target, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetTexEnvfv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexEnvfvDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetTexEnviv(uint32_t target, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetTexEnviv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexEnvivDriver(target, parameter, values);
}

/* Source: CoDUOMP.exe 0x004cd7f0..0x004cdd55. Continuation of the core
 * OpenGL name-only dispatch logger. GL_LogIndexd's x87 load and `ret 0x08`
 * prove the scalar double ABI. The Is* wrappers return the driver's original
 * GLboolean byte. */
void RENDERER_GL_API_CALL GL_LogGetTexGendv(uint32_t coordinate, uint32_t parameter, double *values)
{
    fprintf(rendererGlLogFile, "glGetTexGendv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexGendvDriver(coordinate, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetTexGenfv(uint32_t coordinate, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetTexGenfv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexGenfvDriver(coordinate, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetTexGeniv(uint32_t coordinate, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetTexGeniv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexGenivDriver(coordinate, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetTexImage(uint32_t target, int32_t level, uint32_t format, uint32_t type, void *pixels)
{
    fprintf(rendererGlLogFile, "glGetTexImage\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexImageDriver(target, level, format, type, pixels);
}

void RENDERER_GL_API_CALL GL_LogGetTexLevelParameterfv(uint32_t target, int32_t level, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetTexLevelParameterfv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexLevelParameterfvDriver(target, level, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetTexLevelParameteriv(uint32_t target, int32_t level, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetTexLevelParameteriv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexLevelParameterivDriver(target, level, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetTexParameterfv(uint32_t target, uint32_t parameter, float *values)
{
    fprintf(rendererGlLogFile, "glGetTexParameterfv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexParameterfvDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogGetTexParameteriv(uint32_t target, uint32_t parameter, int32_t *values)
{
    fprintf(rendererGlLogFile, "glGetTexParameteriv\n");
    fflush(rendererGlLogFile);
    rendererGlGetTexParameterivDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogIndexMask(uint32_t mask)
{
    fprintf(rendererGlLogFile, "glIndexMask\n");
    fflush(rendererGlLogFile);
    rendererGlIndexMaskDriver(mask);
}

void RENDERER_GL_API_CALL GL_LogIndexPointer(uint32_t type, int32_t stride, const void *pointer)
{
    fprintf(rendererGlLogFile, "glIndexPointer\n");
    fflush(rendererGlLogFile);
    rendererGlIndexPointerDriver(type, stride, pointer);
}

void RENDERER_GL_API_CALL GL_LogIndexd(double index)
{
    fprintf(rendererGlLogFile, "glIndexd\n");
    fflush(rendererGlLogFile);
    rendererGlIndexdDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexdv(const double *index)
{
    fprintf(rendererGlLogFile, "glIndexdv\n");
    fflush(rendererGlLogFile);
    rendererGlIndexdvDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexf(float index)
{
    fprintf(rendererGlLogFile, "glIndexf\n");
    fflush(rendererGlLogFile);
    rendererGlIndexfDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexfv(const float *index)
{
    fprintf(rendererGlLogFile, "glIndexfv\n");
    fflush(rendererGlLogFile);
    rendererGlIndexfvDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexi(int32_t index)
{
    fprintf(rendererGlLogFile, "glIndexi\n");
    fflush(rendererGlLogFile);
    rendererGlIndexiDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexiv(const int32_t *index)
{
    fprintf(rendererGlLogFile, "glIndexiv\n");
    fflush(rendererGlLogFile);
    rendererGlIndexivDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexs(int16_t index)
{
    fprintf(rendererGlLogFile, "glIndexs\n");
    fflush(rendererGlLogFile);
    rendererGlIndexsDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexsv(const int16_t *index)
{
    fprintf(rendererGlLogFile, "glIndexsv\n");
    fflush(rendererGlLogFile);
    rendererGlIndexsvDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexub(uint8_t index)
{
    fprintf(rendererGlLogFile, "glIndexub\n");
    fflush(rendererGlLogFile);
    rendererGlIndexubDriver(index);
}

void RENDERER_GL_API_CALL GL_LogIndexubv(const uint8_t *index)
{
    fprintf(rendererGlLogFile, "glIndexubv\n");
    fflush(rendererGlLogFile);
    rendererGlIndexubvDriver(index);
}

void RENDERER_GL_API_CALL GL_LogInitNames(void)
{
    fprintf(rendererGlLogFile, "glInitNames\n");
    fflush(rendererGlLogFile);
    rendererGlInitNamesDriver();
}

void RENDERER_GL_API_CALL GL_LogInterleavedArrays(uint32_t format, int32_t stride, const void *pointer)
{
    fprintf(rendererGlLogFile, "glInterleavedArrays\n");
    fflush(rendererGlLogFile);
    rendererGlInterleavedArraysDriver(format, stride, pointer);
}

uint8_t RENDERER_GL_API_CALL GL_LogIsEnabled(uint32_t capability)
{
    fprintf(rendererGlLogFile, "glIsEnabled\n");
    fflush(rendererGlLogFile);
    return rendererGlIsEnabledDriver(capability);
}

uint8_t RENDERER_GL_API_CALL GL_LogIsList(uint32_t list)
{
    fprintf(rendererGlLogFile, "glIsList\n");
    fflush(rendererGlLogFile);
    return rendererGlIsListDriver(list);
}

uint8_t RENDERER_GL_API_CALL GL_LogIsTexture(uint32_t texture)
{
    fprintf(rendererGlLogFile, "glIsTexture\n");
    fflush(rendererGlLogFile);
    return rendererGlIsTextureDriver(texture);
}

void RENDERER_GL_API_CALL GL_LogLightModelf(uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glLightModelf\n");
    fflush(rendererGlLogFile);
    rendererGlLightModelfDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogLightModelfv(uint32_t parameter, const float *values)
{
    fprintf(rendererGlLogFile, "glLightModelfv\n");
    fflush(rendererGlLogFile);
    rendererGlLightModelfvDriver(parameter, values);
}

void RENDERER_GL_API_CALL GL_LogLightModeli(uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glLightModeli\n");
    fflush(rendererGlLogFile);
    rendererGlLightModeliDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogLightModeliv(uint32_t parameter, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glLightModeliv\n");
    fflush(rendererGlLogFile);
    rendererGlLightModelivDriver(parameter, values);
}

/* Source: CoDUOMP.exe 0x004cdd60..0x004ce365. Core fixed-function QGL
 * name-only wrappers. The x87 forwarding and stdcall stack cleanup prove the
 * complete Map1d, Map2d, MapGrid1d, and MapGrid2d double-argument layouts. */
void RENDERER_GL_API_CALL GL_LogLightf(uint32_t light, uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glLightf\n");
    fflush(rendererGlLogFile);
    rendererGlLightfDriver(light, parameter, value);
}

void RENDERER_GL_API_CALL GL_LogLightfv(uint32_t light, uint32_t parameter, const float *values)
{
    fprintf(rendererGlLogFile, "glLightfv\n");
    fflush(rendererGlLogFile);
    rendererGlLightfvDriver(light, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogLighti(uint32_t light, uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glLighti\n");
    fflush(rendererGlLogFile);
    rendererGlLightiDriver(light, parameter, value);
}

void RENDERER_GL_API_CALL GL_LogLightiv(uint32_t light, uint32_t parameter, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glLightiv\n");
    fflush(rendererGlLogFile);
    rendererGlLightivDriver(light, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogLineStipple(int32_t factor, uint16_t pattern)
{
    fprintf(rendererGlLogFile, "glLineStipple\n");
    fflush(rendererGlLogFile);
    rendererGlLineStippleDriver(factor, pattern);
}

void RENDERER_GL_API_CALL GL_LogLineWidth(float width)
{
    fprintf(rendererGlLogFile, "glLineWidth\n");
    fflush(rendererGlLogFile);
    rendererGlLineWidthDriver(width);
}

void RENDERER_GL_API_CALL GL_LogListBase(uint32_t base)
{
    fprintf(rendererGlLogFile, "glListBase\n");
    fflush(rendererGlLogFile);
    rendererGlListBaseDriver(base);
}

void RENDERER_GL_API_CALL GL_LogLoadIdentity(void)
{
    fprintf(rendererGlLogFile, "glLoadIdentity\n");
    fflush(rendererGlLogFile);
    rendererGlLoadIdentityDriver();
}

void RENDERER_GL_API_CALL GL_LogLoadMatrixd(const double *matrix)
{
    fprintf(rendererGlLogFile, "glLoadMatrixd\n");
    fflush(rendererGlLogFile);
    rendererGlLoadMatrixdDriver(matrix);
}

void RENDERER_GL_API_CALL GL_LogLoadMatrixf(const float *matrix)
{
    fprintf(rendererGlLogFile, "glLoadMatrixf\n");
    fflush(rendererGlLogFile);
    rendererGlLoadMatrixfDriver(matrix);
}

void RENDERER_GL_API_CALL GL_LogLoadName(uint32_t name)
{
    fprintf(rendererGlLogFile, "glLoadName\n");
    fflush(rendererGlLogFile);
    rendererGlLoadNameDriver(name);
}

void RENDERER_GL_API_CALL GL_LogLogicOp(uint32_t operation)
{
    fprintf(rendererGlLogFile, "glLogicOp\n");
    fflush(rendererGlLogFile);
    rendererGlLogicOpDriver(operation);
}

void RENDERER_GL_API_CALL GL_LogMap1d(uint32_t target, double u1, double u2, int32_t stride, int32_t order, const double *points)
{
    fprintf(rendererGlLogFile, "glMap1d\n");
    fflush(rendererGlLogFile);
    rendererGlMap1dDriver(target, u1, u2, stride, order, points);
}

void RENDERER_GL_API_CALL GL_LogMap1f(uint32_t target, float u1, float u2, int32_t stride, int32_t order, const float *points)
{
    fprintf(rendererGlLogFile, "glMap1f\n");
    fflush(rendererGlLogFile);
    rendererGlMap1fDriver(target, u1, u2, stride, order, points);
}

void RENDERER_GL_API_CALL GL_LogMap2d(uint32_t target, double u1, double u2, int32_t uStride, int32_t uOrder, double v1, double v2,
                                      int32_t vStride, int32_t vOrder, const double *points)
{
    fprintf(rendererGlLogFile, "glMap2d\n");
    fflush(rendererGlLogFile);
    rendererGlMap2dDriver(target, u1, u2, uStride, uOrder, v1, v2, vStride, vOrder, points);
}

void RENDERER_GL_API_CALL GL_LogMap2f(uint32_t target, float u1, float u2, int32_t uStride, int32_t uOrder, float v1, float v2,
                                      int32_t vStride, int32_t vOrder, const float *points)
{
    fprintf(rendererGlLogFile, "glMap2f\n");
    fflush(rendererGlLogFile);
    rendererGlMap2fDriver(target, u1, u2, uStride, uOrder, v1, v2, vStride, vOrder, points);
}

void RENDERER_GL_API_CALL GL_LogMapGrid1d(int32_t count, double u1, double u2)
{
    fprintf(rendererGlLogFile, "glMapGrid1d\n");
    fflush(rendererGlLogFile);
    rendererGlMapGrid1dDriver(count, u1, u2);
}

void RENDERER_GL_API_CALL GL_LogMapGrid1f(int32_t count, float u1, float u2)
{
    fprintf(rendererGlLogFile, "glMapGrid1f\n");
    fflush(rendererGlLogFile);
    rendererGlMapGrid1fDriver(count, u1, u2);
}

void RENDERER_GL_API_CALL GL_LogMapGrid2d(int32_t uCount, double u1, double u2, int32_t vCount, double v1, double v2)
{
    fprintf(rendererGlLogFile, "glMapGrid2d\n");
    fflush(rendererGlLogFile);
    rendererGlMapGrid2dDriver(uCount, u1, u2, vCount, v1, v2);
}

void RENDERER_GL_API_CALL GL_LogMapGrid2f(int32_t uCount, float u1, float u2, int32_t vCount, float v1, float v2)
{
    fprintf(rendererGlLogFile, "glMapGrid2f\n");
    fflush(rendererGlLogFile);
    rendererGlMapGrid2fDriver(uCount, u1, u2, vCount, v1, v2);
}

void RENDERER_GL_API_CALL GL_LogMaterialf(uint32_t face, uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glMaterialf\n");
    fflush(rendererGlLogFile);
    rendererGlMaterialfDriver(face, parameter, value);
}

void RENDERER_GL_API_CALL GL_LogMaterialfv(uint32_t face, uint32_t parameter, const float *values)
{
    fprintf(rendererGlLogFile, "glMaterialfv\n");
    fflush(rendererGlLogFile);
    rendererGlMaterialfvDriver(face, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogMateriali(uint32_t face, uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glMateriali\n");
    fflush(rendererGlLogFile);
    rendererGlMaterialiDriver(face, parameter, value);
}

void RENDERER_GL_API_CALL GL_LogMaterialiv(uint32_t face, uint32_t parameter, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glMaterialiv\n");
    fflush(rendererGlLogFile);
    rendererGlMaterialivDriver(face, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogMatrixMode(uint32_t mode)
{
    fprintf(rendererGlLogFile, "glMatrixMode\n");
    fflush(rendererGlLogFile);
    rendererGlMatrixModeDriver(mode);
}

void RENDERER_GL_API_CALL GL_LogMultMatrixd(const double *matrix)
{
    fprintf(rendererGlLogFile, "glMultMatrixd\n");
    fflush(rendererGlLogFile);
    rendererGlMultMatrixdDriver(matrix);
}

void RENDERER_GL_API_CALL GL_LogMultMatrixf(const float *matrix)
{
    fprintf(rendererGlLogFile, "glMultMatrixf\n");
    fflush(rendererGlLogFile);
    rendererGlMultMatrixfDriver(matrix);
}

void RENDERER_GL_API_CALL GL_LogNewList(uint32_t list, uint32_t mode)
{
    fprintf(rendererGlLogFile, "glNewList\n");
    fflush(rendererGlLogFile);
    rendererGlNewListDriver(list, mode);
}

void RENDERER_GL_API_CALL GL_LogNormal3b(int8_t x, int8_t y, int8_t z)
{
    fprintf(rendererGlLogFile, "glNormal3b\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3bDriver(x, y, z);
}

/* Source: CoDUOMP.exe 0x004ce370..0x004ce915. Core fixed-function QGL
 * logging wrappers from glNormal3bv through glPushClientAttrib. Each function
 * writes its exact .rdata operation name, flushes rendererGlLogFile, and then
 * invokes the corresponding driver entry. GL_LogNormal3d and GL_LogOrtho copy
 * their double arguments through x87 stack slots before the call; the typed
 * calls below preserve those original argument values and ordering. */
void RENDERER_GL_API_CALL GL_LogNormal3bv(const int8_t *values)
{
    fprintf(rendererGlLogFile, "glNormal3bv\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3bvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogNormal3d(double x, double y, double z)
{
    fprintf(rendererGlLogFile, "glNormal3d\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3dDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogNormal3dv(const double *values)
{
    fprintf(rendererGlLogFile, "glNormal3dv\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogNormal3f(float x, float y, float z)
{
    fprintf(rendererGlLogFile, "glNormal3f\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3fDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogNormal3fv(const float *values)
{
    fprintf(rendererGlLogFile, "glNormal3fv\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogNormal3i(int32_t x, int32_t y, int32_t z)
{
    fprintf(rendererGlLogFile, "glNormal3i\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3iDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogNormal3iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glNormal3iv\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogNormal3s(int16_t x, int16_t y, int16_t z)
{
    fprintf(rendererGlLogFile, "glNormal3s\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3sDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogNormal3sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glNormal3sv\n");
    fflush(rendererGlLogFile);
    rendererGlNormal3svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogOrtho(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    fprintf(rendererGlLogFile, "glOrtho\n");
    fflush(rendererGlLogFile);
    rendererGlOrthoDriver(left, right, bottom, top, nearValue, farValue);
}

void RENDERER_GL_API_CALL GL_LogPassThrough(float token)
{
    fprintf(rendererGlLogFile, "glPassThrough\n");
    fflush(rendererGlLogFile);
    rendererGlPassThroughDriver(token);
}

void RENDERER_GL_API_CALL GL_LogPixelMapfv(uint32_t map, int32_t mapSize, const float *values)
{
    fprintf(rendererGlLogFile, "glPixelMapfv\n");
    fflush(rendererGlLogFile);
    rendererGlPixelMapfvDriver(map, mapSize, values);
}

void RENDERER_GL_API_CALL GL_LogPixelMapuiv(uint32_t map, int32_t mapSize, const uint32_t *values)
{
    fprintf(rendererGlLogFile, "glPixelMapuiv\n");
    fflush(rendererGlLogFile);
    rendererGlPixelMapuivDriver(map, mapSize, values);
}

void RENDERER_GL_API_CALL GL_LogPixelMapusv(uint32_t map, int32_t mapSize, const uint16_t *values)
{
    fprintf(rendererGlLogFile, "glPixelMapusv\n");
    fflush(rendererGlLogFile);
    rendererGlPixelMapusvDriver(map, mapSize, values);
}

void RENDERER_GL_API_CALL GL_LogPixelStoref(uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glPixelStoref\n");
    fflush(rendererGlLogFile);
    rendererGlPixelStorefDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogPixelStorei(uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glPixelStorei\n");
    fflush(rendererGlLogFile);
    rendererGlPixelStoreiDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogPixelTransferf(uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glPixelTransferf\n");
    fflush(rendererGlLogFile);
    rendererGlPixelTransferfDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogPixelTransferi(uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glPixelTransferi\n");
    fflush(rendererGlLogFile);
    rendererGlPixelTransferiDriver(parameter, value);
}

void RENDERER_GL_API_CALL GL_LogPixelZoom(float xFactor, float yFactor)
{
    fprintf(rendererGlLogFile, "glPixelZoom\n");
    fflush(rendererGlLogFile);
    rendererGlPixelZoomDriver(xFactor, yFactor);
}

void RENDERER_GL_API_CALL GL_LogPointSize(float size)
{
    fprintf(rendererGlLogFile, "glPointSize\n");
    fflush(rendererGlLogFile);
    rendererGlPointSizeDriver(size);
}

void RENDERER_GL_API_CALL GL_LogPolygonOffset(float factor, float units)
{
    fprintf(rendererGlLogFile, "glPolygonOffset\n");
    fflush(rendererGlLogFile);
    rendererGlPolygonOffsetDriver(factor, units);
}

void RENDERER_GL_API_CALL GL_LogPolygonStipple(const uint8_t *mask)
{
    fprintf(rendererGlLogFile, "glPolygonStipple\n");
    fflush(rendererGlLogFile);
    rendererGlPolygonStippleDriver(mask);
}

void RENDERER_GL_API_CALL GL_LogPopAttrib(void)
{
    fprintf(rendererGlLogFile, "glPopAttrib\n");
    fflush(rendererGlLogFile);
    rendererGlPopAttribDriver();
}

void RENDERER_GL_API_CALL GL_LogPopClientAttrib(void)
{
    fprintf(rendererGlLogFile, "glPopClientAttrib\n");
    fflush(rendererGlLogFile);
    rendererGlPopClientAttribDriver();
}

void RENDERER_GL_API_CALL GL_LogPopMatrix(void)
{
    fprintf(rendererGlLogFile, "glPopMatrix\n");
    fflush(rendererGlLogFile);
    rendererGlPopMatrixDriver();
}

void RENDERER_GL_API_CALL GL_LogPopName(void)
{
    fprintf(rendererGlLogFile, "glPopName\n");
    fflush(rendererGlLogFile);
    rendererGlPopNameDriver();
}

void RENDERER_GL_API_CALL GL_LogPrioritizeTextures(int32_t count, const uint32_t *textures, const float *priorities)
{
    fprintf(rendererGlLogFile, "glPrioritizeTextures\n");
    fflush(rendererGlLogFile);
    rendererGlPrioritizeTexturesDriver(count, textures, priorities);
}

void RENDERER_GL_API_CALL GL_LogPushAttrib(uint32_t mask)
{
    fprintf(rendererGlLogFile, "glPushAttrib\n");
    fflush(rendererGlLogFile);
    rendererGlPushAttribDriver(mask);
}

void RENDERER_GL_API_CALL GL_LogPushClientAttrib(uint32_t mask)
{
    fprintf(rendererGlLogFile, "glPushClientAttrib\n");
    fflush(rendererGlLogFile);
    rendererGlPushClientAttribDriver(mask);
}

/* Source: CoDUOMP.exe 0x004ce920..0x004ceee7. Core fixed-function QGL
 * logging wrappers from glPushMatrix through glRectd. Each function writes its
 * exact .rdata operation name, flushes rendererGlLogFile, and invokes the
 * corresponding driver entry. The scalar double raster/rectangle functions
 * copy their arguments through x87 stack slots in the i386 build; the typed
 * calls preserve those original values and ordering at source level. */
void RENDERER_GL_API_CALL GL_LogPushMatrix(void)
{
    fprintf(rendererGlLogFile, "glPushMatrix\n");
    fflush(rendererGlLogFile);
    rendererGlPushMatrixDriver();
}

void RENDERER_GL_API_CALL GL_LogPushName(uint32_t name)
{
    fprintf(rendererGlLogFile, "glPushName\n");
    fflush(rendererGlLogFile);
    rendererGlPushNameDriver(name);
}

void RENDERER_GL_API_CALL GL_LogRasterPos2d(double x, double y)
{
    fprintf(rendererGlLogFile, "glRasterPos2d\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos2dDriver(x, y);
}

void RENDERER_GL_API_CALL GL_LogRasterPos2dv(const double *values)
{
    fprintf(rendererGlLogFile, "glRasterPos2dv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos2dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos2f(float x, float y)
{
    fprintf(rendererGlLogFile, "glRasterPos2f\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos2fDriver(x, y);
}

void RENDERER_GL_API_CALL GL_LogRasterPos2fv(const float *values)
{
    fprintf(rendererGlLogFile, "glRasterPos2fv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos2fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos2i(int32_t x, int32_t y)
{
    fprintf(rendererGlLogFile, "glRasterPos2i\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos2iDriver(x, y);
}

void RENDERER_GL_API_CALL GL_LogRasterPos2iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glRasterPos2iv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos2ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos2s(int16_t x, int16_t y)
{
    fprintf(rendererGlLogFile, "glRasterPos2s\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos2sDriver(x, y);
}

void RENDERER_GL_API_CALL GL_LogRasterPos2sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glRasterPos2sv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos2svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos3d(double x, double y, double z)
{
    fprintf(rendererGlLogFile, "glRasterPos3d\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos3dDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogRasterPos3dv(const double *values)
{
    fprintf(rendererGlLogFile, "glRasterPos3dv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos3dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos3f(float x, float y, float z)
{
    fprintf(rendererGlLogFile, "glRasterPos3f\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos3fDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogRasterPos3fv(const float *values)
{
    fprintf(rendererGlLogFile, "glRasterPos3fv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos3fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos3i(int32_t x, int32_t y, int32_t z)
{
    fprintf(rendererGlLogFile, "glRasterPos3i\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos3iDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogRasterPos3iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glRasterPos3iv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos3ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos3s(int16_t x, int16_t y, int16_t z)
{
    fprintf(rendererGlLogFile, "glRasterPos3s\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos3sDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogRasterPos3sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glRasterPos3sv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos3svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos4d(double x, double y, double z, double w)
{
    fprintf(rendererGlLogFile, "glRasterPos4d\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos4dDriver(x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogRasterPos4dv(const double *values)
{
    fprintf(rendererGlLogFile, "glRasterPos4dv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos4dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos4f(float x, float y, float z, float w)
{
    fprintf(rendererGlLogFile, "glRasterPos4f\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos4fDriver(x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogRasterPos4fv(const float *values)
{
    fprintf(rendererGlLogFile, "glRasterPos4fv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos4fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos4i(int32_t x, int32_t y, int32_t z, int32_t w)
{
    fprintf(rendererGlLogFile, "glRasterPos4i\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos4iDriver(x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogRasterPos4iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glRasterPos4iv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos4ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogRasterPos4s(int16_t x, int16_t y, int16_t z, int16_t w)
{
    fprintf(rendererGlLogFile, "glRasterPos4s\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos4sDriver(x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogRasterPos4sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glRasterPos4sv\n");
    fflush(rendererGlLogFile);
    rendererGlRasterPos4svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogReadBuffer(uint32_t mode)
{
    fprintf(rendererGlLogFile, "glReadBuffer\n");
    fflush(rendererGlLogFile);
    rendererGlReadBufferDriver(mode);
}

void RENDERER_GL_API_CALL GL_LogReadPixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                                           void *pixels)
{
    fprintf(rendererGlLogFile, "glReadPixels\n");
    fflush(rendererGlLogFile);
    rendererGlReadPixelsDriver(x, y, width, height, format, type, pixels);
}

void RENDERER_GL_API_CALL GL_LogRectd(double x1, double y1, double x2, double y2)
{
    fprintf(rendererGlLogFile, "glRectd\n");
    fflush(rendererGlLogFile);
    rendererGlRectdDriver(x1, y1, x2, y2);
}

/* Source: CoDUOMP.exe 0x004ceef0..0x004cf495. Core fixed-function QGL
 * logging wrappers from glRectdv through glTexCoord2fv. Each wrapper writes
 * its exact .rdata operation name, flushes rendererGlLogFile, and invokes the
 * corresponding driver entry. GL_LogRenderMode preserves the driver's GLint
 * return value. Scalar double calls are materialized through x87 stack slots
 * by the original i386 compiler; the typed calls preserve their values and
 * ordering without encoding a platform-specific stack layout. */
void RENDERER_GL_API_CALL GL_LogRectdv(const double *vertex1, const double *vertex2)
{
    fprintf(rendererGlLogFile, "glRectdv\n");
    fflush(rendererGlLogFile);
    rendererGlRectdvDriver(vertex1, vertex2);
}

void RENDERER_GL_API_CALL GL_LogRectf(float x1, float y1, float x2, float y2)
{
    fprintf(rendererGlLogFile, "glRectf\n");
    fflush(rendererGlLogFile);
    rendererGlRectfDriver(x1, y1, x2, y2);
}

void RENDERER_GL_API_CALL GL_LogRectfv(const float *vertex1, const float *vertex2)
{
    fprintf(rendererGlLogFile, "glRectfv\n");
    fflush(rendererGlLogFile);
    rendererGlRectfvDriver(vertex1, vertex2);
}

void RENDERER_GL_API_CALL GL_LogRecti(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    fprintf(rendererGlLogFile, "glRecti\n");
    fflush(rendererGlLogFile);
    rendererGlRectiDriver(x1, y1, x2, y2);
}

void RENDERER_GL_API_CALL GL_LogRectiv(const int32_t *vertex1, const int32_t *vertex2)
{
    fprintf(rendererGlLogFile, "glRectiv\n");
    fflush(rendererGlLogFile);
    rendererGlRectivDriver(vertex1, vertex2);
}

void RENDERER_GL_API_CALL GL_LogRects(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    fprintf(rendererGlLogFile, "glRects\n");
    fflush(rendererGlLogFile);
    rendererGlRectsDriver(x1, y1, x2, y2);
}

void RENDERER_GL_API_CALL GL_LogRectsv(const int16_t *vertex1, const int16_t *vertex2)
{
    fprintf(rendererGlLogFile, "glRectsv\n");
    fflush(rendererGlLogFile);
    rendererGlRectsvDriver(vertex1, vertex2);
}

int32_t RENDERER_GL_API_CALL GL_LogRenderMode(uint32_t mode)
{
    fprintf(rendererGlLogFile, "glRenderMode\n");
    fflush(rendererGlLogFile);
    return rendererGlRenderModeDriver(mode);
}

void RENDERER_GL_API_CALL GL_LogRotated(double angle, double x, double y, double z)
{
    fprintf(rendererGlLogFile, "glRotated\n");
    fflush(rendererGlLogFile);
    rendererGlRotatedDriver(angle, x, y, z);
}

void RENDERER_GL_API_CALL GL_LogRotatef(float angle, float x, float y, float z)
{
    fprintf(rendererGlLogFile, "glRotatef\n");
    fflush(rendererGlLogFile);
    rendererGlRotatefDriver(angle, x, y, z);
}

void RENDERER_GL_API_CALL GL_LogScaled(double x, double y, double z)
{
    fprintf(rendererGlLogFile, "glScaled\n");
    fflush(rendererGlLogFile);
    rendererGlScaledDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogScalef(float x, float y, float z)
{
    fprintf(rendererGlLogFile, "glScalef\n");
    fflush(rendererGlLogFile);
    rendererGlScalefDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogSelectBuffer(int32_t size, uint32_t *buffer)
{
    fprintf(rendererGlLogFile, "glSelectBuffer\n");
    fflush(rendererGlLogFile);
    rendererGlSelectBufferDriver(size, buffer);
}

void RENDERER_GL_API_CALL GL_LogShadeModel(uint32_t mode)
{
    fprintf(rendererGlLogFile, "glShadeModel\n");
    fflush(rendererGlLogFile);
    rendererGlShadeModelDriver(mode);
}

void RENDERER_GL_API_CALL GL_LogStencilFunc(uint32_t func, int32_t reference, uint32_t mask)
{
    fprintf(rendererGlLogFile, "glStencilFunc\n");
    fflush(rendererGlLogFile);
    rendererGlStencilFuncDriver(func, reference, mask);
}

void RENDERER_GL_API_CALL GL_LogStencilMask(uint32_t mask)
{
    fprintf(rendererGlLogFile, "glStencilMask\n");
    fflush(rendererGlLogFile);
    rendererGlStencilMaskDriver(mask);
}

void RENDERER_GL_API_CALL GL_LogStencilOp(uint32_t stencilFail, uint32_t depthFail, uint32_t depthPass)
{
    fprintf(rendererGlLogFile, "glStencilOp\n");
    fflush(rendererGlLogFile);
    rendererGlStencilOpDriver(stencilFail, depthFail, depthPass);
}

void RENDERER_GL_API_CALL GL_LogTexCoord1d(double s)
{
    fprintf(rendererGlLogFile, "glTexCoord1d\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord1dDriver(s);
}

void RENDERER_GL_API_CALL GL_LogTexCoord1dv(const double *values)
{
    fprintf(rendererGlLogFile, "glTexCoord1dv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord1dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord1f(float s)
{
    fprintf(rendererGlLogFile, "glTexCoord1f\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord1fDriver(s);
}

void RENDERER_GL_API_CALL GL_LogTexCoord1fv(const float *values)
{
    fprintf(rendererGlLogFile, "glTexCoord1fv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord1fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord1i(int32_t s)
{
    fprintf(rendererGlLogFile, "glTexCoord1i\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord1iDriver(s);
}

void RENDERER_GL_API_CALL GL_LogTexCoord1iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glTexCoord1iv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord1ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord1s(int16_t s)
{
    fprintf(rendererGlLogFile, "glTexCoord1s\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord1sDriver(s);
}

void RENDERER_GL_API_CALL GL_LogTexCoord1sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glTexCoord1sv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord1svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord2d(double s, double t)
{
    fprintf(rendererGlLogFile, "glTexCoord2d\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord2dDriver(s, t);
}

void RENDERER_GL_API_CALL GL_LogTexCoord2dv(const double *values)
{
    fprintf(rendererGlLogFile, "glTexCoord2dv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord2dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord2f(float s, float t)
{
    fprintf(rendererGlLogFile, "glTexCoord2f\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord2fDriver(s, t);
}

void RENDERER_GL_API_CALL GL_LogTexCoord2fv(const float *values)
{
    fprintf(rendererGlLogFile, "glTexCoord2fv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord2fvDriver(values);
}

/* Source: CoDUOMP.exe 0x004cf4a0..0x004cfa45. Core fixed-function QGL
 * logging wrappers from glTexCoord2i through glTexImage1D. Each wrapper writes
 * its exact .rdata operation name, flushes rendererGlLogFile, and invokes the
 * corresponding driver entry. Scalar double calls are materialized through
 * x87 stack slots by the original i386 compiler; the typed calls preserve the
 * values and source-level argument ordering. */
void RENDERER_GL_API_CALL GL_LogTexCoord2i(int32_t s, int32_t t)
{
    fprintf(rendererGlLogFile, "glTexCoord2i\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord2iDriver(s, t);
}

void RENDERER_GL_API_CALL GL_LogTexCoord2iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glTexCoord2iv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord2ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord2s(int16_t s, int16_t t)
{
    fprintf(rendererGlLogFile, "glTexCoord2s\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord2sDriver(s, t);
}

void RENDERER_GL_API_CALL GL_LogTexCoord2sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glTexCoord2sv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord2svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord3d(double s, double t, double r)
{
    fprintf(rendererGlLogFile, "glTexCoord3d\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord3dDriver(s, t, r);
}

void RENDERER_GL_API_CALL GL_LogTexCoord3dv(const double *values)
{
    fprintf(rendererGlLogFile, "glTexCoord3dv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord3dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord3f(float s, float t, float r)
{
    fprintf(rendererGlLogFile, "glTexCoord3f\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord3fDriver(s, t, r);
}

void RENDERER_GL_API_CALL GL_LogTexCoord3fv(const float *values)
{
    fprintf(rendererGlLogFile, "glTexCoord3fv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord3fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord3i(int32_t s, int32_t t, int32_t r)
{
    fprintf(rendererGlLogFile, "glTexCoord3i\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord3iDriver(s, t, r);
}

void RENDERER_GL_API_CALL GL_LogTexCoord3iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glTexCoord3iv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord3ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord3s(int16_t s, int16_t t, int16_t r)
{
    fprintf(rendererGlLogFile, "glTexCoord3s\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord3sDriver(s, t, r);
}

void RENDERER_GL_API_CALL GL_LogTexCoord3sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glTexCoord3sv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord3svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord4d(double s, double t, double r, double q)
{
    fprintf(rendererGlLogFile, "glTexCoord4d\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord4dDriver(s, t, r, q);
}

void RENDERER_GL_API_CALL GL_LogTexCoord4dv(const double *values)
{
    fprintf(rendererGlLogFile, "glTexCoord4dv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord4dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord4f(float s, float t, float r, float q)
{
    fprintf(rendererGlLogFile, "glTexCoord4f\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord4fDriver(s, t, r, q);
}

void RENDERER_GL_API_CALL GL_LogTexCoord4fv(const float *values)
{
    fprintf(rendererGlLogFile, "glTexCoord4fv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord4fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord4i(int32_t s, int32_t t, int32_t r, int32_t q)
{
    fprintf(rendererGlLogFile, "glTexCoord4i\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord4iDriver(s, t, r, q);
}

void RENDERER_GL_API_CALL GL_LogTexCoord4iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glTexCoord4iv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord4ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexCoord4s(int16_t s, int16_t t, int16_t r, int16_t q)
{
    fprintf(rendererGlLogFile, "glTexCoord4s\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord4sDriver(s, t, r, q);
}

void RENDERER_GL_API_CALL GL_LogTexCoord4sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glTexCoord4sv\n");
    fflush(rendererGlLogFile);
    rendererGlTexCoord4svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogTexEnvfv(uint32_t target, uint32_t parameter, const float *values)
{
    fprintf(rendererGlLogFile, "glTexEnvfv\n");
    fflush(rendererGlLogFile);
    rendererGlTexEnvfvDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogTexEnviv(uint32_t target, uint32_t parameter, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glTexEnviv\n");
    fflush(rendererGlLogFile);
    rendererGlTexEnvivDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogTexGend(uint32_t coordinate, uint32_t parameter, double value)
{
    fprintf(rendererGlLogFile, "glTexGend\n");
    fflush(rendererGlLogFile);
    rendererGlTexGendDriver(coordinate, parameter, value);
}

void RENDERER_GL_API_CALL GL_LogTexGendv(uint32_t coordinate, uint32_t parameter, const double *values)
{
    fprintf(rendererGlLogFile, "glTexGendv\n");
    fflush(rendererGlLogFile);
    rendererGlTexGendvDriver(coordinate, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogTexGenf(uint32_t coordinate, uint32_t parameter, float value)
{
    fprintf(rendererGlLogFile, "glTexGenf\n");
    fflush(rendererGlLogFile);
    rendererGlTexGenfDriver(coordinate, parameter, value);
}

void RENDERER_GL_API_CALL GL_LogTexGenfv(uint32_t coordinate, uint32_t parameter, const float *values)
{
    fprintf(rendererGlLogFile, "glTexGenfv\n");
    fflush(rendererGlLogFile);
    rendererGlTexGenfvDriver(coordinate, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogTexGeni(uint32_t coordinate, uint32_t parameter, int32_t value)
{
    fprintf(rendererGlLogFile, "glTexGeni\n");
    fflush(rendererGlLogFile);
    rendererGlTexGeniDriver(coordinate, parameter, value);
}

void RENDERER_GL_API_CALL GL_LogTexGeniv(uint32_t coordinate, uint32_t parameter, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glTexGeniv\n");
    fflush(rendererGlLogFile);
    rendererGlTexGenivDriver(coordinate, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogTexImage1D(uint32_t target, int32_t level, int32_t internalFormat, int32_t width, int32_t border,
                                           uint32_t format, uint32_t type, const void *pixels)
{
    fprintf(rendererGlLogFile, "glTexImage1D\n");
    fflush(rendererGlLogFile);
    rendererGlTexImage1DDriver(target, level, internalFormat, width, border, format, type, pixels);
}

/* Source: CoDUOMP.exe 0x004cfa50..0x004d0065. Core fixed-function QGL
 * logging wrappers from glTexImage2D through glVertex4sv. Each wrapper writes
 * its exact .rdata operation name, flushes rendererGlLogFile, and invokes the
 * corresponding driver entry. The scalar double wrappers explicitly copy
 * their arguments through x87 stack slots in the original i386 code. */
void RENDERER_GL_API_CALL GL_LogTexImage2D(uint32_t target, int32_t level, int32_t internalFormat, int32_t width, int32_t height,
                                           int32_t border, uint32_t format, uint32_t type, const void *pixels)
{
    fprintf(rendererGlLogFile, "glTexImage2D\n");
    fflush(rendererGlLogFile);
    rendererGlTexImage2DDriver(target, level, internalFormat, width, height, border, format, type, pixels);
}

void RENDERER_GL_API_CALL GL_LogTexParameterfv(uint32_t target, uint32_t parameter, const float *values)
{
    fprintf(rendererGlLogFile, "glTexParameterfv\n");
    fflush(rendererGlLogFile);
    rendererGlTexParameterfvDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogTexParameteriv(uint32_t target, uint32_t parameter, const int32_t *values)
{
    fprintf(rendererGlLogFile, "glTexParameteriv\n");
    fflush(rendererGlLogFile);
    rendererGlTexParameterivDriver(target, parameter, values);
}

void RENDERER_GL_API_CALL GL_LogTexSubImage1D(uint32_t target, int32_t level, int32_t xOffset, int32_t width, uint32_t format,
                                              uint32_t type, const void *pixels)
{
    fprintf(rendererGlLogFile, "glTexSubImage1D\n");
    fflush(rendererGlLogFile);
    rendererGlTexSubImage1DDriver(target, level, xOffset, width, format, type, pixels);
}

void RENDERER_GL_API_CALL GL_LogTexSubImage2D(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset, int32_t width,
                                              int32_t height, uint32_t format, uint32_t type, const void *pixels)
{
    fprintf(rendererGlLogFile, "glTexSubImage2D\n");
    fflush(rendererGlLogFile);
    rendererGlTexSubImage2DDriver(target, level, xOffset, yOffset, width, height, format, type, pixels);
}

void RENDERER_GL_API_CALL GL_LogTranslated(double x, double y, double z)
{
    fprintf(rendererGlLogFile, "glTranslated\n");
    fflush(rendererGlLogFile);
    rendererGlTranslatedDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogTranslatef(float x, float y, float z)
{
    fprintf(rendererGlLogFile, "glTranslatef\n");
    fflush(rendererGlLogFile);
    rendererGlTranslatefDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogVertex2d(double x, double y)
{
    fprintf(rendererGlLogFile, "glVertex2d\n");
    fflush(rendererGlLogFile);
    rendererGlVertex2dDriver(x, y);
}

void RENDERER_GL_API_CALL GL_LogVertex2dv(const double *values)
{
    fprintf(rendererGlLogFile, "glVertex2dv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex2dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex2f(float x, float y)
{
    fprintf(rendererGlLogFile, "glVertex2f\n");
    fflush(rendererGlLogFile);
    rendererGlVertex2fDriver(x, y);
}

void RENDERER_GL_API_CALL GL_LogVertex2fv(const float *values)
{
    fprintf(rendererGlLogFile, "glVertex2fv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex2fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex2i(int32_t x, int32_t y)
{
    fprintf(rendererGlLogFile, "glVertex2i\n");
    fflush(rendererGlLogFile);
    rendererGlVertex2iDriver(x, y);
}

void RENDERER_GL_API_CALL GL_LogVertex2iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glVertex2iv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex2ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex2s(int16_t x, int16_t y)
{
    fprintf(rendererGlLogFile, "glVertex2s\n");
    fflush(rendererGlLogFile);
    rendererGlVertex2sDriver(x, y);
}

void RENDERER_GL_API_CALL GL_LogVertex2sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glVertex2sv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex2svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex3d(double x, double y, double z)
{
    fprintf(rendererGlLogFile, "glVertex3d\n");
    fflush(rendererGlLogFile);
    rendererGlVertex3dDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogVertex3dv(const double *values)
{
    fprintf(rendererGlLogFile, "glVertex3dv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex3dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex3f(float x, float y, float z)
{
    fprintf(rendererGlLogFile, "glVertex3f\n");
    fflush(rendererGlLogFile);
    rendererGlVertex3fDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogVertex3fv(const float *values)
{
    fprintf(rendererGlLogFile, "glVertex3fv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex3fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex3i(int32_t x, int32_t y, int32_t z)
{
    fprintf(rendererGlLogFile, "glVertex3i\n");
    fflush(rendererGlLogFile);
    rendererGlVertex3iDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogVertex3iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glVertex3iv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex3ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex3s(int16_t x, int16_t y, int16_t z)
{
    fprintf(rendererGlLogFile, "glVertex3s\n");
    fflush(rendererGlLogFile);
    rendererGlVertex3sDriver(x, y, z);
}

void RENDERER_GL_API_CALL GL_LogVertex3sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glVertex3sv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex3svDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex4d(double x, double y, double z, double w)
{
    fprintf(rendererGlLogFile, "glVertex4d\n");
    fflush(rendererGlLogFile);
    rendererGlVertex4dDriver(x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogVertex4dv(const double *values)
{
    fprintf(rendererGlLogFile, "glVertex4dv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex4dvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex4f(float x, float y, float z, float w)
{
    fprintf(rendererGlLogFile, "glVertex4f\n");
    fflush(rendererGlLogFile);
    rendererGlVertex4fDriver(x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogVertex4fv(const float *values)
{
    fprintf(rendererGlLogFile, "glVertex4fv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex4fvDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex4i(int32_t x, int32_t y, int32_t z, int32_t w)
{
    fprintf(rendererGlLogFile, "glVertex4i\n");
    fflush(rendererGlLogFile);
    rendererGlVertex4iDriver(x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogVertex4iv(const int32_t *values)
{
    fprintf(rendererGlLogFile, "glVertex4iv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex4ivDriver(values);
}

void RENDERER_GL_API_CALL GL_LogVertex4s(int16_t x, int16_t y, int16_t z, int16_t w)
{
    fprintf(rendererGlLogFile, "glVertex4s\n");
    fflush(rendererGlLogFile);
    rendererGlVertex4sDriver(x, y, z, w);
}

void RENDERER_GL_API_CALL GL_LogVertex4sv(const int16_t *values)
{
    fprintf(rendererGlLogFile, "glVertex4sv\n");
    fflush(rendererGlLogFile);
    rendererGlVertex4svDriver(values);
}
