#include "renderer_gpu_profile.h"

#if defined(CODUOMP_RENDERER_GPU_PROFILE)

#include "backend.h"
#include "gl_api.h"
#include "wgl_debug.h"

#include <stdint.h>
#include <string.h>

enum {
    CODUOMP_GPU_PROFILE_QUERY_CAPACITY = 4096,
    CODUOMP_GPU_PROFILE_FRAME_CAPACITY = 64,
    CODUOMP_GPU_PROFILE_SHADER_CAPACITY = 64,
    CODUOMP_GPU_PROFILE_SHADER_NAME_CAPACITY = 64,
    CODUOMP_GPU_PROFILE_SUMMARY_FRAMES = 250,
    CODUOMP_GL_TIME_ELAPSED = 0x88bf
};

typedef void (RENDERER_GL_API_CALL *coduomp_gl_gen_queries_t)(
    int32_t count, uint32_t *queries);
typedef void (RENDERER_GL_API_CALL *coduomp_gl_delete_queries_t)(
    int32_t count, const uint32_t *queries);
typedef void (RENDERER_GL_API_CALL *coduomp_gl_begin_query_t)(
    uint32_t target, uint32_t query);
typedef void (RENDERER_GL_API_CALL *coduomp_gl_end_query_t)(uint32_t target);
typedef void (RENDERER_GL_API_CALL *coduomp_gl_get_query_object_uiv_t)(
    uint32_t query, uint32_t parameter, uint32_t *value);
typedef void (RENDERER_GL_API_CALL *coduomp_gl_get_query_object_ui64v_t)(
    uint32_t query, uint32_t parameter, uint64_t *value);

typedef struct coduomp_gpu_profile_shader_s {
    char name[CODUOMP_GPU_PROFILE_SHADER_NAME_CAPACITY];
    uint64_t nanoseconds;
} coduomp_gpu_profile_shader_t;

typedef struct coduomp_gpu_profile_frame_s {
    qboolean used;
    qboolean closed;
    uint32_t serial;
    int32_t mode;
    float slowMsec;
    uint32_t outstandingQueries;
    uint32_t droppedQueries;
    uint64_t phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_COUNT];
    coduomp_gpu_profile_shader_t
        shaders[CODUOMP_GPU_PROFILE_SHADER_CAPACITY];
    int32_t shaderCount;
    uint64_t untrackedShaderNanoseconds;
} coduomp_gpu_profile_frame_t;

typedef struct coduomp_gpu_profile_query_s {
    int32_t frameIndex;
    uint32_t frameSerial;
    coduomp_gpu_profile_phase_t phase;
    char shaderName[CODUOMP_GPU_PROFILE_SHADER_NAME_CAPACITY];
} coduomp_gpu_profile_query_t;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: this diagnostic state exists only
 * in builds compiled with CODUOMP_RENDERER_GPU_PROFILE. */
static cvar_t *coduompGpuProfileMode;
static cvar_t *coduompGpuProfileSlowMsec;
static coduomp_gl_gen_queries_t coduompGlGenQueries;
static coduomp_gl_delete_queries_t coduompGlDeleteQueries;
static coduomp_gl_begin_query_t coduompGlBeginQuery;
static coduomp_gl_end_query_t coduompGlEndQuery;
static coduomp_gl_get_query_object_uiv_t coduompGlGetQueryObjectuiv;
static coduomp_gl_get_query_object_ui64v_t coduompGlGetQueryObjectui64v;
static uint32_t
    coduompGpuProfileQueryIds[CODUOMP_GPU_PROFILE_QUERY_CAPACITY];
static coduomp_gpu_profile_query_t
    coduompGpuProfileQueries[CODUOMP_GPU_PROFILE_QUERY_CAPACITY];
static coduomp_gpu_profile_frame_t
    coduompGpuProfileFrames[CODUOMP_GPU_PROFILE_FRAME_CAPACITY];
