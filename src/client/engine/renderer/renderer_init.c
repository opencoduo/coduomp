#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "output_gamma_compat.h"
#include "platform_gamma.h"

#include <string.h>

enum {
    R_MIN_CONFIGURABLE_TEXTURE_SIZE = 1024,
    R_MAX_HARDWARE_TEXTURE_SIZE = 8192,
    R_MIN_HARDWARE_LIGHTS = 8,
    R_MAX_HARDWARE_LIGHTS = 16,
    R_GL_ERROR_STRING_SIZE = 64,
    R_CUSTOM_VIDEO_MODE = -1,
    R_VIDEO_MODE_COUNT = 13
};

typedef struct renderer_video_mode_s {
    const char *description;
    int32_t width;
    int32_t height;
    float pixelAspect;
} renderer_video_mode_t;

/* Original CoDUOMP.exe table 0x005ce838..0x005ce907 and count at 0x005ce908.
 * All thirteen Windows records use square pixels.
 * PE_RELOCATION_VALUES_VERIFIED: all thirteen description pointers match. */
static const renderer_video_mode_t rendererVideoModes[R_VIDEO_MODE_COUNT] = {
    {"Mode  0: 320x240", 320, 240, 1.0f},           {"Mode  1: 400x300", 400, 300, 1.0f},     {"Mode  2: 512x384", 512, 384, 1.0f},
    {"Mode  3: 640x480", 640, 480, 1.0f},           {"Mode  4: 800x600", 800, 600, 1.0f},     {"Mode  5: 960x720", 960, 720, 1.0f},
    {"Mode  6: 1024x768", 1024, 768, 1.0f},         {"Mode  7: 1152x864", 1152, 864, 1.0f},   {"Mode  8: 1280x1024", 1280, 1024, 1.0f},
    {"Mode  9: 1600x1200", 1600, 1200, 1.0f},       {"Mode 10: 2048x1536", 2048, 1536, 1.0f}, {"Mode 11: 856x480 (wide)", 856, 480, 1.0f},
    {"Mode 12: 1920x1200 (wide)", 1920, 1200, 1.0f}};

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: modern square-pixel display modes
 * appended after the original stable 0..12 mode-number range. */
static const renderer_video_mode_t coduompModernVideoModes[] = {
    {"Mode 13: 1280x720 (wide)", 1280, 720, 1.0f},   {"Mode 14: 1280x800 (wide)", 1280, 800, 1.0f},
    {"Mode 15: 1366x768 (wide)", 1366, 768, 1.0f},   {"Mode 16: 1440x900 (wide)", 1440, 900, 1.0f},
    {"Mode 17: 1600x900 (wide)", 1600, 900, 1.0f},   {"Mode 18: 1680x1050 (wide)", 1680, 1050, 1.0f},
    {"Mode 19: 1920x1080 (wide)", 1920, 1080, 1.0f}, {"Mode 20: 2560x1440 (wide)", 2560, 1440, 1.0f},
    {"Mode 21: 2560x1600 (wide)", 2560, 1600, 1.0f}, {"Mode 22: 2880x1800 (wide)", 2880, 1800, 1.0f},
    {"Mode 23: 3024x1964 (wide)", 3024, 1964, 1.0f}, {"Mode 24: 3456x2234 (wide)", 3456, 2234, 1.0f},
    {"Mode 25: 3840x2160 (wide)", 3840, 2160, 1.0f}};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_video_mode_t) == 4, "i386 renderer video-mode alignment changed");
_Static_assert(offsetof(renderer_video_mode_t, description) == 0x00, "i386 renderer video-mode description moved");
_Static_assert(offsetof(renderer_video_mode_t, width) == 0x04, "i386 renderer video-mode width moved");
_Static_assert(offsetof(renderer_video_mode_t, height) == 0x08, "i386 renderer video-mode height moved");
_Static_assert(offsetof(renderer_video_mode_t, pixelAspect) == 0x0c, "i386 renderer video-mode pixel aspect moved");
_Static_assert(sizeof(renderer_video_mode_t) == 0x10, "i386 renderer video-mode record size changed");
#endif

