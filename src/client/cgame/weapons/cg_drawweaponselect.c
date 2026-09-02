// Source: uo_cgame_mp_x86.dll 0x30046bb0..0x3004736e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30046bb0_3004736e.mcode
//
// CG_DrawWeaponSelect — draw the weapon-selection HUD strip: one icon per
// occupied inventory slot (slots 7..1, right-to-left from x=632), each slot
// expanding (cg_weaponSelectSlotScale) while current and collapsing otherwise.
// The current slot additionally shows its alt-fire chain: the chain members
// other than the selected weapon are drawn dim below the strip via raw
// stretch-pic traps, and the selected weapon is drawn white, sliding between
// the vertical layout positions pos[] while cg_weaponSelectTransition decays
// toward zero after an alt-switch. The CG_RegisterGraphics size match is
// rejected: this is a per-frame HUD renderer.
//
// .rdata constants byte-verified (objdump -s --section=.rdata):
//   0x3007bce0=1.0f  0x3007bce4=2.0f  0x3007bce8=0.5f  0x3007bcec=0.0f
//   0x3007bda4=10.0f 0x3007bdb4=0.01f(0x3c23d70a) 0x3007bdd0=32.0f
//   0x3007bf20=34.0f 0x3007bf50=-0.5f 0x3007c000=64.0f
//   0x3007c188=0x3bda740e (0.006666667f, the 1/150 transition-decay rate)

#include "../client_recovered.h"

#include <math.h>


enum {
    CG_WEAPON_SELECT_FADE_MSEC = 1800
}; /* 0x30046bec: ECX=0x708 */

#define CG_WEAPON_SELECT_FLOAT_SIGN_MASK 0x80000000u

enum {
    CG_WEAPON_SELECT_DIRECTION_BACKWARD = -1,
    CG_WEAPON_SELECT_DIRECTION_FORWARD = 1
};

