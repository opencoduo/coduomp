// Source: uo_cgame_mp_x86.dll 0x30018090..0x30018727
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30018090_30018727.mcode
//
// CG_DrawFPS — the cgame frame-timing + renderer performance overlay. It is the
// right-aligned debug HUD that shows the fps line and, when the stats detail level
// is high enough, the renderer counters (tris / verts / prims / ents / memory /
// draw-calls). Takes the current vertical text position `y` (float, the sole stack
// argument), draws each line right-justified at x = 620, advancing `y` downward,
// and returns the updated `y` (float in ST0) so the caller can continue the HUD
// column below it.
//
// Naming: the .mcode header size-guess name `CG_DrawWeapReticle` is REJECTED. This
// routine registers no shader and draws no reticle/scope 2D pic; it maintains a
// 32-sample frame-time ring and prints renderer statistics via va() + the glyph
// string helpers. The behavior — %ifps(...) frame timing plus "%i/%i tris",
// "%i vert", "%i prim", "%i ents", "%.2f/%.2f/%.2f mb", "%.2f dc" — is the classic
// id-Tech / CoD renderer-stats (r_speeds-style) overlay. The Mac cgame symbol
// CG_DrawFPS has the identical three named direct callees (CG_DrawBigString,
// CG_DrawSmallString, and va), resolving the source name.
//
// Data model (all resolved off this function's machine code; see globals.h):
//   cg_statsFrameTimes[32] (0x300a8438) — rolling per-frame elapsed-ms ring.
//   cg_statsFrameCount     (0x300a84bc) — running sample counter; <32 == warmup.
//   cg_statsPrevTimeMs     (0x300a84c0) — previous trap_Milliseconds() snapshot.
//   cg_rendererStats       (0x300a84c4) — renderer_frame_statistics_t, filled via CG_R_TRACK_STATISTICS.
//   cg_drawFPS_vmCvar.integer      (0x3044f18c) — detail level; >1 enables the detailed lines.
//
// Callees / traps:
//   cgame_syscall(CG_MILLISECONDS)            -> trap_Milliseconds() engine ms.
//   cgame_syscall(CG_R_TRACK_STATISTICS, &stats)  -> fill cg_rendererStats.
//   va(fmt, ...)               (0x3004e8a0) — ring-buffer string formatter.
//   CG_DrawBigString  (0x3001cf10)      — 16px glyph draw (fps line).
//   CG_DrawSmallString(0x3001cff0)      — 8px glyph draw (detailed lines).
//
// Machine-code notes / self-check (each branch, width, sign, x87 op, const checked):
//   - Ring update (0x30018090..0x300180e2): sample = trap_Milliseconds() -
//     cg_statsPrevTimeMs; cg_statsPrevTimeMs = now. idx = cg_statsFrameCount, then
//     idx &= 0x8000001f as a SIGNED mod-32 (AND keeps sign bit; if negative,
//     DEC/OR 0xffffffe0/INC restores the two's-complement remainder). Store
//     cg_statsFrameTimes[idx] = sample; cg_statsFrameCount++; if the new count < 32
//     (JL 0x3001871d) bail early, returning the unchanged `y`.
//   - fps aggregate (0x300180e8..0x30018146): a single pass sums every sample into
//     `sum` and tracks the min (EDX, seeded 0x7fffffff, kept via CMP/JLE) and max
//     (ESI, seeded 0, kept via CMP/JGE) frame time. The unrolled loop reads 4
//     samples per +0x10 step over the 32-entry array.
//   - avgMs = sum * (1/32) (FMUL 0x3007bf3c = 0.03125f) held in a scratch.
//   - jitter (0x30018170..0x300181ec): second unrolled pass accumulates
//     Sum |sample - avgMs| (FSUB avgMs; FABS; FADDP), then avgDeviation =
//     that * (1/32); FISTP -> jitterInt.
//   - clamps: if sum==0 sum=1; if min<=0 min=1 (avoid divide-by-zero).
//   - display integers via FISTP (round-to-nearest, the tiny +2^-30 double bias at
//     0x3007be50 is a negligible rounding nudge; modeled with rint):
//       fpsInt  = rint(32000.0f / sum)            (FDIVR 0x3007c044 = 32000.0f)
//       minStat = rint(1000.0f / (float)max)  (FDIVR 0x3007be88 = 0x447a0000 = 1000.0f)
//       maxStat = rint(1000.0f / (float)min)  (FDIVR 0x3007be88 = 0x447a0000 = 1000.0f)
//     printed as va("%ifps(%i-%i,%i)", fpsInt, minStat, maxStat, jitterInt).
//   - each line is drawn right-justified: x = 620.0f - visibleLen*cellW, where
//     visibleLen counts glyphs skipping id-Tech "^<digit>" color escapes (the inline
//     loop at each 0x...5e/0x30/0x39 compare), cellW = 16.0f for the big fps line
//     (0x3007bf00) and 8.0f for the small detailed lines (0x3007be08 = 0x41000000).
//     `y` advances by 2.0f then +20.0f after the fps line, and by 1.0f then +16.0f
//     after each small line (the FADD constants 0x3007bce4 (2.0f) / 0x3007be04
//     (0x41a00000 = 20.0f) and 0x3007bce0 (1.0f) / 0x3007bf00 (16.0f)).
//   - tris line: indexCount/3 and drawnIndexCount/3 (IMUL 0x55555556 = signed /3).
//   - mb line: three byte counts * 2^-20 (FMUL 0x3007c03c = 9.5367431640625e-07 =
//     1/1048576), printed as doubles ("%.2f/%.2f/%.2f mb  "); the middle size is
//     (imageMemory - lightmapMemory).
//   - dc line (0x30018670..0x30018705) is drawn only if cg_rendererStats.overdrawRatio
//     != 0.0f (FUCOMPP against 0.0f at 0x3007bcec; TEST AH,0x44 / JNP skips on
//     ordered equality only; unordered values continue into the draw path); its
//     value is printed as a double ("%.2f dc  ").
//   - both exit paths reload `y` into ST0 (the float return value).