/* Source: CoDUOMP.exe 0x004c12a0..0x004c1392.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c12a0_004c1392.mcode.
 * Name: exact same-module Mac symbol AssertCvarRange, with the signature and
 * three warning paths independently proved by its five Windows call sites.
 * MSVC carries cvar/integral in ESI/EAX and the two float bounds on the stack;
 * the maintained source restores the ordinary source calling convention. */
void AssertCvarRange(cvar_t *cvar, float minimum, float maximum, qboolean integral)
{
    if (integral != qfalse && (int32_t)cvar->value != cvar->integer) {
        ri.Printf(R_PRINT_WARNING, "WARNING: cvar '%s' must be integral (%f)\n", cvar->name, cvar->value);
        ri.Cvar_Set(cvar->name, va("%d", cvar->integer));
    }

    if (cvar->value < minimum) {
        ri.Printf(R_PRINT_WARNING, "WARNING: cvar '%s' out of range (%f < %f)\n", cvar->name, cvar->value, minimum);
        ri.Cvar_Set(cvar->name, va("%f", minimum));
        return;
    }

    if (cvar->value > maximum) {
        ri.Printf(R_PRINT_WARNING, "WARNING: cvar '%s' out of range (%f > %f)\n", cvar->name, cvar->value, maximum);
        ri.Cvar_Set(cvar->name, va("%f", maximum));
    }
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
void GL_SetupVBO(void)
{
    tr.vboInterleaved = (r_vbo_interleave->integer != 0);
    tr.vboStreamDraw = (r_vbo_stream_draw->integer != 0);
    tr.vboUsage = (r_vbo_smc_static_draw != NULL) ? GL_STATIC_DRAW_ARB : GL_DYNAMIC_DRAW_ARB;
}

/* Source: CoDUOMP.exe 0x004c13f0..0x004c1541.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c13f0_004c1541.mcode.
 * Name and source-level GL_SetupVBO boundary: exact same-module Mac symbols.
 * The Mac InitOpenGL call graph also confirms GLimp_Init, strcpy, Q_strlwr,
 * both qglGetIntegerv calls, GfxInfo_f, and GL_SetDefaultState in this order. */
void InitOpenGL(void)
{
    char rendererString[MAX_STRING_CHARS];

    if (glConfig.vidWidth == 0) {
        GLimp_Init();

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Q_strncpyz(rendererString, glConfig.rendererString, (int32_t)sizeof(rendererString));
        Q_strlwr(rendererString);

        qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize);
        if (glConfig.maxTextureSize <= 0) {
            glConfig.maxTextureSize = 0;
        } else if (glConfig.maxTextureSize > R_MAX_HARDWARE_TEXTURE_SIZE) {
            glConfig.maxTextureSize = R_MAX_HARDWARE_TEXTURE_SIZE;
        }

        if (r_maxTextureSize->integer >= R_MIN_CONFIGURABLE_TEXTURE_SIZE) {
            while (glConfig.maxTextureSize > r_maxTextureSize->integer)
                glConfig.maxTextureSize /= 2;
        }

        qglGetIntegerv(GL_MAX_LIGHTS, &glConfig.maxLights);
        if (glConfig.maxLights <= 0) {
            glConfig.maxLights = R_MIN_HARDWARE_LIGHTS;
        } else if (glConfig.maxLights > R_MAX_HARDWARE_LIGHTS) {
            glConfig.maxLights = R_MAX_HARDWARE_LIGHTS;
        }
    }

    GfxInfo_f();
    glState.enabledLightCount = 0;
    GL_SetDefaultState();
    GL_SetupVBO();
}

/* Source: CoDUOMP.exe 0x004c1550..0x004c1715.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1550_004c1715.mcode.
 * Name and signature: exact same-module Mac symbol GL_CheckErrors and its
 * one-argument call sites. The Mac body independently confirms qglGetError,
 * the six strcpy cases, Com_sprintf fallback, and fatal error call. */
