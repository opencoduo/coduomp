#include "backend.h"

#include "output_gamma_compat.h"

#include "gl_api.h"
#include "gl_debug.h"
#include "gl_state.h"
#include "renderer_cvars.h"
#include "wgl_debug.h"
#include "../platform/dynamic_library_boundary.h"
#include "../platform/sdl_platform.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) && !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif

enum {
    CODUOMP_GL_ALL_ATTRIB_BITS = 0x000fffff,
    CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT = 256,
    CODUOMP_OUTPUT_GAMMA_COMPONENT_COUNT = 4
};

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: the complete final-presentation
 * compatibility path lives in this translation unit so the recovered WGL
 * source retains only lifecycle hooks. */
static uint32_t coduompOutputPresentationTexture;
static int32_t coduompOutputPresentationTextureWidth;
static int32_t coduompOutputPresentationTextureHeight;
static uint32_t coduompOutputGammaLutTexture;
static uint32_t coduompOutputGammaProgram;
static qboolean coduompOutputGammaSoftwareAvailable;
static qboolean coduompOutputGammaSoftwareActive;
static qboolean coduompOutputGammaLutIdentity = qtrue;

/* NOT_FROM_ORIGINAL_SOURCE: resolve only the ARB program entry points used by
 * the output-LUT pass. Keeping this loader beside its sole consumer avoids
 * publishing compatibility policy from the recovered QGL loader. */
static qboolean coduomp_output_gamma_load_program_functions_compat(void)
{
#define CODUOMP_LOAD_ARB_PROGRAM_ENTRY(type_, name_) \
    rendererGl##name_##Driver = (type_)qwglGetProcAddress("gl" #name_); \
    qgl##name_ = rendererGl##name_##Driver

    if (qwglGetProcAddress == NULL)
        return qfalse;

    CODUOMP_LOAD_ARB_PROGRAM_ENTRY(renderer_gl_bind_program_arb_func_t, BindProgramARB);
    CODUOMP_LOAD_ARB_PROGRAM_ENTRY(renderer_gl_delete_programs_arb_func_t, DeleteProgramsARB);
    CODUOMP_LOAD_ARB_PROGRAM_ENTRY(renderer_gl_gen_programs_arb_func_t, GenProgramsARB);
    CODUOMP_LOAD_ARB_PROGRAM_ENTRY(renderer_gl_program_string_arb_func_t, ProgramStringARB);

#undef CODUOMP_LOAD_ARB_PROGRAM_ENTRY

    return qglBindProgramARB != NULL && qglDeleteProgramsARB != NULL && qglGenProgramsARB != NULL && qglProgramStringARB != NULL ? qtrue
                                                                                                                                 : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: compile one dependent-lookup program that maps
 * each final 8-bit framebuffer channel through the exact renderer-generated
 * 256-entry table. A 256x1 nearest-filtered texture preserves the recovered
 * table's rounding, quantization, overbright saturation, and live r_gamma. */
void coduomp_output_gamma_initialize_compat(void)
{
    static const char programSource[] = "!!ARBfp1.0\n"
                                        "TEMP color, mapped;\n"
                                        "TEX color, fragment.texcoord[0], texture[0], 2D;\n"
                                        "TEX mapped.r, color.rrrr, texture[1], 2D;\n"
                                        "TEX mapped.g, color.gggg, texture[1], 2D;\n"
                                        "TEX mapped.b, color.bbbb, texture[1], 2D;\n"
                                        "MOV mapped.a, color.a;\n"
                                        "MOV result.color, mapped;\n"
                                        "END\n";

    coduompOutputGammaSoftwareAvailable = qfalse;
    coduompOutputGammaSoftwareActive = qfalse;
    coduompOutputGammaLutIdentity = qtrue;

    if (coduompOutputGammaProgram != 0) {
        coduompOutputGammaSoftwareAvailable = qtrue;
        return;
    }
    if (qglActiveTextureARB == NULL || glConfig.maxActiveTextures < 2 ||
        GLW_HasExtension(glConfig.extensionsString, "GL_ARB_fragment_program") == qfalse ||
        coduomp_output_gamma_load_program_functions_compat() == qfalse) {
        ri.Printf(R_PRINT_WARNING, "WARNING: exact OpenGL output gamma LUT is unavailable.\n");
        return;
    }

    qglGenProgramsARB(1, &coduompOutputGammaProgram);
    qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, coduompOutputGammaProgram);
    qglProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (int32_t)strlen(programSource), programSource);
    qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);

    int32_t errorPosition;
    qglGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPosition);
    if (errorPosition >= 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: OpenGL output gamma LUT program failed at char "
                  "%i: %s\n",
                  errorPosition, qglGetString(GL_PROGRAM_ERROR_STRING_ARB));
        qglDeleteProgramsARB(1, &coduompOutputGammaProgram);
        coduompOutputGammaProgram = 0;
        return;
    }

    coduompOutputGammaSoftwareAvailable = qtrue;
    ri.Printf(R_PRINT_ALL, "Exact OpenGL output gamma LUT is available.\n");
}

