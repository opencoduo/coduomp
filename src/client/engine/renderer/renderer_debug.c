#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

#include "../client/debug_lines.h"
#include "../math/vector_math.h"
#include "../system_fatal.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

renderer_debug_state_t rendererDebugState;

/* Shared id renderer color constant used by model-debug and default-color
 * paths. Source data: CoDUOMP.exe 0x0058fc68..0x0058fc77. */
const vec4_t colorWhite = {1.0f, 1.0f, 1.0f, 1.0f};

#define R_SCALED_DEBUG_VIEW_DOT_BIAS \
    0.99500000476837158203125f /* 0x3f7eb852 */

#define R_PLUME_SWAY_RADIANS_PER_MILLISECOND \
    0.012566370889544487f /* 0x3c4de32e: 2*pi/500 ms */
#define R_PLUME_SWAY_DISTANCE \
    4.0f /* 0x40800000 */
#define R_PLUME_RISE_UNITS_PER_MILLISECOND \
    0.06400000303983688f /* 0x3d83126f: approximately 64 units/s */

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(renderer_debug_string_t) == 0x80,
               "renderer debug-string payload size changed");
_Static_assert(sizeof(client_debug_string_t) ==
                   sizeof(renderer_debug_string_t),
               "client/renderer debug-string layouts diverged");
_Static_assert(sizeof(client_debug_line_t) == sizeof(renderer_debug_line_t),
               "client/renderer debug-line layouts diverged");
#endif

/* Source: CoDUOMP.exe 0x004e7670..0x004e76f0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7670_004e76f1.mcode.
 * Name: exact same-module Mac symbol R_InitDebug. The REP STOSD proves that
 * all fields below belong to one contiguous 128-byte state object. */
void R_InitDebug(void)
{
    memset(&rendererDebugState, 0, sizeof(rendererDebugState));
    rendererDebugState.polygonVertexCapacity =
        R_DEBUG_POLYGON_VERTEX_CAPACITY;
    rendererDebugState.polygonCapacity = R_DEBUG_POLYGON_CAPACITY;
    rendererDebugState.stringCapacity = R_DEBUG_STRING_CAPACITY;
    rendererDebugState.lineCapacity = R_DEBUG_LINE_CAPACITY;
    rendererDebugState.plumeCapacity = R_PLUME_CAPACITY;
    rendererDebugState.immediateColor[0] = 1;
    rendererDebugState.immediateColor[1] = 1;
    rendererDebugState.immediateColor[2] = 1;
    rendererDebugState.immediateColor[3] = 1;
    rendererDebugState.immediateVertexCapacity =
        RB_IMMEDIATE_VERTEX_CAPACITY;
}

/* Source: CoDUOMP.exe 0x004e7700..0x004e77ac.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7700_004e77ad.mcode.
 * Name: exact same-module Mac symbol R_ShutdownDebug. The Windows body frees
 * these seven lazily allocated stores in this exact order and nulls only the
 * corresponding pointers. */
void R_ShutdownDebug(void)
{
    if (rendererDebugState.polygons != NULL) {
        free(rendererDebugState.polygons);
        rendererDebugState.polygons = NULL;
    }
    if (rendererDebugState.polygonVertices != NULL) {
        free(rendererDebugState.polygonVertices);
        rendererDebugState.polygonVertices = NULL;
    }
    if (rendererDebugState.strings != NULL) {
        free(rendererDebugState.strings);
        rendererDebugState.strings = NULL;
    }
    if (rendererDebugState.lines != NULL) {
        free(rendererDebugState.lines);
        rendererDebugState.lines = NULL;
    }
    if (rendererDebugState.immediateVertices != NULL) {
        free(rendererDebugState.immediateVertices);
        rendererDebugState.immediateVertices = NULL;
    }
    if (rendererDebugState.plumes != NULL) {
        free(rendererDebugState.plumes);
        rendererDebugState.plumes = NULL;
    }
    if (rendererDebugState.font != NULL) {
        free(rendererDebugState.font);
        rendererDebugState.font = NULL;
    }
}