#include <limits.h>
#include <math.h>

#include "../client_recovered.h"
#include "../globals.h"

/* NOT_FROM_ORIGINAL_SOURCE: isolated widescreen presentation interface. */
extern float cgame_compat_right_hud_virtual_offset(void);

/* trap id 6: no-argument engine-milliseconds query (see CG_MILLISECONDS in
 * client_recovered.h). Named by proven role at this call site. */
/* NOT_FROM_ORIGINAL_SOURCE: inline native-width adapter for the original direct
 * CG_MILLISECONDS syscall and its signed target-dword return. */
static inline int32_t cgame_compat_stats_milliseconds(void)
{
    return coduo_int32_from_bits((uint32_t)cgame_syscall(CG_MILLISECONDS));
}

/* Right-alignment x: the FISTP-to-int display values above and this width count use
 * the same inline "^<digit>" color-escape-skipping visible-length walk the machine
 * code inlines at each draw site (0x300182c1, 0x30018390, ...). Faithful to the
 * per-char loop: a '^' followed by a non-'^' ASCII digit '0'..'9' is a 2-byte color
 * escape that is skipped without counting; any other byte counts as one glyph. */
/* NOT_FROM_ORIGINAL_SOURCE: source factoring of the six identical inline byte
 * walks in CG_DrawFPS; the lower-case compatibility name cannot be mistaken for
 * an original out-of-line cgame function. */
static inline int32_t cgame_compat_stats_visible_len(const char *s)
{
    int32_t count = 0;
    if (*s == '\0') {
        return 0;
    }
    while (*s != '\0') {
        if ((int8_t)s[0] == '^') {
            int8_t next = (int8_t)s[1];
            if (next != '\0' && next != '^' &&
                next >= '0' && next <= '9') {
                s += 2;           /* color escape: skip both bytes, do not count */
                continue;
            }
        }
        count++;
        s++;
    }
    return count;
}

/* FISTP with the default (round-to-nearest) x87 control word. The executable adds
 * its exact +2^-30 double bias after reloading each rounded float quotient. */
#define CG_STATS_FISTP_BIAS 0.000000000931322574615478515625L