/* NOT_FROM_ORIGINAL_SOURCE: activate window-scoped output correction after a
 * native ramp was disabled or rejected. The reason is supplied by the native
 * boundary so logs distinguish explicit policy from an SDL/driver failure. */
qboolean coduomp_output_gamma_activate_software_compat(const char *reason)
{
    if (coduompOutputGammaSoftwareAvailable == qfalse) {
        ri.Printf(R_PRINT_WARNING, "WARNING: %s; no exact output gamma fallback is available.\n", reason);
        return qfalse;
    }

    if (coduompOutputGammaSoftwareActive == qfalse) {
        coduompOutputGammaSoftwareActive = qtrue;
        ri.Printf(R_PRINT_ALL, "%s; using the exact OpenGL output gamma LUT.\n", reason);
    }
    return qtrue;
}

qboolean coduomp_output_gamma_software_active_compat(void)
{
    return coduompOutputGammaSoftwareActive;
}

/* NOT_FROM_ORIGINAL_SOURCE: upload the recovered renderer's actual final RGB
 * lookup table. The alpha component remains identity because display gamma
 * never changed framebuffer alpha. */
void coduomp_output_gamma_set_lut_compat(const uint8_t red[CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT],
                                         const uint8_t green[CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT],
                                         const uint8_t blue[CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT])
{
    uint8_t lut[CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT][CODUOMP_OUTPUT_GAMMA_COMPONENT_COUNT];
    const int32_t previousTextureUnit = glState.currenttmu;

    if (coduompOutputGammaSoftwareActive == qfalse)
        return;

    coduompOutputGammaLutIdentity = qtrue;
    for (int32_t entry = 0; entry < CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT; ++entry) {
        lut[entry][0] = red[entry];
        lut[entry][1] = green[entry];
        lut[entry][2] = blue[entry];
        lut[entry][3] = 255;
        if (red[entry] != entry || green[entry] != entry || blue[entry] != entry) {
            coduompOutputGammaLutIdentity = qfalse;
        }
    }

    qglPushAttrib(CODUOMP_GL_ALL_ATTRIB_BITS);
    qglActiveTextureARB(GL_TEXTURE0_ARB + 1);
    if (coduompOutputGammaLutTexture == 0)
        qglGenTextures(1, &coduompOutputGammaLutTexture);
    qglBindTexture(GL_TEXTURE_2D, coduompOutputGammaLutTexture);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, lut);
    qglPopAttrib();

    if (previousTextureUnit >= 0 && previousTextureUnit < glConfig.maxActiveTextures) {
        qglActiveTextureARB(GL_TEXTURE0_ARB + (uint32_t)previousTextureUnit);
    }
}

#if defined(__linux__) && !defined(_WIN32)

