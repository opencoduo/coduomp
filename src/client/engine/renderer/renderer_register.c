#include "backend.h"

#include "gl_state.h"
#include "platform_gamma.h"
#include "renderer_cvars.h"
#include "../platform/hardware_profile.h"

enum {
    R_DEFAULT_MAX_POLYS = 4096,
    R_DEFAULT_MAX_POLYVERTS = 16384
};

cvar_t *r_cheats;
cvar_t *r_maxActiveTextures;
cvar_t *r_maxTextureSize;
cvar_t *r_ext_compiled_vertex_array;
cvar_t *r_ext_rescale_normal;
cvar_t *r_ext_draw_range_elements;
cvar_t *r_ati_pntriangles;
cvar_t *r_ati_truform_tess;
cvar_t *r_ati_truform_normalmode;
cvar_t *r_ati_truform_pointmode;
cvar_t *r_ati_fsaa_samples;
cvar_t *r_ext_texture_filter_anisotropic;
cvar_t *r_nv_fog_dist;
cvar_t *r_nv_fogdist_mode;
cvar_t *r_arb_texture_env_add;
cvar_t *r_arb_texture_cube_map;
cvar_t *r_arb_texture_env_combine;
cvar_t *r_arb_texture_env_dot3;
cvar_t *r_arb_vertex_buffer_object;
cvar_t *r_arb_vertex_program;
cvar_t *r_nv_register_combiners;
cvar_t *r_nv_texture_shader;
cvar_t *r_nv_fence;
cvar_t *r_nv_vertex_array_range;
cvar_t *r_ati_vertex_array_object;
cvar_t *r_ati_element_array;
cvar_t *r_ati_fragment_shader;
cvar_t *r_vbo_smc_static_draw;
cvar_t *r_vbo_stream_draw;
cvar_t *r_vbo_interleave;
cvar_t *r_vbo_paranoia;
cvar_t *r_vbo_stream_map;
cvar_t *r_skip_auto_config;
cvar_t *r_picmip;
cvar_t *r_picmip2;
cvar_t *r_colorMipLevels;
cvar_t *r_detailtextures;
cvar_t *r_texturebits;
cvar_t *r_colorbits;
cvar_t *r_stencilbits;
cvar_t *r_depthbits;
cvar_t *r_overBrightBits;
cvar_t *r_ignorehwgamma;
cvar_t *r_gammaMode;
cvar_t *r_mode;
cvar_t *r_fullscreen;
cvar_t *r_aspectMode;
cvar_t *r_hiresReticles;
cvar_t *r_customwidth;
cvar_t *r_customheight;
cvar_t *r_customaspect;
cvar_t *r_simpleMipMaps;
cvar_t *r_weightMipMaps;
cvar_t *r_uifullscreen;
cvar_t *r_displayRefresh;
cvar_t *r_fullbright;
cvar_t *r_intensity;
cvar_t *r_singleShader;
cvar_t *r_lodbias;
cvar_t *r_flares;
cvar_t *r_flareOcclusionQuery;
cvar_t *r_znear;
cvar_t *r_zfar;
cvar_t *r_znear_depthhack;
cvar_t *r_ignoreGLErrors;
cvar_t *r_fastsky;
cvar_t *r_inGameVideo;
cvar_t *r_drawSun;
cvar_t *r_dynamiclight;
cvar_t *r_dlightQuality;
cvar_t *r_finish;
cvar_t *r_textureMode;
cvar_t *r_swapDelay;
cvar_t *r_swapInterval;
cvar_t *r_gamma;
cvar_t *r_railWidth;
cvar_t *r_railCoreWidth;
cvar_t *r_railSegmentLength;
cvar_t *r_primitives;
cvar_t *r_showImages;
cvar_t *r_debugSort;
cvar_t *r_printShaders;
cvar_t *r_saveFontData;
cvar_t *r_LightScale;
cvar_t *r_showLeafLights;
cvar_t *r_debugEntLight;
cvar_t *r_maxEntLights;
cvar_t *r_minEntLightIntensity;
cvar_t *r_entLightCutoff;
cvar_t *r_entFullbright;
cvar_t *r_entMinLight;
cvar_t *r_diffuseSunSteps;
cvar_t *r_diffuseSunQuality;
cvar_t *r_vc_makelog;
cvar_t *r_vc_showlog;
cvar_t *r_vc_compile;
cvar_t *r_fog;
cvar_t *r_drawworld;
cvar_t *r_lightmap;
cvar_t *r_graymap;
cvar_t *r_portalOnly;
cvar_t *r_flareSize;
cvar_t *r_flareFadeIn;
cvar_t *r_flareFadeOut;
cvar_t *r_skipBackEnd;
cvar_t *r_measureOverdraw;
cvar_t *r_lodscale;
cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawBModels;
cvar_t *r_drawSModels;
cvar_t *r_drawXModels;
cvar_t *r_ignore;
cvar_t *r_nocull;
cvar_t *outsideMapEnts;
cvar_t *r_speeds;
cvar_t *r_verbose;
cvar_t *r_logFile;
cvar_t *r_debugGLErrors;
cvar_t *r_profileDrawElements;
cvar_t *r_nobind;
cvar_t *r_showtris;
cvar_t *r_showtricounts;
cvar_t *r_showsurfcounts;
cvar_t *r_showsky;
cvar_t *r_shownormals;
cvar_t *r_clear;
cvar_t *r_polygonOffsetFactor;
cvar_t *r_polygonOffsetUnits;
cvar_t *r_drawBuffer;
cvar_t *r_lockpvs;
cvar_t *r_noportals;
cvar_t *cg_shadows;
cvar_t *cg_skybox;
cvar_t *r_maxpolys;
cvar_t *r_maxpolyverts;
cvar_t *r_showportals;
cvar_t *r_showaabbtrees;
cvar_t *r_cullBModels;
cvar_t *r_showCullBModels;
cvar_t *r_showCullSModels;
cvar_t *r_cullXModels;
cvar_t *r_showCullXModels;
cvar_t *r_singlecell;
cvar_t *r_portalbevels;
cvar_t *r_xdebug;
cvar_t *r_errorOnConflicts;
cvar_t *r_highLodDist;
cvar_t *r_mediumLodDist;
cvar_t *r_lowLodDist;
cvar_t *r_lodViewDist;
cvar_t *r_suntest;
cvar_t *r_sunsprite_shader;
cvar_t *r_sunsprite_size;
cvar_t *r_sunflare_shader;
cvar_t *r_sunflare_min_size;
cvar_t *r_sunflare_min_angle;
cvar_t *r_sunflare_max_size;
cvar_t *r_sunflare_max_angle;
cvar_t *r_sunflare_max_alpha;
cvar_t *r_sunflare_fadein;
cvar_t *r_sunflare_fadeout;
cvar_t *r_sunblind_min_angle;
cvar_t *r_sunblind_max_angle;
cvar_t *r_sunblind_max_darken;
cvar_t *r_sunblind_fadein;
cvar_t *r_sunblind_fadeout;
cvar_t *r_sunglare_min_angle;
cvar_t *r_sunglare_max_angle;
cvar_t *r_sunglare_max_lighten;
cvar_t *r_sunglare_fadein;
cvar_t *r_sunglare_fadeout;
cvar_t *r_optimize;
cvar_t *r_optimizeBackend;
cvar_t *r_optimizeSModels;
cvar_t *r_optimizeXModels;
cvar_t *r_optimizeWorld;
cvar_t *r_optimizeTextures;
cvar_t *r_debugOptTex;
cvar_t *r_debugOptModel;
cvar_t *r_mem_manual;
cvar_t *r_mem_agp;
cvar_t *r_mem_video;
cvar_t *r_mem_backend;
cvar_t *r_smc_enable;

