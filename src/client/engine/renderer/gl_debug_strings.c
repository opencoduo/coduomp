#include "gl_api.h"

#include "backend.h"
#include "gl_state.h"
#include "renderer_vbo.h"

#include <stdio.h>
#include <string.h>

/* Original 0x00f92fc8. The longest known result, "GL_TRIANGLE_STRIP", proves
 * all eighteen bytes including its terminator. No other binary reference
 * reaches beyond that extent. */
static char glPrimitiveModeString[18];

/* Original 0x027933d0. "0x" plus eight hexadecimal digits and the terminator
 * is the maximum fallback produced from a 32-bit GLenum. */
static char glEnumString[11];

/* Source: CoDUOMP.exe 0x004c9140..0x004c9159.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9140_004c9159.mcode.
 * Provisional name by exact role. The Windows LTCG body receives its byte
 * argument in AL and has no retained direct callers, while the same selection
 * is inlined into the debug qglIsEnabled wrapper at 0x004ca2a0. */
const char *GL_BooleanToString(uint8_t value)
{
    if (value == 0)
        return "GL_FALSE";
    if (value == 1)
        return "GL_TRUE";
    return "OUT OF RANGE FOR BOOLEAN";
}

/* Source: CoDUOMP.exe 0x004c9160..0x004c91a7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9160_004c91a7.mcode and its switch
 * table at 0x004c91a8..0x004c91c8. Provisional name by exact role. */
const char *GL_DepthFuncToString(uint32_t depthFunc)
{
    switch (depthFunc) {
    case GL_NEVER:
        return "GL_NEVER";
    case GL_LESS:
        return "GL_LESS";
    case GL_EQUAL:
        return "GL_EQUAL";
    case GL_LEQUAL:
        return "GL_LEQUAL";
    case GL_GREATER:
        return "GL_GREATER";
    case GL_NOTEQUAL:
        return "GL_NOTEQUAL";
    case GL_GEQUAL:
        return "GL_GEQUAL";
    case GL_ALWAYS:
        return "GL_ALWAYS";
    default:
        return "!!! UNKNOWN !!!";
    }
}

/* Source: CoDUOMP.exe 0x004c91d0..0x004c940e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c91d0_004c940e.mcode and direct
 * callers in the debug glBegin/glDrawArrays/glDrawElements wrappers.
 * Provisional name by exact role. The shared result buffer and hexadecimal
 * fallback are both observable original behavior. */
const char *GL_PrimitiveModeToString(uint32_t primitiveMode)
{
    const char *name;

    switch (primitiveMode) {
    case GL_POINTS:
        name = "GL_POINTS";
        break;
    case GL_LINES:
        name = "GL_LINES";
        break;
    case GL_LINE_LOOP:
        name = "GL_LINE_LOOP";
        break;
    case GL_LINE_STRIP:
        name = "GL_LINE_STRIP";
        break;
    case GL_TRIANGLES:
        name = "GL_TRIANGLES";
        break;
    case GL_TRIANGLE_STRIP:
        name = "GL_TRIANGLE_STRIP";
        break;
    case GL_TRIANGLE_FAN:
        name = "GL_TRIANGLE_FAN";
        break;
    case GL_QUADS:
        name = "GL_QUADS";
        break;
    case GL_QUAD_STRIP:
        name = "GL_QUAD_STRIP";
        break;
    case GL_POLYGON:
        name = "GL_POLYGON";
        break;
    default:
        sprintf(glPrimitiveModeString, "0x%x", primitiveMode);
        return glPrimitiveModeString;
    }

    strcpy(glPrimitiveModeString, name);
    return glPrimitiveModeString;
}

/* Source: CoDUOMP.exe 0x004c9410..0x004c983c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9410_004c983c.mcode and its
 * compiler lookup tables at 0x004c983c..0x004c9a34. This helper is called by
 * the debug OpenGL wrappers for capability, array, combiner, buffer, and
 * related GLenum arguments. Provisional name by exact role. */
