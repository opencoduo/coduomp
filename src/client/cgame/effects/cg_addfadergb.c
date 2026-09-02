// Source: uo_cgame_mp_x86.dll 0x3002abc0..0x3002ac20
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002abc0_3002ac20.mcode

#include "client/cgame/client_recovered.h"

/*
 * CG_AddFadeRGB — the LE_FADE_RGB handler dispatched by CG_AddLocalEntities
 * (0x3002ad00, case leType==0). The rejected `.mcode` name BG_CheckProne is a
 * pure size match (0x60==0x60) and is contradicted by the machine code: this
 * function does no movement/prone logic. It reads cg.time, a per-entity end time
 * and fade rate, and an RGBA color, packs the faded color into the embedded
 * refEntity's shaderRGBA bytes, and submits the entity to the render scene.
 *
 * Machine-code facts proven for every statement below:
 *   3002abc1  ECX = cg_time                              [uint32 global 0x304831b0]
 *   3002abc7  EAX = le->endTime            (int32 @ +0x10)
 *   3002abca  EAX = EAX - ECX              (endTime - cg.time), 32-bit wrap
 *   3002abcf  FILD [ESP]                   -> (float) of that signed int32
 *   3002abd3  EDI = &le->refEntity         (LEA ESI+0x50)
 *   3002abd6  FMUL le->lifeRate            (float @ +0x14)
 *   3002abd9  FMUL 255.0f                  (float const 0x3007bd64) => P
 * For each channel i in 0..3:
 *   FLD ST0 ; FMUL le->color[i] ; CALL _ftol2(0x3006be3c) ; MOV [EDI+off],AL
 *   channel bytes: +0x6c(0x3c), +0x6d(0x40), +0x6e(0x44), +0x6f(0x48).
 * The last channel omits the FLD (it consumes the surviving P, balancing the x87
 * stack). _ftol2 takes its arg in ST0 and returns the truncated int in EAX; only AL
 * (the low byte) is stored, so each shaderRGBA byte is the converted color, wrapped
 * to 8 bits by the byte store.
 *   3002ac0e  PUSH EDI ; PUSH 0x3d ; CALL [cgame_syscall]  => trap_R_AddRefEntityToScene(&le->refEntity)
 *   3002ac1a  ADD ESP,8   (cdecl caller cleanup of the two pushed args)
 *
 * P = (float)(le->endTime - cg.time) * le->lifeRate * 255.0f is the normalized
 * remaining-life fraction scaled to color-byte range; multiplying by each color
 * component (in [0,1]) yields the faded 0..255 channel value.
 */
void CG_AddFadeRGB(localEntity_t *le)
{
    refEntity_t *re = &le->refEntity;

    /* 0x3002abcf..0x3002abd9: FILD; FMUL lifeRate; FMUL 255.0f — the chain is
     * never stored, and each channel's FLD ST0 / FMUL color[i] feeds the RAW
     * 80-bit value straight into _ftol2. float locals here would round three
     * times where the DLL rounds not at all, so `scaled` is long double.
     * No (float) cast on the ms delta either: 0x3002abcc stores the INTEGER
     * difference and 0x3002abcf FILDs it straight into the FMUL chain with no
     * FSTP DWORD, so the integer enters exact. Under -std=c11
     * (-fexcess-precision=standard) an explicit (float) would compile to a real
     * fildl/fstps/flds and round it, diverging once |endTime - cg_time| exceeds
     * 2^24 ms (~4.66 h of uptime, which a long-running server reaches daily). */
    int32_t remaining = coduo_int32_from_bits(
        (uint32_t)le->endTime - (uint32_t)cg_time);
    long double scaled =
        (long double)remaining * (long double)le->lifeRate *
        (long double)colorByteScale;

    re->shaderRGBA[0] = (uint8_t)coduo_fp_to_i32_extended(scaled * le->color[0]);
    re->shaderRGBA[1] = (uint8_t)coduo_fp_to_i32_extended(scaled * le->color[1]);
    re->shaderRGBA[2] = (uint8_t)coduo_fp_to_i32_extended(scaled * le->color[2]);
    re->shaderRGBA[3] = (uint8_t)coduo_fp_to_i32_extended(scaled * le->color[3]);

    trap_R_AddRefEntityToScene(re);
}
