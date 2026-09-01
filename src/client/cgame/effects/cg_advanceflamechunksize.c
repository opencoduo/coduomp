// Source: uo_cgame_mp_x86.dll 0x30025c60..0x30025ccf
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30025c60_30025ccf.mcode
//
// CG_AdvanceFlameChunkSize (0x30025c60) — advance one flame chunk's radius
// (+0xe4) toward its target startSpeedBits (+0x5c) at a per-frame rate, then clamp
// it to the flame subsystem maximum (290.0).
//
// Name adjudication (RESOLVED): the .mcode header's "G_VehFreePathPos" is a pure
// size match (win size 0x6f == server size 0x6f) and is REJECTED per the
// no-size-matching rule (that server symbol is a server-only vehicle.c routine in
// the wrong module/ABI). An earlier pass modelled this on a provisional
// cg_adsAnimState_t as CG_ADSAnim_AdvanceZoom, but the machine code operates on
// flameChunk_t: it advances radius (+0xe4) toward startSpeedBits (+0x5c), caches
// the rate into sizeRate (+0x64), stamps spawnTimeCopy (+0x68), and clamps to
// 290.0. The 290.0 clamp constant at 0x3007bebc and its siblings are the shared
// flame constants used across CG_FireFlameChunks; the callee rate helper is
// CG_FlameGetSizeRate (0x30023b70). All four call sites are inside
// CG_FireFlameChunks (0x30027d10) passing a flame chunk.
//
// Register ABI (non-default; proven from the four call sites in 0x30027d10,
// e.g. 0x30027ff7: PUSH EAX(time); MOV EAX,EBP(chunk); CALL; ADD ESP,4): the
// flame chunk pointer arrives in EAX; there is one caller-cleaned stack argument
// `flameTime` (a signed int32, fildl-loaded); the function ends in a plain RET.

#include "client/cgame/client_recovered.h"

// 290.0f — the flame chunk radius's absolute maximum. Stored in .rdata as the
// shared flame constant g_const_float_290 (bit pattern 0x43910000).
// Consumed both as the FCOMP compare bound and as the clamp value.
#define CG_FLAME_SIZE_MAX 290.0f

void CG_AdvanceFlameChunkSize(flameChunk_t *f /* EAX */, int32_t flameTime)
{
    // 30025c63: if (radius < startSpeedBits) advance toward the target.
    // FCOMP radius vs startSpeedBits (a float here); TEST AH,0x5; JP skips the
    // body, i.e. the body runs only when the compare is strictly "below".
    if (f->radius < CG_FloatFromBits(f->startSpeedBits)) {
        // 30025c73: rate = CG_FlameGetSizeRate(f), raw in ST0.
        // 30025c78 FST float [ESI+0x64] stores a ROUNDED copy to sizeRate but
        // KEEPS the unrounded ST0, which the FMULP at 30025c88 then consumes --
        // so `rate` is long double and only f->sizeRate rounds (the FST).
        long double rate = CG_FlameGetSizeRate(f);
        f->sizeRate = rate;                          // FST float [ESI+0x64]

        // 30025c7b..30025c8a: newRadius = radius + rate*((flameTime - spawnTimeCopy)
        //                                                * soundAmpRate)
        // flameTime is FILD'd raw (30025c7b, no float cast -- it is a ms clock that
        // exceeds 2^24); spawnTimeCopy (+0x68) is subtracted as a DOUBLE (FSUB m64
        // at 30025c7f, so no (float) narrowing); soundAmpRate is FMUL m32.
        // 30025c90 FST float [ESI+0xe4] stores newRadius ROUNDED to radius but
        // KEEPS the unrounded ST0, which the FCOMP at 30025c96 compares -- so
        // newRadius is long double and only f->radius rounds (the FST).
        long double newRadius = f->radius
                  + rate * (((long double)flameTime - f->spawnTimeCopy)
                            * f->soundAmpRate);
        f->radius = newRadius;                       // FST float [ESI+0xe4]

        // 30025c96: TEST AH,0x41 after FCOMP newRadius vs startSpeedBits; JNZ
        // skips the clamp, so it runs only when newRadius is strictly above the
        // target -- and the compare is on the UNROUNDED newRadius.
        if (newRadius > CG_FloatFromBits(f->startSpeedBits)) {
            // 30025ca0: integer copy of the float bits (MOV EAX,[ESI+0x5c] /
            // MOV [ESI+0xe4],EAX) — radius = startSpeedBits.
            f->radius = CG_FloatFromBits(f->startSpeedBits);
        }
    }

    // 30025ca9..30025cbe: record this frame's time as the new spawnTimeCopy
    // (double) and clamp radius to the 290.0 subsystem maximum. The FCOMP of
    // radius vs 290.0 happens before the FILD/FSTP that store spawnTimeCopy;
    // TEST AH,0x41 / JNZ skip the clamp unless radius is strictly above 290.0.
    qboolean overMax = (f->radius > CG_FLAME_SIZE_MAX) ? qtrue : qfalse;
    f->spawnTimeCopy = (double)flameTime;            // FSTP double [ESI+0x68]
    if (overMax) {
        // 30025cc3: MOV dword [ESI+0xe4],0x43910000 — radius = 290.0f.
        f->radius = CG_FLAME_SIZE_MAX;
    }
}