typedef struct coduomp_xrandr_gamma_api_s {
    void *xrandrLibrary;
    void *x11Library;
    XRRScreenResources *(*getScreenResourcesCurrent)(Display *, Window);
    void (*freeScreenResources)(XRRScreenResources *);
    XRRCrtcInfo *(*getCrtcInfo)(Display *, XRRScreenResources *, RRCrtc);
    void (*freeCrtcInfo)(XRRCrtcInfo *);
    int (*getCrtcGammaSize)(Display *, RRCrtc);
    XRRCrtcGamma *(*getCrtcGamma)(Display *, RRCrtc);
    XRRCrtcGamma *(*allocGamma)(int);
    void (*setCrtcGamma)(Display *, RRCrtc, XRRCrtcGamma *);
    void (*freeGamma)(XRRCrtcGamma *);
    int (*flush)(Display *);
} coduomp_xrandr_gamma_api_t;

static coduomp_xrandr_gamma_api_t coduompXRandrGammaApi;
static Display *coduompXRandrDisplay;
static RRCrtc coduompXRandrCrtc;
static int32_t coduompXRandrGammaSize;
static XRRCrtcGamma *coduompXRandrOriginalGamma;
static qboolean coduompXRandrGammaActive;
static qboolean coduompXRandrGammaApplied;

static void coduomp_output_gamma_unload_xrandr_compat(void)
{
    if (coduompXRandrOriginalGamma != NULL && coduompXRandrGammaApi.freeGamma != NULL) {
        coduompXRandrGammaApi.freeGamma(coduompXRandrOriginalGamma);
    }
    coduompXRandrOriginalGamma = NULL;
    coduompXRandrDisplay = NULL;
    coduompXRandrCrtc = 0;
    coduompXRandrGammaSize = 0;
    coduompXRandrGammaActive = qfalse;
    coduompXRandrGammaApplied = qfalse;

    if (coduompXRandrGammaApi.x11Library != NULL)
        coduomp_library_close(coduompXRandrGammaApi.x11Library);
    if (coduompXRandrGammaApi.xrandrLibrary != NULL)
        coduomp_library_close(coduompXRandrGammaApi.xrandrLibrary);
    memset(&coduompXRandrGammaApi, 0, sizeof(coduompXRandrGammaApi));
}

static qboolean coduomp_output_gamma_load_xrandr_compat(void)
{
#define CODUOMP_LOAD_XRANDR_SYMBOL(field_, name_) \
    coduomp_library_symbol(coduompXRandrGammaApi.xrandrLibrary, name_, &coduompXRandrGammaApi.field_, sizeof(coduompXRandrGammaApi.field_))

    if (coduompXRandrGammaApi.xrandrLibrary != NULL)
        return qtrue;

    coduompXRandrGammaApi.xrandrLibrary = coduomp_library_open("libXrandr.so.2");
    coduompXRandrGammaApi.x11Library = coduomp_library_open("libX11.so.6");
    if (coduompXRandrGammaApi.xrandrLibrary == NULL || coduompXRandrGammaApi.x11Library == NULL) {
        coduomp_output_gamma_unload_xrandr_compat();
        return qfalse;
    }

    CODUOMP_LOAD_XRANDR_SYMBOL(getScreenResourcesCurrent, "XRRGetScreenResourcesCurrent");
    CODUOMP_LOAD_XRANDR_SYMBOL(freeScreenResources, "XRRFreeScreenResources");
    CODUOMP_LOAD_XRANDR_SYMBOL(getCrtcInfo, "XRRGetCrtcInfo");
    CODUOMP_LOAD_XRANDR_SYMBOL(freeCrtcInfo, "XRRFreeCrtcInfo");
    CODUOMP_LOAD_XRANDR_SYMBOL(getCrtcGammaSize, "XRRGetCrtcGammaSize");
    CODUOMP_LOAD_XRANDR_SYMBOL(getCrtcGamma, "XRRGetCrtcGamma");
    CODUOMP_LOAD_XRANDR_SYMBOL(allocGamma, "XRRAllocGamma");
    CODUOMP_LOAD_XRANDR_SYMBOL(setCrtcGamma, "XRRSetCrtcGamma");
    CODUOMP_LOAD_XRANDR_SYMBOL(freeGamma, "XRRFreeGamma");
    coduomp_library_symbol(coduompXRandrGammaApi.x11Library, "XFlush", &coduompXRandrGammaApi.flush, sizeof(coduompXRandrGammaApi.flush));

#undef CODUOMP_LOAD_XRANDR_SYMBOL

    if (coduompXRandrGammaApi.getScreenResourcesCurrent == NULL || coduompXRandrGammaApi.freeScreenResources == NULL ||
        coduompXRandrGammaApi.getCrtcInfo == NULL || coduompXRandrGammaApi.freeCrtcInfo == NULL ||
        coduompXRandrGammaApi.getCrtcGammaSize == NULL || coduompXRandrGammaApi.getCrtcGamma == NULL ||
        coduompXRandrGammaApi.allocGamma == NULL || coduompXRandrGammaApi.setCrtcGamma == NULL || coduompXRandrGammaApi.freeGamma == NULL ||
        coduompXRandrGammaApi.flush == NULL) {
        coduomp_output_gamma_unload_xrandr_compat();
        return qfalse;
    }
    return qtrue;
}

