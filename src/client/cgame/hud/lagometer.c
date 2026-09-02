// Source: uo_cgame_mp_x86.dll 0x30018bc0..0x30019043
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30018bc0_30019043.mcode
//
// CG_DrawLagometer -- draw the frame-time and snapshot-ping sample graphs, the
// synchronous-client marker, and the connection-interrupted warning. The
// CMD_VEH_FireTurret size-match is rejected: the two 128-entry rings and their
// masked counters prove the standard lagometer consumer.

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

enum {
    LAG_COLOR_RED = '1',
    LAG_COLOR_GREEN = '2',
    LAG_COLOR_YELLOW = '3',
    LAG_COLOR_BLUE = '4',
    LAG_COLOR_STATE_FRAME_POSITIVE = 1,
    LAG_COLOR_STATE_FRAME_NEGATIVE = 2,
    LAG_COLOR_STATE_SNAPSHOT_GOOD = 3,
    LAG_COLOR_STATE_SNAPSHOT_DROPPED = 4,
    LAG_COLOR_STATE_SNAPSHOT_DELAYED = 5,
    LAG_SNAPSHOT_DELAYED = 1
};

#define LAGOMETER_X 585.0f
#define LAGOMETER_Y 280.0f
#define LAGOMETER_WIDTH 48.0f
#define LAGOMETER_HEIGHT 48.0f
/* The machine multiplies by rounded-reciprocal .rdata constants; it never
 * divides. FMUL [0x3007bf80]=0x3eaaaaab (~1/3), FMUL [0x3007c288]=0x3b5a740e
 * (~1/300), FMUL [0x3007be00]=0x3a91a2b4 (~1/900). Dividing by 3/300/900
 * instead is 1 ULP off for many inputs, so the reciprocals are kept literal.
 * The snapshot-range halving really is exact (FMUL 0.5f @0x3007bce8). */
#define LAGOMETER_FRAME_RANGE_RECIP 0.33333334f    /* 0x3eaaaaab */
#define LAGOMETER_FRAME_SCALE_RECIP 0.0033333334f  /* 0x3b5a740e */
#define LAGOMETER_SNAPSHOT_RANGE_FACTOR 0.5f       /* 0x3007bce8 */
#define LAGOMETER_SNAPSHOT_SCALE_RECIP 0.0011111111f /* 0x3a91a2b4 */

/* NOT_FROM_ORIGINAL_SOURCE: isolated widescreen presentation interface. */
extern float cgame_compat_right_hud_virtual_offset(void);