void GL_CheckErrors(const char *location)
{
    char errorString[R_GL_ERROR_STRING_SIZE];
    const uint32_t error = qglGetError();

    if (error == GL_NO_ERROR || r_ignoreGLErrors->integer != 0)
        return;

    switch (error) {
    case GL_INVALID_ENUM:
        strcpy(errorString, "GL_INVALID_ENUM");
        break;
    case GL_INVALID_VALUE:
        strcpy(errorString, "GL_INVALID_VALUE");
        break;
    case GL_INVALID_OPERATION:
        strcpy(errorString, "GL_INVALID_OPERATION");
        break;
    case GL_STACK_OVERFLOW:
        strcpy(errorString, "GL_STACK_OVERFLOW");
        break;
    case GL_STACK_UNDERFLOW:
        strcpy(errorString, "GL_STACK_UNDERFLOW");
        break;
    case GL_OUT_OF_MEMORY:
        strcpy(errorString, "GL_OUT_OF_MEMORY");
        break;
    default:
        Com_sprintf(errorString, sizeof(errorString), "%i", (int32_t)error);
        break;
    }

    ri.Error(ERR_FATAL, "\x15GL_CheckErrors: %s (location = %s)", errorString, location);
}

/* Source: CoDUOMP.exe 0x004c1730..0x004c179f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1730_004c179f.mcode.
 * Name and signature: exact same-module Mac symbol R_GetModeInfo. The Mac body
 * independently confirms all four arguments, the custom-mode cvar fields, the
 * 16-byte table stride, and width / (height * pixelAspect) calculation. */
qboolean R_GetModeInfo(int32_t *width, int32_t *height, float *windowAspect, int32_t mode)
{
    const renderer_video_mode_t *videoMode;
    const int32_t modernModeCount = (int32_t)(sizeof(coduompModernVideoModes) / sizeof(coduompModernVideoModes[0]));

    if (mode < R_CUSTOM_VIDEO_MODE || mode >= R_VIDEO_MODE_COUNT + modernModeCount) {
        return qfalse;
    }

    if (mode == R_CUSTOM_VIDEO_MODE) {
        *width = r_customwidth->integer;
        *height = r_customheight->integer;
        *windowAspect = r_customaspect->value;
        return qtrue;
    }

    if (mode < R_VIDEO_MODE_COUNT) {
        videoMode = &rendererVideoModes[mode];
    } else {
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): preserve all stock
         * indices while accepting appended modern display modes. */
        videoMode = &coduompModernVideoModes[mode - R_VIDEO_MODE_COUNT];
    }
    *width = videoMode->width;
    *height = videoMode->height;
    *windowAspect = (float)videoMode->width / ((float)videoMode->height * videoMode->pixelAspect);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: publishes one bit per stable renderer mode so the
 * separately linked compatibility UI can omit presets that the primary
 * display does not report. */
void coduomp_renderer_publish_available_video_modes_compat(coduomp_display_mode_available_callback_t modeAvailable)
{
    const int32_t modeCount = R_VIDEO_MODE_COUNT + (int32_t)(sizeof(coduompModernVideoModes) / sizeof(coduompModernVideoModes[0]));
    uint32_t availableModes = 0;

    for (int32_t mode = 0; mode < modeCount; ++mode) {
        int32_t width;
        int32_t height;
        float aspect;

        if (R_GetModeInfo(&width, &height, &aspect, mode) != qfalse && modeAvailable(width, height) != qfalse) {
            availableModes |= UINT32_C(1) << mode;
        }
    }

    ri.Cvar_Set("r_availableModes", va("%u", availableModes));
}

/* Source: CoDUOMP.exe 0x004c17a0..0x004c17f5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c17a0_004c17f5.mcode. Ghidra left
 * this complete function in the executable-gap inventory; its prologue,
 * table/count loop, return, and following INT3 padding prove the repaired
 * boundary. Name and three print calls: exact same-module Mac R_ModeList_f. */
void R_ModeList_f(void)
{
    int32_t mode;

    ri.Printf(R_PRINT_ALL, "\n");
    ri.Printf(R_PRINT_ALL, "Maximum display, automatic (r_mode %d)\n", R_CURRENT_DISPLAY_VIDEO_MODE);
    for (mode = 0; mode < R_VIDEO_MODE_COUNT; ++mode)
        ri.Printf(R_PRINT_ALL, "%s\n", rendererVideoModes[mode].description);
    for (mode = 0; mode < (int32_t)(sizeof(coduompModernVideoModes) / sizeof(coduompModernVideoModes[0])); ++mode) {
        ri.Printf(R_PRINT_ALL, "%s\n", coduompModernVideoModes[mode].description);
    }
    ri.Printf(R_PRINT_ALL, "\n");
}

