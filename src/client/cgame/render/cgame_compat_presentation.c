#include "../client_recovered.h"
#include "compat/coduo_native_x87.h"
#include "../globals.h"

#include <string.h>

/* This translation unit is an isolated improved presentation interface. */

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: module-side mirror for the engine
 * presentation cvar.  It is intentionally outside the recovered 184-entry
 * retail cvar table. */
static vmCvar_t cgameCompatAspectMode;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: opt-out for the server/mod HUD
 * element edge snap.  Zero pins every hudElem to the centered 640 canvas,
 * which renders exactly like the letterboxed 4:3 composition. */
static vmCvar_t cgameCompatWideHudElems;

enum {
    CG_COMPAT_FPS_DISPLAY_STOCK = 0,
    CG_COMPAT_FPS_DISPLAY_SIMPLE = 1
};

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: selects the recovered stock FPS
 * presentation or the native compact readout without changing cg_drawFPS's
 * original enable/detail-level semantics. */
static vmCvar_t cgameCompatDrawFpsMode;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: presentation policy for HUD content
 * whose layout the widescreen anchor model cannot classify - a mod-authored
 * ui_mp/hud.menu (see cgame_compat_hud_authorship.c).  0 auto: stock-authored
 * menus keep the anchor model, a mod-authored set falls back to the stock
 * full-width stretch; 1 always stretch; 2 always centered canvas with every
 * anchor offset zero; 3 always the anchor model (legacy behavior). */
static vmCvar_t cgameCompatModHudPresentation;

enum {
    CG_COMPAT_MOD_HUD_AUTO = 0,
    CG_COMPAT_MOD_HUD_STRETCH = 1,
    CG_COMPAT_MOD_HUD_CENTERED = 2,
    CG_COMPAT_MOD_HUD_ANCHORED = 3
};

/* NOT_FROM_ORIGINAL_SOURCE: the resolved presentation for ordinary cgame 2D.
 * ANCHORED is the widescreen anchor model; STRETCHED is the stock full-width
 * transform (positions and dimensions both scale with vidWidth/640, exactly
 * the retail presentation every HUD mod was authored against); CENTERED keeps
 * the proportional centered canvas with every anchor offset zero. */
typedef enum {
    CGAME_COMPAT_HUD_PRESENTATION_ANCHORED = 0,
    CGAME_COMPAT_HUD_PRESENTATION_STRETCHED = 1,
    CGAME_COMPAT_HUD_PRESENTATION_CENTERED = 2
} cgameCompatHudPresentation_t;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: last stretch state published to the
 * engine (-1 = never published). The engine's 2D command wrappers consume it
 * through the cg_hudStretchActive cvar at cgame scope-open time. */
static int32_t cgameCompatPublishedHudStretch;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: active declared-menu translation for
 * the passive Menu_PaintAll HUD pass. It is consumed only inside Item_Paint's
 * paint phase and is zero for every menu/open-menu call. */
static float cgameCompatPassiveHudMenuOffset;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: true while the passive menu being
 * painted uses per-item split anchoring. */
static qboolean cgameCompatPassiveHudMenuSplit;

/* NOT_FROM_ORIGINAL_SOURCE: keep the native presentation additions on the
 * same explicit dllEntry lifecycle as the recovered retail module state. */
void cgame_compat_reset_presentation_state(void)
{
    memset(&cgameCompatAspectMode, 0, sizeof(cgameCompatAspectMode));
    memset(&cgameCompatWideHudElems, 0, sizeof(cgameCompatWideHudElems));
    memset(&cgameCompatDrawFpsMode, 0, sizeof(cgameCompatDrawFpsMode));
    memset(&cgameCompatModHudPresentation, 0,
           sizeof(cgameCompatModHudPresentation));
    cgameCompatPublishedHudStretch = -1;
    cgameCompatPassiveHudMenuOffset = 0.0f;
    cgameCompatPassiveHudMenuSplit = qfalse;
    cgame_compat_reset_hud_menu_authorship();
}

