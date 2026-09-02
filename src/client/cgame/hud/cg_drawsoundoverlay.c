// Source: uo_cgame_mp_x86.dll 0x3001acc0..0x3001af0e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001acc0_3001af0e.mcode

#include "../client_recovered.h"
#include "../globals.h"
#include "qcommon/sound_types.h"

#include <stdint.h>

/*
 * CG_DrawSoundOverlay (0x3001acc0) — the developer sound-system debug overlay
 * (info overlay "A", entered by the per-frame dispatcher CG_DrawInfoScreens at
 * 0x3001b360 when its enable flag cg_drawSoundOverlay_vmCvar.integer is set).
 * It queries the Miles Sound System via cgame trap 222, then draws one header
 * line plus one line per active sound channel as 2D text.
 *
 * Name adjudication: the .mcode header's size-match guess "Info_SetValueForKey"
 * (win size 0x24e ~= corpus 0x24d) is REJECTED — a pure size collision with no
 * behavioral basis (the contract forbids size-based naming). This routine sets
 * no info-string key: it issues a sound-system query trap, reads the "mss_*"
 * Miles Sound System cvars, and draws diagnostic text. The Mac
 * CG_DrawSoundOverlay performs the corresponding sound query and diagnostic
 * text path, resolving the source name.
 *
 * Trap 222 (CG_MSS_GET_SOUND_OVERLAY = 0xde), proven from the entry frame
 *   (LEA &cpuValue; PUSH &cpuValue; PUSH 0x40; LEA &channels[0]; PUSH it;
 *    PUSH flag; PUSH 0xde; CALL cgame_syscall; MOV EBX,EAX):
 *     cgame_syscall(222, flag, channelArray, 64, &cpuValue) -> channelCount.
 * The immediate is a 64-record capacity, not a byte count: the original stack
 * reserves 64 * 0x14 bytes for this array. The syscall fills per-channel info
 * records and a scalar CPU-usage value, returning the number of channels.
 * `flag` is the same dword the dispatcher tested to reach this overlay
 * (cg_drawSoundOverlay_vmCvar.integer); it is forwarded verbatim as the query
 * selector. When the count is <= 0 the routine draws nothing (JLE past the whole
 * body) and returns.
 *
 * Header line: the four Miles Sound System cvars are read via
 * trap_Cvar_VariableStringBuffer (trap 0xb) into 0x400-byte buffers —
 * "mss_3d_provider" (string), "mss_bits" / "mss_khz" / "mss_stereo" (parsed with
 * Q_atoi, 0x3005b6ce). The channel count shown is 2 when mss_stereo is nonzero,
 * else 1 (SETNZ + INC). These plus cpuValue are formatted through va() into
 * "CPU: ^3%%%i ^73D provider: ^3%s ^7bits: ^3%i ^7kHz: ^3%i ^7chan: ^3%i"
 * (0x30076c40) and drawn at (x=2, y=82) by CG_EmitTrap54DrawScaled (the trap-54
 * 2D-text emitter, screen-rescale on).
 *
 * Per-channel lines: for each of the `count` channel structs (stride 0x14), a
 * line is drawn at x=2 with y stepping 98.0 + 16.0*i. An empty channel (NULL
 * soundFile) prints just its index via "%2i" (0x30076c3c). A live channel prints
 * the file and float logical-volume/relative-volume/pitch-scale values plus the
 * integer base playback rate via
 * "%2i %-50s vol:^3%04.2f ^7rvol:^3%04.2f ^7pit:^3%04.2f ^7hz:^3%5i"
 * (0x30076bf8). The three float fields are promoted to double for the variadic
 * va() call (the machine code FSTPs them as `double`), matching the %f format.
 * Each line's draw is the same fixed trap-54 frame the header uses — the compiler
 * inlined CG_EmitTrap54DrawScaled(modeFlag=1, adjustFlag=0, &white, x=2, y, str,
 * width=8, height=16, extra=0) into the loop. The inlined copy's ROUNDING
 * STRUCTURE deviates from the out-of-line body in one place (the y-sum multiplies
 * the float-ROUNDED 1/screenYScale, FSTP 0x3001ae82 / FMUL 0x3001ae90, where the
 * callee keeps it 80-bit), so the loop emits the trap frame directly instead of
 * calling the function; see the inline comments for the per-value evidence.
 *
 * Screen scale: CG_EmitTrap54DrawScaled(adjustFlag=0) divides x/width by
 * cgs_screenXScale (0x30447aa4) and the y-sum/height-scale by cgs_screenYScale
 * (0x30447aa8), the CG_AdjustFrom640 rescale. The colour pointer passed is the
 * shared white .rdata constant cg_colorWhite (0x30072034), start of
 * a {1,1,1,1} RGBA. 1.0f is the .rdata pool constant at 0x3007bce0.
 *
 * /GS: a stack cookie from the __security_cookie global (0x30081650) is saved on
 * entry and validated through __security_check_cookie (0x30061639) before RET.
 */

