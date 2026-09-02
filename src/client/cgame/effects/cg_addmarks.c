// Source: uo_cgame_mp_x86.dll 0x3002e8c0..0x3002ea14
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002e8c0_3002ea14.mcode
//
// CG_AddMarks — age, fade, and re-submit every active mark-poly (decal) each
// frame. Walks the circular doubly-linked active list rooted at the
// cg_activeMarkPolys sentinel: marks whose lifetime has fully elapsed are
// unlinked and returned to cg_freeMarkPolys; the rest are re-emitted to the
// render scene via trap_R_AddPolyToScene, with their vertex colors faded during
// the final duration*0.15 ms of their life.
//
// The .mcode ships a size-matched name guess `DObjSkel2MatrixMultiply43`
// (matched only by byte size 0x154==0x154, which the naming rules forbid). It is
// REJECTED: this body does no skeleton/matrix work. It is the cgame mark aging
// pass, proven by (a) gating on cg_marks_vmCvar.integer exactly as CG_ImpactMark
// (0x3002e520) does, (b) traversing the cg_activeMarkPolys sentinel list and
// recycling nodes onto cg_freeMarkPolys, and (c) re-emitting each mark via
// trap(CG_R_ADDPOLYTOSCENE, markShader, numVerts, verts) — the exact re-emit the
// CG_R_ADDPOLYTOSCENE enum comment records for 0x3002e8c0. The name is the
// id-Tech CG_AddMarks, adopted by proven role.
//
// Machine-code facts proven for every behavior-affecting statement:
//   3002e8c0  EAX = cg_marks_vmCvar.integer; if (EAX == 0) return;   [empty prologue]
//   3002e8d1  ESI = cg_activeMarkPolys.nextMark (0x30412d44)
//   3002e8d7  if (ESI == &cg_activeMarkPolys) return;            [empty list]
// Per active mark mp (= ESI):
//   3002e8f0  duration = mp->duration (+0x170)  (saved into [ESP+0x14])
//   3002e8f6  markTime = mp->markTime (+0x8)
//   3002e8f9  EBP = mp->nextMark (+0x4)  (saved BEFORE any unlink so the walk
//             survives recycling this node)
//   3002e900  expiry = markTime + duration   (32-bit ADD)
//   3002e902  cg.time = cg_time (0x304831b0)
//   3002e908  CMP cg.time, expiry ; JLE fade  => expired iff (cg.time > expiry),
//             i.e. signed `if (cg.time > markTime + duration)`.
// Expired branch (0x3002e90c):
//   3002e90c  if (mp->prevMark == NULL)   [+0x0]
//   3002e916      Com_ErrorMessage("CG_FreeLocalEntity: not active"); [string 0x30077908]
//   unlink from active list:
//   3002e923  mp->prevMark->nextMark = mp->nextMark;
//   3002e930  mp->nextMark->prevMark = mp->prevMark;
//   push onto free list:
//   3002e932  mp->nextMark = cg_freeMarkPolys;
//   3002e935  cg_freeMarkPolys = mp;
//   3002e93b  goto advance (skip render)
// Fade/render branch (0x3002e940):
//   3002e940  timeLeft = expiry - cg.time   (== markTime + duration - cg.time)
//   3002e946  ST = (float)timeLeft
//   3002e94a  ST = (float)duration ; FMUL 0.15f => fadeTime = duration*0.15f
//   3002e956  FCOMP timeLeft vs fadeTime ; TEST AH,0x5 ; JP  => fade only entered
//             when (timeLeft < fadeTime); otherwise draw at full color.
//   fade (timeLeft < fadeTime), x87 stack traced to balance:
//   3002e965  fadeAlpha = Q_rint( (timeLeft * mp->colors[3] * 255.0f)
//                                 / (duration*0.15f) );  [+0x20 = alpha]
//             stored as a byte into [ESP+0x14].
//   3002e977  if (mp->alphaFade != 0)  [+0x10]
//     alphaFade set: write only the alpha byte (fadeAlpha) into every vert's
//       modulate[3] (+0x4f, stride 0x20), for i in [0, mp->numVerts).
//     alphaFade clear (0x3002e99f): modulate R/G/B of every vert by fadeAlpha:
//       modulate[0] = Q_rint(fadeAlpha * mp->colors[0]);  [+0x14 red, byte +0x4c]
//       modulate[1] = Q_rint(fadeAlpha * mp->colors[1]);  [+0x18 grn, byte +0x4d]
//       modulate[2] = Q_rint(fadeAlpha * mp->colors[2]);  [+0x1c blu, byte +0x4e]
//   Re-emit (0x3002e9e7):
//   3002e9f5  cgame_syscall(CG_R_ADDPOLYTOSCENE, mp->markShader (+0xc),
//                           mp->numVerts (+0x28), &mp->verts (+0x30));  [ADD ESP,0x10]
//   Advance (0x3002e9fe): mp = EBP; if (mp != &cg_activeMarkPolys) loop.
//
// Q_rint takes its argument in the x87 ST0 register and returns the rounded int
// in EAX; only AL (the low byte) is stored, so each modulate byte is the rounded
// color wrapped to 8 bits by the byte store. The FCOMP/TEST AH,0x5/JP idiom is
// the id-Tech `a < b` float compare (C0 set iff ST0 < ST1). All 32-bit adds are
// natural signed cg.time arithmetic.