/* Source: CoDUOMP.exe 0x004e77b0..0x004e77c3. This renderer export was
 * absent from Ghidra's function table and was recovered from the executable
 * gap. Name and two-argument ABI: same-module Mac symbol
 * RE_LocateDebugStrings and GetRefAPI export slot 50. */
void RE_LocateDebugStrings(const client_debug_string_t *strings,
                           int32_t stringCount)
{
    rendererDebugState.locatedStrings = strings;
    rendererDebugState.locatedStringCount = stringCount;
}

/* Source: CoDUOMP.exe 0x004e77d0..0x004e77e3. This renderer export was
 * absent from Ghidra's function table and was recovered from the executable
 * gap. Name and two-argument ABI: same-module Mac symbol
 * RE_LocateDebugLines and GetRefAPI export slot 51. */
void RE_LocateDebugLines(const client_debug_line_t *lines,
                         int32_t lineCount)
{
    rendererDebugState.locatedLines = lines;
    rendererDebugState.locatedLineCount = lineCount;
}

/* Source: CoDUOMP.exe 0x004e77f0..0x004e790d. This renderer export was
 * absent from Ghidra's function table and was recovered from the executable
 * gap. Name and argument order: exact same-module Mac symbol RE_AddPlume and
 * GetRefAPI export slot 52. The only producer supplies totalSpawnCount as the
 * integer label. The original 40-byte record initially leaves alpha +0x18
 * zero; RB_AddPlumeStrings owns that lane while the plume is live. */
void RE_AddPlume(const vec3_t origin, int32_t labelValue,
                 const vec3_t color, int32_t duration)
{
    renderer_plume_t *plume;

    if (rendererDebugState.plumes == NULL) {
        const uint32_t allocationSize =
            (uint32_t)rendererDebugState.plumeCapacity *
            (uint32_t)sizeof(*rendererDebugState.plumes);

        rendererDebugState.plumes = malloc((size_t)allocationSize);
        if (rendererDebugState.plumes == NULL) {
            Sys_OutOfMemory();
            return;
        }
        memset(rendererDebugState.plumes, 0, allocationSize);
        rendererDebugState.plumeCount = 0;
    }

    if (rendererDebugState.plumeCount == rendererDebugState.plumeCapacity)
        return;

    plume = &rendererDebugState.plumes[rendererDebugState.plumeCount];
    memcpy(plume->origin, origin, sizeof(plume->origin));
    memcpy(plume->color, color, sizeof(vec3_t));
    plume->labelValue = labelValue;
    plume->startTime = backEnd.refdef.time;
    plume->duration = duration;
    ++rendererDebugState.plumeCount;
}

/* Source: CoDUOMP.exe 0x004e70e0..0x004e7211.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e70e0_004e7212.mcode.
 * Name: exact same-module Mac symbol RB_AddPlumeStrings. Active plumes sway
 * four units along the view-right axis, rise at approximately 64 units per
 * second, and fade through the second half of their lifetime. */
void RB_AddPlumeStrings(void)
{
    int32_t plumeIndex = 0;

    while (plumeIndex < rendererDebugState.plumeCount) {
        renderer_plume_t *plume =
            &rendererDebugState.plumes[plumeIndex];
        const int32_t elapsed = (int32_t)(
            (uint32_t)backEnd.refdef.time -
            (uint32_t)plume->startTime);
        const int32_t doubledElapsed =
            (int32_t)((uint32_t)elapsed * 2U);
        float sway;
        vec3_t labelOrigin;

        if (elapsed < 0 || elapsed > plume->duration) {
            --rendererDebugState.plumeCount;
            *plume = rendererDebugState.plumes[rendererDebugState.plumeCount];
            continue;
        }

        plume->color[3] = 1.0f;
        if (doubledElapsed > plume->duration) {
            plume->color[3] = (float)(
                2.0L -
                ((long double)elapsed * 2.0L) /
                    (long double)plume->duration);
        }

        sway = sinf(
                   (float)elapsed *
                       R_PLUME_SWAY_RADIANS_PER_MILLISECOND +
                   (float)plumeIndex) *
               R_PLUME_SWAY_DISTANCE;
        labelOrigin[0] = plume->origin[0] +
                         backEnd.refdef.viewaxis[1][0] * sway;
        labelOrigin[1] = plume->origin[1] +
                         backEnd.refdef.viewaxis[1][1] * sway;
        labelOrigin[2] = plume->origin[2] +
                         backEnd.refdef.viewaxis[1][2] * sway +
                         (float)elapsed *
                             R_PLUME_RISE_UNITS_PER_MILLISECOND;

        R_AddDebugString(labelOrigin, plume->color, 0.5f,
                         va("%i", plume->labelValue));
        ++plumeIndex;
    }
}

