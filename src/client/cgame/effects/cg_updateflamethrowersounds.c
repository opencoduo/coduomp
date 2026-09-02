// Source: uo_cgame_mp_x86.dll 0x30029210..0x3002972c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029210_3002972c.mcode
//
// CG_UpdateFlamethrowerSounds — per-frame update of the client's flamethrower / catch-fire
// looping sound envelopes. Runs once per client frame:
//
//   1. Compute the flame animation clock and the clamped frame delta.
//   2. Walk the secondary flame-chunk list (cg_flameChunkList via .listNext),
//      accumulating a per-index looping-sound amplitude envelope for each active
//      chunk (envA from the frame delta, envB from cg.frametime scaled by the
//      chunk's own rate), each clamped to <= 1.0; and, for chunks whose burn state
//      has elapsed, register the "fl_catch_fire_lo"/"fl_catch_fire_hi" looping
//      catch-fire sounds and start the loop through the cgame sound traps.
//   3. Decay every table envelope toward zero and, per index, emit the flamethrower
//      cooldown sound once a stale active emitter is observed with a still-nonzero
//      post-decay envelope.
//
// NAMING: the .mcode pre-hint `# name CG_UpdateFlamethrowerSounds` was a pure size
// match (win size 0x51c == matched size 0x51c), which the project rules forbid as
// evidence — REJECTED as authority. The proven behavior (per-frame flame sound-loop
// envelope update + "fl_catch_fire" / "flamethrower_*" sound handles) does match a
// flamethrower-sound updater. The Mac CG_UpdateFlamethrowerSounds has the exact
// same function size and corresponding flame-time, origin, sound-alias, and
// blended-sound call sequence, resolving the source name.
//
// ABI: void, no arguments. Callee-cleaned frame (SUB ESP,0x2c + PUSH ebx/ebp/esi/edi;
// matching ADD ESP,0x2c + POPs before RET). RET has no immediate — nothing to clean.
//
// x87 CONDITION-CODE IDIOMS used throughout (FNSTSW AX; TEST AH,mask; Jcc):
//   After FCOM/FCOMP mem: C3(=ZF) in AH bit 0x40, C2(=PF/unordered) in bit 0x04,
//   C0(=CF/less) in bit 0x01.
//   * TEST AH,0x41 ; JNZ  -> taken when ST0 is not ordered-greater
//                              (less, equal, or unordered).
//   * TEST AH,0x41 ; JZ   -> taken when ST0 >  mem.
//   * TEST AH,0x05 ; JP   -> taken for greater, equal, or unordered.
//   * TEST AH,0x05 ; JNP  -> taken only for ordered less-than.
//   * TEST AH,0x44 ; JNP  -> after FUCOMPP, taken only for ordered equality.
// Each branch below records the resolved sense in a comment.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

#include <math.h>

/* 0x3005bcd0 and 0x3006bb20 are the statically linked MSVC floor(double) and
 * pow(double,double) implementations. Portable recovered source crosses that
 * library boundary through the standard math interface. The arguments here make
 * both calls mathematical identities, but the calls remain explicit for fidelity. */

/* Small flame-sound tuning constants, dumped from .rdata at the exact addresses. */
enum {
    FLAME_CHUNK_MODE_2 = 2,  /* field_2c value that suppresses a chunk's sound update */
    FLAME_CHUNK_MODE_3 = 3   /* field_2c value that selects the burn-elapsed emit path */
};

/* .rdata float/double constants (exact addresses verified via objdump). */
#define FLAME_MS_TO_SEC 0.0010000000474974513f  /* 0x3007bd94 (== 0.001f) */
#define FLAME_ENVA_GAIN 2.200000047683716f      /* 0x3007c2e8 (float 2.2f) */
#define FLAME_ENVB_GAIN5 5.0f                    /* 0x3007bde0 (5.0f) */
#define FLAME_ENVB_GAIN_HALF 0.5f                    /* 0x3007bce8 (0.5f) */
#define FLAME_E8_THRESH 0.9                      /* 0x3007c2e0 (double 0.9) */
#define FLAME_E4_SCALE 0.003448275849223137f   /* 0x3007c298 (float) */
#define FLAME_PRIORITY_MAX 100000.0f               /* 0x3007c2d8 (float 1e5) */
#define FLAME_ENVDECAY_SCALE 4.0f                    /* 0x3007be40 (4.0f) */
#define FLAME_LIFE_SCALE 0.0003333333333333333   /* 0x3007c2d0 (double 1/3000) */
#define FLAME_STREAM_FADE 4000.0f                 /* 0x3007c144 (float 4000.0f) */
#define FLAME_ONEF 1.0f                     /* 0x3007bce0 (1.0f) */
#define FLAME_ZEROF 0.0f                     /* 0x3007bcec (0.0f) */
#define FLAME_ONE_D 1.0                      /* 0x3007bcf8 (double 1.0) */
#define FLAME_ZERO_D 0.0                      /* 0x3007bcf0 (double 0.0) */