static uint16_t coduomp_output_gamma_resample_entry_compat(const uint16_t *source, int32_t sourceCount, int32_t destinationIndex,
                                                           int32_t destinationCount)
{
    if (sourceCount <= 1 || destinationCount <= 1)
        return source[0];

    const uint64_t numerator = (uint64_t)(uint32_t)destinationIndex * (uint64_t)(uint32_t)(sourceCount - 1);
    const uint32_t denominator = (uint32_t)(destinationCount - 1);
    const uint32_t lower = (uint32_t)(numerator / denominator);
    const uint32_t remainder = (uint32_t)(numerator % denominator);
    const uint32_t upper = lower + 1u < (uint32_t)sourceCount ? lower + 1u : lower;
    const uint64_t value = (uint64_t)source[lower] * (denominator - remainder) + (uint64_t)source[upper] * remainder + denominator / 2u;
    return (uint16_t)(value / denominator);
}

static int64_t coduomp_output_gamma_intersection_area_compat(int32_t firstX, int32_t firstY, int32_t firstWidth, int32_t firstHeight,
                                                             int32_t secondX, int32_t secondY, int32_t secondWidth, int32_t secondHeight)
{
    const int64_t left = firstX > secondX ? firstX : secondX;
    const int64_t top = firstY > secondY ? firstY : secondY;
    const int64_t firstRight = (int64_t)firstX + firstWidth;
    const int64_t secondRight = (int64_t)secondX + secondWidth;
    const int64_t firstBottom = (int64_t)firstY + firstHeight;
    const int64_t secondBottom = (int64_t)secondY + secondHeight;
    const int64_t right = firstRight < secondRight ? firstRight : secondRight;
    const int64_t bottom = firstBottom < secondBottom ? firstBottom : secondBottom;

    return right > left && bottom > top ? (right - left) * (bottom - top) : 0;
}