/* Source: CoDUOMP.exe 0x004e7290..0x004e73e3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7290_004e73e4.mcode.
 * Name and parameter roles: same-module Mac symbol R_AddDebugPolygon. The
 * Windows body maintains a 512-record table over a separate 4096-vertex
 * vec3 array and allocates both stores together on first use. */
void R_AddDebugPolygon(const vec4_t color, int32_t pointCount,
                       const vec3_t *points)
{
    renderer_debug_polygon_t *polygon;
    const int32_t requestedVertexCount = (int32_t)(
        (uint32_t)rendererDebugState.polygonVertexCount +
        (uint32_t)pointCount);
    const int32_t requestedPolygonCount = (int32_t)(
        (uint32_t)rendererDebugState.polygonCount + 1U);

    if (requestedVertexCount > rendererDebugState.polygonVertexCapacity)
        return;
    if (requestedPolygonCount > rendererDebugState.polygonCapacity)
        return;

    if (rendererDebugState.polygons == NULL ||
        rendererDebugState.polygonVertices == NULL) {
        const uint32_t polygonBytes =
            (uint32_t)rendererDebugState.polygonCapacity *
            (uint32_t)sizeof(*rendererDebugState.polygons);
        const uint32_t vertexBytes =
            (uint32_t)rendererDebugState.polygonVertexCapacity *
            (uint32_t)sizeof(*rendererDebugState.polygonVertices);

        rendererDebugState.polygons = malloc((size_t)polygonBytes);
        if (rendererDebugState.polygons == NULL)
            Sys_OutOfMemory();
        memset(rendererDebugState.polygons, 0, (size_t)polygonBytes);

        rendererDebugState.polygonVertices = malloc((size_t)vertexBytes);
        if (rendererDebugState.polygonVertices == NULL)
            Sys_OutOfMemory();
        memset(rendererDebugState.polygonVertices, 0, (size_t)vertexBytes);
    }

    polygon = &rendererDebugState.polygons[rendererDebugState.polygonCount];
    polygon->firstVertex = rendererDebugState.polygonVertexCount;
    polygon->vertexCount = pointCount;
    memcpy(polygon->color, color, sizeof(polygon->color));
    rendererDebugState.polygonCount = requestedPolygonCount;

    memcpy(&rendererDebugState
                .polygonVertices[rendererDebugState.polygonVertexCount],
           points,
           (size_t)((uint32_t)pointCount *
                    (uint32_t)sizeof(*points)));
    rendererDebugState.polygonVertexCount = requestedVertexCount;
}

/* Source: CoDUOMP.exe 0x004e73f0..0x004e7494.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e73f0_004e7495.mcode.
 * Name and parameter roles: same-module Mac symbol R_AddDebugLine. */
void R_AddDebugLine(const vec3_t start, const vec3_t end,
                    const vec4_t color)
{
    renderer_debug_line_t *line;

    if (rendererDebugState.lineCount + 1 > rendererDebugState.lineCapacity)
        return;

    if (rendererDebugState.lines == NULL) {
        const size_t allocationSize =
            (size_t)rendererDebugState.lineCapacity *
            sizeof(*rendererDebugState.lines);

        rendererDebugState.lines = malloc(allocationSize);
        if (rendererDebugState.lines == NULL)
            Sys_OutOfMemory();
        memset(rendererDebugState.lines, 0, allocationSize);
    }

    line = &rendererDebugState.lines[rendererDebugState.lineCount];
    memcpy(line->start, start, sizeof(line->start));
    memcpy(line->end, end, sizeof(line->end));
    memcpy(line->color, color, sizeof(line->color));
    line->depthTest = qfalse;
    ++rendererDebugState.lineCount;
}

