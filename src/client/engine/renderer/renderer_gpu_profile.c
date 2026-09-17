#include "renderer_gpu_profile.h"

#if defined(CODUOMP_RENDERER_GPU_PROFILE)

#include "backend.h"
#include "gl_api.h"
#include "wgl_debug.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

enum {
    CODUOMP_GPU_PROFILE_QUERY_CAPACITY = 4096,
    CODUOMP_GPU_PROFILE_FRAME_CAPACITY = 64,
    CODUOMP_GPU_PROFILE_SHADER_CAPACITY = 512,
    CODUOMP_GPU_PROFILE_SHADER_NAME_CAPACITY = 64,
    CODUOMP_GPU_PROFILE_WORLD_TOP_COUNT = 12,
    CODUOMP_GPU_PROFILE_SUMMARY_FRAMES = 250,
    CODUOMP_GL_TIME_ELAPSED = 0x88bf
};

static const char coduompGpuProfileLogPath[] = "gpu_profile.log";

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
    const shader_t *shader;
    char name[CODUOMP_GPU_PROFILE_SHADER_NAME_CAPACITY];
    uint64_t nanoseconds;
    uint32_t batches;
    uint32_t drawCalls;
    uint32_t sceneDrawCalls;
    uint32_t worldBatches;
    uint32_t worldDrawCalls;
    uint32_t entityBreaks;
} coduomp_gpu_profile_shader_t;

typedef struct coduomp_gpu_profile_merge_class_s {
    const shader_t *shader;
    uint32_t shaderCount;
    uint32_t batches;
    uint32_t collapsedBatches;
    uint32_t drawCalls;
    uint32_t collapsedDrawCalls;
    qboolean worldArrayEligible;
} coduomp_gpu_profile_merge_class_t;

typedef struct coduomp_gpu_profile_frame_s {
    qboolean used;
    qboolean closed;
    qboolean sawWorld;
    uint32_t serial;
    int32_t mode;
    int32_t detailLevel;
    float slowMsec;
    int32_t refdefTime;
    uint64_t cpuFrameStartNanoseconds;
    uint64_t cpuFrameNanoseconds;
    uint64_t cpuFrameIntervalNanoseconds;
    uint64_t cpuPreFrontendNanoseconds;
    uint64_t cpuFrontendNanoseconds;
    uint64_t cpuCgameNanoseconds;
    uint64_t cpuRenderSceneNanoseconds;
    uint64_t cpuBackendStartNanoseconds;
    uint64_t cpuBackendNanoseconds;
    uint64_t cpuFinishNanoseconds;
    uint64_t cpuPresentNanoseconds;
    uint64_t frontendScopeCpuNanoseconds[
        CODUOMP_GPU_PROFILE_FRONTEND_SCOPE_COUNT];
    uint32_t frontendScopeCounts[
        CODUOMP_GPU_PROFILE_FRONTEND_SCOPE_COUNT];
    uint64_t phaseCpuNanoseconds[CODUOMP_GPU_PROFILE_PHASE_COUNT];
    uint32_t outstandingQueries;
    uint32_t droppedQueries;
    uint64_t phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_COUNT];
    uint32_t phaseBatches[CODUOMP_GPU_PROFILE_PHASE_COUNT];
    uint32_t phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_COUNT];
    uint32_t portalBatches;
    uint32_t portalDrawCalls;
    uint32_t mainSceneBatches;
    uint32_t mainSceneDrawCalls;
    uint32_t drawSurfBreaks;
    uint32_t shaderBreaks;
    uint32_t storageBreaks;
    uint32_t dlightBreaks;
    uint32_t batchFlag2Breaks;
    uint32_t entityBreaks;
    uint32_t exactShaderBreaks;
    uint32_t overflowBreaks;
    uint32_t breakReasonMasks[32];
    coduomp_gpu_profile_shader_t
        shaders[CODUOMP_GPU_PROFILE_SHADER_CAPACITY];
    int32_t shaderCount;
    uint64_t untrackedShaderNanoseconds;
    uint32_t untrackedShaderBatches;
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
static cvar_t *coduompGpuProfileDetail;
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
static uint64_t coduompGpuProfileSummaryBatches;
static uint64_t coduompGpuProfileSummaryDrawCalls;
static uint64_t coduompGpuProfileSummaryPortalBatches;
static uint64_t coduompGpuProfileSummaryPortalDrawCalls;
static uint64_t coduompGpuProfileSummaryMainSceneBatches;
static uint64_t coduompGpuProfileSummaryMainSceneDrawCalls;
static uint64_t coduompGpuProfileSummaryDrawSurfBreaks;
static uint64_t coduompGpuProfileSummaryShaderBreaks;
static uint64_t coduompGpuProfileSummaryStorageBreaks;
static uint64_t coduompGpuProfileSummaryDlightBreaks;
static uint64_t coduompGpuProfileSummaryBatchFlag2Breaks;
static uint64_t coduompGpuProfileSummaryEntityBreaks;
static uint64_t coduompGpuProfileSummaryExactShaderBreaks;
static uint64_t coduompGpuProfileSummaryOverflowBreaks;
static uint64_t coduompGpuProfileSummaryBreakReasonMasks[32];
static FILE *coduompGpuProfileLogFile;
static qboolean coduompGpuProfileLogOpenAttempted;
static qboolean coduompGpuProfileWasEnabled;
static qboolean coduompGpuProfileCapacityWarningPrinted;
static int32_t coduompGpuProfileSurfaceFrame = -1;
static uint32_t coduompGpuProfileSurfaceFrameSerial;
static int32_t coduompGpuProfileSurfaceShader = -1;
static coduomp_gpu_profile_phase_t coduompGpuProfileSurfacePhase;
static qboolean coduompGpuProfileSurfacePortal;
static uint64_t coduompGpuProfileFrontendStartNanoseconds;
static uint64_t coduompGpuProfileLastFrontendStartNanoseconds;
static uint64_t coduompGpuProfilePendingFrameIntervalNanoseconds;
static uint64_t coduompGpuProfilePendingFrontendNanoseconds;
static uint64_t coduompGpuProfilePendingFrameStartNanoseconds;
static uint64_t coduompGpuProfilePendingPreFrontendNanoseconds;
static uint64_t coduompGpuProfilePendingCgameNanoseconds;
static uint64_t coduompGpuProfilePendingRenderSceneNanoseconds;
static uint64_t coduompGpuProfilePendingFrontendScopeNanoseconds[
    CODUOMP_GPU_PROFILE_FRONTEND_SCOPE_COUNT];
static uint32_t coduompGpuProfilePendingFrontendScopeCounts[
    CODUOMP_GPU_PROFILE_FRONTEND_SCOPE_COUNT];
static uint64_t coduompGpuProfileCgameStartNanoseconds;
static uint64_t coduompGpuProfileRenderSceneStartNanoseconds;
static uint64_t coduompGpuProfileFinishStartNanoseconds;
static uint64_t coduompGpuProfilePresentStartNanoseconds;
static coduomp_gpu_profile_phase_t coduompGpuProfileCpuPhase =
    CODUOMP_GPU_PROFILE_PHASE_COUNT;
static uint64_t coduompGpuProfileCpuSegmentStartNanoseconds;

/* NOT_FROM_ORIGINAL_SOURCE: provide a monotonic high-resolution CPU clock for
 * the compiler-gated diagnostic without narrowing measurements to milliseconds. */