const char *GL_EnumToString(uint32_t value)
{
    switch (value) {
    case GL_ADD: return "GL_ADD";
    case GL_SRC_COLOR: return "GL_SRC_COLOR";
    case GL_ONE_MINUS_SRC_COLOR: return "GL_ONE_MINUS_SRC_COLOR";
    case GL_SRC_ALPHA: return "GL_SRC_ALPHA";
    case GL_ONE_MINUS_SRC_ALPHA: return "GL_ONE_MINUS_SRC_ALPHA";
    case GL_FRONT: return "GL_FRONT";
    case GL_BACK: return "GL_BACK";
    case GL_FRONT_AND_BACK: return "GL_FRONT_AND_BACK";
    case GL_CULL_FACE: return "GL_CULL_FACE";
    case GL_LIGHTING: return "GL_LIGHTING";
    case GL_FOG: return "GL_FOG";
    case GL_DEPTH_TEST: return "GL_DEPTH_TEST";
    case GL_STENCIL_TEST: return "GL_STENCIL_TEST";
    case GL_NORMALIZE: return "GL_NORMALIZE";
    case GL_ALPHA_TEST: return "GL_ALPHA_TEST";
    case GL_BLEND: return "GL_BLEND";
    case GL_ALPHA_SCALE: return "GL_ALPHA_SCALE";
    case GL_TEXTURE_2D: return "GL_TEXTURE_2D";
    case GL_TEXTURE: return "GL_TEXTURE";
    case GL_POINT: return "GL_POINT";
    case GL_LINE: return "GL_LINE";
    case GL_FILL: return "GL_FILL";
    case GL_REPLACE: return "GL_REPLACE";
    case GL_MODULATE: return "GL_MODULATE";
    case GL_TEXTURE_ENV_MODE: return "GL_TEXTURE_ENV_MODE";
    case GL_TEXTURE_ENV: return "GL_TEXTURE_ENV";
    case GL_CLIP_PLANE0: return "GL_CLIP_PLANE0";
    case GL_LIGHT0: return "GL_LIGHT0";
    case GL_LIGHT1: return "GL_LIGHT1";
    case GL_LIGHT2: return "GL_LIGHT2";
    case GL_LIGHT3: return "GL_LIGHT3";
    case GL_LIGHT4: return "GL_LIGHT4";
    case GL_LIGHT5: return "GL_LIGHT5";
    case GL_LIGHT6: return "GL_LIGHT6";
    case GL_LIGHT7: return "GL_LIGHT7";
    case GL_POLYGON_OFFSET_FILL: return "GL_POLYGON_OFFSET_FILL";
    case GL_RESCALE_NORMAL_EXT: return "GL_RESCALE_NORMAL_EXT";
    case GL_VERTEX_ARRAY: return "GL_VERTEX_ARRAY";
    case GL_NORMAL_ARRAY: return "GL_NORMAL_ARRAY";
    case GL_COLOR_ARRAY: return "GL_COLOR_ARRAY";
    case GL_TEXTURE_COORD_ARRAY: return "GL_TEXTURE_COORD_ARRAY";
    case GL_TEXTURE_CUBE_MAP_ARB: return "GL_TEXTURE_CUBE_MAP_ARB";
    case GL_VERTEX_ARRAY_RANGE_NV: return "GL_VERTEX_ARRAY_RANGE_NV";
    case GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV:
        return "GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV";
    case GL_COMBINE_ARB: return "GL_COMBINE_ARB";
    case GL_COMBINE_RGB_ARB: return "GL_COMBINE_RGB_ARB";
    case GL_COMBINE_ALPHA_ARB: return "GL_COMBINE_ALPHA_ARB";
    case GL_RGB_SCALE_ARB: return "GL_RGB_SCALE_ARB";
    case GL_ADD_SIGNED_EXT: return "GL_ADD_SIGNED_EXT";
    case GL_INTERPOLATE_EXT: return "GL_INTERPOLATE_EXT";
    case GL_CONSTANT_EXT: return "GL_CONSTANT_EXT";
    case GL_PRIMARY_COLOR_EXT: return "GL_PRIMARY_COLOR_EXT";
    case GL_PREVIOUS_EXT: return "GL_PREVIOUS_EXT";
    case GL_SOURCE0_RGB_ARB: return "GL_SOURCE0_RGB_ARB";
    case GL_SOURCE1_RGB_ARB: return "GL_SOURCE1_RGB_ARB";
    case GL_SOURCE2_RGB_ARB: return "GL_SOURCE2_RGB_ARB";
    case GL_SOURCE0_ALPHA_ARB: return "GL_SOURCE0_ALPHA_ARB";
    case GL_SOURCE1_ALPHA_ARB: return "GL_SOURCE1_ALPHA_ARB";
    case GL_SOURCE2_ALPHA_ARB: return "GL_SOURCE2_ALPHA_ARB";
    case GL_OPERAND0_RGB_ARB: return "GL_OPERAND0_RGB_ARB";
    case GL_OPERAND1_RGB_ARB: return "GL_OPERAND1_RGB_ARB";
    case GL_OPERAND2_RGB_ARB: return "GL_OPERAND2_RGB_ARB";
    case GL_OPERAND0_ALPHA_ARB: return "GL_OPERAND0_ALPHA_ARB";
    case GL_OPERAND1_ALPHA_ARB: return "GL_OPERAND1_ALPHA_ARB";
    case GL_OPERAND2_ALPHA_ARB: return "GL_OPERAND2_ALPHA_ARB";
    case GL_VERTEX_PROGRAM_ARB: return "GL_VERTEX_PROGRAM_ARB";
    case GL_DOT3_RGB_ARB: return "GL_DOT3_RGB_ARB";
    case GL_DOT3_RGBA_ARB: return "GL_DOT3_RGBA_ARB";
    case GL_ARRAY_BUFFER_ARB: return "GL_ARRAY_BUFFER_ARB";
    case GL_ELEMENT_ARRAY_BUFFER_ARB: return "GL_ELEMENT_ARRAY_BUFFER_ARB";
    case GL_READ_ONLY_ARB: return "GL_READ_ONLY_ARB";
    case GL_WRITE_ONLY_ARB: return "GL_WRITE_ONLY_ARB";
    case GL_READ_WRITE_ARB: return "GL_READ_WRITE_ARB";
    case GL_STREAM_DRAW_ARB: return "GL_STREAM_DRAW_ARB";
    case GL_STREAM_READ_ARB: return "GL_STREAM_READ_ARB";
    case GL_STREAM_COPY_ARB: return "GL_STREAM_COPY_ARB";
    case GL_STATIC_DRAW_ARB: return "GL_STATIC_DRAW_ARB";
    case GL_STATIC_READ_ARB: return "GL_STATIC_READ_ARB";
    case GL_STATIC_COPY_ARB: return "GL_STATIC_COPY_ARB";
    case GL_DYNAMIC_DRAW_ARB: return "GL_DYNAMIC_DRAW_ARB";
    case GL_DYNAMIC_READ_ARB: return "GL_DYNAMIC_READ_ARB";
    case GL_DYNAMIC_COPY_ARB: return "GL_DYNAMIC_COPY_ARB";
    case GL_FRAGMENT_SHADER_ATI: return "GL_FRAGMENT_SHADER_ATI";
    default:
        sprintf(glEnumString, "0x%x", value);
        return glEnumString;
    }
}

