#include "backend.h"
#include "gl_api.h"
#include "gl_state.h"
#include "renderer_cvars.h"

#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"

#include <math.h>
#include <string.h>

enum {
    R_MAX_FLARES = 128
};

/* Persistent flare state shared by RB_AddFlare, RB_TestFlare, and
 * RB_RenderFlare. The adjacent Windows bodies prove every i386 offset: the
 * view identity prevents a portal view from reusing a main-view flare, while
 * fadeTime/drawIntensity preserve visibility transitions across frames. */
typedef struct renderer_flare_s {
    struct renderer_flare_s *next;          /* original +0x00 */
    int32_t addedFrame;                      /* original +0x04 */
    qboolean portalView;                     /* original +0x08 */
    int32_t frameSceneNum;                   /* original +0x0c */
    int32_t fadeInMsec;                      /* original +0x10 */
    int32_t fadeOutMsec;                     /* original +0x14 */
    shader_t *shader;                        /* original +0x18 */
    qboolean active;                         /* original +0x1c */
    int32_t fadeTime;                        /* original +0x20 */
    float drawIntensity;                     /* original +0x24 */
    int32_t windowX;                         /* original +0x28 */
    int32_t windowY;                         /* original +0x2c */
    float eyeZ;                              /* original +0x30 */
    vec3_t color;                            /* original +0x34 */
    float size;                              /* original +0x40 */
    int32_t screenRadius;                    /* original +0x44 */
    int32_t id;                              /* original +0x48 */
} renderer_flare_t;

static renderer_flare_t rendererFlarePool[R_MAX_FLARES];
static renderer_flare_t *rendererActiveFlares;
static renderer_flare_t *rendererFreeFlares;
static float rendererSunFlareVisibility;