static qboolean coduompGpuProfileApiAttempted;
static qboolean coduompGpuProfileApiReady;
static int32_t coduompGpuProfileQueryHead;
static int32_t coduompGpuProfileQueryTail;
static int32_t coduompGpuProfilePendingQueries;
static int32_t coduompGpuProfileActiveQuery = -1;
static int32_t coduompGpuProfileCurrentFrame = -1;
static uint32_t coduompGpuProfileNextFrameSerial;
static uint32_t coduompGpuProfileSummaryCount;
static uint64_t
    coduompGpuProfileSummaryNanoseconds[CODUOMP_GPU_PROFILE_PHASE_COUNT];
static uint64_t coduompGpuProfileSummaryMaximumNanoseconds;
static uint32_t coduompGpuProfileSummaryDroppedQueries;

/* NOT_FROM_ORIGINAL_SOURCE: return milliseconds without narrowing the stored
 * nanosecond result before conversion. */
static double coduomp_gpu_profile_msec(uint64_t nanoseconds)
{
    return (double)nanoseconds / 1000000.0;
}

/* NOT_FROM_ORIGINAL_SOURCE: calculate one phase's per-frame summary average. */
static double coduomp_gpu_profile_summary_phase_msec(
    coduomp_gpu_profile_phase_t phase)
{
    return coduomp_gpu_profile_msec(
               coduompGpuProfileSummaryNanoseconds[phase]) /
           (double)coduompGpuProfileSummaryCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: sum mutually exclusive profiling scopes for one
 * completed backend frame. */
static uint64_t coduomp_gpu_profile_frame_total(
    const coduomp_gpu_profile_frame_t *frame)
{
    uint64_t total = 0;

    for (int32_t phase = 0;
         phase < CODUOMP_GPU_PROFILE_PHASE_COUNT; ++phase) {
        total += frame->phaseNanoseconds[phase];
    }
    return total;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the three largest per-frame shader totals
 * without allocating or sorting renderer-owned records. */
static void coduomp_gpu_profile_find_top_shaders(
    const coduomp_gpu_profile_frame_t *frame, int32_t top[3])
{
    top[0] = -1;
    top[1] = -1;
    top[2] = -1;

    for (int32_t shader = 0; shader < frame->shaderCount; ++shader) {
        for (int32_t rank = 0; rank < 3; ++rank) {
            if (top[rank] < 0 ||
                frame->shaders[shader].nanoseconds >
                    frame->shaders[top[rank]].nanoseconds) {
                for (int32_t move = 2; move > rank; --move)
                    top[move] = top[move - 1];
                top[rank] = shader;
                break;
            }
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: emit a parseable slow-frame record and its most
 * expensive shader batches after every query for the frame has completed. */
static void coduomp_gpu_profile_print_frame(
    const coduomp_gpu_profile_frame_t *frame, uint64_t totalNanoseconds)
{
    int32_t top[3];

    coduomp_gpu_profile_find_top_shaders(frame, top);
    ri.Printf(
        R_PRINT_ALL,
        "GPU_PROFILE frame=%u total_ms=%.3f view_ms=%.3f world_ms=%.3f "
        "bmodel_ms=%.3f model_ms=%.3f smodel_ms=%.3f effects_ms=%.3f "
        "sky_ms=%.3f shadows_ms=%.3f flares_ms=%.3f 2d_ms=%.3f "
        "clear_ms=%.3f copy_ms=%.3f present_ms=%.3f misc_ms=%.3f "
        "dropped=%u\n",
        frame->serial, coduomp_gpu_profile_msec(totalNanoseconds),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_VIEW_SETUP]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_WORLD]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_BRUSH_MODELS]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_MODELS]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_STATIC_MODELS]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_EFFECTS]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_SKY]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_SHADOWS]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_FLARES]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_2D]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_CLEAR]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_SCREEN_COPY]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_PRESENT]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_MISC]),
        frame->droppedQueries);

    if (top[0] >= 0) {
        ri.Printf(
            R_PRINT_ALL,
            "GPU_PROFILE_TOP frame=%u shader1=%s:%.3f shader2=%s:%.3f "
            "shader3=%s:%.3f untracked_ms=%.3f\n",
            frame->serial,
            frame->shaders[top[0]].name,
            coduomp_gpu_profile_msec(
                frame->shaders[top[0]].nanoseconds),
            top[1] >= 0 ? frame->shaders[top[1]].name : "-",
            top[1] >= 0
                ? coduomp_gpu_profile_msec(
                      frame->shaders[top[1]].nanoseconds)
                : 0.0,
            top[2] >= 0 ? frame->shaders[top[2]].name : "-",
            top[2] >= 0
                ? coduomp_gpu_profile_msec(
                      frame->shaders[top[2]].nanoseconds)
                : 0.0,
            coduomp_gpu_profile_msec(
                frame->untrackedShaderNanoseconds));
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: periodically report average phase costs even when
 * no individual frame exceeds the configured slow-frame threshold. */
static void coduomp_gpu_profile_print_summary(void)
{
    uint64_t totalNanoseconds = 0;

    for (int32_t phase = 0;
         phase < CODUOMP_GPU_PROFILE_PHASE_COUNT; ++phase) {
        totalNanoseconds += coduompGpuProfileSummaryNanoseconds[phase];
    }

    ri.Printf(
        R_PRINT_ALL,
        "GPU_PROFILE_SUMMARY frames=%u avg_total_ms=%.3f max_total_ms=%.3f "
        "avg_view_ms=%.3f avg_world_ms=%.3f avg_bmodel_ms=%.3f "
        "avg_model_ms=%.3f avg_smodel_ms=%.3f avg_effects_ms=%.3f "
        "avg_sky_ms=%.3f avg_shadows_ms=%.3f avg_flares_ms=%.3f "
        "avg_2d_ms=%.3f avg_clear_ms=%.3f avg_copy_ms=%.3f "
        "avg_present_ms=%.3f avg_misc_ms=%.3f dropped=%u\n",
        coduompGpuProfileSummaryCount,
        coduomp_gpu_profile_msec(totalNanoseconds) /
            (double)coduompGpuProfileSummaryCount,
        coduomp_gpu_profile_msec(
            coduompGpuProfileSummaryMaximumNanoseconds),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_VIEW_SETUP),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_WORLD),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_BRUSH_MODELS),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_MODELS),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_STATIC_MODELS),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_EFFECTS),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_SKY),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_SHADOWS),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_FLARES),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_2D),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_CLEAR),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_SCREEN_COPY),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_PRESENT),
        coduomp_gpu_profile_summary_phase_msec(
            CODUOMP_GPU_PROFILE_PHASE_MISC),
        coduompGpuProfileSummaryDroppedQueries);

    coduompGpuProfileSummaryCount = 0;
    coduompGpuProfileSummaryMaximumNanoseconds = 0;
    coduompGpuProfileSummaryDroppedQueries = 0;
    memset(coduompGpuProfileSummaryNanoseconds, 0,
           sizeof(coduompGpuProfileSummaryNanoseconds));
}