/* NOT_FROM_ORIGINAL_SOURCE: registers presentation controls without changing
 * the recovered retail cvar-table shape or its original objects. */
void cgame_compat_register_presentation_cvars(void)
{
    trap_Cvar_Register(&cgameCompatAspectMode, "r_aspectMode", "0",
                       CVAR_ARCHIVE | CVAR_LATCH);
    trap_Cvar_Register(&cgameCompatWideHudElems, "cg_wideHudElems", "1",
                       CVAR_ARCHIVE);
    trap_Cvar_Register(&cgameCompatDrawFpsMode, "cg_drawFPSMode", "1",
                       CVAR_ARCHIVE);
    trap_Cvar_Register(&cgameCompatModHudPresentation, "cg_modHudPresentation",
                       "0", CVAR_ARCHIVE);
}

/* NOT_FROM_ORIGINAL_SOURCE: refreshes the compatibility mirror alongside the
 * original table-driven cvar update. */
void cgame_compat_update_presentation_cvars(void)
{
    cgame_syscall(CG_CVAR_UPDATE, (intptr_t)&cgameCompatAspectMode);
    cgame_syscall(CG_CVAR_UPDATE, (intptr_t)&cgameCompatWideHudElems);
    cgame_syscall(CG_CVAR_UPDATE, (intptr_t)&cgameCompatDrawFpsMode);
    cgame_syscall(CG_CVAR_UPDATE, (intptr_t)&cgameCompatModHudPresentation);
    cgame_compat_publish_hud_presentation();
}

/* NOT_FROM_ORIGINAL_SOURCE: exposes only the compact-mode policy so the
 * recovered FPS renderer does not own or register compatibility cvar storage. */