float CG_DrawFPS(float y)
{
    int32_t now;
    int32_t sample;
    int32_t idx;
    int32_t i;
    int32_t sum;
    int32_t minFrameTime;
    int32_t maxFrameTime;
    float avgMs;
    float avgDeviation;
    int32_t fpsInt;
    int32_t minStat;
    int32_t maxStat;
    int32_t jitterInt;
    const char *text;
    float x;

    /* --- ring update -------------------------------------------------------- */
    now = cgame_compat_stats_milliseconds();
    sample = coduo_int32_from_bits(
        (uint32_t)now - (uint32_t)cg_statsPrevTimeMs);
    cg_statsPrevTimeMs = now;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    idx = (int32_t)((uint32_t)cg_statsFrameCount & 31u);
    cg_statsFrameTimes[idx] = sample;
    cg_statsFrameCount = coduo_int32_from_bits(
        (uint32_t)cg_statsFrameCount + 1u);
    if ((uint32_t)cg_statsFrameCount < 32u) {
        return y; /* warmup: not enough samples yet */
    }

    /* --- fps aggregate: sum, min, max over the 32-sample ring --------------- */
    sum = 0;
    minFrameTime = 0x7fffffff;
    maxFrameTime = 0;
    for (i = 0; i < 32; i++) {
        int32_t ft = cg_statsFrameTimes[i];
        sum = coduo_int32_from_bits((uint32_t)sum + (uint32_t)ft);
        if (minFrameTime > ft) {
            minFrameTime = ft;
        }
        if (maxFrameTime < ft) {
            maxFrameTime = ft;
        }
    }

    avgMs = (float)((long double)sum * (long double)0.03125f);

    /* --- jitter: mean absolute deviation of the samples from avgMs ---------- */
    {
        long double devSum = 0.0f; /* seeded from FLD 0.0f at 0x3007bcec */
        for (i = 0; i < 32; i++) {
            devSum += fabsl((long double)cg_statsFrameTimes[i] -
                            (long double)avgMs);
        }
        avgDeviation =
            (float)(devSum * (long double)0.03125f);
    }

    if (sum == 0) {
        sum = 1;
    }
    if (minFrameTime <= 0) {
        minFrameTime = 1;
    }

    /* 0x30018210/0x30018234/0x30018258: the divisor is FILD'd straight into the
     * FDIVR (no FSTP DWORD), so sum/max/min stay exact -- no (float) casts. */
    const float fpsValue = (float)(
        (long double)32000.0f / (long double)sum);
    const float minValue = (float)(
        (long double)1000.0f / (long double)maxFrameTime);
    const float maxValue = (float)(
        (long double)1000.0f / (long double)minFrameTime);
    fpsInt = coduo_x87_fistp_i32(
        (long double)fpsValue + CG_STATS_FISTP_BIAS);
    minStat = coduo_x87_fistp_i32(
        (long double)minValue + CG_STATS_FISTP_BIAS);
    maxStat = coduo_x87_fistp_i32(
        (long double)maxValue + CG_STATS_FISTP_BIAS);
    jitterInt = coduo_x87_fistp_i32(
        (long double)avgDeviation + CG_STATS_FISTP_BIAS);

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): keep the recovered FPS
     * column intact and translate its authored right edge to the native
     * widescreen edge. */
    const float rightAnchor =
        620.0f + cgame_compat_right_hud_virtual_offset();

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): cg_drawFPSMode 1 keeps
     * the original 32-frame aggregate but presents only its rounded number. Use
     * the stock big-string path so the compact readout retains its built-in
     * one-pixel black shadow and does not reduce the glyphs to the blurry,
     * unshadowed 8-pixel debug font. cg_drawFPS still owns visibility. */
    if (cgame_compat_uses_simple_fps_display() != qfalse) {
        text = va("%i", fpsInt);
        x = (float)((long double)rightAnchor -
                    (long double)cgame_compat_stats_visible_len(text) *
                        (long double)16.0f);
        CG_DrawBigString(
            x, (float)((long double)y + (long double)2.0f), text, 1.0f);
        return (float)((long double)y + (long double)20.0f);
    }

    /* --- fps line (big glyphs) ---------------------------------------------- */
    text = va("%ifps(%i-%i,%i)", fpsInt, minStat, maxStat, jitterInt);
    int32_t visibleLength = cgame_compat_stats_visible_len(text);
    float drawY = (float)((long double)y + (long double)2.0f);
    x = (float)((long double)rightAnchor -
                (long double)visibleLength *
                    (long double)16.0f);
    CG_DrawBigString(x, drawY, text, 1.0f);               /* +2.0f = 0x3007bce4 */
    int32_t detailLevel = cg_drawFPS_vmCvar.integer;
    y = (float)((long double)y + (long double)20.0f);     /* 0x3007be04 */

    /* Only the detailed renderer lines below are gated by the detail level. */
    if (detailLevel <= 1) {
        return y;
    }

    /* Fetch the renderer performance counters for this pass. */
    cgame_syscall(CG_R_TRACK_STATISTICS, (intptr_t)&cg_rendererStats);

    /* --- "%i/%i tris" -------------------------------------------------------- */
    /* Push order (0x3001835b push [c8]/3, 0x30018370 push [c4]/3, then fmt) makes
     * the first "%i" = indexCount/3 (0x84c4) and the second = drawnIndexCount/3 (0x84c8). */
    text = va("%i/%i tris", cg_rendererStats.indexCount / 3,
              cg_rendererStats.drawnIndexCount / 3);
    x = (float)((long double)rightAnchor -
                (long double)cgame_compat_stats_visible_len(text) *
                    (long double)8.0f);
    CG_DrawSmallString(x, (float)((long double)y + (long double)1.0f),
                       text, 1.0f);
    y = (float)((long double)y + (long double)16.0f);

    /* --- "%i vert" ----------------------------------------------------------- */
    text = va("%i vert", cg_rendererStats.vertexCount);
    x = (float)((long double)rightAnchor -
                (long double)cgame_compat_stats_visible_len(text) *
                    (long double)8.0f);
    CG_DrawSmallString(x, (float)((long double)y + (long double)1.0f),
                       text, 1.0f);
    y = (float)((long double)y + (long double)16.0f);

    /* --- "%i prim" ----------------------------------------------------------- */
    text = va("%i prim", cg_rendererStats.drawCallCount);
    x = (float)((long double)rightAnchor -
                (long double)cgame_compat_stats_visible_len(text) *
                    (long double)8.0f);
    CG_DrawSmallString(x, (float)((long double)y + (long double)1.0f),
                       text, 1.0f);
    y = (float)((long double)y + (long double)16.0f);

    /* --- "%i ents" ----------------------------------------------------------- */
    text = va("%i ents", cg_rendererStats.entityCount);
    x = (float)((long double)rightAnchor -
                (long double)cgame_compat_stats_visible_len(text) *
                    (long double)8.0f);
    CG_DrawSmallString(x, (float)((long double)y + (long double)1.0f),
                       text, 1.0f);
    y = (float)((long double)y + (long double)16.0f);

    /* --- "%.2f/%.2f/%.2f mb  " ---------------------------------------------- */
    {
        /* Each byte count is scaled to megabytes by * 2^-20 (0x3007c03c =
         * 1/1048576) and printed as a double. The three FSTP-qword slots and the
         * push order (0x300185bd..0x300185ea) put the args, in order, as:
         *   1st %f = textureMemory                           (0x300185db, [dc])
         *   2nd %f = imageMemory - lightmapMemory (0x300185b0 SUB)
         *   3rd %f = imageMemory                             (0x300185bd, [d4]) */
        /* 0x300185bd/0x300185cd/0x300185db: each byte count is FILD'd straight
         * into FMUL 2^-20 (no FSTP DWORD), so the integer stays exact in the
         * 80-bit multiply. (float) casts would round the byte counts, which
         * diverge once a count exceeds 2^24 (16 MB) -- the common case for
         * renderer memory. The (double) narrowing is the FSTP QWORD to the va arg. */
        int32_t imageMemory = cg_rendererStats.imageMemory;
        int32_t lightmapMemory = cg_rendererStats.lightmapMemory;
        int32_t nonLightmapImageMemory = coduo_int32_from_bits(
            (uint32_t)imageMemory - (uint32_t)lightmapMemory);
        double imageMemoryMb = (double)(
            (long double)cg_rendererStats.imageMemory *
            (long double)9.5367431640625e-07f);
        double nonLightmapImageMemoryMb = (double)(
            (long double)nonLightmapImageMemory *
            (long double)9.5367431640625e-07f);
        double textureMemoryMb = (double)(
            (long double)cg_rendererStats.textureMemory *
            (long double)9.5367431640625e-07f);
        text = va("%.2f/%.2f/%.2f mb  ", textureMemoryMb,
                  nonLightmapImageMemoryMb, imageMemoryMb);
    }
    x = (float)((long double)rightAnchor -
                (long double)cgame_compat_stats_visible_len(text) *
                    (long double)8.0f);
    CG_DrawSmallString(x, (float)((long double)y + (long double)1.0f),
                       text, 1.0f);
    y = (float)((long double)y + (long double)16.0f);

    /* --- "%.2f dc  " (only when depth complexity is non-zero) ---------------- */
    if (cg_rendererStats.overdrawRatio != 0.0f) { /* FUCOMPP vs 0.0f @0x3007bcec */
        text = va("%.2f dc  ", (double)cg_rendererStats.overdrawRatio);
        x = (float)((long double)rightAnchor -
                    (long double)cgame_compat_stats_visible_len(text) *
                        (long double)8.0f);
        CG_DrawSmallString(x, (float)((long double)y + (long double)1.0f),
                           text, 1.0f);
        y = (float)((long double)y + (long double)16.0f);
    }

    return y;
}
