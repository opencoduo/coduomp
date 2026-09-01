// Source: uo_cgame_mp_x86.dll 0x3002efc0..0x3002f90a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002efc0_3002f90a.mcode
//
// CG_DrawPlayerStance — draw the on-screen "change stance" HUD prompt plus the stance
// and fatigue icons.
//
// Naming: the mechanical `.mcode` name `Bullet_Fire_Extended` (a size-guess from
// game_mp_uo coverage, per its own name_note) is REJECTED — this function does no
// bullet firing. Its body reads the hudStance*/hudFatigue* icon shaders
// (cg_stanceHudShaders), latches the player's stance flags, builds three tables of
// the stance-change key hints ("+prone","goprone","toggleprone","gocrouch",... and
// CGAME_STANCEHINT_JUMP/STAND/CROUCH/PRONE), resolves the key bound to each command,
// and draws the hint text and stance/fatigue icons through the 2D-draw traps.
// Retail UO assigns owner-draw id 20 the exact name CG_PLAYER_STANCE, and the
// corresponding macOS owner-draw jump-table case calls CG_DrawPlayerStance,
// establishing the exact original function name.
//
// This is a 2D HUD draw with no frame pointer; every argument and local is
// ESP-relative on entry. All coordinates handed to the draw traps are single-
// precision floats passed as their raw 32-bit words (the machine code FSTPs a float
// then forwards the dword), reproduced with CG_FloatBits so no double-promotion is
// introduced. The deep stack layout the machine code indexes with `[ESP+idx+base]`
// (three char*[4][6] command tables, a char*[4] hint table, a char*[4] chosen list,
// and a reused float drawColor[4]) is modelled as real C arrays. Every draw is
// issued directly through cgame_syscall (the 0x30085e9c dispatch pointer), which is
// exactly what the machine code does (it does NOT call the trap_R_* wrappers).
//
// Trap ids used (first cgame_syscall argument):
//   0x34/52 CG_R_TEXT_WIDTH          — measure text width (result FILD'd to float)
//   0x35/53 CG_R_TEXT_HEIGHT          — text/line height query (result FILD'd to float)
//   0x36/54 CG_R_TEXT_PAINT          — draw a 2D text frame (variadic; arity per call site)
//   0x48/72 CG_R_SETCOLOR       — set the 2D draw color (rgba[4] | NULL)
//   0x49/73 CG_R_DRAWSTRETCHPIC — 2D stretch-pic (x,y,w,h,s1,t1,s2,t2,hShader)

#include <stdint.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "client/common/client_format_validation.h"

/* Stance flag bits latched into cg_stanceHintFlags (= playerState snapshot flags
 * & 0x10003) and tested against cg_predictedPlayerState.playerStateFlags. Exact EF/PMF source
 * flag names unresolved; named by the shader selected for each bit. */
enum {
    STANCE_FLAG_PRONE  = 0x00000001u, /* 0x1     -> prone table / prone icons */
    STANCE_FLAG_CROUCH = 0x00000002u, /* 0x2     -> crouch table / crouch icons */
    STANCE_FLAG_SPRINT = 0x00010000u, /* 0x10000 -> sprint icons */
    STANCE_FLAG_MASK   = 0x00010003u, /* the three bits latched together */
    STANCE_FLAG_HELD   = 0x00008000u  /* bit 0x8000 (TEST CH,CH): re-arm the timer */
};

/* Suppress bit read from cg_stanceHintSuppressFlags. */
enum { STANCE_HINT_SUPPRESS_BIT = 0x1u };

enum {
    STANCE_VIEW_RESTRICTED = 0x100000u,
    STANCE_VIEW_OVERRIDE   = 0x400000u
};

/* cg_stanceHudShaders[] indices (see globals.h). */
enum {
    HUD_SHADER_STANCE_STAND   = 0, /* 0x3044bb24: hudStanceStand */
    HUD_SHADER_STANCE_CROUCH  = 1, /* 0x3044bb28: hudStanceCrouch */
    HUD_SHADER_STANCE_PRONE   = 2, /* 0x3044bb2c: hudStanceProne */
    HUD_SHADER_STANCE_SPRINT  = 3, /* 0x3044bb30: hudStanceSprint */
    HUD_SHADER_STANCE_FLASH   = 4, /* 0x3044bb34: hudStanceFlash */
    HUD_SHADER_FATIGUE_STAND  = 5, /* 0x3044bb38: hudFatigueStand */
    HUD_SHADER_FATIGUE_CROUCH = 6, /* 0x3044bb3c: hudFatigueCrouch */
    HUD_SHADER_FATIGUE_PRONE  = 7, /* 0x3044bb40: hudFatigueProne */
    HUD_SHADER_FATIGUE_SPRINT = 8  /* 0x3044bb44: hudFatigueSprint */
};