qboolean coduomp_output_gamma_try_xrandr_compat(uint16_t originalRamp[3][CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT])
{
    void *nativeDisplay;
    unsigned long nativeWindow;
    int32_t windowX;
    int32_t windowY;
    int32_t windowWidth;
    int32_t windowHeight;
    XRRScreenResources *resources;
    RRCrtc selectedCrtc = 0;
    int64_t selectedArea = 0;
    const char *sessionType;

    if (coduompXRandrGammaActive != qfalse)
        return qtrue;
    if (r_fullscreen == NULL || r_fullscreen->integer != 1)
        return qfalse;

    sessionType = getenv("XDG_SESSION_TYPE");
    if ((sessionType != NULL && strcmp(sessionType, "wayland") == 0) || (sessionType == NULL && getenv("WAYLAND_DISPLAY") != NULL)) {
        return qfalse;
    }
    if (coduomp_sdl_get_x11_window_compat(&nativeDisplay, &nativeWindow, &windowX, &windowY, &windowWidth, &windowHeight) == qfalse ||
        nativeDisplay == NULL || nativeWindow == 0 || coduomp_output_gamma_load_xrandr_compat() == qfalse) {
        return qfalse;
    }

    resources = coduompXRandrGammaApi.getScreenResourcesCurrent((Display *)nativeDisplay, (Window)nativeWindow);
    if (resources == NULL) {
        coduomp_output_gamma_unload_xrandr_compat();
        return qfalse;
    }

    for (int32_t index = 0; index < resources->ncrtc; ++index) {
        XRRCrtcInfo *const info = coduompXRandrGammaApi.getCrtcInfo((Display *)nativeDisplay, resources, resources->crtcs[index]);
        int64_t area = 0;

        if (info != NULL && info->mode != None && info->width != 0 && info->height != 0) {
            area = coduomp_output_gamma_intersection_area_compat(windowX, windowY, windowWidth, windowHeight, info->x, info->y,
                                                                 (int32_t)info->width, (int32_t)info->height);
        }
        if (area > selectedArea) {
            selectedArea = area;
            selectedCrtc = resources->crtcs[index];
        }
        if (info != NULL)
            coduompXRandrGammaApi.freeCrtcInfo(info);
    }
    coduompXRandrGammaApi.freeScreenResources(resources);

    if (selectedCrtc == 0 || selectedArea == 0) {
        coduomp_output_gamma_unload_xrandr_compat();
        return qfalse;
    }

    const int32_t gammaSize = coduompXRandrGammaApi.getCrtcGammaSize((Display *)nativeDisplay, selectedCrtc);
    XRRCrtcGamma *const original = gammaSize > 0 ? coduompXRandrGammaApi.getCrtcGamma((Display *)nativeDisplay, selectedCrtc) : NULL;
    if (original == NULL || original->size != gammaSize) {
        if (original != NULL)
            coduompXRandrGammaApi.freeGamma(original);
        coduomp_output_gamma_unload_xrandr_compat();
        return qfalse;
    }

    for (int32_t entry = 0; entry < CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT; ++entry) {
        originalRamp[0][entry] =
            coduomp_output_gamma_resample_entry_compat(original->red, gammaSize, entry, CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT);
        originalRamp[1][entry] =
            coduomp_output_gamma_resample_entry_compat(original->green, gammaSize, entry, CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT);
        originalRamp[2][entry] =
            coduomp_output_gamma_resample_entry_compat(original->blue, gammaSize, entry, CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT);
    }

    coduompXRandrDisplay = (Display *)nativeDisplay;
    coduompXRandrCrtc = selectedCrtc;
    coduompXRandrGammaSize = gammaSize;
    coduompXRandrOriginalGamma = original;
    coduompXRandrGammaActive = qtrue;
    coduompXRandrGammaApplied = qfalse;
    ri.Printf(R_PRINT_ALL, "Using XRandR CRTC 0x%lx native gamma (%d entries).\n", (unsigned long)selectedCrtc, gammaSize);
    return qtrue;
}

qboolean coduomp_output_gamma_xrandr_active_compat(void)
{
    return coduompXRandrGammaActive;
}

qboolean coduomp_output_gamma_set_xrandr_compat(const uint16_t ramp[3][CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT])
{
    if (coduompXRandrGammaActive == qfalse || coduompXRandrDisplay == NULL || coduompXRandrGammaSize <= 0)
        return qfalse;

    XRRCrtcGamma *const gamma = coduompXRandrGammaApi.allocGamma(coduompXRandrGammaSize);
    if (gamma == NULL)
        return qfalse;

    for (int32_t entry = 0; entry < coduompXRandrGammaSize; ++entry) {
        gamma->red[entry] =
            coduomp_output_gamma_resample_entry_compat(ramp[0], CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT, entry, coduompXRandrGammaSize);
        gamma->green[entry] =
            coduomp_output_gamma_resample_entry_compat(ramp[1], CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT, entry, coduompXRandrGammaSize);
        gamma->blue[entry] =
            coduomp_output_gamma_resample_entry_compat(ramp[2], CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT, entry, coduompXRandrGammaSize);
    }

    coduompXRandrGammaApi.setCrtcGamma(coduompXRandrDisplay, coduompXRandrCrtc, gamma);
    coduompXRandrGammaApi.flush(coduompXRandrDisplay);
    coduompXRandrGammaApi.freeGamma(gamma);
    coduompXRandrGammaApplied = qtrue;
    return qtrue;
}