/* Source: CoDUOMP.exe 0x004c3160..0x004c3243.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c3160_004c3243.mcode.
 * Name and common-tail source structure: exact same-module Mac symbol
 * R_SetNVFogMode. Windows proves the three case-insensitive strings, exact GL
 * enum values, glConfig +0x58 destination, fallback Cvar_Set, and qglFogi call. */
void R_SetNVFogMode(void)
{
    const char *mode = r_nv_fogdist_mode->string;

    if (mode != NULL && Q_stricmp(mode, "GL_EYE_PLANE_ABSOLUTE_NV") == 0) {
        glConfig.NVFogMode = GL_EYE_PLANE_ABSOLUTE_NV;
    } else if (mode != NULL && Q_stricmp(mode, "GL_EYE_PLANE") == 0) {
        glConfig.NVFogMode = GL_EYE_PLANE;
    } else if (mode != NULL && Q_stricmp(mode, "GL_EYE_RADIAL_NV") == 0) {
        glConfig.NVFogMode = GL_EYE_RADIAL_NV;
    } else {
        glConfig.NVFogMode = GL_EYE_RADIAL_NV;
        ri.Cvar_Set("r_nv_fogdist_mode", "GL_EYE_RADIAL_NV");
    }

    qglFogi(GL_FOG_DISTANCE_MODE_NV, glConfig.NVFogMode);
}

/* Original fixed-function defaults at 0x00590ec8..0x00590f07. The four
 * independent 16-byte blocks are passed directly to the corresponding OpenGL
 * material/light-model calls by GL_SetDefaultState. */
static const vec4_t defaultMaterialAmbientAndDiffuse = {1.0f, 1.0f, 1.0f, 1.0f};
static const vec4_t defaultMaterialSpecular = {0.0f, 0.0f, 0.0f, 1.0f};
static const vec4_t defaultMaterialEmission = {0.0f, 0.0f, 0.0f, 1.0f};
static const vec4_t defaultLightModelAmbient = {0.0f, 0.0f, 0.0f, 1.0f};

/* Source: CoDUOMP.exe 0x004c3250..0x004c3650.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c3250_004c3650.mcode.
 * Name and source-level calls: exact same-module Mac symbol
 * GL_SetDefaultState. The Windows instructions independently prove every GL
 * enum, extension gate, cvar field, cache write, and call order. MSVC inlined
 * GL_SelectTexture and GL_TexEnv into this body; maintained source restores
 * those original helper boundaries. */