/*
 * arg1 (colorVec) is a float color[4]; arg0 (iconRect) is a float[4] rect
 * [x@0, y@4, w@8, h@c]. arg2/arg3 are the two integer style/handle words threaded
 * through the CG_R_TEXT_WIDTH/53/54 text calls; arg4 is the trailing text-frame parameter
 * forwarded to CG_R_TEXT_PAINT. Exactly five stack args: the highest incoming slot
 * this body reads is [entry+0x14] (arg4), and the sole caller (FUN_300320e0,
 * call at 0x3003229f) cleans ADD ESP,0x14 = 5 dwords.
 */
void CG_DrawPlayerStance(const rectDef_t *iconRect, /* arg0 (EDI) */
                       const float *colorVec,   /* arg1 (float[4] color) */
                       int32_t textArgA,        /* arg2 */
                       int32_t textArgB,        /* arg3 */
                       int32_t textArgC)        /* arg4 */
{
    /* Reused 2D draw-color buffer (frame slots -0x150..-0x144). The top of the
     * function copies colorVec[0..2] into [0..2]; the alpha [3] is overwritten per
     * draw. Modelled as one local, matching the machine code's buffer reuse. */
    float drawColor[4];
    int32_t now = coduo_int32_from_bits((uint32_t)cg_time);
    float slideX;   /* frame -0x154: animated horizontal slide offset (all icons) */

    /* 0x3002efc0: gate on the predicted playerState entity-state flags. Proceed only
     * when 0x100000 is clear, OR 0x400000 is set. */
    if ((cg_predictedPlayerState.entityStateFlags & STANCE_VIEW_RESTRICTED) != 0u &&
        (cg_predictedPlayerState.entityStateFlags & STANCE_VIEW_OVERRIDE) == 0u) {
        return;
    }

    {
        /* 0x3002efed: snapshot / stance-change detection. */
        const uint32_t stanceFlags = cg_predictedPlayerState.playerStateFlags;

        if (cg_hudStanceHintPrints_vmCvar.integer == 0u) {
            cg_stanceHintChangeTime = -1;                        /* 0x3002f005 */
        } else if (cg_stanceHintChangeTime <= now) {            /* 0x3002f017 JG */
            if (((stanceFlags ^ cg_stanceHintFlags) & 0x3u) != 0u) { /* 0x3002f023 */
                cg_stanceHintChangeTime = now;                   /* 0x3002f028 */
            }
        } else {
            cg_stanceHintChangeTime = now;                       /* 0x3002f028 */
        }

        /* 0x3002f02d: one unstored x87 chain, rounded only into slideX. */
        slideX = (float)((((long double)cg_hudCompassSize_vmCvar.value - 1.0L) *
                          112.0L) + (long double)iconRect->x);

        /* 0x3002f040: stash colorVec[0..2] into the reused draw-color buffer. */
        drawColor[0] = colorVec[0];
        drawColor[1] = colorVec[1];
        drawColor[2] = colorVec[2];
        drawColor[3] = 0.0f;

        /* 0x3002f068: latch the masked stance flags. */
        cg_stanceHintFlags = stanceFlags & STANCE_FLAG_MASK;

        /* 0x3002f084: bit 0x8000 (TEST CH,CH) governs whether the slide timer is
         * re-armed. When it is set and the current expire time has already passed,
         * arm it to now + 1500ms. */
        {
            int32_t expire = cg_stanceHintExpireTime;
            if ((stanceFlags & STANCE_FLAG_HELD) != 0u) {
                if (expire < now) {
                    expire = coduo_int32_from_bits((uint32_t)now + 1500u); /* +0x5dc */
                    cg_stanceHintExpireTime = expire;
                }
            }

            /* 0x3002f096: while the slide window is open, draw the animated
             * stance-change label. */
            if (expire > now) {
                if (((uint8_t)cg_predictedEventEntity.nextState.eFlags &
                     STANCE_HINT_SUPPRESS_BIT) != 0u) {
                    expire = -1;                                  /* 0x3002f0a7 */
                    cg_stanceHintExpireTime = expire;
                }
                if (expire > now) {
                    /* 0x3002f0b8: translate and measure "CGAME_PRONE_BLOCKED".
                     * CG_R_TEXT_WIDTH(label, arg2, arg3, 0) -> width (FILD'd). */
                    char *label = CG_SafeTranslateString_Internal("cgame", "CGAME_PRONE_BLOCKED");
                    int32_t width = coduo_int32_from_bits((uint32_t)cgame_syscall(
                                                  CG_R_TEXT_WIDTH, label,
                                                  textArgA, textArgB, 0));

                    /* 0x3002f0e8: remaining slide time (expire - now) is threaded
                     * through the fixed constants and FSIN to make the pulsing alpha;
                     * the x position is centered as 320 - width/2. `width` and
                     * `remaining` are FILD'd straight into the arithmetic below
                     * (0x3002f0f2 / 0x3002f110) with no FSTP DWORD, so they stay
                     * exact -- no (float) cast, which would round them. */
                    int32_t remaining = coduo_int32_from_bits(
                        (uint32_t)cg_stanceHintExpireTime - (uint32_t)now);
                    float centerX;

                    /* 0x3002f110..0x3002f142: FMUL chain composing the sine argument
                     * from the FILD'd REMAINING TIME (ST0). The whole chain, the
                     * FSIN (0x3002f14a) and the FABS (0x3002f15a) stay in 80-bit
                     * registers; the ONLY rounding is the drawColor[3] store
                     * (FSTP 0x3002f15c) -- hence one expression through sinl/fabsl
                     * (sinf would round both the argument and the result). */
                    drawColor[3] = (float)__builtin_fabsl(__builtin_sinl(
                        (long double)remaining
                             * (long double)0.0006666666595f  /* 0x3007c1f8 */
                             * (long double)540.0f            /* 0x3007c1f4 */
                             * (long double)3.1415927410125732f /* PI */
                             * (long double)0.0055555556900799274f)); /* 1/180 */

                    /* 0x3002f103: FMUL 0.5 applies to the FILD'd WIDTH; the half-
                     * width stays UNROUNDED in ST1 (no float store) until the
                     * centering FSUB ST0,ST1 at 0x3002f166, so it is written
                     * inline: centerX = 320.0f - width*0.5f. */
                    centerX = (float)((long double)320.0f -
                                      (long double)width * (long double)0.5f);

                    /* 0x3002f0d0..0x3002f175: draw the centered label.
                     * CG_R_TEXT_PAINT(x=centerX, y=270.0f, arg2, arg3, color, text,
                     *            0, 0, arg4). y const 0x43870000 = 270.0f. */
                    cgame_syscall(CG_R_TEXT_PAINT,
                                  CG_FloatBits(centerX),
                                  CG_FloatBits(270.0f),
                                  textArgA,
                                  textArgB,
                                  drawColor,
                                  label,
                                  0,
                                  0,
                                  textArgC);
                    now = coduo_int32_from_bits((uint32_t)cg_time); /* 0x3002f17b reload */
                }
            }
        }
    }

    /* 0x3002f189: is the hint help still within its 3000ms display window
     * (cg_stanceHintChangeTime + 3000 > cg.time)? If not, skip to the icon draws. */
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (coduo_int32_from_bits((uint32_t)cg_stanceHintChangeTime + 3000u) > now) {
        /* Three parallel command-name tables (one per stance context) and a hint
         * label table, built exactly as the machine code fills the frame at
         * 0x3002f19d..0x3002f3d3. Each row is a stance target; the six columns are
         * the alternative bind commands, tried in order. */
        char *cmdTableStanding[4][6] = {
            { 0, 0, 0, 0, 0, 0 },
            { "+gostand", "toggleprone", 0, 0, 0, 0 },
            { "gocrouch", "togglecrouch", "raisestance", "+movedown", "+moveup", 0 },
            { 0, 0, 0, 0, 0, 0 }
        };
        char *cmdTableCrouch[4][6] = {
            { 0, 0, 0, 0, 0, 0 },
            { "+gostand", "raisestance", "+moveup", 0, 0, 0 },
            { 0, 0, 0, 0, 0, 0 },
            { "goprone", "lowerstance", "toggleprone", "+prone", 0, 0 }
        };
        char *cmdTableProne[4][6] = {
            { "+gostand", "+moveup", 0, 0, 0, 0 },
            { 0, 0, 0, 0, 0, 0 },
            { "gocrouch", "togglecrouch", "lowerstance", "+movedown", 0, 0 },
            { "goprone", "+prone", 0, 0, 0, 0 }
        };
        char *hintLabels[4] = {
            "CGAME_STANCEHINT_JUMP",
            "CGAME_STANCEHINT_STAND",
            "CGAME_STANCEHINT_CROUCH",
            "CGAME_STANCEHINT_PRONE"
        };
        char *chosenCmd[4] = { 0, 0, 0, 0 };
        int32_t row;
        int32_t boundCount = 0;
        float lineHeight;  /* frame -0x164: measured line height */
        float lineY;       /* frame -0x170: running text baseline */

        /* 0x3002f3db: refresh the cached key bindings before querying them. */
        Controls_GetConfig();
        now = coduo_int32_from_bits((uint32_t)cg_time); /* 0x3002f3e5 reload after call */

        /* 0x3002f3e0: fade-in over the last 1000ms of the 3000ms window; stored into
         * the alpha slot drawColor[3] (change+2000 > now ? 1.0 : (change-now+3000)/1000). */
        if (coduo_int32_from_bits((uint32_t)cg_stanceHintChangeTime + 2000u) > now) {
            drawColor[3] = 1.0f;                                 /* 0x3f800000 */
        } else {
            int32_t e = coduo_int32_from_bits(
                (uint32_t)cg_stanceHintChangeTime - (uint32_t)now + 3000u);
            /* 0x3002f40a FILD [ESP+0x10]; FMUL 0.001f; FSTP -- the int e is loaded
             * straight into the multiply (no FSTP DWORD first), so the implicit
             * int->float conversion must stay exact; an explicit (float) cast would
             * round e under -std=c11. */
            drawColor[3] = (float)((long double)e *
                                   (long double)0.001f); /* 0x3007bd94 */
        }

        /* 0x3002f418: query the line height via CG_R_TEXT_HEIGHT(textArgA, textArgB). */
        {
            int32_t h = coduo_int32_from_bits((uint32_t)cgame_syscall(
                CG_R_TEXT_HEIGHT, textArgA, textArgB));
            uint32_t selStanding = cg_stanceHintFlags & 0x1u;    /* AND EBX,0x1 */
            lineHeight = (float)h;

            /* Loop 1 (0x3002f46b..0x3002f512): choose, for each of the 4 stance rows,
             * the first alternative command that is actually bound to a key. The
             * table depends on the current stance context. */
            for (row = 0; row < 4; ++row) {
                char *(*table)[6];
                int col;
                chosenCmd[row] = 0;                              /* MOV [ECX],EBP */
                if (selStanding != 0u) {
                    table = cmdTableStanding;
                } else if ((cg_stanceHintFlags & STANCE_FLAG_CROUCH) != 0u) {
                    table = cmdTableCrouch;
                } else {
                    table = cmdTableProne;
                }
                for (col = 0; col < 6; ++col) {
                    char *cmd = table[row][col];
                    if (cmd == 0) {
                        break;                                   /* TEST EDI,EDI; JZ */
                    }
                    if (GetCommandHasBinding(cmd)) {
                        chosenCmd[row] = cmd;
                        ++boundCount;
                        break;
                    }
                }
            }

            /* 0x3002f518: seed the baseline: rect.h*0.5 + rect.y - 1.5, then adjust
             * by the number of chosen lines (boundCount - 1). */
            lineY = (float)((long double)iconRect->h * 0.5L +
                            (long double)iconRect->y - 1.5L);
            {
                int32_t k = boundCount - 1;
                if (k == 0) {
                    lineY = (float)((long double)lineHeight * 0.5L +
                                    (long double)lineY);
                } else if (k == 2) {
                    lineY = (float)((long double)lineY -
                                    ((long double)lineHeight * 0.5L + 1.5L));
                }
                /* other counts leave lineY unchanged (0x3002f569) */
            }

            /* Loop 2 (0x3002f580..0x3002f625): draw one hint line per chosen command. */
            for (row = 0; row < 4; ++row) {
                char *keyStr = 0;
                char *hintText;
                char *fmt;
                float x, y;

                if (chosenCmd[row] == 0) {
                    continue;                                    /* TEST ECX,ECX; JZ */
                }

                /* 0x3002f593: localized key string bound to this command. */
                UI_KeysStringForBinding(chosenCmd[row], &keyStr);
                /* 0x3002f5a1: translate the hint label, then format "<hint> <key>". */
                hintText = CG_SafeTranslateString_Internal("cgame", hintLabels[row]);
                /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
                if (client_compat_validate_format_signature(hintText, "s") ==
                    qfalse) {
                    Com_Printf("WARNING: rejected invalid stance-hint format\n");
                    fmt = hintText;
                } else {
                    fmt = (char *)va(hintText, keyStr);
                }

                /* 0x3002f5b1: x = slideX + rect.w (slide-in), y = lineY - 35.0f.
                 * (The result is written to a fresh slot -0x15c; slideX is unchanged
                 * across iterations.) */
                x = slideX + iconRect->w;                        /* rect.w */
                y = lineY - 35.0f;                               /* 0x3007c1f0 = 35 */

                /* 0x3002f5c2..0x3002f604: CG_R_TEXT_PAINT(x, y, arg2, arg3, color,
                 * text, 0, 0, arg4) — the same 9-args-after-id shape as the label
                 * draw at 0x3002f173. The cleanup ADD ESP,0x30 at 0x3002f612 covers
                 * 12 dwords: the two va() arguments left uncleaned above plus the
                 * trap id and these nine. */
                cgame_syscall(CG_R_TEXT_PAINT,
                              CG_FloatBits(x),
                              CG_FloatBits(y),
                              textArgA,
                              textArgB,
                              drawColor,
                              fmt,
                              0,
                              0,
                              textArgC);

                /* 0x3002f60a: advance the baseline by the measured line height plus
                 * the current baseline plus 1.5f (FLD lineHeight; FADD lineY; FADD 1.5). */
                lineY = (float)((long double)lineHeight +
                                (long double)lineY + 1.5L);
            }
        }
    }

    /* -------- stance icon (0x3002f633) -------- */
    {
        qhandle_t shader;
        const uint32_t f = cg_stanceHintFlags;
        if ((f & STANCE_FLAG_SPRINT) != 0u) {
            shader = cg_stanceHudShaders[HUD_SHADER_STANCE_SPRINT];
        } else if ((f & STANCE_FLAG_PRONE) != 0u) {
            shader = cg_stanceHudShaders[HUD_SHADER_STANCE_PRONE];
        } else if ((f & STANCE_FLAG_CROUCH) != 0u) {
            shader = cg_stanceHudShaders[HUD_SHADER_STANCE_CROUCH];
        } else {
            shader = cg_stanceHudShaders[HUD_SHADER_STANCE_STAND];
        }

        /* 0x3002f661: drawColor[3] = colorVec[3]; set color, then draw the icon rect
         * scaled from virtual to real screen (x = slideX*screenXScale). */
        drawColor[3] = colorVec[3];                              /* [ECX+0xc] */
        cgame_syscall(CG_R_SETCOLOR, drawColor);
        cgame_syscall(CG_R_DRAWSTRETCHPIC,
                      CG_FloatBits(slideX * cgs_screenXScale),
                      CG_FloatBits(iconRect->y * cgs_screenYScale),
                      CG_FloatBits(iconRect->w * cgs_screenXScale),
                      CG_FloatBits(iconRect->h * cgs_screenYScale),
                      CG_FloatBits(0.0f),
                      CG_FloatBits(0.0f),
                      CG_FloatBits(1.0f),
                      CG_FloatBits(1.0f),
                      shader);
    }

    /* -------- fatigue icon + partial overlay (0x3002f70a) -------- */
    {
        qhandle_t shader;
        const uint32_t f = cg_stanceHintFlags;
        if ((f & STANCE_FLAG_SPRINT) != 0u) {
            shader = cg_stanceHudShaders[HUD_SHADER_FATIGUE_SPRINT];
        } else if ((f & STANCE_FLAG_PRONE) != 0u) {
            shader = cg_stanceHudShaders[HUD_SHADER_FATIGUE_PRONE];
        } else if ((f & STANCE_FLAG_CROUCH) != 0u) {
            shader = cg_stanceHudShaders[HUD_SHADER_FATIGUE_CROUCH];
        } else {
            shader = cg_stanceHudShaders[HUD_SHADER_FATIGUE_STAND];
        }

        /* 0x3002f735: opaque-white color for the base icon. */
        drawColor[0] = 1.0f;
        drawColor[1] = 1.0f;
        drawColor[2] = 1.0f;
        drawColor[3] = 1.0f;
        cgame_syscall(CG_R_SETCOLOR, drawColor);

        /* 0x3002f762: draw the partial fatigue overlay only when the fatigue
         * fraction is > 0.0f (FCOMP against 0.0f). */
        if (cg_predictedPlayerState.fatigueScale > 0.0f) {                   /* 0x3007bcec = 0.0f */
            /* 0x3002f77c..0x3002f796 keeps 1-fatigueScale live in x87 after
             * storing its rounded float copy for the texture coordinate.  The
             * live value, not that copy, feeds the filled-height multiply. */
            long double fracWide = 1.0L -
                (long double)cg_predictedPlayerState.fatigueScale;
            float frac = (float)fracWide;                                    /* 0x3002f796 FST */
            float filledHeight = (float)((long double)iconRect->h * fracWide); /* 0x3002f7a2..0x3002f7aa */
            /* 0x3002f7a2..0x3002f7f8: the overlay covers the BOTTOM (1-frac) part of
             * the icon: y = (rect.h*frac + rect.y)*screenYScale (FLD tmp=h*frac;
             * FADD [EDI+4]; FMUL yscale), h = (rect.h - rect.h*frac)*screenYScale
             * (FLD [EDI+0xc]; FSUB tmp; FMUL yscale — positive), top texcoord
             * t1 = frac. x is the animated slide (slideX*screenXScale). */
            cgame_syscall(CG_R_DRAWSTRETCHPIC,
                          CG_FloatBits(slideX * cgs_screenXScale),
                          CG_FloatBits((float)(((long double)filledHeight +
                                               (long double)iconRect->y) *
                                              (long double)cgs_screenYScale)),
                          CG_FloatBits(iconRect->w * cgs_screenXScale),
                          CG_FloatBits((float)(((long double)iconRect->h -
                                               (long double)filledHeight) *
                                              (long double)cgs_screenYScale)),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(frac),
                          CG_FloatBits(1.0f),
                          CG_FloatBits(1.0f),
                          shader);
        }
    }

    /* -------- countdown overlay (0x3002f81f) -------- */
    now = coduo_int32_from_bits((uint32_t)cg_time); /* 0x3002f824 countdown reload */
    if (coduo_int32_from_bits((uint32_t)cg_stanceHintChangeTime + 1000u) > now) {
        int32_t e = coduo_int32_from_bits(
            (uint32_t)cg_stanceHintChangeTime - (uint32_t)now + 1000u);
        /* 0x3002f86a loads ds:0x3044bb34 = cg_stanceHudShaders[4] = hudStanceFlash
         * (the countdown "flash" overlay), NOT index 5 (hudFatigueStand). A prior
         * pass used HUD_SHADER_FATIGUE_STAND (=5, ds:0x3044bb38) here. */
        qhandle_t shader = cg_stanceHudShaders[HUD_SHADER_STANCE_FLASH];

        /* 0x3002f843..0x3002f85a: color alpha fades with the countdown =
         * e * (1/1000) * 0.8f. FILD; FMUL 0x3007bd94 (1/1000); FMUL 0x3007bdf0
         * (0.8f); FSTP -- the 1/1000 product is NEVER stored, so the whole chain
         * is one expression with a single rounding at the drawColor[3] store.
         * FILD feeds the multiply directly (no FSTP DWORD), so e stays exact --
         * no (float) cast (it would round e under -std=c11). */
        drawColor[3] = (float)((long double)e * (long double)0.001f *
                               (long double)0.8f);
        cgame_syscall(CG_R_SETCOLOR, drawColor);

        cgame_syscall(CG_R_DRAWSTRETCHPIC,
                      CG_FloatBits(slideX * cgs_screenXScale),
                      CG_FloatBits(iconRect->y * cgs_screenYScale),
                      CG_FloatBits(iconRect->w * cgs_screenXScale),
                      CG_FloatBits(iconRect->h * cgs_screenYScale),
                      CG_FloatBits(0.0f),
                      CG_FloatBits(0.0f),
                      CG_FloatBits(1.0f),
                      CG_FloatBits(1.0f),
                      shader);
    }

    /* 0x3002f8f3: reset the 2D draw color to white. */
    cgame_syscall(CG_R_SETCOLOR, (void *)0);
}