/* NOT_FROM_ORIGINAL_SOURCE: finish one frame record only after all of its GPU
 * queries have become available. */
static void coduomp_gpu_profile_finish_frame(int32_t frameIndex)
{
    coduomp_gpu_profile_frame_t *frame =
        &coduompGpuProfileFrames[frameIndex];
    const uint64_t totalNanoseconds =
        coduomp_gpu_profile_frame_total(frame);

    if (frame->mode >= 2 ||
        coduomp_gpu_profile_msec(totalNanoseconds) >= frame->slowMsec) {
        coduomp_gpu_profile_print_frame(frame, totalNanoseconds);
    }

    ++coduompGpuProfileSummaryCount;
    if (totalNanoseconds > coduompGpuProfileSummaryMaximumNanoseconds)
        coduompGpuProfileSummaryMaximumNanoseconds = totalNanoseconds;
    coduompGpuProfileSummaryDroppedQueries += frame->droppedQueries;
    for (int32_t phase = 0;
         phase < CODUOMP_GPU_PROFILE_PHASE_COUNT; ++phase) {
        coduompGpuProfileSummaryNanoseconds[phase] +=
            frame->phaseNanoseconds[phase];
    }

    if (coduompGpuProfileSummaryCount >=
        CODUOMP_GPU_PROFILE_SUMMARY_FRAMES) {
        coduomp_gpu_profile_print_summary();
    }

    memset(frame, 0, sizeof(*frame));
}