void coduomp_output_gamma_restore_xrandr_compat(void)
{
    if (coduompXRandrGammaActive == qfalse || coduompXRandrGammaApplied == qfalse || coduompXRandrDisplay == NULL ||
        coduompXRandrOriginalGamma == NULL) {
        return;
    }

    coduompXRandrGammaApi.setCrtcGamma(coduompXRandrDisplay, coduompXRandrCrtc, coduompXRandrOriginalGamma);
    coduompXRandrGammaApi.flush(coduompXRandrDisplay);
    coduompXRandrGammaApplied = qfalse;
}

#else

qboolean coduomp_output_gamma_try_xrandr_compat(uint16_t originalRamp[3][CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT])
{
    (void)originalRamp;
    return qfalse;
}

qboolean coduomp_output_gamma_xrandr_active_compat(void)
{
    return qfalse;
}

qboolean coduomp_output_gamma_set_xrandr_compat(const uint16_t ramp[3][CODUOMP_OUTPUT_GAMMA_ENTRY_COUNT])
{
    (void)ramp;
    return qfalse;
}

void coduomp_output_gamma_restore_xrandr_compat(void)
{
}

#endif

/* NOT_FROM_ORIGINAL_SOURCE: copy the selected render-size region from the
 * backbuffer, optionally map it through the exact final RGB LUT, and scale it
 * into the hardware drawable. */