/* Fixed layout / draw constants, proven from the pushed immediates. */
enum {
    CG_SNDINFO_MAX_CHANNELS = 64, /* PUSH 0x40: record capacity              */
    CG_SNDINFO_CVAR_SIZE = 0x400  /* PUSH 0x400: cvar string buffer size     */
};
#define CG_SNDINFO_HEADER_X 2.0f   /* header line x                 */
#define CG_SNDINFO_HEADER_Y 82.0f   /* header line y                 */
#define CG_SNDINFO_LINE_X 2.0f   /* per-channel line x            */
#define CG_SNDINFO_LINE_Y0 98.0f   /* first per-channel line y (0x42c40000) */
#define CG_SNDINFO_LINE_STEP 16.0f   /* line pitch (0x3007bf00)       */
#define CG_SNDINFO_TEXT_W 8.0f   /* text width / char size        */
#define CG_SNDINFO_TEXT_H 16.0f   /* text height                   */

void CG_DrawSoundOverlay(void)
{
    mss_sound_overlay_t channels[CG_SNDINFO_MAX_CHANNELS];
    int32_t cpuValue;

    /* Query the sound system: fills `channels` + `cpuValue`, returns active count.
     * The query selector is the overlay's own enable dword (forwarded verbatim). */
    int32_t overlayType = cg_drawSoundOverlay_vmCvar.integer;
    int32_t count =
        (int32_t)cgame_syscall(CG_MSS_GET_SOUND_OVERLAY, overlayType, (intptr_t)channels, CG_SNDINFO_MAX_CHANNELS, (intptr_t)&cpuValue);
    if (count <= 0) {
        return;
    }

    /* Header line: Miles Sound System cvars + CPU value. */
    char provider[CG_SNDINFO_CVAR_SIZE];
    char bitsStr[CG_SNDINFO_CVAR_SIZE];
    char khzStr[CG_SNDINFO_CVAR_SIZE];
    char stereoStr[CG_SNDINFO_CVAR_SIZE];

    trap_Cvar_VariableStringBuffer(mss_3dProviderCvarName, provider, CG_SNDINFO_CVAR_SIZE);
    trap_Cvar_VariableStringBuffer(mss_bitsCvarName, bitsStr, CG_SNDINFO_CVAR_SIZE);
    int32_t bits = coduo_crt_atoi(bitsStr);
    trap_Cvar_VariableStringBuffer(mss_khzCvarName, khzStr, CG_SNDINFO_CVAR_SIZE);
    int32_t khz = coduo_crt_atoi(khzStr);
    trap_Cvar_VariableStringBuffer(mss_stereoCvarName, stereoStr, CG_SNDINFO_CVAR_SIZE);
    int32_t stereo = coduo_crt_atoi(stereoStr);
    qboolean stereoEnabled = stereo != 0;
    /* 0x3001ada0 loads cpuValue between TEST stereo and SETNZ. */
    int32_t cpuForHeader = cpuValue;
    int32_t chan = stereoEnabled + 1;

    const char *headerText = va(cg_soundDebugHeaderFormat, cpuForHeader, provider, bits, khz, chan);
    CG_EmitTrap54DrawScaled(1, 0, cg_colorWhite, CG_SNDINFO_HEADER_X, CG_SNDINFO_HEADER_Y, (void *)headerText, CG_SNDINFO_TEXT_W,
                            CG_SNDINFO_TEXT_H, 0);

    /* One line per channel, y stepping by 16.0. */
    int32_t i = 0;
    float y = CG_SNDINFO_LINE_Y0;
    const mss_sound_overlay_t *ch = channels;
    while (i < count) {
        const char *lineText;
        const char *soundFile = ch->soundFile;

        if (soundFile == NULL) {
            /* Inactive channel: index only. */
            lineText = va(cg_soundChannelIndexFormat, i);
        } else {
            /* 0x3001ae17..0x3001ae33: base rate is loaded first, followed by
             * pitch, relative volume, and logical volume conversions to double.
             * The already-tested soundFile pointer remains the %s argument. */
            int32_t basePlaybackRate = ch->basePlaybackRate;
            double pitchScale = (double)ch->pitchScale;
            double relativeVolume = (double)ch->relativeVolume;
            double logicalVolume = (double)ch->logicalVolume;
            lineText = va(cg_soundChannelDebugFormat, i, soundFile, logicalVolume, relativeVolume, pitchScale, basePlaybackRate);
        }

        /* Inlined CG_EmitTrap54DrawScaled(1, 0, &white, x=2, y, text, 8, 16, 0),
         * emitted directly because the inlined copy rounds the y-sum's reciprocal
         * to float where the out-of-line body does not (0x3001ae40..0x3001aede):
         *   invX      = 1.0f/screenXScale, kept 80-bit (FDIV 0x3001ae48), a float
         *               copy spilled by FST 0x3001ae70;
         *   x arg     = invX + invX       (FADD ST0,ST0 0x3001ae74; x=2 folded);
         *   invYf     = 1.0f/screenYScale ROUNDED to float (FSTP 0x3001ae82);
         *   sum arg   = (y + 12.8f) * invYf   (0x3001ae86..ae90; 12.8f @0x3007c028
         *               is height*0.8f = 16*0.8f constant-folded);
         *   width arg = invXf * 8.0f          (0x3001ae94/ae98, the FLOAT copy);
         *   scale arg = invYf * 0.33333334f   (0x3001ae9e/aea2; @0x3007bf80 is
         *               height/48 = 16/48 constant-folded);
         * each argument rounded once at its FSTP (0x3001aeaa..0x3001aec9). */
        {
            long double invX = (long double)1.0f / (long double)cgs_screenXScale;

            /* The inlined helper still initializes its unused local-white
             * fallback before selecting the nonnull shared white pointer. */
            vec4_t localWhite;
            localWhite[0] = 1.0f;
            localWhite[1] = 1.0f;
            localWhite[2] = 1.0f;
            localWhite[3] = 1.0f;

            float invXf = (float)invX;                     /* FST 0x3001ae70 */
            long double xArgRaw = invX + invX;
            long double invYRaw = (long double)1.0f / (long double)cgs_screenYScale;
            float invYf = (float)invYRaw;                  /* FSTP 0x3001ae82 */
            long double sumArgRaw = ((long double)y + (long double)12.8f) * (long double)invYf;
            long double widthArgRaw = (long double)invXf * (long double)8.0f;
            long double scaleArgRaw = (long double)invYf * (long double)(1.0f / 3.0f);

            /* 0x3001aeaa..0x3001aec9: FXCH makes the outgoing binary32 stores
             * occur in width, scale, y-sum, x order. */
            float widthArg = (float)widthArgRaw;
            float scaleArg = (float)scaleArgRaw;
            float sumArg = (float)sumArgRaw;
            float xArg = (float)xArgRaw;

            cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(xArg), CG_FloatBits(sumArg), 5, /* style (PUSH 5) */
                          CG_FloatBits(scaleArg), (intptr_t)&cg_colorWhite, (intptr_t)lineText, CG_FloatBits(widthArg), 0,
                          3); /* mode (PUSH 3)  */
            (void)localWhite;
        }

        /* The next y remains live in ST0 through the index/pointer increments and
         * loop comparison, and is rounded only immediately before the back edge. */
        long double nextYRaw = (long double)y + (long double)CG_SNDINFO_LINE_STEP;
        i = coduo_int32_from_bits((uint32_t)i + 1u);
        ++ch;
        qboolean anotherChannel = i < count;
        y = (float)nextYRaw;
        if (!anotherChannel) {
            break;
        }
    }
}
