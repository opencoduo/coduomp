// Source: uo_cgame_mp_x86.dll 0x3001f5c0..0x3001f6ef
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001f5c0_3001f6ef.mcode
//
// CG_AdjustPositionForMover - compensate a world point for the motion of a mover
// entity between two client times.
//
// Given a point `in`, a mover entity number, and a from/to time pair, this
// evaluates the mover's position trajectory (currentState.pos, cent+0x0c) at both
// times, adds (posAt(toTime) - posAt(fromTime)) to `in`, and writes the result to
// `out`. It also evaluates the angle trajectory (currentState.apos, cent+0x30)
// delta into the optional `angleDelta` vec3 (skipped when NULL).
//
// The mover is only compensated when its currentState.eType is ET_MOVER(5) or
// ET_SCRIPTMOVER(8) (the [cent+0x4] == 5 || == 8 gate at 0x3001f5f6..0x3001f601 --
// note this reads the eType field at cent+0x4, NOT the pos trajectory type). For
// any other eType, or a moverNum outside [1, MAX_GENTITIES-2], `out` is a plain
// copy of `in`.
//
// Naming: the .mcode header carries the SIZE-GUESS name
// "GetKeyBindingLocalizedString" (matched only on byte size 0x12f/0x130).
// REJECTED: the body is pure vec3 trajectory-delta math -- four BG_EvaluateTrajectory
// calls (callee 0x30005f30, identified by its own "BG_EvaluateTrajectory: unknown
// trType" diagnostic) plus component-wise FSUB/FADD -- with no string, key, or
// localization handling whatsoever. The provisional-by-role name
// CG_AdjustPositionForMover reflects the proven behavior (the classic Q3/CoD
// mover-lag position compensation); the exact original source name is not fully
// proven and this identity is independent of the REJECTED size-guess also named
// "CG_AdjustPositionForMover" for the unrelated eType-5 render handler 0x3001f120.
//
// Register/stack ABI (proven by callers 0x30021e73 / 0x30035b3c / 0x30036014):
//   EAX = moverNum          (index into cg_entities[], stride 0x288)
//   EDI = in                (const source point)
//   ESI = out               (destination point)
//   [esp+4] = fromTime, [esp+8] = toTime, [esp+0xc] = angleDelta  (cdecl; RET,
//   caller `add esp,0xc`). The function reserves a 0x34-byte frame and additionally
//   pushes EBX/EBP only inside the mover branch (i386 register-save detail).
//
// Machine-code facts preserved:
//   - angleDelta (arg3) is zeroed on entry (0x3001f5cd..0x3001f5d3) whenever
//     non-NULL, BEFORE the range/type checks -- so on the copy paths it stays zero.
//   - moverNum guard: `moverNum > 0 && moverNum < 0x3fe` (signed JLE/JGE at
//     0x3001f5d7/0x3001f5e2). 0x3fe == MAX_GENTITIES - 2.
//   - The two trajectories handed to BG_EvaluateTrajectory are &cent->currentState.pos (EBP =
//     &cent + 0x0c, 0x3001f619) and &cent->currentState.apos (EBX = &cent + 0x30, 0x3001f633).
//   - out = (posAt(toTime) - posAt(fromTime)) + in   (FSUB then FADD [EDI],
//     0x3001f663..0x3001f6be). The pos deltas use toTime for two evals and
//     fromTime for the other two; per the stack-slot dataflow the SUBTRAHEND is the
//     fromTime evaluation, so the delta is toPos - fromPos.
//   - angleDelta = aposAt(toTime) - aposAt(fromTime), stored only when non-NULL
//     (TEST EAX,EAX / JZ at 0x3001f66f/0x3001f6c1).