static uint64_t coduomp_gpu_profile_cpu_nanoseconds(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    static LARGE_INTEGER frequency;

    if (frequency.QuadPart == 0 &&
        QueryPerformanceFrequency(&frequency) == 0) {
        return 0;
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)(((long double)counter.QuadPart * 1000000000.0L) /
                      (long double)frequency.QuadPart);
#else
    struct timespec now;

    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: open one host-file diagnostic log below
 * fs_homepath. This deliberately avoids an engine filesystem handle because
 * a server-namespace restart closes those handles before renderer shutdown. */
static qboolean coduomp_gpu_profile_open_log(void)
{
    char logPath[MAX_OSPATH];
    cvar_t *homePath;
    int32_t logPathLength;

    if (coduompGpuProfileLogFile != NULL)
        return qtrue;
    if (coduompGpuProfileLogOpenAttempted != qfalse)
        return qfalse;

    coduompGpuProfileLogOpenAttempted = qtrue;
    homePath = ri.Cvar_Get("fs_homepath", "", CVAR_NONE);
    if (homePath == NULL || homePath->string[0] == '\0') {
        ri.Printf(R_PRINT_WARNING,
                  "GPU profiling unavailable: invalid fs_homepath\n");
        return qfalse;
    }
    logPathLength = snprintf(logPath, sizeof(logPath), "%s/%s",
                             homePath->string,
                             coduompGpuProfileLogPath);
    if (logPathLength < 0 ||
        (size_t)logPathLength >= sizeof(logPath)) {
        ri.Printf(R_PRINT_WARNING,
                  "GPU profiling unavailable: log path is too long\n");
        return qfalse;
    }

    coduompGpuProfileLogFile = fopen(logPath, "a");
    if (coduompGpuProfileLogFile == NULL) {
        ri.Printf(R_PRINT_WARNING,
                  "GPU profiling unavailable: could not open %s\n",
                  logPath);
        return qfalse;
    }

    fprintf(coduompGpuProfileLogFile,
            "GPU_PROFILE_LOG version=3 time_unit=milliseconds\n");
    ri.Printf(R_PRINT_ALL, "GPU profiling writing to %s\n", logPath);
    return qtrue;
}

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

/* NOT_FROM_ORIGINAL_SOURCE: total the CPU-side tessellation batches recorded
 * without adding timer queries around individual surfaces. */
static uint32_t coduomp_gpu_profile_frame_batches(
    const coduomp_gpu_profile_frame_t *frame)
{
    uint32_t total = 0;

    for (int32_t phase = 0;
         phase < CODUOMP_GPU_PROFILE_PHASE_COUNT; ++phase) {
        total += frame->phaseBatches[phase];
    }
    return total;
}

/* NOT_FROM_ORIGINAL_SOURCE: total actual GL draw submissions attributed to
 * the current backend frame. */
static uint32_t coduomp_gpu_profile_frame_draw_calls(
    const coduomp_gpu_profile_frame_t *frame)
{
    uint32_t total = 0;

    for (int32_t phase = 0;
         phase < CODUOMP_GPU_PROFILE_PHASE_COUNT; ++phase) {
        total += frame->phaseDrawCalls[phase];
    }
    return total;
}

/* NOT_FROM_ORIGINAL_SOURCE: identify phases submitted by an RE_RenderScene
 * command rather than later screen-space or miscellaneous backend work. */
static qboolean coduomp_gpu_profile_phase_is_scene(
    coduomp_gpu_profile_phase_t phase)
{
    return phase == CODUOMP_GPU_PROFILE_PHASE_WORLD ||
           phase == CODUOMP_GPU_PROFILE_PHASE_BRUSH_MODELS ||
           phase == CODUOMP_GPU_PROFILE_PHASE_MODELS ||
           phase == CODUOMP_GPU_PROFILE_PHASE_STATIC_MODELS ||
           phase == CODUOMP_GPU_PROFILE_PHASE_EFFECTS ||
           phase == CODUOMP_GPU_PROFILE_PHASE_SKY ||
           phase == CODUOMP_GPU_PROFILE_PHASE_SHADOWS ||
           phase == CODUOMP_GPU_PROFILE_PHASE_FLARES;
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

/* NOT_FROM_ORIGINAL_SOURCE: retain the three materials responsible for the
 * most actual draw submissions, breaking ties by tessellation batch count. */
static void coduomp_gpu_profile_find_top_batch_shaders(
    const coduomp_gpu_profile_frame_t *frame, int32_t top[3])
{
    top[0] = -1;
    top[1] = -1;
    top[2] = -1;

    for (int32_t shader = 0; shader < frame->shaderCount; ++shader) {
        for (int32_t rank = 0; rank < 3; ++rank) {
            const qboolean outranks =
                top[rank] < 0 ||
                frame->shaders[shader].drawCalls >
                    frame->shaders[top[rank]].drawCalls ||
                (frame->shaders[shader].drawCalls ==
                     frame->shaders[top[rank]].drawCalls &&
                 frame->shaders[shader].batches >
                     frame->shaders[top[rank]].batches);

            if (outranks != qfalse) {
                for (int32_t move = 2; move > rank; --move)
                    top[move] = top[move - 1];
                top[rank] = shader;
                break;
            }
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: rank scene materials separately so frequently
 * changing UI font pages cannot hide world or entity submission hot spots. */
static void coduomp_gpu_profile_find_top_scene_shaders(
    const coduomp_gpu_profile_frame_t *frame, int32_t top[3])
{
    top[0] = -1;
    top[1] = -1;
    top[2] = -1;

    for (int32_t shader = 0; shader < frame->shaderCount; ++shader) {
        for (int32_t rank = 0; rank < 3; ++rank) {
            if (top[rank] < 0 ||
                frame->shaders[shader].sceneDrawCalls >
                    frame->shaders[top[rank]].sceneDrawCalls) {
                for (int32_t move = 2; move > rank; --move)
                    top[move] = top[move - 1];
                top[rank] = shader;
                break;
            }
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: rank the materials whose repeated entity
 * transforms prevent otherwise equal sort-key surfaces from sharing a batch. */
static void coduomp_gpu_profile_find_top_entity_break_shaders(
    const coduomp_gpu_profile_frame_t *frame, int32_t top[3])
{
    top[0] = -1;
    top[1] = -1;
    top[2] = -1;

    for (int32_t shader = 0; shader < frame->shaderCount; ++shader) {
        for (int32_t rank = 0; rank < 3; ++rank) {
            if (top[rank] < 0 ||
                frame->shaders[shader].entityBreaks >
                    frame->shaders[top[rank]].entityBreaks) {
                for (int32_t move = 2; move > rank; --move)
                    top[move] = top[move - 1];
                top[rank] = shader;
                break;
            }
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: rank world materials without allowing model or
 * screen-space submissions to obscure the static-map workload. */
static void coduomp_gpu_profile_find_top_world_shaders(
    const coduomp_gpu_profile_frame_t *frame,
    int32_t top[CODUOMP_GPU_PROFILE_WORLD_TOP_COUNT])
{
    for (int32_t rank = 0;
         rank < CODUOMP_GPU_PROFILE_WORLD_TOP_COUNT; ++rank) {
        top[rank] = -1;
    }

    for (int32_t shader = 0; shader < frame->shaderCount; ++shader) {
        for (int32_t rank = 0;
             rank < CODUOMP_GPU_PROFILE_WORLD_TOP_COUNT; ++rank) {
            const qboolean outranks =
                top[rank] < 0 ||
                frame->shaders[shader].worldDrawCalls >
                    frame->shaders[top[rank]].worldDrawCalls ||
                (frame->shaders[shader].worldDrawCalls ==
                     frame->shaders[top[rank]].worldDrawCalls &&
                 frame->shaders[shader].worldBatches >
                     frame->shaders[top[rank]].worldBatches);

            if (outranks != qfalse) {
                for (int32_t move =
                         CODUOMP_GPU_PROFILE_WORLD_TOP_COUNT - 1;
                     move > rank; --move) {
                    top[move] = top[move - 1];
                }
                top[rank] = shader;
                break;
            }
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: identify images that could occupy layers of one
 * texture array without resampling or changing their sampler wrap policy. */
static qboolean coduomp_gpu_profile_array_images_compatible(
    const image_t *left, const image_t *right)
{
    const uint32_t samplerFlags =
        IMAGE_FLAG_MIPMAP | IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T;

    return left != NULL && right != NULL &&
           left->target == GL_TEXTURE_2D && right->target == GL_TEXTURE_2D &&
           left->uploadWidth == right->uploadWidth &&
           left->uploadHeight == right->uploadHeight &&
           left->internalFormat == right->internalFormat &&
           (left->flags & samplerFlags) == (right->flags & samplerFlags);
}

/* NOT_FROM_ORIGINAL_SOURCE: restrict the implementation census to the
 * fixed-function world case that a texture-array fragment program can replace
 * exactly without dynamic texture coordinates or vendor-specific programs. */
static qboolean coduomp_gpu_profile_world_array_shader_eligible(
    const shader_t *shader)
{
    if (shader == NULL || shader->primaryImage == NULL ||
        CanOptimizeShader(shader) == qfalse) {
        return qfalse;
    }

    int32_t ordinaryStageCount = 0;
    for (int32_t stageIndex = 0;
         stageIndex < shader->numUnfoggedPasses; ++stageIndex) {
        const shaderStage_t *candidate = shader->stages[stageIndex];

        if (candidate != NULL &&
            (candidate->flags & SHADER_STAGE_PER_LIGHT) == 0) {
            ++ordinaryStageCount;
        }
    }
    if (ordinaryStageCount != 1)
        return qfalse;

    const image_t *image = shader->primaryImage;
    if (image->internalFormat != GL_COMPRESSED_RGB_S3TC_DXT1_EXT &&
        image->internalFormat != GL_COMPRESSED_RGBA_S3TC_DXT1_EXT &&
        image->internalFormat != GL_COMPRESSED_RGBA_S3TC_DXT3_EXT &&
        image->internalFormat != GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        return qfalse;
    }

    const shaderStage_t *stage = shader->stages[0];
    if (stage == NULL ||
        (stage->flags & SHADER_STAGE_PER_LIGHT) != 0 ||
        stage->fragmentShaderATI != 0 || stage->registerCombiners != NULL ||
        stage->vertexProgram != NULL) {
        return qfalse;
    }

    const textureBundle_t *base = &stage->bundle[0];
    const textureBundle_t *lightmap = &stage->bundle[1];
    if (base->image[0] != image || base->numImageAnimations > 1 ||
        base->textureEnvMode != GL_MODULATE ||
        base->texCoordComponentCount != 2 || base->tcGen != TCGEN_TEXTURE ||
        base->numTexMods != 0 || base->waterMap != NULL ||
        base->textureCombine != NULL || base->textureShader != NULL ||
        base->isVideoMap != 0 ||
        lightmap->image[0] == NULL || lightmap->numImageAnimations > 1 ||
        lightmap->textureEnvMode != GL_MODULATE ||
        lightmap->texCoordComponentCount != 2 ||
        lightmap->tcGen != TCGEN_LIGHTMAP || lightmap->numTexMods != 0 ||
        lightmap->waterMap != NULL || lightmap->textureCombine != NULL ||
        lightmap->textureShader != NULL || lightmap->isLightmap == 0 ||
        lightmap->isVideoMap != 0 ||
        stage->bundle[2].textureEnvMode != 0) {
        return qfalse;
    }

    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: estimate the best-case draw reduction if world
 * materials with identical render state shared one texture-array pipeline.
 * This is a census only; it does not alter sorting or rendering. */
static void coduomp_gpu_profile_write_world_merge_census(
    const coduomp_gpu_profile_frame_t *frame)
{
    coduomp_gpu_profile_merge_class_t compatible[
        CODUOMP_GPU_PROFILE_SHADER_CAPACITY] = {{0}};
    coduomp_gpu_profile_merge_class_t arrayCompatible[
        CODUOMP_GPU_PROFILE_SHADER_CAPACITY] = {{0}};
    int32_t compatibleCount = 0;
    int32_t arrayCompatibleCount = 0;
    uint32_t worldShaders = 0;

    for (int32_t shaderIndex = 0;
         shaderIndex < frame->shaderCount; ++shaderIndex) {
        const coduomp_gpu_profile_shader_t *entry =
            &frame->shaders[shaderIndex];
        const shader_t *shader = entry->shader;

        if (shader == NULL || entry->worldBatches == 0)
            continue;
        ++worldShaders;

        int32_t classIndex;
        for (classIndex = 0; classIndex < compatibleCount; ++classIndex) {
            const shader_t *representative = compatible[classIndex].shader;
            if (CompareMergableShaders(
                    representative, shader,
                    representative->primaryImage,
                    shader->primaryImage) == 0) {
                break;
            }
        }
        if (classIndex == compatibleCount) {
            compatible[classIndex].shader = shader;
            ++compatibleCount;
        }
        coduomp_gpu_profile_merge_class_t *mergeClass =
            &compatible[classIndex];
        ++mergeClass->shaderCount;
        mergeClass->batches += entry->worldBatches;
        mergeClass->drawCalls += entry->worldDrawCalls;
        if (entry->worldBatches > mergeClass->collapsedBatches)
            mergeClass->collapsedBatches = entry->worldBatches;
        if (entry->worldDrawCalls > mergeClass->collapsedDrawCalls)
            mergeClass->collapsedDrawCalls = entry->worldDrawCalls;

        for (classIndex = 0;
             classIndex < arrayCompatibleCount; ++classIndex) {
            const shader_t *representative =
                arrayCompatible[classIndex].shader;
            if (coduomp_gpu_profile_array_images_compatible(
                    representative->primaryImage,
                    shader->primaryImage) != qfalse &&
                CompareMergableShaders(
                    representative, shader,
                    representative->primaryImage,
                    shader->primaryImage) == 0) {
                break;
            }
        }
        if (classIndex == arrayCompatibleCount) {
            arrayCompatible[classIndex].shader = shader;
            arrayCompatible[classIndex].worldArrayEligible =
                coduomp_gpu_profile_world_array_shader_eligible(shader);
            ++arrayCompatibleCount;
        }
        mergeClass = &arrayCompatible[classIndex];
        ++mergeClass->shaderCount;
        mergeClass->batches += entry->worldBatches;
        mergeClass->drawCalls += entry->worldDrawCalls;
        if (entry->worldBatches > mergeClass->collapsedBatches)
            mergeClass->collapsedBatches = entry->worldBatches;
        if (entry->worldDrawCalls > mergeClass->collapsedDrawCalls)
            mergeClass->collapsedDrawCalls = entry->worldDrawCalls;
    }

    uint32_t compatibleBatches = 0;
    uint32_t compatibleDraws = 0;
    uint32_t arrayCompatibleBatches = 0;
    uint32_t arrayCompatibleDraws = 0;
    uint32_t eligibleShaders = 0;
    uint32_t eligibleClasses = 0;
    uint32_t eligibleBatches = 0;
    uint32_t eligibleDraws = 0;
    uint32_t eligibleCollapsedBatches = 0;
    uint32_t eligibleCollapsedDraws = 0;
    for (int32_t classIndex = 0;
         classIndex < compatibleCount; ++classIndex) {
        compatibleBatches += compatible[classIndex].collapsedBatches;
        compatibleDraws += compatible[classIndex].collapsedDrawCalls;
    }
    for (int32_t classIndex = 0;
         classIndex < arrayCompatibleCount; ++classIndex) {
        arrayCompatibleBatches +=
            arrayCompatible[classIndex].collapsedBatches;
        arrayCompatibleDraws +=
            arrayCompatible[classIndex].collapsedDrawCalls;
        if (arrayCompatible[classIndex].worldArrayEligible != qfalse) {
            eligibleShaders += arrayCompatible[classIndex].shaderCount;
            ++eligibleClasses;
            eligibleBatches += arrayCompatible[classIndex].batches;
            eligibleDraws += arrayCompatible[classIndex].drawCalls;
            eligibleCollapsedBatches +=
                arrayCompatible[classIndex].collapsedBatches;
            eligibleCollapsedDraws +=
                arrayCompatible[classIndex].collapsedDrawCalls;
        }
    }

    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE_WORLD_MERGE frame=%u shaders=%u compatible_classes=%d "
        "compatible_batches=%u compatible_draws=%u array_classes=%d "
        "array_batches=%u array_draws=%u eligible_shaders=%u "
        "eligible_classes=%u eligible_batches=%u eligible_draws=%u "
        "eligible_collapsed_batches=%u eligible_collapsed_draws=%u\n",
        frame->serial, worldShaders, compatibleCount,
        compatibleBatches, compatibleDraws, arrayCompatibleCount,
        arrayCompatibleBatches, arrayCompatibleDraws, eligibleShaders,
        eligibleClasses, eligibleBatches, eligibleDraws,
        eligibleCollapsedBatches, eligibleCollapsedDraws);
}

/* NOT_FROM_ORIGINAL_SOURCE: emit a parseable slow-frame record and its most
 * expensive shader batches after every query for the frame has completed. */
static void coduomp_gpu_profile_write_frame(
    const coduomp_gpu_profile_frame_t *frame, uint64_t totalNanoseconds)
{
    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE frame=%u total_ms=%.3f view_ms=%.3f scene_ms=%.3f world_ms=%.3f "
        "bmodel_ms=%.3f model_ms=%.3f smodel_ms=%.3f effects_ms=%.3f "
        "sky_ms=%.3f shadows_ms=%.3f flares_ms=%.3f 2d_ms=%.3f "
        "clear_ms=%.3f copy_ms=%.3f present_ms=%.3f misc_ms=%.3f "
        "dropped=%u gameplay=%d refdef_time=%d frame_cpu_ms=%.3f "
        "frame_interval_cpu_ms=%.3f "
        "pre_frontend_cpu_ms=%.3f frontend_cpu_ms=%.3f "
        "cgame_cpu_ms=%.3f render_scene_cpu_ms=%.3f "
        "dpvs_setup_cpu_ms=%.3f model_filter_cpu_ms=%.3f "
        "world_traversal_cpu_ms=%.3f entities_cpu_ms=%.3f "
        "sort_cpu_ms=%.3f "
        "brush_entities_cpu_ms=%.3f xmodel_entities_cpu_ms=%.3f "
        "static_entities_cpu_ms=%.3f effect_entities_cpu_ms=%.3f "
        "brush_entity_count=%u xmodel_entity_count=%u "
        "static_entity_count=%u effect_entity_count=%u "
        "static_cache_build_cpu_ms=%.3f static_lighting_cpu_ms=%.3f "
        "static_cache_build_count=%u static_lighting_count=%u "
        "backend_cpu_ms=%.3f finish_cpu_ms=%.3f "
        "present_cpu_ms=%.3f scene_submit_cpu_ms=%.3f "
        "2d_submit_cpu_ms=%.3f clear_submit_cpu_ms=%.3f "
        "misc_submit_cpu_ms=%.3f other_cpu_ms=%.3f\n",
        frame->serial, coduomp_gpu_profile_msec(totalNanoseconds),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_VIEW_SETUP]),
        coduomp_gpu_profile_msec(
            frame->phaseNanoseconds[CODUOMP_GPU_PROFILE_PHASE_SCENE]),
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
        frame->droppedQueries, frame->sawWorld, frame->refdefTime,
        coduomp_gpu_profile_msec(frame->cpuFrameNanoseconds),
        coduomp_gpu_profile_msec(frame->cpuFrameIntervalNanoseconds),
        coduomp_gpu_profile_msec(frame->cpuPreFrontendNanoseconds),
        coduomp_gpu_profile_msec(frame->cpuFrontendNanoseconds),
        coduomp_gpu_profile_msec(frame->cpuCgameNanoseconds),
        coduomp_gpu_profile_msec(frame->cpuRenderSceneNanoseconds),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_DPVS_SETUP]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_MODEL_FILTER]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_WORLD_TRAVERSAL]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_ENTITIES]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_SORT]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_BRUSH_ENTITIES]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_XMODEL_ENTITIES]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_STATIC_ENTITIES]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_EFFECT_ENTITIES]),
        frame->frontendScopeCounts[
            CODUOMP_GPU_PROFILE_FRONTEND_BRUSH_ENTITIES],
        frame->frontendScopeCounts[
            CODUOMP_GPU_PROFILE_FRONTEND_XMODEL_ENTITIES],
        frame->frontendScopeCounts[
            CODUOMP_GPU_PROFILE_FRONTEND_STATIC_ENTITIES],
        frame->frontendScopeCounts[
            CODUOMP_GPU_PROFILE_FRONTEND_EFFECT_ENTITIES],
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_STATIC_CACHE_BUILD]),
        coduomp_gpu_profile_msec(frame->frontendScopeCpuNanoseconds[
            CODUOMP_GPU_PROFILE_FRONTEND_STATIC_LIGHTING]),
        frame->frontendScopeCounts[
            CODUOMP_GPU_PROFILE_FRONTEND_STATIC_CACHE_BUILD],
        frame->frontendScopeCounts[
            CODUOMP_GPU_PROFILE_FRONTEND_STATIC_LIGHTING],
        coduomp_gpu_profile_msec(frame->cpuBackendNanoseconds),
        coduomp_gpu_profile_msec(frame->cpuFinishNanoseconds),
        coduomp_gpu_profile_msec(frame->cpuPresentNanoseconds),
        coduomp_gpu_profile_msec(
            frame->phaseCpuNanoseconds[CODUOMP_GPU_PROFILE_PHASE_SCENE]),
        coduomp_gpu_profile_msec(
            frame->phaseCpuNanoseconds[CODUOMP_GPU_PROFILE_PHASE_2D]),
        coduomp_gpu_profile_msec(
            frame->phaseCpuNanoseconds[CODUOMP_GPU_PROFILE_PHASE_CLEAR]),
        coduomp_gpu_profile_msec(
            frame->phaseCpuNanoseconds[CODUOMP_GPU_PROFILE_PHASE_MISC]),
        coduomp_gpu_profile_msec(
            frame->cpuFrameNanoseconds >
                    frame->cpuPreFrontendNanoseconds +
                        frame->cpuFrontendNanoseconds +
                        frame->cpuBackendNanoseconds
                ? frame->cpuFrameNanoseconds -
                      frame->cpuPreFrontendNanoseconds -
                      frame->cpuFrontendNanoseconds -
                      frame->cpuBackendNanoseconds
                : 0));

    /* CPU-only stability captures deliberately omit the per-surface census.
     * Its material lookup, merge analysis, and multi-line output are useful
     * for batch investigations but measurably perturb high-rate frames. */
    if (frame->mode == 3)
        return;

    int32_t top[3];
    int32_t batchTop[3];
    int32_t sceneTop[3];
    int32_t entityTop[3];
    int32_t worldTop[CODUOMP_GPU_PROFILE_WORLD_TOP_COUNT];

    coduomp_gpu_profile_find_top_shaders(frame, top);
    coduomp_gpu_profile_find_top_batch_shaders(frame, batchTop);
    coduomp_gpu_profile_find_top_scene_shaders(frame, sceneTop);
    coduomp_gpu_profile_find_top_entity_break_shaders(frame, entityTop);
    coduomp_gpu_profile_find_top_world_shaders(frame, worldTop);

    if (top[0] >= 0 && frame->shaders[top[0]].nanoseconds != 0) {
        fprintf(
            coduompGpuProfileLogFile,
            "GPU_PROFILE_TOP frame=%u shader1=%s:%.3f shader2=%s:%.3f "
            "shader3=%s:%.3f shader1_batches=%u shader2_batches=%u "
            "shader3_batches=%u untracked_ms=%.3f untracked_batches=%u\n",
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
            frame->shaders[top[0]].batches,
            top[1] >= 0 ? frame->shaders[top[1]].batches : 0,
            top[2] >= 0 ? frame->shaders[top[2]].batches : 0,
            coduomp_gpu_profile_msec(
                frame->untrackedShaderNanoseconds),
            frame->untrackedShaderBatches);
    }

    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE_BATCHES frame=%u view=%u scene=%u world=%u bmodel=%u model=%u "
        "smodel=%u effects=%u sky=%u shadows=%u flares=%u 2d=%u clear=%u "
        "copy=%u present=%u misc=%u\n",
        frame->serial,
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_VIEW_SETUP],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_SCENE],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_WORLD],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_BRUSH_MODELS],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_MODELS],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_STATIC_MODELS],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_EFFECTS],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_SKY],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_SHADOWS],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_FLARES],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_2D],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_CLEAR],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_SCREEN_COPY],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_PRESENT],
        frame->phaseBatches[CODUOMP_GPU_PROFILE_PHASE_MISC]);

    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE_DRAWS frame=%u total_batches=%u total_draws=%u "
        "portal_batches=%u portal_draws=%u main_batches=%u main_draws=%u "
        "world=%u bmodel=%u model=%u smodel=%u effects=%u sky=%u "
        "shadows=%u flares=%u 2d=%u misc=%u\n",
        frame->serial, coduomp_gpu_profile_frame_batches(frame),
        coduomp_gpu_profile_frame_draw_calls(frame),
        frame->portalBatches, frame->portalDrawCalls,
        frame->mainSceneBatches, frame->mainSceneDrawCalls,
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_WORLD],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_BRUSH_MODELS],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_MODELS],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_STATIC_MODELS],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_EFFECTS],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_SKY],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_SHADOWS],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_FLARES],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_2D],
        frame->phaseDrawCalls[CODUOMP_GPU_PROFILE_PHASE_MISC]);

    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE_BREAKS frame=%u transitions=%u shader=%u storage=%u "
        "dlight=%u batch2=%u entity=%u overflow=%u overlapping=1\n",
        frame->serial, frame->drawSurfBreaks, frame->shaderBreaks,
        frame->storageBreaks, frame->dlightBreaks,
        frame->batchFlag2Breaks, frame->entityBreaks,
        frame->overflowBreaks);

    if (batchTop[0] >= 0) {
        fprintf(
            coduompGpuProfileLogFile,
            "GPU_PROFILE_BATCH_TOP frame=%u shader1=%s:%u/%u "
            "shader2=%s:%u/%u shader3=%s:%u/%u "
            "untracked_batches=%u\n",
            frame->serial,
            frame->shaders[batchTop[0]].name,
            frame->shaders[batchTop[0]].drawCalls,
            frame->shaders[batchTop[0]].batches,
            batchTop[1] >= 0 ? frame->shaders[batchTop[1]].name : "-",
            batchTop[1] >= 0 ? frame->shaders[batchTop[1]].drawCalls : 0,
            batchTop[1] >= 0 ? frame->shaders[batchTop[1]].batches : 0,
            batchTop[2] >= 0 ? frame->shaders[batchTop[2]].name : "-",
            batchTop[2] >= 0 ? frame->shaders[batchTop[2]].drawCalls : 0,
            batchTop[2] >= 0 ? frame->shaders[batchTop[2]].batches : 0,
            frame->untrackedShaderBatches);
    }

    if (sceneTop[0] >= 0 &&
        frame->shaders[sceneTop[0]].sceneDrawCalls != 0) {
        fprintf(
            coduompGpuProfileLogFile,
            "GPU_PROFILE_SCENE_TOP frame=%u shader1=%s:%u "
            "shader2=%s:%u shader3=%s:%u\n",
            frame->serial,
            frame->shaders[sceneTop[0]].name,
            frame->shaders[sceneTop[0]].sceneDrawCalls,
            sceneTop[1] >= 0 ? frame->shaders[sceneTop[1]].name : "-",
            sceneTop[1] >= 0
                ? frame->shaders[sceneTop[1]].sceneDrawCalls : 0,
            sceneTop[2] >= 0 ? frame->shaders[sceneTop[2]].name : "-",
            sceneTop[2] >= 0
                ? frame->shaders[sceneTop[2]].sceneDrawCalls : 0);
    }

    if (entityTop[0] >= 0 &&
        frame->shaders[entityTop[0]].entityBreaks != 0) {
        fprintf(
            coduompGpuProfileLogFile,
            "GPU_PROFILE_ENTITY_TOP frame=%u shader1=%s:%u "
            "shader2=%s:%u shader3=%s:%u\n",
            frame->serial,
            frame->shaders[entityTop[0]].name,
            frame->shaders[entityTop[0]].entityBreaks,
            entityTop[1] >= 0 ? frame->shaders[entityTop[1]].name : "-",
            entityTop[1] >= 0
                ? frame->shaders[entityTop[1]].entityBreaks : 0,
            entityTop[2] >= 0 ? frame->shaders[entityTop[2]].name : "-",
            entityTop[2] >= 0
                ? frame->shaders[entityTop[2]].entityBreaks : 0);
    }

    coduomp_gpu_profile_write_world_merge_census(frame);
    if (worldTop[0] >= 0 &&
        frame->shaders[worldTop[0]].worldDrawCalls != 0 &&
        frame->serial % 60u == 0) {
        for (int32_t rank = 0;
             rank < CODUOMP_GPU_PROFILE_WORLD_TOP_COUNT; ++rank) {
            if (worldTop[rank] < 0)
                break;
            const coduomp_gpu_profile_shader_t *entry =
                &frame->shaders[worldTop[rank]];
            const image_t *image = entry->shader != NULL
                                       ? entry->shader->primaryImage
                                       : NULL;
            fprintf(
                coduompGpuProfileLogFile,
                "GPU_PROFILE_WORLD_MATERIAL frame=%u rank=%d shader=%s "
                "batches=%u draws=%u image=%s upload=%ux%u format=0x%x "
                "flags=0x%x\n",
                frame->serial, rank + 1, entry->name,
                entry->worldBatches, entry->worldDrawCalls,
                image != NULL ? image->imgName : "-",
                image != NULL ? image->uploadWidth : 0,
                image != NULL ? image->uploadHeight : 0,
                image != NULL ? image->internalFormat : 0,
                image != NULL ? image->flags : 0);
        }
    }

    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE_BREAK_MASKS frame=%u shader_only=%u entity_only=%u "
        "shader_entity=%u other=%u\n",
        frame->serial,
        frame->breakReasonMasks[CODUOMP_GPU_PROFILE_BREAK_SHADER],
        frame->breakReasonMasks[CODUOMP_GPU_PROFILE_BREAK_ENTITY],
        frame->breakReasonMasks[CODUOMP_GPU_PROFILE_BREAK_SHADER |
                                CODUOMP_GPU_PROFILE_BREAK_ENTITY],
        frame->drawSurfBreaks -
            frame->breakReasonMasks[CODUOMP_GPU_PROFILE_BREAK_SHADER] -
            frame->breakReasonMasks[CODUOMP_GPU_PROFILE_BREAK_ENTITY] -
            frame->breakReasonMasks[CODUOMP_GPU_PROFILE_BREAK_SHADER |
                                    CODUOMP_GPU_PROFILE_BREAK_ENTITY]);
}

