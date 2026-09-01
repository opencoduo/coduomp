#include "gl_error_wrappers.h"

#include "gl_debug.h"
#include "../platform/dynamic_library_boundary.h"
#include "client/engine/q_shared.h"

enum {
    RENDERER_PLATFORM_NO_ERROR = 0
};

/* Source: CoDUOMP.exe 0x004d05e0..0x004d071b. These extension dispatch
 * wrappers call the real GL entry, query glGetError exactly once afterward,
 * and print the operation-specific diagnostic only for a nonzero result.
 * Provisional Checked names distinguish this layer from the separate
 * GL_Log* stream wrappers at 0x004cab50..0x004cae95. */
void RENDERER_GL_API_CALL GL_CheckedMultiTexCoord2fARB(
    uint32_t target, float s, float t)
{
    uint32_t error;

    rendererGlMultiTexCoord2fARBDriver(target, s, t);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMultiTexCoord2fARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedActiveTextureARB(uint32_t texture)
{
    uint32_t error;

    rendererGlActiveTextureARBDriver(texture);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glActiveTextureARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedClientActiveTextureARB(uint32_t texture)
{
    uint32_t error;

    rendererGlClientActiveTextureARBDriver(texture);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glClientActiveTextureARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLockArraysEXT(
    int32_t first, int32_t count)
{
    uint32_t error;

    rendererGlLockArraysEXTDriver(first, count);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLockArraysEXT: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedUnlockArraysEXT(void)
{
    uint32_t error;

    rendererGlUnlockArraysEXTDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glUnlockArraysEXT: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPNTrianglesiATI(
    uint32_t parameter, int32_t value)
{
    uint32_t error;

    rendererGlPNTrianglesiATIDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPNTrianglesiATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPNTrianglesfATI(
    uint32_t parameter, float value)
{
    uint32_t error;

    rendererGlPNTrianglesfATIDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPNTrianglesfATI: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d0720..0x004d097d. Checked dispatch for the draw-
 * range and ARB compressed-texture entry points. Argument order and widths are
 * proven by the explicit right-to-left stack copies before each driver call. */
void RENDERER_GL_API_CALL GL_CheckedDrawRangeElementsEXT(
    uint32_t mode, uint32_t start, uint32_t end, int32_t count,
    uint32_t type, const void *indices)
{
    uint32_t error;

    rendererGlDrawRangeElementsEXTDriver(
        mode, start, end, count, type, indices);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDrawRangeElementsEXT: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCompressedTexImage3DARB(
    uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
    int32_t height, int32_t depth, int32_t border, int32_t imageSize,
    const void *data)
{
    uint32_t error;

    rendererGlCompressedTexImage3DARBDriver(
        target, level, internalFormat, width, height, depth, border, imageSize,
        data);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCompressedTexImage3DARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCompressedTexImage2DARB(
    uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
    int32_t height, int32_t border, int32_t imageSize, const void *data)
{
    uint32_t error;

    rendererGlCompressedTexImage2DARBDriver(
        target, level, internalFormat, width, height, border, imageSize, data);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCompressedTexImage2DARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCompressedTexImage1DARB(
    uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
    int32_t border, int32_t imageSize, const void *data)
{
    uint32_t error;

    rendererGlCompressedTexImage1DARBDriver(
        target, level, internalFormat, width, border, imageSize, data);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCompressedTexImage1DARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCompressedTexSubImage3DARB(
    uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
    int32_t zOffset, int32_t width, int32_t height, int32_t depth,
    uint32_t format, int32_t imageSize, const void *data)
{
    uint32_t error;

    rendererGlCompressedTexSubImage3DARBDriver(
        target, level, xOffset, yOffset, zOffset, width, height, depth, format,
        imageSize, data);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glCompressedTexSubImage3DARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedCompressedTexSubImage2DARB(
    uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
    int32_t width, int32_t height, uint32_t format, int32_t imageSize,
    const void *data)
{
    uint32_t error;

    rendererGlCompressedTexSubImage2DARBDriver(
        target, level, xOffset, yOffset, width, height, format, imageSize,
        data);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glCompressedTexSubImage2DARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedCompressedTexSubImage1DARB(
    uint32_t target, int32_t level, int32_t xOffset, int32_t width,
    uint32_t format, int32_t imageSize, const void *data)
{
    uint32_t error;

    rendererGlCompressedTexSubImage1DARBDriver(
        target, level, xOffset, width, format, imageSize, data);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glCompressedTexSubImage1DARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetCompressedTexImageARB(
    uint32_t target, int32_t level, void *image)
{
    uint32_t error;

    rendererGlGetCompressedTexImageARBDriver(target, level, image);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetCompressedTexImageARB: glGetError() = 0x%04x\n",
                   error);
}

/* Source: CoDUOMP.exe 0x004d0980..0x004d0bcd. Checked ARB buffer-object
 * dispatch. glIsBufferARB and glUnmapBufferARB save AL across glGetError;
 * glMapBufferARB saves the complete returned pointer in ESI. */
void RENDERER_GL_API_CALL GL_CheckedBindBufferARB(
    uint32_t target, uint32_t buffer)
{
    uint32_t error;

    rendererGlBindBufferARBDriver(target, buffer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBindBufferARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDeleteBuffersARB(
    int32_t count, const uint32_t *buffers)
{
    uint32_t error;

    rendererGlDeleteBuffersARBDriver(count, buffers);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDeleteBuffersARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGenBuffersARB(
    int32_t count, uint32_t *buffers)
{
    uint32_t error;

    rendererGlGenBuffersARBDriver(count, buffers);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGenBuffersARB: glGetError() = 0x%04x\n", error);
}

uint8_t RENDERER_GL_API_CALL GL_CheckedIsBufferARB(uint32_t buffer)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlIsBufferARBDriver(buffer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIsBufferARB: glGetError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedBufferDataARB(
    uint32_t target, intptr_t size, const void *data, uint32_t usage)
{
    uint32_t error;

    rendererGlBufferDataARBDriver(target, size, data, usage);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBufferDataARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedBufferSubDataARB(
    uint32_t target, intptr_t offset, intptr_t size, const void *data)
{
    uint32_t error;

    rendererGlBufferSubDataARBDriver(target, offset, size, data);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBufferSubDataARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetBufferSubDataARB(
    uint32_t target, intptr_t offset, intptr_t size, void *data)
{
    uint32_t error;

    rendererGlGetBufferSubDataARBDriver(target, offset, size, data);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetBufferSubDataARB: glGetError() = 0x%04x\n", error);
}

void *RENDERER_GL_API_CALL GL_CheckedMapBufferARB(
    uint32_t target, uint32_t access)
{
    void *result;
    uint32_t error;

    result = rendererGlMapBufferARBDriver(target, access);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMapBufferARB: glGetError() = 0x%04x\n", error);
    return result;
}

uint8_t RENDERER_GL_API_CALL GL_CheckedUnmapBufferARB(uint32_t target)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlUnmapBufferARBDriver(target);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glUnmapBufferARB: glGetError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedGetBufferParameterivARB(
    uint32_t target, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetBufferParameterivARBDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetBufferParameterivARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedGetBufferPointervARB(
    uint32_t target, uint32_t parameter, void **pointer)
{
    uint32_t error;

    rendererGlGetBufferPointervARBDriver(target, parameter, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetBufferPointervARB: glGetError() = 0x%04x\n",
                   error);
}

/* Source: CoDUOMP.exe 0x004d0bd0..0x004d0e7b. Checked ATI object-buffer and
 * array-object dispatch. glNewObjectBufferATI saves the returned object name
 * in ESI and glIsObjectBufferATI saves AL while each wrapper checks the error. */
uint32_t RENDERER_GL_API_CALL GL_CheckedNewObjectBufferATI(
    int32_t size, const void *data, uint32_t usage)
{
    uint32_t result;
    uint32_t error;

    result = rendererGlNewObjectBufferATIDriver(size, data, usage);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNewObjectBufferATI: glGetError() = 0x%04x\n", error);
    return result;
}

uint8_t RENDERER_GL_API_CALL GL_CheckedIsObjectBufferATI(uint32_t buffer)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlIsObjectBufferATIDriver(buffer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIsObjectBufferATI: glGetError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedUpdateObjectBufferATI(
    uint32_t buffer, uint32_t offset, int32_t size, const void *data,
    uint32_t preserveMode)
{
    uint32_t error;

    rendererGlUpdateObjectBufferATIDriver(
        buffer, offset, size, data, preserveMode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glUpdateObjectBufferATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetObjectBufferfvATI(
    uint32_t buffer, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetObjectBufferfvATIDriver(buffer, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetObjectBufferfvATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetObjectBufferivATI(
    uint32_t buffer, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetObjectBufferivATIDriver(buffer, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetObjectBufferivATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFreeObjectBufferATI(uint32_t buffer)
{
    uint32_t error;

    rendererGlFreeObjectBufferATIDriver(buffer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFreeObjectBufferATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedArrayObjectATI(
    uint32_t array, int32_t size, uint32_t type, int32_t stride,
    uint32_t buffer, uint32_t offset)
{
    uint32_t error;

    rendererGlArrayObjectATIDriver(array, size, type, stride, buffer, offset);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glArrayObjectATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetArrayObjectfvATI(
    uint32_t array, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetArrayObjectfvATIDriver(array, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetArrayObjectfvATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetArrayObjectivATI(
    uint32_t array, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetArrayObjectivATIDriver(array, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetArrayObjectivATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVariantArrayObjectATI(
    uint32_t id, uint32_t type, int32_t stride, uint32_t buffer,
    uint32_t offset)
{
    uint32_t error;

    rendererGlVariantArrayObjectATIDriver(id, type, stride, buffer, offset);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVariantArrayObjectATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetVariantArrayObjectfvATI(
    uint32_t id, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetVariantArrayObjectfvATIDriver(id, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetVariantArrayObjectfvATI: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetVariantArrayObjectivATI(
    uint32_t id, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetVariantArrayObjectivATIDriver(id, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetVariantArrayObjectivATI: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedElementPointerATI(
    uint32_t type, const void *pointer)
{
    uint32_t error;

    rendererGlElementPointerATIDriver(type, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glElementPointerATI: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d0e80..0x004d10fb. Checked ATI element-array and
 * NV vertex-array/fence dispatch. The WGL allocator preserves its native
 * pointer result across GetLastError; the two fence predicates preserve AL
 * across glGetError. */
void RENDERER_GL_API_CALL GL_CheckedDrawElementArrayATI(
    uint32_t mode, int32_t count)
{
    uint32_t error;

    rendererGlDrawElementArrayATIDriver(mode, count);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDrawElementArrayATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDrawRangeElementArrayATI(
    uint32_t mode, uint32_t start, uint32_t end, int32_t count)
{
    uint32_t error;

    rendererGlDrawRangeElementArrayATIDriver(mode, start, end, count);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glDrawRangeElementArrayATI: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedFlushVertexArrayRangeNV(void)
{
    uint32_t error;

    rendererGlFlushVertexArrayRangeNVDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glFlushVertexArrayRangeNV: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexArrayRangeNV(
    int32_t length, const void *pointer)
{
    uint32_t error;

    rendererGlVertexArrayRangeNVDriver(length, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexArrayRangeNV: glGetError() = 0x%04x\n", error);
}

void *RENDERER_GL_API_CALL GL_CheckedAllocateMemoryNV(
    int32_t size, float readFrequency, float writeFrequency, float priority)
{
    void *result;
    uint32_t error;

    result = rendererGlAllocateMemoryNVDriver(
        size, readFrequency, writeFrequency, priority);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    error = coduomp_platform_last_error();
    if (error != RENDERER_PLATFORM_NO_ERROR)
        Com_Printf("^3wglAllocateMemoryNV: GetLastError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedFreeMemoryNV(void *memory)
{
    uint32_t error;

    rendererGlFreeMemoryNVDriver(memory);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    error = coduomp_platform_last_error();
    if (error != RENDERER_PLATFORM_NO_ERROR)
        Com_Printf("^3wglFreeMemoryNV: GetLastError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDeleteFencesNV(
    int32_t count, const uint32_t *fences)
{
    uint32_t error;

    rendererGlDeleteFencesNVDriver(count, fences);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDeleteFencesNV: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGenFencesNV(
    int32_t count, uint32_t *fences)
{
    uint32_t error;

    rendererGlGenFencesNVDriver(count, fences);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGenFencesNV: glGetError() = 0x%04x\n", error);
}

uint8_t RENDERER_GL_API_CALL GL_CheckedIsFenceNV(uint32_t fence)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlIsFenceNVDriver(fence);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIsFenceNV: glGetError() = 0x%04x\n", error);
    return result;
}

uint8_t RENDERER_GL_API_CALL GL_CheckedTestFenceNV(uint32_t fence)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlTestFenceNVDriver(fence);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTestFenceNV: glGetError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedGetFenceivNV(
    uint32_t fence, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetFenceivNVDriver(fence, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetFenceivNV: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFinishFenceNV(uint32_t fence)
{
    uint32_t error;

    rendererGlFinishFenceNVDriver(fence);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFinishFenceNV: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedSetFenceNV(
    uint32_t fence, uint32_t condition)
{
    uint32_t error;

    rendererGlSetFenceNVDriver(fence, condition);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glSetFenceNV: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d1100..0x004d1460. Checked NV register-combiner
 * setters and queries. Argument order and the three GLboolean output controls
 * match the corresponding extension driver signatures. */
void RENDERER_GL_API_CALL GL_CheckedCombinerParameterfvNV(
    uint32_t parameter, const float *values)
{
    uint32_t error;

    rendererGlCombinerParameterfvNVDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCombinerParameterfvNV: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCombinerParameterfNV(
    uint32_t parameter, float value)
{
    uint32_t error;

    rendererGlCombinerParameterfNVDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCombinerParameterfNV: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCombinerParameterivNV(
    uint32_t parameter, const int32_t *values)
{
    uint32_t error;

    rendererGlCombinerParameterivNVDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCombinerParameterivNV: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCombinerParameteriNV(
    uint32_t parameter, int32_t value)
{
    uint32_t error;

    rendererGlCombinerParameteriNVDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCombinerParameteriNV: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCombinerInputNV(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t input,
    uint32_t mapping, uint32_t componentUsage)
{
    uint32_t error;

    rendererGlCombinerInputNVDriver(
        stage, portion, variable, input, mapping, componentUsage);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCombinerInputNV: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedCombinerOutputNV(
    uint32_t stage, uint32_t portion, uint32_t abOutput, uint32_t cdOutput,
    uint32_t sumOutput, uint32_t scale, uint32_t bias,
    uint8_t abDotProduct, uint8_t cdDotProduct, uint8_t muxSum)
{
    uint32_t error;

    rendererGlCombinerOutputNVDriver(
        stage, portion, abOutput, cdOutput, sumOutput, scale, bias,
        abDotProduct, cdDotProduct, muxSum);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCombinerOutputNV: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFinalCombinerInputNV(
    uint32_t variable, uint32_t input, uint32_t mapping,
    uint32_t componentUsage)
{
    uint32_t error;

    rendererGlFinalCombinerInputNVDriver(
        variable, input, mapping, componentUsage);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFinalCombinerInputNV: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedGetCombinerInputParameterfvNV(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
    float *values)
{
    uint32_t error;

    rendererGlGetCombinerInputParameterfvNVDriver(
        stage, portion, variable, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetCombinerInputParameterfvNV: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetCombinerInputParameterivNV(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
    int32_t *values)
{
    uint32_t error;

    rendererGlGetCombinerInputParameterivNVDriver(
        stage, portion, variable, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetCombinerInputParameterivNV: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetCombinerOutputParameterfvNV(
    uint32_t stage, uint32_t portion, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetCombinerOutputParameterfvNVDriver(
        stage, portion, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetCombinerOutputParameterfvNV: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetCombinerOutputParameterivNV(
    uint32_t stage, uint32_t portion, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetCombinerOutputParameterivNVDriver(
        stage, portion, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetCombinerOutputParameterivNV: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetFinalCombinerInputParameterfvNV(
    uint32_t variable, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetFinalCombinerInputParameterfvNVDriver(
        variable, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetFinalCombinerInputParameterfvNV: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetFinalCombinerInputParameterivNV(
    uint32_t variable, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetFinalCombinerInputParameterivNVDriver(
        variable, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetFinalCombinerInputParameterivNV: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedCombinerStageParameterfvNV(
    uint32_t stage, uint32_t parameter, const float *values)
{
    uint32_t error;

    rendererGlCombinerStageParameterfvNVDriver(stage, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCombinerStageParameterfvNV: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedGetCombinerStageParameterfvNV(
    uint32_t stage, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetCombinerStageParameterfvNVDriver(stage, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetCombinerStageParameterfvNV: glGetError() = 0x%04x\n",
            error);
}

/* Source: CoDUOMP.exe 0x004d1460..0x004d17cb. Checked ATI fragment-shader
 * lifecycle, instruction emission, and constant upload. The generator saves
 * its returned shader-name base in ESI across glGetError. */
uint32_t RENDERER_GL_API_CALL GL_CheckedGenFragmentShadersATI(uint32_t range)
{
    uint32_t result;
    uint32_t error;

    result = rendererGlGenFragmentShadersATIDriver(range);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGenFragmentShadersATI: glGetError() = 0x%04x\n",
                   error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedBindFragmentShaderATI(uint32_t shader)
{
    uint32_t error;

    rendererGlBindFragmentShaderATIDriver(shader);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBindFragmentShaderATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedDeleteFragmentShaderATI(uint32_t shader)
{
    uint32_t error;

    rendererGlDeleteFragmentShaderATIDriver(shader);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDeleteFragmentShaderATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedBeginFragmentShaderATI(void)
{
    uint32_t error;

    rendererGlBeginFragmentShaderATIDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBeginFragmentShaderATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedEndFragmentShaderATI(void)
{
    uint32_t error;

    rendererGlEndFragmentShaderATIDriver();
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEndFragmentShaderATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPassTexCoordATI(
    uint32_t destination, uint32_t coordinate, uint32_t swizzle)
{
    uint32_t error;

    rendererGlPassTexCoordATIDriver(destination, coordinate, swizzle);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPassTexCoordATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedSampleMapATI(
    uint32_t destination, uint32_t interpolation, uint32_t swizzle)
{
    uint32_t error;

    rendererGlSampleMapATIDriver(destination, interpolation, swizzle);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glSampleMapATI: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColorFragmentOp1ATI(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier)
{
    uint32_t error;

    rendererGlColorFragmentOp1ATIDriver(
        operation, destination, destinationMask, destinationModifier,
        argument1, argument1Replication, argument1Modifier);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColorFragmentOp1ATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedColorFragmentOp2ATI(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier,
    uint32_t argument2, uint32_t argument2Replication,
    uint32_t argument2Modifier)
{
    uint32_t error;

    rendererGlColorFragmentOp2ATIDriver(
        operation, destination, destinationMask, destinationModifier,
        argument1, argument1Replication, argument1Modifier, argument2,
        argument2Replication, argument2Modifier);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColorFragmentOp2ATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedColorFragmentOp3ATI(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier,
    uint32_t argument2, uint32_t argument2Replication,
    uint32_t argument2Modifier, uint32_t argument3,
    uint32_t argument3Replication, uint32_t argument3Modifier)
{
    uint32_t error;

    rendererGlColorFragmentOp3ATIDriver(
        operation, destination, destinationMask, destinationModifier,
        argument1, argument1Replication, argument1Modifier, argument2,
        argument2Replication, argument2Modifier, argument3,
        argument3Replication, argument3Modifier);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColorFragmentOp3ATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedAlphaFragmentOp1ATI(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier)
{
    uint32_t error;

    rendererGlAlphaFragmentOp1ATIDriver(
        operation, destination, destinationModifier, argument1,
        argument1Replication, argument1Modifier);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glAlphaFragmentOp1ATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedAlphaFragmentOp2ATI(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier, uint32_t argument2,
    uint32_t argument2Replication, uint32_t argument2Modifier)
{
    uint32_t error;

    rendererGlAlphaFragmentOp2ATIDriver(
        operation, destination, destinationModifier, argument1,
        argument1Replication, argument1Modifier, argument2,
        argument2Replication, argument2Modifier);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glAlphaFragmentOp2ATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedAlphaFragmentOp3ATI(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier, uint32_t argument2,
    uint32_t argument2Replication, uint32_t argument2Modifier,
    uint32_t argument3, uint32_t argument3Replication,
    uint32_t argument3Modifier)
{
    uint32_t error;

    rendererGlAlphaFragmentOp3ATIDriver(
        operation, destination, destinationModifier, argument1,
        argument1Replication, argument1Modifier, argument2,
        argument2Replication, argument2Modifier, argument3,
        argument3Replication, argument3Modifier);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glAlphaFragmentOp3ATI: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedSetFragmentShaderConstantATI(
    uint32_t destination, const float *value)
{
    uint32_t error;

    rendererGlSetFragmentShaderConstantATIDriver(destination, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glSetFragmentShaderConstantATI: glGetError() = 0x%04x\n",
            error);
}

/* Source: CoDUOMP.exe 0x004d17d0..0x004d1aca. Checked scalar ARB vertex
 * attributes. Typed parameters preserve the original signed-short, float,
 * double, and normalized-unsigned-byte API widths without i386 stack math. */
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1sARB(
    uint32_t index, int16_t x)
{
    uint32_t error;

    rendererGlVertexAttrib1sARBDriver(index, x);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib1sARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1fARB(
    uint32_t index, float x)
{
    uint32_t error;

    rendererGlVertexAttrib1fARBDriver(index, x);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib1fARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1dARB(
    uint32_t index, double x)
{
    uint32_t error;

    rendererGlVertexAttrib1dARBDriver(index, x);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib1dARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2sARB(
    uint32_t index, int16_t x, int16_t y)
{
    uint32_t error;

    rendererGlVertexAttrib2sARBDriver(index, x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib2sARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2fARB(
    uint32_t index, float x, float y)
{
    uint32_t error;

    rendererGlVertexAttrib2fARBDriver(index, x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib2fARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2dARB(
    uint32_t index, double x, double y)
{
    uint32_t error;

    rendererGlVertexAttrib2dARBDriver(index, x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib2dARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3sARB(
    uint32_t index, int16_t x, int16_t y, int16_t z)
{
    uint32_t error;

    rendererGlVertexAttrib3sARBDriver(index, x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib3sARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3fARB(
    uint32_t index, float x, float y, float z)
{
    uint32_t error;

    rendererGlVertexAttrib3fARBDriver(index, x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib3fARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3dARB(
    uint32_t index, double x, double y, double z)
{
    uint32_t error;

    rendererGlVertexAttrib3dARBDriver(index, x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib3dARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4sARB(
    uint32_t index, int16_t x, int16_t y, int16_t z, int16_t w)
{
    uint32_t error;

    rendererGlVertexAttrib4sARBDriver(index, x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4sARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4fARB(
    uint32_t index, float x, float y, float z, float w)
{
    uint32_t error;

    rendererGlVertexAttrib4fARBDriver(index, x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4fARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4dARB(
    uint32_t index, double x, double y, double z, double w)
{
    uint32_t error;

    rendererGlVertexAttrib4dARBDriver(index, x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4dARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NubARB(
    uint32_t index, uint8_t x, uint8_t y, uint8_t z, uint8_t w)
{
    uint32_t error;

    rendererGlVertexAttrib4NubARBDriver(index, x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4NubARB: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d1ad0..0x004d1f1b. Checked ARB vector attribute
 * entry points. Each function retains its exact signedness, element width,
 * normalization variant, and const-qualified vector pointer. */
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1svARB(
    uint32_t index, const int16_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib1svARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib1svARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1fvARB(
    uint32_t index, const float *values)
{
    uint32_t error;

    rendererGlVertexAttrib1fvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib1fvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1dvARB(
    uint32_t index, const double *values)
{
    uint32_t error;

    rendererGlVertexAttrib1dvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib1dvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2svARB(
    uint32_t index, const int16_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib2svARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib2svARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2fvARB(
    uint32_t index, const float *values)
{
    uint32_t error;

    rendererGlVertexAttrib2fvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib2fvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2dvARB(
    uint32_t index, const double *values)
{
    uint32_t error;

    rendererGlVertexAttrib2dvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib2dvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3svARB(
    uint32_t index, const int16_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib3svARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib3svARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3fvARB(
    uint32_t index, const float *values)
{
    uint32_t error;

    rendererGlVertexAttrib3fvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib3fvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3dvARB(
    uint32_t index, const double *values)
{
    uint32_t error;

    rendererGlVertexAttrib3dvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib3dvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4bvARB(
    uint32_t index, const int8_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4bvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4bvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4svARB(
    uint32_t index, const int16_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4svARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4svARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4ivARB(
    uint32_t index, const int32_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4ivARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4ivARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4ubvARB(
    uint32_t index, const uint8_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4ubvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4ubvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4usvARB(
    uint32_t index, const uint16_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4usvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4usvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4uivARB(
    uint32_t index, const uint32_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4uivARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4uivARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4fvARB(
    uint32_t index, const float *values)
{
    uint32_t error;

    rendererGlVertexAttrib4fvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4fvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4dvARB(
    uint32_t index, const double *values)
{
    uint32_t error;

    rendererGlVertexAttrib4dvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4dvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NbvARB(
    uint32_t index, const int8_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4NbvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4NbvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NsvARB(
    uint32_t index, const int16_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4NsvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4NsvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NivARB(
    uint32_t index, const int32_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4NivARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4NivARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NubvARB(
    uint32_t index, const uint8_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4NubvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4NubvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NusvARB(
    uint32_t index, const uint16_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4NusvARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4NusvARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NuivARB(
    uint32_t index, const uint32_t *values)
{
    uint32_t error;

    rendererGlVertexAttrib4NuivARBDriver(index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttrib4NuivARB: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d1f20..0x004d208b. Checked ARB generic-attribute
 * array setup and initial program-object lifecycle calls. The vertex pointer
 * remains a native host pointer; only the API's normalized flag is byte-wide. */
void RENDERER_GL_API_CALL GL_CheckedVertexAttribPointerARB(
    uint32_t index, int32_t size, uint32_t type, uint8_t normalized,
    int32_t stride, const void *pointer)
{
    uint32_t error;

    rendererGlVertexAttribPointerARBDriver(
        index, size, type, normalized, stride, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexAttribPointerARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedEnableVertexAttribArrayARB(uint32_t index)
{
    uint32_t error;

    rendererGlEnableVertexAttribArrayARBDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glEnableVertexAttribArrayARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedDisableVertexAttribArrayARB(uint32_t index)
{
    uint32_t error;

    rendererGlDisableVertexAttribArrayARBDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glDisableVertexAttribArrayARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedProgramStringARB(
    uint32_t target, uint32_t format, int32_t length, const void *string)
{
    uint32_t error;

    rendererGlProgramStringARBDriver(target, format, length, string);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glProgramStringARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedBindProgramARB(
    uint32_t target, uint32_t program)
{
    uint32_t error;

    rendererGlBindProgramARBDriver(target, program);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBindProgramARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDeleteProgramsARB(
    int32_t count, const uint32_t *programs)
{
    uint32_t error;

    rendererGlDeleteProgramsARBDriver(count, programs);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDeleteProgramsARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGenProgramsARB(
    int32_t count, uint32_t *programs)
{
    uint32_t error;

    rendererGlGenProgramsARBDriver(count, programs);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGenProgramsARB: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d2090..0x004d226d. Checked ARB program
 * environment/local-parameter setters. The scalar-double wrappers preserve
 * all four 64-bit arguments; the vector wrappers pass the original arrays. */
void RENDERER_GL_API_CALL GL_CheckedProgramEnvParameter4fARB(
    uint32_t target, uint32_t index, float x, float y, float z, float w)
{
    uint32_t error;

    rendererGlProgramEnvParameter4fARBDriver(target, index, x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glProgramEnvParameter4fARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedProgramEnvParameter4dARB(
    uint32_t target, uint32_t index,
    double x, double y, double z, double w)
{
    uint32_t error;

    rendererGlProgramEnvParameter4dARBDriver(target, index, x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glProgramEnvParameter4dARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedProgramEnvParameter4fvARB(
    uint32_t target, uint32_t index, const float *values)
{
    uint32_t error;

    rendererGlProgramEnvParameter4fvARBDriver(target, index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glProgramEnvParameter4fvARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedProgramEnvParameter4dvARB(
    uint32_t target, uint32_t index, const double *values)
{
    uint32_t error;

    rendererGlProgramEnvParameter4dvARBDriver(target, index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glProgramEnvParameter4dvARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedProgramLocalParameter4fARB(
    uint32_t target, uint32_t index, float x, float y, float z, float w)
{
    uint32_t error;

    rendererGlProgramLocalParameter4fARBDriver(target, index, x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glProgramLocalParameter4fARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedProgramLocalParameter4dARB(
    uint32_t target, uint32_t index,
    double x, double y, double z, double w)
{
    uint32_t error;

    rendererGlProgramLocalParameter4dARBDriver(target, index, x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glProgramLocalParameter4dARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedProgramLocalParameter4fvARB(
    uint32_t target, uint32_t index, const float *values)
{
    uint32_t error;

    rendererGlProgramLocalParameter4fvARBDriver(target, index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glProgramLocalParameter4fvARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedProgramLocalParameter4dvARB(
    uint32_t target, uint32_t index, const double *values)
{
    uint32_t error;

    rendererGlProgramLocalParameter4dvARBDriver(target, index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glProgramLocalParameter4dvARB: glGetError() = 0x%04x\n",
            error);
}

/* Source: CoDUOMP.exe 0x004d2270..0x004d247c. Checked ARB program and
 * generic-attribute queries. GL_CheckedIsProgramARB preserves the driver's
 * byte-wide result across the subsequent glGetError call. */
void RENDERER_GL_API_CALL GL_CheckedGetProgramEnvParameterfvARB(
    uint32_t target, uint32_t index, float *values)
{
    uint32_t error;

    rendererGlGetProgramEnvParameterfvARBDriver(target, index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetProgramEnvParameterfvARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetProgramEnvParameterdvARB(
    uint32_t target, uint32_t index, double *values)
{
    uint32_t error;

    rendererGlGetProgramEnvParameterdvARBDriver(target, index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetProgramEnvParameterdvARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetProgramLocalParameterfvARB(
    uint32_t target, uint32_t index, float *values)
{
    uint32_t error;

    rendererGlGetProgramLocalParameterfvARBDriver(target, index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetProgramLocalParameterfvARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetProgramLocalParameterdvARB(
    uint32_t target, uint32_t index, double *values)
{
    uint32_t error;

    rendererGlGetProgramLocalParameterdvARBDriver(target, index, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetProgramLocalParameterdvARB: glGetError() = 0x%04x\n",
            error);
}

void RENDERER_GL_API_CALL GL_CheckedGetProgramivARB(
    uint32_t target, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetProgramivARBDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetProgramivARB: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetProgramStringARB(
    uint32_t target, uint32_t parameter, void *string)
{
    uint32_t error;

    rendererGlGetProgramStringARBDriver(target, parameter, string);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetProgramStringARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedGetVertexAttribdvARB(
    uint32_t index, uint32_t parameter, double *values)
{
    uint32_t error;

    rendererGlGetVertexAttribdvARBDriver(index, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetVertexAttribdvARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedGetVertexAttribfvARB(
    uint32_t index, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetVertexAttribfvARBDriver(index, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetVertexAttribfvARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedGetVertexAttribivARB(
    uint32_t index, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetVertexAttribivARBDriver(index, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetVertexAttribivARB: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedGetVertexAttribPointervARB(
    uint32_t index, uint32_t parameter, void **pointer)
{
    uint32_t error;

    rendererGlGetVertexAttribPointervARBDriver(index, parameter, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetVertexAttribPointervARB: glGetError() = 0x%04x\n",
            error);
}

uint8_t RENDERER_GL_API_CALL GL_CheckedIsProgramARB(uint32_t program)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlIsProgramARBDriver(program);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIsProgramARB: glGetError() = 0x%04x\n", error);
    return result;
}

/* Source: CoDUOMP.exe 0x004d2480..0x004d25f4. First checked core-OpenGL
 * wrapper group. GL_CheckedAreTexturesResident preserves the driver's
 * byte-wide aggregate result while the residence array is filled in place. */
void RENDERER_GL_API_CALL GL_CheckedAccum(uint32_t operation, float value)
{
    uint32_t error;

    rendererGlAccumDriver(operation, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glAccum: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedAlphaFunc(
    uint32_t function, float reference)
{
    uint32_t error;

    rendererGlAlphaFuncDriver(function, reference);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glAlphaFunc: glGetError() = 0x%04x\n", error);
}

uint8_t RENDERER_GL_API_CALL GL_CheckedAreTexturesResident(
    int32_t count, const uint32_t *textures, uint8_t *residences)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlAreTexturesResidentDriver(count, textures, residences);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glAreTexturesResident: glGetError() = 0x%04x\n",
                   error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedArrayElement(int32_t index)
{
    uint32_t error;

    rendererGlArrayElementDriver(index);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glArrayElement: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedBegin(uint32_t mode)
{
    uint32_t error;

    rendererGlBeginDriver(mode);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBegin: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedBindTexture(
    uint32_t target, uint32_t texture)
{
    uint32_t error;

    rendererGlBindTextureDriver(target, texture);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBindTexture: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedBitmap(
    int32_t width, int32_t height, float xOrigin, float yOrigin,
    float xMove, float yMove, const uint8_t *bitmap)
{
    uint32_t error;

    rendererGlBitmapDriver(
        width, height, xOrigin, yOrigin, xMove, yMove, bitmap);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBitmap: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d2600..0x004d27c6. Checked blend, display-list,
 * and clear-state wrappers. ClearDepth is the original double-precision API;
 * the remaining scalar and component arguments retain their native GL widths. */
void RENDERER_GL_API_CALL GL_CheckedBlendFunc(
    uint32_t sourceFactor, uint32_t destinationFactor)
{
    uint32_t error;

    rendererGlBlendFuncDriver(sourceFactor, destinationFactor);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glBlendFunc: glGetError() = 0x%04x\n", error);
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
void RENDERER_GL_API_CALL GL_CheckedCallList(uint32_t list)
{
    uint32_t error;

    rendererGlCallListDriver(list);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCallList: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedCallLists(
    int32_t count, uint32_t type, const void *lists)
{
    uint32_t error;

    rendererGlCallListsDriver(count, type, lists);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCallLists: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedClear(uint32_t mask)
{
    uint32_t error;

    rendererGlClearDriver(mask);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glClear: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedClearAccum(
    float red, float green, float blue, float alpha)
{
    uint32_t error;

    rendererGlClearAccumDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glClearAccum: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedClearColor(
    float red, float green, float blue, float alpha)
{
    uint32_t error;

    rendererGlClearColorDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glClearColor: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedClearDepth(double depth)
{
    uint32_t error;

    rendererGlClearDepthDriver(depth);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glClearDepth: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedClearIndex(float index)
{
    uint32_t error;

    rendererGlClearIndexDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glClearIndex: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedClearStencil(int32_t stencil)
{
    uint32_t error;

    rendererGlClearStencilDriver(stencil);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glClearStencil: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d27d0..0x004d2b06. Checked clip-plane and
 * complete Color3 family. Scalar signedness and width follow the individual
 * OpenGL entry points; vector forms forward their typed source arrays. */
void RENDERER_GL_API_CALL GL_CheckedClipPlane(
    uint32_t plane, const double *equation)
{
    uint32_t error;

    rendererGlClipPlaneDriver(plane, equation);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glClipPlane: glGetError() = 0x%04x\n", error);
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
void RENDERER_GL_API_CALL GL_CheckedColor3b(
    int8_t red, int8_t green, int8_t blue)
{
    uint32_t error;

    rendererGlColor3bDriver(red, green, blue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3b: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3bv(const int8_t *values)
{
    uint32_t error;

    rendererGlColor3bvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3bv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3d(
    double red, double green, double blue)
{
    uint32_t error;

    rendererGlColor3dDriver(red, green, blue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3dv(const double *values)
{
    uint32_t error;

    rendererGlColor3dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3f(
    float red, float green, float blue)
{
    uint32_t error;

    rendererGlColor3fDriver(red, green, blue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3fv(const float *values)
{
    uint32_t error;

    rendererGlColor3fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3i(
    int32_t red, int32_t green, int32_t blue)
{
    uint32_t error;

    rendererGlColor3iDriver(red, green, blue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3iv(const int32_t *values)
{
    uint32_t error;

    rendererGlColor3ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3s(
    int16_t red, int16_t green, int16_t blue)
{
    uint32_t error;

    rendererGlColor3sDriver(red, green, blue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3sv(const int16_t *values)
{
    uint32_t error;

    rendererGlColor3svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3ub(
    uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t error;

    rendererGlColor3ubDriver(red, green, blue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3ub: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3ubv(const uint8_t *values)
{
    uint32_t error;

    rendererGlColor3ubvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3ubv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3ui(
    uint32_t red, uint32_t green, uint32_t blue)
{
    uint32_t error;

    rendererGlColor3uiDriver(red, green, blue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3ui: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3uiv(const uint32_t *values)
{
    uint32_t error;

    rendererGlColor3uivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3uiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3us(
    uint16_t red, uint16_t green, uint16_t blue)
{
    uint32_t error;

    rendererGlColor3usDriver(red, green, blue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3us: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor3usv(const uint16_t *values)
{
    uint32_t error;

    rendererGlColor3usvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor3usv: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d2b10..0x004d2e96. Complete checked Color4
 * family. Each scalar variant retains the original component signedness and
 * width; each vector variant forwards the corresponding typed array. */
void RENDERER_GL_API_CALL GL_CheckedColor4b(
    int8_t red, int8_t green, int8_t blue, int8_t alpha)
{
    uint32_t error;

    rendererGlColor4bDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4b: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4bv(const int8_t *values)
{
    uint32_t error;

    rendererGlColor4bvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4bv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4d(
    double red, double green, double blue, double alpha)
{
    uint32_t error;

    rendererGlColor4dDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4dv(const double *values)
{
    uint32_t error;

    rendererGlColor4dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4f(
    float red, float green, float blue, float alpha)
{
    uint32_t error;

    rendererGlColor4fDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4fv(const float *values)
{
    uint32_t error;

    rendererGlColor4fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4i(
    int32_t red, int32_t green, int32_t blue, int32_t alpha)
{
    uint32_t error;

    rendererGlColor4iDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4iv(const int32_t *values)
{
    uint32_t error;

    rendererGlColor4ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4s(
    int16_t red, int16_t green, int16_t blue, int16_t alpha)
{
    uint32_t error;

    rendererGlColor4sDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4sv(const int16_t *values)
{
    uint32_t error;

    rendererGlColor4svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4ub(
    uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    uint32_t error;

    rendererGlColor4ubDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4ub: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4ubv(const uint8_t *values)
{
    uint32_t error;

    rendererGlColor4ubvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4ubv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4ui(
    uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha)
{
    uint32_t error;

    rendererGlColor4uiDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4ui: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4uiv(const uint32_t *values)
{
    uint32_t error;

    rendererGlColor4uivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4uiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4us(
    uint16_t red, uint16_t green, uint16_t blue, uint16_t alpha)
{
    uint32_t error;

    rendererGlColor4usDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4us: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColor4usv(const uint16_t *values)
{
    uint32_t error;

    rendererGlColor4usvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColor4usv: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d2ea0..0x004d30e6. Checked color-array state,
 * framebuffer-to-framebuffer/texture copies, and face culling. Native host
 * pointers are retained for ColorPointer; copy dimensions remain signed GL
 * sizes/coordinates while targets and formats remain enum-width values. */
void RENDERER_GL_API_CALL GL_CheckedColorMask(
    uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    uint32_t error;

    rendererGlColorMaskDriver(red, green, blue, alpha);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColorMask: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColorMaterial(
    uint32_t face, uint32_t mode)
{
    uint32_t error;

    rendererGlColorMaterialDriver(face, mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColorMaterial: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedColorPointer(
    int32_t size, uint32_t type, int32_t stride, const void *pointer)
{
    uint32_t error;

    rendererGlColorPointerDriver(size, type, stride, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glColorPointer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedCopyPixels(
    int32_t x, int32_t y, int32_t width, int32_t height, uint32_t type)
{
    uint32_t error;

    rendererGlCopyPixelsDriver(x, y, width, height, type);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCopyPixels: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedCopyTexImage1D(
    uint32_t target, int32_t level, uint32_t internalFormat,
    int32_t x, int32_t y, int32_t width, int32_t border)
{
    uint32_t error;

    rendererGlCopyTexImage1DDriver(
        target, level, internalFormat, x, y, width, border);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCopyTexImage1D: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedCopyTexImage2D(
    uint32_t target, int32_t level, uint32_t internalFormat,
    int32_t x, int32_t y, int32_t width, int32_t height, int32_t border)
{
    uint32_t error;

    rendererGlCopyTexImage2DDriver(
        target, level, internalFormat, x, y, width, height, border);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCopyTexImage2D: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedCopyTexSubImage1D(
    uint32_t target, int32_t level, int32_t xOffset,
    int32_t x, int32_t y, int32_t width)
{
    uint32_t error;

    rendererGlCopyTexSubImage1DDriver(target, level, xOffset, x, y, width);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCopyTexSubImage1D: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCopyTexSubImage2D(
    uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
    int32_t x, int32_t y, int32_t width, int32_t height)
{
    uint32_t error;

    rendererGlCopyTexSubImage2DDriver(
        target, level, xOffset, yOffset, x, y, width, height);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCopyTexSubImage2D: glGetError() = 0x%04x\n",
                   error);
}

void RENDERER_GL_API_CALL GL_CheckedCullFace(uint32_t mode)
{
    uint32_t error;

    rendererGlCullFaceDriver(mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glCullFace: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d30f0..0x004d34b6. Checked fixed-function state,
 * drawing, edge-flag, display-list, and evaluator dispatch. The original
 * wrappers forward each argument to the driver slot, query glGetError once,
 * and print only when that query returns a nonzero error. */
void RENDERER_GL_API_CALL GL_CheckedDeleteLists(
    uint32_t list, int32_t range)
{
    uint32_t error;

    rendererGlDeleteListsDriver(list, range);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDeleteLists: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDeleteTextures(
    int32_t count, const uint32_t *textures)
{
    uint32_t error;

    rendererGlDeleteTexturesDriver(count, textures);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDeleteTextures: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDepthFunc(uint32_t function)
{
    uint32_t error;

    rendererGlDepthFuncDriver(function);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDepthFunc: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDepthMask(uint8_t enabled)
{
    uint32_t error;

    rendererGlDepthMaskDriver(enabled);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDepthMask: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDepthRange(
    double nearValue, double farValue)
{
    uint32_t error;

    rendererGlDepthRangeDriver(nearValue, farValue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDepthRange: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDisable(uint32_t capability)
{
    uint32_t error;

    rendererGlDisableDriver(capability);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDisable: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDisableClientState(uint32_t array)
{
    uint32_t error;

    rendererGlDisableClientStateDriver(array);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDisableClientState: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDrawArrays(
    uint32_t mode, int32_t first, int32_t count)
{
    uint32_t error;

    rendererGlDrawArraysDriver(mode, first, count);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDrawArrays: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDrawBuffer(uint32_t buffer)
{
    uint32_t error;

    rendererGlDrawBufferDriver(buffer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDrawBuffer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDrawElements(
    uint32_t mode, int32_t count, uint32_t type, const void *indices)
{
    uint32_t error;

    rendererGlDrawElementsDriver(mode, count, type, indices);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDrawElements: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedDrawPixels(
    int32_t width, int32_t height, uint32_t format, uint32_t type,
    const void *pixels)
{
    uint32_t error;

    rendererGlDrawPixelsDriver(width, height, format, type, pixels);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glDrawPixels: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEdgeFlag(uint8_t flag)
{
    uint32_t error;

    rendererGlEdgeFlagDriver(flag);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEdgeFlag: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEdgeFlagPointer(
    int32_t stride, const void *pointer)
{
    uint32_t error;

    rendererGlEdgeFlagPointerDriver(stride, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEdgeFlagPointer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEdgeFlagv(const uint8_t *flag)
{
    uint32_t error;

    rendererGlEdgeFlagvDriver(flag);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEdgeFlagv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEnable(uint32_t capability)
{
    uint32_t error;

    rendererGlEnableDriver(capability);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEnable: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEnableClientState(uint32_t array)
{
    uint32_t error;

    rendererGlEnableClientStateDriver(array);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEnableClientState: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEnd(void)
{
    uint32_t error;

    rendererGlEndDriver();
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEnd: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEndList(void)
{
    uint32_t error;

    rendererGlEndListDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEndList: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalCoord1d(double u)
{
    uint32_t error;

    rendererGlEvalCoord1dDriver(u);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalCoord1d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalCoord1dv(const double *u)
{
    uint32_t error;

    rendererGlEvalCoord1dvDriver(u);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalCoord1dv: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d34c0..0x004d396b. Checked evaluator completion,
 * feedback, pipeline completion, fog, projection, object generation, and
 * initial state-query dispatch. GL_CheckedGenLists preserves the driver's
 * return value across the error check exactly as the original wrapper does. */
void RENDERER_GL_API_CALL GL_CheckedEvalCoord1f(float u)
{
    uint32_t error;

    rendererGlEvalCoord1fDriver(u);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalCoord1f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalCoord1fv(const float *u)
{
    uint32_t error;

    rendererGlEvalCoord1fvDriver(u);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalCoord1fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalCoord2d(double u, double v)
{
    uint32_t error;

    rendererGlEvalCoord2dDriver(u, v);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalCoord2d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalCoord2dv(const double *values)
{
    uint32_t error;

    rendererGlEvalCoord2dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalCoord2dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalCoord2f(float u, float v)
{
    uint32_t error;

    rendererGlEvalCoord2fDriver(u, v);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalCoord2f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalCoord2fv(const float *values)
{
    uint32_t error;

    rendererGlEvalCoord2fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalCoord2fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalMesh1(
    uint32_t mode, int32_t i1, int32_t i2)
{
    uint32_t error;

    rendererGlEvalMesh1Driver(mode, i1, i2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalMesh1: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalMesh2(
    uint32_t mode, int32_t i1, int32_t i2, int32_t j1, int32_t j2)
{
    uint32_t error;

    rendererGlEvalMesh2Driver(mode, i1, i2, j1, j2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalMesh2: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalPoint1(int32_t i)
{
    uint32_t error;

    rendererGlEvalPoint1Driver(i);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalPoint1: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedEvalPoint2(int32_t i, int32_t j)
{
    uint32_t error;

    rendererGlEvalPoint2Driver(i, j);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glEvalPoint2: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFeedbackBuffer(
    int32_t size, uint32_t type, float *buffer)
{
    uint32_t error;

    rendererGlFeedbackBufferDriver(size, type, buffer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFeedbackBuffer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFinish(void)
{
    uint32_t error;

    rendererGlFinishDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFinish: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFlush(void)
{
    uint32_t error;

    rendererGlFlushDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFlush: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFogf(uint32_t parameter, float value)
{
    uint32_t error;

    rendererGlFogfDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFogf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFogfv(
    uint32_t parameter, const float *values)
{
    uint32_t error;

    rendererGlFogfvDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFogfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFogi(uint32_t parameter, int32_t value)
{
    uint32_t error;

    rendererGlFogiDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFogi: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFogiv(
    uint32_t parameter, const int32_t *values)
{
    uint32_t error;

    rendererGlFogivDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFogiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFrontFace(uint32_t mode)
{
    uint32_t error;

    rendererGlFrontFaceDriver(mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFrontFace: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedFrustum(
    double left, double right, double bottom, double top,
    double nearValue, double farValue)
{
    uint32_t error;

    rendererGlFrustumDriver(left, right, bottom, top, nearValue, farValue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glFrustum: glGetError() = 0x%04x\n", error);
}

uint32_t RENDERER_GL_API_CALL GL_CheckedGenLists(int32_t range)
{
    uint32_t result;
    uint32_t error;

    result = rendererGlGenListsDriver(range);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGenLists: glGetError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedGenTextures(
    int32_t count, uint32_t *textures)
{
    uint32_t error;

    rendererGlGenTexturesDriver(count, textures);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGenTextures: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetBooleanv(
    uint32_t parameter, uint8_t *values)
{
    uint32_t error;

    rendererGlGetBooleanvDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetBooleanv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetClipPlane(
    uint32_t plane, double *equation)
{
    uint32_t error;

    rendererGlGetClipPlaneDriver(plane, equation);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetClipPlane: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetDoublev(
    uint32_t parameter, double *values)
{
    uint32_t error;

    rendererGlGetDoublevDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetDoublev: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d3970..0x004d3eab. Checked state-query family and
 * glHint. GetError and GetString preserve the first driver's scalar/pointer
 * result across the separate diagnostic error query. All output parameters
 * remain native pointers with the element width proved by their GL entry. */
uint32_t RENDERER_GL_API_CALL GL_CheckedGetError(void)
{
    uint32_t result;
    uint32_t diagnosticError;

    result = rendererGlGetErrorDriver();
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    diagnosticError = rendererGlGetErrorDriver();
    if (diagnosticError != GL_NO_ERROR)
        Com_Printf("^3glGetError: glGetError() = 0x%04x\n",
                   diagnosticError);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedGetFloatv(
    uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetFloatvDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetFloatv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetIntegerv(
    uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetIntegervDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetIntegerv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetLightfv(
    uint32_t light, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetLightfvDriver(light, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetLightfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetLightiv(
    uint32_t light, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetLightivDriver(light, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetLightiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetMapdv(
    uint32_t target, uint32_t query, double *values)
{
    uint32_t error;

    rendererGlGetMapdvDriver(target, query, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetMapdv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetMapfv(
    uint32_t target, uint32_t query, float *values)
{
    uint32_t error;

    rendererGlGetMapfvDriver(target, query, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetMapfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetMapiv(
    uint32_t target, uint32_t query, int32_t *values)
{
    uint32_t error;

    rendererGlGetMapivDriver(target, query, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetMapiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetMaterialfv(
    uint32_t face, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetMaterialfvDriver(face, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetMaterialfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetMaterialiv(
    uint32_t face, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetMaterialivDriver(face, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetMaterialiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetPixelMapfv(
    uint32_t map, float *values)
{
    uint32_t error;

    rendererGlGetPixelMapfvDriver(map, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetPixelMapfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetPixelMapuiv(
    uint32_t map, uint32_t *values)
{
    uint32_t error;

    rendererGlGetPixelMapuivDriver(map, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetPixelMapuiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetPixelMapusv(
    uint32_t map, uint16_t *values)
{
    uint32_t error;

    rendererGlGetPixelMapusvDriver(map, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetPixelMapusv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetPointerv(
    uint32_t parameter, void **value)
{
    uint32_t error;

    rendererGlGetPointervDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetPointerv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetPolygonStipple(uint8_t *mask)
{
    uint32_t error;

    rendererGlGetPolygonStippleDriver(mask);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetPolygonStipple: glGetError() = 0x%04x\n", error);
}

const uint8_t *RENDERER_GL_API_CALL GL_CheckedGetString(uint32_t name)
{
    const uint8_t *result;
    uint32_t error;

    result = rendererGlGetStringDriver(name);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetString: glGetError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedGetTexEnvfv(
    uint32_t target, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetTexEnvfvDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetTexEnvfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexEnviv(
    uint32_t target, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetTexEnvivDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetTexEnviv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexGendv(
    uint32_t coordinate, uint32_t parameter, double *values)
{
    uint32_t error;

    rendererGlGetTexGendvDriver(coordinate, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetTexGendv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexGenfv(
    uint32_t coordinate, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetTexGenfvDriver(coordinate, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetTexGenfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexGeniv(
    uint32_t coordinate, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetTexGenivDriver(coordinate, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetTexGeniv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexImage(
    uint32_t target, int32_t level, uint32_t format, uint32_t type,
    void *pixels)
{
    uint32_t error;

    rendererGlGetTexImageDriver(target, level, format, type, pixels);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetTexImage: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexLevelParameterfv(
    uint32_t target, int32_t level, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetTexLevelParameterfvDriver(target, level, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetTexLevelParameterfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexLevelParameteriv(
    uint32_t target, int32_t level, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetTexLevelParameterivDriver(target, level, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf(
            "^3glGetTexLevelParameteriv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexParameterfv(
    uint32_t target, uint32_t parameter, float *values)
{
    uint32_t error;

    rendererGlGetTexParameterfvDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetTexParameterfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedGetTexParameteriv(
    uint32_t target, uint32_t parameter, int32_t *values)
{
    uint32_t error;

    rendererGlGetTexParameterivDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glGetTexParameteriv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedHint(uint32_t target, uint32_t mode)
{
    uint32_t error;

    rendererGlHintDriver(target, mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glHint: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d3eb0..0x004d42bd. Checked color-index,
 * interleaved-array, object-query, and initial lighting dispatch. The Is*
 * wrappers preserve the driver's one-byte GLboolean result across the
 * diagnostic query; scalar index widths follow the corresponding GL entry. */
void RENDERER_GL_API_CALL GL_CheckedIndexMask(uint32_t mask)
{
    uint32_t error;

    rendererGlIndexMaskDriver(mask);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexMask: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexPointer(
    uint32_t type, int32_t stride, const void *pointer)
{
    uint32_t error;

    rendererGlIndexPointerDriver(type, stride, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexPointer: glGetError() = 0x%04x\n", error);
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
void RENDERER_GL_API_CALL GL_CheckedIndexd(double index)
{
    uint32_t error;

    rendererGlIndexdDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexd: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexdv(const double *index)
{
    uint32_t error;

    rendererGlIndexdvDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexdv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexf(float index)
{
    uint32_t error;

    rendererGlIndexfDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexfv(const float *index)
{
    uint32_t error;

    rendererGlIndexfvDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexi(int32_t index)
{
    uint32_t error;

    rendererGlIndexiDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexi: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexiv(const int32_t *index)
{
    uint32_t error;

    rendererGlIndexivDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexs(int16_t index)
{
    uint32_t error;

    rendererGlIndexsDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexs: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexsv(const int16_t *index)
{
    uint32_t error;

    rendererGlIndexsvDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexsv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexub(uint8_t index)
{
    uint32_t error;

    rendererGlIndexubDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexub: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedIndexubv(const uint8_t *index)
{
    uint32_t error;

    rendererGlIndexubvDriver(index);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIndexubv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedInitNames(void)
{
    uint32_t error;

    rendererGlInitNamesDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glInitNames: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedInterleavedArrays(
    uint32_t format, int32_t stride, const void *pointer)
{
    uint32_t error;

    rendererGlInterleavedArraysDriver(format, stride, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glInterleavedArrays: glGetError() = 0x%04x\n", error);
}

uint8_t RENDERER_GL_API_CALL GL_CheckedIsEnabled(uint32_t capability)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlIsEnabledDriver(capability);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIsEnabled: glGetError() = 0x%04x\n", error);
    return result;
}

uint8_t RENDERER_GL_API_CALL GL_CheckedIsList(uint32_t list)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlIsListDriver(list);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIsList: glGetError() = 0x%04x\n", error);
    return result;
}

uint8_t RENDERER_GL_API_CALL GL_CheckedIsTexture(uint32_t texture)
{
    uint8_t result;
    uint32_t error;

    result = rendererGlIsTextureDriver(texture);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glIsTexture: glGetError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedLightModelf(
    uint32_t parameter, float value)
{
    uint32_t error;

    rendererGlLightModelfDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLightModelf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLightModelfv(
    uint32_t parameter, const float *values)
{
    uint32_t error;

    rendererGlLightModelfvDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLightModelfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLightModeli(
    uint32_t parameter, int32_t value)
{
    uint32_t error;

    rendererGlLightModeliDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLightModeli: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLightModeliv(
    uint32_t parameter, const int32_t *values)
{
    uint32_t error;

    rendererGlLightModelivDriver(parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLightModeliv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLightf(
    uint32_t light, uint32_t parameter, float value)
{
    uint32_t error;

    rendererGlLightfDriver(light, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLightf: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d42c0..0x004d44b6. Checked lighting completion,
 * line/list state, matrix loads, selection-name loading, and logic operation
 * dispatch. The line stipple pattern retains its GLushort width. */
void RENDERER_GL_API_CALL GL_CheckedLightfv(
    uint32_t light, uint32_t parameter, const float *values)
{
    uint32_t error;

    rendererGlLightfvDriver(light, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLightfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLighti(
    uint32_t light, uint32_t parameter, int32_t value)
{
    uint32_t error;

    rendererGlLightiDriver(light, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLighti: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLightiv(
    uint32_t light, uint32_t parameter, const int32_t *values)
{
    uint32_t error;

    rendererGlLightivDriver(light, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLightiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLineStipple(
    int32_t factor, uint16_t pattern)
{
    uint32_t error;

    rendererGlLineStippleDriver(factor, pattern);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLineStipple: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLineWidth(float width)
{
    uint32_t error;

    rendererGlLineWidthDriver(width);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLineWidth: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedListBase(uint32_t base)
{
    uint32_t error;

    rendererGlListBaseDriver(base);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glListBase: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLoadIdentity(void)
{
    uint32_t error;

    rendererGlLoadIdentityDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLoadIdentity: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLoadMatrixd(const double *matrix)
{
    uint32_t error;

    rendererGlLoadMatrixdDriver(matrix);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLoadMatrixd: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLoadMatrixf(const float *matrix)
{
    uint32_t error;

    rendererGlLoadMatrixfDriver(matrix);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLoadMatrixf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLoadName(uint32_t name)
{
    uint32_t error;

    rendererGlLoadNameDriver(name);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLoadName: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedLogicOp(uint32_t operation)
{
    uint32_t error;

    rendererGlLogicOpDriver(operation);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glLogicOp: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d44c0..0x004d47e0. Checked evaluator-map,
 * evaluator-grid, and material-state dispatch. The x87 loads and stores prove
 * that every d-suffixed domain argument remains a double at the driver
 * boundary; the remaining scalar and pointer widths match the OpenGL 1.1
 * entry-point signatures. */
void RENDERER_GL_API_CALL GL_CheckedMap1d(
    uint32_t target, double u1, double u2, int32_t stride, int32_t order,
    const double *points)
{
    uint32_t error;

    rendererGlMap1dDriver(target, u1, u2, stride, order, points);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMap1d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMap1f(
    uint32_t target, float u1, float u2, int32_t stride, int32_t order,
    const float *points)
{
    uint32_t error;

    rendererGlMap1fDriver(target, u1, u2, stride, order, points);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMap1f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMap2d(
    uint32_t target, double u1, double u2, int32_t uStride, int32_t uOrder,
    double v1, double v2, int32_t vStride, int32_t vOrder,
    const double *points)
{
    uint32_t error;

    rendererGlMap2dDriver(
        target, u1, u2, uStride, uOrder, v1, v2, vStride, vOrder, points);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMap2d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMap2f(
    uint32_t target, float u1, float u2, int32_t uStride, int32_t uOrder,
    float v1, float v2, int32_t vStride, int32_t vOrder,
    const float *points)
{
    uint32_t error;

    rendererGlMap2fDriver(
        target, u1, u2, uStride, uOrder, v1, v2, vStride, vOrder, points);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMap2f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMapGrid1d(
    int32_t count, double u1, double u2)
{
    uint32_t error;

    rendererGlMapGrid1dDriver(count, u1, u2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMapGrid1d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMapGrid1f(
    int32_t count, float u1, float u2)
{
    uint32_t error;

    rendererGlMapGrid1fDriver(count, u1, u2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMapGrid1f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMapGrid2d(
    int32_t uCount, double u1, double u2,
    int32_t vCount, double v1, double v2)
{
    uint32_t error;

    rendererGlMapGrid2dDriver(uCount, u1, u2, vCount, v1, v2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMapGrid2d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMapGrid2f(
    int32_t uCount, float u1, float u2,
    int32_t vCount, float v1, float v2)
{
    uint32_t error;

    rendererGlMapGrid2fDriver(uCount, u1, u2, vCount, v1, v2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMapGrid2f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMaterialf(
    uint32_t face, uint32_t parameter, float value)
{
    uint32_t error;

    rendererGlMaterialfDriver(face, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMaterialf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMaterialfv(
    uint32_t face, uint32_t parameter, const float *values)
{
    uint32_t error;

    rendererGlMaterialfvDriver(face, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMaterialfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMateriali(
    uint32_t face, uint32_t parameter, int32_t value)
{
    uint32_t error;

    rendererGlMaterialiDriver(face, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMateriali: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMaterialiv(
    uint32_t face, uint32_t parameter, const int32_t *values)
{
    uint32_t error;

    rendererGlMaterialivDriver(face, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMaterialiv: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d47e0..0x004d4c40. Checked matrix/list,
 * fixed-function normal, projection, feedback-token, pixel-map, and pixel-
 * store dispatch. The Normal3b/Normal3s scalar parameters retain their GLbyte
 * and GLshort source widths even though stdcall allocates one stack word per
 * argument; Ortho retains all six double-width arguments. */
void RENDERER_GL_API_CALL GL_CheckedMatrixMode(uint32_t mode)
{
    uint32_t error;

    rendererGlMatrixModeDriver(mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMatrixMode: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMultMatrixd(const double *matrix)
{
    uint32_t error;

    rendererGlMultMatrixdDriver(matrix);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMultMatrixd: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedMultMatrixf(const float *matrix)
{
    uint32_t error;

    rendererGlMultMatrixfDriver(matrix);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glMultMatrixf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNewList(uint32_t list, uint32_t mode)
{
    uint32_t error;

    rendererGlNewListDriver(list, mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNewList: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3b(int8_t x, int8_t y, int8_t z)
{
    uint32_t error;

    rendererGlNormal3bDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3b: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3bv(const int8_t *values)
{
    uint32_t error;

    rendererGlNormal3bvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3bv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3d(double x, double y, double z)
{
    uint32_t error;

    rendererGlNormal3dDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3dv(const double *values)
{
    uint32_t error;

    rendererGlNormal3dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3f(float x, float y, float z)
{
    uint32_t error;

    rendererGlNormal3fDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3fv(const float *values)
{
    uint32_t error;

    rendererGlNormal3fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3i(
    int32_t x, int32_t y, int32_t z)
{
    uint32_t error;

    rendererGlNormal3iDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3iv(const int32_t *values)
{
    uint32_t error;

    rendererGlNormal3ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3s(
    int16_t x, int16_t y, int16_t z)
{
    uint32_t error;

    rendererGlNormal3sDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormal3sv(const int16_t *values)
{
    uint32_t error;

    rendererGlNormal3svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormal3sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedNormalPointer(
    uint32_t type, int32_t stride, const void *pointer)
{
    uint32_t error;

    rendererGlNormalPointerDriver(type, stride, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glNormalPointer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedOrtho(
    double left, double right, double bottom, double top,
    double nearValue, double farValue)
{
    uint32_t error;

    rendererGlOrthoDriver(left, right, bottom, top, nearValue, farValue);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glOrtho: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPassThrough(float token)
{
    uint32_t error;

    rendererGlPassThroughDriver(token);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPassThrough: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPixelMapfv(
    uint32_t map, int32_t mapSize, const float *values)
{
    uint32_t error;

    rendererGlPixelMapfvDriver(map, mapSize, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPixelMapfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPixelMapuiv(
    uint32_t map, int32_t mapSize, const uint32_t *values)
{
    uint32_t error;

    rendererGlPixelMapuivDriver(map, mapSize, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPixelMapuiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPixelMapusv(
    uint32_t map, int32_t mapSize, const uint16_t *values)
{
    uint32_t error;

    rendererGlPixelMapusvDriver(map, mapSize, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPixelMapusv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPixelStoref(
    uint32_t parameter, float value)
{
    uint32_t error;

    rendererGlPixelStorefDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPixelStoref: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPixelStorei(
    uint32_t parameter, int32_t value)
{
    uint32_t error;

    rendererGlPixelStoreiDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPixelStorei: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d4c40..0x004d4ef0. Checked pixel-transfer,
 * point/polygon, attribute-stack, matrix-stack, and selection-name dispatch.
 * PolygonMode is the two-enum OpenGL entry immediately between PointSize and
 * PolygonOffset; its operation string and driver slot independently identify
 * the function. */
void RENDERER_GL_API_CALL GL_CheckedPixelTransferf(
    uint32_t parameter, float value)
{
    uint32_t error;

    rendererGlPixelTransferfDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPixelTransferf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPixelTransferi(
    uint32_t parameter, int32_t value)
{
    uint32_t error;

    rendererGlPixelTransferiDriver(parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPixelTransferi: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPixelZoom(float xFactor, float yFactor)
{
    uint32_t error;

    rendererGlPixelZoomDriver(xFactor, yFactor);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPixelZoom: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPointSize(float size)
{
    uint32_t error;

    rendererGlPointSizeDriver(size);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPointSize: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPolygonMode(
    uint32_t face, uint32_t mode)
{
    uint32_t error;

    rendererGlPolygonModeDriver(face, mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPolygonMode: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPolygonOffset(float factor, float units)
{
    uint32_t error;

    rendererGlPolygonOffsetDriver(factor, units);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPolygonOffset: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPolygonStipple(const uint8_t *mask)
{
    uint32_t error;

    rendererGlPolygonStippleDriver(mask);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPolygonStipple: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPopAttrib(void)
{
    uint32_t error;

    rendererGlPopAttribDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPopAttrib: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPopClientAttrib(void)
{
    uint32_t error;

    rendererGlPopClientAttribDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPopClientAttrib: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPopMatrix(void)
{
    uint32_t error;

    rendererGlPopMatrixDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPopMatrix: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPopName(void)
{
    uint32_t error;

    rendererGlPopNameDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPopName: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPrioritizeTextures(
    int32_t count, const uint32_t *textures, const float *priorities)
{
    uint32_t error;

    rendererGlPrioritizeTexturesDriver(count, textures, priorities);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPrioritizeTextures: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPushAttrib(uint32_t mask)
{
    uint32_t error;

    rendererGlPushAttribDriver(mask);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPushAttrib: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPushClientAttrib(uint32_t mask)
{
    uint32_t error;

    rendererGlPushClientAttribDriver(mask);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPushClientAttrib: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPushMatrix(void)
{
    uint32_t error;

    rendererGlPushMatrixDriver();
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPushMatrix: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedPushName(uint32_t name)
{
    uint32_t error;

    rendererGlPushNameDriver(name);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glPushName: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d4ef0..0x004d53e0. Complete checked RasterPos
 * family for dimensions 2, 3, and 4 and scalar/vector forms. The x87 copies
 * prove double-width scalar arguments; GLshort scalar arguments retain their
 * source widths even though stdcall reserves a dword for each argument. */
void RENDERER_GL_API_CALL GL_CheckedRasterPos2d(double x, double y)
{
    uint32_t error;
    rendererGlRasterPos2dDriver(x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos2d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos2dv(const double *values)
{
    uint32_t error;
    rendererGlRasterPos2dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos2dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos2f(float x, float y)
{
    uint32_t error;
    rendererGlRasterPos2fDriver(x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos2f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos2fv(const float *values)
{
    uint32_t error;
    rendererGlRasterPos2fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos2fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos2i(int32_t x, int32_t y)
{
    uint32_t error;
    rendererGlRasterPos2iDriver(x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos2i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos2iv(const int32_t *values)
{
    uint32_t error;
    rendererGlRasterPos2ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos2iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos2s(int16_t x, int16_t y)
{
    uint32_t error;
    rendererGlRasterPos2sDriver(x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos2s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos2sv(const int16_t *values)
{
    uint32_t error;
    rendererGlRasterPos2svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos2sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos3d(
    double x, double y, double z)
{
    uint32_t error;
    rendererGlRasterPos3dDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos3d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos3dv(const double *values)
{
    uint32_t error;
    rendererGlRasterPos3dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos3dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos3f(float x, float y, float z)
{
    uint32_t error;
    rendererGlRasterPos3fDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos3f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos3fv(const float *values)
{
    uint32_t error;
    rendererGlRasterPos3fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos3fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos3i(
    int32_t x, int32_t y, int32_t z)
{
    uint32_t error;
    rendererGlRasterPos3iDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos3i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos3iv(const int32_t *values)
{
    uint32_t error;
    rendererGlRasterPos3ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos3iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos3s(
    int16_t x, int16_t y, int16_t z)
{
    uint32_t error;
    rendererGlRasterPos3sDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos3s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos3sv(const int16_t *values)
{
    uint32_t error;
    rendererGlRasterPos3svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos3sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos4d(
    double x, double y, double z, double w)
{
    uint32_t error;
    rendererGlRasterPos4dDriver(x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos4d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos4dv(const double *values)
{
    uint32_t error;
    rendererGlRasterPos4dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos4dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos4f(
    float x, float y, float z, float w)
{
    uint32_t error;
    rendererGlRasterPos4fDriver(x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos4f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos4fv(const float *values)
{
    uint32_t error;
    rendererGlRasterPos4fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos4fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos4i(
    int32_t x, int32_t y, int32_t z, int32_t w)
{
    uint32_t error;
    rendererGlRasterPos4iDriver(x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos4i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos4iv(const int32_t *values)
{
    uint32_t error;
    rendererGlRasterPos4ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos4iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos4s(
    int16_t x, int16_t y, int16_t z, int16_t w)
{
    uint32_t error;
    rendererGlRasterPos4sDriver(x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos4s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRasterPos4sv(const int16_t *values)
{
    uint32_t error;
    rendererGlRasterPos4svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRasterPos4sv: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d53e0..0x004d5890. Checked framebuffer read,
 * rectangle, render-mode, transform, scissor, selection-buffer, shading, and
 * stencil dispatch. RenderMode preserves the signed driver result across the
 * separate glGetError call exactly as the original saves it in ESI. */
void RENDERER_GL_API_CALL GL_CheckedReadBuffer(uint32_t mode)
{
    uint32_t error;
    rendererGlReadBufferDriver(mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glReadBuffer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedReadPixels(
    int32_t x, int32_t y, int32_t width, int32_t height,
    uint32_t format, uint32_t type, void *pixels)
{
    uint32_t error;
    rendererGlReadPixelsDriver(x, y, width, height, format, type, pixels);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glReadPixels: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRectd(
    double x1, double y1, double x2, double y2)
{
    uint32_t error;
    rendererGlRectdDriver(x1, y1, x2, y2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRectd: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRectdv(
    const double *vertex1, const double *vertex2)
{
    uint32_t error;
    rendererGlRectdvDriver(vertex1, vertex2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRectdv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRectf(
    float x1, float y1, float x2, float y2)
{
    uint32_t error;
    rendererGlRectfDriver(x1, y1, x2, y2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRectf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRectfv(
    const float *vertex1, const float *vertex2)
{
    uint32_t error;
    rendererGlRectfvDriver(vertex1, vertex2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRectfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRecti(
    int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    uint32_t error;
    rendererGlRectiDriver(x1, y1, x2, y2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRecti: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRectiv(
    const int32_t *vertex1, const int32_t *vertex2)
{
    uint32_t error;
    rendererGlRectivDriver(vertex1, vertex2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRectiv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRects(
    int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    uint32_t error;
    rendererGlRectsDriver(x1, y1, x2, y2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRects: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRectsv(
    const int16_t *vertex1, const int16_t *vertex2)
{
    uint32_t error;
    rendererGlRectsvDriver(vertex1, vertex2);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRectsv: glGetError() = 0x%04x\n", error);
}

int32_t RENDERER_GL_API_CALL GL_CheckedRenderMode(uint32_t mode)
{
    int32_t result;
    uint32_t error;

    result = rendererGlRenderModeDriver(mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRenderMode: glGetError() = 0x%04x\n", error);
    return result;
}

void RENDERER_GL_API_CALL GL_CheckedRotated(
    double angle, double x, double y, double z)
{
    uint32_t error;
    rendererGlRotatedDriver(angle, x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRotated: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedRotatef(
    float angle, float x, float y, float z)
{
    uint32_t error;
    rendererGlRotatefDriver(angle, x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glRotatef: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedScaled(double x, double y, double z)
{
    uint32_t error;
    rendererGlScaledDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glScaled: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedScalef(float x, float y, float z)
{
    uint32_t error;
    rendererGlScalefDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glScalef: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedScissor(
    int32_t x, int32_t y, int32_t width, int32_t height)
{
    uint32_t error;
    rendererGlScissorDriver(x, y, width, height);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glScissor: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedSelectBuffer(
    int32_t size, uint32_t *buffer)
{
    uint32_t error;
    rendererGlSelectBufferDriver(size, buffer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glSelectBuffer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedShadeModel(uint32_t mode)
{
    uint32_t error;
    rendererGlShadeModelDriver(mode);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glShadeModel: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedStencilFunc(
    uint32_t function, int32_t reference, uint32_t mask)
{
    uint32_t error;
    rendererGlStencilFuncDriver(function, reference, mask);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glStencilFunc: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedStencilMask(uint32_t mask)
{
    uint32_t error;
    rendererGlStencilMaskDriver(mask);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glStencilMask: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedStencilOp(
    uint32_t stencilFail, uint32_t depthFail, uint32_t depthPass)
{
    uint32_t error;
    rendererGlStencilOpDriver(stencilFail, depthFail, depthPass);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glStencilOp: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d5890..0x004d5ba0. Complete checked texture-
 * coordinate families for dimensions 1 and 2. Scalar double calls retain the
 * x87-proven width; GLshort scalar calls retain their OpenGL source type. */
void RENDERER_GL_API_CALL GL_CheckedTexCoord1d(double s)
{
    uint32_t error;
    rendererGlTexCoord1dDriver(s);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord1d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord1dv(const double *values)
{
    uint32_t error;
    rendererGlTexCoord1dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord1dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord1f(float s)
{
    uint32_t error;
    rendererGlTexCoord1fDriver(s);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord1f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord1fv(const float *values)
{
    uint32_t error;
    rendererGlTexCoord1fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord1fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord1i(int32_t s)
{
    uint32_t error;
    rendererGlTexCoord1iDriver(s);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord1i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord1iv(const int32_t *values)
{
    uint32_t error;
    rendererGlTexCoord1ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord1iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord1s(int16_t s)
{
    uint32_t error;
    rendererGlTexCoord1sDriver(s);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord1s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord1sv(const int16_t *values)
{
    uint32_t error;
    rendererGlTexCoord1svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord1sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord2d(double s, double t)
{
    uint32_t error;
    rendererGlTexCoord2dDriver(s, t);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord2d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord2dv(const double *values)
{
    uint32_t error;
    rendererGlTexCoord2dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord2dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord2f(float s, float t)
{
    uint32_t error;
    rendererGlTexCoord2fDriver(s, t);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord2f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord2fv(const float *values)
{
    uint32_t error;
    rendererGlTexCoord2fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord2fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord2i(int32_t s, int32_t t)
{
    uint32_t error;
    rendererGlTexCoord2iDriver(s, t);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord2i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord2iv(const int32_t *values)
{
    uint32_t error;
    rendererGlTexCoord2ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord2iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord2s(int16_t s, int16_t t)
{
    uint32_t error;
    rendererGlTexCoord2sDriver(s, t);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord2s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord2sv(const int16_t *values)
{
    uint32_t error;
    rendererGlTexCoord2svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord2sv: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d5ba0..0x004d5f00. Complete checked texture-
 * coordinate families for dimensions 3 and 4. Scalar double calls retain the
 * x87-proven width; GLshort scalar calls retain their OpenGL source type. */
void RENDERER_GL_API_CALL GL_CheckedTexCoord3d(
    double s, double t, double r)
{
    uint32_t error;
    rendererGlTexCoord3dDriver(s, t, r);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord3d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord3dv(const double *values)
{
    uint32_t error;
    rendererGlTexCoord3dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord3dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord3f(float s, float t, float r)
{
    uint32_t error;
    rendererGlTexCoord3fDriver(s, t, r);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord3f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord3fv(const float *values)
{
    uint32_t error;
    rendererGlTexCoord3fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord3fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord3i(
    int32_t s, int32_t t, int32_t r)
{
    uint32_t error;
    rendererGlTexCoord3iDriver(s, t, r);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord3i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord3iv(const int32_t *values)
{
    uint32_t error;
    rendererGlTexCoord3ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord3iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord3s(
    int16_t s, int16_t t, int16_t r)
{
    uint32_t error;
    rendererGlTexCoord3sDriver(s, t, r);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord3s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord3sv(const int16_t *values)
{
    uint32_t error;
    rendererGlTexCoord3svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord3sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord4d(
    double s, double t, double r, double q)
{
    uint32_t error;
    rendererGlTexCoord4dDriver(s, t, r, q);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord4d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord4dv(const double *values)
{
    uint32_t error;
    rendererGlTexCoord4dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord4dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord4f(
    float s, float t, float r, float q)
{
    uint32_t error;
    rendererGlTexCoord4fDriver(s, t, r, q);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord4f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord4fv(const float *values)
{
    uint32_t error;
    rendererGlTexCoord4fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord4fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord4i(
    int32_t s, int32_t t, int32_t r, int32_t q)
{
    uint32_t error;
    rendererGlTexCoord4iDriver(s, t, r, q);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord4i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord4iv(const int32_t *values)
{
    uint32_t error;
    rendererGlTexCoord4ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord4iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord4s(
    int16_t s, int16_t t, int16_t r, int16_t q)
{
    uint32_t error;
    rendererGlTexCoord4sDriver(s, t, r, q);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord4s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexCoord4sv(const int16_t *values)
{
    uint32_t error;
    rendererGlTexCoord4svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoord4sv: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d5f00..0x004d6130. Checked texture-coordinate
 * array, texture-environment, and texture-coordinate-generation entry points. */
void RENDERER_GL_API_CALL GL_CheckedTexCoordPointer(
    int32_t size, uint32_t type, int32_t stride, const void *pointer)
{
    uint32_t error;
    rendererGlTexCoordPointerDriver(size, type, stride, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexCoordPointer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexEnvf(
    uint32_t target, uint32_t parameter, float value)
{
    uint32_t error;
    rendererGlTexEnvfDriver(target, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexEnvf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexEnvfv(
    uint32_t target, uint32_t parameter, const float *values)
{
    uint32_t error;
    rendererGlTexEnvfvDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexEnvfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexEnvi(
    uint32_t target, uint32_t parameter, int32_t value)
{
    uint32_t error;
    rendererGlTexEnviDriver(target, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexEnvi: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexEnviv(
    uint32_t target, uint32_t parameter, const int32_t *values)
{
    uint32_t error;
    rendererGlTexEnvivDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexEnviv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexGend(
    uint32_t coordinate, uint32_t parameter, double value)
{
    uint32_t error;
    rendererGlTexGendDriver(coordinate, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexGend: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexGendv(
    uint32_t coordinate, uint32_t parameter, const double *values)
{
    uint32_t error;
    rendererGlTexGendvDriver(coordinate, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexGendv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexGenf(
    uint32_t coordinate, uint32_t parameter, float value)
{
    uint32_t error;
    rendererGlTexGenfDriver(coordinate, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexGenf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexGenfv(
    uint32_t coordinate, uint32_t parameter, const float *values)
{
    uint32_t error;
    rendererGlTexGenfvDriver(coordinate, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexGenfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexGeni(
    uint32_t coordinate, uint32_t parameter, int32_t value)
{
    uint32_t error;
    rendererGlTexGeniDriver(coordinate, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexGeni: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexGeniv(
    uint32_t coordinate, uint32_t parameter, const int32_t *values)
{
    uint32_t error;
    rendererGlTexGenivDriver(coordinate, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexGeniv: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d6130..0x004d6330. Checked texture image upload,
 * texture-parameter, and texture sub-image upload entry points. */
void RENDERER_GL_API_CALL GL_CheckedTexImage1D(
    uint32_t target, int32_t level, int32_t internalFormat, int32_t width,
    int32_t border, uint32_t format, uint32_t type, const void *pixels)
{
    uint32_t error;
    rendererGlTexImage1DDriver(
        target, level, internalFormat, width, border, format, type, pixels);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexImage1D: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexImage2D(
    uint32_t target, int32_t level, int32_t internalFormat, int32_t width,
    int32_t height, int32_t border, uint32_t format, uint32_t type,
    const void *pixels)
{
    uint32_t error;
    rendererGlTexImage2DDriver(
        target, level, internalFormat, width, height, border, format, type,
        pixels);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexImage2D: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexParameterf(
    uint32_t target, uint32_t parameter, float value)
{
    uint32_t error;
    rendererGlTexParameterfDriver(target, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexParameterf: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexParameterfv(
    uint32_t target, uint32_t parameter, const float *values)
{
    uint32_t error;
    rendererGlTexParameterfvDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexParameterfv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexParameteri(
    uint32_t target, uint32_t parameter, int32_t value)
{
    uint32_t error;
    rendererGlTexParameteriDriver(target, parameter, value);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexParameteri: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexParameteriv(
    uint32_t target, uint32_t parameter, const int32_t *values)
{
    uint32_t error;
    rendererGlTexParameterivDriver(target, parameter, values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexParameteriv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexSubImage1D(
    uint32_t target, int32_t level, int32_t xOffset, int32_t width,
    uint32_t format, uint32_t type, const void *pixels)
{
    uint32_t error;
    rendererGlTexSubImage1DDriver(
        target, level, xOffset, width, format, type, pixels);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexSubImage1D: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTexSubImage2D(
    uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
    int32_t width, int32_t height, uint32_t format, uint32_t type,
    const void *pixels)
{
    uint32_t error;
    rendererGlTexSubImage2DDriver(
        target, level, xOffset, yOffset, width, height, format, type, pixels);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTexSubImage2D: glGetError() = 0x%04x\n", error);
}

/* Source: CoDUOMP.exe 0x004d6330..0x004d6910. Checked translation, vertex,
 * vertex-array pointer, and viewport entry points. Scalar double calls retain
 * the x87-proven width; GLshort scalar calls retain their OpenGL source type. */
void RENDERER_GL_API_CALL GL_CheckedTranslated(double x, double y, double z)
{
    uint32_t error;
    rendererGlTranslatedDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTranslated: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedTranslatef(float x, float y, float z)
{
    uint32_t error;
    rendererGlTranslatefDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glTranslatef: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex2d(double x, double y)
{
    uint32_t error;
    rendererGlVertex2dDriver(x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex2d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex2dv(const double *values)
{
    uint32_t error;
    rendererGlVertex2dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex2dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex2f(float x, float y)
{
    uint32_t error;
    rendererGlVertex2fDriver(x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex2f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex2fv(const float *values)
{
    uint32_t error;
    rendererGlVertex2fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex2fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex2i(int32_t x, int32_t y)
{
    uint32_t error;
    rendererGlVertex2iDriver(x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex2i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex2iv(const int32_t *values)
{
    uint32_t error;
    rendererGlVertex2ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex2iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex2s(int16_t x, int16_t y)
{
    uint32_t error;
    rendererGlVertex2sDriver(x, y);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex2s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex2sv(const int16_t *values)
{
    uint32_t error;
    rendererGlVertex2svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex2sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex3d(double x, double y, double z)
{
    uint32_t error;
    rendererGlVertex3dDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex3d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex3dv(const double *values)
{
    uint32_t error;
    rendererGlVertex3dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex3dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex3f(float x, float y, float z)
{
    uint32_t error;
    rendererGlVertex3fDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex3f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex3fv(const float *values)
{
    uint32_t error;
    rendererGlVertex3fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex3fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex3i(int32_t x, int32_t y, int32_t z)
{
    uint32_t error;
    rendererGlVertex3iDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex3i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex3iv(const int32_t *values)
{
    uint32_t error;
    rendererGlVertex3ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex3iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex3s(
    int16_t x, int16_t y, int16_t z)
{
    uint32_t error;
    rendererGlVertex3sDriver(x, y, z);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex3s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex3sv(const int16_t *values)
{
    uint32_t error;
    rendererGlVertex3svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex3sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex4d(
    double x, double y, double z, double w)
{
    uint32_t error;
    rendererGlVertex4dDriver(x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex4d: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex4dv(const double *values)
{
    uint32_t error;
    rendererGlVertex4dvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex4dv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex4f(
    float x, float y, float z, float w)
{
    uint32_t error;
    rendererGlVertex4fDriver(x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex4f: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex4fv(const float *values)
{
    uint32_t error;
    rendererGlVertex4fvDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex4fv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex4i(
    int32_t x, int32_t y, int32_t z, int32_t w)
{
    uint32_t error;
    rendererGlVertex4iDriver(x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex4i: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex4iv(const int32_t *values)
{
    uint32_t error;
    rendererGlVertex4ivDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex4iv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex4s(
    int16_t x, int16_t y, int16_t z, int16_t w)
{
    uint32_t error;
    rendererGlVertex4sDriver(x, y, z, w);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex4s: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertex4sv(const int16_t *values)
{
    uint32_t error;
    rendererGlVertex4svDriver(values);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertex4sv: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedVertexPointer(
    int32_t size, uint32_t type, int32_t stride, const void *pointer)
{
    uint32_t error;
    rendererGlVertexPointerDriver(size, type, stride, pointer);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glVertexPointer: glGetError() = 0x%04x\n", error);
}

void RENDERER_GL_API_CALL GL_CheckedViewport(
    int32_t x, int32_t y, int32_t width, int32_t height)
{
    uint32_t error;
    rendererGlViewportDriver(x, y, width, height);
    error = rendererGlGetErrorDriver();
    if (error != GL_NO_ERROR)
        Com_Printf("^3glViewport: glGetError() = 0x%04x\n", error);
}
