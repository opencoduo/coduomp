#include "renderer_api.h"

#include "gl_api.h"
#include "gl_state.h"

#include <stdint.h>

typedef enum pb_gl_query_e {
    PB_GL_QUERY_READ_PIXELS = 101,
    PB_GL_QUERY_VIDEO_WIDTH = 102,
    PB_GL_QUERY_VIDEO_HEIGHT = 103,
    PB_GL_QUERY_PIXEL_FORMAT = 104,
    PB_GL_QUERY_PIXEL_TYPE = 105
} pb_gl_query_t;

/* Source: CoDUOMP.exe 0x004c5510..0x004c5543.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5510_004c5544.mcode.
 * The five-entry switch table is preserved separately as compiler-emitted
 * data at 0x004c5544..0x004c5557. The same-module Mac PbGlQuery symbol proves
 * the source name and pointer return ABI; Windows independently proves each
 * query result, including the qglReadPixels dispatch pointer at 0x048993ac.
 * Pointer/integer conversions are intentional at this legacy PunkBuster ABI
 * boundary. */
void *PbGlQuery(int32_t query)
{
    switch ((pb_gl_query_t)query) {
    case PB_GL_QUERY_READ_PIXELS:
        return (void *)(uintptr_t)&qglReadPixels;
    case PB_GL_QUERY_VIDEO_WIDTH:
        return (void *)(uintptr_t)(uint32_t)glConfig.vidWidth;
    case PB_GL_QUERY_VIDEO_HEIGHT:
        return (void *)(uintptr_t)(uint32_t)glConfig.vidHeight;
    case PB_GL_QUERY_PIXEL_FORMAT:
        return (void *)(uintptr_t)GL_RGB;
    case PB_GL_QUERY_PIXEL_TYPE:
        return (void *)(uintptr_t)GL_UNSIGNED_BYTE;
    default:
        return NULL;
    }
}