void GL_SetDefaultState(void)
{
    int32_t textureUnit;

    qglClearDepth(1.0);
    qglCullFace(GL_FRONT);
    qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    if (qglActiveTextureARB != NULL) {
        for (textureUnit = 1; textureUnit < glConfig.maxActiveTextures; ++textureUnit) {
            GL_SelectTexture(textureUnit);
            GL_TextureMode(r_textureMode->string);
            GL_TexEnv(GL_MODULATE);
            if (glConfig.cubeMapAvailable)
                qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
            qglDisable(GL_TEXTURE_2D);
            qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glState.currentTextureTargets[textureUnit] = 0;
        }
        GL_SelectTexture(0);
    }

    if (glConfig.cubeMapAvailable)
        qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
    qglEnable(GL_TEXTURE_2D);
    qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glState.currentTextureTargets[0] = GL_TEXTURE_2D;
    GL_TextureMode(r_textureMode->string);
    GL_TexEnv(GL_MODULATE);

    qglShadeModel(GL_SMOOTH);
    qglDepthFunc(GL_LEQUAL);
    qglLightModelfv(GL_LIGHT_MODEL_AMBIENT, defaultLightModelAmbient);
    qglLightModelf(GL_LIGHT_MODEL_LOCAL_VIEWER, 0.0f);
    qglLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 0.0f);
    qglMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, defaultMaterialAmbientAndDiffuse);
    qglMaterialfv(GL_FRONT, GL_SPECULAR, defaultMaterialSpecular);
    qglMaterialf(GL_FRONT, GL_SHININESS, 0.0f);
    qglMaterialfv(GL_FRONT, GL_EMISSION, defaultMaterialEmission);

    qglEnableClientState(GL_VERTEX_ARRAY);
    qglDisableClientState(GL_COLOR_ARRAY);
    qglDisableClientState(GL_NORMAL_ARRAY);
    glState.clientStateBits = GLS_CLIENT_VERTEX_ARRAY;
    glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;

    qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    qglDepthMask(1);
    qglDisable(GL_DEPTH_TEST);
    qglEnable(GL_SCISSOR_TEST);
    qglDisable(GL_CULL_FACE);
    qglDisable(GL_BLEND);

    if (qglPNTrianglesiATI != NULL) {
        qglGetIntegerv(GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI, &glConfig.maxPNTrianglesTessellationLevel);
        if ((float)glConfig.maxPNTrianglesTessellationLevel < r_ati_truform_tess->value) {
            ri.Cvar_Set("r_ati_truform_tess", va("%d", glConfig.maxPNTrianglesTessellationLevel));
        }
        qglPNTrianglesiATI(GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, r_ati_truform_tess->integer);
    }

    if (glConfig.textureFilterAnisotropicAvailable) {
        qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.maxTextureFilterAnisotropy);
    }

    glState.fogMode = GL_EXP;
    glState.fogHint = GL_DONT_CARE;
    glState.fogColor[0] = 0.0f;
    glState.fogColor[1] = 0.0f;
    glState.fogColor[2] = 0.0f;
    glState.fogColor[3] = 0.0f;
    glState.fogStart = 0.0f;
    glState.fogEnd = 1.0f;
    glState.fogDensity = 1.0f;

    qglFogi(GL_FOG_MODE, glState.fogMode);
    qglHint(GL_FOG_HINT, glState.fogHint);
    qglFogf(GL_FOG_DENSITY, glState.fogDensity);
    qglFogf(GL_FOG_START, glState.fogStart);
    qglFogf(GL_FOG_END, glState.fogEnd);

    if (glConfig.fogDistanceAvailable)
        R_SetNVFogMode();

    glState.enabledLightCount = 0;
    glState.currentStorageMode = R_STATIC_VERTEX_MEMORY_HUNK;
}

/* Source: CoDUOMP.exe 0x004c3650..0x004c39d3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c3650_004c39d3.mcode.
 * Name: exact same-module Mac symbol GfxInfo_f. Windows proves the complete
 * print sequence, each cvar/config source, extension-pointer gate, and the
 * rendering-primitives selection. */