void coduomp_output_gamma_present_compat(void)
{
    int32_t outputWidth;
    int32_t outputHeight;
    int32_t viewportX;
    int32_t viewportY;
    int32_t viewportWidth;
    int32_t viewportHeight;

    const qboolean presentationActive =
        coduomp_get_output_presentation_compat(&outputWidth, &outputHeight, &viewportX, &viewportY, &viewportWidth, &viewportHeight);
    const qboolean applySoftwareGamma =
        coduompOutputGammaSoftwareActive != qfalse && coduompOutputGammaLutIdentity == qfalse && coduompOutputGammaLutTexture != 0 ? qtrue
                                                                                                                                   : qfalse;

    if (presentationActive == qfalse && applySoftwareGamma == qfalse)
        return;
    if (presentationActive == qfalse) {
        outputWidth = glConfig.vidWidth;
        outputHeight = glConfig.vidHeight;
        viewportX = 0;
        viewportY = 0;
        viewportWidth = outputWidth;
        viewportHeight = outputHeight;
    }

    const int32_t textureWidth = SmallestTextureSizeFitting(glConfig.vidWidth);
    const int32_t textureHeight = SmallestTextureSizeFitting(glConfig.vidHeight);
    const float textureS = (float)glConfig.vidWidth / (float)textureWidth;
    const float textureT = (float)glConfig.vidHeight / (float)textureHeight;

    RB_EndMultitexture();
    qglPushAttrib(CODUOMP_GL_ALL_ATTRIB_BITS);
    qglReadBuffer(GL_BACK);
    qglBindTexture(GL_TEXTURE_2D, coduompOutputPresentationTexture);
    if (coduompOutputPresentationTexture == 0) {
        qglGenTextures(1, &coduompOutputPresentationTexture);
        qglBindTexture(GL_TEXTURE_2D, coduompOutputPresentationTexture);
    }
    if (textureWidth != coduompOutputPresentationTextureWidth || textureHeight != coduompOutputPresentationTextureHeight) {
        qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        coduompOutputPresentationTextureWidth = textureWidth;
        coduompOutputPresentationTextureHeight = textureHeight;
    }
    qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight);

    if (applySoftwareGamma != qfalse) {
        qglActiveTextureARB(GL_TEXTURE0_ARB + 1);
        qglBindTexture(GL_TEXTURE_2D, coduompOutputGammaLutTexture);
        qglActiveTextureARB(GL_TEXTURE0_ARB);
    }

    qglDisable(GL_ALPHA_TEST);
    qglDisable(GL_BLEND);
    qglDisable(GL_CULL_FACE);
    qglDisable(GL_DEPTH_TEST);
    qglDisable(GL_FOG);
    qglDisable(GL_LIGHTING);
    qglDisable(GL_SCISSOR_TEST);
    qglDisable(GL_STENCIL_TEST);
    qglDisable(GL_TEXTURE_GEN_S);
    qglDisable(GL_TEXTURE_GEN_T);
    qglDisable(GL_TEXTURE_GEN_R);
    qglDisable(GL_TEXTURE_GEN_Q);
    qglEnable(GL_TEXTURE_2D);
    qglColorMask(qtrue, qtrue, qtrue, qtrue);
    qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    if (applySoftwareGamma != qfalse) {
        qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, coduompOutputGammaProgram);
        qglEnable(GL_FRAGMENT_PROGRAM_ARB);
    }
    qglViewport(0, 0, outputWidth, outputHeight);
    qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    qglClear(GL_COLOR_BUFFER_BIT);
    qglViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    qglMatrixMode(GL_PROJECTION);
    qglPushMatrix();
    qglLoadIdentity();
    qglMatrixMode(GL_MODELVIEW);
    qglPushMatrix();
    qglLoadIdentity();
    qglMatrixMode(GL_TEXTURE);
    qglPushMatrix();
    qglLoadIdentity();
    qglMatrixMode(GL_MODELVIEW);
    qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    qglBegin(GL_QUADS);
    qglTexCoord2f(0.0f, 0.0f);
    qglVertex2f(-1.0f, -1.0f);
    qglTexCoord2f(textureS, 0.0f);
    qglVertex2f(1.0f, -1.0f);
    qglTexCoord2f(textureS, textureT);
    qglVertex2f(1.0f, 1.0f);
    qglTexCoord2f(0.0f, textureT);
    qglVertex2f(-1.0f, 1.0f);
    qglEnd();
    if (applySoftwareGamma != qfalse) {
        qglDisable(GL_FRAGMENT_PROGRAM_ARB);
        qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
    }
    qglMatrixMode(GL_TEXTURE);
    qglPopMatrix();
    qglMatrixMode(GL_MODELVIEW);
    qglPopMatrix();
    qglMatrixMode(GL_PROJECTION);
    qglPopMatrix();
    qglMatrixMode(GL_MODELVIEW);
    qglPopAttrib();
}

/* NOT_FROM_ORIGINAL_SOURCE: serves front-buffer screenshot captures while the
 * composited output presentation owns the presented image. The recovered
 * capture helpers read a render-sized rectangle from GL_FRONT, but the
 * composited front buffer holds the scaled hardware drawable, so that read
 * returns the drawable's lower-left corner (observed as a magnified
 * bottom-left crop on high-DPI fullscreen). The presentation texture holds
 * exactly the render-sized frame currently on screen, before the output LUT
 * is applied, which matches what the recovered front-buffer read observed
 * under retail's scanout gamma ramp; the callers' existing overbright
 * correction therefore still applies once. Returns qfalse when the
 * presentation path does not own the presented image, in which case the
 * direct front-buffer read remains correct. */
