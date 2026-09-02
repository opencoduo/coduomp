// Source: uo_cgame_mp_x86.dll 0x30019cf0..0x3001a4c6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30019cf0_3001a4c6.mcode
//
// CG_DrawCrosshair — per-frame crosshair/reticle renderer for the local player.
// (The .mcode size-guess name "VEH_PlayerCollision" was a server-side size
// collision and is rejected.) In view order it: draws the 3D weapon icon
// instead when the scope/zoom flag pair is up (unless following nobody); draws
// the two hard-coded vehicle sights for vehicle positions 1 and 2; tints the
// reticle green when the cursor hint is the value-10 hint (aiming at a
// teammate); computes the ordinary-crosshair alpha from
// CG_DrawWeapReticle() * cg_crosshairAlpha and bails when invisible, disabled,
// paused, at full ADS with the gun model drawn, or in a melee/reload/raise/drop
// weapon state; while the ADS transition is in progress it derives the shrink
// fraction from the weapon's adsCrosshairIn/OutFrac, shrinks the reticle
// toward half size at full zoom, computes a vertical offset from the weapon's
// adsPitchOffset (degrees -> virtual pixels via 480/fovY), draws a grown
// (1.5 - sizeScale) center reticle at the projected impact point when the gun
// model is hidden (at full ADS that overlay is the ONLY reticle drawn;
// mid-transition frames draw it plus the shrunk, offset ordinary reticle);
// then draws the center reticle (with the grenade cook-off pulse
// growing it and playing a per-second pulse sound) and four side reticle
// pieces spread apart by the stance-blended weapon spread, each submitted as a
// 0/90-degree rotated quad through trap_R_DrawQuadPic with per-piece texture
// flips.
//
// The previous C artifact for this range was a plausible rewrite (fabricated
// axis-aligned side reticle, missing ADS overlay / green tint / full-ADS gate,
// grenade pulse hoisted out of its reticleCenter guard); this file re-derives
// every block from the instruction stream with full FPU-stack tracking. All
// .rdata constants dumped byte-exact from the DLL:
//   0x3007bce0=1.0f  0x3007bce8=0.5f   0x3007bcec=0.0f  0x3007bdb4=0.01f
//   0x3007be24=1/255 0x3007be70=1.5f   0x3007be8c=90.0f 0x3007bf34=640.0f
//   0x3007c148=480.0f

#include "../client_recovered.h"

enum cgCrosshairVehiclePosition_e {
    CG_CROSSHAIR_VEHICLE_POSITION_LARGE_SIGHT = 1,
    CG_CROSSHAIR_VEHICLE_POSITION_SMALL_SIGHT = 2,
    CG_CROSSHAIR_VEHICLE_POSITION_WEAPON_RETICLE = 3
};

/* NOT_FROM_ORIGINAL_SOURCE: the draw sites below construct physical
 * refdef-pixel geometry.  The isolated adapter prevents the generic centered
 * 640-canvas image bias from being applied a second time. */
#define DRAW_CROSSHAIR_STRETCH_PIC(x_, y_, w_, h_, s1_, t1_, s2_, t2_, shader_) \
    cgame_compat_draw_physical_stretch_pic((x_), (y_), (w_), (h_), (s1_), (t1_), (s2_), (t2_), (shader_))
#define DRAW_CROSSHAIR_QUAD_PIC(x_, y_, w_, h_, s1_, t1_, s2_, t2_, angle_, shader_) \
    cgame_compat_draw_physical_quad_pic((x_), (y_), (w_), (h_), (s1_), (t1_), (s2_), (t2_), (angle_), (shader_))

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): reticleCenterSize and
 * reticleSideSize are authored in stock-era 640x480 pixels, and this function
 * consumes them as raw physical pixels, so on a high-resolution drawable the
 * ordinary crosshair shrinks toward invisibility (the stock ADS overlay and
 * 3D weapon icon already multiply their sizes by the screen scale; this
 * drawer is the one stock omission).  Scale each authored size ONCE, where it
 * enters, so every downstream anchor, spread, nudge, and hip-position
 * computation reproduces the recovered 640x480 composition proportionally.
 * Rectangles must never be re-expanded at the draw boundary: the arm pieces
 * are anchored, not centered, and post-hoc expansion collapses the arm gaps. */
