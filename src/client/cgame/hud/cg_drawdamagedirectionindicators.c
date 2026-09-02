#include "../globals.h"
#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3001a980..0x3001aafe
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001a980_3001aafe.mcode
//
// CG_DrawDamageDirectionIndicators — draw the fading "damage direction" arrow ring around the
// crosshair. This is the directional companion of CG_DrawFlashDamage (0x3001a8e0,
// the red full-screen flash); both share the damage-feedback state block at
// 0x3048ae74.. (cg_damageDirIndicators[] plus cg_damageFlashEndTime/Scale).
//
// Naming: the .mcode header's size-matched guess "CG_EjectWeaponBrass" is REJECTED.
// Nothing here ejects a 3D shell or spawns a local entity: it reads cg_snap only as a
// present-frame gate, builds a 2D icon quad in virtual 640x480 space, rotates it by a
// view-relative yaw, and submits it through the 2D-draw traps CG_R_SETCOLOR (72) and
// CG_DrawTurretTagQuad (trap 76). The 320.0/240.0 anchor constants are the 640x480
// screen center; the per-slot {serverTime, duration, yaw} records and the
// vectoyaw(viewForward)-yaw rotation identify it as the damage-direction HUD drawer.
// It also supersedes the provisional caller-observed name CG_DrawScreenBlend
// (0x3001a980 in client_recovered.h): the body proves damage arrows, not a color-blend.
// The Mac CG_DrawDamageDirectionIndicators has the corresponding reticle-position,
// yaw, SetColor, and rotated-quad draw sequence, resolving the source name.
//
// Behavior (all proven against the .mcode instruction stream, frame reconstructed by
// tracking ESP across the PUSH ESI/EDI/EBX and the two trailing call arg batches):
//
//   * Gate on cg_snap (MOV EAX,[cg_snap]; TEST; JZ epilogue). The color[] slots are
//     pre-seeded to white (1,1,1,0) before the gate but only used on the drawing path.
//
//   * Anchor the ring (sx,sy):
//       CG_CalcAdsOverlayFrac(&sx) returns whether the ADS/scope overlay is active
//       (its out-fraction lands in the sx slot but is immediately overwritten below,
//       so only the boolean matters here).
//       - overlay inactive -> sx = 320, sy = 240 (plain screen center).
//       - overlay active   -> require cg_hudDamageIconInScope_vmCvar.integer (else return with no
//                             draw); then CG_ProjectDamageDirToScreen(&sx,&sy) projects
//                             the aim point, and sx += 320, sy += 240 recenters it.
//
//   * Build the icon quad once (cornerOffsets[8], four {x,y} pairs) about the anchor:
//       h = cg_hudDamageIconWidth_vmCvar.value * 0.5;   yTop = cg_hudDamageIconOffset_vmCvar.value;
//       yBot = cg_hudDamageIconOffset_vmCvar.value + cg_hudDamageIconHeight_vmCvar.value;
//       corners = (-h,yTop),(+h,yTop),(+h,yBot),(-h,yBot).
//     (The eight floats are stored interleaved by the FST/FSTP pairs at 0x3001aa25..53.)
//
//   * Walk cg_damageDirIndicators[8] (ESI from 0x3048ae74, stride 0xc, until the hard
//     end address 0x3048aed4 == &cg_damageDirIndicators[8]). For each slot:
//       elapsed = cg.time - slot->serverTime;
//       skip if elapsed <= 0 or elapsed >= slot->duration;   (signed JLE / JGE)
//       yaw       = vectoyaw(cg_refdef.viewaxis[0]) - slot->yaw;   (view-relative angle)
//       alpha     = 2.0f - 2.0f*elapsed/slot->duration;  clamped to a max of 1.0f
//                   (FCOMP 1.0; the arrow holds full alpha for the first half of its
//                    lifetime, then fades to 0 over the second half);
//       color     = (1,1,1,alpha);  cgame_syscall(CG_R_SETCOLOR, color);
//       CG_DrawTurretTagQuad(cornerOffsets, sx, sy, cg_damageDirShaderParams,
//                            yaw, cg_hitDirectionShader);   // rotated icon at the anchor
//
// .rdata / .data constants recovered exactly via objdump -s -j:
//   [0x3007c030]=320.0f, [0x3007c02c]=240.0f (screen-center recenter offsets);
//   [0x3007bce8]=0.5f, [0x3007bce4]=2.0f, [0x3007bce0]=1.0f (icon-half and fade math);
//   [0x30071854]= the static shaderParams float{1,1,0,1,0,0,0,1} (cg_damageDirShaderParams);
//   [0x3044bb6c]= the "hudHitDirection" shader handle (cg_hitDirectionShader),
//                 registered by the asset-load path via CG_RegisterMaterial.

#include <stddef.h>
#include <stdint.h>

