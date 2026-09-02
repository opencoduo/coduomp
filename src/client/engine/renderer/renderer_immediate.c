#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "../system_fatal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The Windows helpers multiply by the exact 255.0f constant and pass the x87
 * value through MSVC's integer conversion helper before storing AL. */
#define RB_COLOR_BYTE(component_) ((uint8_t)(int32_t)((component_) * 255.0f))

/* Source: CoDUOMP.exe 0x004e7910..0x004e7af3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7910_004e7af4.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * RB_BeginImmediateMode. The Windows body proves the 8192-entry, 32-byte
 * vertex store and the client-array state installed below. */
void RB_BeginImmediateMode(void)
{
    if (rendererDebugState.immediateVertices == NULL) {
        const size_t allocationSize = (size_t)rendererDebugState.immediateVertexCapacity * sizeof(*rendererDebugState.immediateVertices);

        rendererDebugState.immediateVertices = malloc(allocationSize);
        if (rendererDebugState.immediateVertices == NULL) {
            Sys_OutOfMemory();
            return;
        }
        memset(rendererDebugState.immediateVertices, 0, allocationSize);
    }

    rendererDebugState.immediateColor[0] = UINT8_MAX;
    rendererDebugState.immediateColor[1] = UINT8_MAX;
    rendererDebugState.immediateColor[2] = UINT8_MAX;
    rendererDebugState.immediateColor[3] = UINT8_MAX;
    rendererDebugState.immediatePrimitiveMode = 0;
    rendererDebugState.immediateLineWidth = 0.0f;
    rendererDebugState.immediateTexCoord[0] = 0.0f;
    rendererDebugState.immediateTexCoord[1] = 0.0f;

    RB_SelectStorage(R_STATIC_VERTEX_MEMORY_HUNK);
    RB_EndMultitexture();
    GL_ClientState(GLS_CLIENT_TEXCOORD0_ARRAY | GLS_CLIENT_COLOR_ARRAY | GLS_CLIENT_VERTEX_ARRAY);
    R_FogOff();

    qglTexCoordPointer(2, GL_FLOAT, (int32_t)sizeof(*rendererDebugState.immediateVertices),
                       rendererDebugState.immediateVertices[0].texCoord);
    qglColorPointer(4, GL_UNSIGNED_BYTE, (int32_t)sizeof(*rendererDebugState.immediateVertices),
                    rendererDebugState.immediateVertices[0].color);
    qglNormal3f(0.0f, 0.0f, 1.0f);
    qglVertexPointer(RB_IMMEDIATE_VERTEX_COMPONENTS, GL_FLOAT, (int32_t)sizeof(*rendererDebugState.immediateVertices),
                     rendererDebugState.immediateVertices[0].xyz);
    rendererDebugState.immediateModeActive = qtrue;
}

/* Source: CoDUOMP.exe 0x004e7b00..0x004e7b23. This function was absent from
 * Ghidra's function table and was recovered from the executable gap. Name:
 * exact same-module Mac symbol RB_EndImmediateMode. */
void RB_EndImmediateMode(void)
{
    rendererDebugState.immediateModeActive = qfalse;
    R_FogOn();
    qglVertexPointer(RB_IMMEDIATE_VERTEX_COMPONENTS, GL_FLOAT, 0, tess.xyz);
}

/* Source: CoDUOMP.exe 0x004e7b30..0x004e7b35. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glBegin. */
void RB_glBegin(uint32_t mode)
{
    rendererDebugState.immediatePrimitiveMode = mode;
}

/* Source: CoDUOMP.exe 0x004e7b40..0x004e7b69. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glEnd. */
void RB_glEnd(void)
{
    qglDrawArrays(rendererDebugState.immediatePrimitiveMode, 0, rendererDebugState.immediateVertexCount);
    rendererDebugState.immediateVertexCount = 0;
    rendererDebugState.immediatePrimitiveMode = 0;
}

/* Source: CoDUOMP.exe 0x004e7bd0..0x004e7c38.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7bd0_004e7c39.mcode.
 * Name: exact same-module Mac symbol RB_glVertex3f. */