/* Source: CoDUOMP.exe 0x004e74a0..0x004e751e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e74a0_004e751f.mcode.
 * Name and parameter roles: same-module Mac symbol R_AddDebugBox. The edge
 * pairs are the exact 24 dwords at CoDUOMP.exe 0x00590f08..0x00590f67. */
void R_AddDebugBox(const vec3_t mins, const vec3_t maxs,
                   const vec4_t color)
{
    static const int32_t edgeVertexIndexes[12][2] = {
        {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3},
        {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7}
    };
    vec3_t corners[8];
    int32_t cornerIndex;
    int32_t edgeIndex;

    for (cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
        corners[cornerIndex][0] =
            (cornerIndex & 1) != 0 ? maxs[0] : mins[0];
        corners[cornerIndex][1] =
            (cornerIndex & 2) != 0 ? maxs[1] : mins[1];
        corners[cornerIndex][2] =
            (cornerIndex & 4) != 0 ? maxs[2] : mins[2];
    }

    for (edgeIndex = 0; edgeIndex < 12; ++edgeIndex) {
        R_AddDebugLine(corners[edgeVertexIndexes[edgeIndex][0]],
                       corners[edgeVertexIndexes[edgeIndex][1]], color);
    }
}

/* Source: CoDUOMP.exe 0x004e6830..0x004e6aea.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e6830_004e6aeb.mcode.
 * Name: exact same-module Mac symbol RB_DrawDebugPolys. Each polygon is first
 * drawn as an alpha-blended filled primitive, then redrawn as an opaque
 * wireframe at depth zero. The optional EXT lock brackets the one shared
 * vertex array exactly as in the Windows renderer. */
void RB_DrawDebugPolys(void)
{
    enum {
        R_DEBUG_POLYGON_FILL_STATE =
            GLS_DEPTHMASK_TRUE |
            GLS_SRCBLEND_SRC_ALPHA |
            GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA,
        R_DEBUG_POLYGON_WIREFRAME_STATE =
            GLS_DEPTHMASK_TRUE | GLS_POLYMODE_LINE
    };
    int32_t polygonIndex;

    if (rendererDebugState.polygonCount == 0)
        return;

    RB_SelectStorage(R_STATIC_VERTEX_MEMORY_HUNK);
    GL_Bind(tr.defaultImage);
    GL_ClientState(GLS_CLIENT_VERTEX_ARRAY);
    qglVertexPointer(3, GL_FLOAT, 0,
                     rendererDebugState.polygonVertices);

    qglLoadMatrixf(tr.viewParms.world.modelMatrix);
    qglMatrixMode(GL_PROJECTION);
    qglLoadMatrixf(tr.viewParms.projectionMatrix);
    qglMatrixMode(GL_MODELVIEW);

    R_FogOff();
    if (qglLockArraysEXT != NULL) {
        qglLockArraysEXT(0, rendererDebugState.polygonVertexCount);
    }
    GL_Cull(CT_TWO_SIDED);

    for (polygonIndex = 0;
         polygonIndex < rendererDebugState.polygonCount;
         ++polygonIndex) {
        const renderer_debug_polygon_t *polygon =
            &rendererDebugState.polygons[polygonIndex];

        GL_State(R_DEBUG_POLYGON_FILL_STATE);
        qglColor4fv(polygon->color);
        qglDrawArrays(GL_POLYGON, polygon->firstVertex,
                      polygon->vertexCount);

        GL_State(R_DEBUG_POLYGON_WIREFRAME_STATE);
        qglDepthRange(0.0, 0.0);
        qglColor3fv(polygon->color);
        qglDrawArrays(GL_POLYGON, polygon->firstVertex,
                      polygon->vertexCount);
        qglDepthRange(0.0, 1.0);
    }

    rendererDebugState.polygonCount = 0;
    rendererDebugState.polygonVertexCount = 0;
    if (qglUnlockArraysEXT != NULL)
        qglUnlockArraysEXT();
    R_FogOn();
}

