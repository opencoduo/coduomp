/*
 * Source reconstruction for player state to entity state conversion.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "game_functions.h"
#include "compat/libm/coduo_libm.h"

#define MARK_DIR_NORMAL_MIN_LENGTH 1.0f
#define MARK_DIR_DEFAULT_DOT 0.3f
#define MARK_DIR_STEEP_NORMAL_Z 0.8f
#define MARK_DIR_STEEP_DOT 0.7f
#define MARK_DIR_NORMAL_BLEND 0.5f

/* ------------------------------------------------------------------ */
/*  0x20869  BG_GetMarkDir                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x20869, 30869_BG_GetMarkDir.c, VERIFY-WAVE3-BGSTATE-TRAJECTORY-2026-06-17): DATAFLOW_VERIFIED - normal length fallback, inverse direction normalize, steep-normal dot threshold, blend loop, and output copy checked against current decompiler output. */
void BG_GetMarkDir(const float *dir, const float *normal, float *out)
{
    float minDot = MARK_DIR_DEFAULT_DOT;
    vec3_t safeNormal;
    vec3_t markDir;
    float normalLength;

    normalLength =
        (float)CoduoLibm_Sqrt((double)((long double)normal[0] * (long double)normal[0] + (long double)normal[1] * (long double)normal[1] +
                                       (long double)normal[2] * (long double)normal[2]));
    if (normalLength < MARK_DIR_NORMAL_MIN_LENGTH) {
        safeNormal[0] = 0.0f;
        safeNormal[1] = 0.0f;
        safeNormal[2] = 1.0f;
    } else {
        safeNormal[0] = normal[0];
        safeNormal[1] = normal[1];
        safeNormal[2] = normal[2];
    }

    markDir[0] = -dir[0];
    markDir[1] = -dir[1];
    markDir[2] = -dir[2];
    VectorNormalize(markDir);

    if (normal[2] > MARK_DIR_STEEP_NORMAL_Z) {
        minDot = MARK_DIR_STEEP_DOT;
    }

    while ((long double)markDir[0] * (long double)safeNormal[0] + (long double)markDir[1] * (long double)safeNormal[1] +
               (long double)markDir[2] * (long double)safeNormal[2] <
           (long double)minDot) {
        markDir[0] = (float)((long double)markDir[0] + (long double)safeNormal[0] * MARK_DIR_NORMAL_BLEND);
        markDir[1] = (float)((long double)markDir[1] + (long double)safeNormal[1] * MARK_DIR_NORMAL_BLEND);
        markDir[2] = (float)((long double)markDir[2] + (long double)safeNormal[2] * MARK_DIR_NORMAL_BLEND);
        VectorNormalize(markDir);
    }

    out[0] = markDir[0];
    out[1] = markDir[1];
    out[2] = markDir[2];
}

/* ------------------------------------------------------------------ */
/*  0x23087  BG_GetVehiclePosTag                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x23087, 33087_BG_GetVehiclePosTag.c, VERIFY-WAVE4-BGSTATE-VEHICLE-2026-06-17): DATAFLOW_VERIFIED - seven-entry vehicle position tag table and indexed return checked against current decompiler output. */
const char *BG_GetVehiclePosTag(int vehiclePos)
{
    const char *vehiclePosTags[7] = {
        "*unused*", "tag_player", "tag_secondary_player", "tag_passenger", "tag_passenger2", "tag_passenger3", "tag_passenger4",
    };

    return vehiclePosTags[vehiclePos];
}