/* Per-axis factors: in classic 4:3 presentation the backend compresses 2D x
 * by the fitted-viewport ratio and cgame pre-compensates through
 * cgs_screenXScale (exactly as the stock spread conversion does), so width
 * terms must scale by the X factor and height terms by the Y factor.  In
 * native widescreen the two factors are equal. */
/* Half the 640x480 screen fraction: matches the raw-pixel reticle feel of
 * the era's common 1280x960 presentation, validated in play twice (via the
 * since-removed cg_crosshairScale calibration knob at its 0.5 default). */
#define CG_CROSSHAIR_PROPORTION 0.5f

static float cg_crosshair_reticle_scale_x(void)
{
    const float screenScale = cgs_screenXScale > 0.0f ? cgs_screenXScale : 1.0f;
    const float scale = screenScale * CG_CROSSHAIR_PROPORTION;

    /* Never below the stock raw-pixel size. */
    return scale > 1.0f ? scale : 1.0f;
}

static float cg_crosshair_reticle_scale_y(void)
{
    const float screenScale = cgs_screenYScale > 0.0f ? cgs_screenYScale : 1.0f;
    const float scale = screenScale * CG_CROSSHAIR_PROPORTION;

    return scale > 1.0f ? scale : 1.0f;
}

void CG_DrawCrosshair(void)
{
    weaponInfo_t *weapon;
    uint32_t entityStateFlags;
    qboolean zoomScopeView;
    qboolean thirdPerson;
    int32_t weaponIndex;
    int32_t weaponState;
    // 0x30019d05..0x30019d1d: color = {1,1,1,0}; alpha rewritten below.
    vec4_t color;
    // 0x30019f13/0x30019f17: two adjacent stack floats handed to
    // CG_ProjectDamageDirToScreen (EDI=&projected[0], ESI=&projected[1]).
    vec2_t projected;
    // 0x30019cf6/0x30019cfb: ps.adsFraction is copied to a frame slot once and
    // every later read uses the copy.
    float adsFraction;
    float sizeScale;
    float adsOffset;

    adsFraction = cg_predictedPlayerState.adsFraction;
    thirdPerson = cg_thirdPerson;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 0.0f;
    sizeScale = 1.0f;
    adsOffset = 0.0f;

    // 0x30019cfe/0x30019d03/0x30019d35: no crosshair in third person.
    if (thirdPerson) {
        return;
    }

    entityStateFlags = cg_predictedPlayerState.entityStateFlags;
    zoomScopeView = (entityStateFlags & EF_ZOOM_FOV_MASK) != 0;
    // 0x30019d43/0x30019d49: color[3] starts as the raw cvar value.
    color[3] = cg_crosshairAlpha_vmCvar.value;

    // 0x30019d40 TEST AH,0x60 — entityStateFlags & 0x6000 (zoom/scope pair).
    if (zoomScopeView) {
        // 0x30019d4f: CMP viewLockedEntityNum,0x3ff (1023).
        if (cg_predictedPlayerState.viewLockedEntityNum == ENTITYNUM_NONE) {
            return;
        }
        CG_DrawWeaponIcon3D();  // 0x30019d5f CALL 0x30019ba0
        return;
    }

    // 0x30019d6b TEST 0x100000 / 0x30019d76 TEST 0x400000.
    if ((entityStateFlags & EF_IN_VEHICLE) != 0 && (entityStateFlags & EF_VEHICLE_ALLOW_WEAPON) == 0) {
        int32_t vehicleType = cg_predictedPlayerState.vehicleType;
        qboolean isTypeOne = vehicleType == VEHICLE_TYPE_4_WHEEL;
        int32_t vehiclePosition = cg_predictedPlayerState.vehiclePosition;

        // 0x30019d81..0x30019d94: vehicle type 1 in position 3 uses the
        // ordinary weapon reticle below.
        if (isTypeOne && vehiclePosition == CG_CROSSHAIR_VEHICLE_POSITION_WEAPON_RETICLE) {
            /* fall through to the weapon reticle */
        } else {
            qboolean isPositionOne = vehiclePosition == CG_CROSSHAIR_VEHICLE_POSITION_LARGE_SIGHT;

            // 0x30019d9d..0x30019db5: fixed black 60% sight color (stored on
            // every remaining vehicle path, drawn or not, as the machine does).
            color[2] = 0.0f;
            color[1] = 0.0f;
            color[0] = 0.0f;
            color[3] = 0.6f;  // 0x3f19999a
            // 0x30019d9a CMP vehiclePosition,1
            if (isPositionOne) {
                // 0x30019dbf/0x30019dc1: type-1 vehicles draw nothing here.
                if (vehicleType == vehiclePosition) {
                    return;
                }
                // 0x30019dc7..0x30019dfe: horizontal bar + vertical stem.
                CG_FillRect(300.0f, 240.0f, 40.0f, 2.0f, color);
                CG_FillRect(319.0f, 242.0f, 2.0f, 16.0f, color);
                return;
            }
            // 0x30019e0d CMP vehiclePosition,2 (anything else returns bare).
            if (vehiclePosition == CG_CROSSHAIR_VEHICLE_POSITION_SMALL_SIGHT) {
                // 0x30019e16..0x30019e4d: smaller sight.
                CG_FillRect(310.0f, 240.0f, 20.0f, 2.0f, color);
                CG_FillRect(319.0f, 242.0f, 2.0f, 8.0f, color);
            }
            return;
        }
    }

    // 0x30019e5c..0x30019e6d: the friendly-player cursor hint (value 10, the
    // "hintFriendly" hint kind) tints the reticle green {0.25, 1.0, 0.25}.
    if (cg_predictedPlayerState.serverCursorHint == CURSOR_HINT_FRIENDLY) {
        color[2] = 0.25f;  // 0x3e800000
        color[0] = 0.25f;
    }

    // 0x30019e75/0x30019e7b: the pointer is dereferenced with no NULL guard.
    weapon = cg_currentWeaponInfo;
    weaponIndex = weapon->weaponIndex;
    if (weaponIndex == 0) {  // 0x30019e7d/0x30019e7f
        return;
    }

    // 0x30019e85..0x30019e90: alpha = reticle fade * cvar, stored to color[3]
    // even on the early-out paths below (FST keeps it for the compare).
    long double alphaRaw = CG_DrawWeapReticle() * (long double)cg_crosshairAlpha_vmCvar.value;
    color[3] = (float)alphaRaw;
    // 0x30019e94..0x30019e9f: FCOMP vs 0.01f (0x3007bdb4), JNP on less.
    if (alphaRaw < (long double)0.01f) {
        return;
    }
    if (cg_drawCrosshair_vmCvar.integer == 0) {  // 0x30019ea5
        return;
    }
    if (cl_paused_vmCvar.integer != 0) {  // 0x30019eb2
        return;
    }

    // 0x30019ebf..0x30019ed9: at full ADS (adsFraction == 1.0) the crosshair
    // is suppressed entirely unless the gun model is hidden (cg_drawGun 0).
    if (adsFraction == 1.0f && cg_drawGun_vmCvar.integer != 0) {
        return;
    }

    // 0x30019edf..0x30019f0b: no crosshair in these weapon states (compared
    // in machine order 10, 11, 5, 1, 2).
    weaponState = cg_predictedPlayerState.weaponState;
    if (weaponState == WEAPON_STATE_MELEE_WINDUP || weaponState == WEAPON_STATE_MELEE_RELAX || weaponState == WEAPON_STATE_RELOADING ||
        weaponState == WEAPON_STATE_RAISING || weaponState == WEAPON_STATE_DROPPING) {
        return;
    }

    // 0x30019f13..0x30019f1b (EDI/ESI register args).
    CG_ProjectDamageDirToScreen(&projected[0], &projected[1]);
    // 0x30019f20..0x30019f27: the draw color is installed HERE, before the
    // ADS and grenade blocks.
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);

    /* 0x30019f37 reloads the global weapon pointer after both draw calls and
     * before the ADS-zero comparison. */
    weapon = cg_currentWeaponInfo;
    // 0x30019f2d..0x30019f47: ADS transition math only while adsFraction != 0.
    if (adsFraction != 0.0f) {
        // 0x30019f4d..0x30019f92: the machine specializes both branches of
        // the cg_adsZoomingIn test with identical code over adsCrosshairInFrac
        // (+0x3cc) vs adsCrosshairOutFrac (+0x3d0).
        float crosshairFrac = cg_adsZoomingIn ? weapon->adsCrosshairInFrac : weapon->adsCrosshairOutFrac;
        long double frac = (long double)adsFraction - (1.0L - (long double)crosshairFrac);

        // 0x30019f66/0x30019f8b FCOM 0.0; TEST AH,0x41 — proceed only when > 0.
        if (frac > 0.0f) {
            frac /= (long double)crosshairFrac;
            // 0x30019f98..0x30019fa3: re-guarded after the divide.
            if (frac > 0.0f) {
                // 0x30019fa5..0x30019fb3: sizeScale = 1 - 0.5*frac.
                sizeScale = (float)(1.0L - 0.5f * frac);
                // 0x30019fb7..0x30019fcb: degrees -> virtual pixels via
                // 480/fovY, scaled by the weapon's ADS pitch offset and frac.
                adsOffset = (float)(((long double)480.0f / (long double)cg_refdef.fov_y) * (long double)weapon->adsPitchOffset * frac);
            }
        }

        // 0x30019fcf..0x30019fd8: with the gun model drawn, skip the overlay
        // AND the full-ADS suppression below (the ordinary reticle keeps
        // drawing, shrunk by sizeScale).
        if (cg_drawGun_vmCvar.integer == 0) {
            // 0x30019fde..0x30019fed: overlay only while actually shrunk.
            if (sizeScale < 1.0f) {
                // 0x30019ff3..0x3001a001: registered center-reticle shader.
                int32_t hShader = (int32_t)cg_weaponInfos[weaponIndex].reticleCenterShader;

                if (hShader == 0) {
                    // 0x3001a005..0x3001a01c: register the fallback material on
                    // the fly from the cg_crosshairNoGun cvar string (0x3048c030
                    // = cg_crosshairNoGun_vmCvar.string, engine-filled; default
                    // "gfx/reticle/hud@center_ads.tga"); skip the draw if that
                    // fails too.
                    hShader = (int32_t)CG_RegisterMaterial(cg_crosshairNoGun_vmCvar.string, R_IMAGE_TRACK_HUD);
                    /* 0x3001a011 reloads the pointer even when registration
                     * returns zero. */
                    weapon = cg_currentWeaponInfo;
                }
                if (hShader != 0) {
                    // 0x3001a022..0x3001a043: size grows as the ordinary
                    // reticle shrinks: centerSize * (1.5 - sizeScale).
                    float size = (float)((long double)weapon->reticleCenterSize * ((long double)1.5f - (long double)sizeScale));
                    const float sizeForX = size * cg_crosshair_reticle_scale_x();
                    const float sizeForY = size * cg_crosshair_reticle_scale_y();
                    // 0x3001a047..0x3001a072 (y), 0x3001a076..0x3001a098 (x):
                    // centered in the view rect, shifted by the projected
                    // impact point (NOT yet overridden), + refdef origin last.
                    /* 0x3001a047/0x3001a076: refdef height/width FILD'd and
                     * refdef y/x FIADD'd straight into the chain -- integers
                     * kept exact, no (float) casts. */
                    float y = (float)(((long double)cg_refdef.height - (long double)sizeForY) * 0.5f +
                                      (long double)cgs_screenYScale * (long double)projected[1] + (long double)cg_refdef.y);
                    float x = (float)(((long double)cg_refdef.width - (long double)sizeForX) * 0.5f +
                                      (long double)cgs_screenXScale * (long double)projected[0] + (long double)cg_refdef.x);

                    // 0x3001a09b CALL 0x3003e0f0.
                    DRAW_CROSSHAIR_STRETCH_PIC(x, y, sizeForX, sizeForY, 0.0f, 0.0f, 1.0f, 1.0f, hShader);
                    /* 0x3001a0a0 reloads after the stretch-pic call. */
                    weapon = cg_currentWeaponInfo;
                }
            }
            // 0x3001a0a9..0x3001a0ba: FUCOMPP vs 1.0; TEST AH,0x44; JNP —
            // JNP fires only on C3 alone (0x40, odd parity), i.e. on EQUAL:
            // at FULL ADS (with the gun hidden) only the overlay above is
            // drawn; mid-transition frames fall through and also draw the
            // ordinary reticle, shrunk by sizeScale and shifted by adsOffset.
            if (adsFraction == 1.0f) {
                goto resetColor;
            }
        }
    }

    // 0x3001a0c0..0x3001a0d5: unless the dynamic crosshair is enabled, the
    // projected impact point is replaced by {0, adsOffset} (adsOffset is 0
    // outside the ADS transition, so this normally re-centers the reticle).
    if (cg_crosshairDynamic_vmCvar.integer == 0) {
        projected[0] = 0.0f;
        projected[1] = adsOffset;
    }

    // 0x3001a0d9..0x3001a0e2: center reticle only with a non-empty name (the
    // shader handle itself is NOT checked; handle 0 is passed through).
    if (weapon->reticleCenter[0] != '\0') {
        // 0x3001a0e8..0x3001a106: projected point scaled to real pixels.
        long double screenXScaleRaw = (long double)cgs_screenXScale;
        int32_t weaponType = weapon->weaponType;
        float projX = (float)(screenXScaleRaw * (long double)projected[0]);
        float projY = (float)((long double)cgs_screenYScale * (long double)projected[1]);
        // 0x3001a10a/0x3001a110: FILD reticleCenterSize.
        float centerSize = (float)(long double)weapon->reticleCenterSize;
        float x;
        float y;

        // 0x3001a0f1/0x3001a114 (type), 0x3001a116..0x3001a11e (timer),
        // 0x3001a120..0x3001a128 (cookOffHold): grenade cook-off pulse.
        if (weaponType == WEAPTYPE_GRENADE) {
            int32_t grenadeTimeLeft = cg_predictedPlayerState.grenadeTimeLeft;

            if (grenadeTimeLeft != 0 && weapon->specialTimeEnabled != 0) {
                int32_t seconds = grenadeTimeLeft / 1000;
                int32_t remainder = grenadeTimeLeft % 1000;
                int32_t previousRemainder = cg_grenadePulseLastSpecialTime % 1000;

            // 0x3001a12a..0x3001a14e: play the per-second pulse sound once per
            // wrap of the millisecond remainder, for whole seconds 0..3 (the
            // unsigned JNC guard rejects >= 4 and negatives).
                if (previousRemainder < remainder && (uint32_t)seconds < 4u) {
                // 0x3001a150..0x3001a158: alias table indexed by whole seconds.
                    CG_PlayClientSoundAliasByName(cg_soundGrenadePulse[seconds]);
                // 0x3001a15d: the machine refetches grenadeTimeLeft after
                // the call (register refill; the sound trap cannot change it),
                // matching the re-reads below.
                    grenadeTimeLeft = cg_predictedPlayerState.grenadeTimeLeft;
                }
            // 0x3001a166..0x3001a188: latch the counter and grow the reticle
            // by the fractional second.
                remainder = grenadeTimeLeft % 1000;
                cg_grenadePulseLastSpecialTime = grenadeTimeLeft;
                centerSize = (float)((long double)remainder * 0.01f + (long double)centerSize);
            }
        }

        // 0x3001a18c..0x3001a19c: ADS shrink applies after the pulse growth.
        centerSize = (float)((long double)centerSize * (long double)sizeScale);
        const float centerSizeForX = centerSize * cg_crosshair_reticle_scale_x();
        const float centerSizeForY = centerSize * cg_crosshair_reticle_scale_y();

        // 0x3001a1a0..0x3001a1f6: y then x; the refdef origin is added BEFORE
        // the scaled projection here (opposite term order vs the ADS overlay).
        /* 0x3001a1a0/0x3001a1dc: height/width FILD'd, y/x FIADD'd -- ints exact,
         * no (float) casts. */
        long double centerYRaw = (long double)cg_refdef.height - (long double)centerSizeForY;
        int32_t hCenterShader = (int32_t)cg_weaponInfos[weaponIndex].reticleCenterShader;
        y = (float)(centerYRaw * 0.5f + (long double)cg_refdef.y + (long double)projY);
        x = (float)(((long double)cg_refdef.width - (long double)centerSizeForX) * 0.5f + (long double)cg_refdef.x + (long double)projX);

        // 0x3001a1f9 CALL 0x3003e0f0 with cg_weaponInfos[i].reticleCenterShader.
        DRAW_CROSSHAIR_STRETCH_PIC(x, y, centerSizeForX, centerSizeForY, 0.0f, 0.0f, 1.0f, 1.0f, hCenterShader);
        /* 0x3001a1fe reloads the global before the side-name dereference. */
        weapon = cg_currentWeaponInfo;
    }

    // 0x3001a207..0x3001a210: side reticle only with a non-empty name (again
    // no shader-zero check; handle 0 is passed through).
    if (weapon->reticleSide[0] != '\0') {
        float spreadX;
        float minSpread;
        long double reticleFade;
        long double aimFracRaw;
        long double spreadRaw;
        long double maxSpreadDelta;
        long double spreadScaled;
        long double spreadYRaw;
        int32_t serverTime;
        int32_t hSideShader;
        int32_t i;

        // 0x3001a216..0x3001a235: second reticle-fade pass; alpha falls as
        // the aim spread rises. The aim product is read only after the call
        // and remains unrounded in x87 for the subtraction and multiplies.
        reticleFade = CG_DrawWeapReticle();
        aimFracRaw = (long double)cg_predictedPlayerState.aimSpreadScale * (long double)(1.0f / 255.0f);
        alphaRaw = reticleFade * (1.0L - aimFracRaw) * (long double)cg_crosshairAlpha_vmCvar.value;
        color[3] = (float)alphaRaw;  // FST at 0x3001a235
        // 0x3001a239..0x3001a24c: clamped up to the cvar minimum.
        if (alphaRaw < (long double)cg_crosshairAlphaMin_vmCvar.value) {
            color[3] = cg_crosshairAlphaMin_vmCvar.value;
        }
        // 0x3001a250..0x3001a257.
        cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);

        // 0x3001a25d..0x3001a26f: custom ABI (ECX=ps, EDX=weaponIndex,
        // EAX=cg_snap->serverTime, stack ads=0).
        // BG_GetMinSpreadForWeapon returns raw st(0) (long double); the DLL
        // folds the lerp+scale into one 80-bit chain off that unrounded value
        // (FSUB ST0,ST1 @0x3001a283 .. FMULP/FADDP @0x3001a293 .. FMUL sizeScale)
        // and rounds ONCE at the FSTP @0x3001a2a7. A float `spread` for the
        // return would round it before the lerp.
        serverTime = cg_snap->serverTime;
        spreadRaw = BG_GetMinSpreadForWeapon(&cg_predictedPlayerState, weaponIndex, serverTime, 0);
        /* 0x3001a274 reloads the global after the spread helper. */
        weapon = cg_currentWeaponInfo;
        // 0x3001a27a..0x3001a295: lerp toward maxSpread by the aim fraction,
        // then apply the ADS shrink scale. This is retained in x87: the first
        // axis is rounded at 0x3001a2a7, while the second remains live through
        // its clamp and all four loop iterations.
        maxSpreadDelta = (long double)weapon->maxSpread - spreadRaw;
        aimFracRaw = (long double)cg_predictedPlayerState.aimSpreadScale * (long double)(1.0f / 255.0f);
        spreadScaled = (spreadRaw + maxSpreadDelta * aimFracRaw) * (long double)sizeScale;

        // 0x3001a299..0x3001a2b7: separate horizontal/vertical pixel spreads
        // (640/fovX vs 480/fovY degrees-to-virtual-pixels).
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): 640/fov_x is
         * authored against the 4:3 canvas; use the 4:3-equivalent angle so
         * the Hor+ expansion does not narrow the horizontal spread. */
        spreadX = (float)(((long double)640.0f / cgame_compat_spread_fov_x()) * spreadScaled);
        spreadYRaw = ((long double)480.0f / (long double)cg_refdef.fov_y) * spreadScaled;
        // 0x3001a2b9..0x3001a2e7: both clamped up to reticleMinOfs.
        minSpread = (float)(long double)weapon->reticleMinOfs;
        if (spreadX < minSpread) {
            spreadX = minSpread;
        }
        if (spreadYRaw < (long double)minSpread) {
            spreadYRaw = (long double)minSpread;
        }

        // 0x3001a2eb/0x3001a2f1: side shader fetched once, before the arrays.
        hSideShader = (int32_t)cg_weaponInfos[weaponIndex].reticleSideShader;

        // Per-piece rect anchor offsets in units of the piece size.
        // Built at 0x3001a2f9..0x3001a329.
        const float align[4][2] = {{-0.5f, -1.0f}, {0.0f, -0.5f}, {-0.5f, 0.0f}, {-1.0f, -0.5f}};
        // Per-piece outward direction (screen coords, y down): up, right,
        // down, left. Built at 0x3001a331..0x3001a35d.
        const float dir[4][2] = {{0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}};
        // Per-piece fixed one-pixel nudges (top piece up one, left piece left
        // one). Built at 0x3001a361..0x3001a394.
        const float nudge[4][2] = {{0.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f}};

        // 0x3001a39d..0x3001a4a7: four rotated quads.
        for (i = 0; i < 4; i++) {
            long double yRaw;
            long double xRaw;

            if (i != 0) {
                /* 0x3001a39d reloads this pointer before iterations 1..3. */
                weapon = cg_currentWeaponInfo;
            }
            // 0x3001a3a3/0x3001a3b2: recomputed each pass.
            float sideSize = (float)((long double)weapon->reticleSideSize * (long double)sizeScale);
            const float sideSizeForX = sideSize * cg_crosshair_reticle_scale_x();
            const float sideSizeForY = sideSize * cg_crosshair_reticle_scale_y();
            // 0x3001a3ab/0x3001a3c3/0x3001a3d0: odd pieces rotate 90 degrees.
            float angle = (float)((long double)(i & 1) * 90.0f);
            // 0x3001a3d6/0x3001a3f3: t1 = 0,0,1,1.
            float t1 = (float)(coduo_int32_sar((uint32_t)i, 1) & 1);
            // 0x3001a3ba/0x3001a3c1/0x3001a3c7: SAR of i-2 (arithmetic shift:
            // (-2)>>1 == -1 and (-1)>>1 == -1), so t2 = 1,1,0,0.
            float t2 = (float)(coduo_int32_sar((uint32_t)i - 2u, 1) & 1);
            // 0x3001a44e..0x3001a498 (x), 0x3001a3fe..0x3001a44a (y): term
            // order matches the FADD/FSUBP stream exactly.
            /* 0x3001a484/0x3001a436: refdef width/height FILD'd into FMUL 0.5,
             * refdef x/y FIADD'd -- integers exact, no (float) casts. */
            yRaw = spreadYRaw * (long double)dir[i][1];
            yRaw += (long double)projected[1];
            yRaw *= (long double)cgs_screenYScale;
            yRaw += (long double)sideSizeForY * (long double)align[i][1];
            yRaw += (long double)nudge[i][1];
            yRaw += (long double)nudge[i][1] * ((long double)cg_crosshair_reticle_scale_y() - 1.0L);
            yRaw -= (long double)sideSizeForY * (long double)dir[i][1] * (long double)weapon->hipReticleSidePos;
            yRaw += (long double)cg_refdef.height * 0.5f;
            yRaw += (long double)cg_refdef.y;
            float y = (float)yRaw;

            xRaw = (long double)spreadX * (long double)dir[i][0];
            xRaw += (long double)projected[0];
            xRaw *= (long double)cgs_screenXScale;
            xRaw += (long double)sideSizeForX * (long double)align[i][0];
            xRaw += (long double)nudge[i][0];
            xRaw += (long double)nudge[i][0] * ((long double)cg_crosshair_reticle_scale_x() - 1.0L);
            xRaw -= (long double)sideSizeForX * (long double)dir[i][0] * (long double)weapon->hipReticleSidePos;
            xRaw += (long double)cg_refdef.width * 0.5f;
            xRaw += (long double)cg_refdef.x;
            float x = (float)xRaw;

            // 0x3001a49b CALL 0x3003e200 (10 args, ADD ESP,0x28): the trap-75
            // quad draw; arg 9 is the rotation in degrees (0 or 90).
            /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the odd pieces
             * rotate 90 degrees about their center BEFORE the classic-mode
             * backend applies its horizontal viewport compression, so their
             * width/height pre-compensation factors swap, and the rect is
             * re-anchored so the post-rotation footprint occupies exactly
             * [x, x+sideSizeForX] by [y, y+sideSizeForY].  With equal axis
             * factors (native widescreen, true 4:3) this is an exact no-op. */
            {
                float drawX = x;
                float drawY = y;
                float drawW = sideSizeForX;
                float drawH = sideSizeForY;

                if ((i & 1) != 0) {
                    drawW = sideSizeForY;
                    drawH = sideSizeForX;
                    drawX = x + (sideSizeForX - sideSizeForY) * 0.5f;
                    drawY = y + (sideSizeForY - sideSizeForX) * 0.5f;
                }
                DRAW_CROSSHAIR_QUAD_PIC(drawX, drawY, drawW, drawH, 0.0f, t1, 1.0f, t2, angle, hSideShader);
            }
        }
    }

resetColor:
    // 0x3001a4af..0x3001a4b3: cgame_syscall(72, NULL).
    cgame_syscall(CG_R_SETCOLOR, 0);
}