/* Source: CoDUOMP.exe 0x004e6af0..0x004e6c6f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e6af0_004e6c70.mcode.
 * Name, argument roles, and source-level (lines, lineCount) order: exact
 * same-module Mac symbol RB_DrawDebugLines. The Windows optimizer inlines the
 * RB_gl* calls; the Mac call graph confirms their original boundaries. */
void RB_DrawDebugLines(const renderer_debug_line_t *lines,
                       int32_t lineCount)
{
    int32_t lineIndex;

    if (lineCount == 0)
        return;

    GL_Bind(tr.defaultImage);
    RB_BeginImmediateMode();
    qglLoadMatrixf(tr.viewParms.world.modelMatrix);
    qglMatrixMode(GL_PROJECTION);
    qglLoadMatrixf(tr.viewParms.projectionMatrix);
    qglMatrixMode(GL_MODELVIEW);

    for (lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        const renderer_debug_line_t *line = &lines[lineIndex];

        GL_State(GLS_DEPTHMASK_TRUE |
                 (line->depthTest == qfalse
                      ? GLS_DEPTHTEST_DISABLE
                      : 0));
        RB_glBegin(GL_LINES);
        RB_glColor4fv(line->color);
        RB_glVertex3fv(line->start);
        RB_glVertex3fv(line->end);
        RB_glEnd();
    }

    RB_EndImmediateMode();
}

/* Source: CoDUOMP.exe 0x004e6c70..0x004e70d6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e6c70_004e70d7.mcode.
 * Name, source-level (strings, stringCount) order, and calls: exact same-module
 * Mac symbol RB_DrawDebugStrings and its calls to RE_RegisterFont,
 * RB_SelectStorage, RB_BeginSurface, RB_AddQuadStampExt, and RB_EndSurface.
 * Windows machine code remains authoritative for the glyph geometry, color
 * conversion, shader changes, and dedicated backEnd.debugEntity selection. */