/* NOT_FROM_ORIGINAL_SOURCE: periodically report average phase costs even when
 * no individual frame exceeds the configured slow-frame threshold. */
static void coduomp_gpu_profile_write_summary(void)
{
    uint64_t totalNanoseconds = 0;

    for (int32_t phase = 0;
         phase < CODUOMP_GPU_PROFILE_PHASE_COUNT; ++phase) {
        totalNanoseconds += coduompGpuProfileSummaryNanoseconds[phase];
    }

    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE_SUMMARY frames=%u avg_total_ms=%.3f max_total_ms=%.3f "
        "avg_view_ms=%.3f avg_scene_ms=%.3f avg_world_ms=%.3f avg_bmodel_ms=%.3f "
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
            CODUOMP_GPU_PROFILE_PHASE_SCENE),
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

    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE_CENSUS_SUMMARY frames=%u avg_batches=%.3f avg_draws=%.3f "
        "avg_portal_batches=%.3f avg_portal_draws=%.3f "
        "avg_main_batches=%.3f avg_main_draws=%.3f "
        "breaks=%llu shader=%llu storage=%llu dlight=%llu batch2=%llu "
        "entity=%llu exact_shader=%llu overflow=%llu overlapping=1\n",
        coduompGpuProfileSummaryCount,
        (double)coduompGpuProfileSummaryBatches /
            (double)coduompGpuProfileSummaryCount,
        (double)coduompGpuProfileSummaryDrawCalls /
            (double)coduompGpuProfileSummaryCount,
        (double)coduompGpuProfileSummaryPortalBatches /
            (double)coduompGpuProfileSummaryCount,
        (double)coduompGpuProfileSummaryPortalDrawCalls /
            (double)coduompGpuProfileSummaryCount,
        (double)coduompGpuProfileSummaryMainSceneBatches /
            (double)coduompGpuProfileSummaryCount,
        (double)coduompGpuProfileSummaryMainSceneDrawCalls /
            (double)coduompGpuProfileSummaryCount,
        (unsigned long long)coduompGpuProfileSummaryDrawSurfBreaks,
        (unsigned long long)coduompGpuProfileSummaryShaderBreaks,
        (unsigned long long)coduompGpuProfileSummaryStorageBreaks,
        (unsigned long long)coduompGpuProfileSummaryDlightBreaks,
        (unsigned long long)coduompGpuProfileSummaryBatchFlag2Breaks,
        (unsigned long long)coduompGpuProfileSummaryEntityBreaks,
        (unsigned long long)coduompGpuProfileSummaryExactShaderBreaks,
        (unsigned long long)coduompGpuProfileSummaryOverflowBreaks);

    fprintf(
        coduompGpuProfileLogFile,
        "GPU_PROFILE_CENSUS_BREAK_MASKS frames=%u shader_only=%llu "
        "entity_only=%llu shader_entity=%llu other=%llu\n",
        coduompGpuProfileSummaryCount,
        (unsigned long long)coduompGpuProfileSummaryBreakReasonMasks[
            CODUOMP_GPU_PROFILE_BREAK_SHADER],
        (unsigned long long)coduompGpuProfileSummaryBreakReasonMasks[
            CODUOMP_GPU_PROFILE_BREAK_ENTITY],
        (unsigned long long)coduompGpuProfileSummaryBreakReasonMasks[
            CODUOMP_GPU_PROFILE_BREAK_SHADER |
            CODUOMP_GPU_PROFILE_BREAK_ENTITY],
        (unsigned long long)(
            coduompGpuProfileSummaryDrawSurfBreaks -
            coduompGpuProfileSummaryBreakReasonMasks[
                CODUOMP_GPU_PROFILE_BREAK_SHADER] -
            coduompGpuProfileSummaryBreakReasonMasks[
                CODUOMP_GPU_PROFILE_BREAK_ENTITY] -
            coduompGpuProfileSummaryBreakReasonMasks[
                CODUOMP_GPU_PROFILE_BREAK_SHADER |
                CODUOMP_GPU_PROFILE_BREAK_ENTITY]));

    coduompGpuProfileSummaryCount = 0;
    coduompGpuProfileSummaryMaximumNanoseconds = 0;
    coduompGpuProfileSummaryDroppedQueries = 0;
    coduompGpuProfileSummaryBatches = 0;
    coduompGpuProfileSummaryDrawCalls = 0;
    coduompGpuProfileSummaryPortalBatches = 0;
    coduompGpuProfileSummaryPortalDrawCalls = 0;
    coduompGpuProfileSummaryMainSceneBatches = 0;
    coduompGpuProfileSummaryMainSceneDrawCalls = 0;
    coduompGpuProfileSummaryDrawSurfBreaks = 0;
    coduompGpuProfileSummaryShaderBreaks = 0;
    coduompGpuProfileSummaryStorageBreaks = 0;
    coduompGpuProfileSummaryDlightBreaks = 0;
    coduompGpuProfileSummaryBatchFlag2Breaks = 0;
    coduompGpuProfileSummaryEntityBreaks = 0;
    coduompGpuProfileSummaryExactShaderBreaks = 0;
    coduompGpuProfileSummaryOverflowBreaks = 0;
    memset(coduompGpuProfileSummaryBreakReasonMasks, 0,
           sizeof(coduompGpuProfileSummaryBreakReasonMasks));
    memset(coduompGpuProfileSummaryNanoseconds, 0,
           sizeof(coduompGpuProfileSummaryNanoseconds));
}