qboolean coduomp_capture_presented_frame_compat(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t pixelFormat, uint8_t *pixels)
{
    int32_t outputWidth;
    int32_t outputHeight;
    int32_t viewportX;
    int32_t viewportY;
    int32_t viewportWidth;
    int32_t viewportHeight;

    const qboolean presentationActive =
        coduomp_get_output_presentation_compat(&outputWidth, &outputHeight, &viewportX, &viewportY, &viewportWidth, &viewportHeight);
    const qboolean applySoftwareGamma =
        coduompOutputGammaSoftwareActive != qfalse && coduompOutputGammaLutIdentity == qfalse && coduompOutputGammaLutTexture != 0 ? qtrue
                                                                                                                                   : qfalse;

    /* Mirror coduomp_output_gamma_present_compat's early return: when the
     * composite pass does not run, the front buffer is the render-sized
     * frame and the texture may be stale or absent. */
    if (presentationActive == qfalse && applySoftwareGamma == qfalse)
        return qfalse;

    if (coduompOutputPresentationTexture == 0 || glConfig.vidWidth > coduompOutputPresentationTextureWidth ||
        glConfig.vidHeight > coduompOutputPresentationTextureHeight || x < 0 || y < 0 || width <= 0 || height <= 0 ||
        width > glConfig.vidWidth - x || height > glConfig.vidHeight - y) {
        return qfalse;
    }

    const int32_t bytesPerPixel = pixelFormat == GL_RGB ? 3 : 4;
    const size_t textureRowBytes = (size_t)coduompOutputPresentationTextureWidth * (size_t)bytesPerPixel;
    const size_t textureByteCount = textureRowBytes * (size_t)coduompOutputPresentationTextureHeight;
    uint8_t *texturePixels;
#if UINTPTR_MAX > UINT32_MAX
    texturePixels = ri.Z_Malloc(textureByteCount);
#else
    texturePixels = ri.Hunk_AllocateTempMemory(textureByteCount);
#endif

    int32_t previousPackAlignment = 4;
    qglGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    qglPushAttrib(CODUOMP_GL_ALL_ATTRIB_BITS);
    if (qglActiveTextureARB != NULL)
        qglActiveTextureARB(GL_TEXTURE0_ARB);
    qglBindTexture(GL_TEXTURE_2D, coduompOutputPresentationTexture);
    qglPixelStorei(GL_PACK_ALIGNMENT, 1);
    qglGetTexImage(GL_TEXTURE_2D, 0, pixelFormat, GL_UNSIGNED_BYTE, texturePixels);
    qglPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    qglPopAttrib();

    for (int32_t row = 0; row < height; ++row) {
        memcpy(pixels + (size_t)row * (size_t)width * (size_t)bytesPerPixel,
               texturePixels + (size_t)(y + row) * textureRowBytes + (size_t)x * (size_t)bytesPerPixel,
               (size_t)width * (size_t)bytesPerPixel);
    }

#if UINTPTR_MAX > UINT32_MAX
    ri.Z_Free(texturePixels);
#else
    ri.Hunk_FreeTempMemory(texturePixels);
#endif
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: release all compatibility-owned GL and XRandR
 * resources before the platform destroys the current window/context. */
void coduomp_output_gamma_shutdown_compat(void)
{
#if defined(__linux__) && !defined(_WIN32)
    coduomp_output_gamma_restore_xrandr_compat();
    coduomp_output_gamma_unload_xrandr_compat();
#endif

    if (coduompOutputPresentationTexture != 0)
        qglDeleteTextures(1, &coduompOutputPresentationTexture);
    if (coduompOutputGammaLutTexture != 0)
        qglDeleteTextures(1, &coduompOutputGammaLutTexture);
    if (coduompOutputGammaProgram != 0)
        qglDeleteProgramsARB(1, &coduompOutputGammaProgram);

    coduompOutputPresentationTexture = 0;
    coduompOutputPresentationTextureWidth = 0;
    coduompOutputPresentationTextureHeight = 0;
    coduompOutputGammaLutTexture = 0;
    coduompOutputGammaProgram = 0;
    coduompOutputGammaSoftwareAvailable = qfalse;
    coduompOutputGammaSoftwareActive = qfalse;
    coduompOutputGammaLutIdentity = qtrue;
}