void CG_DrawLagometer(void)
{
    if (cg_lagometer_vmCvar.integer != qfalse && cgs_localServer == 0) {
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): preserve the
         * recovered graph as one fixed-size composition while translating its
         * stock right-side anchor to the native widescreen edge. */
        const float lagometerX =
            LAGOMETER_X + cgame_compat_right_hud_virtual_offset();
        float ax;
        float ay;
        float aw;
        float ah;
        float range;
        float mid;
        float vscale;
        vec4_t color;
        int32_t currentColorState = -1;

        trap_R_SetColor(NULL);
        qhandle_t lagometerShader = cgs_lagometerShader;
        CG_DrawPic(lagometerX, LAGOMETER_Y,
                   LAGOMETER_WIDTH, LAGOMETER_HEIGHT,
                   lagometerShader);

        /* 0x30018c08..0x30018c4c read the four screen scales only after the
         * set-color and background-picture calls have returned. */
        ax = (float)((long double)cgs_screenXScale *
                     (long double)lagometerX);
        ay = (float)((long double)cgs_screenYScale *
                     (long double)LAGOMETER_Y);
        aw = (float)((long double)cgs_screenXScale *
                     (long double)LAGOMETER_WIDTH);
        ah = (float)((long double)cgs_screenYScale *
                     (long double)LAGOMETER_HEIGHT);
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): retail draws one
         * ring entry per physical pixel.  Modern high-density drawables can
         * make this 48-unit box wider than the 128-entry ring, which wraps the
         * index and redraws the same history two or three times.  Use every
         * entry at most once and widen adjacent columns to retain the box's
         * physical width.  At widths up to LAG_SAMPLES this remains the exact
         * one-pixel retail presentation. */
        float renderedSampleLimit = aw;
        float renderedSampleStep = 1.0f;
        float renderedColumnWidth = 1.0f;
        if (aw > (float)LAG_SAMPLES) {
            renderedSampleLimit = (float)LAG_SAMPLES;
            renderedSampleStep = aw / (float)LAG_SAMPLES;
            renderedColumnWidth = renderedSampleStep;
        }

        /* 0x30018c5a is FST (keep), not FSTP: `range` is stored rounded and then
         * RELOADED at 0x30018c66 for vscale, but `mid` at 0x30018c5e adds ay to the
         * UNROUNDED ah*(1/3) still in st0. rangeRaw carries that 80-bit value. */
        {
            long double rangeRaw =
                (long double)ah *
                (long double)LAGOMETER_FRAME_RANGE_RECIP;
            range = (float)rangeRaw;               /* 0x30018c5a FST  [ESP+0xc]  */
            mid = (float)((long double)ay + rangeRaw); /* 0x30018c5e FADD [ESP+0x20] */
        }
        vscale = (float)((long double)range *
                         (long double)LAGOMETER_FRAME_SCALE_RECIP); /* 0x30018c66 reload */

        int32_t a = 0;
        for (;;) {
            /* The loop tail FILDs a, stores it to binary32, and compares that
             * stored float with aw. The first iteration uses the same +0.0 bits. */
            float aFloat = (float)a;
            if (!(aFloat < renderedSampleLimit)) {
                break;
            }
            uint32_t indexBits = (uint32_t)cg_lagometerFrameCount -
                                 (uint32_t)a - 1u;
            int32_t index = (int32_t)(indexBits & (uint32_t)(LAG_SAMPLES - 1));
            long double valueRaw =
                (long double)cg_lagometerFrameSamples[index] *
                (long double)vscale;
            float value = (float)valueRaw;

            if (valueRaw > 0.0L) {
                if (currentColorState != LAG_COLOR_STATE_FRAME_POSITIVE) {
                    currentColorState = LAG_COLOR_STATE_FRAME_POSITIVE;
                    cgame_syscall(CG_CL_LOOKUP_COLOR, LAG_COLOR_YELLOW,
                                  (intptr_t)&color[0]);
                    trap_R_SetColor(color);
                }
                if (value > range) {
                    value = range;
                }
                const long double sampleOffset =
                    (long double)aFloat * (long double)renderedSampleStep;
                float drawX = (float)((long double)ax + (long double)aw -
                                      sampleOffset);
                float drawY = (float)((long double)mid - (long double)value);
                trap_R_DrawStretchPic(CG_FloatBits(drawX),
                                      CG_FloatBits(drawY),
                                      CG_FloatBits(renderedColumnWidth),
                                      CG_FloatBits(value),
                                      CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                                      CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                                      cgs_media_whiteShader);
            } else if (value < 0.0f) {
                value = -value;
                if (currentColorState != LAG_COLOR_STATE_FRAME_NEGATIVE) {
                    currentColorState = LAG_COLOR_STATE_FRAME_NEGATIVE;
                    cgame_syscall(CG_CL_LOOKUP_COLOR, LAG_COLOR_BLUE,
                                  (intptr_t)&color[0]);
                    trap_R_SetColor(color);
                }
                if (value > range) {
                    value = range;
                }
                const long double sampleOffset =
                    (long double)aFloat * (long double)renderedSampleStep;
                float drawX = (float)((long double)ax + (long double)aw -
                                      sampleOffset);
                trap_R_DrawStretchPic(CG_FloatBits(drawX),
                                      CG_FloatBits(mid),
                                      CG_FloatBits(renderedColumnWidth),
                                      CG_FloatBits(value),
                                      CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                                      CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                                      cgs_media_whiteShader);
            }
            a = coduo_int32_from_bits((uint32_t)a + 1u);
        }

        /* 0x30018dd6 is FST (keep), not FSTP, and unlike the frame section above the
         * reload is absent: vscale's FMUL at 0x30018dda multiplies the UNROUNDED
         * ah*0.5f still in st0. `range` itself is the rounded copy. */
        {
            long double rangeRaw =
                (long double)ah *
                (long double)LAGOMETER_SNAPSHOT_RANGE_FACTOR;
            range = (float)rangeRaw;                            /* 0x30018dd6 FST */
            vscale = (float)(rangeRaw *
                (long double)LAGOMETER_SNAPSHOT_SCALE_RECIP);   /* 0x30018dda FMUL */
        }
        a = 0;
        for (;;) {
            float aFloat = (float)a;
            if (!(aFloat < renderedSampleLimit)) {
                break;
            }
            uint32_t indexBits = (uint32_t)cg_lagometer.snapshotCount -
                                 (uint32_t)a - 1u;
            int32_t index = (int32_t)(indexBits & (uint32_t)(LAG_SAMPLES - 1));
            long double valueRaw =
                (long double)cg_lagometer.snapshotSamples[index];
            float value = (float)valueRaw;

            if (valueRaw > 0.0L) {
                int32_t wantedColorState =
                    ((cg_lagometer.snapshotFlags[index].bytes[0] &
                      LAG_SNAPSHOT_DELAYED) != 0)
                        ? LAG_COLOR_STATE_SNAPSHOT_DELAYED
                        : LAG_COLOR_STATE_SNAPSHOT_GOOD;
                if (currentColorState != wantedColorState) {
                    currentColorState = wantedColorState;
                    cgame_syscall(CG_CL_LOOKUP_COLOR,
                                  wantedColorState == LAG_COLOR_STATE_SNAPSHOT_DELAYED
                                      ? LAG_COLOR_YELLOW : LAG_COLOR_GREEN,
                                  (intptr_t)&color[0]);
                    trap_R_SetColor(color);
                }
                /* 0x30018e7f FMUL then 0x30018e83 FCOM vs range: the clamp compares
                 * the UNROUNDED product; the only FST of value is at 0x30018e9a,
                 * AFTER the clamp has selected between the product and range. */
                long double drawnHeightRaw =
                    (long double)value * (long double)vscale;
                if (drawnHeightRaw > (long double)range) {
                    drawnHeightRaw = (long double)range;
                }
                float drawnHeight = (float)drawnHeightRaw;
                float drawY = (float)((long double)ah + (long double)ay -
                                      drawnHeightRaw);
                const long double sampleOffset =
                    (long double)aFloat * (long double)renderedSampleStep;
                float drawX = (float)((long double)ax + (long double)aw -
                                      sampleOffset);
                trap_R_DrawStretchPic(CG_FloatBits(drawX),
                                      CG_FloatBits(drawY),
                                      CG_FloatBits(renderedColumnWidth),
                                      CG_FloatBits(drawnHeight),
                                      CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                                      CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                                      cgs_media_whiteShader);
            } else if (value < 0.0f) {
                if (currentColorState != LAG_COLOR_STATE_SNAPSHOT_DROPPED) {
                    currentColorState = LAG_COLOR_STATE_SNAPSHOT_DROPPED;
                    cgame_syscall(CG_CL_LOOKUP_COLOR, LAG_COLOR_RED,
                                  (intptr_t)&color[0]);
                    trap_R_SetColor(color);
                }
                float drawY = (float)((long double)ah + (long double)ay -
                                      (long double)range);
                const long double sampleOffset =
                    (long double)aFloat * (long double)renderedSampleStep;
                float drawX = (float)((long double)ax + (long double)aw -
                                      sampleOffset);
                trap_R_DrawStretchPic(CG_FloatBits(drawX),
                                      CG_FloatBits(drawY),
                                      CG_FloatBits(renderedColumnWidth),
                                      CG_FloatBits(range),
                                      CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                                      CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                                      cgs_media_whiteShader);
            }
            a = coduo_int32_from_bits((uint32_t)a + 1u);
        }

        trap_R_SetColor(NULL);
        if (cg_nopredict_vmCvar.integer != qfalse ||
            g_synchronousClients_vmCvar.integer != qfalse) {
            /* 0x30018ffc converts ay first while staging the reverse-order
             * call arguments, then converts ax at 0x3001901e. */
            float labelY = (float)coduo_fp_to_i32_extended((long double)ay);
            float labelX = (float)coduo_fp_to_i32_extended((long double)ax);
            CG_DrawBigString(labelX, labelY, cg_lagometerSyncLabel, 1.0f);
        }
    }

    CG_DrawDisconnect();
}