/* NOT_FROM_ORIGINAL_SOURCE: finish and flush a capture only after every
 * delayed query from it has resolved, keeping later captures in the same log. */
static void coduomp_gpu_profile_pause_log(void)
{
    if (coduompGpuProfileLogFile == NULL)
        return;

    if (coduompGpuProfileSummaryCount != 0)
        coduomp_gpu_profile_write_summary();
    fprintf(coduompGpuProfileLogFile,
            "GPU_PROFILE_END next_frame=%u\n",
            coduompGpuProfileNextFrameSerial + 1);
    fflush(coduompGpuProfileLogFile);
    coduompGpuProfileWasEnabled = qfalse;
    ri.Printf(R_PRINT_ALL, "GPU profiling paused; flushed %s\n",
              coduompGpuProfileLogPath);
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
        coduomp_gpu_profile_write_frame(frame, totalNanoseconds);
    }

    if (frame->mode == 3) {
        memset(frame, 0, sizeof(*frame));
        return;
    }

    ++coduompGpuProfileSummaryCount;
    if (totalNanoseconds > coduompGpuProfileSummaryMaximumNanoseconds)
        coduompGpuProfileSummaryMaximumNanoseconds = totalNanoseconds;
    coduompGpuProfileSummaryDroppedQueries += frame->droppedQueries;
    coduompGpuProfileSummaryBatches +=
        coduomp_gpu_profile_frame_batches(frame);
    coduompGpuProfileSummaryDrawCalls +=
        coduomp_gpu_profile_frame_draw_calls(frame);
    coduompGpuProfileSummaryPortalBatches += frame->portalBatches;
    coduompGpuProfileSummaryPortalDrawCalls += frame->portalDrawCalls;
    coduompGpuProfileSummaryMainSceneBatches += frame->mainSceneBatches;
    coduompGpuProfileSummaryMainSceneDrawCalls +=
        frame->mainSceneDrawCalls;
    coduompGpuProfileSummaryDrawSurfBreaks += frame->drawSurfBreaks;
    coduompGpuProfileSummaryShaderBreaks += frame->shaderBreaks;
    coduompGpuProfileSummaryStorageBreaks += frame->storageBreaks;
    coduompGpuProfileSummaryDlightBreaks += frame->dlightBreaks;
    coduompGpuProfileSummaryBatchFlag2Breaks += frame->batchFlag2Breaks;
    coduompGpuProfileSummaryEntityBreaks += frame->entityBreaks;
    coduompGpuProfileSummaryExactShaderBreaks +=
        frame->exactShaderBreaks;
    coduompGpuProfileSummaryOverflowBreaks += frame->overflowBreaks;
    for (int32_t reasonMask = 0; reasonMask < 32; ++reasonMask) {
        coduompGpuProfileSummaryBreakReasonMasks[reasonMask] +=
            frame->breakReasonMasks[reasonMask];
    }
    for (int32_t phase = 0;
         phase < CODUOMP_GPU_PROFILE_PHASE_COUNT; ++phase) {
        coduompGpuProfileSummaryNanoseconds[phase] +=
            frame->phaseNanoseconds[phase];
    }

    if (coduompGpuProfileSummaryCount >=
        CODUOMP_GPU_PROFILE_SUMMARY_FRAMES) {
        coduomp_gpu_profile_write_summary();
    }

    memset(frame, 0, sizeof(*frame));
}