#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_flare_t) == 0x4, "renderer_flare_t original alignment");
_Static_assert(offsetof(renderer_flare_t, next) == 0x00, "renderer_flare_t next offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->next) == 0x04, "renderer_flare_t next extent");
_Static_assert(offsetof(renderer_flare_t, addedFrame) == 0x04, "renderer_flare_t addedFrame offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->addedFrame) == 0x04, "renderer_flare_t addedFrame extent");
_Static_assert(offsetof(renderer_flare_t, portalView) == 0x08, "renderer_flare_t portalView offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->portalView) == 0x04, "renderer_flare_t portalView extent");
_Static_assert(offsetof(renderer_flare_t, frameSceneNum) == 0x0c, "renderer_flare_t frameSceneNum offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->frameSceneNum) == 0x04, "renderer_flare_t frameSceneNum extent");
_Static_assert(offsetof(renderer_flare_t, fadeInMsec) == 0x10, "renderer_flare_t fadeInMsec offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->fadeInMsec) == 0x04, "renderer_flare_t fadeInMsec extent");
_Static_assert(offsetof(renderer_flare_t, fadeOutMsec) == 0x14, "renderer_flare_t fadeOutMsec offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->fadeOutMsec) == 0x04, "renderer_flare_t fadeOutMsec extent");
_Static_assert(offsetof(renderer_flare_t, shader) == 0x18, "renderer_flare_t shader offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->shader) == 0x04, "renderer_flare_t shader extent");
_Static_assert(offsetof(renderer_flare_t, active) == 0x1c, "renderer_flare_t active offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->active) == 0x04, "renderer_flare_t active extent");
_Static_assert(offsetof(renderer_flare_t, fadeTime) == 0x20, "renderer_flare_t fadeTime offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->fadeTime) == 0x04, "renderer_flare_t fadeTime extent");
_Static_assert(offsetof(renderer_flare_t, drawIntensity) == 0x24, "renderer_flare_t drawIntensity offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->drawIntensity) == 0x04, "renderer_flare_t drawIntensity extent");
_Static_assert(offsetof(renderer_flare_t, windowX) == 0x28, "renderer_flare_t windowX offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->windowX) == 0x04, "renderer_flare_t windowX extent");
_Static_assert(offsetof(renderer_flare_t, windowY) == 0x2c, "renderer_flare_t windowY offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->windowY) == 0x04, "renderer_flare_t windowY extent");
_Static_assert(offsetof(renderer_flare_t, eyeZ) == 0x30, "renderer_flare_t eyeZ offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->eyeZ) == 0x04, "renderer_flare_t eyeZ extent");
_Static_assert(offsetof(renderer_flare_t, color) == 0x34, "renderer_flare_t color offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->color) == 0x0c, "renderer_flare_t color extent");
_Static_assert(offsetof(renderer_flare_t, size) == 0x40, "renderer_flare_t size offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->size) == 0x04, "renderer_flare_t size extent");
_Static_assert(offsetof(renderer_flare_t, screenRadius) == 0x44, "renderer_flare_t screenRadius offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->screenRadius) == 0x04, "renderer_flare_t screenRadius extent");
_Static_assert(offsetof(renderer_flare_t, id) == 0x48, "renderer_flare_t id offset");
_Static_assert(sizeof(((renderer_flare_t *)0)->id) == 0x04, "renderer_flare_t id extent");
_Static_assert(sizeof(renderer_flare_t) == 0x4c, "renderer_flare_t original size");
#endif

/* Source: CoDUOMP.exe 0x004eed90..0x004eedd9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eed90_004eedda.mcode.
 * Name: exact same-module Mac symbol RE_ClearFlares. MSVC inlines R_ClearSun
 * and the zero fill, then links the 128 pool records from the highest address
 * back toward the first record. */
void RE_ClearFlares(void)
{
    memset(rendererFlarePool, 0, sizeof(rendererFlarePool));
    rendererActiveFlares = NULL;
    rendererSunFlareVisibility = 0.0f;
    R_ClearSun();

    renderer_flare_t *previous = NULL;
    for (int32_t flareIndex = 0; flareIndex < R_MAX_FLARES; ++flareIndex) {
        rendererFlarePool[flareIndex].next = previous;
        previous = &rendererFlarePool[flareIndex];
    }
    rendererFreeFlares = previous;
}

/* Source: CoDUOMP.exe 0x004eede0..0x004ef108.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eede0_004ef109.mcode.
 * Name and source signature: exact same-module Mac symbol RB_AddFlare. A
 * persistent flare is keyed by source id, portal-view identity, and scene
 * number. New records are clipped before allocation; existing records remain
 * linked while their visibility fades. The depthOffset path uses the exact
 * -12.0f displacement constant at 0x005b9da0. */
void RB_AddFlare(const renderer_flare_source_t *source, const vec3_t direction)
{
    vec4_t eye;
    vec4_t sourcePosition = {source->origin[0], source->origin[1], source->origin[2], source->depthOffset};
    vec4_t clip;
    vec4_t normalized;
    vec4_t window;
    renderer_flare_t *flare;

    ++backEnd.pc.flareAddCount;

    for (flare = rendererActiveFlares; flare != NULL; flare = flare->next) {
        if (flare->id == source->id && flare->frameSceneNum == backEnd.viewParms.frameSceneNum &&
            flare->portalView == backEnd.viewParms.isPortal) {
            break;
        }
    }

    if (source->depthOffset != 0.0f) {
        vec3_t offsetDirection = {source->origin[0] - source->depthOffset * backEnd.viewParms.orientation.origin[0],
                                  source->origin[1] - source->depthOffset * backEnd.viewParms.orientation.origin[1],
                                  source->origin[2] - source->depthOffset * backEnd.viewParms.orientation.origin[2]};
        VectorNormalizeFast(offsetDirection);

        const float displacement = source->depthOffset * -12.0f;
        sourcePosition[0] += displacement * offsetDirection[0];
        sourcePosition[1] += displacement * offsetDirection[1];
        sourcePosition[2] += displacement * offsetDirection[2];
    }

    R_TransformHomogenousModelToClip(sourcePosition, backEnd.orientation.modelMatrix, backEnd.viewParms.projectionMatrix, eye, clip);

    if (flare == NULL && source->screenRadius == 0) {
        for (int32_t component = 0; component < 3; ++component) {
            if (clip[component] >= clip[3] || clip[component] <= -clip[3]) {
                return;
            }
        }
    }

    R_TransformClipToWindow(clip, &backEnd.viewParms, normalized, window);

    if (flare == NULL) {
        /* 0x004eef4b..0x004eef62 stores the integer conversion to radius but
         * uses the retained x87 value for the first horizontal clip test. */
        const long double radiusRaw = (long double)source->screenRadius;
        const float radius = (float)radiusRaw;

        if ((long double)window[0] + radiusRaw < 0.0L || window[0] - radius >= (float)backEnd.viewParms.viewportWidth ||
            window[1] + radius < 0.0f || window[1] - radius >= (float)backEnd.viewParms.viewportHeight) {
            return;
        }

        flare = rendererFreeFlares;
        if (flare == NULL)
            return;

        rendererFreeFlares = flare->next;
        flare->next = rendererActiveFlares;
        rendererActiveFlares = flare;
        flare->frameSceneNum = backEnd.viewParms.frameSceneNum;
        flare->portalView = backEnd.viewParms.isPortal;
        flare->addedFrame = -1;
        flare->id = source->id;
        flare->drawIntensity = 0.0f;
        flare->fadeTime = backEnd.refdef.time - 10;
    }

    flare->shader = source->shader;
    flare->active = source->active;
    flare->fadeInMsec = source->fadeInMsec;
    flare->fadeOutMsec = source->fadeOutMsec;
    flare->addedFrame = backEnd.viewParms.frameCount;
    flare->color[0] = source->color[0];
    flare->color[1] = source->color[1];
    flare->color[2] = source->color[2];
    /* 0x004ef035..0x004ef040 copies the fourth source-color lane into
     * flare->size, then the following source-size store overwrites it. */
    flare->size = source->color[3];
    flare->size = source->size;
    flare->screenRadius = source->screenRadius;

    if (direction != NULL) {
        vec3_t viewDirection = {backEnd.viewParms.orientation.origin[0] - source->origin[0],
                                backEnd.viewParms.orientation.origin[1] - source->origin[1],
                                backEnd.viewParms.orientation.origin[2] - source->origin[2]};
        VectorNormalizeFast(viewDirection);

        const float facing = viewDirection[0] * direction[0] + viewDirection[1] * direction[1] + viewDirection[2] * direction[2];
        flare->color[0] *= facing;
        flare->color[1] *= facing;
        flare->color[2] *= facing;
    }

    flare->windowX = coduo_fp_to_i32_extended((long double)window[0]) + backEnd.viewParms.viewportX;
    flare->windowY = coduo_fp_to_i32_extended((long double)window[1]) + backEnd.viewParms.viewportY;
    if (source->depthOffset != 0.0f)
        flare->eyeZ = (clip[2] / clip[3] + 1.0f) * 0.5f;
    else
        flare->eyeZ = 1.0f;
}

/* Source: CoDUOMP.exe 0x004ef110..0x004ef22f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ef110_004ef230.mcode.
 * Name: exact same-module Mac symbol RB_AddDlightFlares. World dynamic lights
 * use persistent ids beginning at 1024 and the built-in flare shader. Their
 * homogeneous position W becomes the source depth offset. */
void RB_AddDlightFlares(void)
{
    enum {
        R_DLIGHT_FLARE_FIRST_ID = 1024
    };

    if (r_flares->integer < 2)
        return;

    renderer_flare_source_t source = {.id = R_DLIGHT_FLARE_FIRST_ID,
                                      .shader = tr.flareShader,
                                      .color = {0.0f, 0.0f, 0.0f, 1.0f},
                                      .size = r_flareSize->value,
                                      .screenRadius = 0,
                                      .fadeInMsec = FastRound(r_flareFadeIn->value * 1000.0f),
                                      .fadeOutMsec = FastRound(r_flareFadeOut->value * 1000.0f),
                                      .active = qtrue};

    for (int32_t lightIndex = 0; lightIndex < backEnd.refdef.num_dlights; ++lightIndex, ++source.id) {
        const renderer_light_t *light = &backEnd.refdef.dlights[lightIndex];

        source.origin[0] = light->position[0];
        source.origin[1] = light->position[1];
        source.origin[2] = light->position[2];
        source.depthOffset = light->position[3];
        source.color[0] = light->color[0];
        source.color[1] = light->color[1];
        source.color[2] = light->color[2];
        RB_AddFlare(&source, NULL);
    }
}

/* Source: CoDUOMP.exe 0x004ef230..0x004ef36c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ef230_004ef36d.mcode.
 * Name: exact same-module Mac symbol RB_AddCoronaFlares. Corona mode is
 * enabled by r_flares values 1 and 3; the stored selector bit chooses the
 * built-in spotlight material for that source. */
void RB_AddCoronaFlares(void)
{
    enum {
        R_FLARES_CORONAS_ONLY = 1,
        R_FLARES_DLIGHTS_AND_CORONAS = 3
    };

    if ((r_flares->integer != R_FLARES_CORONAS_ONLY && r_flares->integer != R_FLARES_DLIGHTS_AND_CORONAS) || tr.world == NULL) {
        return;
    }

    renderer_flare_source_t source = {.depthOffset = 1.0f,
                                      .color = {0.0f, 0.0f, 0.0f, 1.0f},
                                      .screenRadius = 0,
                                      .fadeInMsec = FastRound(r_flareFadeIn->value * 1000.0f),
                                      .fadeOutMsec = FastRound(r_flareFadeOut->value * 1000.0f),
                                      .active = qtrue};

    for (int32_t coronaIndex = 0; coronaIndex < backEnd.refdef.coronaCount; ++coronaIndex) {
        const renderer_corona_t *corona = &backEnd.refdef.coronas[coronaIndex];

        source.id = corona->id;
        source.shader = (corona->flags & R_CORONA_FLAG_SPOT_LIGHT_SHADER) != 0 ? tr.spotLightShader : tr.flareShader;
        source.origin[0] = corona->origin[0];
        source.origin[1] = corona->origin[1];
        source.origin[2] = corona->origin[2];
        source.color[0] = corona->color[0];
        source.color[1] = corona->color[1];
        source.color[2] = corona->color[2];
        source.size = r_flareSize->value * corona->scale;
        RB_AddFlare(&source, NULL);
    }
}


/* Source: CoDUOMP.exe 0x004ef430..0x004ef655.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ef430_004ef656.mcode.
 * Name: exact same-module Mac symbol RB_TestFlare. The 65-pixel cap bounds
 * the fixed depth-sample square. Pixels are visible when the flare depth is
 * no farther than the stored depth; the denominator remains the unclipped
 * square area so off-screen samples contribute zero visibility. */
static void RB_TestFlare(renderer_flare_t *flare)
{
    enum {
        R_FLARE_DEPTH_SAMPLE_LIMIT = 65
    };

    float depthSamples[R_FLARE_DEPTH_SAMPLE_LIMIT * R_FLARE_DEPTH_SAMPLE_LIMIT];
    float targetVisibility = (flare->active & 1) != 0 ? 1.0f : 0.0f;

    ++backEnd.pc.flareTestCount;

    if (flare->windowX + flare->screenRadius < 0 || flare->windowX - flare->screenRadius >= backEnd.viewParms.viewportWidth ||
        flare->windowY + flare->screenRadius < 0 || flare->windowY - flare->screenRadius >= backEnd.viewParms.viewportHeight) {
        targetVisibility = (flare->active & 1) != 0 ? 1.0f : 0.0f;
    } else {
        int32_t sampleSide = flare->screenRadius * 2 + 1;
        int32_t visiblePixelCount = 0;

        if (sampleSide > R_FLARE_DEPTH_SAMPLE_LIMIT)
            sampleSide = R_FLARE_DEPTH_SAMPLE_LIMIT;
        else if (sampleSide < 1)
            sampleSide = 1;

        const int32_t sampleArea = sampleSide * sampleSide;
        const int32_t sampleHalfSide = sampleSide / 2;
        int32_t sampleX = flare->windowX - sampleHalfSide;
        int32_t sampleY = flare->windowY - sampleHalfSide;
        int32_t sampleWidth = sampleSide;
        int32_t sampleHeight = sampleSide;

        if (sampleX < 0) {
            sampleWidth += sampleX;
            sampleX = 0;
        } else if (sampleWidth > backEnd.viewParms.viewportWidth - sampleX) {
            sampleWidth = backEnd.viewParms.viewportWidth - sampleX;
        }

        if (sampleY < 0) {
            sampleHeight += sampleY;
            sampleY = 0;
        } else if (sampleHeight > backEnd.viewParms.viewportHeight - sampleY) {
            sampleHeight = backEnd.viewParms.viewportHeight - sampleY;
        }

        glState.finishCalled = qfalse;
        if (sampleWidth > 0 && sampleHeight > 0) {
            const int32_t readPixelCount = sampleWidth * sampleHeight;

            qglReadPixels(sampleX, sampleY, sampleWidth, sampleHeight, GL_DEPTH_COMPONENT, GL_FLOAT, depthSamples);
            for (int32_t sampleIndex = 0; sampleIndex < readPixelCount; ++sampleIndex) {
                if (flare->eyeZ <= depthSamples[sampleIndex])
                    ++visiblePixelCount;
            }
        }

        targetVisibility = (float)visiblePixelCount / (float)sampleArea;
    }

    if (flare->id == -1)
        rendererSunFlareVisibility = targetVisibility;

    flare->drawIntensity = R_UpdateOverTime(flare->drawIntensity, targetVisibility, flare->fadeInMsec, flare->fadeOutMsec,
                                            backEnd.refdef.time - flare->fadeTime);
    flare->fadeTime = backEnd.refdef.time;
}

/* Source: CoDUOMP.exe 0x004ef660..0x004ef90e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ef660_004ef913.mcode.
 * Name: exact same-module Mac symbol RB_RenderFlare. The flare is emitted as
 * one independently flushed, screen-space tessellation quad at r_znear. */
static void RB_RenderFlare(renderer_flare_t *flare)
{
    /* Exact 0x005b9cf4 float; semantically 1.0f / 640.0f. */
    const float viewportScale = 0.0015625000232830644f;
    const float halfSize = (float)((long double)flare->size * (long double)viewportScale * (long double)backEnd.viewParms.viewportWidth);
    const long double redRaw = (long double)tr.identityLight * (long double)flare->color[0];
    const float green = (float)((long double)tr.identityLight * (long double)flare->color[1]);
    const float blue = (float)((long double)tr.identityLight * (long double)flare->color[2]);
    uint8_t colorBytes[4];
    uint32_t packedColor;

    ++backEnd.pc.flareRenderCount;

    colorBytes[0] = coduo_fp_to_u8_extended(redRaw * 255.0L);
    colorBytes[1] = coduo_fp_to_u8_extended((long double)green * 255.0L);
    colorBytes[2] = coduo_fp_to_u8_extended((long double)blue * 255.0L);
    colorBytes[3] = coduo_fp_to_u8_extended((long double)flare->drawIntensity * 255.0L);
    memcpy(&packedColor, colorBytes, sizeof(packedColor));

    backEnd.currentEntity = &tr.worldEntity;
    RB_SelectStorage(tr.defaultStorageMode);
    RB_BeginSurface(flare->shader, 3);

    const int32_t firstVertex = tess.vertexCount;
    const int32_t firstIndex = tess.indexCount;
    const float left = (float)flare->windowX - halfSize;
    const float right = (float)flare->windowX + halfSize;
    const float bottom = (float)flare->windowY - halfSize;
    const float top = (float)flare->windowY + halfSize;
    const float depth = r_znear->value;
    const vec2_t texCoords[4] = {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}};
    const vec2_t positions[4] = {{left, bottom}, {left, top}, {right, top}, {right, bottom}};

    for (int32_t vertexOffset = 0; vertexOffset < 4; ++vertexOffset) {
        const int32_t vertexIndex = firstVertex + vertexOffset;
        float *position = &tess.xyz[vertexIndex * tess.vertexComponentCount];

        position[0] = positions[vertexOffset][0];
        position[1] = positions[vertexOffset][1];
        position[2] = depth;
        tess.texCoords[0][vertexIndex][0] = texCoords[vertexOffset][0];
        tess.texCoords[0][vertexIndex][1] = texCoords[vertexOffset][1];
        tess.vertexColors[vertexIndex] = packedColor;
    }

    tess.indexes[firstIndex + 0] = (uint16_t)(firstVertex + 0);
    tess.indexes[firstIndex + 1] = (uint16_t)(firstVertex + 1);
    tess.indexes[firstIndex + 2] = (uint16_t)(firstVertex + 2);
    tess.indexes[firstIndex + 3] = (uint16_t)(firstVertex + 0);
    tess.indexes[firstIndex + 4] = (uint16_t)(firstVertex + 2);
    tess.indexes[firstIndex + 5] = (uint16_t)(firstVertex + 3);
    tess.vertexCount += 4;
    tess.indexCount += 6;

    RB_EndSurface();
}

/* Source: CoDUOMP.exe 0x004ef920..0x004efcb8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ef920_004efcb9.mcode.
 * Name: exact same-module Mac symbol RB_RenderFlares. This owns stale-flare
 * reclamation, visibility tests, sun blindness/glare, the full-screen overlay,
 * and the final per-view flare draw pass. */
void RB_RenderFlares(void)
{
    renderer_flare_t **link;
    qboolean renderAnyFlares = qfalse;
    float blindFraction;
    float glareFraction;

    if (r_flares->integer == 0)
        return;

    RB_AddDlightFlares();
    RB_AddCoronaFlares();
    rendererSunFlareVisibility = 0.0f;

    link = &rendererActiveFlares;
    while (*link != NULL) {
        renderer_flare_t *flare = *link;

        if (flare->addedFrame < backEnd.viewParms.frameCount) {
            if (flare->drawIntensity <= 0.0f) {
                *link = flare->next;
                flare->next = rendererFreeFlares;
                rendererFreeFlares = flare;
                continue;
            }
            flare->active = qfalse;
            flare->eyeZ = 2.0f;
        }

        if (flare->frameSceneNum == backEnd.viewParms.frameSceneNum && flare->portalView == backEnd.viewParms.isPortal) {
            RB_TestFlare(flare);
            if (flare->drawIntensity == 0.0f) {
                *link = flare->next;
                flare->next = rendererFreeFlares;
                rendererFreeFlares = flare;
                continue;
            }
            renderAnyFlares = qtrue;
        }

        link = &flare->next;
    }

    RB_CalcSunBlind(rendererSunFlareVisibility, &blindFraction, &glareFraction);
    if (renderAnyFlares == qfalse && blindFraction <= 0.0f && glareFraction <= 0.0f) {
        return;
    }

    if (backEnd.viewParms.isPortal != qfalse)
        qglDisable(GL_CLIP_PLANE0);

    qglPushMatrix();
    qglMatrixMode(GL_PROJECTION);
    qglPushMatrix();
    qglLoadIdentity();
    qglOrtho((double)backEnd.viewParms.viewportX, (double)(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth),
             (double)backEnd.viewParms.viewportY, (double)(backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight), -99999.0,
             99999.0);

    if (blindFraction != 0.0f || glareFraction != 0.0f) {
        RB_BeginImmediateMode();
        GL_Bind(tr.whiteImage);
        GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
        RB_glColor4f(glareFraction, glareFraction, glareFraction, blindFraction);
        RB_glBegin(GL_QUADS);
        RB_glVertex3f((float)backEnd.viewParms.viewportX, (float)backEnd.viewParms.viewportY, 0.0f);
        RB_glVertex3f((float)backEnd.viewParms.viewportX, (float)(backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight), 0.0f);
        RB_glVertex3f((float)(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth),
                      (float)(backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight), 0.0f);
        RB_glVertex3f((float)(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth), (float)backEnd.viewParms.viewportY, 0.0f);
        RB_glEnd();
        RB_EndImmediateMode();
    }

    if (renderAnyFlares != qfalse) {
        for (renderer_flare_t *flare = rendererActiveFlares; flare != NULL; flare = flare->next) {
            if (flare->frameSceneNum == backEnd.viewParms.frameSceneNum && flare->portalView == backEnd.viewParms.isPortal &&
                flare->drawIntensity != 0.0f) {
                RB_RenderFlare(flare);
            }
        }
    }

    qglPopMatrix();
    qglMatrixMode(GL_MODELVIEW);
    qglPopMatrix();
}
