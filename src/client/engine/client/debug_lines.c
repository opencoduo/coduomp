#include "debug_lines.h"

#include "cgame.h"
#include "../scripting/script_runtime.h"

enum {
    CLIENT_DEBUG_STRING_LIMIT = 256,
    CLIENT_DEBUG_LINE_LIMIT = 4096
};

/* Source: CoDUOMP.exe 0x00417000..0x004170ce.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00417000_004170cf.mcode.
 * Name and argument roles: same-module Mac symbol/call graph
 * CL_AddDebugString. The Windows optimizer carries origin and color in EBX
 * and EDI; scale, text, and fromServer remain stack arguments. */
void CL_AddDebugString(const vec3_t origin, const vec4_t color, float scale, const char *text, qboolean fromServer)
{
    if (cls.rendererStarted == qfalse) {
        return;
    }

    cls.debugStringCapacity = CLIENT_DEBUG_STRING_LIMIT;
    if (cls.debugStringCount + 1 > cls.debugStringCapacity) {
        return;
    }

    if (cls.debugStrings == NULL) {
        cls.debugStrings = Z_MallocInternal(CLIENT_DEBUG_STRING_LIMIT * sizeof(cls.debugStrings[0]));
        cls.debugStringFromServer = Z_MallocInternal((size_t)cls.debugStringCapacity * sizeof(cls.debugStringFromServer[0]));
        cls.debugStringCount = 0;
    }

    client_debug_string_t *debugString = &cls.debugStrings[cls.debugStringCount];
    for (int component = 0; component < 3; ++component) {
        debugString->origin[component] = origin[component];
    }
    for (int component = 0; component < 4; ++component) {
        debugString->color[component] = color[component];
    }
    debugString->scale = scale;
    Q_strncpyz(debugString->text, text, sizeof(debugString->text));
    cls.debugStringFromServer[cls.debugStringCount] = (uint8_t)fromServer;
    ++cls.debugStringCount;
}

/* Source: CoDUOMP.exe 0x00417300..0x0041733d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00417300_0041733e.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_UpdateDebugData. The same operation is also inlined into the Windows
 * RB_SwapBuffers body at 0x004c0e02..0x004c0e3d. */
void CL_UpdateDebugData(void)
{
    if (cls.rendererStarted == qfalse)
        return;

    if (cls.debugStrings != NULL) {
        RE_LocateDebugStrings(cls.debugStrings, cls.debugStringCount);
    }
    if (cls.debugLines != NULL) {
        RE_LocateDebugLines(cls.debugLines, cls.debugLineCount);
    }
}

/* Source: CoDUOMP.exe 0x00417340..0x004173eb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00417340_004173ec.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_ShutdownDebugData. The free order follows the original machine code:
 * the three line allocations precede the two string allocations. */
void CL_ShutdownDebugData(void)
{
    if (cls.debugLines != NULL) {
        Z_FreeInternal(cls.debugLines);
        cls.debugLines = NULL;
    }
    if (cls.debugLineFromServer != NULL) {
        Z_FreeInternal(cls.debugLineFromServer);
        cls.debugLineFromServer = NULL;
    }
    if (cls.debugLineDurations != NULL) {
        Z_FreeInternal(cls.debugLineDurations);
        cls.debugLineDurations = NULL;
    }
    if (cls.debugStrings != NULL) {
        Z_FreeInternal(cls.debugStrings);
        cls.debugStrings = NULL;
    }
    if (cls.debugStringFromServer != NULL) {
        Z_FreeInternal(cls.debugStringFromServer);
        cls.debugStringFromServer = NULL;
    }

    cls.debugStringCapacity = 0;
    cls.debugStringCount = 0;
    cls.debugLineCapacity = 0;
    cls.debugLineCount = 0;
}

/* Source: CoDUOMP.exe 0x004170d0..0x004171ba.
 * Name and argument roles: same-module Mac symbol/call graph
 * CL_AddDebugLine and the Windows update consumer at 0x004171c0. The Windows
 * optimizer carries start/end/color in EBX/EDI/ESI and the final three
 * arguments on the stack; this source signature restores the ordinary C
 * boundary. */
void CL_AddDebugLine(const vec3_t start, const vec3_t end, const vec4_t color, qboolean depthTest, int32_t duration, qboolean fromServer)
{
    if (cls.rendererStarted == qfalse) {
        return;
    }

    cls.debugLineCapacity = CLIENT_DEBUG_LINE_LIMIT;
    if (cls.debugLineCount + 1 > cls.debugLineCapacity) {
        return;
    }

    if (cls.debugLines == NULL) {
        cls.debugLines = Z_MallocInternal(CLIENT_DEBUG_LINE_LIMIT * sizeof(cls.debugLines[0]));
        cls.debugLineFromServer = Z_MallocInternal((size_t)cls.debugLineCapacity * sizeof(cls.debugLineFromServer[0]));
        cls.debugLineDurations = Z_MallocInternal((size_t)cls.debugLineCapacity * sizeof(cls.debugLineDurations[0]));
        cls.debugLineCount = 0;
    }

    client_debug_line_t *line = &cls.debugLines[cls.debugLineCount];
    for (int component = 0; component < 3; ++component) {
        line->start[component] = start[component];
        line->end[component] = end[component];
    }
    for (int component = 0; component < 4; ++component) {
        line->color[component] = color[component];
    }
    line->depthTest = depthTest;
    cls.debugLineFromServer[cls.debugLineCount] = (uint8_t)fromServer;
    cls.debugLineDurations[cls.debugLineCount] = duration;
    ++cls.debugLineCount;
}

/* Source: CoDUOMP.exe 0x004171c0..0x004172fc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004171c0_004172fd.mcode.
 * Name and fromServer argument: exact same-module Mac symbol
 * CL_FlushDebugData. Strings from the selected source are removed each call;
 * selected lines instead count down their duration and are removed at zero. */
void CL_FlushDebugData(qboolean fromServer)
{
    if (cls.rendererStarted == qfalse)
        return;

    if (cls.debugStrings != NULL) {
        int32_t index = 0;
        while (index < cls.debugStringCount) {
            if (cls.debugStringFromServer[index] != (uint8_t)fromServer) {
                ++index;
                continue;
            }

            --cls.debugStringCount;
            cls.debugStringFromServer[index] = cls.debugStringFromServer[cls.debugStringCount];
            cls.debugStrings[index] = cls.debugStrings[cls.debugStringCount];
        }
        RE_LocateDebugStrings(cls.debugStrings, cls.debugStringCount);
    }

    if (cls.debugLines != NULL) {
        int32_t index = 0;
        while (index < cls.debugLineCount) {
            if (cls.debugLineFromServer[index] != (uint8_t)fromServer) {
                ++index;
                continue;
            }

            --cls.debugLineDurations[index];
            if (cls.debugLineDurations[index] > 0) {
                ++index;
                continue;
            }

            --cls.debugLineCount;
            cls.debugLineFromServer[index] = cls.debugLineFromServer[cls.debugLineCount];
            cls.debugLineDurations[index] = cls.debugLineDurations[cls.debugLineCount];
            cls.debugLines[index] = cls.debugLines[cls.debugLineCount];
        }
        RE_LocateDebugLines(cls.debugLines, cls.debugLineCount);
    }
}