#include "client/cgame/client_recovered.h"

void CG_AddMarks(void)
{
    markPoly_t *mp;
    markPoly_t *next;

    if (cg_marks_vmCvar.integer == 0) {
        return;
    }

    for (mp = cg_activeMarkPolys.nextMark; mp != &cg_activeMarkPolys; mp = next) {
        int32_t markTime = mp->markTime;
        int32_t duration = mp->duration;
        int32_t expiry;
        int32_t timeLeft;
        long double fadeTime;

        /* Save the forward link before this node may be unlinked/recycled. */
        next = mp->nextMark;

        expiry = coduo_int32_from_bits((uint32_t)markTime + (uint32_t)duration);

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((int32_t)cg_time > expiry) {
            /* Mark has fully elapsed: unlink it and return it to the free list. */
            if (mp->prevMark == NULL) {
                Com_ErrorMessage("CG_FreeLocalEntity: not active");
            }
            mp->prevMark->nextMark = mp->nextMark;
            mp->nextMark->prevMark = mp->prevMark;
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            mp->prevMark = NULL;
            mp->nextMark = cg_freeMarkPolys;
            cg_freeMarkPolys = mp;
            continue;
        }

        timeLeft = coduo_int32_from_bits((uint32_t)expiry - cg_time);
        /* 3002e94a FILD duration ; 3002e94e FMUL 0.15f: duration is a raw extended
         * int (no (float) narrowing) and the product is kept on the x87 stack (st1)
         * across the compare and divide below -- it is never stored to a float slot.
         * Modelled as long double so no round-to-float is introduced. */
        fadeTime = (long double)duration * markFadeFraction; /* duration * 0.15f */

        /* 3002e946 FILD timeLeft is a raw extended int compared against the extended
         * fadeTime -- neither operand is narrowed to float (no FSTP DWORD). */
        if ((long double)timeLeft < fadeTime) {
            int32_t i;
            /* Fade fraction scaled to a color byte, using the mark's alpha.
             * 3002e963..e96e: (timeLeft * colors[3] * 255.0f) / fadeTime kept entirely
             * in the wide register and handed to _ftol2 (0x3002e970) unrounded;
             * timeLeft is still the raw FILD int, so no (float) cast. */
            int32_t fadeAlpha = coduo_fp_to_i32_extended(((long double)timeLeft * mp->colors[3] * colorByteScale) / fadeTime);

            if (mp->alphaFade != 0) {
                /* Fade only the alpha byte of every vertex. */
                for (i = 0; i < mp->numVerts; i++) {
                    mp->verts[i].modulate[3] = (uint8_t)fadeAlpha;
                }
            } else {
                /* Modulate each vertex's RGB by the fade value.
                 * 3002e9a8 FILD fadeAlpha once (raw extended int, kept in st0 across
                 * the loop); each modulate = _ftol2(fadeAlpha * colors[k]) via a bare
                 * FILD -- fadeAlpha is not narrowed to float. */
                for (i = 0; i < mp->numVerts; i++) {
                    mp->verts[i].modulate[0] = (uint8_t)coduo_fp_to_i32_extended((long double)fadeAlpha * (long double)mp->colors[0]);
                    mp->verts[i].modulate[1] = (uint8_t)coduo_fp_to_i32_extended((long double)fadeAlpha * (long double)mp->colors[1]);
                    mp->verts[i].modulate[2] = (uint8_t)coduo_fp_to_i32_extended((long double)fadeAlpha * (long double)mp->colors[2]);
                }
            }
        }

        cgame_syscall(CG_R_ADDPOLYTOSCENE, mp->markShader, mp->numVerts, mp->verts);
    }
}