/* Draw descriptors: cg_damageDirShaderParams (0x30071854 .rdata) and the
 * "hudHitDirection" shader handle cg_hitDirectionShader (0x3044bb6c .data) are both
 * declared in globals.h. */

void CG_DrawDamageDirectionIndicators(void)
{
    /* 0x3001a983: take the snapshot pointer before initializing the color
     * scratch.  The original tests this retained value after all four stores. */
    snapshot_t *snap = cg_snap;

    /* Pre-seed white draw color (0x3001a98a..0x3001a9a2: [+0..+2]=1.0f, [+3]=0.0f).
     * Only the drawing path below consumes it; alpha is filled per arrow. */
    float color[4];
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 0.0f;

    /* Gate: nothing to draw until a snapshot is installed. */
    if (snap == NULL) {
        return;
    }

    /* Anchor the ring. CG_CalcAdsOverlayFrac writes its out-fraction into the sx slot,
     * but that value is discarded here (overwritten below); only its qboolean return
     * selects the anchor. */
    float sx;
    float sy;
    if (CG_CalcAdsOverlayFrac(&sx)) {
        /* ADS/scope overlay active: require the anchor gate, then project the aim
         * point and recenter it on the 640x480 screen. */
        if (!cg_hudDamageIconInScope_vmCvar.integer) {
            return;
        }
        CG_ProjectDamageDirToScreen(&sx, &sy);
        sx = (float)((long double)sx + (long double)320.0f);
        sy = (float)((long double)sy + (long double)240.0f);
    } else {
        /* No overlay: anchor at plain screen center. */
        sx = 320.0f;
        sy = 240.0f;
    }

    /* Build the icon quad about the anchor. The width product and bottom-edge sum
     * stay in x87 registers until their individual binary32 stores. Store order is
     * also the original's: both -h values, both +h values, both top values, then
     * both bottom values. */
    float cornerOffsets[8];
    long double hRaw = (long double)cg_hudDamageIconWidth_vmCvar.value * 0.5L;
    float negativeH = (float)-hRaw;
    cornerOffsets[6] = negativeH;
    cornerOffsets[0] = negativeH;
    float positiveH = (float)hRaw;
    cornerOffsets[4] = positiveH;
    cornerOffsets[2] = positiveH;

    float yTop = cg_hudDamageIconOffset_vmCvar.value;
    cornerOffsets[3] = yTop;
    cornerOffsets[1] = yTop;

    long double yBottomRaw = (long double)cg_hudDamageIconHeight_vmCvar.value + (long double)cg_hudDamageIconOffset_vmCvar.value;
    float yBottom = (float)yBottomRaw;
    cornerOffsets[7] = yBottom;
    cornerOffsets[5] = yBottom;

    /* Walk the arrow ring; draw each live slot. */
    for (int i = 0; i < CG_DAMAGE_DIRECTION_SLOT_COUNT; ++i) {
        cg_damageDirIndicator_t *ind = &cg_damageDirIndicators[i];

        /* 0x3001aa60..0x3001aa72: the current time, start time, and duration are
         * all snapshotted before either local is consumed. SUB is target i386
         * two's-complement wrapping arithmetic. */
        int32_t now = cg_time;
        int32_t serverTime = ind->serverTime;
        int32_t duration = ind->duration;
        int32_t elapsed = coduo_int32_from_bits((uint32_t)now - (uint32_t)serverTime);

        /* Signed lifetime window: skip freshly-cleared (elapsed <= 0) and expired
         * (elapsed >= duration) slots. */
        if (elapsed <= 0 || elapsed >= duration) {
            continue;
        }

        /* Rotate the icon by the attacker direction relative to the current view. */
        float yaw = (float)((long double)vectoyaw(cg_refdef.viewaxis[0]) - (long double)ind->yaw);

        /* alpha = 2 - 2*elapsed/duration, held at a max of 1.0 (full for the first
         * half of the lifetime, then linear fade to 0). 0x3001aa8d FILD elapsed /
         * 0x3001aa93 FIDIV duration: both integers enter the chain EXACT (no FSTP
         * DWORD), so no (float) casts. (The max-1.0 clamp makes float-vs-80-bit
         * alpha bit-identical here, since the clamp target equals the round
         * boundary; the raw value is kept live for the original unordered-aware
         * max comparison.) */
        long double alphaWide = 2.0L - ((long double)elapsed + (long double)elapsed) / (long double)duration;
        float alpha;
        if (alphaWide > 1.0L) {
            alpha = 1.0f;
        } else {
            alpha = (float)alphaWide;
        }
        color[3] = alpha;

        cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);
        qhandle_t shader = cg_hitDirectionShader;
        CG_DrawTurretTagQuad(cornerOffsets, sx, sy, cg_damageDirShaderParams, yaw, shader);
    }
}