#include <stddef.h>
#include <string.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_AdjustPositionForMover(const vec3_t in /* EDI */, int32_t moverNum /* EAX */,
                               int32_t fromTime, int32_t toTime, vec3_t out /* ESI */,
                               vec3_t angleDelta)
{
    // 0x3001f5c4..0x3001f5d3: pre-zero the optional angle-delta output.
    if (angleDelta != NULL) {
        angleDelta[2] = 0.0f;
        angleDelta[1] = 0.0f;
        angleDelta[0] = 0.0f;
    }

    // 0x3001f5d5..0x3001f5e2: only real, in-range mover entities are compensated.
    // Signed compares: moverNum > 0 and moverNum < MAX_GENTITIES - 2.
    if (moverNum > 0 && moverNum < MAX_GENTITIES - 2) {
        // 0x3001f5e8..0x3001f5f3: cent = &cg_entities[moverNum].
        // The array element stride is 0x288 (sizeof centity_t, the canonical
        // centity view), so the byte-correct slot address is formed by indexing the
        // 0x288-strided centity_t and reinterpreting it as the trajectory view
        // (both overlay the same physical centity; eType/pos/apos are read from it).
        centity_t *cent = cg_entities + moverNum;

        // 0x3001f5f6..0x3001f601: only true movers carry a compensable trajectory.
        entityType_t eType = cent->currentState.eType;
        if (eType == ET_MOVER || eType == ET_SCRIPTMOVER) {
            // 0x3001f61c..0x3001f65e: evaluate pos and apos at both times.
            vec3_t posFrom;   // pos at fromTime   ([esp+0x18])
            vec3_t aposFrom;  // apos at fromTime  ([esp+0x30])
            vec3_t posTo;     // pos at toTime     ([esp+0x0c])
            vec3_t aposTo;    // apos at toTime    ([esp+0x24])

            BG_EvaluateTrajectory(&cent->currentState.pos, fromTime, posFrom);
            BG_EvaluateTrajectory(&cent->currentState.apos, fromTime, aposFrom);
            BG_EvaluateTrajectory(&cent->currentState.pos, toTime, posTo);
            BG_EvaluateTrajectory(&cent->currentState.apos, toTime, aposTo);

            /* 0x3001f663..0x3001f6be: the x and y position deltas remain live
             * on the x87 stack across the optional-output test and all three
             * angle-delta calculations. Z alone is rounded through a float
             * scratch slot before its later reload. */
            long double posDelta0 =
                (long double)posTo[0] - (long double)posFrom[0];
            qboolean publishAngleDelta = angleDelta != NULL;
            long double posDelta1 =
                (long double)posTo[1] - (long double)posFrom[1];
            float posDelta2 = (float)(
                (long double)posTo[2] - (long double)posFrom[2]);

            vec3_t aposDelta;
            aposDelta[0] = (float)(
                (long double)aposTo[0] - (long double)aposFrom[0]);
            aposDelta[1] = (float)(
                (long double)aposTo[1] - (long double)aposFrom[1]);
            aposDelta[2] = (float)(
                (long double)aposTo[2] - (long double)aposFrom[2]);

            /* FXCH selects x first; y remains live for the following FADD. */
            out[0] = (float)(posDelta0 + (long double)in[0]);
            out[1] = (float)(posDelta1 + (long double)in[1]);
            out[2] = (float)((long double)posDelta2 + (long double)in[2]);

            // 0x3001f6c1..0x3001f6d4: publish the angle delta only if requested.
            if (publishAngleDelta) {
                memcpy(&angleDelta[0], &aposDelta[0],
                                 sizeof(angleDelta[0]));
                memcpy(&angleDelta[1], &aposDelta[1],
                                 sizeof(angleDelta[1]));
                memcpy(&angleDelta[2], &aposDelta[2],
                                 sizeof(angleDelta[2]));
            }
            return;
        }

        // 0x3001f603..0x3001f610: non-mover trType -> out = in (angleDelta stays 0).
        memcpy(&out[0], &in[0], sizeof(out[0]));
        memcpy(&out[1], &in[1], sizeof(out[1]));
        memcpy(&out[2], &in[2], sizeof(out[2]));
        return;
    }

    // 0x3001f6db..0x3001f6e8: out-of-range / non-positive moverNum -> out = in.
    memcpy(&out[0], &in[0], sizeof(out[0]));
    memcpy(&out[1], &in[1], sizeof(out[1]));
    memcpy(&out[2], &in[2], sizeof(out[2]));
}
