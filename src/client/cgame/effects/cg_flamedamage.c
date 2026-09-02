// Source: uo_cgame_mp_x86.dll 0x300265c0..0x300268d7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300265c0_300268d7.mcode
//
// CG_FlameDamage (0x300265c0) — client flame-damage proximity + line-of-sight
// test for one flame source against the LOCAL player. Given a flame source world position
// `flamePos`, its owner client number, a base radius, and the owning flame chunk, decide
// whether the local player (described by the current snapshot cg_snap) is close enough to,
// and in line of sight of, the flame; if so, latch the once-per-frame flame-damage flag and
// send a throttled flame-damage command to the server for the local player's client number.
//
// NAMING: the .mcode pre-hint `# name Item_ListBox_OverLB` is REJECTED. It is a pure
// size-match ("win size 0x317, matched size 0x318") and is contradicted by the body: there
// is no listbox/itemDef_t/rectDef_t access, no cursor rect hit-test, and no ui_shared code
// at all. This function does 3D world math over cg_snap (predicted player origin at +0x20,
// player bounds psFlameTraceA/B at +0x568/+0x574, clientNum at +0xe0), calls VectorDistance
// (0x300495b0), the CRT sqrt helper sqrt_f (0x3006bee0), the projection trace
// CG_Trace (0x30035310) and the flame-damage command sender CG_SendFlameDamageCommand
// (0x300291c0), and drives cg_flameTime / cg_flameInfo[].activeUntil /
// cg_flameDamageBestPos / cg_flameDamageTakenThisFrame. The Mac CG_FlameDamage
// shares the distance and trace callees and performs the corresponding local-client
// damage update, resolving the source name.
//
// ABI (proven from the sole call site 0x3002698c):
//   EDI (register)   = flamePos      -> LEA EDI,[caller+0xd8] : pointer to a vec3 world pos
//   [ESP+0x64] arg0  = ownerClientNum-> caller pushes [caller+0x38]; kept in EBP; compared to
//                      cg_snap->ps.psClientNum and forwarded to CG_SendFlameDamageCommand as painId
//   [ESP+0x68] arg1  = radiusBase    -> caller pushes [caller+0xe4] (a float, FLD [ESP+0x68])
//   [ESP+0x6c] arg2  = chunk         -> caller pushes its own EBP (a flameChunk_t *); only
//                      chunk->lifeFraction (+0xe8, a float) is read here (FLD [EAX+0xe8])
// The function ends with `ADD ESP,0x5c ; RET` (no RET imm): the three stack dwords are
// caller-cleaned (cdecl for the stack portion). EBP/ESI/EBX are callee-saved.
//
// FLOAT CONSTANTS (exact .rdata dumps, not inferred from neighbours):
//   0x3007bcec = 0.0f            (zero-vector test)
//   0x3007162c = 0.4f            (radiusBase * 0.4)
//   0x3007bce8 = 0.5f            (extra 0.5 for non-self owners)
//   0x3007c0a0 = 14.0f  (also MOV 0x41600000) (radius floor)
//   0x3007c098 = 0.4   (double) (chunk->lifeFraction self-damage gate)
//   0x3007bce0 = 1.0f            (z-clamp epsilon at the bounds edges)
//   0x3f800000 = 1.0f            (trace fraction == far-end test, compared as int bits)

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"
#include "compat/coduo_native_x87.h"

/* Radius shaping constants (see the FLOAT CONSTANTS note above). */
enum {
    CG_FLAME_DAMAGE_TRACE_MODEL = 0x2802031
}; /* EAX handle passed to CG_Trace */

/* chunk->lifeFraction (flameChunk_t +0xe8) — a chunk strength/alpha the self-damage path
 * requires to exceed this before the local player can be hurt by their own flame. */
static const double CG_FLAME_SELF_DAMAGE_MIN = 0.4;