/* Source: CoDUOMP.exe 0x004c3b10..0x004c4ddc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c3b10_004c4ddc.mcode.
 * Name: exact same-module Mac symbol R_Register. The Windows body proves every
 * cvar destination, registration string, default, flag word, range assertion,
 * and command callback below. The Mac implementation independently confirms
 * the source-level registration boundary and command names. */
void R_Register(void)
{
    r_cheats = Cvar_Get("sv_cheats", "0",
                        CVAR_ROM | CVAR_SYSTEMINFO);
    r_maxActiveTextures =
        ri.Cvar_Get("r_maxActiveTextures", "0", CVAR_LATCH);
    r_maxTextureSize = ri.Cvar_Get("r_maxTextureSize", "0", CVAR_LATCH);
    r_ext_compiled_vertex_array =
        ri.Cvar_Get("r_ext_compiled_vertex_array", "1", CVAR_LATCH);
    r_ext_rescale_normal =
        ri.Cvar_Get("r_ext_rescale_normal", "1", CVAR_LATCH);
    r_ext_draw_range_elements =
        ri.Cvar_Get("r_ext_draw_range_elements", "1", CVAR_LATCH);
    r_ati_pntriangles = ri.Cvar_Get(
        "r_ati_pntriangles", "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_ati_truform_tess =
        ri.Cvar_Get("r_ati_truform_tess", "1", CVAR_ARCHIVE);
    r_ati_truform_normalmode = ri.Cvar_Get(
        "r_ati_truform_normalmode", "QUADRATIC", CVAR_ARCHIVE);
    r_ati_truform_pointmode = ri.Cvar_Get(
        "r_ati_truform_pointmode", "CUBIC", CVAR_ARCHIVE);
    r_ati_fsaa_samples =
        ri.Cvar_Get("r_ati_fsaa_samples", "1", CVAR_ARCHIVE);
    r_ext_texture_filter_anisotropic = ri.Cvar_Get(
        "r_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE);
    r_nv_fog_dist = ri.Cvar_Get(
        "r_nv_fog_dist", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_nv_fogdist_mode = ri.Cvar_Get(
        "r_nv_fogdist_mode", "GL_EYE_RADIAL_NV", CVAR_ARCHIVE);
    (void)ri.Cvar_Get("r_nv_fog_available", "1", CVAR_ROM);
    r_arb_texture_env_add =
        ri.Cvar_Get("r_arb_texture_env_add", "1", CVAR_LATCH);
    r_arb_texture_cube_map =
        ri.Cvar_Get("r_arb_texture_cube_map", "1", CVAR_LATCH);
    r_arb_texture_env_combine =
        ri.Cvar_Get("r_arb_texture_env_combine", "1", CVAR_LATCH);
    r_arb_texture_env_dot3 =
        ri.Cvar_Get("r_arb_texture_env_dot3", "1", CVAR_LATCH);
    r_arb_vertex_buffer_object =
        ri.Cvar_Get("r_arb_vertex_buffer_object", "1", CVAR_LATCH);
    r_arb_vertex_program =
        ri.Cvar_Get("r_arb_vertex_program", "1", CVAR_LATCH);
    r_nv_register_combiners =
        ri.Cvar_Get("r_nv_register_combiners", "2", CVAR_LATCH);
    r_nv_texture_shader =
        ri.Cvar_Get("r_nv_texture_shader", "1", CVAR_LATCH);
    r_nv_fence = ri.Cvar_Get("r_nv_fence", "1", CVAR_LATCH);
    r_nv_vertex_array_range =
        ri.Cvar_Get("r_nv_vertex_array_range", "2", CVAR_LATCH);
    r_ati_vertex_array_object =
        ri.Cvar_Get("r_ati_vertex_array_object", "1", CVAR_LATCH);
    r_ati_element_array =
        ri.Cvar_Get("r_ati_element_array", "1", CVAR_LATCH);
    r_ati_fragment_shader =
        ri.Cvar_Get("r_ati_fragment_shader", "1", CVAR_LATCH);
    r_vbo_smc_static_draw = ri.Cvar_Get(
        "r_vbo_smc_static_draw", "1", CVAR_ARCHIVE | CVAR_LATCH);
    /* PERFORMANCE_PATCH (NOT_FROM_ORIGINAL_SOURCE): default to the reusable dynamic-VBO upload path while preserving user selection. */
    r_vbo_stream_draw = ri.Cvar_Get("r_vbo_stream_draw", "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_vbo_interleave = ri.Cvar_Get(
        "r_vbo_interleave", "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_vbo_paranoia =
        ri.Cvar_Get("r_vbo_paranoia", "0", CVAR_ARCHIVE);
    /* NOT_FROM_ORIGINAL_SOURCE DIAGNOSTIC: 0 sends stream-mode stage batches
     * through the recovered map-failure fallback (temporary memory plus
     * BufferData) so the original unmap-failure draw suppression can never
     * trigger (CoDUOMP.exe 0x51ee5d..0x51eec8). */
    r_vbo_stream_map =
        ri.Cvar_Get("r_vbo_stream_map", "1", CVAR_ARCHIVE);
    r_skip_auto_config = ri.Cvar_Get(
        "r_skip_auto_config", "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_picmip =
        ri.Cvar_Get("r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_picmip2 =
        ri.Cvar_Get("r_picmip2", "2", CVAR_ARCHIVE | CVAR_LATCH);
    r_colorMipLevels = ri.Cvar_Get(
        "r_colorMipLevels", "0", CVAR_CHEAT | CVAR_LATCH);
    AssertCvarRange(r_picmip, 0.0f, 3.0f, qtrue);
    AssertCvarRange(r_picmip2, 0.0f, 3.0f, qtrue);

    r_detailtextures = ri.Cvar_Get(
        "r_detailtextures", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_texturebits = ri.Cvar_Get(
        "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_colorbits =
        ri.Cvar_Get("r_colorbits", "32", CVAR_ARCHIVE | CVAR_LATCH);
    r_stencilbits =
        ri.Cvar_Get("r_stencilbits", "8", CVAR_ARCHIVE | CVAR_LATCH);
    r_depthbits =
        ri.Cvar_Get("r_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_overBrightBits = ri.Cvar_Get(
        "r_overBrightBits", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_ignorehwgamma = ri.Cvar_Get(
        "r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_LATCH);
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): select disabled,
     * automatic native-with-fallback, or forced final-frame software gamma.
     * The existing r_ignorehwgamma remains an independent native-ramp veto. */
    r_gammaMode = ri.Cvar_Get(
        "r_gammaMode", "1", CVAR_ARCHIVE | CVAR_LATCH);
    AssertCvarRange(r_gammaMode,
                    (float)CODUOMP_GAMMA_MODE_DISABLED,
                    (float)CODUOMP_GAMMA_MODE_SOFTWARE, qtrue);
    r_mode = ri.Cvar_Get("r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH);
    r_fullscreen =
        ri.Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH);
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): zero uses the native
     * display aspect; one presents the complete renderer in a fitted 4:3
     * viewport with black bars. Keep the cgame and renderer presentation
     * transition atomic by applying it only during a vid_restart. */
    r_aspectMode =
        ri.Cvar_Get(
            "r_aspectMode", "0", CVAR_ARCHIVE | CVAR_LATCH);
    /* NOT_FROM_ORIGINAL_SOURCE: platform-discovered renderer-mode bits for
     * the separately linked compatibility UI. */
    (void)ri.Cvar_Get("r_availableModes", "0", CVAR_ROM);
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): edge-preserving
     * load-time upscale of gfx/reticle/ images; latched because the
     * transform runs when images load. */
    r_hiresReticles = ri.Cvar_Get(
        "r_hiresReticles", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_customwidth = ri.Cvar_Get(
        "r_customwidth", "1600", CVAR_ARCHIVE | CVAR_LATCH);
    r_customheight = ri.Cvar_Get(
        "r_customheight", "1024", CVAR_ARCHIVE | CVAR_LATCH);
    r_customaspect = ri.Cvar_Get(
        "r_customaspect", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_simpleMipMaps = ri.Cvar_Get(
        "r_simpleMipMaps", "1", CVAR_ARCHIVE | CVAR_LATCH);
    r_weightMipMaps =
        ri.Cvar_Get("r_weightMipMaps", "0", CVAR_LATCH);
    r_uifullscreen = ri.Cvar_Get("r_uifullscreen", "0", CVAR_NONE);
    r_displayRefresh =
        ri.Cvar_Get("r_displayRefresh", "0", CVAR_LATCH);
    AssertCvarRange(r_displayRefresh, 0.0f, 200.0f, qtrue);

    r_fullbright =
        ri.Cvar_Get("r_fullbright", "0", CVAR_CHEAT | CVAR_LATCH);
    r_intensity =
        ri.Cvar_Get("r_intensity", "1", CVAR_CHEAT | CVAR_LATCH);
    r_singleShader =
        ri.Cvar_Get("r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH);
    r_lodbias = ri.Cvar_Get("r_lodbias", "0", CVAR_ARCHIVE);
    r_flares = ri.Cvar_Get("r_flares", "1", CVAR_CHEAT);
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): Apple Silicon avoids
     * synchronous depth readback through nonblocking occlusion queries.
     * Other hardware retains the recovered path unless explicitly enabled. */
    r_flareOcclusionQuery = ri.Cvar_Get(
        "r_flareOcclusionQuery",
        coduomp_is_apple_silicon() != qfalse ? "1" : "0",
        CVAR_ARCHIVE);
    r_znear = ri.Cvar_Get("r_znear", "4", CVAR_CHEAT);
    AssertCvarRange(r_znear, 0.0010000000474974513f, 200.0f, qtrue);
    r_zfar = ri.Cvar_Get("r_zfar", "0", CVAR_CHEAT);
    r_znear_depthhack =
        ri.Cvar_Get("r_znear_depthhack", "0.1", CVAR_CHEAT);
    AssertCvarRange(r_znear_depthhack, 0.0010000000474974513f,
                    100.0f, qtrue);
    r_ignoreGLErrors =
        ri.Cvar_Get("r_ignoreGLErrors", "1", CVAR_ARCHIVE);
    r_fastsky = ri.Cvar_Get("r_fastsky", "0", CVAR_CHEAT);
    r_inGameVideo = ri.Cvar_Get("r_inGameVideo", "1", CVAR_ARCHIVE);
    r_drawSun = ri.Cvar_Get("r_drawSun", "1", CVAR_ARCHIVE);
    r_dynamiclight =
        ri.Cvar_Get("r_dynamiclight", "1", CVAR_ARCHIVE);
    r_dlightQuality =
        ri.Cvar_Get("r_dlightQuality", "1", CVAR_ARCHIVE);
    r_finish = ri.Cvar_Get("r_finish", "0", CVAR_ARCHIVE);
    r_textureMode = ri.Cvar_Get(
        "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
    r_swapDelay = ri.Cvar_Get("r_swapDelay", "0", CVAR_ARCHIVE);
    r_swapInterval =
        ri.Cvar_Get("r_swapInterval", "0", CVAR_ARCHIVE);
    r_gamma = ri.Cvar_Get("r_gamma", "1.0", CVAR_ARCHIVE);
    r_railWidth = ri.Cvar_Get("r_railWidth", "16", CVAR_ARCHIVE);
    r_railCoreWidth =
        ri.Cvar_Get("r_railCoreWidth", "1", CVAR_ARCHIVE);
    r_railSegmentLength =
        ri.Cvar_Get("r_railSegmentLength", "32", CVAR_ARCHIVE);
    r_primitives = ri.Cvar_Get("r_primitives", "0", CVAR_ARCHIVE);
    r_showImages = ri.Cvar_Get("r_showImages", "0", CVAR_TEMP);
    r_debugSort = ri.Cvar_Get("r_debugSort", "0", CVAR_CHEAT);
    r_printShaders = ri.Cvar_Get("r_printShaders", "0", CVAR_NONE);
    r_saveFontData = ri.Cvar_Get("r_saveFontData", "0", CVAR_NONE);
    r_LightScale = ri.Cvar_Get("r_LightScale", "1", CVAR_CHEAT);
    r_showLeafLights =
        ri.Cvar_Get("r_showLeafLights", "0", CVAR_CHEAT);
    r_debugEntLight =
        ri.Cvar_Get("r_debugEntLight", "0", CVAR_CHEAT);
    r_maxEntLights = ri.Cvar_Get("r_maxEntLights", "8", CVAR_ARCHIVE);
    r_minEntLightIntensity = ri.Cvar_Get(
        "r_minEntLightIntensity", "0.02", CVAR_ARCHIVE | CVAR_CHEAT);
    r_entLightCutoff =
        ri.Cvar_Get("r_entLightCutoff", "0.2", CVAR_ARCHIVE);
    r_entFullbright =
        ri.Cvar_Get("r_entFullbright", "0", CVAR_CHEAT);
    r_entMinLight = ri.Cvar_Get("r_entMinLight", ".15", CVAR_CHEAT);
    r_diffuseSunSteps =
        ri.Cvar_Get("r_diffuseSunSteps", "3", CVAR_ARCHIVE);
    r_diffuseSunQuality =
        ri.Cvar_Get("r_diffuseSunQuality", "2", CVAR_ARCHIVE);
    r_vc_makelog = ri.Cvar_Get("r_vc_makelog", "0", CVAR_LATCH);
    r_vc_showlog = ri.Cvar_Get("r_vc_showlog", "0", CVAR_NONE);
    r_vc_compile = ri.Cvar_Get("r_vc_compile", "0", CVAR_LATCH);

    r_fog = ri.Cvar_Get("r_fog", "1", CVAR_CHEAT);
    r_drawworld = ri.Cvar_Get("r_drawworld", "1", CVAR_CHEAT);
    r_lightmap = ri.Cvar_Get("r_lightmap", "0", CVAR_CHEAT);
    r_graymap =
        ri.Cvar_Get("r_graymap", "0", CVAR_CHEAT | CVAR_LATCH);
    r_portalOnly = ri.Cvar_Get("r_portalOnly", "0", CVAR_CHEAT);
    r_flareSize = ri.Cvar_Get("r_flareSize", "96", CVAR_CHEAT);
    r_flareFadeIn = ri.Cvar_Get("r_flareFadeIn", ".2", CVAR_CHEAT);
    r_flareFadeOut =
        ri.Cvar_Get("r_flareFadeOut", ".2", CVAR_CHEAT);
    r_skipBackEnd = ri.Cvar_Get("r_skipBackEnd", "0", CVAR_CHEAT);
    r_measureOverdraw =
        ri.Cvar_Get("r_measureOverdraw", "0", CVAR_CHEAT);
    r_lodscale = ri.Cvar_Get("r_lodscale", "1", CVAR_ARCHIVE);
    r_norefresh = ri.Cvar_Get("r_norefresh", "0", CVAR_CHEAT);
    r_drawentities = ri.Cvar_Get("r_drawentities", "1", CVAR_CHEAT);
    r_drawBModels = ri.Cvar_Get("r_drawBModels", "1", CVAR_CHEAT);
    r_drawSModels = ri.Cvar_Get("r_drawSModels", "1", CVAR_CHEAT);
    r_drawXModels = ri.Cvar_Get("r_drawXModels", "1", CVAR_CHEAT);
    r_ignore = ri.Cvar_Get("r_ignore", "1", CVAR_NONE);
    r_nocull = ri.Cvar_Get("r_nocull", "0", CVAR_CHEAT);
    outsideMapEnts = ri.Cvar_Get("outsideMapEnts", "0", CVAR_CHEAT);
    r_speeds = ri.Cvar_Get("r_speeds", "0", CVAR_CHEAT);
    r_verbose = ri.Cvar_Get("r_verbose", "0", CVAR_NONE);
    r_logFile = ri.Cvar_Get("r_logFile", "0", CVAR_NONE);
    r_debugGLErrors =
        ri.Cvar_Get("r_debugGLErrors", "0", CVAR_NONE);
    r_profileDrawElements =
        ri.Cvar_Get("r_profileDrawElements", "0", CVAR_CHEAT);
    r_nobind = ri.Cvar_Get("r_nobind", "0", CVAR_CHEAT);
    r_showtris = ri.Cvar_Get("r_showtris", "0", CVAR_CHEAT);
    r_showtricounts =
        ri.Cvar_Get("r_showtricounts", "0", CVAR_CHEAT);
    r_showsurfcounts =
        ri.Cvar_Get("r_showsurfcounts", "0", CVAR_CHEAT);
    r_showsky = ri.Cvar_Get("r_showsky", "0", CVAR_CHEAT);
    r_shownormals = ri.Cvar_Get("r_shownormals", "", CVAR_CHEAT);
    r_clear = ri.Cvar_Get("r_clear", "0", CVAR_CHEAT);
    r_polygonOffsetFactor =
        ri.Cvar_Get("r_offsetfactor", "-1", CVAR_CHEAT);
    r_polygonOffsetUnits =
        ri.Cvar_Get("r_offsetunits", "-2", CVAR_CHEAT);
    r_drawBuffer = ri.Cvar_Get("r_drawBuffer", "GL_BACK", CVAR_CHEAT);
    r_lockpvs = ri.Cvar_Get("r_lockpvs", "0", CVAR_CHEAT);
    r_noportals = ri.Cvar_Get("r_noportals", "0", CVAR_CHEAT);
    cg_shadows = ri.Cvar_Get(
        "cg_shadows", "0", CVAR_ARCHIVE | CVAR_CHEAT);
    cg_skybox = ri.Cvar_Get("cg_skybox", "1", CVAR_NONE);
    r_maxpolys = ri.Cvar_Get(
        "r_maxpolys", va("%d", R_DEFAULT_MAX_POLYS), CVAR_NONE);
    r_maxpolyverts = ri.Cvar_Get(
        "r_maxpolyverts", va("%d", R_DEFAULT_MAX_POLYVERTS), CVAR_NONE);
    r_showportals = ri.Cvar_Get("r_showportals", "0", CVAR_CHEAT);
    r_showaabbtrees =
        ri.Cvar_Get("r_showaabbtrees", "0", CVAR_CHEAT);
    r_cullBModels = ri.Cvar_Get("r_cullBModels", "1", CVAR_NONE);
    r_showCullBModels =
        ri.Cvar_Get("r_showCullBModels", "0", CVAR_CHEAT);
    r_showCullSModels =
        ri.Cvar_Get("r_showCullSModels", "0", CVAR_CHEAT);
    r_cullXModels = ri.Cvar_Get("r_cullXModels", "1", CVAR_NONE);
    r_showCullXModels =
        ri.Cvar_Get("r_showCullXModels", "0", CVAR_CHEAT);
    r_singlecell = ri.Cvar_Get("r_singlecell", "0", CVAR_CHEAT);
    r_portalbevels =
        ri.Cvar_Get("r_portalbevels", "0.7", CVAR_ARCHIVE);
    r_xdebug = ri.Cvar_Get("r_xdebug", "", CVAR_CHEAT);
    r_errorOnConflicts =
        ri.Cvar_Get("r_errorOnConflicts", "1", CVAR_NONE);
    r_highLodDist = ri.Cvar_Get("r_highLodDist", "-1", CVAR_CHEAT);
    r_mediumLodDist =
        ri.Cvar_Get("r_mediumLodDist", "0", CVAR_CHEAT);
    r_lowLodDist = ri.Cvar_Get("r_lowLodDist", "0", CVAR_CHEAT);
    r_lodViewDist = ri.Cvar_Get("r_lodViewDist", "0", CVAR_CHEAT);
    r_suntest = ri.Cvar_Get("r_suntest", "0", CVAR_CHEAT);
    r_sunsprite_shader =
        ri.Cvar_Get("r_sunsprite_shader", "sun", CVAR_NONE);
    r_sunsprite_size =
        ri.Cvar_Get("r_sunsprite_size", "16", CVAR_NONE);
    r_sunflare_shader =
        ri.Cvar_Get("r_sunflare_shader", "sunFlareShader", CVAR_NONE);
    r_sunflare_min_size =
        ri.Cvar_Get("r_sunflare_min_size", "0", CVAR_NONE);
    r_sunflare_min_angle =
        ri.Cvar_Get("r_sunflare_min_angle", "45", CVAR_NONE);
    r_sunflare_max_size =
        ri.Cvar_Get("r_sunflare_max_size", "2500", CVAR_NONE);
    r_sunflare_max_angle =
        ri.Cvar_Get("r_sunflare_max_angle", "2", CVAR_NONE);
    r_sunflare_max_alpha =
        ri.Cvar_Get("r_sunflare_max_alpha", "1", CVAR_NONE);
    r_sunflare_fadein =
        ri.Cvar_Get("r_sunflare_fadein", "1", CVAR_NONE);
    r_sunflare_fadeout =
        ri.Cvar_Get("r_sunflare_fadeout", "1", CVAR_NONE);
    r_sunblind_min_angle =
        ri.Cvar_Get("r_sunblind_min_angle", "30", CVAR_NONE);
    r_sunblind_max_angle =
        ri.Cvar_Get("r_sunblind_max_angle", "5", CVAR_NONE);
    r_sunblind_max_darken =
        ri.Cvar_Get("r_sunblind_max_darken", ".75", CVAR_NONE);
    r_sunblind_fadein =
        ri.Cvar_Get("r_sunblind_fadein", ".5", CVAR_NONE);
    r_sunblind_fadeout =
        ri.Cvar_Get("r_sunblind_fadeout", "3", CVAR_NONE);
    r_sunglare_min_angle =
        ri.Cvar_Get("r_sunglare_min_angle", "30", CVAR_NONE);
    r_sunglare_max_angle =
        ri.Cvar_Get("r_sunglare_max_angle", "5", CVAR_NONE);
    r_sunglare_max_lighten =
        ri.Cvar_Get("r_sunglare_max_lighten", ".75", CVAR_NONE);
    r_sunglare_fadein =
        ri.Cvar_Get("r_sunglare_fadein", ".5", CVAR_NONE);
    r_sunglare_fadeout =
        ri.Cvar_Get("r_sunglare_fadeout", "3", CVAR_NONE);

    r_optimize = ri.Cvar_Get("r_optimize", "1", CVAR_LATCH);
    r_optimizeBackend =
        ri.Cvar_Get("r_optimizeBackend", "1", CVAR_LATCH);
    r_optimizeSModels =
        ri.Cvar_Get("r_optimizeSModels", "1", CVAR_LATCH);
    r_optimizeXModels =
        ri.Cvar_Get("r_optimizeXModels", "100", CVAR_LATCH);
    r_optimizeWorld =
        ri.Cvar_Get("r_optimizeWorld", "1", CVAR_LATCH);
    r_optimizeTextures =
        ri.Cvar_Get("r_optimizeTextures", "2", CVAR_LATCH);
    r_debugOptTex = ri.Cvar_Get("r_debugOptTex", "0", CVAR_NONE);
    r_debugOptModel =
        ri.Cvar_Get("r_debugOptModel", "0", CVAR_NONE);
    r_mem_manual = ri.Cvar_Get(
        "r_mem_manual", "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_mem_agp =
        ri.Cvar_Get("r_mem_agp", "8", CVAR_ARCHIVE | CVAR_LATCH);
    r_mem_video =
        ri.Cvar_Get("r_mem_video", "2", CVAR_ARCHIVE | CVAR_LATCH);
    r_mem_backend = ri.Cvar_Get(
        "r_mem_backend", "0.5", CVAR_ARCHIVE | CVAR_LATCH);
    r_smc_enable = ri.Cvar_Get("r_smc_enable", "1", CVAR_NONE);

    ri.Cmd_AddCommand("imagelist", R_ImageList_f);
    ri.Cmd_AddCommand("shaderlist", R_ShaderList_f);
    ri.Cmd_AddCommand("modelist", R_ModeList_f);
    ri.Cmd_AddCommand("screenshot", R_ScreenShot_f);
    ri.Cmd_AddCommand("screenshotJPEG", R_ScreenShotJPEG_f);
    ri.Cmd_AddCommand("gfxinfo", GfxInfo_f);
    ri.Cmd_AddCommand("r_meminfo", R_MemInfo_f);
    ri.Cmd_AddCommand("r_vc_stats", R_VC_Stats_f);
    ri.Cmd_AddCommand("r_smc_stats", R_StaticModelCacheStats_f);
    ri.Cmd_AddCommand("r_smc_flush", R_StaticModelCacheFlush_f);
    ri.Cmd_AddCommand("r_loadsun", R_LoadSun_f);
    ri.Cmd_AddCommand("r_savesun", R_SaveSun_f);
    ri.Cmd_AddCommand("r_sunhelp", R_SunHelp_f);
    ri.Cmd_AddCommand("r_vbo_refresh", R_VboRefresh_f);
}
