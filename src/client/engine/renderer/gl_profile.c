#include "gl_debug.h"

#include "backend.h"
#include "gl_state.h"
#include "renderer_cvars.h"

#include <string.h>

enum {
    R_DRAW_PROFILE_MIN_MODE = 2,
    R_DRAW_PROFILE_REPEAT_MODE_MAX = 32,
    R_DRAW_PROFILE_INDEX_CAPACITY = R_MAX_TESS_INDEXES * R_DRAW_PROFILE_REPEAT_MODE_MAX,
    R_DRAW_PROFILE_CALLS_PER_FRAME = 350,
    R_TRIANGLE_INDEX_COUNT = 3,
    R_DRAW_PROFILE_ABSOLUTE_TRIANGLE_MAX = R_DRAW_PROFILE_INDEX_CAPACITY / R_TRIANGLE_INDEX_COUNT
};

/* Original scratch storage begins at 0x00f933d0. Its source carrier uses the
 * exact 0x00c00000 upper bound checked by the wrapper, which equals
 * R_MAX_TESS_INDEXES * 32. The absolute-triangle mode's security gate below
 * derives its safe cvar maximum from this capacity. */
static uint16_t rendererDrawProfileIndexes[R_DRAW_PROFILE_INDEX_CAPACITY];

/* Original 0x0389fea4 and 0x0389fea8. */
static int32_t rendererDrawProfileFrame;
static int32_t rendererDrawProfileCallCount;

/* Original 0x00f933cc. */
static qboolean rendererDrawProfilingEnabled;

/* Source: CoDUOMP.exe 0x004db130..0x004db30d.
 * Provisional name by exact dispatch role. For repeat modes 2..32 the complete
 * source index list is repeated N times. Larger accepted values mean an
 * absolute triangle target: the original list is repeated or truncated to
 * mode*3 indices. At most 350 profiled calls are submitted per renderer frame;
 * backEnd.pc.drawnIndexCount is corrected to the count actually submitted. */
static void RENDERER_GL_API_CALL QGL_ProfileDrawElements(uint32_t mode, int32_t count, uint32_t type, const void *indexes)
{
    const uint16_t *sourceIndexes = (const uint16_t *)indexes;
    int32_t profileMode;
    int32_t submittedCount;

    if (backEnd.projection2D != qfalse) {
        rendererGlDrawElementsDriver(mode, count, type, indexes);
        return;
    }

    profileMode = r_profileDrawElements->integer;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (profileMode < R_DRAW_PROFILE_MIN_MODE || profileMode > R_DRAW_PROFILE_ABSOLUTE_TRIANGLE_MAX) {
        rendererGlDrawElementsDriver(mode, R_TRIANGLE_INDEX_COUNT, type, indexes);
        backEnd.pc.drawnIndexCount = (int32_t)((uint32_t)backEnd.pc.drawnIndexCount + (uint32_t)R_TRIANGLE_INDEX_COUNT - (uint32_t)count);
        return;
    }

    if (rendererDrawProfileFrame != tr.frameCount) {
        rendererDrawProfileFrame = tr.frameCount;
        rendererDrawProfileCallCount = 0;
    }
    rendererDrawProfileCallCount = (int32_t)((uint32_t)rendererDrawProfileCallCount + 1u);
    if (rendererDrawProfileCallCount > R_DRAW_PROFILE_CALLS_PER_FRAME) {
        backEnd.pc.drawnIndexCount = (int32_t)((uint32_t)backEnd.pc.drawnIndexCount - (uint32_t)count);
        return;
    }

    if (profileMode <= R_DRAW_PROFILE_REPEAT_MODE_MAX) {
        int32_t repeatIndex;
        uint16_t *destination = rendererDrawProfileIndexes;

        for (repeatIndex = 0; repeatIndex < profileMode; ++repeatIndex) {
            memcpy(destination, sourceIndexes, (size_t)count * sizeof(*sourceIndexes));
            destination += count;
        }
        submittedCount = profileMode * count;
        rendererGlDrawElementsDriver(mode, submittedCount, type, rendererDrawProfileIndexes);
        backEnd.pc.drawnIndexCount =
            (int32_t)((uint32_t)backEnd.pc.drawnIndexCount + ((uint32_t)r_profileDrawElements->integer - 1u) * (uint32_t)count);
    } else {
        int32_t remainingCount = profileMode * R_TRIANGLE_INDEX_COUNT;
        int32_t destinationIndex = 0;

        while (count <= remainingCount) {
            memcpy(&rendererDrawProfileIndexes[destinationIndex], sourceIndexes, (size_t)count * sizeof(*sourceIndexes));
            remainingCount -= count;
            destinationIndex += count;
        }
        if (remainingCount != 0) {
            memcpy(&rendererDrawProfileIndexes[destinationIndex], sourceIndexes, (size_t)remainingCount * sizeof(*sourceIndexes));
        }
        submittedCount = profileMode * R_TRIANGLE_INDEX_COUNT;
        rendererGlDrawElementsDriver(mode, submittedCount, type, rendererDrawProfileIndexes);
        backEnd.pc.drawnIndexCount =
            (int32_t)((uint32_t)backEnd.pc.drawnIndexCount + (uint32_t)r_profileDrawElements->integer * (uint32_t)R_TRIANGLE_INDEX_COUNT -
                      (uint32_t)count);
    }
}

/* Source: CoDUOMP.exe 0x004db310..0x004db367.
 * Provisional name by exact dispatch role. In the normal profiling modes the
 * core draw-elements path owns the synthetic draw, so this ATI submission is
 * suppressed and removed from the submitted-index counter. The fallback mode
 * submits exactly one triangle, matching the core wrapper above. */
static void RENDERER_GL_API_CALL QGL_ProfileDrawElementArrayATI(uint32_t mode, int32_t count)
{
    if (backEnd.projection2D != qfalse) {
        rendererGlDrawElementArrayATIDriver(mode, count);
        return;
    }

    if (r_profileDrawElements->integer >= R_DRAW_PROFILE_MIN_MODE) {
        /* SUB at 0x004db335 wraps the signed counter in 32 bits. */
        backEnd.pc.drawnIndexCount = (int32_t)((uint32_t)backEnd.pc.drawnIndexCount - (uint32_t)count);
        return;
    }

    rendererGlDrawElementArrayATIDriver(mode, R_TRIANGLE_INDEX_COUNT);
    /* SUB/ADD at 0x004db35b..0x004db35d both wrap in 32 bits. */
    backEnd.pc.drawnIndexCount = (int32_t)((uint32_t)backEnd.pc.drawnIndexCount + (uint32_t)R_TRIANGLE_INDEX_COUNT - (uint32_t)count);
}

/* Source: CoDUOMP.exe 0x004db370..0x004db3b9.
 * Installs/removes the two profiling wrappers without disturbing the
 * underlying driver entries. The Windows compiler passes enable in EAX at its
 * internal call sites; the maintained source uses the normal C ABI. */
void QGL_EnableDrawProfiling(qboolean enable)
{
    if (rendererDrawProfilingEnabled == enable)
        return;

    rendererDrawProfilingEnabled = enable;
    if (enable != qfalse) {
        qglDrawElements = QGL_ProfileDrawElements;
        qglDrawElementArrayATI = QGL_ProfileDrawElementArrayATI;
    } else {
        qglDrawElements = rendererGlDrawElementsDriver;
        qglDrawElementArrayATI = rendererGlDrawElementArrayATIDriver;
    }
}
