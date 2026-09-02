// Source: uo_cgame_mp_x86.dll 0x30021860..0x30021a2f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30021860_30021a2f.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <math.h>

/*
 * CG_AddLoopedEntitySound (0x30021860)
 *
 * Registers and (re)starts a client entity's pair of looping sounds for the current
 * frame, using a snapshot-interpolated "phase" value.
 *
 * ABI: `self` (centity_t *) arrives in EBX (register argument; the caller keeps
 * self there across the call and needs no stack cleanup). The AND ESP,~7 alignment and
 * the SUB ESP,0x10 local frame are compiler scaffolding.
 *
 * Behaviour, proven instruction-by-instruction:
 *
 *  Guards (0x30021869..0x3002189a) — emit nothing if any fails:
 *    - self->currentState.eventParm (+0xa4) == 0            -> return  (no primary looped sound)
 *    - self->loopedFxId (+0xdc) == 0     -> return  (no secondary looped sound)
 *    - if cg_snap->ps.pmType == PM_TYPE_INTERMISSION (5) AND self->currentState.vehicleEntityNum == 0x3ff
 *      (1023, the "no bound entity" sentinel) -> return  (suppressed at intermission)
 *
 *  Two sound-alias lookups (0x300218a0.. and 0x300218df..):
 *    Each index is formed as cfgIndex = CS_SOUNDS + soundIndex (0x295 == 661) and
 *    resolved through CG_ConfigString's INLINED lookup (the code does not CALL
 *    CG_ConfigString; it inlines the identical body): if cfgIndex is outside
 *    [0, MAX_CONFIGSTRINGS) it reports Com_ErrorMessage("CG_ConfigString: bad index:
 *    %i", cfgIndex) (string @0x30077d90) and STILL proceeds, then reads
 *    name = &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]]. The alias
 *    name is registered with the engine via cgame_syscall(CG_COM_PICK_SOUND_ALIAS, name,
 *    &self->lerpOrigin) -> handle (CG_COM_PICK_SOUND_ALIAS == 0xc4, the name->handle sound registrar;
 *    &self->lerpOrigin is the +0x208 vec3 the trap receives as its second argument).
 *    alias1 comes from fxId, alias2 from surfaceType.
 *
 *  Start (0x30021926..): only if BOTH handles are non-zero. Otherwise nothing plays.
 *    A single float `phase` is interpolated between self->currentState.leanValue (+0x6c)
 *    and self->nextState.leanAmount (+0x160) by t = cg_frameInterpolation (0x304831a8):
 *
 *      let a = self->nextState.leanAmount, b = self->currentState.leanValue
 *      if (!(a < 0))          // FCOMP a vs 0.0f (@0x3007bcec); the JP path,
 *                             // including unordered/NaN
 *          phase = b + (a - b) * t;                        // plain lerp(b, a, t)
 *      else {                 // fractional-part / wraparound lerp for a cyclic value
 *          phase = frac(|b|) + (frac(|a|) - frac(|b|)) * t;   // lerp of frac parts
 *          if (phase >= 0.99f) phase = 1.0f;               // @0x3007bed0 == 0.99f
 *          intPart = floor(|b|) + (floor(|a|) - floor(|b|)) * t; // lerp of int parts
 *          phase = -floor(intPart) - phase;                // recombine, negated
 *      }
 *      where frac(x) = x - floor(x). floor here is the CRT DOUBLE routine at
 *      0x3005bcd0 (argument and result are marshalled as double on the stack), so
 *      intPart rounds to double at 0x300219e4, not to float; |x| is the x87 FABS.
 *      Only the A-side splits are spilled to float slots and reloaded; the B-side
 *      floor() returns are consumed unrounded out of st1 (see the body comment).
 *
 *    The pair is then started via trap_MSS_PlayBlendedSoundAliases (0x3003e500, the cdecl CG_MSS_PLAY_BLENDED_SOUND_ALIASES == 0xc7
 *    sound-family wrapper). Its four stack args are the aliases, phase, and
 *    entity number; EDX/ECX carry origin and a zero timeShift:
 *
 *        trap_MSS_PlayBlendedSoundAliases(alias1, alias2, phase,
 *                                         self->currentState.number, self->lerpOrigin, 0);
 *
 * Naming: role name from behaviour (registers two CS_SOUNDS-aliased engine sounds and
 * starts them with an interpolated phase). The .mcode's "CheckMatchTimeout" is a pure
 * SIZE match against a function in the WRONG DLL (game_mp_uo) and carries no behavioural
 * signal — REJECTED. The exact CoD symbol is unproven (no cgame syscall-id table
 * recovered); the trap ids keep their proven-id names.
 *
 * Notes on faithful modelling:
 *  - `phase` is a float. The i386 wrapper forwards its raw 32-bit stack word;
 *    the native wrapper performs the equivalent CG_FloatBits conversion.
 *  - XOR ECX,ECX / MOV EDX,EDI supply timeShift=0 and origin=&lerpOrigin.
 *  - Out-of-range cfgIndex still proceeds to the array read after the diagnostic — this
 *    mirrors CG_ConfigString's own inlined behaviour and is preserved as-is.
 */