/* NOT_FROM_ORIGINAL_SOURCE: find or add a CPU-side shader census entry using
 * pointer identity so recording a batch does not compare material strings. */
static int32_t coduomp_gpu_profile_find_shader(
    coduomp_gpu_profile_frame_t *frame, const shader_t *shader)
{
    if (shader == NULL)
        return -1;

    for (int32_t shaderIndex = 0;
         shaderIndex < frame->shaderCount; ++shaderIndex) {
        if (frame->shaders[shaderIndex].shader == shader)
            return shaderIndex;
    }

    if (frame->shaderCount >= CODUOMP_GPU_PROFILE_SHADER_CAPACITY)
        return -1;

    const int32_t shaderIndex = frame->shaderCount++;
    frame->shaders[shaderIndex].shader = shader;
    Q_strncpyz(frame->shaders[shaderIndex].name, shader->name,
               sizeof(frame->shaders[shaderIndex].name));
    return shaderIndex;
}

/* NOT_FROM_ORIGINAL_SOURCE: aggregate a completed surface query by shader so
 * slow-frame records identify the renderer material that contributed most. */
static void coduomp_gpu_profile_add_shader_time(
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
                coduomp_gpu_profile_add_shader_time(
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
              "GPU profiling ready: r_gpuProfile 1 logs slow frames; 2 logs every frame; "
              "r_gpuProfileDetail 1 logs command phases; 2 enables invasive per-batch timing\n");
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: register runtime controls only in the dedicated
 * profiling build. Build it with:
 * make client BUILD_DIR=.workbench/build/gpu-profile
 * CODUOMP_RENDERER_GPU_PROFILE=1 */
void coduomp_gpu_profile_register(void)
{
    coduompGpuProfileFrontendStartNanoseconds = 0;
    coduompGpuProfileLastFrontendStartNanoseconds = 0;
    coduompGpuProfilePendingFrameIntervalNanoseconds = 0;
    coduompGpuProfilePendingFrontendNanoseconds = 0;
    coduompGpuProfilePendingFrameStartNanoseconds = 0;
    coduompGpuProfilePendingPreFrontendNanoseconds = 0;
    coduompGpuProfilePendingCgameNanoseconds = 0;
    coduompGpuProfilePendingRenderSceneNanoseconds = 0;
    memset(coduompGpuProfilePendingFrontendScopeNanoseconds, 0,
           sizeof(coduompGpuProfilePendingFrontendScopeNanoseconds));
    memset(coduompGpuProfilePendingFrontendScopeCounts, 0,
           sizeof(coduompGpuProfilePendingFrontendScopeCounts));
    coduompGpuProfileCgameStartNanoseconds = 0;
    coduompGpuProfileRenderSceneStartNanoseconds = 0;
    coduompGpuProfileFinishStartNanoseconds = 0;
    coduompGpuProfilePresentStartNanoseconds = 0;
    coduompGpuProfileCpuPhase = CODUOMP_GPU_PROFILE_PHASE_COUNT;
    coduompGpuProfileCpuSegmentStartNanoseconds = 0;
    coduompGpuProfileMode =
        ri.Cvar_Get("r_gpuProfile", "0", CVAR_TEMP);
    coduompGpuProfileSlowMsec =
        ri.Cvar_Get("r_gpuProfileSlowMsec", "4.0", CVAR_TEMP);
    coduompGpuProfileDetail =
        ri.Cvar_Get("r_gpuProfileDetail", "0", CVAR_TEMP);
}

/* NOT_FROM_ORIGINAL_SOURCE: start the client frame early enough to include
 * snapshot/demo processing and other work preceding renderer submission. */
void coduomp_gpu_profile_client_frame_begin(void)
{
    const uint64_t now = coduomp_gpu_profile_cpu_nanoseconds();

    coduompGpuProfilePendingFrameStartNanoseconds = now;
    coduompGpuProfilePendingFrameIntervalNanoseconds =
        coduompGpuProfileLastFrontendStartNanoseconds != 0
            ? now - coduompGpuProfileLastFrontendStartNanoseconds
            : 0;
    coduompGpuProfileLastFrontendStartNanoseconds = now;
}

/* NOT_FROM_ORIGINAL_SOURCE: begin the CPU portion of one rendered frame. The
 * interval between these calls includes simulation, event processing, frame
 * limiting, and the preceding renderer work. */
void coduomp_gpu_profile_frontend_begin(void)
{
    const uint64_t now = coduomp_gpu_profile_cpu_nanoseconds();

    coduompGpuProfileFrontendStartNanoseconds = now;
    coduompGpuProfilePendingCgameNanoseconds = 0;
    coduompGpuProfilePendingRenderSceneNanoseconds = 0;
    memset(coduompGpuProfilePendingFrontendScopeNanoseconds, 0,
           sizeof(coduompGpuProfilePendingFrontendScopeNanoseconds));
    memset(coduompGpuProfilePendingFrontendScopeCounts, 0,
           sizeof(coduompGpuProfilePendingFrontendScopeCounts));
    if (coduompGpuProfilePendingFrameStartNanoseconds == 0) {
        coduompGpuProfilePendingFrameStartNanoseconds = now;
        coduompGpuProfilePendingFrameIntervalNanoseconds =
            coduompGpuProfileLastFrontendStartNanoseconds != 0
                ? now - coduompGpuProfileLastFrontendStartNanoseconds
                : 0;
        coduompGpuProfileLastFrontendStartNanoseconds = now;
    }
    coduompGpuProfilePendingPreFrontendNanoseconds =
        now - coduompGpuProfilePendingFrameStartNanoseconds;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain frontend wall time until the synchronous
 * backend allocates the corresponding delayed GPU-query record. */
void coduomp_gpu_profile_frontend_end(void)
{
    if (coduompGpuProfileFrontendStartNanoseconds == 0)
        return;

    coduompGpuProfilePendingFrontendNanoseconds =
        coduomp_gpu_profile_cpu_nanoseconds() -
        coduompGpuProfileFrontendStartNanoseconds;
    coduompGpuProfileFrontendStartNanoseconds = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: begin timing cgame frame construction and its
 * renderer export calls as one frontend component. */
void coduomp_gpu_profile_cgame_begin(void)
{
    coduompGpuProfileCgameStartNanoseconds =
        coduomp_gpu_profile_cpu_nanoseconds();
}

/* NOT_FROM_ORIGINAL_SOURCE: finish the cgame frontend component. */
void coduomp_gpu_profile_cgame_end(void)
{
    if (coduompGpuProfileCgameStartNanoseconds == 0)
        return;

    coduompGpuProfilePendingCgameNanoseconds +=
        coduomp_gpu_profile_cpu_nanoseconds() -
        coduompGpuProfileCgameStartNanoseconds;
    coduompGpuProfileCgameStartNanoseconds = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: begin timing renderer visibility, sorting, and
 * draw-surface construction performed by RE_RenderScene. */
void coduomp_gpu_profile_render_scene_begin(void)
{
    coduompGpuProfileRenderSceneStartNanoseconds =
        coduomp_gpu_profile_cpu_nanoseconds();
}

/* NOT_FROM_ORIGINAL_SOURCE: finish one possibly repeated scene build. */
void coduomp_gpu_profile_render_scene_end(void)
{
    if (coduompGpuProfileRenderSceneStartNanoseconds == 0)
        return;

    coduompGpuProfilePendingRenderSceneNanoseconds +=
        coduomp_gpu_profile_cpu_nanoseconds() -
        coduompGpuProfileRenderSceneStartNanoseconds;
    coduompGpuProfileRenderSceneStartNanoseconds = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: start a nested frontend scope only while frame
 * capture is enabled. The returned timestamp keeps recursive renderer calls
 * independent instead of storing one global start value per scope. */
uint64_t coduomp_gpu_profile_frontend_scope_begin(void)
{
    if (coduompGpuProfileMode == NULL ||
        coduompGpuProfileMode->integer <= 0) {
        return 0;
    }
    return coduomp_gpu_profile_cpu_nanoseconds();
}

/* NOT_FROM_ORIGINAL_SOURCE: accumulate visibility and sorting subscopes for
 * the frontend record that the synchronous backend will claim later. */
void coduomp_gpu_profile_frontend_scope_end(
    coduomp_gpu_profile_frontend_scope_t scope,
    uint64_t startNanoseconds)
{
    if (startNanoseconds == 0 || scope < 0 ||
        scope >= CODUOMP_GPU_PROFILE_FRONTEND_SCOPE_COUNT) {
        return;
    }
    coduompGpuProfilePendingFrontendScopeNanoseconds[scope] +=
        coduomp_gpu_profile_cpu_nanoseconds() - startNanoseconds;
    ++coduompGpuProfilePendingFrontendScopeCounts[scope];
}

/* NOT_FROM_ORIGINAL_SOURCE: choose a free delayed-result record for the next
 * backend command list without overwriting in-flight data. */
void coduomp_gpu_profile_frame_begin(void)
{
    coduompGpuProfileCurrentFrame = -1;
    coduompGpuProfileSurfaceFrame = -1;
    coduompGpuProfileSurfaceShader = -1;
    if (coduompGpuProfileApiReady != qfalse)
        coduomp_gpu_profile_collect();

    if (coduompGpuProfileMode == NULL ||
        coduompGpuProfileMode->integer <= 0) {
        if (coduompGpuProfileWasEnabled != qfalse &&
            coduompGpuProfilePendingQueries == 0) {
            coduomp_gpu_profile_pause_log();
        }
        return;
    }

    if ((coduompGpuProfileMode->integer != 3 &&
         coduomp_gpu_profile_load_api() == qfalse) ||
        coduomp_gpu_profile_open_log() == qfalse) {
        return;
    }

    if (coduompGpuProfileWasEnabled == qfalse) {
        fprintf(coduompGpuProfileLogFile,
                "GPU_PROFILE_BEGIN mode=%d detail=%d slow_ms=%.3f\n",
                coduompGpuProfileMode->integer,
                coduompGpuProfileDetail != NULL
                    ? coduompGpuProfileDetail->integer
                    : 0,
                coduompGpuProfileSlowMsec != NULL
                    ? (double)coduompGpuProfileSlowMsec->value
                    : 0.0);
        coduompGpuProfileWasEnabled = qtrue;
        coduompGpuProfileCapacityWarningPrinted = qfalse;
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
        frame->detailLevel = coduompGpuProfileDetail != NULL
                                 ? coduompGpuProfileDetail->integer
                                 : 0;
        frame->slowMsec = coduompGpuProfileSlowMsec != NULL &&
                                  coduompGpuProfileSlowMsec->value > 0.0f
                              ? coduompGpuProfileSlowMsec->value
                              : 0.0f;
        frame->cpuFrameIntervalNanoseconds =
            coduompGpuProfilePendingFrameIntervalNanoseconds;
        frame->cpuFrameStartNanoseconds =
            coduompGpuProfilePendingFrameStartNanoseconds;
        frame->cpuFrontendNanoseconds =
            coduompGpuProfilePendingFrontendNanoseconds;
        frame->cpuPreFrontendNanoseconds =
            coduompGpuProfilePendingPreFrontendNanoseconds;
        frame->cpuCgameNanoseconds =
            coduompGpuProfilePendingCgameNanoseconds;
        frame->cpuRenderSceneNanoseconds =
            coduompGpuProfilePendingRenderSceneNanoseconds;
        memcpy(frame->frontendScopeCpuNanoseconds,
               coduompGpuProfilePendingFrontendScopeNanoseconds,
               sizeof(frame->frontendScopeCpuNanoseconds));
        memcpy(frame->frontendScopeCounts,
               coduompGpuProfilePendingFrontendScopeCounts,
               sizeof(frame->frontendScopeCounts));
        frame->cpuBackendStartNanoseconds =
            coduomp_gpu_profile_cpu_nanoseconds();
        coduompGpuProfilePendingFrameIntervalNanoseconds = 0;
        coduompGpuProfilePendingFrontendNanoseconds = 0;
        coduompGpuProfilePendingFrameStartNanoseconds = 0;
        coduompGpuProfilePendingPreFrontendNanoseconds = 0;
        coduompGpuProfilePendingCgameNanoseconds = 0;
        coduompGpuProfilePendingRenderSceneNanoseconds = 0;
        memset(coduompGpuProfilePendingFrontendScopeNanoseconds, 0,
               sizeof(coduompGpuProfilePendingFrontendScopeNanoseconds));
        memset(coduompGpuProfilePendingFrontendScopeCounts, 0,
               sizeof(coduompGpuProfilePendingFrontendScopeCounts));
        coduompGpuProfileCpuPhase = CODUOMP_GPU_PROFILE_PHASE_COUNT;
        coduompGpuProfileCpuSegmentStartNanoseconds = 0;
        coduompGpuProfileCurrentFrame = frameIndex;
        if (frame->detailLevel <= 0) {
            (void)coduomp_gpu_profile_begin(
                CODUOMP_GPU_PROFILE_PHASE_MISC, NULL);
        }
        return;
    }

    if (coduompGpuProfileCapacityWarningPrinted == qfalse) {
        coduompGpuProfileCapacityWarningPrinted = qtrue;
        ri.Printf(R_PRINT_WARNING,
                  "GPU profiling skipped frames: delayed records are full\n");
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: mark the current command list complete and poll
 * results without waiting for the GPU. */
void coduomp_gpu_profile_frame_end(void)
{
    const int32_t frameIndex = coduompGpuProfileCurrentFrame;
    uint64_t now;

    if (frameIndex < 0)
        return;

    if (coduompGpuProfileFrames[frameIndex].detailLevel <= 1 &&
        coduompGpuProfileActiveQuery >= 0) {
        coduomp_gpu_profile_end(qtrue);
    }

    now = coduomp_gpu_profile_cpu_nanoseconds();
    if (coduompGpuProfileCpuPhase < CODUOMP_GPU_PROFILE_PHASE_COUNT &&
        coduompGpuProfileCpuSegmentStartNanoseconds != 0) {
        coduompGpuProfileFrames[frameIndex]
            .phaseCpuNanoseconds[coduompGpuProfileCpuPhase] +=
            now - coduompGpuProfileCpuSegmentStartNanoseconds;
    }
    coduompGpuProfileCpuPhase = CODUOMP_GPU_PROFILE_PHASE_COUNT;
    coduompGpuProfileCpuSegmentStartNanoseconds = 0;
    coduompGpuProfileFrames[frameIndex].cpuBackendNanoseconds =
        now - coduompGpuProfileFrames[frameIndex].cpuBackendStartNanoseconds;
    if (coduompGpuProfileFrames[frameIndex].cpuFrameStartNanoseconds != 0) {
        coduompGpuProfileFrames[frameIndex].cpuFrameNanoseconds =
            now - coduompGpuProfileFrames[frameIndex].cpuFrameStartNanoseconds;
    }
    coduompGpuProfileFrames[frameIndex].refdefTime = backEnd.refdef.time;

    coduompGpuProfileFrames[frameIndex].closed = qtrue;
    coduomp_gpu_profile_collect();
    if (coduompGpuProfileFrames[frameIndex].used != qfalse &&
        coduompGpuProfileFrames[frameIndex].outstandingQueries == 0) {
        coduomp_gpu_profile_finish_frame(frameIndex);
    }
    coduompGpuProfileCurrentFrame = -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: measure any explicit glFinish wait separately
 * from ordinary backend command submission. */
void coduomp_gpu_profile_finish_begin(void)
{
    coduompGpuProfileFinishStartNanoseconds =
        coduomp_gpu_profile_cpu_nanoseconds();
}

/* NOT_FROM_ORIGINAL_SOURCE: finish the explicit GPU-wait measurement. */
void coduomp_gpu_profile_finish_end(void)
{
    if (coduompGpuProfileCurrentFrame < 0 ||
        coduompGpuProfileFinishStartNanoseconds == 0) {
        return;
    }

    coduompGpuProfileFrames[coduompGpuProfileCurrentFrame]
        .cpuFinishNanoseconds +=
        coduomp_gpu_profile_cpu_nanoseconds() -
        coduompGpuProfileFinishStartNanoseconds;
    coduompGpuProfileFinishStartNanoseconds = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: begin timing the platform presentation call,
 * including any compositor or drawable back-pressure. */
void coduomp_gpu_profile_present_begin(void)
{
    coduompGpuProfilePresentStartNanoseconds =
        coduomp_gpu_profile_cpu_nanoseconds();
}

/* NOT_FROM_ORIGINAL_SOURCE: finish the platform presentation measurement. */
void coduomp_gpu_profile_present_end(void)
{
    if (coduompGpuProfileCurrentFrame < 0 ||
        coduompGpuProfilePresentStartNanoseconds == 0) {
        return;
    }

    coduompGpuProfileFrames[coduompGpuProfileCurrentFrame]
        .cpuPresentNanoseconds +=
        coduomp_gpu_profile_cpu_nanoseconds() -
        coduompGpuProfilePresentStartNanoseconds;
    coduompGpuProfilePresentStartNanoseconds = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: group adjacent backend commands into a handful of
 * non-overlapping GPU scopes. This middle detail level avoids the render-pass
 * disruption caused by starting a timer query for every submitted batch. */
void coduomp_gpu_profile_segment(coduomp_gpu_profile_phase_t phase)
{
    coduomp_gpu_profile_frame_t *frame;
    qboolean cpuOnly;
    uint64_t now;

    if (coduompGpuProfileCurrentFrame < 0)
        return;

    frame = &coduompGpuProfileFrames[coduompGpuProfileCurrentFrame];
    cpuOnly = frame->mode == 3;
    if (frame->detailLevel != 1 && cpuOnly == qfalse)
        return;

    if (cpuOnly == qfalse && coduompGpuProfileActiveQuery >= 0) {
        const coduomp_gpu_profile_query_t *query =
            &coduompGpuProfileQueries[coduompGpuProfileActiveQuery];

        if (query->frameIndex == coduompGpuProfileCurrentFrame &&
            query->frameSerial == frame->serial &&
            query->phase == phase) {
            return;
        }
        coduomp_gpu_profile_end(qtrue);
    }

    now = coduomp_gpu_profile_cpu_nanoseconds();
    if (coduompGpuProfileCpuPhase < CODUOMP_GPU_PROFILE_PHASE_COUNT &&
        coduompGpuProfileCpuSegmentStartNanoseconds != 0) {
        frame->phaseCpuNanoseconds[coduompGpuProfileCpuPhase] +=
            now - coduompGpuProfileCpuSegmentStartNanoseconds;
    }
    coduompGpuProfileCpuPhase = phase;
    coduompGpuProfileCpuSegmentStartNanoseconds = now;
    if (cpuOnly == qfalse)
        (void)coduomp_gpu_profile_begin(phase, NULL);
}

/* NOT_FROM_ORIGINAL_SOURCE: close the low-overhead command-group query before
 * entering platform code that may block without submitting GPU work. */
void coduomp_gpu_profile_suspend_segment(void)
{
    coduomp_gpu_profile_frame_t *frame;
    uint64_t now;

    if (coduompGpuProfileCurrentFrame < 0)
        return;

    frame = &coduompGpuProfileFrames[coduompGpuProfileCurrentFrame];
    if (frame->mode != 3 && frame->detailLevel == 1 &&
        coduompGpuProfileActiveQuery >= 0) {
        coduomp_gpu_profile_end(qtrue);
    }
    now = coduomp_gpu_profile_cpu_nanoseconds();
    if ((frame->mode == 3 || frame->detailLevel == 1) &&
        coduompGpuProfileCpuPhase < CODUOMP_GPU_PROFILE_PHASE_COUNT &&
        coduompGpuProfileCpuSegmentStartNanoseconds != 0) {
        frame->phaseCpuNanoseconds[coduompGpuProfileCpuPhase] +=
            now - coduompGpuProfileCpuSegmentStartNanoseconds;
        coduompGpuProfileCpuPhase = CODUOMP_GPU_PROFILE_PHASE_COUNT;
        coduompGpuProfileCpuSegmentStartNanoseconds = 0;
    }
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
    coduomp_gpu_profile_frame_t *frame;
    int32_t shaderIndex;
    qboolean portalScene;
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

    if (coduompGpuProfileCurrentFrame < 0)
        return qfalse;

    frame = &coduompGpuProfileFrames[coduompGpuProfileCurrentFrame];
    if (phase == CODUOMP_GPU_PROFILE_PHASE_WORLD)
        frame->sawWorld = qtrue;
    if (frame->mode == 3)
        return qfalse;
    ++frame->phaseBatches[phase];

    shaderIndex = coduomp_gpu_profile_find_shader(frame, tess.shader);
    if (shaderIndex >= 0) {
        ++frame->shaders[shaderIndex].batches;
        if (phase == CODUOMP_GPU_PROFILE_PHASE_WORLD)
            ++frame->shaders[shaderIndex].worldBatches;
    } else if (tess.shader != NULL) {
        ++frame->untrackedShaderBatches;
    }

    portalScene = coduomp_gpu_profile_phase_is_scene(phase) != qfalse &&
                  (backEnd.refdef.rdflags & RDF_SKYBOX_PORTAL) != 0;
    if (portalScene != qfalse) {
        ++frame->portalBatches;
    } else if (coduomp_gpu_profile_phase_is_scene(phase) != qfalse) {
        ++frame->mainSceneBatches;
    }

    coduompGpuProfileSurfaceFrame = coduompGpuProfileCurrentFrame;
    coduompGpuProfileSurfaceFrameSerial = frame->serial;
    coduompGpuProfileSurfaceShader = shaderIndex;
    coduompGpuProfileSurfacePhase = phase;
    coduompGpuProfileSurfacePortal = portalScene;

    if (frame->detailLevel < 2)
        return qfalse;
    return coduomp_gpu_profile_begin(phase, shaderName);
}

/* NOT_FROM_ORIGINAL_SOURCE: end an instrumented surface and clear its
 * CPU-side draw-call attribution after the optional timer query closes. */
void coduomp_gpu_profile_end_surface(qboolean started)
{
    coduomp_gpu_profile_end(started);
    coduompGpuProfileSurfaceFrame = -1;
    coduompGpuProfileSurfaceShader = -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: count one actual GL draw under the active
 * tessellation batch without starting a timer query or synchronizing. */
void coduomp_gpu_profile_record_draw_call(void)
{
    coduomp_gpu_profile_frame_t *frame;

    if (coduompGpuProfileCurrentFrame >= 0 &&
        coduompGpuProfileFrames[coduompGpuProfileCurrentFrame].mode == 3) {
        return;
    }

    if (coduompGpuProfileSurfaceFrame < 0 ||
        coduompGpuProfileSurfaceFrame >= CODUOMP_GPU_PROFILE_FRAME_CAPACITY) {
        if (coduompGpuProfileCurrentFrame >= 0) {
            frame = &coduompGpuProfileFrames[coduompGpuProfileCurrentFrame];
            ++frame->phaseDrawCalls[
                backEnd.projection2D != qfalse
                    ? CODUOMP_GPU_PROFILE_PHASE_2D
                    : CODUOMP_GPU_PROFILE_PHASE_MISC];
        }
        return;
    }

    frame = &coduompGpuProfileFrames[coduompGpuProfileSurfaceFrame];
    if (frame->used == qfalse ||
        frame->serial != coduompGpuProfileSurfaceFrameSerial) {
        return;
    }

    ++frame->phaseDrawCalls[coduompGpuProfileSurfacePhase];
    if (coduompGpuProfileSurfaceShader >= 0 &&
        coduompGpuProfileSurfaceShader < frame->shaderCount) {
        ++frame->shaders[coduompGpuProfileSurfaceShader].drawCalls;
        if (coduomp_gpu_profile_phase_is_scene(
                coduompGpuProfileSurfacePhase) != qfalse) {
            ++frame->shaders[coduompGpuProfileSurfaceShader]
                  .sceneDrawCalls;
        }
        if (coduompGpuProfileSurfacePhase ==
            CODUOMP_GPU_PROFILE_PHASE_WORLD) {
            ++frame->shaders[coduompGpuProfileSurfaceShader]
                  .worldDrawCalls;
        }
    }
    if (coduompGpuProfileSurfacePortal != qfalse)
        ++frame->portalDrawCalls;
    else if (coduomp_gpu_profile_phase_is_scene(
                 coduompGpuProfileSurfacePhase) != qfalse)
        ++frame->mainSceneDrawCalls;
}

/* NOT_FROM_ORIGINAL_SOURCE: record every changed draw-surface key field that
 * contributed to a batch transition. Counts overlap intentionally. */
void coduomp_gpu_profile_note_drawsurf_break(
    uint32_t reasons, const shader_t *nextShader)
{
    coduomp_gpu_profile_frame_t *frame;

    if (coduompGpuProfileCurrentFrame < 0 ||
        (tess.indexCount == 0 && tess.renderedIndexCount == 0)) {
        return;
    }
    frame = &coduompGpuProfileFrames[coduompGpuProfileCurrentFrame];
    if (frame->mode == 3)
        return;
    ++frame->drawSurfBreaks;
    ++frame->breakReasonMasks[reasons & 31u];
    if ((reasons & CODUOMP_GPU_PROFILE_BREAK_SHADER) != 0)
        ++frame->shaderBreaks;
    if ((reasons & CODUOMP_GPU_PROFILE_BREAK_SHADER) != 0 &&
        tess.shader != NULL && nextShader != NULL &&
        CompareMergableShaders(
            tess.shader, nextShader, NULL, NULL) == 0) {
        ++frame->exactShaderBreaks;
    }
    if ((reasons & CODUOMP_GPU_PROFILE_BREAK_STORAGE) != 0)
        ++frame->storageBreaks;
    if ((reasons & CODUOMP_GPU_PROFILE_BREAK_DLIGHT) != 0)
        ++frame->dlightBreaks;
    if ((reasons & CODUOMP_GPU_PROFILE_BREAK_BATCH_FLAG2) != 0)
        ++frame->batchFlag2Breaks;
    if ((reasons & CODUOMP_GPU_PROFILE_BREAK_ENTITY) != 0)
        ++frame->entityBreaks;
    if ((reasons & CODUOMP_GPU_PROFILE_BREAK_ENTITY) != 0) {
        const int32_t shaderIndex =
            coduomp_gpu_profile_find_shader(frame, tess.shader);
        if (shaderIndex >= 0)
            ++frame->shaders[shaderIndex].entityBreaks;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: identify capacity-driven submissions separately
 * from sort-key transitions. */
void coduomp_gpu_profile_note_overflow(void)
{
    if (coduompGpuProfileCurrentFrame >= 0 &&
        coduompGpuProfileFrames[coduompGpuProfileCurrentFrame].mode != 3) {
        ++coduompGpuProfileFrames[coduompGpuProfileCurrentFrame]
              .overflowBreaks;
    }
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
    if (coduompGpuProfileApiReady != qfalse)
        coduomp_gpu_profile_collect();

    if (coduompGpuProfileLogFile != NULL) {
        if (coduompGpuProfileSummaryCount != 0)
            coduomp_gpu_profile_write_summary();
        if (coduompGpuProfileWasEnabled != qfalse) {
            fprintf(coduompGpuProfileLogFile,
                    "GPU_PROFILE_END shutdown=1 pending_queries=%d\n",
                    coduompGpuProfilePendingQueries);
        }
        fclose(coduompGpuProfileLogFile);
        if (coduompGpuProfileWasEnabled != qfalse) {
            ri.Printf(R_PRINT_ALL, "GPU profiling stopped; closed %s\n",
                      coduompGpuProfileLogPath);
        }
    }

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
    coduompGpuProfileSummaryBatches = 0;
    coduompGpuProfileSummaryDrawCalls = 0;
    coduompGpuProfileSummaryPortalBatches = 0;
    coduompGpuProfileSummaryPortalDrawCalls = 0;
    coduompGpuProfileSummaryMainSceneBatches = 0;
    coduompGpuProfileSummaryMainSceneDrawCalls = 0;
    coduompGpuProfileSummaryDrawSurfBreaks = 0;
    coduompGpuProfileSummaryShaderBreaks = 0;
    coduompGpuProfileSummaryStorageBreaks = 0;
    coduompGpuProfileSummaryDlightBreaks = 0;
    coduompGpuProfileSummaryBatchFlag2Breaks = 0;
    coduompGpuProfileSummaryEntityBreaks = 0;
    coduompGpuProfileSummaryExactShaderBreaks = 0;
    coduompGpuProfileSummaryOverflowBreaks = 0;
    memset(coduompGpuProfileSummaryBreakReasonMasks, 0,
           sizeof(coduompGpuProfileSummaryBreakReasonMasks));
    coduompGpuProfileSurfaceFrame = -1;
    coduompGpuProfileSurfaceFrameSerial = 0;
    coduompGpuProfileSurfaceShader = -1;
    coduompGpuProfileSurfacePhase = CODUOMP_GPU_PROFILE_PHASE_MISC;
    coduompGpuProfileSurfacePortal = qfalse;
    coduompGpuProfileLogFile = NULL;
    coduompGpuProfileLogOpenAttempted = qfalse;
    coduompGpuProfileWasEnabled = qfalse;
    coduompGpuProfileCapacityWarningPrinted = qfalse;
}

#endif