void CG_UpdateFlamethrowerSounds(void)
{
    /* 0x30029210..0x3002922d: flameTime = Q_rint(floor(2.0 * cg.time)).
     * cg.time is integral, so floor and Q_rint are both identities and flameTime
     * is exactly 2*cg.time; the FP path is preserved for machine-code fidelity.
     * The _ftol2 at 0x3002922d consumes floor's ST0 return directly (0x3002922a is
     * ADD ESP only -- no store), so the raw unrounded ST0 is what gets converted;
     * a (float) cast here would round it. coduo_fp_to_i32_extended takes long double
     * for exactly this. (Same shape as CG_FireFlameChunks 0x30027d30/0x30027d38.)
     * EBP holds flameTime for the rest of the function. */
    int32_t signedTime = coduo_int32_from_bits(cg_time);
    double timeD = (double)signedTime;
    int32_t flameTime = coduo_fp_to_i32_extended(floor(timeD + timeD));

    /* 0x30029232..0x30029241: latch the flame animation clock (0x300ab718, the shared
     * flame time snapshot cg_flameTime, also consumed by the per-frame chunk driver
     * CG_UpdateFlameChunk). */
    int32_t now = signedTime;                          /* 0x3002923c MOV EAX,cg.time */
    cg_flameTime = (uint32_t)flameTime;

    /* 0x30029232/0x30029247..0x3002924d: clampedPrev = (prev == 0) ? now
     *                                                : min(prev, now). */
    int32_t prev = cg_flameSoundsPrevTime;             /* 0x30029232 MOV ECX,prev */
    int32_t clampedPrev;
    if (prev == 0) {
        clampedPrev = now;                             /* JE 0x3002924d */
    } else if (prev <= now) {
        clampedPrev = prev;                            /* JLE keeps prev */
    } else {
        clampedPrev = now;                             /* else ECX = now */
    }

    /* 0x30029255..0x3002925f: clamped nonnegative frame delta; then advance the
     * previous-run timestamp to now. */
    int32_t deltaTime = coduo_int32_from_bits((uint32_t)now - (uint32_t)clampedPrev);         /* [ESP+0x18] */
    cg_flameSoundsPrevTime = now;

    /* 0x3002924f/0x30029259/0x30029264: walk the flame-chunk list only when it is
     * non-empty; otherwise fall straight through to the decay pass. */
    for (flameChunk_t *chunk = cg_flameChunkList;      /* 0x3002924f EBX */
         chunk != NULL;                                /* 0x30029639 TEST; 0x3002963b JNZ */
         chunk = chunk->listNext) {                    /* 0x30029636 MOV EBX,[EBX+0xc] */

        int32_t idx = chunk->ownerInfoIndex;                 /* 0x30029270 MOV EDX,[EBX+0x34] */

        /* 0x3002927b..0x30029283: skip a chunk whose flame-info sound is already
         * active (field_64 nonzero). */
        if (cg_flameInfo[idx].soundActiveFlag != 0) {
            continue;                                  /* JNZ 0x30029636 */
        }
        /* 0x30029289..0x3002928d: skip a chunk in mode 2. */
        if (chunk->kind == FLAME_CHUNK_MODE_2) {
            continue;                                  /* JZ 0x30029636 */
        }

        cgFlameSoundLoop_t *loop = &cg_flameSoundLoops[idx]; /* base 0x300a8718, stride 12 */

        /* 0x30029299..0x3002929f: if this index was NOT already updated this frame,
         * accumulate and clamp envA. */
        if (loop->frameOwner != flameTime) {           /* CMP [+8],EBP ; JZ 0x30029351 */
            /* 0x300292a5..0x300292bb: envA += deltaTime * 0.001f * 2.2f.
             * 0x300292bb is FST, not FSTP: it writes the ROUNDED sum to envA and
             * KEEPS the unrounded value in st0, which is what the clamp compare at
             * 0x300292c1 then reads. Hence the long double live value plus an
             * explicit (float) cast at the store. (Contrast the envB accumulations
             * below, which FSTP and genuinely reload the rounded float -- the two
             * are asymmetric and must not be normalised to one shape.) */
            long double envAFull =
                (long double)deltaTime * (long double)FLAME_MS_TO_SEC * (long double)FLAME_ENVA_GAIN + (long double)loop->envA;
            loop->envA = (float)envAFull;              /* FST 0x300292bb */
            /* 0x300292c1..0x300292d6: clamp envA to <= 1.0. FCOMP 1.0f; TEST AH,0x41;
             * JNZ (envA <= 1.0) skips the store; store 1.0 only when envA > 1.0. */
            if (envAFull > FLAME_ONEF) {
                loop->envA = FLAME_ONEF;
            }

            /* 0x300292d8..0x300292f8: if this emitter was touched this/last frame
             * AND its flame-info gate (field_5c) is set, this chunk is done. */
            int32_t previousFrame = coduo_int32_from_bits((uint32_t)cg_clientFrame - 1u); /* 0x300292ec DEC ECX */
            if (cg_flameInfo[idx].clientFrame >= previousFrame && cg_flameInfo[idx].soundPathFlag == 1) { /* CMP ...,0x1 ; JZ 0x30029351 */
                goto stamp_owner; /* fall through to the stamp label */
            }

            /* 0x300292fa..0x30029324: envB += cg.frametime * 0.001f * chunk.soundAmpRate
             *                              * 5.0f, clamped to <= 1.0. The FILD at
             * 0x300292fa feeds the FMUL at 0x30029306 with no intervening store, so
             * cg_frametime stays exact in 80-bit -- no (float) cast. */
            float envB = (float)((long double)cg_frametime * (long double)FLAME_MS_TO_SEC * (long double)chunk->soundAmpRate *
                                     (long double)FLAME_ENVB_GAIN5 +
                                 (long double)loop->envB);
            loop->envB = envB; /* FSTP */
            /* 0x30029329..0x3002933e: FLD envB; FCOMP 1.0(double); TEST AH,0x41; JNZ
             * (envB <= 1.0) skips; store 1.0 only when envB > 1.0. */
            if ((double)envB > FLAME_ONE_D) {
                loop->envB = FLAME_ONEF; /* 0x30029346 store 0x3f800000 */
            }
        }

    stamp_owner:
        /* 0x30029351..0x3002935d: mark this index as updated this frame. */
        loop->frameOwner = flameTime; /* MOV [ECX*4 + 0x300a8720],EBP */

        /* 0x30029364..0x30029390: the "secondary" envB accumulation path, taken only
         * when this emitter's frame stamp is current/recent, this flame-info's owning chunk
         * matches the current chunk, and its gate (field_5c) == 1. */
        {
            int32_t previousFrame = coduo_int32_from_bits((uint32_t)cg_clientFrame - 1u); /* 0x30029373 DEC EDI */
            if (cg_flameInfo[idx].clientFrame >= previousFrame && /* JL 0x300293e9 */
                cg_flameInfo[idx].ownerChunk == chunk && /* CMP ...,EBX ; JNZ */
                cg_flameInfo[idx].soundPathFlag == 1) { /* CMP ...,0x1 ; JNZ */
                /* 0x30029392..0x300293b7: envB += cg.frametime * 0.001f
                 *                              * chunk.soundAmpRate * 0.5f. FILD at
                 * 0x30029392 feeds the FMUL directly (no store) -- no (float) cast. */
                float envB = (float)((long double)cg_frametime * (long double)FLAME_MS_TO_SEC * (long double)chunk->soundAmpRate *
                                         (long double)FLAME_ENVB_GAIN_HALF +
                                     (long double)loop->envB);
                loop->envB = envB; /* FSTP */
                /* 0x300293be..0x300293d6: clamp envB to <= 1.0 (double 1.0). */
                if ((double)envB > FLAME_ONE_D) {
                    loop->envB = FLAME_ONEF; /* 0x300293de store 0x3f800000 */
                }
            }
        }

        /* 0x300293e9..0x3002948f: reduce the chunk's child chain (linked via +0x08
         * parent, then +0x08 forward) into a running "priority" max in `priority`.
         * The result is a local; it is not read after the walk in this build (dead
         * store), but is computed for fidelity. */
        float priority = 0.0f; /* 0x300293ee [ESP+0x10] = 0 */
        for (flameChunk_t *child = chunk->parent; /* 0x300293e9 ESI = [EBX+8] (parent) */
             child != NULL; /* 0x3002948d/8f */
             child = child->parent) { /* 0x3002948a MOV ESI,[ESI+8] (parent chain) */
            /* 0x30029400..0x30029405: skip a child in a nonzero mode. */
            if (child->kind != 0) {
                continue; /* JNZ 0x3002948a */
            }
            /* 0x3002940b..0x30029422: contribution is 0 unless
             * chunk.lifeFraction is ordered below 0.9. FLD 0.0; FLD
             * chunk.lifeFraction; FCOMP 0.9(double); TEST AH,0x5; JP leaves
             * ST0 == 0.0 for greater, equal, and unordered. */
            /* contribution is NEVER stored: both arms converge on 0x3002945e with
             * the value live in st0 (the >=0.9 arm keeps the FLD'd 0.0f constant
             * from 0x3002940b; the else arm keeps pow's RAW st0 return from
             * 0x30029459), and the FCOM at 0x3002945e compares that unrounded st0
             * against priority. It is rounded ONLY if it wins, by the FSTP float
             * [ESP+0x10] at 0x30029469. A float local would round it early. */
            long double contribution;
            float chunkE8 = chunk->lifeFraction;
            if (!((double)chunkE8 < FLAME_E8_THRESH)) {
                contribution = FLAME_ZEROF; /* ST0 holds the FLD 0.0 */
            } else {
                /* 0x30029424..0x30029459: t = child.radius * 0.00344..f. The
                 * first comparison consumes the unrounded x87 product; ordered
                 * t < 1.0 is then stored to a float slot, while greater/equal/
                 * unordered selects stored 1.0. _CIpow leaves its result raw for
                 * the later priority compare. */
                long double tFull = (long double)child->radius * FLAME_E4_SCALE;
                float t;
                /* 0x30029432..0x3002943d: FCOM 1.0f; TEST AH,0x5; JP taken (t >= 1.0)
                 * -> t = 1.0f; else store t. */
                if (!(tFull < FLAME_ONEF)) {
                    t = FLAME_ONEF; /* 0x30029447 store 0x3f800000 */
                } else {
                    t = (float)tFull; /* 0x3002943f FSTP m32 */
                }
                contribution = powl((long double)t, 1.0L); /* exp 1.0 @0x3007bcf8 */
            }
            /* 0x3002945e..0x3002946d: priority = max(priority, contribution).
             * FCOM priority; TEST AH,0x41; JNZ (contribution <= priority) pops;
             * else store contribution. */
            if (contribution > priority) {
                priority = (float)contribution; /* FSTP float @0x30029469 */
            }
            /* 0x30029471..0x30029489: cap priority at 100000.0f. FCOMP 1e5f;
             * TEST AH,0x41; JNZ (priority <= 1e5) skips; else store 1e5. */
            if (priority > FLAME_PRIORITY_MAX) {
                priority = FLAME_PRIORITY_MAX;
            }
        }
        (void)priority; /* dead store — preserved for fidelity */

        /* 0x30029495..0x3002949b: mode-3 chunks take the burn-elapsed path below
         * (0x30029593); otherwise process the active-stream sound. */
        if (chunk->kind != FLAME_CHUNK_MODE_3) {
            /* 0x300294a1..0x300294cc: the stream sound plays only when this flame-info
             * has a live emit (field_60 != 0), its frame stamp is current/recent,
             * and its last-emit stamp (field_44) is not already this frame's flameTime. */
            cgFlameInfo_t *fi = &cg_flameInfo[idx];
            int32_t previousFrame = coduo_int32_from_bits((uint32_t)cg_clientFrame - 1u);
            if (fi->activeFlag != 0 && /* TEST ESI ; JZ 0x3002958a */
                fi->clientFrame >= previousFrame && /* CMP ...,EDI ; JL 0x3002958a */
                fi->lastUpdateTime != flameTime) { /* CMP ...,EBP ; JZ 0x3002958a */

                /* 0x300294d2..0x30029543: when the flamethrower-stream alias name is
                 * registered and this index's envB remains nonzero, start the stream
                 * sound positioned near the effect slot's origin. */
                if (cg_flameStreamSound != 0) { /* TEST EDX ; JZ 0x30029546 */
                    /* 0x300294dc..0x300294f3: FLD 0.0; FLD envB; FUCOMPP; TEST AH,0x44;
                     * JNP is taken when envB == 0.0 (equal => PF=0), jumping past the
                     * block to 0x30029546. So the stream start fires when envB != 0.0
                     * (the envelope is active). A prior pass misread the JNP sense. */
                    if (cg_flameSoundLoops[idx].envB != FLAME_ZEROF) {
                        /* 0x300294f5..0x30029532: build a channel-position vec3 from
                         * the effect slot's origin. The three writes land in
                         * CONSECUTIVE dwords: MOV [ESP+0x20] / MOV [ESP+0x24] before
                         * the PUSH, then FSTP [ESP+0x2c] AFTER the PUSH ECX at
                         * 0x3002951a (= pre-push [ESP+0x28]). The channelObj LEA at
                         * 0x3002951b is post-push [ESP+0x24] = pre-push [ESP+0x20],
                         * i.e. the BASE of the triple. */
                        float scratch[3];
                        scratch[0] = cg_entities[idx].lerpOrigin[0]; /* +0x208 */
                        scratch[1] = cg_entities[idx].lerpOrigin[1]; /* +0x20c */
                        /* scratch[2] = (1.0f - envB) * 4000.0f + origin[2]:
                         *   FLD 1.0f; FSUB envB; FMUL 4000.0f; FADD origin[2]; FSTP. */
                        scratch[2] = (FLAME_ONEF - cg_flameSoundLoops[idx].envB) * FLAME_STREAM_FADE + cg_entities[idx].lerpOrigin[2];
                        /* 0x30029538: CG_PlaySoundAliasByName(channelObj=scratch (ECX),
                         * soundName=cg_flameStreamSound (EAX=EDX, an alias-name pointer),
                         * entityNum=idx (ECX pushed at 0x3002951a)). */
                        (void)CG_PlaySoundAliasByName(idx, &scratch[0], cg_flameStreamSound);
                        /* 0x3002953d: reload flameTime latch (unchanged). */
                    }
                }

                /* 0x30029546..0x30029573: when the flamethrower-fire sound is
                 * registered, emit it for this chunk and then start it as a local
                 * sound on this frame's channel. */
                if (cg_flameFireSound != 0) { /* MOV EAX,fire ; TEST ; JZ */
                    /* 0x3002954f..0x30029555: compute the chunk's current vec3
                     * origin into the stack block beginning at ESP+0x2c. */
                    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    vec3_t soundOrigin = {chunk->worldPos[0], chunk->worldPos[1], chunk->worldPos[2]};
                    CG_ComputeFlameChunkOrigin(chunk, flameTime, soundOrigin);
                    /* 0x3002955a..0x30029568: after the two still-live compute
                     * arguments and the entity-number push, LEA [ESP+0x38]
                     * resolves back to that same output vec3 at the pre-call
                     * [ESP+0x2c].  The alias-name pointer is reloaded in EAX. */
                    (void)CG_PlaySoundAliasByName(ENTITYNUM_WORLD, soundOrigin, cg_flameFireSound);
                    /* 0x3002956d: reload flameTime latch (unchanged). */
                }

                /* 0x30029576..0x30029585: stamp this flame-info's last stream-emit
                 * time and move on to the next chunk. */
                cg_flameInfo[idx].lastUpdateTime = flameTime;
                continue; /* JMP 0x30029636 */
            }

            /* 0x3002958a..0x3002958d: only mode-3 chunks continue to the burn-elapsed
             * path; any other mode goes to the next chunk. */
            if (chunk->kind != FLAME_CHUNK_MODE_3) {
                continue; /* JNZ 0x30029636 */
            }
        }

        /* 0x30029593..0x300295d9: burn-progress catch-fire path (mode 3).
         *   burn = clamp((chunk.endTime - flameTime) / 3000, 0.0, 1.0);
         * the looping catch-fire sound below plays only while burn > 0 (the chunk's
         * expiry time field_50 is still ahead of the flame clock). */
        {
            /* 0x30029593 FILD [0x300ab718] (== flameTime, latched above and unchanged);
             * 0x30029599 FSUBR chunk.endTime (double) => field_50 - flameTime;
             * 0x3002959c FMUL (1/3000 double, 0x3007c2d0). */
            /* burn is never stored — the whole clamp/gate sequence runs on the
             * x87 stack (FILD/FSUBR/FMUL feed FCOM/FCOMP directly), so it must
             * stay at register precision (a double local would insert a
             * rounding the DLL does not perform). */
            long double burn = ((long double)chunk->endTime - (long double)(int32_t)cg_flameTime) * (long double)FLAME_LIFE_SCALE;
            /* 0x300295a2..0x300295c8: clamp burn to [0.0, 1.0].
             *   FCOM 0.0(double 0x3007bcf0); TEST AH,0x5; JP (burn >= 0.0) -> 0x300295b9,
             *   else burn = 0.0.
             *   0x300295b9 FCOM 1.0(double 0x3007bcf8); TEST AH,0x41; JNZ (burn <= 1.0)
             *   keeps burn; else burn = 1.0. */
            if (burn < FLAME_ZERO_D) {
                burn = FLAME_ZERO_D;
            } else if (burn > FLAME_ONE_D) {
                burn = FLAME_ONE_D;
            }
            /* 0x300295ce..0x300295d9: FCOMP 0.0f; TEST AH,0x41; JNZ skips for
             * less, equal, and unordered. Play only for an ordered burn > 0.0.
             * The raw register value is compared without a float narrowing. */
            if (!(burn > FLAME_ZEROF)) {
                continue; /* JNZ 0x30029636 */
            }
        }

        /* 0x300295db..0x30029633: register the two catch-fire looping sound aliases
         * for this chunk and start the loop. The chunk's sound origin is at
         * chunk +0xd8; the three calls go through the cgame trap vector. */
        {
            const float *soundOrigin = chunk->worldPos; /* 0x300295db LEA ESI,[EBX+0xd8] */
            /* 0x300295e1..0x300295ec: loAlias = trap_Com_PickSoundAlias(
             * "fl_catch_fire_low" @0x30077578, nameBuf).  Pushes low->high:
             * nameBuf, string, 0xc4. */
            snd_alias_t *loAlias = trap_Com_PickSoundAlias("fl_catch_fire_low", soundOrigin);
            /* 0x300295f5..0x30029602: hiAlias = trap_Com_PickSoundAlias(
             * "fl_catch_fire_high" @0x30077564, nameBuf).  (loHandle is saved into
             * EDI at 0x30029600 as the previous EAX before this second registration
             * overwrites EAX.) */
            snd_alias_t *hiAlias = trap_Com_PickSoundAlias("fl_catch_fire_high", soundOrigin);
            /* 0x3002960b..0x30029627: start the looping catch-fire sound.
             * cgame_syscall(CG_MSS_PLAY_BLENDED_SOUND_ALIASES, loHandle(EDI), hiHandle(EAX), floatBits(1.0f)
             * (ECX from [ESP+0x28]=0x3f800000), 0x3fe(1022), nameBuf, 0).
             * Pushes low->high: 0, nameBuf, 0x3fe, ECX(=1.0f bits), EAX(hiHandle),
             * EDI(loHandle), 0xc7. */
            trap_MSS_PlayBlendedSoundAliases(loAlias, hiAlias, FLAME_ONEF, ENTITYNUM_WORLD, soundOrigin, 0);
            /* 0x3002962d: reload flameTime latch (unchanged). */
        }
    }

    /* 0x30029641..0x30029724: decay pass — walk every flame-info element, its
     * flame-sound-loop entry, and its effect slot in lockstep, decaying the two
     * envelopes toward zero and emitting the cooldown sound as an envelope goes
     * silent. Loop over FLAME_INFO_COUNT (1024) elements.
     *
     *   scaledDelta = deltaTime * 0.001f;                 // [ESP+0x18], reused
     *   envStep     = scaledDelta * 4.0f;                 // [ESP+0x1c] */
    /* 0x30029641 FILD feeds 0x30029652 FMUL with no intervening store, so
     * deltaTime stays exact in 80-bit -- no (float) cast. 0x30029662 is FST, not
     * FSTP: it writes the ROUNDED scaledDelta to [ESP+0x18] (which the envA decay
     * at 0x30029681 genuinely reloads) while KEEPING the unrounded value in st0,
     * which is what the *4.0f at 0x30029666 consumes. Hence the split. */
    long double scaledDeltaFull = (long double)deltaTime * (long double)FLAME_MS_TO_SEC; /* FILD; FMUL 0.001f */
    float scaledDelta = (float)scaledDeltaFull; /* FST [ESP+0x18] @0x30029662 */
    float envStep = (float)(scaledDeltaFull * FLAME_ENVDECAY_SCALE); /* FMUL 4.0f; FSTP [ESP+0x1c] @0x3002966c */

    const char *cooldownSound = cg_flameCooldownSound; /* 0x30029645 MOV ECX,cooldown */

    for (int32_t i = 0; i < FLAME_INFO_COUNT; ++i) { /* ESI 0x300ab7b0.. step 0xb8 */
        cgFlameSoundLoop_t *loop = &cg_flameSoundLoops[i]; /* EDI 0x300a8718.. step 12 */
        cgFlameInfo_t *fi = &cg_flameInfo[i];

        /* 0x30029670..0x30029689: if envA != 0.0, decay it by 2*scaledDelta.
         * FLD 0.0; FLD envA; FUCOMPP; TEST AH,0x44; JNP skips only ordered
         * equality. Greater, less, and unordered values enter the decay. */
        if (loop->envA != FLAME_ZEROF) {
            /* 0x30029681..0x30029689: FLD scaledDelta; FADD ST0,ST0 (=2*scaledDelta);
             * FSUBR envA (= envA - 2*scaledDelta); FSTP envA. */
            loop->envA = (float)((long double)loop->envA - ((long double)scaledDelta + (long double)scaledDelta));
        }
        /* 0x3002968b..0x3002969a: FLD envA; FCOMP 0.0f; TEST AH,0x5; JP (envA >= 0)
         * skips; when envA < 0 clamp to 0.0. */
        if (loop->envA < FLAME_ZEROF) {
            loop->envA = FLAME_ZEROF;
        }

        /* 0x300296a0..0x300296ad: envBnew = envB - envStep. The FST at 0x300296a9
         * stores the DECAYED value to the [ESP+0x14] scratch (FLD envB; FSUB
         * envStep; FST [ESP+0x14]; FSTP envB) — the silent-test below reads the
         * POST-decay value, not a pre-decay copy. */
        float newEnvB = loop->envB - envStep; /* FLD envB; FSUB [ESP+0x1c] */
        loop->envB = newEnvB; /* FST scratch / FSTP envB */

        /* 0x300296b0..0x300296ee: emit the cooldown sound once when it is
         * registered, post-decay envB is nonzero, this flame-info still has a
         * live emit, and its frame stamp is stale. */
        if (cooldownSound != 0) { /* TEST ECX ; JZ 0x300296f1 */
            /* 0x300296b2..0x300296c3: FLD 0.0; FLD newEnvB([ESP+0x14]); FUCOMPP;
             * TEST AH,0x44; JNP is taken when newEnvB == 0.0 (equal => PF=0),
             * skipping to 0x300296f1. So the cooldown block runs when newEnvB != 0.0
             * (envelope still decaying). A prior pass misread the JNP sense. */
            if (newEnvB != FLAME_ZEROF && fi->activeFlag != 0 && /* CMP [ESI],0 ; JZ skip */
                fi->clientFrame < coduo_int32_from_bits((uint32_t)cg_clientFrame - 1u)) { /* DEC EDX; CMP; JGE skip */
                /* 0x300296d8..0x300296e3: clear the live-emit flag and start the
                 * flamethrower-cooldown local sound positioned on this slot.
                 * CG_PlaySoundAliasByName(channelObj=&cg_entities[i].lerpOrigin (ECX=EBP),
                 * soundName=cooldownSound (EAX, an alias-name pointer), entityNum=i (EBX)). */
                fi->activeFlag = 0;
                (void)CG_PlaySoundAliasByName(i, &cg_entities[i].lerpOrigin[0], cooldownSound);
                cooldownSound = cg_flameCooldownSound; /* 0x300296e8 reload ECX */
            }
        }

        /* 0x300296f1..0x30029701: FLD envB; FCOMP 0.0f; TEST AH,0x5; JP (envB >= 0)
         * skips; when envB < 0 clamp to 0.0. */
        if (loop->envB < FLAME_ZEROF) {
            loop->envB = FLAME_ZEROF;
        }
    }
}