void CG_DrawWeaponSelect(void)
{
    int32_t frameMsec;
    int32_t currentSlot;
    int32_t selectedWeapon;
    vec_t *fade;
    vec4_t color;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    float pos[MAX_WEAPONS];
    float x = 632.0f; /* 0x30046c53: [ESP+0x10] = 0x441e0000 */


    /* 0x30046bb6..0x30046bd1: milliseconds since the previous carousel frame;
     * latch the new draw time and clamp a backwards clock to zero (JNS). */
    frameMsec = coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)cg_weaponSelectLastTime);
    cg_weaponSelectLastTime = cg_time;
    if (frameMsec < 0)
        frameMsec = 0;

    /* 0x30046bd8: CMP [0x304831c8],6; JGE ret — no carousel while dead. */
    if (cg_predictedPlayerState.pmType >= PM_TYPE_DEAD)
        return;

    /* 0x30046be5..0x30046bfa: CG_FadeColor(EDX=cg_weaponSelectTime, ECX=1800);
     * NULL means the select HUD has fully faded out. */
    fade = CG_FadeColor(cg_weaponSelectTime, CG_WEAPON_SELECT_FADE_MSEC);
    if (fade == NULL)
        return;

    /* 0x30046c03: refresh the cached key bindings the slot hints display. */
    Controls_GetConfig();

    /* 0x30046c08..0x30046c2e: base draw color = 0.5 gray at the fade alpha
     * (fade[3] read from [ESI+0xc]); installed via trap 0x48. */
    color[0] = 0.5f;
    color[1] = 0.5f;
    color[2] = 0.5f;
    color[3] = fade[3];
    cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);

    /* 0x30046c34..0x30046c4f: which slot holds the selected weapon.
     * ESI = [0x3044034c] = cg_weaponSelect_vmCvar.integer, loop-invariant (the
     * reloads at 0x30046cf7/0x30047329/0x30047031 are register refills after
     * calls; nothing in the loop writes it). */
    selectedWeapon = cg_weaponSelect_vmCvar.integer;
    currentSlot = BG_IsPlayerWeaponInSlot(&cg_predictedPlayerState, selectedWeapon, 1);

    /* 0x30046c47..0x30046c64 + 0x30047345..0x3004735d: EBX=7 down to 1, EBP
     * walking &cg_weaponSelectSlotScale[7] down while > &[0]. */
    for (int32_t slot = WEAPSLOT_COUNT - 1; slot >= 1; --slot) {
        float *scale = &cg_weaponSelectSlotScale[slot];
        /* 0x30046c70..0x30046c7e: MOVSX of ps->weaponSlots[slot] (byte). */
        int32_t weapon = (int8_t)cg_predictedPlayerState.weaponSlots[slot];

        /* 0x30046c76/0x30046c81..0x30046c99: empty byte, or held bit clear in
         * cg_predictedPlayerState.weaponBits (word weapon>>5, bit weapon&0x1f;
         * the inline form of CG_IsWeaponHeld, same sign-extended index math).
         * 0x30047334..0x30047345: back up one unexpanded icon width plus the
         * 2px gutter and collapse the slot's expansion; nothing drawn. */
        if (weapon == 0 || !CG_IsWeaponHeld(weapon)) {
            x -= 32.0f;    /* FSUB [0x3007bdd0] */
            *scale = 0.0f;
            x -= 2.0f;     /* FSUB [0x3007bce4] */
            continue;
        }

        weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];          /* 0x30046ca5 */
        cgWeaponInfo_t *cgWeaponInfo = &cg_weaponInfos[weapon];  /* 0x30046ca8 */

        /* 0x30046cb3..0x30046d00: no hud icon registered — draw the shared
         * "no weapon" icon at a fixed 32x32, collapse the slot's expansion,
         * and skip the expansion animation, alt-chain drawing AND the key
         * hint (jump straight to the 2px gutter tail at 0x3004733e). */
        if (cgWeaponInfo->hudIconShader == 0) {
            x -= 32.0f;
            CG_DrawPic(x, 8.0f, 32.0f, 32.0f, cgs_media_hudNoWeaponIcon);
            *scale = 0.0f;
            x -= 2.0f;
            continue;
        }

        if (slot != currentSlot) {
            /* 0x30046d14..0x30046d4d: collapse: only while scale > 0 and time
             * advanced; clamp at 0 (FSUBR; FCOMP vs [0x3007bcec]=0.0f). */
            if (*scale > 0.0f && frameMsec != 0) {
                /* frameMsec enters via a bare FILD fed straight into FMUL 0.01f
                 * (0x30046d29 FILD; 0x30046d2d FMUL), so drop the (float) cast
                 * (Class 4). */
                long double scaleRaw = (long double)*scale - (long double)frameMsec * (long double)0.01f;
                *scale = (float)scaleRaw; /* [0x3007bdb4] */
                if (scaleRaw < 0.0L)
                    *scale = 0.0f;
            }

            /* 0x30046d4d..0x30046da2: icon size from the slot weapon —
             * height (scale+1)*32, doubled width for wideListIcon (+0x344);
             * draw at y=8 after stepping x left by the width. */
            float iconHeight = (*scale + 1.0f) * 32.0f;
            float iconWidth = weaponInfo->wideListIcon ? iconHeight * 2.0f /* FADD ST0,ST0 */
                                                       : iconHeight;
            x -= iconWidth;
            CG_DrawPic(x, 8.0f, iconWidth, iconHeight, (qhandle_t)cgWeaponInfo->hudIconShader);
        } else {
            /* 0x30046daa..0x30046de3: expand: only while scale < 1 and time
             * advanced; clamp at 1 (FADD; FCOMP vs [0x3007bce0]=1.0f). */
            if (*scale < 1.0f && frameMsec != 0) {
                /* Bare FILD into FMUL 0.01f (0x30046dbf FILD; 0x30046dc3 FMUL);
                 * drop the (float) cast (Class 4). */
                long double scaleRaw = (long double)*scale + (long double)frameMsec * (long double)0.01f;
                *scale = (float)scaleRaw;
                if (scaleRaw > 1.0L)
                    *scale = 1.0f;
            }

            /* 0x30046de3..0x30046e0c: measure the alt-fire chain rooted at
             * the SELECTED weapon (ESI), not the slot byte: head is
             * bg_weaponInfos[selected]->altWeapon (+0x36c); walk until the
             * chain returns to the selected weapon or hits 0. */
            int32_t chainHead = bg_weaponInfos[selectedWeapon]->altWeapon;
            int32_t count = 1; /* EDI */
            for (int32_t w = chainHead; w != 0 && w != selectedWeapon; w = bg_weaponInfos[w]->altWeapon)
                ++count;

            if (count == 1) {
                /* 0x30046e12..0x30046ebf: lone weapon in the slot: reset the
                 * transition state, latch prev slot/weapon, and draw the one
                 * icon in full white (RGB 1.0, fade alpha kept). */
                cg_weaponSelectTransition = 0.0f;
                cg_weaponSelectPreviousSlot = currentSlot;
                cg_weaponSelectPreviousWeapon = selectedWeapon;
                color[0] = 1.0f;
                color[1] = 1.0f;
                color[2] = 1.0f;
                cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);

                float iconHeight = (*scale + 1.0f) * 32.0f;
                float iconWidth = weaponInfo->wideListIcon ? iconHeight * 2.0f : iconHeight;
                x -= iconWidth;
                CG_DrawPic(x, 8.0f, iconWidth, iconHeight, (qhandle_t)cgWeaponInfo->hudIconShader);

                /* 0x300472de..0x30047303: restore the 0.5 gray draw color. */
                color[0] = 0.5f;
                color[1] = 0.5f;
                color[2] = 0.5f;
                cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);
            } else {
                /* 0x30046ec4..0x30046f0e: transition bookkeeping. A slot
                 * change resets the state; a weapon change within the slot
                 * starts a slide: +1 if the previous selection's altWeapon is
                 * the new selection (stepping forward through the chain),
                 * else -1. */
                if (cg_weaponSelectPreviousSlot != currentSlot) {
                    cg_weaponSelectPreviousSlot = currentSlot;
                    cg_weaponSelectPreviousWeapon = selectedWeapon;
                    cg_weaponSelectTransition = 0.0f; /* via 0x30046f74 */
                } else {
                    if (cg_weaponSelectPreviousWeapon != selectedWeapon) {
                        cg_weaponSelectTransition =
                            bg_weaponInfos[cg_weaponSelectPreviousWeapon]->altWeapon == selectedWeapon ? 1.0f : -1.0f;
                        cg_weaponSelectPreviousWeapon = selectedWeapon;
                    }

                    /* 0x30046f0e..0x30046f74: decay the transition toward 0
                     * at 1/150 per ms ([0x3007c188]=0x3bda740e), clamping at
                     * the zero crossing. Runs only when the transition is
                     * nonzero (FUCOMPP gate) and only on this prevSlot ==
                     * currentSlot path (the slot-change reset above jumps
                     * past it to 0x30046f74). */
                    if (cg_weaponSelectTransition != 0.0f) {
                        /* Bare FILD into FMUL 0.006666667f (0x30046f2f FILD;
                         * 0x30046f33 FMUL); drop the (float) cast (Class 4). */
                        long double step = (long double)frameMsec * (long double)0.006666667f;
                        if (cg_weaponSelectTransition < 0.0f) {
                            long double transitionRaw = (long double)cg_weaponSelectTransition + step;
                            cg_weaponSelectTransition = (float)transitionRaw;
                            if (transitionRaw > 0.0L)
                                cg_weaponSelectTransition = 0.0f;
                        } else {
                            long double transitionRaw = (long double)cg_weaponSelectTransition - step;
                            cg_weaponSelectTransition = (float)transitionRaw;
                            if (transitionRaw < 0.0L)
                                cg_weaponSelectTransition = 0.0f;
                        }
                    }
                }

                /* 0x30046f7e..0x30047019: slot icon size (from the slot
                 * weapon's wideListIcon), and the vertical position table:
                 * pos[0]=8 (the strip row), pos[1]=iconHeight+10 (first alt
                 * row), each further row 34 lower ([0x3007bf20]; the mcode
                 * builds it with a 4-unrolled loop + remainder). */
                long double iconHeightRaw = ((long double)*scale + 1.0L) * 32.0L;
                float iconHeight = (float)iconHeightRaw;
                long double iconWidthRaw;
                if (weaponInfo->wideListIcon) {
                    iconWidthRaw = (long double)iconHeight * 2.0L;
                } else {
                    iconWidthRaw = iconHeightRaw;
                }
                float iconWidth = (float)iconWidthRaw;
                pos[0] = 8.0f;
                pos[1] = iconHeight + 10.0f; /* [0x3007bda4] */
                int32_t posIndex = 2;
                if (count >= 6) {
                    long double posRaw = (long double)pos[1];
                    for (; posIndex < 6; ++posIndex) {
                        posRaw += 34.0L;
                        pos[posIndex] = (float)posRaw;
                    }
                }
                for (; posIndex < count; ++posIndex)
                    pos[posIndex] = pos[posIndex - 1] + 34.0f;

                /* 0x3004701b..0x30047025: FMUL [0x3007bf50] = -0.5f; the
                 * common "back up half the expanded slot width" term both
                 * draw paths center on. */
                float iconHalfBack = (float)(iconWidthRaw * (long double)-0.5f);

                /* 0x30047031..0x300471bc: draw the alt chain, dim (current
                 * gray color), stopping at the selected weapon. Sizes are
                 * UNexpanded (32, or 64 wide [0x3007c000]); every alt icon
                 * uses layout row pos[1]. */
                for (int32_t w = chainHead; w != 0 && w != selectedWeapon; w = bg_weaponInfos[w]->altWeapon) {
                    weaponInfo_t *altInfo = bg_weaponInfos[w];          /* 0x30047048 */
                    cgWeaponInfo_t *altCgInfo = &cg_weaponInfos[w];  /* 0x30047051 */
                    long double drawWidthRaw = altInfo->wideListIcon ? 64.0L : 32.0L;
                    float drawHeight = 32.0f; /* 0x30047066: [ESP+0x14]=0x42000000 */
                    long double yRaw;

                    if (cg_weaponSelectTransition != 0.0f) {
                        /* 0x30047091..0x300470c7: slide row: lerp from pos[1]
                         * toward pos[(1+dir)%count] by |transition| (dir from
                         * the float's sign: SETL on the raw dword; the IDIV
                         * remainder matches C's % on the nonnegative 0/2). */
                        float shift = fabsf(cg_weaponSelectTransition);
                        int32_t dir = (CG_FloatBits(cg_weaponSelectTransition) & CG_WEAPON_SELECT_FLOAT_SIGN_MASK) != 0u
                                          ? CG_WEAPON_SELECT_DIRECTION_BACKWARD
                                          : CG_WEAPON_SELECT_DIRECTION_FORWARD;
                        int32_t target = (1 + dir) % count;
                        yRaw = (long double)pos[1] - ((long double)pos[1] - (long double)pos[target]) * (long double)shift;

                        /* 0x300470cb..0x30047110: while sliding toward the
                         * strip (transition > 0, or the two-weapon chain's
                         * backward slide), the alt icon grows toward the
                         * expanded size: factor 1 + |transition|*scale
                         * (FMULP ST3 scales the width, FMUL 32 the height). */
                        if (cg_weaponSelectTransition > 0.0f || (count == 2 && cg_weaponSelectTransition < 0.0f)) {
                            long double growRaw = (long double)shift * (long double)*scale + 1.0L;
                            drawWidthRaw *= growRaw;
                            drawHeight = (float)(growRaw * 32.0L);
                        }
                    } else {
                        yRaw = pos[1]; /* 0x30047116 */
                    }

                    /* 0x3004711a..0x300471ab: raw trap 0x49 stretch-pic with
                     * pre-scaled pixel coords (cgs_screenXScale/YScale) and
                     * the full texture window (s1=0,t1=0,s2=1,t2=1); shader
                     * from the alt weapon's cgWeaponInfo (+0x144) with NO
                     * no-icon fallback. The icon is centered on the slot
                     * center x + iconHalfBack. */
                    cgame_syscall(CG_R_DRAWSTRETCHPIC,
                                  CG_FloatBits((float)(((long double)x + (long double)iconHalfBack - drawWidthRaw * 0.5L) *
                                                       (long double)cgs_screenXScale)),
                                  CG_FloatBits((float)(yRaw * (long double)cgs_screenYScale)),
                                  CG_FloatBits((float)(drawWidthRaw * (long double)cgs_screenXScale)),
                                  CG_FloatBits((float)((long double)drawHeight * (long double)cgs_screenYScale)), CG_FloatBits(0.0f),
                                  CG_FloatBits(0.0f), CG_FloatBits(1.0f), CG_FloatBits(1.0f), (intptr_t)altCgInfo->hudIconShader);
                }

                /* 0x300471c2..0x300471e1: selected weapon drawn white. */
                color[0] = 1.0f;
                color[1] = 1.0f;
                color[2] = 1.0f;
                cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);

                /* 0x300471e7..0x30047206: unexpanded base size from the SLOT
                 * weapon's wideListIcon (the shader below is likewise the
                 * slot weapon's — the slot byte tracks the active selection). */
                float baseWidth = weaponInfo->wideListIcon ? 64.0f : 32.0f;
                float ySel;
                float growSel;

                if (cg_weaponSelectTransition != 0.0f) {
                    /* 0x3004721b..0x3004724f: slide from pos[0] toward
                     * pos[dir % count] (dir = +1/-1 via SETL on the sign
                     * bit; the IDIV remainder of -1/count is -1, as C's %). */
                    float shift = fabsf(cg_weaponSelectTransition);
                    int32_t dir = (CG_FloatBits(cg_weaponSelectTransition) & CG_WEAPON_SELECT_FLOAT_SIGN_MASK) != 0u
                                      ? CG_WEAPON_SELECT_DIRECTION_BACKWARD
                                      : CG_WEAPON_SELECT_DIRECTION_FORWARD;
                    int32_t target = dir % count; /* 1 or -1 */
                    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    float targetPos = target < 0 ? color[3] : pos[target];
                    ySel = pos[0] - (pos[0] - targetPos) * shift;
                    /* 0x30047253..0x30047264: the selected icon shrinks from
                     * the expanded size as it slides away: factor
                     * 1 + (1-|transition|)*scale. */
                    growSel = (1.0f - shift) * *scale + 1.0f;
                } else {
                    /* 0x30047270..0x30047281 */
                    ySel = pos[0];
                    growSel = *scale + 1.0f;
                }

                /* 0x30047285..0x300472cd: CG_DrawPic (640-space, unlike the
                 * alts' raw pixel trap), centered on the same slot center. */
                float selWidth = growSel * baseWidth;
                float selHeight = growSel * 32.0f;
                CG_DrawPic(x + iconHalfBack - selWidth * 0.5f, ySel, selWidth, selHeight, (qhandle_t)cgWeaponInfo->hudIconShader);

                /* 0x300472d2: commit the slot advance. */
                x -= iconWidth;

                /* 0x300472de..0x30047303: restore the 0.5 gray draw color. */
                color[0] = 0.5f;
                color[1] = 0.5f;
                color[2] = 0.5f;
                cgame_syscall(CG_R_SETCOLOR, (intptr_t)color);
            }
        }

        /* 0x30047310..0x30047325: slot key hint at the slot's left edge
         * (ECX=slot, EDI=color, stack x, 8.0f); reached by every path that
         * drew a real icon (the empty and no-icon paths skip it). */
        CG_DrawWeaponSelectKeyHint(color, slot, x, 8.0f);

        /* 0x30047345: 2px gutter between slots ([0x3007bce4] = 2.0f). */
        x -= 2.0f;
    }
    /* 0x30047363..0x3004736d: no trailing trap_R_SetColor(NULL) — the 0.5
     * gray modulation is left installed on return. */
}