void CG_AddLoopedEntitySound(centity_t *self /* EBX */)
{
    int soundIndex1;
    int soundIndex2;
    int cfgIndex;
    int32_t stringOffset;
    const char *soundName;
    snd_alias_t *alias1;
    snd_alias_t *alias2;
    float phase;

    // 0x30021869..0x3002189a: gate on the two alias indices and the intermission case.
    if (self->currentState.eventParm == 0)
        return;
    if (self->currentState.loopedFxId == 0)
        return;
    /* 0x3002189a: `cmp [ebx+0x74],0x3ff; jne 0x30021a29` -- the return (epilogue at
     * 0x30021a29) is taken when vehicleEntityNum != 0x3ff, NOT == 0x3ff. During
     * intermission a vehicle-attached looped sound (!= the 0x3ff no-vehicle sentinel)
     * is silenced; a world sound (== 0x3ff) still plays. A prior pass used == 0x3ff,
     * inverting it. */
    if (cg_snap->ps.pmType == PM_TYPE_INTERMISSION && self->currentState.vehicleEntityNum != 0x3ff)
        return;

    // 0x300218a0..0x300218df: primary sound alias = CS_SOUNDS + fxId.
    soundIndex1 = self->currentState.eventParm;
    cfgIndex = coduo_int32_from_bits((uint32_t)soundIndex1 + (uint32_t)CS_SOUNDS);
    if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS)
        Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex);
    stringOffset = cgame_compat_read_target_i32_index(cg_gameState.stringOffsets, cfgIndex);
    soundName = (const char *)((uintptr_t)(const void *)cg_gameState.stringData + (uintptr_t)(intptr_t)stringOffset);
    alias1 = trap_Com_PickSoundAlias(soundName, self->lerpOrigin);

    // 0x300218df..0x30021924: secondary sound alias = CS_SOUNDS + surfaceType.
    soundIndex2 = self->currentState.loopedFxId;
    cfgIndex = coduo_int32_from_bits((uint32_t)soundIndex2 + (uint32_t)CS_SOUNDS);
    if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS)
        Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex);
    stringOffset = cgame_compat_read_target_i32_index(cg_gameState.stringOffsets, cfgIndex);
    soundName = (const char *)((uintptr_t)(const void *)cg_gameState.stringData + (uintptr_t)(intptr_t)stringOffset);
    alias2 = trap_Com_PickSoundAlias(soundName, self->lerpOrigin);

    // 0x30021926..0x30021937: only start the pair when both sounds registered.
    if (alias1 == NULL || alias2 == NULL)
        return;

    // 0x3002193d..0x30021a0b: interpolate the sound phase across the frame.
    /* TEST AH,5 / JP routes both ordered nonnegative values and unordered/NaN
     * through the plain interpolation path. */
    if (!(self->nextState.leanAmount < 0.0f)) {
        // 0x300219f9: plain lerp of the two phase samples.
        float a = self->nextState.leanAmount;
        float b = self->currentState.leanValue;
        float t = cg_frameInterpolation;
        phase = (float)(((long double)a - (long double)b) * (long double)t + (long double)b);
    } else {
        // 0x30021954..0x300219f0: cyclic (wraparound) interpolation. Split each sample
        // into integer and fractional parts of its magnitude, lerp them separately, and
        // recombine with a negated integer part.
        //
        // Asymmetric spill: the A-side intermediates are spilled to float slots
        // across the intervening floor() call and reloaded (fracA 0x3002196f store /
        // 0x3002198b reload; intA 0x300219c5 store / 0x300219d6 reload), so they are
        // genuinely float. The B-side values are floor()'s raw st0 return, consumed
        // straight out of st1 with NO store (fracB at 0x30021984/0x3002198f/
        // 0x30021997; intB at 0x300219d1/0x300219da/0x300219e2), so they stay
        // long double. 0x3005bcd0 is the MSVC CRT double floor(), which is why the
        // arguments and intLerp round to double, not float.
        /* The retail body makes four distinct floor calls in A,B,A,B order.
         * Volatile double carriers retain those calls under optimization and
         * model each FSTP QWORD argument store. */
        const float absAFrac = fabsf(self->nextState.leanAmount);
        volatile double absAFracDouble = (double)absAFrac;
        const double floorFracA = floor(absAFracDouble);
        float fracA = (float)((long double)absAFrac - (long double)floorFracA);
        const float absBFrac = fabsf(self->currentState.leanValue);
        volatile double absBFracDouble = (double)absBFrac;
        const double floorFracB = floor(absBFracDouble);
        long double fracB = (long double)absBFrac - (long double)floorFracB;
        double intLerp;

        // FST (no pop) at 0x30021999 rounds a copy into the phase slot while the
        // 0x3002199d FCOMP compares the UNROUNDED st0 against 0.99f. The stored
        // float is what 0x300219f0 later subtracts, so the two differ.
        float fracLerp = cg_frameInterpolation;
        long double phaseChain = (fracA - fracB) * fracLerp + fracB;
        phase = (float)phaseChain;
        if (phaseChain >= 0.99f)
            phase = 1.0f;

        const float absAInt = fabsf(self->nextState.leanAmount);
        volatile double absAIntDouble = (double)absAInt;
        float intA = (float)floor(absAIntDouble);
        const float absBInt = fabsf(self->currentState.leanValue);
        volatile double absBIntDouble = (double)absBInt;
        long double intB = (long double)floor(absBIntDouble);

        // 0x300219e4 FSTP QWORD: the lerp rounds to DOUBLE (floor's argument slot).
        float intLerpFraction = cg_frameInterpolation;
        intLerp = (intA - intB) * intLerpFraction + intB;
        phase = (float)(-(long double)floor(intLerp) - (long double)phase);
    }

    // 0x30021a0b..0x30021a26: the wrapper receives origin in EDX and a zero
    // timeShift in ECX in addition to its four stack arguments.
    trap_MSS_PlayBlendedSoundAliases(alias1, alias2, phase, self->currentState.number, self->lerpOrigin, 0);
}