qboolean cgame_compat_uses_simple_fps_display(void)
{
    return cgameCompatDrawFpsMode.integer == CG_COMPAT_FPS_DISPLAY_SIMPLE
               ? qtrue
               : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: shared policy gate for view-rectangle and FOV
 * compatibility paths. */
qboolean cgame_compat_uses_classic_aspect(void)
{
    return cgameCompatAspectMode.integer != 0 ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: resolve the ordinary-2D presentation from the
 * cg_modHudPresentation policy and the HUD menu authorship verdict. */
static cgameCompatHudPresentation_t cgame_compat_resolved_hud_presentation(void)
{
    switch (cgameCompatModHudPresentation.integer) {
    case CG_COMPAT_MOD_HUD_STRETCH:
        return CGAME_COMPAT_HUD_PRESENTATION_STRETCHED;
    case CG_COMPAT_MOD_HUD_CENTERED:
        return CGAME_COMPAT_HUD_PRESENTATION_CENTERED;
    case CG_COMPAT_MOD_HUD_ANCHORED:
        return CGAME_COMPAT_HUD_PRESENTATION_ANCHORED;
    default:
        return cgame_compat_hud_menus_are_mod_authored() != qfalse
                   ? CGAME_COMPAT_HUD_PRESENTATION_STRETCHED
                   : CGAME_COMPAT_HUD_PRESENTATION_ANCHORED;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: true when ordinary cgame 2D uses the stock
 * full-width stretch. The world view, its Hor+ FOV, the physical-pixel
 * crosshair, and native full-screen effects are unaffected; every
 * canvas-relative mechanism (proportional scales, centered-canvas bias,
 * anchor offsets, hudElem snap, full-canvas shader expansion, optical
 * letterbox) collapses to the recovered stock transform. */
qboolean cgame_compat_uses_stretched_hud(void)
{
    return cgame_compat_resolved_hud_presentation() ==
                   CGAME_COMPAT_HUD_PRESENTATION_STRETCHED
               ? qtrue
               : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: publish the resolved stretch state to the engine,
 * whose 2D command wrappers own the centered-canvas bias and the deferred
 * text transform. Republished only on change; the engine snapshots the cvar
 * at each cgame presentation scope open, so a mid-game policy flip changes
 * the complete next frame rather than part of the current one. */
void cgame_compat_publish_hud_presentation(void)
{
    const int32_t active = cgame_compat_uses_stretched_hud() != qfalse ? 1 : 0;

    if (active == cgameCompatPublishedHudStretch)
        return;

    cgameCompatPublishedHudStretch = active;
    cgame_syscall(CG_CVAR_SET, (intptr_t)"cg_hudStretchActive",
                  (intptr_t)(active != 0 ? "1" : "0"));
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the stock cropped-view tile fill for native
 * presentation, but leave classic side bars untouched after the engine has
 * cleared them to black. Classic world and HUD now share the same fitted 4:3
 * backend viewport, so no cgame-owned border texture is needed. */
void cgame_compat_tile_clear(void)
{
    if (cgame_compat_uses_classic_aspect() == qfalse)
        CG_TileClear();
}

/* NOT_FROM_ORIGINAL_SOURCE: selects one coherent proportional 640x480 canvas
 * for native widescreen presentation. Stock cgame draw helpers multiply image
 * coordinates by these globals before entering the renderer, so this is the
 * only point at which their horizontal and vertical scales can be made
 * consistent without reinterpreting individual draw primitives later.
 * Classic presentation and non-widescreen modes retain the recovered stock
 * scales. */
void cgame_compat_configure_screen_scales(void)
{
    const float stockXScale = (float)(
        (long double)cgs_glconfig.vidWidth * (long double)(1.0f / 640.0f));
    const float stockYScale = (float)(
        (long double)cgs_glconfig.vidHeight * (long double)(1.0f / 480.0f));

    cgs_screenYScale = stockYScale;
    if (cgame_compat_uses_classic_aspect() == qfalse &&
        cgame_compat_uses_stretched_hud() == qfalse &&
        cgs_glconfig.vidWidth > 0 && cgs_glconfig.vidHeight > 0 &&
        (int64_t)cgs_glconfig.vidWidth * 3 >
            (int64_t)cgs_glconfig.vidHeight * 4) {
        cgs_screenXScale = stockYScale;
    } else {
        cgs_screenXScale = stockXScale;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: open cgame menus mix two stock drawing interfaces:
 * pictures/fills pre-scale through cgs_screenXScale, while text remains in
 * virtual coordinates until the renderer applies its 640x480 transform. Keep
 * the picture side on the same height-derived scale for the duration of one
 * open-menu composition. This is a single canvas projection for all stock and
 * mod menus; it does not classify menu names or individual assets. */
float cgame_compat_begin_open_menu_canvas(void)
{
    const float previousXScale = cgs_screenXScale;

    if (cgame_compat_uses_classic_aspect() == qfalse &&
        cgame_compat_uses_stretched_hud() == qfalse &&
        cgs_glconfig.vidWidth > 0 && cgs_glconfig.vidHeight > 0 &&
        (int64_t)cgs_glconfig.vidWidth * 3 >
            (int64_t)cgs_glconfig.vidHeight * 4) {
        cgs_screenXScale = cgs_screenYScale;
    }

    return previousXScale;
}

/* NOT_FROM_ORIGINAL_SOURCE: close the temporary open-menu canvas projection. */
void cgame_compat_end_open_menu_canvas(float previousXScale)
{
    cgs_screenXScale = previousXScale;
}

/* NOT_FROM_ORIGINAL_SOURCE: returns the width of one native-widescreen side
 * extension in the cgame's virtual coordinate system.  The ordinary cgame
 * canvas is still exactly 640x480; this value is used only by explicit,
 * complete HUD-item adapters. */
static float cgame_compat_virtual_side_width(void)
{
    if (cgame_compat_uses_classic_aspect() != qfalse ||
        cgame_compat_uses_stretched_hud() != qfalse ||
        cgs_glconfig.vidWidth <= 0 || cgs_glconfig.vidHeight <= 0 ||
        (int64_t)cgs_glconfig.vidWidth * 3 <=
            (int64_t)cgs_glconfig.vidHeight * 4) {
        return 0.0f;
    }

    return ((float)cgs_glconfig.vidWidth /
                ((float)cgs_glconfig.vidHeight / 480.0f) -
            640.0f) * 0.5f;
}

/* NOT_FROM_ORIGINAL_SOURCE: one constant translation per composition.  A
 * composition either stays on the centered 640 canvas (CENTER, offset 0) or
 * moves as a whole to a native edge (LEFT/RIGHT, offset -/+ sideWidth).
 * A constant translation preserves every internal distance by construction;
 * a position-proportional projection cannot (it multiplies the spacing
 * between separately projected items by 1 + sideWidth/320). */
typedef enum {
    CGAME_COMPAT_HUD_ANCHOR_CENTER = 0,
    CGAME_COMPAT_HUD_ANCHOR_LEFT = 1,
    CGAME_COMPAT_HUD_ANCHOR_RIGHT = 2,
    /* The declared menu spans both corners (vehicle preview at the stance
     * position, status bars at the opposite edge); anchor each child item
     * independently by its own authored span. */
    CGAME_COMPAT_HUD_ANCHOR_SPLIT = 3
} cgameCompatHudAnchor_t;

static float cgame_compat_hud_anchor_offset(cgameCompatHudAnchor_t anchor)
{
    const float sideWidth = cgame_compat_virtual_side_width();

    if (sideWidth <= 0.0f)
        return 0.0f;
    /* The forced-centered policy keeps the centered canvas (and therefore a
     * real sideWidth for the physical bias and optics) while zeroing every
     * anchor translation, so the complete ordinary-2D layout renders exactly
     * like the letterboxed 4:3 composition. */
    if (cgame_compat_resolved_hud_presentation() ==
        CGAME_COMPAT_HUD_PRESENTATION_CENTERED) {
        return 0.0f;
    }
    if (anchor == CGAME_COMPAT_HUD_ANCHOR_LEFT)
        return -sideWidth;
    if (anchor == CGAME_COMPAT_HUD_ANCHOR_RIGHT)
        return sideWidth;
    return 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit anchors for the stock passive-HUD menu
 * compositions, keyed by declared menu name.  A menu name not in this table
 * (any mod HUD menu) keeps the centered canvas, which renders exactly like
 * the letterboxed 4:3 layout.  Layout is never inferred from item-rect
 * geometry: the declared rects are authoring containers, not visual bounds,
 * and a derived offset tears sibling compositions apart. */
enum {
    CGAME_COMPAT_MENU_NAME_COMPARE_LIMIT = 99999
};

static const struct {
    const char *menuName;
    cgameCompatHudAnchor_t anchor;
} cgameCompatHudMenuAnchors[] = {
    { "Compass",    CGAME_COMPAT_HUD_ANCHOR_LEFT },
    { "stance",     CGAME_COMPAT_HUD_ANCHOR_LEFT },
    { "weaponinfo", CGAME_COMPAT_HUD_ANCHOR_RIGHT },
    { "Health",     CGAME_COMPAT_HUD_ANCHOR_RIGHT },
    /* The vehicle menus author the rotating vehicle preview and seat status
     * at the stance position (left) and the health/limiter bars at the right
     * edge in one declared menu, so their items anchor independently. */
    { "tankstatus", CGAME_COMPAT_HUD_ANCHOR_SPLIT },
    { "jeepstatus", CGAME_COMPAT_HUD_ANCHOR_SPLIT },
};

/* NOT_FROM_ORIGINAL_SOURCE: snap one authored horizontal span to a constant
 * edge translation.  A span entirely inside the left third of the 640 canvas
 * moves with the left edge, one entirely inside the right third moves with
 * the right edge, and everything else stays on the centered canvas. */
static float cgame_compat_span_snap_offset(float left, float right)
{
    if (right <= 640.0f / 3.0f)
        return cgame_compat_hud_anchor_offset(CGAME_COMPAT_HUD_ANCHOR_LEFT);
    if (left >= 640.0f * (2.0f / 3.0f))
        return cgame_compat_hud_anchor_offset(CGAME_COMPAT_HUD_ANCHOR_RIGHT);
    return 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: begin one declared passive-HUD menu group.  The
 * menu's declared anchor supplies one shared constant translation, so
 * independently painted pieces of a compass, vehicle summary, weapon display,
 * or mod HUD cannot drift apart. Every item receives the same translation.
 * Open/pre-game/in-game menus do not use this adapter and remain a centered
 * 4:3 composition. */
float cgame_compat_begin_passive_hud_menu(menuDef_t *menu)
{
    cgameCompatPassiveHudMenuOffset = 0.0f;
    cgameCompatPassiveHudMenuSplit = qfalse;

    if (menu != NULL && menu->window.name != NULL) {
        for (size_t index = 0;
             index < sizeof(cgameCompatHudMenuAnchors) /
                         sizeof(cgameCompatHudMenuAnchors[0]);
             ++index) {
            if (Q_stricmpn(menu->window.name,
                           cgameCompatHudMenuAnchors[index].menuName,
                           CGAME_COMPAT_MENU_NAME_COMPARE_LIMIT) == 0) {
                if (cgameCompatHudMenuAnchors[index].anchor ==
                    CGAME_COMPAT_HUD_ANCHOR_SPLIT) {
                    /* The menu window itself is a container; each child item
                     * receives its own span-snapped offset at paint time. */
                    cgameCompatPassiveHudMenuSplit = qtrue;
                } else {
                    cgameCompatPassiveHudMenuOffset =
                        cgame_compat_hud_anchor_offset(
                            cgameCompatHudMenuAnchors[index].anchor);
                }
                break;
            }
        }
    }

    return cgameCompatPassiveHudMenuOffset;
}

/* NOT_FROM_ORIGINAL_SOURCE: close the declared passive-HUD menu projection. */
void cgame_compat_end_passive_hud_menu(void)
{
    cgameCompatPassiveHudMenuOffset = 0.0f;
    cgameCompatPassiveHudMenuSplit = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: begin the temporary geometry translation only
 * after Item_Paint has completed stock orbit/transition and visibility work.
 * Returning the applied offset lets the matching end adapter remove it from
 * whatever current text-layout state the paint routines produced. */
float client_ui_compat_begin_item_paint(itemDef_t *item)
{
    float offset = cgameCompatPassiveHudMenuOffset;

    if (cgameCompatPassiveHudMenuSplit != qfalse) {
        const float left = item->window.rect.x;
        const float right = item->window.rect.w >= 0.0f
                                ? left + item->window.rect.w
                                : left;

        offset = cgame_compat_span_snap_offset(left, right);
    }

    item->window.rect.x += offset;
    item->textRect.x += offset;
    return offset;
}

/* NOT_FROM_ORIGINAL_SOURCE: close the complete-item paint scope without
 * reverting animation or text-layout updates made by the stock painters. */
void client_ui_compat_end_item_paint(itemDef_t *item, float offset)
{
    item->window.rect.x -= offset;
    item->textRect.x -= offset;
}

/* NOT_FROM_ORIGINAL_SOURCE: server/mod hudElem_t content has no declared menu
 * group.  Snap the element by its authored alignX ANCHOR POINT - the fixed
 * point of hudElem size animations (a left-aligned meter fill keeps its left
 * edge, a center-aligned element its center, a right-aligned one its right
 * edge while scaleOverTime runs) - so a filling or draining bar can never
 * change presentation class mid-animation.  Classifying by the live span did
 * exactly that: the HQ radio meter fill snapped left the moment its receding
 * right edge cleared the left band.  The bands are the outer screen QUARTERS
 * (anchor <= 160 left, >= 480 right), biased center-ward so a centered meter
 * composition whose backdrop begins near x=176 stays on the centered canvas
 * with its fill.  cg_wideHudElems 0 pins every element to the centered
 * canvas. */
enum {
    CGAME_COMPAT_HUD_LEFT_BAND_MAX = 160,
    CGAME_COMPAT_HUD_RIGHT_BAND_MIN = 480
};

void cgame_compat_project_server_hud_item(
    cgAlignedDrawItem *item, const hudElem_t *elem)
{
    float anchor;

    /* An element's presentation class is a function of its authored,
     * animation-invariant geometry only - never of transient screen state or
     * animated extents.  (An earlier scoreboard-showing gate here made every
     * persistent snapped element jump when the scoreboard opened; the span
     * classifier made animated meters jump as they drained.) */
    if (item == NULL || elem == NULL ||
        cgameCompatWideHudElems.integer == 0) {
        return;
    }

    /* Full-canvas shaders are a distinct composition type. Leave their stock
     * 0..640 descriptor intact here so the shader boundary adapter below can
     * widen the complete effect. Treating x=0 as an ordinary left HUD anchor
     * would shift the 640-wide effect without expanding its right edge. */
    if (elem->type == HE_TYPE_SHADER &&
        item->x == 0.0f && item->width == 640.0f) {
        return;
    }

    /* Recover the authored anchor from the completed item: CG_HudElemX
     * subtracted width/2 or width for the HALF/FULL alignments, so adding it
     * back yields the animation-stable authored coordinate. */
    anchor = item->x;
    if (item->width >= 0.0f) {
        if (elem->alignX == HUDELEM_ALIGN_CENTER)
            anchor += item->width * 0.5f;
        else if (elem->alignX == HUDELEM_ALIGN_END)
            anchor += item->width;
    }

    if (anchor <= (float)CGAME_COMPAT_HUD_LEFT_BAND_MAX) {
        item->x += cgame_compat_hud_anchor_offset(
            CGAME_COMPAT_HUD_ANCHOR_LEFT);
    } else if (anchor >= (float)CGAME_COMPAT_HUD_RIGHT_BAND_MIN) {
        item->x += cgame_compat_hud_anchor_offset(
            CGAME_COMPAT_HUD_ANCHOR_RIGHT);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit group offset for the recovered
 * CG_DrawTeamInfo chat strip, whose image and text use different draw paths.
 * Applying this same virtual translation to both keeps the group intact. */
float cgame_compat_left_hud_virtual_offset(void)
{
    return cgame_compat_hud_anchor_offset(CGAME_COMPAT_HUD_ANCHOR_LEFT);
}

/* NOT_FROM_ORIGINAL_SOURCE: matching explicit offset for direct stock HUD
 * compositions authored right-flush outside the declared-menu system (the
 * weapon-select strip, lagometer, and diagnostics readouts).  Their recovered
 * 640-canvas coordinates stay unchanged; default widescreen presentation
 * translates each complete draw to the native right edge. */
float cgame_compat_right_hud_virtual_offset(void)
{
    return cgame_compat_hud_anchor_offset(CGAME_COMPAT_HUD_ANCHOR_RIGHT);
}

/* NOT_FROM_ORIGINAL_SOURCE: reticle and optical-overlay routines construct
 * their rectangles directly in physical refdef pixels rather than through
 * CG_AdjustFrom640.  Cancel the generic centered-cgame image bias before the
 * trap so the engine compatibility wrapper restores, rather than duplicates,
 * that bias.  This keeps the recovered coordinate arithmetic unchanged and
 * isolates the physical-pixel exception at its draw boundary. */
static float cgame_compat_physical_overlay_input_x(float x)
{
    return x - cgame_compat_virtual_side_width() * cgs_screenYScale;
}

/* NOT_FROM_ORIGINAL_SOURCE: physical-pixel stretch-pic submission for weapon
 * reticles and full-screen optical overlays. */
void cgame_compat_draw_physical_stretch_pic(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2, int32_t shaderHandle)
{
    trap_R_DrawStretchPic(
        CG_FloatBits(cgame_compat_physical_overlay_input_x(x)),
        CG_FloatBits(y), CG_FloatBits(width), CG_FloatBits(height),
        CG_FloatBits(s1), CG_FloatBits(t1),
        CG_FloatBits(s2), CG_FloatBits(t2), shaderHandle);
}

/* NOT_FROM_ORIGINAL_SOURCE: fill a complete gameplay effect over the native
 * drawable. Damage, explosion, kill/fade, and similar screen effects are not
 * ordinary 640-canvas HUD content and must cover the widescreen extensions. */
void cgame_compat_fill_native_screen_effect(const float color[4])
{
    trap_R_SetColor(color);
    cgame_compat_draw_physical_stretch_pic(
        0.0f, 0.0f,
        (float)cgs_glconfig.vidWidth, (float)cgs_glconfig.vidHeight,
        0.0f, 0.0f, 0.0f, 1.0f, cgs_media_whiteShader);
    trap_R_SetColor(NULL);
}

/* NOT_FROM_ORIGINAL_SOURCE: expand a complete server-authored shader object
 * only when that object explicitly spans the entire stock 640-wide canvas.
 * This object-boundary rule supports mod killcam and screen-effect shaders
 * without classifying or moving their individual draw primitives. */
void cgame_compat_expand_native_server_hud_shader(
    const cgAlignedDrawItem *item, const hudElem_t *elem,
    float *drawX, float *drawWidth)
{
    const float sideWidth = cgame_compat_virtual_side_width();

    /* No scoreboard-showing gate here either: a full-canvas shader must not
     * change width when the stock scoreboard opens (same transient-state rule
     * as the span snap above). */
    if (item == NULL || elem == NULL || drawX == NULL || drawWidth == NULL ||
        elem->type != HE_TYPE_SHADER || sideWidth <= 0.0f ||
        *drawX != 0.0f || *drawWidth != 640.0f) {
        return;
    }

    *drawX = -sideWidth;
    *drawWidth += sideWidth * 2.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: cover the native-widescreen side extensions with
 * opaque optical-mask bars. The stock scope composition remains inside the
 * centered 4:3 canvas rather than stretching its edge texels over the wider
 * world view. Alpha follows the recovered ADS-overlay transition. */
void cgame_compat_draw_optical_letterbox(float alpha)
{
    const float sideWidth =
        cgame_compat_virtual_side_width() * cgs_screenYScale;
    vec4_t black = { 0.0f, 0.0f, 0.0f, alpha };
    vec4_t white = { 1.0f, 1.0f, 1.0f, alpha };

    if (sideWidth <= 0.0f)
        return;

    trap_R_SetColor(black);
    cgame_compat_draw_physical_stretch_pic(
        0.0f, 0.0f, sideWidth, (float)cgs_glconfig.vidHeight,
        0.0f, 0.0f, 0.0f, 0.0f, cgs_media_whiteShader);
    cgame_compat_draw_physical_stretch_pic(
        (float)cgs_glconfig.vidWidth - sideWidth, 0.0f,
        sideWidth, (float)cgs_glconfig.vidHeight,
        0.0f, 0.0f, 0.0f, 0.0f, cgs_media_whiteShader);
    trap_R_SetColor(white);
}

/* NOT_FROM_ORIGINAL_SOURCE: return the physical right edge of the fitted
 * optical canvas. The recovered scope mask uses refdef.width as its stock
 * endpoint even though its center includes refdef.x; when the native view is
 * inset, that leaves a refdef.x-wide live-world gap on the right. The optical
 * composition owns the complete fitted canvas, so its improved endpoint is
 * the canvas edge. Narrow and classic presentations retain the recovered
 * endpoint. */
long double cgame_compat_optical_canvas_right(long double stockViewRight)
{
    const float sideWidth =
        cgame_compat_virtual_side_width() * cgs_screenYScale;

    if (sideWidth <= 0.0f)
        return stockViewRight;

    return (long double)cgs_glconfig.vidWidth - (long double)sideWidth;
}

/* NOT_FROM_ORIGINAL_SOURCE: clip one stock optical-overlay primitive to the
 * centered 4:3 canvas. The stock center, scale, draw order, and degenerate
 * edge-mask texture coordinates are preserved; only pixels in the native
 * side extensions are discarded. */
void cgame_compat_draw_letterboxed_optical_pic(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2, int32_t shaderHandle)
{
    const float sideWidth =
        cgame_compat_virtual_side_width() * cgs_screenYScale;
    const float safeLeft = sideWidth;
    const float safeRight = (float)cgs_glconfig.vidWidth - sideWidth;
    const float originalLeft = x;
    const float originalRight = x + width;
    const float originalS1 = s1;
    const float originalS2 = s2;
    float clippedLeft;
    float clippedRight;

    if (sideWidth <= 0.0f) {
        cgame_compat_draw_physical_stretch_pic(
            x, y, width, height, s1, t1, s2, t2, shaderHandle);
        return;
    }

    clippedLeft = originalLeft < safeLeft ? safeLeft : originalLeft;
    clippedRight = originalRight > safeRight ? safeRight : originalRight;
    if (clippedRight <= clippedLeft || width <= 0.0f)
        return;

    s1 = originalS1 + (originalS2 - originalS1) *
                           ((clippedLeft - originalLeft) / width);
    s2 = originalS1 + (originalS2 - originalS1) *
                           ((clippedRight - originalLeft) / width);
    cgame_compat_draw_physical_stretch_pic(
        clippedLeft, y, clippedRight - clippedLeft, height,
        s1, t1, s2, t2, shaderHandle);
}

/* NOT_FROM_ORIGINAL_SOURCE: physical-pixel rotated-quad submission for the
 * four ordinary crosshair side pieces. */
void cgame_compat_draw_physical_quad_pic(
    float x, float y, float width, float height,
    float s1, float t1, float s2, float t2,
    float angleDegrees, int32_t shaderHandle)
{
    trap_R_DrawQuadPic(
        CG_FloatBits(cgame_compat_physical_overlay_input_x(x)),
        CG_FloatBits(y), CG_FloatBits(width), CG_FloatBits(height),
        CG_FloatBits(s1), CG_FloatBits(t1),
        CG_FloatBits(s2), CG_FloatBits(t2),
        CG_FloatBits(angleDegrees), shaderHandle);
}

/* NOT_FROM_ORIGINAL_SOURCE: converts a 4:3-authored horizontal FOV to Hor+
 * while preserving its vertical FOV.  Narrow and classic presentations keep
 * the authored angle unchanged. */
/* NOT_FROM_ORIGINAL_SOURCE: the 4:3-equivalent horizontal FOV for the
 * current view - the exact inverse of cgame_compat_expand_horizontal_fov.
 * The recovered crosshair spread converts degrees to virtual pixels with
 * 640/fov_x, an expression authored against the 640-wide 4:3 canvas; feeding
 * it the Hor+-expanded angle narrows the horizontal spread by the expansion
 * factor and the reticle cross stops being square.  Narrow and classic
 * presentations return the live angle unchanged. */
long double cgame_compat_spread_fov_x(void)
{
    const long double classicAspect = 4.0L / 3.0L;
    long double aspect;
    long double halfAngle;
    long double tangent;

    if (cgame_compat_uses_classic_aspect() != qfalse ||
        cg_refdef.width <= 0 || cg_refdef.height <= 0) {
        return cg_refdef.fov_x;
    }

    aspect = (long double)cg_refdef.width / (long double)cg_refdef.height;
    if (aspect <= classicAspect)
        return cg_refdef.fov_x;

    halfAngle = (long double)cg_refdef.fov_x * (long double)DEG_TO_HALF_RAD;
    tangent = coduo_x87_tanl(halfAngle);
    return coduo_x87_atan2l(
               tangent * classicAspect / aspect, 1.0L) *
           (long double)HALF_RAD_TO_DEG;
}

long double cgame_compat_expand_horizontal_fov(long double baseFov,
                                               int32_t width,
                                               int32_t height)
{
    const long double classicAspect = 4.0L / 3.0L;
    long double aspect;
    long double halfAngle;
    long double tangent;

    if (cgame_compat_uses_classic_aspect() != qfalse ||
        width <= 0 || height <= 0) {
        return baseFov;
    }

    aspect = (long double)width / (long double)height;
    if (aspect <= classicAspect)
        return baseFov;

    halfAngle = baseFov * (long double)DEG_TO_HALF_RAD;
    tangent = coduo_x87_tanl(halfAngle);
    return coduo_x87_atan2l(
               tangent * aspect / classicAspect, 1.0L) *
           (long double)HALF_RAD_TO_DEG;
}