void RB_DrawDebugStrings(const renderer_debug_string_t *strings,
                         int32_t stringCount)
{
    enum {
        R_DEBUG_FONT_POINT_SIZE = 16,
        R_DEBUG_FONT_LOAD_MODE = 1,
        R_DEBUG_STRING_VERTEX_COMPONENT_COUNT = 3,
        R_INVALID_SHADER_HANDLE = -1
    };
    const uint8_t shadowColor[4] = {0, 0, 0, UINT8_MAX};
    int32_t currentShaderHandle = R_INVALID_SHADER_HANDLE;
    int32_t stringIndex;

    if (stringCount == 0)
        return;

    if (rendererDebugState.font == NULL) {
        rendererDebugState.font = malloc(sizeof(*rendererDebugState.font));
        if (rendererDebugState.font == NULL)
            Sys_OutOfMemory();
        memset(rendererDebugState.font, 0, sizeof(*rendererDebugState.font));
        RE_RegisterFont("fonts/normalFont", R_DEBUG_FONT_POINT_SIZE,
                        rendererDebugState.font, R_DEBUG_FONT_LOAD_MODE);
    }

    qglLoadMatrixf(tr.viewParms.world.modelMatrix);
    qglMatrixMode(GL_PROJECTION);
    qglLoadMatrixf(tr.viewParms.projectionMatrix);
    qglMatrixMode(GL_MODELVIEW);
    qglDepthRange(0.0, 0.0);

    for (stringIndex = 0; stringIndex < stringCount; ++stringIndex) {
        const renderer_debug_string_t *debugString = &strings[stringIndex];
        const unsigned char *text =
            (const unsigned char *)debugString->text;
        uint8_t color[4];
        vec3_t stringOrigin;
        vec3_t right;
        vec3_t up;
        vec3_t diagonalOffset;
        float halfScale;
        int32_t colorIndex;

        for (colorIndex = 0; colorIndex < 4; ++colorIndex) {
            float component = debugString->color[colorIndex];

            if (component < 0.0f)
                component = 0.0f;
            else if (component > 1.0f)
                component = 1.0f;
            color[colorIndex] =
                (uint8_t)(int32_t)(component * 255.0f);
        }

        halfScale = debugString->scale * 0.5f;
        stringOrigin[0] = debugString->origin[0];
        stringOrigin[1] = debugString->origin[1];
        stringOrigin[2] = debugString->origin[2];

        right[0] = backEnd.refdef.viewaxis[1][0] * halfScale;
        right[1] = backEnd.refdef.viewaxis[1][1] * halfScale;
        right[2] = backEnd.refdef.viewaxis[1][2] * halfScale;
        up[0] = backEnd.refdef.viewaxis[2][0] * halfScale;
        up[1] = backEnd.refdef.viewaxis[2][1] * halfScale;
        up[2] = backEnd.refdef.viewaxis[2][2] * halfScale;

        diagonalOffset[0] = (up[0] + right[0]) * -2.0f;
        diagonalOffset[1] = (up[1] + right[1]) * -2.0f;
        diagonalOffset[2] = (up[2] + right[2]) * -2.0f;

        while (*text != '\0') {
            glyphInfo_t *glyph =
                &rendererDebugState.font->glyphs[*text];
            vec3_t widthOffset;
            vec3_t heightOffset;
            vec3_t glyphOrigin;
            float advance;

            if (glyph->glyph != currentShaderHandle) {
                currentShaderHandle = glyph->glyph;
                backEnd.currentEntity = &backEnd.debugEntity;
                RB_SelectStorage(tr.defaultStorageMode);
                RB_BeginSurface(
                    tr.sortedShaders[currentShaderHandle],
                    R_DEBUG_STRING_VERTEX_COMPONENT_COUNT);
            }

            widthOffset[0] = (float)glyph->imageWidth * right[0];
            widthOffset[1] = (float)glyph->imageWidth * right[1];
            widthOffset[2] = (float)glyph->imageWidth * right[2];
            heightOffset[0] = (float)glyph->imageHeight * up[0];
            heightOffset[1] = (float)glyph->imageHeight * up[1];
            heightOffset[2] = (float)glyph->imageHeight * up[2];

            glyphOrigin[0] =
                stringOrigin[0] - (float)glyph->imageWidth * right[0];
            glyphOrigin[1] =
                stringOrigin[1] - (float)glyph->imageWidth * right[1];
            glyphOrigin[2] =
                stringOrigin[2] - (float)glyph->imageWidth * right[2];
            glyphOrigin[0] +=
                (glyph->top + glyph->top - (float)glyph->imageHeight) * up[0];
            glyphOrigin[1] +=
                (glyph->top + glyph->top - (float)glyph->imageHeight) * up[1];
            glyphOrigin[2] +=
                (glyph->top + glyph->top - (float)glyph->imageHeight) * up[2];
            glyphOrigin[0] += diagonalOffset[0];
            glyphOrigin[1] += diagonalOffset[1];
            glyphOrigin[2] += diagonalOffset[2];

            RB_AddQuadStampExt(glyphOrigin, widthOffset, heightOffset,
                               shadowColor,
                               glyph->s, glyph->t, glyph->s2, glyph->t2);
            glyphOrigin[0] -= diagonalOffset[0];
            glyphOrigin[1] -= diagonalOffset[1];
            glyphOrigin[2] -= diagonalOffset[2];
            RB_AddQuadStampExt(glyphOrigin, widthOffset, heightOffset, color,
                               glyph->s, glyph->t, glyph->s2, glyph->t2);

            advance = glyph->xSkip * -2.0f;
            stringOrigin[0] += right[0] * advance;
            stringOrigin[1] += right[1] * advance;
            stringOrigin[2] += right[2] * advance;
            ++text;
        }
    }

    RB_EndSurface();
    qglDepthRange(0.0, 1.0);
}