void CG_FlameDamage(const vec3_t flamePos, int32_t ownerClientNum, float radiusBase, const flameChunk_t *chunk)
{
    snapshot_t *snap;
    float radius;
    float box[3];       /* [ESP+0x14/0x18/0x1c]: player box corner in snap-origin space */
    float closest[3];   /* [ESP+0x2c/0x30/0x34]: box point back in world space */
    float diff[3];      /* [ESP+0x20/0x24/0x28]: closest - flamePos */
    float dist;

    /* 0x300265c0: already handled a flame-damage hit for the local player this frame? */
    if (cg_flameDamageTakenThisFrame != 0) {          /* 0x300265c8 TEST EAX,EAX; JNZ ret */
        return;
    }

    /* 0x300265d5..0x3002660e: skip a zero flame position. Each component is FUCOMPP'd
     * against 0.0f; if all three equal 0.0f (worldPos == origin) the JNP at 0x30026608
     * returns. Any nonzero component falls through to the real work. */
    if (flamePos[0] == 0.0f && flamePos[1] == 0.0f && flamePos[2] == 0.0f) {
        return;
    }

    snap = cg_snap;                                   /* 0x30026613 MOV ESI,[0x30459160] */

    /* 0x3002660e..0x30026654: damage radius = radiusBase * 0.4, halved when the flame
     * owner IS the local player (self-flame), then floored at 14.0f. The JNZ at 0x3002662b
     * jumps PAST the 0.5 multiply, so the multiply runs only when clientNum == owner. */
    radius = radiusBase * 0.4f;                        /* FMUL [0x3007162c] */
    if (snap->ps.psClientNum == ownerClientNum) {           /* 0x30026625 CMP ECX,EBP; JNZ skip */
        radius = radius * 0.5f;                         /* 0x30026631 FMUL [0x3007bce8] */
    }
    /* 0x3002663b FLD radius; FCOMP 14.0f; TEST AH,5; JP skip: raise to 14.0f when below. */
    if (radius < 14.0f) {                              /* 0x3002663f FCOMP [0x3007c0a0] */
        radius = 14.0f;                                 /* 0x3002664c MOV ...,0x41600000 */
    }

    /* 0x30026654..0x30026669: per-LOCAL-PLAYER flame-damage cooldown,
     * cg_flameInfo[snap->ps.psClientNum]. The index register ECX is the same
     * snap->ps.psClientNum loaded at 0x3002661f (MOV ECX,[ESI+0xe0]) and never
     * reassigned before the IMUL ECX,ECX,0xb8 at 0x30026654 (element stride) --
     * NOT ownerClientNum (EBP), which is only CMP'd against it at 0x30026625.
     * activeUntil is a cg_flameTime stamp bumped to cg_flameTime + 100 whenever the
     * local player takes flame damage; while it is still in the future
     * (activeUntil >= cg_flameTime) flame damage is on cooldown and we skip. Signed
     * compare (JGE). A prior pass indexed this by ownerClientNum. */
    /* 0x3002665a loads activeUntil before 0x30026661 snapshots cg_flameTime;
     * EBX then carries that one flame-clock dword through the best-source path. */
    int32_t activeUntil = cg_flameInfo[snap->ps.psClientNum].activeUntil;
    int32_t flameClock = coduo_int32_from_bits(cg_flameTime);
    if (activeUntil >= flameClock) {
        return;                                        /* 0x30026669 JGE ret */
    }

    /* 0x3002666f..0x3002668c: de-dup against the best flame source already recorded THIS
     * flame-frame. cg_flameDamageBestPosTime == cg_flameTime means a best position exists;
     * if the new flame is within `radius` of it, ignore this one (a nearer chunk already
     * counts). VectorDistance(a in EAX, b in ECX) returns the distance on the x87 stack. */
    if (cg_flameDamageBestPosTime == flameClock) {   /* 0x3002666f CMP EBX,[..] */
        if (VectorDistance(cg_flameDamageBestPos, flamePos) < radius) { /* CALL 0x300495b0; FCOMP */
            return;                                    /* 0x3002668c JNP ret */
        }
    }

    /* 0x30026692..0x300266b6: latch this flame position as the new best for the frame. */
    cg_flameDamageBestPos[0] = flamePos[0];            /* 0x30026694 */
    cg_flameDamageBestPos[1] = flamePos[1];            /* 0x3002669c */
    cg_flameDamageBestPos[2] = flamePos[2];            /* 0x300266a5 */
    cg_flameDamageBestPosTime = flameClock; /* 0x300266b0 MOV [0x300a8710],EBX */

    /* 0x300266ab..0x30026770: clamp (flamePos - snap.psOrigin) into the player's bounds
     * box [psFlameTraceA .. psFlameTraceB] (A = mins, B = maxs), component by component.
     * Upper clamp uses TEST AH,0x41/JNZ (skip when delta <= maxs), lower clamp uses
     * TEST AH,5/JP (skip when delta >= mins). */
    box[0] = flamePos[0] - snap->ps.psOrigin[0];   /* 0x300266ab */
    box[1] = flamePos[1] - snap->ps.psOrigin[1];   /* 0x300266ba */
    box[2] = flamePos[2] - snap->ps.psOrigin[2];   /* 0x300266c4 */

    if (box[0] > snap->ps.playerMaxs[0]) {
        box[0] = snap->ps.playerMaxs[0];
    } /* 0x300266ce */
    if (box[0] < snap->ps.playerMins[0]) {
        box[0] = snap->ps.playerMins[0];
    } /* 0x300266e9 */
    if (box[1] > snap->ps.playerMaxs[1]) {
        box[1] = snap->ps.playerMaxs[1];
    } /* 0x30026704 */
    if (box[1] < snap->ps.playerMins[1]) {
        box[1] = snap->ps.playerMins[1];
    } /* 0x3002671f */
    if (box[2] > snap->ps.playerMaxs[2]) {
        box[2] = snap->ps.playerMaxs[2];
    } /* 0x3002673a */
    if (box[2] < snap->ps.playerMins[2]) {
        box[2] = snap->ps.playerMins[2];
    } /* 0x30026755 */

    /* 0x30026770..0x30026795: closest box point back in world space. */
    closest[0] = box[0] + snap->ps.psOrigin[0];    /* 0x30026770 */
    closest[1] = box[1] + snap->ps.psOrigin[1];    /* 0x30026783 */
    closest[2] = box[2] + snap->ps.psOrigin[2];    /* 0x3002678e */

    /* 0x30026774..0x300267b0: self-damage extra gate. When the flame owner IS the local
     * player, require chunk->lifeFraction > 0.4 (double compare, JNZ on <=) before continuing. */
    if (snap->ps.psClientNum == ownerClientNum) {           /* 0x3002677a CMP EAX,EBP; JNZ skip */
        /* TEST AH,0x41 / JNZ rejects less, equal, and unordered. */
        if (!((double)chunk->lifeFraction > CG_FLAME_SELF_DAMAGE_MIN)) { /* FCOMP [0x3007c098] */
            return;                                    /* 0x300267b0 JNZ ret */
        }
    }

    /* 0x300267b6..0x30026808: reject if the closest box point is farther than `radius`
     * from the flame. dist = sqrt(diff.z^2 + diff.y^2 + diff.x^2). JP-on-(>=) returns. */
    diff[0] = closest[0] - flamePos[0];                /* 0x300267b6 */
    diff[1] = closest[1] - flamePos[1];                /* 0x300267c0 */
    diff[2] = closest[2] - flamePos[2];                /* 0x300267cb */
    /* The squared sum is never stored: the FADDP chain (0x300267d6..0x300267f0)
     * hands the CRT sqrt helper its argument RAW in st0, so the argument must not
     * round to float. sqrtl on the 80-bit chain, one float rounding at the result
     * store (0x300267f7). */
    dist = (float)coduo_x87_sqrtl((long double)diff[2] * diff[2] + (long double)diff[1] * diff[1] +
                                  (long double)diff[0] * diff[0]); /* CALL 0x3006bee0 */
    /* TEST AH,5 / JP rejects greater, equal, and unordered. */
    if (!(dist < radius)) {                            /* 0x300267ff FCOMP [ESP+0xc]; JP ret */
        return;                                        /* 0x30026808 */
    }

    /* 0x3002680e..0x3002687b: build the trace start point at (snap.origin.x, snap.origin.y,
     * z) where z is the flame's z clamped to the player's z-bounds with a 1.0 epsilon:
     *   d = flamePos.z - origin.z
     *   if d >= maxs.z - 1  : z = maxs.z + origin.z - 1
     *   else if d <= mins.z + 1 : z = flamePos.z            (left unchanged)
     *   else                : z = mins.z + origin.z + 1
     * The x/y of the trace start are the player's origin; the trace end is the flame pos. */
    {
        float traceStart[3];
        /* dz is never stored: both branch compares recompute flamePos.z-origin.z
         * into a register and FCOMPP it raw (0x3002681f/0x30026850), so it must
         * stay at register precision. */
        long double dz = flamePos[2] - snap->ps.psOrigin[2];

        traceStart[0] = snap->ps.psOrigin[0]; /* 0x3002680e MOV [ESP+0x14],[ESI+0x20] */
        traceStart[1] = snap->ps.psOrigin[1]; /* 0x30026818 MOV [ESP+0x18],[ESI+0x24] */
        traceStart[2] = flamePos[2];                     /* 0x30026811/0x3002681b default = flamePos.z */

        /* 0x3002681f..0x30026838: FCOMPP compares (maxs.z - 1) vs dz; TEST AH,5; JP goes to
         * the mins branch. JP taken when (maxs.z-1) >= dz, so this maxs branch runs only when
         * dz > (maxs.z - 1.0f) (strict; equality falls to the mins branch). */
        if (dz > snap->ps.playerMaxs[2] - 1.0f) {        /* 0x30026830 FSUB [0x3007bce0] */
            traceStart[2] = snap->ps.playerMaxs[2] + snap->ps.psOrigin[2] - 1.0f; /* 0x3002683f..0x3002684e */
        } else {
            /* 0x30026850..0x3002686a: FCOMPP compares (mins.z + 1) vs dz; TEST AH,0x41; JNZ
             * skips the store. JNZ taken when (mins.z+1) <= dz (equal or less), so the store
             * runs only when dz < (mins.z + 1.0f); otherwise traceStart.z stays flamePos.z. */
            if (dz < snap->ps.playerMins[2] + 1.0f) {    /* 0x3002685d FADD [0x3007bce0] */
                traceStart[2] = snap->ps.playerMins[2] + snap->ps.psOrigin[2] + 1.0f; /* 0x3002686c..0x3002687b */
            }
        }

        /* 0x3002687f..0x30026894: trace from traceStart to flamePos through the projection
         * trap. CG_Trace(handle, origin=traceStart, flags=0, out, arg1=flamePos,
         * arg2=0, arg3=-1). Register ABI: EAX=handle, ECX=origin, EBX=flags. */
        {
            trace_t out;

            CG_Trace(CG_FLAME_DAMAGE_TRACE_MODEL, traceStart, 0, &out, flamePos, 0, -1);

            /* 0x30026899..0x300268b7: accept when the projection reached the flame
             * (out.fraction == 1.0f, compared as raw bits 0x3f800000) OR when it struck
             * the local player's own entity (out.entityNum == cg_snap->ps.psClientNum). */
            snap = cg_snap;                              /* 0x300268a5 reload MOV EAX,[0x30459160] */
            if (CG_FloatBits(out.fraction) != 0x3f800000) { /* 0x300268a0 raw dword CMP; JZ */
                /* With the wrapper's four live arguments removed, [ESP+0x60] is
                 * out+0x28: the standard trace_t.entityNum word. */
                if ((int32_t)out.entityNum != snap->ps.psClientNum) { /* 0x300268b1 CMP EDX,[EAX+0xe0] */
                    return;                              /* 0x300268b7 JNZ ret */
                }
            }

            /* 0x300268b9..0x300268c6: the local player is taking flame damage. Notify the
             * server for this client (throttled inside CG_SendFlameDamageCommand) and latch
             * the once-per-frame flag so no further command is sent this frame. */
            CG_SendFlameDamageCommand(snap->ps.psClientNum, ownerClientNum); /* 0x300268c1 */
            cg_flameDamageTakenThisFrame = 1;            /* 0x300268c6 */
        }
    }
}