void RB_glVertex3f(float x, float y, float z)
{
    rb_immediate_vertex_t *vertex;

    if (rendererDebugState.immediateVertexCount >= rendererDebugState.immediateVertexCapacity)
        return;

    vertex = &rendererDebugState.immediateVertices[rendererDebugState.immediateVertexCount];
    memcpy(vertex->color, rendererDebugState.immediateColor, sizeof(vertex->color));
    vertex->texCoord[0] = rendererDebugState.immediateTexCoord[0];
    vertex->texCoord[1] = rendererDebugState.immediateTexCoord[1];
    vertex->xyz[0] = x;
    vertex->xyz[1] = y;
    vertex->xyz[2] = z;
    ++rendererDebugState.immediateVertexCount;
}

/* Source: CoDUOMP.exe 0x004e7c40..0x004e7c53. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glVertex3fv. */
void RB_glVertex3fv(const vec3_t vertex)
{
    RB_glVertex3f(vertex[0], vertex[1], vertex[2]);
}

/* Source: CoDUOMP.exe 0x004e7b70..0x004e7b8c. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glVertex2i. */
void RB_glVertex2i(int32_t x, int32_t y)
{
    RB_glVertex3f((float)x, (float)y, 0.0f);
}

/* Source: CoDUOMP.exe 0x004e7b90..0x004e7ba4. This emitted Windows helper is
 * the two-float counterpart of RB_glVertex2i; its body supplies a zero Z
 * coordinate to RB_glVertex3f. */
void RB_glVertex2f(float x, float y)
{
    RB_glVertex3f(x, y, 0.0f);
}

/* Source: CoDUOMP.exe 0x004e7bb0..0x004e7bc1. The Windows register-carried
 * body loads the two source floats and supplies a zero Z coordinate. */
void RB_glVertex2fv(const vec2_t vertex)
{
    RB_glVertex3f(vertex[0], vertex[1], 0.0f);
}

/* Source: CoDUOMP.exe 0x004e7c60..0x004e7c73. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glTexCoord2f. */
void RB_glTexCoord2f(float s, float t)
{
    rendererDebugState.immediateTexCoord[0] = s;
    rendererDebugState.immediateTexCoord[1] = t;
}

/* Source: CoDUOMP.exe 0x004e7c80..0x004e7c91. The Windows register-carried
 * body is the vector form of RB_glTexCoord2f. */
void RB_glTexCoord2fv(const vec2_t texCoord)
{
    rendererDebugState.immediateTexCoord[0] = texCoord[0];
    rendererDebugState.immediateTexCoord[1] = texCoord[1];
}

/* Source: CoDUOMP.exe 0x004e7ca0..0x004e7ce3. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glColor3f. */
void RB_glColor3f(float red, float green, float blue)
{
    rendererDebugState.immediateColor[0] = RB_COLOR_BYTE(red);
    rendererDebugState.immediateColor[1] = RB_COLOR_BYTE(green);
    rendererDebugState.immediateColor[2] = RB_COLOR_BYTE(blue);
    rendererDebugState.immediateColor[3] = UINT8_MAX;
}

/* Source: CoDUOMP.exe 0x004e7cf0..0x004e7d40.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7cf0_004e7d41.mcode.
 * Name: exact same-module Mac symbol RB_glColor4f. */
void RB_glColor4f(float red, float green, float blue, float alpha)
{
    rendererDebugState.immediateColor[0] = RB_COLOR_BYTE(red);
    rendererDebugState.immediateColor[1] = RB_COLOR_BYTE(green);
    rendererDebugState.immediateColor[2] = RB_COLOR_BYTE(blue);
    rendererDebugState.immediateColor[3] = RB_COLOR_BYTE(alpha);
}

/* Source: CoDUOMP.exe 0x004e7d50..0x004e7d8f. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glColor3fv. */
void RB_glColor3fv(const vec3_t color)
{
    RB_glColor3f(color[0], color[1], color[2]);
}

/* Source: CoDUOMP.exe 0x004e7d90..0x004e7ddb. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glColor4fv. */
void RB_glColor4fv(const vec4_t color)
{
    RB_glColor4f(color[0], color[1], color[2], color[3]);
}

/* Source: CoDUOMP.exe 0x004e7de0..0x004e7e05. Recovered from the executable
 * gap; exact same-module Mac symbol RB_glLineWidth. */
void RB_glLineWidth(float width)
{
    if (width == rendererDebugState.immediateLineWidth)
        return;

    rendererDebugState.immediateLineWidth = width;
    qglLineWidth(width);
}

#undef RB_COLOR_BYTE