/* Source: CoDUOMP.exe 0x004e7220..0x004e7283.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7220_004e7284.mcode.
 * Name: exact same-module Mac symbol RB_DrawDebug. Located client records are
 * the same 44-byte line and 128-byte string layouts as the renderer stores. */
void RB_DrawDebug(void)
{
    RB_AddPlumeStrings();
    RB_DrawDebugPolys();
    RB_DrawDebugLines(rendererDebugState.lines,
                      rendererDebugState.lineCount);
    RB_DrawDebugLines(
        (const renderer_debug_line_t *)rendererDebugState.locatedLines,
        rendererDebugState.locatedLineCount);
    rendererDebugState.lineCount = 0;
    RB_DrawDebugStrings(rendererDebugState.strings,
                        rendererDebugState.stringCount);
    RB_DrawDebugStrings(
        (const renderer_debug_string_t *)rendererDebugState.locatedStrings,
        rendererDebugState.locatedStringCount);
    rendererDebugState.stringCount = 0;
}

/* Source: CoDUOMP.exe 0x004e7520..0x004e75cd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7520_004e75cd.mcode.
 * Name and argument roles: same-module Mac symbol R_AddDebugString and the
 * corresponding Mac call from R_ShowLightVisCachePoints. The Windows LTCG
 * body receives origin in EBX and color in EDI, then copies the final scale
 * and text arguments into one fixed 128-byte debug record. */
void R_AddDebugString(const vec3_t origin, const vec4_t color,
                      float scale, const char *text)
{
    renderer_debug_string_t *debugString;

    if (rendererDebugState.stringCount + 1 >
        rendererDebugState.stringCapacity)
        return;

    if (rendererDebugState.strings == NULL) {
        const size_t allocationSize =
            (size_t)rendererDebugState.stringCapacity *
            sizeof(*rendererDebugState.strings);

        rendererDebugState.strings = malloc(allocationSize);
        if (rendererDebugState.strings == NULL)
            Sys_OutOfMemory();
        memset(rendererDebugState.strings, 0, allocationSize);
    }

    debugString = &rendererDebugState.strings[rendererDebugState.stringCount];
    memcpy(debugString->origin, origin, sizeof(debugString->origin));
    memcpy(debugString->color, color, sizeof(debugString->color));
    debugString->scale = scale;
    strncpy(debugString->text, text, sizeof(debugString->text) - 1U);
    debugString->text[sizeof(debugString->text) - 1U] = '\0';
    ++rendererDebugState.stringCount;
}

/* Source: CoDUOMP.exe 0x004e75d0..0x004e7666.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e75d0_004e7667.mcode.
 * Name and three-argument interface: same-module Mac symbol
 * R_AddScaledDebugString. The scale grows only for points nearly collinear
 * with the view axis and is clamped to one exactly as in the x87 path. */
void R_AddScaledDebugString(const vec3_t origin, const vec4_t color,
                            const char *text)
{
    vec3_t viewDirection;
    long double distance;
    long double viewDot;
    float scale;

    viewDirection[0] = origin[0] - tr.viewParms.orientation.origin[0];
    viewDirection[1] = origin[1] - tr.viewParms.orientation.origin[1];
    viewDirection[2] = origin[2] - tr.viewParms.orientation.origin[2];
    distance = (long double)VectorNormalize(viewDirection);
    /* The x87 body accumulates Z, then Y, then X before subtracting the
     * collinearity bias. Preserve that exact association across hosts. */
    viewDot =
        ((long double)viewDirection[2] *
             (long double)tr.viewParms.orientation.axis[0][2] +
         (long double)viewDirection[1] *
             (long double)tr.viewParms.orientation.axis[0][1]) +
        (long double)viewDirection[0] *
            (long double)tr.viewParms.orientation.axis[0][0];
    scale = (float)(
        distance *
        (viewDot - (long double)R_SCALED_DEBUG_VIEW_DOT_BIAS));
    if (scale < 1.0f)
        scale = 1.0f;

    R_AddDebugString(origin, color, scale, text);
}
