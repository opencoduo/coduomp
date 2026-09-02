#include "renderer_api.h"

#include "../physics/cm_trace.h"
#include "../surface_types.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    R_PICK_SHADER_CONTENTS_MASK = 0x0f83fff7
};

#define R_PICK_SHADER_TRACE_DISTANCE 262144.0f /* 0x48800000: the positive world-bound sentinel */

/* Source: CoDUOMP.exe 0x004f0a10..0x004f0c39.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f0a10_004f0c3a.mcode.
 * Name and six-argument renderer export: exact same-module Mac symbol
 * RE_PickShader, renderer export slot 43, and cgame syscall 110's sole
 * Windows caller.
 *
 * materialName intentionally has no size argument in the original interface;
 * the traced collision material is copied with strcpy. bufferSize bounds both
 * diagnostic flag buffers. As in the shipped code, a truncated strncpy leaves
 * its last byte nonzero and makes the query fail rather than repairing the
 * partial output. */
qboolean RE_PickShader(const vec3_t start, const vec3_t direction, char *materialName, char *surfaceFlags, char *contents,
                       int32_t bufferSize)
{
    vec3_t end;
    trace_t trace;

    for (int32_t component = 0; component < 3; ++component) {
        end[component] = start[component] + direction[component] * R_PICK_SHADER_TRACE_DISTANCE;
    }

    trace.fraction = 1.0f;
    CM_BoxTrace(&trace, start, end, vec3_origin, vec3_origin, CM_WORLD_MODEL, R_PICK_SHADER_CONTENTS_MASK, qfalse);

    if (trace.startsolid != 0 || trace.allsolid != 0 || trace.fraction == 1.0f || trace.material == NULL) {
        return qfalse;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (memchr(trace.material, '\0', sizeof(((dshader_t *)0)->shader)) == NULL) {
        return qfalse;
    }
    strcpy(materialName, trace.material);

    surfaceFlags[0] = '\0';
    surfaceFlags[bufferSize - 1] = '\0';
    contents[0] = '\0';
    contents[bufferSize - 1] = '\0';

    const uint32_t surfaceType = ((uint32_t)trace.surfaceFlags >> SURFACE_TYPE_SHIFT) & SURFACE_TYPE_MASK;
    const char *const surfaceTypeName =
        surfaceType > SURFACE_TYPE_DEFAULT && surfaceType < SURFACE_TYPE_COUNT ? surfaceParms[surfaceType - 1].name : "^1default^7";

    strncpy(surfaceFlags, surfaceTypeName, (size_t)bufferSize);
    if (surfaceFlags[bufferSize - 1] != '\0') {
        return qfalse;
    }
    int32_t surfaceLength = (int32_t)strlen(surfaceFlags);

    const char *const solidityName = ((uint32_t)trace.contents & CONTENTS_SOLID) != 0U ? "solid" : "^3nonsolid^7";
    strncpy(contents, solidityName, (size_t)bufferSize);
    if (contents[bufferSize - 1] != '\0') {
        return qfalse;
    }
    int32_t contentsLength = (int32_t)strlen(contents);

    for (const surfaceParm_t *parm = &surfaceParms[SURFACE_PARM_GENERAL_FIRST]; parm->name != NULL; ++parm) {
        if (((uint32_t)trace.surfaceFlags & parm->surfaceFlags) != 0U) {
            surfaceFlags[surfaceLength++] = ' ';
            char *const destination = surfaceFlags + surfaceLength;
            strncpy(destination, parm->name, (size_t)(bufferSize - surfaceLength));
            if (surfaceFlags[bufferSize - 1] != '\0') {
                return qfalse;
            }
            surfaceLength += (int32_t)strlen(destination);
        }

        if (((uint32_t)trace.contents & parm->contents) != 0U) {
            contents[contentsLength++] = ' ';
            char *const destination = contents + contentsLength;
            strncpy(destination, parm->name, (size_t)(bufferSize - contentsLength));
            if (contents[bufferSize - 1] != '\0') {
                return qfalse;
            }
            contentsLength += (int32_t)strlen(destination);
        }
    }

    return qtrue;
}