/* Source: CoDUOMP.exe 0x004c9a40..0x004c9a87.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9a40_004c9a87.mcode and its
 * compiler switch table at 0x004c9a88..0x004c9ab4. The debug OpenGL wrappers
 * call this helper for vertex and index element types. Values GL_2_BYTES,
 * GL_3_BYTES, and GL_4_BYTES deliberately take the same unknown path as every
 * value outside the recognized scalar types. Provisional name by exact role. */
const char *GL_TypeToString(uint32_t type)
{
    switch (type) {
    case GL_BYTE: return "GL_BYTE";
    case GL_UNSIGNED_BYTE: return "GL_UNSIGNED_BYTE";
    case GL_SHORT: return "GL_SHORT";
    case GL_UNSIGNED_SHORT: return "GL_UNSIGNED_SHORT";
    case GL_INT: return "GL_INT";
    case GL_UNSIGNED_INT: return "GL_UNSIGNED_INT";
    case GL_FLOAT: return "GL_FLOAT";
    case GL_DOUBLE: return "GL_DOUBLE";
    default: return "!!! UNKNOWN !!!";
    }
}

/* Source: CoDUOMP.exe 0x004c9ac0..0x004c9b3b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9ac0_004c9b3b.mcode. The debug
 * OpenGL pointer wrappers call this with either a CPU address or the scalar
 * byte offset convention used while an ARB vertex buffer is bound. Converting
 * to uintptr_t is therefore confined to this genuine pointer-or-offset API
 * boundary. Provisional name by exact role. */
const char *GL_MemoryPointerToString(const void *pointerOrOffset)
{
    const uintptr_t value = (uintptr_t)pointerOrOffset;
    const uintptr_t tessBegin = (uintptr_t)(const void *)&tess;
    const uintptr_t tessEnd = tessBegin + sizeof(tess);

    if (value >= tessBegin && value < tessEnd)
        return "TESS";

    if (glConfig.vertexBufferObjectAvailable != qfalse) {
        if (value < 1024U * 1024U)
            return va("VBO_OFFSET(%i)", (int32_t)value);
        return "MEM";
    }

    if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
        const uintptr_t secondaryBegin =
            (uintptr_t)tr.staticVertexMemorySecondary.address;
        const uintptr_t secondaryEnd =
            secondaryBegin + tr.staticVertexMemorySecondaryLimit;
        const uintptr_t primaryBegin =
            (uintptr_t)tr.staticVertexMemoryPrimary.address;
        const uintptr_t primaryEnd =
            primaryBegin + tr.staticVertexMemoryPrimaryLimit;

        if (value >= secondaryBegin && value < secondaryEnd)
            return "AGP_MEM";
        if (value >= primaryBegin && value < primaryEnd)
            return "VID_MEM";
    }

    return "MEM";
}

/* Source: CoDUOMP.exe 0x004c9e10..0x004c9f16.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9e10_004c9f16.mcode. The helper
 * copies a known blend-factor name into caller storage and formats every
 * other GLenum as lowercase hexadecimal. Provisional name by exact role. */
void GL_BlendFactorToString(uint32_t factor, char *buffer)
{
    const char *name;

    switch (factor) {
    case GL_ZERO: name = "GL_ZERO"; break;
    case GL_ONE: name = "GL_ONE"; break;
    case GL_SRC_ALPHA: name = "GL_SRC_ALPHA"; break;
    case GL_ONE_MINUS_SRC_ALPHA: name = "GL_ONE_MINUS_SRC_ALPHA"; break;
    case GL_DST_ALPHA: name = "GL_DST_ALPHA"; break;
    case GL_DST_COLOR: name = "GL_DST_COLOR"; break;
    case GL_ONE_MINUS_DST_COLOR: name = "GL_ONE_MINUS_DST_COLOR"; break;
    default:
        sprintf(buffer, "0x%x", factor);
        return;
    }

    strcpy(buffer, name);
}