void GfxInfo_f(void)
{
    /* Conditional string pairs constructed in the original stack frame. */
    const char *const enabledNames[2] = {"disabled", "enabled"};
    const char *const windowModeNames[3] = {"windowed", "fullscreen", "borderless"};
    cvar_t *cpuString;

    cpuString = ri.Cvar_Get("sys_cpustring", "", CVAR_NONE);

    ri.Printf(R_PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendorString);
    ri.Printf(R_PRINT_ALL, "GL_RENDERER: %s\n", glConfig.rendererString);
    ri.Printf(R_PRINT_ALL, "GL_VERSION: %s\n", glConfig.versionString);
    ri.Printf(R_PRINT_ALL, "GL_EXTENSIONS: %s\n", glConfig.extensionsString);
    ri.Printf(R_PRINT_ALL, "WGL_EXTENSIONS: %s\n", glConfig.wglExtensionsString);
    ri.Printf(R_PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize);
    ri.Printf(R_PRINT_ALL, "GL_MAX_ACTIVE_TEXTURES_ARB: %d\n", glConfig.maxActiveTextures);
    ri.Printf(R_PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits,
              glConfig.stencilBits);

    int32_t windowMode = r_fullscreen->integer;
    if (windowMode < 0 || windowMode > 2)
        windowMode = 0;

    ri.Printf(R_PRINT_ALL, "MODE: %d, %d x %d %s hz:", r_mode->integer, glConfig.vidWidth, glConfig.vidHeight, windowModeNames[windowMode]);
    if (glConfig.displayFrequency != 0)
        ri.Printf(R_PRINT_ALL, "%d\n", glConfig.displayFrequency);
    else
        ri.Printf(R_PRINT_ALL, "N/A\n");

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): name the selected gamma
     * provider rather than reporting every non-native policy as software. */
    if (glConfig.deviceSupportsGamma != qfalse) {
        ri.Printf(R_PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits);
    } else if (coduomp_output_gamma_software_active_compat() != qfalse) {
        ri.Printf(R_PRINT_ALL, "GAMMA: full-frame software w/ %d overbright bits\n", tr.overbrightBits);
    } else if (coduomp_gamma_texture_fallback_enabled_compat() != qfalse) {
        ri.Printf(R_PRINT_ALL, "GAMMA: texture fallback w/ %d overbright bits\n", tr.overbrightBits);
    } else {
        ri.Printf(R_PRINT_ALL, "GAMMA: disabled\n");
    }

    ri.Printf(R_PRINT_ALL, "CPU: %s\n", cpuString->string);
    ri.Printf(R_PRINT_ALL, "rendering primitives: ");
    switch ((renderer_primitive_mode_t)r_primitives->integer) {
    case R_PRIMITIVES_NONE:
        ri.Printf(R_PRINT_ALL, "none\n");
        break;
    case R_PRIMITIVES_AUTOMATIC:
#if defined(__APPLE__) && defined(__aarch64__)
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): report the native
         * automatic-mode adaptation implemented in R_DrawElements. */
        ri.Printf(R_PRINT_ALL, "single glDrawElements\n");
#else
        ri.Printf(R_PRINT_ALL, qglLockArraysEXT != NULL ? "single glDrawElements\n" : "multiple glArrayElement\n");
#endif
        break;
    case R_PRIMITIVES_ARRAY_ELEMENTS:
        ri.Printf(R_PRINT_ALL, "multiple glArrayElement\n");
        break;
    case R_PRIMITIVES_DRAW_ELEMENTS:
        ri.Printf(R_PRINT_ALL, "single glDrawElements\n");
        break;
    case R_PRIMITIVES_IMMEDIATE:
        ri.Printf(R_PRINT_ALL, "multiple glColor4ubv + glTexCoord2fv + glVertex3fv\n");
        break;
    }

    ri.Printf(R_PRINT_ALL, "texturemode: %s\n", r_textureMode->string);
    ri.Printf(R_PRINT_ALL, "picmip: %d\n", r_picmip->integer);
    ri.Printf(R_PRINT_ALL, "picmip2: %d\n", r_picmip2->integer);
    ri.Printf(R_PRINT_ALL, "texture bits: %d\n", r_texturebits->integer);
    ri.Printf(R_PRINT_ALL, "multitexture: %s\n", enabledNames[qglActiveTextureARB != NULL]);
    ri.Printf(R_PRINT_ALL, "compiled vertex arrays: %s\n", enabledNames[qglLockArraysEXT != NULL]);
    ri.Printf(R_PRINT_ALL, "texenv add: %s\n", enabledNames[glConfig.textureEnvAddAvailable != 0]);
    ri.Printf(R_PRINT_ALL, "ATI truform: %s\n", enabledNames[qglPNTrianglesiATI != NULL]);

    if (qglPNTrianglesiATI != NULL) {
        ri.Printf(R_PRINT_ALL, "Truform Tess: %d\n", r_ati_truform_tess->integer);
        ri.Printf(R_PRINT_ALL, "Truform Point Mode: %s\n", r_ati_truform_pointmode->string);
        ri.Printf(R_PRINT_ALL, "Truform Normal Mode: %s\n", r_ati_truform_normalmode->string);
    }

    ri.Printf(R_PRINT_ALL, "NV distance fog: %s\n", enabledNames[glConfig.fogDistanceAvailable != 0]);
    if (glConfig.fogDistanceAvailable) {
        ri.Printf(R_PRINT_ALL, "Fog Mode: %s\n", r_nv_fogdist_mode->string);
    }

    if (r_finish->integer != 0)
        ri.Printf(R_PRINT_ALL, "Forcing glFinish\n");
}