/* NOT_FROM_ORIGINAL_SOURCE: aggregate a completed surface query by shader so
 * slow-frame records identify the renderer material that contributed most. */
static void coduomp_gpu_profile_add_shader(
    coduomp_gpu_profile_frame_t *frame, const char *shaderName,
    uint64_t nanoseconds)
{
    if (shaderName[0] == '\0')
        return;

    for (int32_t shader = 0; shader < frame->shaderCount; ++shader) {
        if (strcmp(frame->shaders[shader].name, shaderName) == 0) {
            frame->shaders[shader].nanoseconds += nanoseconds;
            return;
        }
    }

    if (frame->shaderCount >= CODUOMP_GPU_PROFILE_SHADER_CAPACITY) {
        frame->untrackedShaderNanoseconds += nanoseconds;
        return;
    }

    Q_strncpyz(frame->shaders[frame->shaderCount].name, shaderName,
               sizeof(frame->shaders[frame->shaderCount].name));
    frame->shaders[frame->shaderCount].nanoseconds = nanoseconds;
    ++frame->shaderCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: consume completed timer queries in submission
 * order and never request a result until the driver reports it available. */
static void coduomp_gpu_profile_collect(void)
{
    while (coduompGpuProfilePendingQueries > 0) {
        const int32_t queryIndex = coduompGpuProfileQueryTail;
        coduomp_gpu_profile_query_t *query =
            &coduompGpuProfileQueries[queryIndex];
        uint32_t available = 0;
        uint64_t nanoseconds = 0;

        coduompGlGetQueryObjectuiv(
            coduompGpuProfileQueryIds[queryIndex],
            GL_QUERY_RESULT_AVAILABLE, &available);
        if (available == 0)
            break;

        coduompGlGetQueryObjectui64v(
            coduompGpuProfileQueryIds[queryIndex],
            GL_QUERY_RESULT, &nanoseconds);

        if (query->frameIndex >= 0 &&
            query->frameIndex < CODUOMP_GPU_PROFILE_FRAME_CAPACITY) {
            coduomp_gpu_profile_frame_t *frame =
                &coduompGpuProfileFrames[query->frameIndex];

            if (frame->used != qfalse &&
                frame->serial == query->frameSerial) {
                frame->phaseNanoseconds[query->phase] += nanoseconds;
                coduomp_gpu_profile_add_shader(
                    frame, query->shaderName, nanoseconds);
                if (frame->outstandingQueries > 0)
                    --frame->outstandingQueries;
                if (frame->closed != qfalse &&
                    frame->outstandingQueries == 0) {
                    coduomp_gpu_profile_finish_frame(query->frameIndex);
                }
            }
        }

        memset(query, 0, sizeof(*query));
        coduompGpuProfileQueryTail =
            (queryIndex + 1) % CODUOMP_GPU_PROFILE_QUERY_CAPACITY;
        --coduompGpuProfilePendingQueries;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: resolve timer-query entry points lazily after the
 * renderer has made its OpenGL context current. */
static qboolean coduomp_gpu_profile_load_api(void)
{
    const char *extensions;

    if (coduompGpuProfileApiAttempted != qfalse)
        return coduompGpuProfileApiReady;
    coduompGpuProfileApiAttempted = qtrue;

    extensions = (const char *)qglGetString(GL_EXTENSIONS);
    if (extensions == NULL ||
        (GLW_HasExtension(extensions, "GL_ARB_timer_query") == qfalse &&
         GLW_HasExtension(extensions, "GL_EXT_timer_query") == qfalse)) {
        ri.Printf(R_PRINT_WARNING,
                  "GPU profiling unavailable: OpenGL timer queries are not exposed\n");
        return qfalse;
    }

    coduompGlGenQueries =
        (coduomp_gl_gen_queries_t)qwglGetProcAddress("glGenQueries");
    coduompGlDeleteQueries =
        (coduomp_gl_delete_queries_t)qwglGetProcAddress("glDeleteQueries");
    coduompGlBeginQuery =
        (coduomp_gl_begin_query_t)qwglGetProcAddress("glBeginQuery");
    coduompGlEndQuery =
        (coduomp_gl_end_query_t)qwglGetProcAddress("glEndQuery");
    coduompGlGetQueryObjectuiv =
        (coduomp_gl_get_query_object_uiv_t)
            qwglGetProcAddress("glGetQueryObjectuiv");
    coduompGlGetQueryObjectui64v =
        (coduomp_gl_get_query_object_ui64v_t)
            qwglGetProcAddress("glGetQueryObjectui64v");

    if (coduompGlGenQueries == NULL || coduompGlDeleteQueries == NULL ||
        coduompGlBeginQuery == NULL || coduompGlEndQuery == NULL ||
        coduompGlGetQueryObjectuiv == NULL) {
        coduompGlGenQueries =
            (coduomp_gl_gen_queries_t)
                qwglGetProcAddress("glGenQueriesARB");
        coduompGlDeleteQueries =
            (coduomp_gl_delete_queries_t)
                qwglGetProcAddress("glDeleteQueriesARB");
        coduompGlBeginQuery =
            (coduomp_gl_begin_query_t)
                qwglGetProcAddress("glBeginQueryARB");
        coduompGlEndQuery =
            (coduomp_gl_end_query_t)
                qwglGetProcAddress("glEndQueryARB");
        coduompGlGetQueryObjectuiv =
            (coduomp_gl_get_query_object_uiv_t)
                qwglGetProcAddress("glGetQueryObjectuivARB");
    }
    if (coduompGlGetQueryObjectui64v == NULL) {
        coduompGlGetQueryObjectui64v =
            (coduomp_gl_get_query_object_ui64v_t)
                qwglGetProcAddress("glGetQueryObjectui64vEXT");
    }

    if (coduompGlGenQueries == NULL || coduompGlDeleteQueries == NULL ||
        coduompGlBeginQuery == NULL || coduompGlEndQuery == NULL ||
        coduompGlGetQueryObjectuiv == NULL ||
        coduompGlGetQueryObjectui64v == NULL) {
        ri.Printf(R_PRINT_WARNING,
                  "GPU profiling unavailable: timer-query entry points are incomplete\n");
        return qfalse;
    }

    coduompGlGenQueries(CODUOMP_GPU_PROFILE_QUERY_CAPACITY,
                        coduompGpuProfileQueryIds);
    coduompGpuProfileApiReady = qtrue;
    ri.Printf(R_PRINT_ALL,
              "GPU profiling ready: r_gpuProfile 1 logs slow frames; 2 logs every frame\n");
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: register runtime controls only in the dedicated
 * profiling build. Build it with:
 * make client BUILD_DIR=.workbench/build/gpu-profile
 * CODUOMP_RENDERER_GPU_PROFILE=1 */
void coduomp_gpu_profile_register(void)
{
    coduompGpuProfileMode =
        ri.Cvar_Get("r_gpuProfile", "0", CVAR_TEMP);
    coduompGpuProfileSlowMsec =
        ri.Cvar_Get("r_gpuProfileSlowMsec", "4.0", CVAR_TEMP);
}

/* NOT_FROM_ORIGINAL_SOURCE: choose a free delayed-result record for the next
 * backend command list without overwriting in-flight data. */
void coduomp_gpu_profile_frame_begin(void)
{
    coduompGpuProfileCurrentFrame = -1;
    if (coduompGpuProfileApiReady != qfalse)
        coduomp_gpu_profile_collect();

    if (coduompGpuProfileMode == NULL ||
        coduompGpuProfileMode->integer <= 0 ||
        coduomp_gpu_profile_load_api() == qfalse) {
        return;
    }

    for (int32_t frameIndex = 0;
         frameIndex < CODUOMP_GPU_PROFILE_FRAME_CAPACITY; ++frameIndex) {
        coduomp_gpu_profile_frame_t *frame =
            &coduompGpuProfileFrames[frameIndex];

        if (frame->used != qfalse)
            continue;
        memset(frame, 0, sizeof(*frame));
        frame->used = qtrue;
        frame->serial = ++coduompGpuProfileNextFrameSerial;
        frame->mode = coduompGpuProfileMode->integer;
        frame->slowMsec = coduompGpuProfileSlowMsec != NULL &&
                                  coduompGpuProfileSlowMsec->value > 0.0f
                              ? coduompGpuProfileSlowMsec->value
                              : 0.0f;
        coduompGpuProfileCurrentFrame = frameIndex;
        return;
    }

    ri.Printf(R_PRINT_WARNING,
              "GPU profiling skipped a frame: delayed frame records are full\n");
}

/* NOT_FROM_ORIGINAL_SOURCE: mark the current command list complete and poll
 * results without waiting for the GPU. */
void coduomp_gpu_profile_frame_end(void)
{
    const int32_t frameIndex = coduompGpuProfileCurrentFrame;

    if (frameIndex < 0)
        return;

    coduompGpuProfileFrames[frameIndex].closed = qtrue;
    coduomp_gpu_profile_collect();
    if (coduompGpuProfileFrames[frameIndex].used != qfalse &&
        coduompGpuProfileFrames[frameIndex].outstandingQueries == 0) {
        coduomp_gpu_profile_finish_frame(frameIndex);
    }
    coduompGpuProfileCurrentFrame = -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: begin one non-overlapping elapsed-time query. A
 * full query pool drops the sample rather than stalling the render thread. */
qboolean coduomp_gpu_profile_begin(
    coduomp_gpu_profile_phase_t phase, const char *shaderName)
{
    coduomp_gpu_profile_query_t *query;
    coduomp_gpu_profile_frame_t *frame;
    int32_t queryIndex;

    if (coduompGpuProfileCurrentFrame < 0 ||
        coduompGpuProfileActiveQuery >= 0 ||
        coduompGpuProfileApiReady == qfalse) {
        return qfalse;
    }

    frame = &coduompGpuProfileFrames[coduompGpuProfileCurrentFrame];
    if (coduompGpuProfilePendingQueries >=
        CODUOMP_GPU_PROFILE_QUERY_CAPACITY) {
        ++frame->droppedQueries;
        return qfalse;
    }

    queryIndex = coduompGpuProfileQueryHead;
    query = &coduompGpuProfileQueries[queryIndex];
    memset(query, 0, sizeof(*query));
    query->frameIndex = coduompGpuProfileCurrentFrame;
    query->frameSerial = frame->serial;
    query->phase = phase;
    if (shaderName != NULL) {
        Q_strncpyz(query->shaderName, shaderName,
                   sizeof(query->shaderName));
    }

    coduompGlBeginQuery(CODUOMP_GL_TIME_ELAPSED,
                        coduompGpuProfileQueryIds[queryIndex]);
    coduompGpuProfileActiveQuery = queryIndex;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: classify the renderer's current tessellation
 * batch before RB_EndSurface submits it. */
qboolean coduomp_gpu_profile_begin_surface(void)
{
    coduomp_gpu_profile_phase_t phase;
    const char *shaderName =
        tess.shader != NULL ? tess.shader->name : NULL;

    if (backEnd.projection2D != qfalse) {
        phase = CODUOMP_GPU_PROFILE_PHASE_2D;
    } else if (tess.shader == tr.stencilShadowShader) {
        phase = CODUOMP_GPU_PROFILE_PHASE_SHADOWS;
    } else if (tess.stageIterator == RB_StageIteratorSky) {
        phase = CODUOMP_GPU_PROFILE_PHASE_SKY;
    } else if (tess.entity == &tr.worldEntity) {
        phase = CODUOMP_GPU_PROFILE_PHASE_WORLD;
    } else if (tess.entity == NULL) {
        phase = CODUOMP_GPU_PROFILE_PHASE_MISC;
    } else {
        switch (tess.entity->e.reType) {
        case RT_BRUSH_MODEL:
            phase = CODUOMP_GPU_PROFILE_PHASE_BRUSH_MODELS;
            break;
        case RT_MODEL:
            phase = CODUOMP_GPU_PROFILE_PHASE_MODELS;
            break;
        case RT_STATIC_MODEL:
            phase = CODUOMP_GPU_PROFILE_PHASE_STATIC_MODELS;
            break;
        default:
            phase = CODUOMP_GPU_PROFILE_PHASE_EFFECTS;
            break;
        }
    }

    return coduomp_gpu_profile_begin(phase, shaderName);
}

/* NOT_FROM_ORIGINAL_SOURCE: close a query only when its matching begin call
 * acquired a slot; nested scopes remain owned by their outer operation. */
void coduomp_gpu_profile_end(qboolean started)
{
    coduomp_gpu_profile_query_t *query;
    coduomp_gpu_profile_frame_t *frame;
    int32_t queryIndex;

    if (started == qfalse || coduompGpuProfileActiveQuery < 0)
        return;

    queryIndex = coduompGpuProfileActiveQuery;
    query = &coduompGpuProfileQueries[queryIndex];
    coduompGlEndQuery(CODUOMP_GL_TIME_ELAPSED);
    frame = &coduompGpuProfileFrames[query->frameIndex];
    ++frame->outstandingQueries;
    ++coduompGpuProfilePendingQueries;
    coduompGpuProfileQueryHead =
        (queryIndex + 1) % CODUOMP_GPU_PROFILE_QUERY_CAPACITY;
    coduompGpuProfileActiveQuery = -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: release context-owned query objects before the
 * renderer destroys or replaces its OpenGL context. */
void coduomp_gpu_profile_shutdown(void)
{
    if (coduompGpuProfileActiveQuery >= 0 &&
        coduompGlEndQuery != NULL) {
        coduompGlEndQuery(CODUOMP_GL_TIME_ELAPSED);
    }
    if (coduompGpuProfileApiReady != qfalse &&
        coduompGlDeleteQueries != NULL) {
        coduompGlDeleteQueries(CODUOMP_GPU_PROFILE_QUERY_CAPACITY,
                               coduompGpuProfileQueryIds);
    }

    memset(coduompGpuProfileQueryIds, 0,
           sizeof(coduompGpuProfileQueryIds));
    memset(coduompGpuProfileQueries, 0,
           sizeof(coduompGpuProfileQueries));
    memset(coduompGpuProfileFrames, 0,
           sizeof(coduompGpuProfileFrames));
    memset(coduompGpuProfileSummaryNanoseconds, 0,
           sizeof(coduompGpuProfileSummaryNanoseconds));
    coduompGlGenQueries = NULL;
    coduompGlDeleteQueries = NULL;
    coduompGlBeginQuery = NULL;
    coduompGlEndQuery = NULL;
    coduompGlGetQueryObjectuiv = NULL;
    coduompGlGetQueryObjectui64v = NULL;
    coduompGpuProfileApiAttempted = qfalse;
    coduompGpuProfileApiReady = qfalse;
    coduompGpuProfileQueryHead = 0;
    coduompGpuProfileQueryTail = 0;
    coduompGpuProfilePendingQueries = 0;
    coduompGpuProfileActiveQuery = -1;
    coduompGpuProfileCurrentFrame = -1;
    coduompGpuProfileNextFrameSerial = 0;
    coduompGpuProfileSummaryCount = 0;
    coduompGpuProfileSummaryMaximumNanoseconds = 0;
    coduompGpuProfileSummaryDroppedQueries = 0;
}

#endif
