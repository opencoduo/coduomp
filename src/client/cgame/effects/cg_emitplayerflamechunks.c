// Source: uo_cgame_mp_x86.dll 0x30024050..0x30025561
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30024050_30025561.mcode
//
// CG_EmitPlayerFlameChunks (mechanical .mcode name CG_AddFlameChunks; the real
// per-owner-chunk frame updater CG_AddFlameChunks is a DIFFERENT function at
// 0x300272b0, already reconstructed. This 0x30024050 giant is the flame-chunk
// EMITTER: it spawns a run of flame chunks along a fire path for one owner and
// stitches them onto the flame-info emit state cg_flameInfo[owner]).
//
// ============================================================================
// REGION-SPLIT RECONSTRUCTION — all three phases complete.
//   * Frame/local scaffold, entry register+stack ABI ........ DONE (below)
//   * REGION A  (setup, 0x30024050..0x30024817) ............. RECONSTRUCTED (phase 3)
//   * MAIN EMIT LOOP (0x30024817..0x300251f4) ............... RECONSTRUCTED (phase 2)
//   * TERMINAL-CHUNK STAMP + retry (0x30025201/0x3002521b
//               ..0x300254da) ............................... RECONSTRUCTED (phase 2)
//   * REGION C  (info writeback, 0x300254dd..0x30025561) .... RECONSTRUCTED
// PHASE 3 (this pass) filled Region A's pre-loop x87 SETUP ARITHMETIC
// instruction-by-instruction from objdump: the frametime/angle-delta rate limiter
// (radiusClamp), the two AngleVectors bases, the emitOrigin/emitBasis blend, the
// owner-liveness gate + vDir spawn direction, chunkCount / regionATemp10, the
// CG_Trace adsGate path, the alpha reseed, and the emit-time cursor span
// (dblTemp68/chunkEndTime/timeStep/emitTimeDbl/radiusSeed). Every FLD/FMUL/FSUB/
// FSUBR/FDIV(R)/FSTP, FILD/FISTP, FCOMP/FNSTSW/TEST-AH parity, and .rdata constant
// address is proven against the bytes. The terminal-chunk gate now branches via
// `goto region_c_terminalChunk`. ONE documented TODO remains: the [ESP+0x167]
// local flag byte in the trace path (its writer is outside the traced range).
// ============================================================================
//
// -------------------------- ENTRY REGISTER + STACK ABI ----------------------
// Custom __usercall (proven from the two call sites 0x300346a0 and 0x30046387):
//     EAX               = vec3_t   dir       (aim/forward direction; caller
//                                             passes &localVec, e.g. LEA [ESP+0x34])
//     [ESP+arg0]        = centity_t *cent    (emitter entity; cent[0]=owner
//                                             clientNum, cent+0x208 = flame origin
//                                             vec3, cent+0x20c... , cent+0xcc used)
//     [ESP+arg1]        = vec3_t   angles    (emitter angles; AngleVectors input)
//     [ESP+arg2]        = float    alpha     (1.8f at both sites; life/scale seed;
//                                             this slot is READ-MODIFY-WRITTEN)
//     [ESP+arg3]        = int32_t  arg3      (flag, 0 or 1; selects several const
//                                             pairs and the sprite-shader table)
//     [ESP+arg4]        = int32_t  arg4      (0 at both sites; stamped to chunk
//                                             field_28 and cent-info fields)
// After `SUB ESP,0x158` + PUSH EBX,EBP,ESI (12 bytes) the first arg read
// (0x30024061 MOV EAX,[ESP+0x168]) sees arg0 at +0x168; every later arg read is
// after PUSH EDI (0x30024070), so post-EDI the args sit at +0x16c..+0x17c:
//     arg0=[ESP+0x16c]  arg1=[ESP+0x170]  arg2=[ESP+0x174]
//     arg3=[ESP+0x178]  arg4=[ESP+0x17c]
// Callee-saved: EBX, EBP, ESI, EDI (pushed at entry, popped before both RETs).
// Return: void (no meaningful return value; EAX left as last computed).
// Both exits are plain `RET` (no immediate) => cdecl stack cleanup by the caller
// for the five stack args; do NOT add a calling-convention attribute (syntax-
// only build). Frame: `SUB ESP,0x158` = 0x158 (344) bytes of locals.
//
// ---------------------------- FRAME / LOCAL SLOT MAP ------------------------
// Slots are [ESP+off] at the base frame (before the transient `SUB ESP,8`
// brackets used to pass 8-byte double args to floor()/the wave helper). Roles
// proven from Region A / Region C; loop-interior-only slots are marked (loop).
//   +0x10  scratch int/float (rounding & compare temp; reused throughout)
//   +0x14  scratch int (rand() result temp; also a double half at [+0x14] in the
//          transient bracket)                                          (loop/C)
//   +0x18  float  radiusClamp  (arg2-derived, clamped to <=200.0f; == field size)
//   +0x1c  float  frametimeFrac (cg.frametime as float, clamped; also holds an
//          int chunk-count = Q_rint((arg2+1)*0.5f*1666.0f))  [Q_rint = CRT _ftol2, truncates]
//   +0x20  float  vDir.x   (dir-derived spawn direction x)  [also loop uses]
//   +0x24  float  vDir.y
//   +0x28  float  vDir.z
//   +0x2c  float  velOrAng temp (cg_snap velocity / basis)          (loop/C)
//   +0x30  float  temp                                              (loop/C)
//   +0x34  float  temp                                              (loop/C)
//   +0x38  float  emitOrigin.x  (cent origin blended with dir*4.0)   built A
//   +0x3c  float  emitOrigin.y
//   +0x40  float  emitOrigin.z  (also reused as int/float temp)
//   +0x44  int32_t isLocalFirstPerson  (cent owner == cg_snap->ps.psClientNum &&
//                                       g_data..304831c0 != 0)         set A
//   +0x54  float  dir.x    (copy of EAX/dir vec, arg to AngleVectors #? no)
//   +0x58  float  dir.y
//   +0x5c  float  dir.z    (dir vec copied from ESI = EAX at 0x300241e2)
//   +0x60  int    rand temp                                          (loop)
//   +0x68  double dblTemp                                             (loop)
//   +0x6c  ...    (upper half of +0x68)
//   +0x70  float  chunkPos.x   (VectorNormalize workspace)          (loop)
//   +0x74  float  chunkPos.y
//   +0x78  float  chunkPos.z
//   +0x7c  float  angInfo2.x (AngleVectors#2 angles IN, +0x7c/0x80/0x84; filled
//          from info->{field_04,08,0c})                                     A
//   +0x80  float  angInfo2.y
//   +0x84  float  angInfo2.z
//   +0x88  float  emitBasis.x  (+0x88/0x8c/0x90 = origin blended with basis, loop)
//   +0x8c  float  basisF.y
//   +0x90  float  basisF.z
//   +0x94  float  basisFullF.x (AngleVectors#1 FORWARD out, +0x94/0x98/0x9c)  A
//   +0x98  float  basisFullF.y
//   +0x9c  float  basisFullF.z
//   +0xa0  int32_t adsGate  (0, set to 1 on the CG_Trace-hit path)  A
//   +0xa4  float  accum.x                                             (loop)
//   +0xa8  float  accum.y                                             (loop)
//   +0xac  float  accum.z                                             (loop)
//   +0xb0  float  chunkDrift.x                                        (loop)
//   +0xb4  float  chunkDrift.y
//   +0xb8  float  chunkDrift.z
//   +0xbc..+0xd0  three vec3 workspaces for the per-chunk basis xforms (loop)
//   +0xd4  float  basisR.x  (AngleVectors#1 UP out, +0xd4/0xd8/0xdc)         A
//   +0xd8  float  basisR.y
//   +0xdc  float  basisR.z
//   +0xe0  float  infoDir.x (+0xe0/0xe4/0xe8 = info->{field_34,field_38,field_3c};
//          the emitter's previous-frame direction, used by the loop).         A
//   +0xe4  float  infoDir.y
//   +0xe8  float  infoDir.z
//   +0xf0  double posDbl (double chunk position accumulator)            (loop)
//   +0xf8  double emitTimeDbl (the running emit-time cursor; loop induction #2) A
//   +0x100 float  basisFwd2.x (AngleVectors#2 FORWARD out, +0x100/0x104/0x108) A
//   +0x104 float  basisFwd2.y
//   +0x108 float  basisFwd2.z
//   +0x10c float  angRight2.x (AngleVectors#2 RIGHT out, +0x10c/0x110/0x114)  A
//   +0x118 float  basisUp2.x  (AngleVectors#2 UP out, +0x118/0x11c/0x120)     A
//   +0x11c float  basisUp2.y
//   +0x120 float  basisUp2.z
//   +0x124 float  angRight1.x (AngleVectors#1 RIGHT out, +0x124/0x128/0x12c)  A
//   +0x130 double  traceDbl  (CG_Trace result temp)            (loop)
//   +0x13c float  traceResult.frac hi region (the trace_t buffer
//                 for CG_Trace lives at +0x13c..+0x148+; +0x148 is the
//                 fraction field read at 0x300244ce)                       A
//   +0x144 ...     trace buffer body
//   +0x148 float  traceResult.fraction (FCOMP'd against 1.0f)             A
//   +0x167 uint8_t argByte  (byte read from [ESP+0x167] == low byte of arg... a
//                 caller flag byte inside the arg region; gates the trace path)  A
//   +0x168..+0x17c  incoming stack args (see ABI above; NOT locals)
//
// (The 0x158-byte frame is larger than the union of named slots because the
// trace-result buffer, the AngleVectors output basis, and the per-chunk vec3
// workspaces overlap different phases; the numeric offsets above are the ground
// truth the phase-2 loop must honor.)
//
// ---------------------- .rdata FLOAT / DOUBLE CONSTANT POOL -----------------
// (dumped from the binary; used across Region A and the teardown)
//   float  @0x3007bdb4 = 0.01f        @0x3007c130 = 1e-4f (~0.0001)
//   float  @0x3007bd94 = 0.001f       @0x3007bf44 = 200.0f
//   float  @0x3007be40 = 4.0f         @0x3007bcec = 0.0f
//   float  @0x3007bce0 = 1.0f         @0x3007bce8 = 0.5f
//   float  @0x3007be88 = 1000.0f      @0x3007c120 = 1666.0f
//   float  @0x3007be38 = 0.75f        @0x3007bec0 = 3.0517578e-05f (1/32768)
//   float  @0x3007bd54 = 360.0f       @0x3007bea0 = 0.3f
//   double @0x3007c128 = 20.0         @0x3007bcf8 = 1.0
//   double @0x3007bd08 = 1000.0       @0x3007bd28 = 0.5
//   double @0x3007bcf0 = 0.0
//   .data  @0x30450148 = a float global (cg_flameSpawnScale_30450148, refs=1)

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/coduo_native_x87.h"

#include <math.h>

/* -------- provisional callees not yet in the shared header (single-use here) -----
 * 0x3004d200 (FUN_3004d200_3004d242): a 3-float turbulence/wave helper. Its own
 * body computes cos(a*(t/1000)*k)*sin(b*(t/1000)*k)-style noise from (t, b, a);
 * caller cleans 3 stack dwords (ADD ESP,0xc). Provisional; ARITY/TYPES proven
 * from THIS caller's pushes (3 floats) and the callee body, not from a decl.
 * Declared centrally because it is a cross-translation-unit game helper. */

/* The loop's owner<64 turbulence term (0x30025089..0x300250a5) computes an inline
 * x87 FSQRT of dot(vDir,vDir); expressed with the C library sqrt (single isolated
 * math op, not a substitute for source recovery). */

/* Region A's angle-delta rate limiter (0x3002413c/0x30024182) uses the x87 FABS
 * instruction on the AngleNormalize180 result; expressed with the C
 * library single-precision fabsf (one isolated math op). */

/* 0x3005bcd0: statically-linked MSVC floor(double). Reuses the name the sibling
 * flame function FUN_30029210 already gave this address (symbol convergence, not
 * an alias). 0x3006be3c is the CRT `_ftol2` float->int helper (truncates toward
 * zero); its evidence is documented in the shared header, while portable code
 * calls coduo_fp_to_i32_extended explicitly. */

/* CG_Trace push at 0x300244c4/0x30024674 uses two .rdata table pointers:
 * 0x300851c4 (an immediate/config), 0x300851d0 (a 4-byte table, owner cg_addflamechunks).
 * These stay address-shaped in the loop stub; phase 2 resolves them from the
 * loop body's exact use. */

/* Signature matches the shared header decl. Parameter aliases used in the body:
 *   viewAngles  == the `dir` register-vec (EAX)      cent == emitter (centity_t)
 *   flashOrigin == AngleVectors input angles         spread == arg2 float (RMW slot)
 *   a3 == int flag (must be nonzero)                a4 == int (0 at both call sites)
 * `cent` is an incomplete centity_t; Region A reads only cent[0] (owner
 * clientNum) via a cast at its proven offset (the loop reads cent+0x208 etc.). */

void CG_EmitPlayerFlameChunks(vec3_t viewAngles, centity_t *cent, vec3_t flashOrigin, float spread, int32_t a3, int32_t a4)
{
    /* vec3_t params decay to float*; use them directly. `angles` == flashOrigin.
     * alpha is the [ESP+0x174] arg2 slot; the machine code read-modify-writes it
     * (the reseed at 0x3002457d..0x300245bc), so it is a mutable local, not const. */
    float alpha = spread; /* [ESP+0x174] arg2 (RMW slot) */
    const int32_t arg3 = a3;
    const int32_t arg4 = a4;
    /* -------- local frame (named where Region A / Region C prove a role) -------- */
    cgFlameInfo_t *info;            /* EBP: &cg_flameInfo[owner], stride 0xb8 */
    int32_t owner;           /* cent's owner client/entity number = cent[0] */
    int32_t isLocalFirstPerson;      /* [ESP+0x44] */
    int32_t adsGate;         /* [ESP+0xa0] */
    int32_t spawnEnabled;    /* EDI: the loop-entry / terminal-chunk predicate */
    float radiusClamp;     /* [ESP+0x18] (Region A) */
    float frametimeFrac;   /* [ESP+0x1c] (float)cg_frametime, min-clamped 1e-4f */
    int32_t chunkCount;      /* Q_rint((alpha+1)*0.5f*1666.0f) (Region A) */
    /* Far-outside sentinel init: determinizes the pre-construction
     * CG_PointContents read below (see the ORIGINAL_BINARY_BUG block). */
    vec3_t emitOrigin = {1.0e30f, 1.0e30f, 1.0e30f}; /* [ESP+0x38..0x40] */
    vec3_t dirCopy;         /* [ESP+0x54..0x5c] (copy of `dir`) */
    vec3_t basisFullF;      /* [ESP+0x94..0x9c] AngleVectors#1 forward out */
    vec3_t angRight1;       /* [ESP+0x124..0x12c] AngleVectors#1 right out */
    vec3_t basisR;          /* [ESP+0xd4..0xdc] AngleVectors#1 up out */
    vec3_t angInfo2;        /* [ESP+0x7c..0x84] AngleVectors#2 angles in (reused) */
    vec3_t basisFwd2;       /* [ESP+0x100..0x108] AngleVectors#2 forward out */
    vec3_t angRight2;       /* [ESP+0x10c..0x114] AngleVectors#2 right out */
    vec3_t basisUp2;        /* [ESP+0x118..0x120] AngleVectors#2 up out */
    double emitTimeDbl;     /* [ESP+0xf8] running emit-time cursor (Region A) */

    trace_t traceBuf;     /* [ESP+0x138..0x167] CG_Trace scratch */

    /* regionATemp10 == the Region-A [ESP+0x10] value the terminal stamp reads
     * (0x300252e6); proven at 0x3002446e to be (alpha+1.0f)*0.5f. The terminal
     * path is entered via `goto region_c_terminalChunk` when Region A's
     * single-terminal-chunk gate (0x3002453a/45/55) fires instead of the loop. */
    float regionATemp10 = 0.0f;

    /* -------- loop-carried working values produced by Region A ------------- */
    /* Frame slots / registers Region A hands the loop and the terminal block.
     *   EDI(chunk) = info->field_40 on entry (0x30024605); ESI(prevChunk)=old EDI;
     *   EBX(radiusSeed) from Region A's Q_rint (0x300247d2);
     *   [0x48]=chunkEndTime, [0x130]=timeStep, [0xf8]=emitTimeDbl (cursor),
     *   [0x68]=dblTemp68 (per-chunk time span);
     *   vDir=[0x20], emitBasis=[0x88], rowEmit=[0xec], infoDir2=[0xe0]. */
    flameChunk_t *chunk;               /* EDI = info->field_40 on entry */
    flameChunk_t *prevChunk;          /* ESI = old EDI */
    int32_t radiusSeed = 0;     /* EBX */
    double chunkEndTime = 0.0; /* [0x48] */
    double timeStep = 0.0;     /* [0x130] */
    double dblTemp68 = 0.0;    /* [0x68] */
    double chunkSpanScratch = 0.0; /* [0x60] loop scratch */
    vec3_t vDir = {0.0f, 0.0f, 0.0f}; /* [0x20] */
    vec3_t emitBasis = {0.0f, 0.0f, 0.0f}; /* [0x88] */
    vec3_t rowEmit = {0.0f, 0.0f, 0.0f}; /* [0xec] */
    vec3_t infoDir2 = {0.0f, 0.0f, 0.0f}; /* [0xe0] */

    /* ==================================================================== */
    /* REGION A — SETUP (0x30024050 .. 0x30024817)                          */
    /* ==================================================================== */

    /* owner = cent[0]; info = &cg_flameInfo[owner]  (0x30024061..0x300240a7)
     * isLocalFirstPerson = 1 iff the flame belongs to the local player
     * (owner == cg_snap->ps.psClientNum) AND the view-state gate g_data..304831c0 is
     * CLEAR (== 0); otherwise 0.  Exact branch shape (0x30024071..0x30024086):
     *   owner != clientNum  -> [0x44] = 0                    (JNZ 0x30024084)
     *   owner == clientNum && 304831c0 == 0 -> [0x44] = 1    (JZ skips the store-0)
     *   owner == clientNum && 304831c0 != 0 -> [0x44] = 0    (falls through)
     * (The store of 1 at 0x3002407a happens first, then the falls-through store
     * of 0 overwrites it when 304831c0 != 0.) */
    owner = cent->currentState.number;   /* cent[0] = emitter owner clientNum */
    info = &cg_flameInfo[owner];
    if (owner == cg_snap->ps.psClientNum && cg_thirdPerson == 0) {
        isLocalFirstPerson = 1;
    } else {
        isLocalFirstPerson = 0;
    }
    adsGate = 0;                                   /* [ESP+0xa0] = 0 */

    /* if (alpha <= 0.01f) -> BARE epilogue at 0x30025556 (no info writeback)
     * (0x3002408c FCOMP 0.01f; JNP 0x30025556). */
    if (alpha <= 0.01f) {
        return;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (CG_PointContents(emitOrigin, owner, 0x20) != 0) {
        return;   /* 0x300240cf JNZ 0x30025556 (bare epilogue) */
    }

    /* if (arg3 == 0) -> bare epilogue  (0x300240d5 MOV EAX,[arg3]; TEST; JZ 0x30025556) */
    if (arg3 == 0) {
        return;
    }

    /* cg_flameTime = Q_rint( floor( (double)cg_time * 2 ) )
     * (0x300240e4 FILD [0x304831b0]=cg_time; FADD ST0,ST0 (=*2); FSTP dbl;
     *  CALL floor (0x3005bcd0); CALL 0x3006be3c -> EAX; MOV [0x300ab718],EAX). */
    {
        double t2 = (double)coduo_int32_from_bits(cg_time);
        t2 = t2 + t2;
        cg_flameTime = (uint32_t)coduo_fp_to_i32_extended(floor(t2)); /* raw st0 into _ftol2 */
    }

    /* frametimeFrac = (float)cg_frametime (0x300240ff FILD; 0x3002410a FST [0x1c]).
     * clampedFrametime = max(frametimeFrac, 1e-4f): FCOMP against 1e-4f
     * (0x3007c130); TEST AH,0x41; when frametime <= 1e-4f the divisor is replaced
     * with 1e-4f (bit pattern 0x38d1b717). (0x3002410e..0x30024129) */
    frametimeFrac = (float)cg_frametime;
    float clampedFrametime = (frametimeFrac > 1e-4f) ? frametimeFrac : 1e-4f;

    /* --- angle-delta blocks (0x3002412d..0x300241c7): a per-axis emit-spread rate
     * limiter. Each axis takes fabs(AngleNormalize180(dir[k]-info->field))
     * divided by (clampedFrametime * 0.001f), and radiusClamp keeps the larger.
     *   deltaY (0x3002412d): dir[1]-info->field_08 -> stored to [0x18]
     *   deltaX (0x30024151): dir[0]-info->field_04 -> [0x18] = max(deltaX, deltaY)
     *     (0x30024193 FCOMP [0x18]; TEST AH,0x5; JNP keep-first else store deltaX).
     * The recompute in the JNP-not-taken tail re-materialises the same deltaX. */
    {
        /* clampedFrametime*0.001f is recomputed on the x87 stack per axis
         * (0x30024145/0x3002418b FMUL) and never spilled, and deltaX stays in
         * st0 through the FCOMP at 0x30024193 (it is rounded to float only if
         * it wins the max, FSTP [0x18] at 0x300241c3) - keep both 80-bit. */
        const long double divisor = (long double)clampedFrametime * (long double)0.001f; /* 0x3007bd94 = 0.001f */
        const float deltaY = fabsf(AngleNormalize180(viewAngles[1] - info->prevDir[1] /* +0x08 */)) / divisor;
        const long double deltaX = (long double)fabsf(AngleNormalize180(viewAngles[0] - info->prevDir[0] /* +0x04 */)) / divisor;
        /* 0x3002419c takes the stored deltaY only for ordered deltaX<deltaY;
         * equality, greater-than, and unordered all store deltaX. */
        radiusClamp = (deltaX < (long double)deltaY) ? deltaY : (float)deltaX;
        /* clamp radiusClamp <= 200.0f (0x300241c9 FCOMP 200.0f @0x3007bf44;
         * TEST AH,0x5; JNP skip else store 0x43480000=200.0f). */
        if (!(radiusClamp < 200.0f)) {
            radiusClamp = 200.0f;
        }
    }

    /* dirCopy = viewAngles triple ([ESI]/[ESI+4]/[ESI+8] -> +0x54/0x58/0x5c);
     * infoDir2 = info->emitDir (+0x34/+0x38/+0x3c), the emitter's previous-frame
     * direction the loop's accum2 lerps. (0x300241e2..0x30024226) */
    dirCopy[0] = viewAngles[0];
    dirCopy[1] = viewAngles[1];
    dirCopy[2] = viewAngles[2];
    infoDir2[0] = info->emitDir[0];
    infoDir2[1] = info->emitDir[1];
    infoDir2[2] = info->emitDir[2];

    /* AngleVectors #1 (0x30024206..0x3002422d): angles=dirCopy(+0x54),
     * forward=basisFullF(+0x94), right=angRight1(+0x124), up=basisR(+0xd4).
     * The first basis is built from `dirCopy` (the register dir vec). */
    AngleVectors(dirCopy, basisFullF, angRight1, basisR);

    /* emitOrigin[i] = flashOrigin[i] + basisR[i]*4.0f + basisFullF[i]*4.0f
     * (0x30024232..0x300242f8; 4.0f @0x3007be40). emitBasis (+0x88..) is a copy of
     * emitOrigin. The scalar reads/writes go through int-typed slot copies in the
     * machine code; the value is the float sum computed on the x87 stack. */
    /* ASYMMETRIC SPILL: component 0 stores its partial sum to [ESP+0x38] and
     * reloads it (0x30024269 FSTP / 0x3002429e FADD) - an extra float rounding
     * components 1 and 2 do not have (their partials ride the x87 stack). */
    {
        const float partial0 = flashOrigin[0] + basisR[0] * 4.0f;
        emitOrigin[0] = partial0 + basisFullF[0] * 4.0f;
    }
    emitOrigin[1] = (float)(((long double)flashOrigin[1] + (long double)basisR[1] * 4.0f) + (long double)basisFullF[1] * 4.0f);
    emitOrigin[2] = (float)(((long double)flashOrigin[2] + (long double)basisR[2] * 4.0f) + (long double)basisFullF[2] * 4.0f);
    emitBasis[0] = emitOrigin[0];
    emitBasis[1] = emitOrigin[1];
    emitBasis[2] = emitOrigin[2];

    /* emitter-recency gate (0x3002430b..0x30024435): sets spawnEnabled (EDI) and the
     * spawn-direction vDir (+0x20..). owner=cent[0]; clientNum=cg_snap->ps.psClientNum;
     * frameCutoff = cg_clientFrame - 1. vDir is initialised to {0,0} (+0x20/0x24)
     * (0x3002425f/57); vDir.z stays 0 unless a branch writes it. */
    spawnEnabled = 0;
    /* vDir.x/.y are zeroed at 0x3002425f/57. The Z component NEVER touches
     * [ESP+0x28] before the *0.5f convergence: on the velocity path
     * (0x30024381) and the spawn path (0x30024433 FMULP) it rides st0 into the
     * FMUL 0.5f (0x30024451) and is rounded ONCE at the FSTP [0x28]; when
     * neither path fires it is the leftover x87 0.0f (0x3002430f FLD).
     * `vz` models that st0-carried value. */
    vDir[0] = 0.0f;
    vDir[1] = 0.0f;
    long double vz = 0.0f;
    {
        const int32_t clientNum = cg_snap->ps.psClientNum;
        const int32_t frameCutoff = coduo_int32_from_bits((uint32_t)cg_clientFrame - 1u);
        const int32_t lastClientFrame = info->clientFrame;
        if (owner == clientNum) {
            /* 0x30024315 JE -> 0x3002432a: a current/recent local emitter may spawn. */
            if (lastClientFrame >= frameCutoff) {
                spawnEnabled = 1;
            }
        } else if (lastClientFrame >= frameCutoff) {
            spawnEnabled = 1;                    /* 0x30024319 JGE */
        } else {
            /* 0x3002431b: if info->lastEmitTime >= cg_flameTime-150, spawnEnabled=1. */
            const int32_t recentCutoff = coduo_int32_from_bits(cg_flameTime - 150u);
            if (info->lastEmitTime >= recentCutoff) {
                spawnEnabled = 1;
            }
        }

        /* Control flow (0x30024333..0x30024433):
         *   owner==clientNum && |vel|>1.0f  -> vDir = velocity * 0.75f
         *   else if spawnEnabled            -> vDir = emitOrigin-delta scaled
         *   else                            -> vDir stays {0,0,0}
         * The |vel| test is only reachable when owner==clientNum; when it fails
         * the code falls through to the same `test edi,edi` gate as the
         * owner!=clientNum path. */
        int velPathTaken = 0;
        if (owner == clientNum) {
            /* local player: velocity magnitude from cg_snap+0x2c/0x30/0x34.
             * FSQRT of the dot; FCOMP 1.0f (0x3007bce0); TEST AH,0x41; JNE skip.
             * (0x30024337..0x3002438a) */
            const float velX = cg_snap->ps.velocity[0]; /* +0x2c */
            const float velY = cg_snap->ps.velocity[1]; /* +0x30 */
            const float velZ = cg_snap->ps.velocity[2]; /* +0x34 */
            /* dot product and FSQRT stay on the x87 stack (0x30024340..0x30024350);
             * the FCOMP 1.0f at 0x30024358 tests the UNROUNDED square root. */
            const long double speed = coduo_x87_sqrtl(((long double)velX * velX + (long double)velY * velY) + (long double)velZ * velZ);
            if (speed > 1.0f) {
                vDir[0] = velX * 0.75f;          /* 0x30024367 (@0x3007be38) */
                vDir[1] = velY * 0.75f;          /* 0x30024374 */
                vz = velZ * 0.75f;               /* 0x30024381: NOT stored; st0 to 0x30024435 */
                velPathTaken = 1;
            }
        }
        if (!velPathTaken && spawnEnabled != 0) {
            /* spawning (0x3002439e..0x30024433): vDir = (emitOrigin - info->field_10..)
             * scaled by 1000.0 / max(cg_flameTime - info->field_54, 20.0). */
            /* dx/dy are spilled ([0x7c]/[0x80] FSTP); dz (0x300243c4 FSUB) and
             * the 1000/denom quotient ride the x87 stack - the quotient is
             * recomputed per component (0x300243e2/0x30024407/0x3002442d
             * FDIVR) and never stored, so it is one 80-bit value. */
            const float dx = emitOrigin[0] - info->prevEmitOrigin[0]; /* +0x10 */
            const float dy = emitOrigin[1] - info->prevEmitOrigin[1]; /* +0x14 */
            const long double dz = (long double)emitOrigin[2] - (long double)info->prevEmitOrigin[2]; /* +0x18 */
            const int32_t dt = coduo_int32_from_bits(cg_flameTime - (uint32_t)info->lastEmitTime); /* +0x54 */
            /* denom = max((double)dt, 20.0) (0x3007c128 = 20.0). */
            const double denom = ((double)dt > 20.0) ? (double)dt : 20.0;
            const long double scale = (long double)1000.0f / (long double)denom; /* 1000.0f @0x3007be88 */
            vDir[0] = (float)(scale * dx);
            vDir[1] = (float)(scale * dy);
            vz = scale * dz;
        }
        /* both branches converge at 0x30024435: vDir *= 0.5f (@0x3007bce8). */
        vDir[0] = vDir[0] * 0.5f;
        vDir[1] = vDir[1] * 0.5f;
        vDir[2] = (float)(vz * 0.5f);            /* single rounding at [0x28] */
    }

    /* chunkCount = Q_rint((alpha + 1.0f) * 0.5f * 1666.0f); regionATemp10 stores
     * the (alpha+1.0f)*0.5f intermediate (0x3002446e FST [0x10]; 1666.0f @0x3007c120).
     * The FST keeps st0, so the *1666.0f runs on the UNROUNDED product. */
    {
        const long double alphaHalf = ((long double)alpha + (long double)1.0f) * (long double)0.5f;
        regionATemp10 = (float)alphaHalf;                    /* FST [0x10] */
        chunkCount = coduo_fp_to_i32_extended(alphaHalf * 1666.0f); /* 0x30024472: unrounded product into _ftol2 */
    }

    /* CG_Trace(handle=0x2810011, origin=&emitBasis, flags=0x300851d0,
     *   out=&traceBuf, arg1=&originCopy, arg2=0x300851c4, arg3=owner)
     * (0x30024481..0x300244c9). originCopy is a scratch vec3 filled from
     * cent->field_208/field_20c and cg_refdef.vieworg.z. */
    {
        vec3_t originCopy;
        originCopy[0] = cent->lerpOrigin[0];   /* +0x208 */
        originCopy[1] = cent->lerpOrigin[1];   /* +0x20c */
        originCopy[2] = cg_refdef.vieworg[2];
        CG_Trace(0x2810011, emitBasis, cg_flameTraceMaxs, &traceBuf, originCopy, cg_flameTraceMins, owner);
        /* If traceBuf.fraction < 1.0f OR traceBuf.startsolid != 0, set adsGate,
         * halve chunkCount, and replace emitBasis with the trace result vec
         * (traceBuf+0x4/0x8/0xc). (0x300244ce..0x30024531)
         *
         * The byte read at 0x300244e5 `MOV AL,[ESP+0x167]` is depth 0 (after the
         * ADD ESP,0x10 at 0x300244db) = base+0x167. The CG_Trace out pointer is
         * LEA EDX,[ESP+0x144] with 3 pushes outstanding = base+0x138, so the
         * 0x30-byte trace record spans base+0x138..base+0x167 and the byte is
         * traceBuf+0x2f — trace_t.startsolid, the same post-trace flag the rest
         * of the flame code tests (cf. 0x30028291 in CG_FireFlameChunks). */
        if (traceBuf.fraction < 1.0f || traceBuf.startsolid != 0) {
            adsGate = 1;                          /* [0xa0] = 1 */
            chunkCount = coduo_fp_to_i32_extended((float)chunkCount * 0.5f);
            emitBasis[0] = traceBuf.endpos[0];   /* +0x04 */
            emitBasis[1] = traceBuf.endpos[1];   /* +0x08 */
            emitBasis[2] = traceBuf.endpos[2];   /* +0x0c */
        }
    }

    /* per-info liveness gate (0x30024538..0x30024555): if NOT (spawnEnabled &&
     * info->field_40 && info->soundPathFlag == arg3), the emit takes the single
     * terminal-chunk path (0x3002453a/45/55 JE/JNE -> 0x3002521b), bypassing the
     * loop.  NB the compare is against [ESP+0x178] = arg3 (the arg2/alpha slot at
     * [0x174] shifted; arg3 lives at 0x178), not arg4. */
    if (!(spawnEnabled != 0 && info->ownerChunk != NULL && info->soundPathFlag == arg3)) {    /* +0x5c */
        goto region_c_terminalChunk;
    }

    /* non-terminal path: accumulate info->field_58 and optionally re-seed alpha.
     *   info->field_58 += cg_frametime  (0x30024562, unconditional)
     *   if (alpha > 1.0f) alpha = pow((noise+1)*0.5, 0.5) * 0.0f + 1.0f
     * APPARENT SOURCE ODDITY: the pow term is multiplied by 0.0f then 1.0f is
     * added, so the reseed always leaves alpha == 1.0f; the wave/pow chain is a
     * no-op computed anyway. Preserved faithfully. (0x3002455b..0x300245bc) */
    {
        /* 0x3002456b FCOMP float [0x3007bce0] — the pool 1.0f (0.0f lives at
         * 0x3007bcec); TEST AH,0x41; JNZ skip-reseed => the reseed (which always
         * ends in alpha = 1.0f) runs only when alpha > 1.0f. */
        const int reseedAlpha = (alpha > 1.0f);
        info->emitAccumTime = coduo_int32_from_bits((uint32_t)info->emitAccumTime + (uint32_t)cg_frametime); /* +0x58 */
        if (reseedAlpha) {
            /* CG_FlameWaveNoise(t=cg_flameTime, b=0.334234f, a=0.197865f)
             * (pushes 0x3e4a9d34=a, 0x3eab20b8=b, t). */
            float n = Q_SwayRand(0.33423399925231934f, 0.1978653073310852f, (float)coduo_int32_from_bits(cg_flameTime));
            /* base (n+1)*0.5 rides st0 raw into _CIpow (ST1; no store); the raw
             * st0 result takes FMUL 0.0f then FADD 1.0f, with a single FSTP float
             * to the alpha slot at 0x300245bc -- powl keeps the base (operand
             * widened) and result 80-bit, and the lone (float) narrows once. */
            alpha = (float)(powl(((long double)n + 1.0f) * 0.5f, 0.5L) * 0.0f + 1.0f);
        }
    }

    /* second dir setup + AngleVectors #2 (0x300245c3..0x300245f7):
     *   angInfo2 = info->{field_04,field_08,field_0c} (AV#2 input, +0x7c/0x80/0x84)
     *   AV#2 -> forward=basisFwd2(+0x100), right=angRight2(+0x10c), up=basisUp2(+0x118). */
    angInfo2[0] = info->prevDir[0]; /* +0x04 */
    angInfo2[1] = info->prevDir[1]; /* +0x08 */
    angInfo2[2] = info->prevDir[2]; /* +0x0c */
    AngleVectors(angInfo2, basisFwd2, angRight2, basisUp2); /* 0x300245f7 */

    /* more info-field copies (0x300245fc..0x30024645):
     *   rowEmit = info->{field_10,field_14,field_18}  (+0xec/0xf0/0xf4)
     *   angInfo2 is REUSED as info->{field_28,field_2c,field_30} (+0x7c/0x80/0x84);
     *     the loop's accum1 lerps this reused triple, NOT the AV#2 input.
     *   info->soundPathFlag = arg3   (0x30024645)
     *   chunk (EDI) = info->field_40 */
    rowEmit[0] = info->prevEmitOrigin[0]; /* +0x10 */
    rowEmit[1] = info->prevEmitOrigin[1]; /* +0x14 */
    rowEmit[2] = info->prevEmitOrigin[2]; /* +0x18 */
    angInfo2[0] = info->prevSpawnVel[0]; /* +0x28 */
    angInfo2[1] = info->prevSpawnVel[1]; /* +0x2c */
    angInfo2[2] = info->prevSpawnVel[2]; /* +0x30 */
    info->soundPathFlag = arg3; /* +0x5c */
    chunk = info->ownerChunk;

    /* dblTemp68 = per-chunk time span (ms) (0x30024648..0x30024783):
     *   base = (isLocalFirstPerson ? 0.75 : 0.35) * 0.017777... * 1000.0
     * then scaled by a distance/spread factor. */
    {
        const double baseD = isLocalFirstPerson ? 0.75 : 0.35; /* @0x3007c118 : @0x3007c110 */
        dblTemp68 = (double)((long double)baseD * (long double)0.017777777777777778 * (long double)1000.0); /* @0x3007c108, @0x3007bd08 */
        if (isLocalFirstPerson == 0) {
            /* view-distance falloff: dist between emitOrigin and cg_refdef.vieworg.
             * (0x30024670..0x300246c9)  factor:
             *   dist < 2048.0        -> * 1.0
             *   2048 <= dist <= 6000 -> * dist/2048
             *   dist > 6000          -> * 6000/2048   */
            const float dist = VectorDistance(emitOrigin, cg_refdef.vieworg);
            if (dist < 2048.0f) { /* 0x3007c100 = 2048.0f */
                dblTemp68 = 2048.0f * 0.00048828125f * dblTemp68; /* @0x3007c0fc, == *1.0 */
            } else {
                /* Ordered dist>6000 selects the cap; unordered retains the
                 * VectorDistance result at 0x300246bb. */
                const float clampedDist = (dist > 6000.0f) ? 6000.0f : dist; /* @0x3007c0f8 */
                dblTemp68 = (double)((long double)clampedDist * (long double)0.00048828125f * (long double)dblTemp68);
            }
        } else {
            /* first-person: spread falloff from radiusClamp.
             * factor = clamp(0.0025 * max(radiusClamp - 30.0, 0.0), 0.0, 0.8);
             * dblTemp68 *= (1.0 - factor). (0x300246ce..0x30024783) */
            const float x = radiusClamp - 30.0f; /* 0x3007becc = 30.0f */
            const float m = (x > 0.0f) ? x : 0.0f;
            /* factor never leaves the x87 stack: the 0.0/0.8 clamp compares
             * (0x300246fd/0x30024739) and the (1.0f - factor) at 0x30024779 all
             * consume the unrounded 0.0025f*m product. */
            long double factor = (long double)0.0025f * (long double)m; /* 0x3007bf54 = 0.0025f */
            if (!(factor >= 0.0f)) {
                factor = 0.0f;
            } else if (factor > 0.8f) { /* 0x3007bdf0 = 0.8f */
                factor = 0.8f;
            }
            dblTemp68 = (1.0f - factor) * dblTemp68; /* 1.0f @0x3007bce0 */
        }

        /* chunk-count cap: prevChunkEnd = old info->field_40 chunk's field_48.
         * nSpans = (cg_flameTime - prevChunkEnd) / dblTemp68; if nSpans > 180.0,
         * widen the span so exactly 180 chunks fit. (0x30024785..0x300247af) */
        const double prevChunkEnd = chunk->spawnTime;
        /* delta and nSpans ride the x87 stack (0x3002478e FSUB / 0x30024794
         * FDIVR feed the FCOMP at 0x30024796 directly; nothing is spilled). */
        const long double delta = (long double)coduo_int32_from_bits(cg_flameTime) - (long double)prevChunkEnd;
        const long double nSpans = delta / (long double)dblTemp68;
        if (nSpans > 180.0) { /* 0x3007bd48 = 180.0 */
            dblTemp68 = delta * 0.005555555555555556; /* @0x3007c0f0 == 1/180 */
        }

        /* chunkEndTime = floor(dblTemp68 + prevChunkEnd), Q_rint -> radiusSeed.
         * (0x300247b1..0x300247da) */
        const double firstEnd = dblTemp68 + prevChunkEnd;
        chunkEndTime = firstEnd; /* [ESP+0x48] */
        radiusSeed = coduo_fp_to_i32_extended(floor(firstEnd)); /* raw st0 into _ftol2 */

        /* timeStep = dblTemp68 / (cg_flameTime - prevChunkEnd); emitTimeDbl =
         * 1.0 - timeStep (the initial cursor). (0x300247d4..0x300247fb)
         * The FST at 0x300247e5 keeps st0: emitTimeDbl subtracts the UNROUNDED
         * quotient, not the double stored to [0x130]. */
        const long double timeStepRaw =
            (long double)dblTemp68 / ((long double)coduo_int32_from_bits(cg_flameTime) - (long double)prevChunkEnd);
        timeStep = (double)timeStepRaw; /* FST double [0x130] */
        emitTimeDbl = 1.0 - timeStepRaw; /* 1.0 @0x3007bcf8 */

        /* if chunkEndTime > cg_flameTime, the run is already past the end -> skip
         * the loop straight to the info writeback. (0x300247fd FCOMP; 0x3002480a JP) */
        /* JP at 0x3002480a also takes the unordered x87 result. */
        if (!(chunkEndTime <= (double)coduo_int32_from_bits(cg_flameTime))) {
            goto region_c_infoWriteback;
        }
    }

    /* ==================================================================== */
    /* PHASE 2: MAIN EMIT LOOP (0x30024817..0x300251f4) + NESTED RETRY SUB-  */
    /* LOOP (0x30025203..0x30025282). Reconstructed instruction-by-instruction */
    /* from the objdump; every x87 stack order, FILD/FISTP, .rdata constant   */
    /* address and per-chunk field write below is proven against the bytes.   */
    /* ==================================================================== */

    /* THE LOOP-CARRIED EMIT CURSOR RIDES THE X87 STACK: iteration 1 loads the
     * rounded double from [0xf8] (0x30024810 FLD), but the back edge
     * (0x300251f4 JZ) re-enters at 0x30024817 with the UNROUNDED subtraction
     * result still in st0 - the FST at 0x300251de only rounds the memory copy.
     * The accum lerps and NB1 multiply by this DOUBLE cursor and by 80-bit
     * (1 - cursor) factors that are never stored; only NB2/NB3 reload the
     * rounded float copy from [0x10]. `cursor` models that st0 value. */
    long double cursor = emitTimeDbl;
    for (;;) {
        /* ---- LOOP HEAD 0x30024817 --------------------------------------- */
        const float cursorF = (float)cursor; /* FST float [0x10] (keeps st0) */
        const long double t0 = 1.0f - cursor; /* 0x30024825; never stored */
        prevChunk = chunk; /* 0x3002481b MOV ESI,EDI */

        /* accum0[3] @ [0x2c/0x30/0x34]: lerp of emitBasis(+0x88) by t0 and
         * rowEmit(+0xec) by cursor. (0x30024827..0x30024877) */
        vec3_t accum0;
        accum0[1] = emitBasis[1] * t0; /* [0x30] = t0*b8c ... */
        accum0[2] = emitBasis[2] * t0; /* [0x34] = t0*b90 ... */
        accum0[0] = cursor * rowEmit[0] + emitBasis[0] * t0; /* [0x2c] */
        accum0[1] = rowEmit[1] * cursor + accum0[1]; /* [0x30] += f0*cursor */
        accum0[2] = cursor * rowEmit[2] + accum0[2]; /* [0x34] += cursor*f4 */

        /* accum1[3] @ [0xa4/0xa8/0xac]: lerp of vDir(+0x20) by t1 and
         * angInfo2(+0x7c) by cursor, plus basisFwd2? no: uses [0x7c/0x80/0x84]
         * (angInfo2) and vDir. (0x3002487b..0x300248dd) */
        const long double t1 = 1.0f - cursor; /* 0x30024883; never stored */
        vec3_t accum1;
        accum1[1] = vDir[1] * t1;
        accum1[2] = vDir[2] * t1;
        accum1[0] = cursor * angInfo2[0] + vDir[0] * t1; /* [0xa4] */
        accum1[1] = angInfo2[1] * cursor + accum1[1]; /* [0xa8] */
        accum1[2] = cursor * angInfo2[2] + accum1[2]; /* [0xac] */

        /* accum2[3] @ [0xb0/0xb4/0xb8]: lerp of dirCopy(+0x54) by t2 and
         * infoDir(+0xe0) by cursor. (0x300248e4..0x30024947) */
        const long double t2 = 1.0f - cursor; /* 0x300248ec; never stored */
        vec3_t accum2;
        accum2[1] = dirCopy[1] * t2;
        accum2[2] = dirCopy[2] * t2;
        accum2[0] = cursor * infoDir2[0] + dirCopy[0] * t2; /* [0xb0] */
        accum2[1] = infoDir2[1] * cursor + accum2[1]; /* [0xb4] */
        accum2[2] = infoDir2[2] * cursor + accum2[2]; /* [0xb8] */

        /* ---- spawn the chunk (0x3002494e CALL 0x30025600, parent=ESI) ---- */
        chunk = CG_SpawnFlameChunk(prevChunk); /* EDI = EAX */
        if (chunk == NULL) {
            goto region_c_outOfChunks; /* 0x30024957 JE 0x30025201 */
        }

        chunk->emitArgFlag = (uint32_t)arg4; /* 0x3002496c [EDI+0x28]=[0x17c] */
        chunk->kind = 0; /* 0x30024976 */
        chunk->emitCounter = info->emitCounter; /* 0x30024990 info->+0x90 */

        /* NB1 @ [0x70/74/78]: lerp( basisFullF(AV#1 fwd,+0x94) by t0, basisFwd2
         * (AV#2 fwd,+0x100) by cursor ), then VectorNormalize. cursor still ST0
         * so the same t0=(1-cursor) is reused. (0x3002495d..0x300249e5) */
        vec3_t nbFwd;
        {
            /* NB1 still consumes the st-resident DOUBLE cursor (it survives the
             * CG_SpawnFlameChunk call); tf is never stored. */
            const long double tf = 1.0f - cursor; /* 0x30024966 */
            nbFwd[1] = basisFullF[1] * tf; /* [0x74] */
            nbFwd[2] = basisFullF[2] * tf; /* [0x78] */
            nbFwd[0] = basisFwd2[0] * cursor + basisFullF[0] * tf; /* [0x70] */
            nbFwd[1] = basisFwd2[1] * cursor + nbFwd[1]; /* [0x74] */
            nbFwd[2] = basisFwd2[2] * cursor + nbFwd[2]; /* [0x78] */
            VectorNormalize(nbFwd); /* 0x300249e0 */
        }

        /* NB2 @ [0xc8/cc/d0]: lerp( angRight1(AV#1 right,+0x124) by tf,
         * angRight2(AV#2 right,+0x10c) by cursor ), normalize. cursorF reloaded
         * from [0x10]. (0x300249e7..0x30024a6e) */
        vec3_t nbRight;
        {
            /* cursorF reloaded from [0x10] (0x300249ee): NB2 uses the ROUNDED
             * float cursor, unlike accum0..2/NB1. tf is never stored. */
            const long double tf = 1.0f - cursorF; /* 0x300249f8 */
            nbRight[1] = angRight1[1] * tf; /* [0xcc] */
            nbRight[2] = angRight1[2] * tf; /* [0xd0] */
            nbRight[0] = angRight2[0] * cursorF + angRight1[0] * tf; /* [0xc8] */
            nbRight[1] = angRight2[1] * cursorF + nbRight[1]; /* [0xcc] */
            nbRight[2] = angRight2[2] * cursorF + nbRight[2]; /* [0xd0] */
            VectorNormalize(nbRight); /* 0x30024a69 */
        }

        /* NB3 @ [0xbc/c0/c4]: lerp( basisR(AV#1 up,+0xd4) by tf, basisUp2
         * (AV#2 up,+0x118) by cursor ), normalize. (0x30024a70..0x30024af7) */
        vec3_t nbUp;
        {
            /* NB3 reloads cursorF from [0x10] (0x30024a70); tf never stored. */
            const long double tf = 1.0f - cursorF; /* 0x30024a7a */
            nbUp[1] = basisR[1] * tf; /* [0xc0] */
            nbUp[2] = basisR[2] * tf; /* [0xc4] */
            nbUp[0] = basisUp2[0] * cursorF + basisR[0] * tf; /* [0xbc] */
            nbUp[1] = basisUp2[1] * cursorF + nbUp[1]; /* [0xc0] */
            nbUp[2] = basisUp2[2] * cursorF + nbUp[2]; /* [0xc4] */
            VectorNormalize(nbUp); /* 0x30024af2 */
        }

        /* ---- chunk->smokeDensityRate (smoke-density rate) via two wave-noise samples
         * and a no-op pow (0x30024af9..0x30024ba0). ---------------------- */
        const float endTimeF = (float)chunkEndTime; /* [0x14] = (float)[0x48] */
        const float a075 = alpha * 0.75f; /* [0x10] = alpha*0.75f (0x3007be38) */
        /* FMUL by 1/32768 (0x3007bec0 = 3.0517578125e-05f), i.e. rand()/32768 ->
         * a [0,1) uniform. The machine multiplies by the reciprocal, not FDIV.
         * The base rides st0 raw into _CIpow (FILD rand; FMUL 1/32768; no store);
         * the raw result takes FMUL 0.5f and one FSTP float to [0x60] -- powl
         * keeps base+result 80-bit, (float) narrows once, rand() feeds FILD
         * directly (no (float) round). */
        const float half = (float)(powl((long double)coduo_crt_rand() * (long double)3.0517578125e-05f, 1.0L) *
                                   (long double)0.5f); /* [0x60] temp (0x3007bce8) */
        /* CG_FlameWaveNoise stack args (top->): PUSH 0x409729fe = 4.723876f,
         * PUSH 0x40d0b7d4 = 6.52244f (6.522439f would be 2 ULP low at
         * 0x40d0b7d2), endTimeF. */
        const long double n1Raw = (long double)Q_SwayRand(4.723876f, 6.52244f, endTimeF) * (long double)0.5f;
        const float n1b = (float)((n1Raw + (long double)1.0f) * (long double)0.25f + (long double)half); /* [0x14] (0x3007be58=0.25) */
        /* n2 never leaves the x87 stack (0x30024b7b..0x30024b8a chain straight
         * into the FADD [0x14]), and the full product rides st0 through the
         * FST at 0x30024ba0: the 110.0f clamp compare (0x30024ba3 FCOMP) tests
         * the UNROUNDED value, not the stored field. */
        const long double n2 = ((long double)Q_SwayRand(10.453760147094727f, 11.476519584655762f, endTimeF) + (long double)1.0f) *
                               (long double)0.125f; /* 0x3007c0e8 */
        const long double density =
            (n2 + (long double)n1b) * ((long double)a075 + (long double)0.34999999f) * (long double)140.0f; /* 0x3007c0e4, 0x3007bec8 */
        chunk->smokeDensityRate = (float)density; /* FST [EDI+0x60] */
        /* clamp field_60 to <= 110.0f: 0x30024ba3 FCOMP 110.0; TEST AH,0x41; JNE
         * keep -> overwrite with 110.0f (0x42dc0000) only when field_60 > 110. */
        if (density > 110.0f) {
            chunk->smokeDensityRate = 110.0f;
        }

        /* chunk->startSpeedBits = (alpha*0.75 + 0.25) * (140.0 - field_60)  (float bits
         * into the uint32_t slot; the emitter writes a float here even though
         * other consumers read field_5c as an int gate). (0x30024bb7..0x30024bd2)
         * (0x3007be58=0.25,0x3007bec8=140) */
        chunk->startSpeedBits = (uint32_t)CG_FloatBits(
            (float)(((long double)a075 + (long double)0.25f) * ((long double)140.0f - (long double)chunk->smokeDensityRate)));

        /* chunk->startSpeed / field_e4 seed. factor = isLocalFirstPerson ? 0.9 : 0.6
         * (0x30024bd5 JE picks 0.6f@0x3007be0c else 0.9f@0x3007c038). Then
         *   x = 1.0f - factor*field_60*0.0071428571f;
         *   field_e4 = field_58 = x * 16.0f;
         * plus the unconditional stores field_a8=60.0f, field_a4=0, field_14=1.
         * (0x30024bd7..0x30024c20) (0x3007c0dc=1/140,0x3007bf00=16.0) */
        {
            /* factor: the flame-density falloff weight. First-person view uses 0.9
             * (0x3007c038 = 0.89999998f), the world/third-person view uses 0.6
             * (0x3007be0c = 0.60000002f) — a stronger density falloff for your own
             * flame so it doesn't obscure the first-person view. */
            const float factor = isLocalFirstPerson ? 0.9f : 0.6f;
            chunk->expansionRate = 60.0f; /* 0x42700000 (float bits) */
            chunk->birthTime = 0; /* MOV [edi+0xa4],0 (int) */
            chunk->ownerSentinel = 1; /* 0x30024c04 */
            /* x: density scale = 1 - factor * smokeRate / 140. 0.0071428571f
             * (0x3007c0dc) == 1/140 (the smoke-rate normaliser); field_60 is the
             * chunk's smoke-density rate. */
            const long double scaledDensity =
                ((long double)1.0f - (long double)factor * (long double)chunk->smokeDensityRate * (long double)0.0071428571f) *
                (long double)16.0f;
            chunk->radius = (float)scaledDensity; /* FST (keeps) */
            chunk->startSpeed = (float)scaledDensity; /* FSTP */
            info->emitRandSeed = radiusSeed; /* 0x30024c20 info->emitRandSeed=EBX */
        }

        /* chunk->emitScatterIndex (a rand()-scattered emit index) — ONLY when
         * isLocalFirstPerson AND the info->field_4c gate passes.
         * (0x30024c26 JE skips when isLocalFirstPerson==0) */
        if (isLocalFirstPerson != 0) {
            int32_t doBlock;
            if (info->emitSeedGate > radiusSeed) { /* +0x4c; 0x30024c2b JG */
                doBlock = 1;
            } else if (info->emitSeedGate >= coduo_int32_from_bits((uint32_t)radiusSeed - 200u)) { /* 0x30024c35 JGE skip (0xc8=200) */
                doBlock = 0;
            } else {
                doBlock = 1;
            }
            if (doBlock) {
                /* esi = chunkCount / 10 (0x66666667 magic, SAR 2, sign-fix). */
                const int32_t tenth = chunkCount / 10; /* 0x30024c39.. */
                const int32_t r = coduo_crt_rand(); /* 0x30024c4c */
                const int32_t quarter = chunkCount / 4; /* 0x30024c53.. */
                const int32_t denom =
                    coduo_int32_from_bits(((uint32_t)chunkCount - (uint32_t)quarter) - (uint32_t)tenth); /* 0x30024c64/66 */
                const int32_t scatterRemainder = r % denom;
                chunk->emitScatterIndex = (uint32_t)coduo_int32_from_bits(((uint32_t)scatterRemainder + (uint32_t)radiusSeed) +
                                                                          (uint32_t)tenth); /* 0x30024c6f/71/73 */
                info->emitSeedGate = radiusSeed; /* +0x4c; 0x30024c76 */
            }
        }

        /* chunk->spawnTime = chunkEndTime (double) (0x30024c79/86)
         * chunk->endTime = chunkEndTime + chunkCount (double) (0x30024c89..0x30024c91) */
        chunk->spawnTime = chunkEndTime;
        chunk->endTime = (double)chunkCount + chunkEndTime;
        /* chunk->sizeRate = alpha * field_5c * 0.0006002400768920779f (0x3007bec4)
         * (float bits; reads field_5c as float via FMUL). (0x30024c94..0x30024ca4) */
        chunk->sizeRate =
            (float)((long double)alpha * (long double)CG_FloatFromBits(chunk->startSpeedBits) * (long double)0.0006002400768920779f);
        /* chunk->driftSpeed: adsGate ? 0 : (alpha*0.9 + 0.1)*900.0
         * (0x30024ca7 JE picks the compute path; 0x3007c038=0.9,0x3007bf6c=0.1,0x3007c0d8=900) */
        if (adsGate != 0) {
            chunk->driftSpeed = 0.0f; /* 0x30024ca9 */
        } else {
            chunk->driftSpeed = (float)(((long double)alpha * (long double)0.89999998f + (long double)0.10000000f) * (long double)900.0f);
        }

        /* chunk drift basis (+0x88..): copy the NB1 normalized fwd vec into
         * field_88/8c/90 AND field_ac/b0/b4, field_b8=alpha, normalize, then
         * scale by field_94 and add accum1. (0x30024cd4..0x30024d62) */
        chunk->driftDir[0] = nbFwd[0];
        chunk->axisDir[0] = nbFwd[0];
        chunk->driftDir[1] = nbFwd[1];
        chunk->axisDir[1] = nbFwd[1];
        chunk->driftDir[2] = nbFwd[2];
        chunk->axisDir[2] = nbFwd[2];
        chunk->soundAmpRate = alpha; /* [EDI+0xb8] = [0x174] */
        VectorNormalize(&chunk->driftDir[0]); /* 0x30024d13 */
        chunk->driftDir[0] = (float)((long double)chunk->driftDir[0] * chunk->driftSpeed + accum1[0]); /* 0x30024d1a */
        chunk->driftDir[1] = (float)((long double)chunk->driftSpeed * chunk->driftDir[1] + accum1[1]); /* 0x30024d2b */
        chunk->driftDir[2] = (float)((long double)chunk->driftDir[2] * chunk->driftSpeed + accum1[2]); /* 0x30024d44 */
        VectorNormalize(&chunk->driftDir[0]); /* 0x30024d5d */

        /* time/position stamps (0x30024d64..0x30024da8):
         *   field_68 = chunkEndTime (double), field_80 = chunkEndTime (double),
         *   field_d0 = chunkEndTime (double)
         *   field_70/74/78 = accum0 vec, field_c0/c4/c8 = accum0 vec
         *   field_bc = 1.0f  */
        chunk->spawnTimeCopy = chunkEndTime;
        chunk->localPos[0] = accum0[0];
        chunk->posCopy[0] = accum0[0];
        chunk->localPos[1] = accum0[1];
        chunk->posCopy[1] = accum0[1];
        chunk->driftStartTime = chunkEndTime;
        chunk->endTimeCopy = chunkEndTime;
        chunk->localPos[2] = accum0[2];
        chunk->posCopy[2] = accum0[2];
        chunk->alpha = 1.0f; /* 0x3f800000 */

        /* ---- chunk->lifeRate (life-rate) and field_130 (spawn timestamp) ----
         * Two structurally-identical branches selected by isLocalFirstPerson
         * (0x30024db2 JE). The !=0 branch clamps against 400.0 with the 0.25/20.0
         * envelope and floors field_138 to [0.06,100.0]; the ==0 branch uses
         * 100.0, 0.125 and floors to [0.04,100.0]. Both end at field_130.
         * (0x30024db8..0x30025082) */
        /* lifeStart models the st0 the FST at 0x30024f12/0x3002507c KEEPS: the
         * owner<64 turbulence add below (0x300250dd FADD ST0,ST1) consumes the
         * UNROUNDED sum, not the double stored to field_130. */
        long double lifeStart = 0.0;
        {
            const float clampHi = isLocalFirstPerson ? 400.0f : 100.0f; /* bf58 : 715ec */
            const double scaleD = isLocalFirstPerson ? 0.25 : 0.125; /* be30 : c0c0 */
            const double f138Lo = isLocalFirstPerson ? 0.06 : 0.04; /* c0d0 : c0b8 */
            const int32_t info58 = info->emitAccumTime; /* +0x58 */
            const int32_t clamped58 = (info58 < 1000) ? info58 : 1000; /* min(emitAccumTime,1000) */
            const float v = (radiusClamp < clampHi) ? radiusClamp : clampHi; /* min(radiusClamp,clampHi) */

            /* branch predicate (0x30024e09 FCOMPP A vs B; 0x30024e0f TEST AH,0x41;
             * JNE P2): A=(1-clamped58*0.001)*0.25+0.75, B=scaleD*v. AH&0x41 is
             * C0|C3, so JNE (-> P2) fires when A<=B; fall-through (A>B) -> P1.
             * A, B, R and the 0.35/R quotient ALL ride the x87 stack: nothing
             * is rounded before the FSTP to field_138 (the FST at 0x30024eaf /
             * 0x30025019 keeps st0, so both clamp compares test the unrounded
             * quotient) - keep the whole chain in long double. */
            /* 0.0010000000474974513f (0x3007bd94) == 0.001f exactly (float rep). */
            const long double rTerm =
                ((long double)1.0f - (long double)clamped58 * (long double)0.001f) * (long double)0.25f + (long double)0.75f;
            long double R;
            if (rTerm > (long double)scaleD * (long double)v) {
                /* path P1 (A>B): R = (1 - clamped58*0.001)*0.25 + 0.75 (recomputed) */
                R = ((long double)1.0f - (long double)clamped58 * (long double)0.001f) * (long double)0.25f + (long double)0.75f;
            } else {
                /* path P2 (A<=B): v2 = min(radiusClamp,clampHi); R = min(scaleD*v2,20.0).
                 * (0x30024e6d TEST AH,0x41; JNE -> R=scaleD*v3; else R=20.0, so
                 * R=20.0 only when scaleD*v2 > 20.0.) */
                const float v2 = (radiusClamp < clampHi) ? radiusClamp : clampHi;
                if ((long double)scaleD * (long double)v2 > (long double)20.0) {
                    R = 20.0; /* c128 */
                } else {
                    const float v3 = (radiusClamp < clampHi) ? radiusClamp : clampHi;
                    R = (long double)scaleD * (long double)v3;
                }
            }
            /* field_138 = clamp(0.35 / R, f138Lo, 100.0) (bcf8=1.0,c110=0.35,c0c8=100) */
            long double f138 = (1.0 / R) * 0.35;
            if (f138 < f138Lo) {
                f138 = f138Lo;
            } else if (f138 > 100.0) {
                f138 = 100.0;
            }
            chunk->lifeRate = (double)f138;

            /* field_130 = (rand()/32768)*span*field_138 + field_48, span=field_50-field_48
             * (0x30024ee1..0x30024f12 / 0x3002504b..0x3002507c) */
            const double span = chunk->endTime - chunk->spawnTime;
            chunkSpanScratch = span; /* [0x60] reused just below */
            /* FILD->double, then FMUL float 1/32768, all in x87 double precision. */
            lifeStart = (long double)coduo_crt_rand() * (long double)3.0517578125e-05f * (long double)span * (long double)chunk->lifeRate +
                        (long double)chunk->spawnTime;
            chunk->lifeStartTime = (double)lifeStart; /* FST double [EDI+0x130] */
        }

        /* owner = cent[0]; if (owner < 64) add a vDir-length turbulence term to
         * field_130. (0x30025082 CMP owner,0x40; JGE skips) */
        {
            const int32_t owner2 = cent->currentState.number;
            if (owner2 < 64) {
                /* w = clamp( |vDir| * 0.0005, 0.0, 0.07 ) (c058=0.0005,c0b4=0.07)
                 * The FSQRT chain and the product stay on the x87 stack through
                 * both clamp compares (0x300250a5..0x300250d3); nothing is
                 * spilled, so len and w stay 80-bit. */
                const long double len =
                    coduo_x87_sqrtl(((long double)vDir[0] * vDir[0] + (long double)vDir[1] * vDir[1]) + (long double)vDir[2] * vDir[2]);
                long double w = len * 0.0005000000237487257f;
                if (w < 0.0f) {
                    w = 0.0f;
                } else if (w > 0.070000000298f) {
                    w = 0.070000000298f;
                }
                chunk->lifeStartTime = (double)(w * chunkSpanScratch + lifeStart);
            }
        }

        /* info->lastFlameTime = (double)cg_flameTime (0x300250ee..0x300250f4) */
        info->lastFlameTime = (double)coduo_int32_from_bits(cg_flameTime); /* +0xb0 */

        /* chunk->ownerInfoIndex = field_38 = owner; field_3c = cent->currentState.weapon (+0xcc)
         * (0x300250fa..0x30025108) */
        {
            const int32_t owner2 = cent->currentState.number;
            chunk->ownerInfoIndex = owner2;
            chunk->ownerClientNum = owner2;
            chunk->centFlags = cent->currentState.weapon; /* +0xcc */
        }

        /* chunk->radiusBaseA = (rand()/32768)*360.0 (0x3002510b..0x30025124) */
        chunk->radiusBaseA = (float)((long double)coduo_crt_rand() * (long double)3.0517578125e-05f * (long double)360.0f);

        /* second rand() (0x3002512a): its result -> chunk->radiusBaseB below. The
         * accum2 vec (+0xb0/b4/b8 frame slots) is copied to field_100/104/108,
         * field_a0=0. (0x3002512a..0x30025177) */
        {
            const int32_t r9c = coduo_crt_rand(); /* 0x3002512a; -> [0x14] */
            chunk->deadFlag = 0; /* 0x3002514c */
            chunk->emitBasis[0] = accum2[0]; /* [0xb0] -> field_100 */
            chunk->emitBasis[1] = accum2[1]; /* [0xb4] -> field_104 */
            chunk->emitBasis[2] = accum2[2]; /* [0xb8] -> field_108 */
            chunk->radiusBaseB = (float)((long double)r9c * (long double)3.0517578125e-05f * (long double)360.0f); /* 0x30025177 */
        }

        /* chunk->endTimeCopy2 = field_f8 = chunkEndTime; field_10c/110/114 = emitOrigin.
         * Then advance: nextEnd = floor(dblTemp + chunkEndTime), stored to [0x48];
         * field_130-based cursor step; new radiusSeed = Q_rint(floor(...)).
         * (0x30025174..0x300251c6) NOTE the SUB ESP,8 bracket for CG_CrtFloor. */
        chunk->endTimeCopy2 = chunkEndTime;
        chunk->endTimeCopy3 = chunkEndTime;
        chunk->emitOrigin[0] = emitOrigin[0];
        chunk->emitOrigin[1] = emitOrigin[1];
        chunk->emitOrigin[2] = emitOrigin[2];
        {
            const double advanced = dblTemp68 + chunkEndTime; /* [0x68] + [0x48] */
            chunkEndTime = advanced; /* stored to [0x48] */
            radiusSeed = coduo_fp_to_i32_extended(floor(advanced)); /* EBX = round(floor); raw st0 into _ftol2 */
        }

        /* advance cursor and test loop condition (0x300251cb..0x300251f4):
         *   cursor -= timeStep (stays in st0 across the back edge; the FST at
         *   0x300251de only rounds the [0xf8] memory copy);
         *   info->field_40 = chunk;
         *   continue while cg_flameTime >= chunkEndTime.  */
        cursor = cursor - timeStep;
        emitTimeDbl = (double)cursor; /* FST double [0xf8] */
        info->ownerChunk = chunk; /* 0x300251db */
        if (!((double)coduo_int32_from_bits(cg_flameTime) >= chunkEndTime)) { /* 0x300251eb FCOMP; TEST AH,1 */
            break; /* fall to 0x300251fa normal exit */
        }
    }

    /* Normal loop exit (0x300251fa FSTP; 0x300251fc JMP 0x300254dd). */
    goto region_c_infoWriteback;

    /* ==================================================================== */
    /* REGION C — TEARDOWN                                                   */
    /* ==================================================================== */

    /* -------------------------------------------------------------------- */
    /* TERMINAL-CHUNK STAMP + spawn-fail path (0x3002521b..0x300254da,        */
    /* 0x30025201..0x3002521a). This block is entered from Region A's         */
    /* single-terminal-chunk gate (0x3002453a/45/55 JE/JNE 0x3002521b) — the  */
    /* "emit exactly one terminal chunk (field_2c=1)" case that bypasses the  */
    /* main loop. It plays the flame start-sound, force-spawns one            */
    /* chunk (parent=NULL), and on spawn failure prints and returns; on       */
    /* success it stamps the terminal chunk and falls through to the info     */
    /* writeback. `regionATemp10` is the Region-A [ESP+0x10] value this block */
    /* consumes (== (alpha+1.0f)*0.5f). Reached only via the Region-A gate    */
    /* `goto region_c_terminalChunk` (region_a_reached_terminalChunk == 1).   */
region_c_terminalChunk: {
    flameChunk_t *term;
    int32_t owner2 = cent->currentState.number; /* 0x30025229 */

    info->emitAccumTime = 0; /* +0x58; 0x30025222 */
    /* CG_PlaySoundAliasByName(&emitOrigin, cg_flameStartSound, owner)
         * (0x30025231 LEA ECX,[ESP+0x3c]; EAX=[0x3044c24c]; PUSH owner). The
         * channel object is a local scratch at [ESP+0x3c]; soundName is the
         * .data pointer at 0x3044c24c (a config-string handle; role unresolved). */
    CG_PlaySoundAliasByName(owner2, &emitOrigin, cg_flameStartSound);

    /* info->emitCounter++ ; info->soundPathFlag = arg3 (0x3002523a..0x3002525f) */
    info->emitCounter = coduo_int32_from_bits((uint32_t)info->emitCounter + 1u); /* +0x90; 0x30025251 INC; store */
    info->soundPathFlag = arg3; /* +0x5c; [ESP+0x178] = arg3 */
    /* [0x2c/0x30/0x34] = emitBasis {[0x88],[0x8c],[0x90]} (0x30025258..0x30025273) */
    vec3_t termBasis;
    termBasis[0] = emitBasis[0];
    termBasis[1] = emitBasis[1];
    termBasis[2] = emitBasis[2];

    term = CG_SpawnFlameChunk(NULL); /* 0x30025277 parent=ESI=0 */
    if (term == NULL) {
        goto region_c_outOfChunks; /* 0x30025282 JE 0x30025203 */
    }

    const int32_t flameTimeSigned = coduo_int32_from_bits(cg_flameTime);
    term->emitArgFlag = (uint32_t)arg4; /* 0x30025295 */
    term->kind = 1; /* 0x3002529e */
    term->emitCounter = info->emitCounter; /* +0x90; 0x300252ae */
    term->spawnTime = (double)flameTimeSigned; /* 0x300252ab */
    term->endTime = (double)coduo_int32_from_bits((uint32_t)flameTimeSigned + (uint32_t)chunkCount); /* 0x300252b4..0x300252c2 */

    /* field_e4 = field_58 = (old_field_60 * (0.9f/140) + 0.9f) * 16.0f
         * (0x300252c5..0x300252e3): 0x300252cb FMUL float [0x3007c0b0] =
         * 0x3BD2A6C4 = 0.006428571417927742f = (float)(0.9/140) — the same
         * 1/140-family density normalizer as 0x3007c0dc. (c038=0.9f, bf00=16.0f).
         * field_60:=0 first. */
    {
        float oldf60 = term->smokeDensityRate;
        term->smokeDensityRate = 0.0f; /* [EDI+0x60] = eax(=0) */
        float e = (float)(((long double)oldf60 * (long double)0.006428571417927742f + (long double)0.89999998f) * (long double)16.0f);
        term->radius = e;
        term->startSpeed = e;
    }

    /* field_5c = regionATemp10 * 140.0 ; field_64 = field_5c * alpha * 0.0006002
         * (float bits into the uint32_t slots). (0x300252e6..0x30025300)
         * (bec8=140,bec4=0.0006002) */
    /* The FST at 0x300252f0 keeps st0: sizeRate multiplies the UNROUNDED
         * regionATemp10*140.0f product, not the stored field_5c float. */
    {
        const long double speedRaw = (long double)regionATemp10 * (long double)140.0f;
        term->startSpeedBits = (uint32_t)CG_FloatBits((float)speedRaw);
        term->sizeRate = (float)(speedRaw * alpha * 0.0006002400768920779f);
    }
    term->spawnTimeCopy = (double)flameTimeSigned; /* 0x30025303 FST QWORD */

    /* field_70/74/78 = termBasis ; field_80 = (double)cg_flameTime
         * (0x30025306..0x30025325) */
    term->localPos[0] = termBasis[0];
    term->localPos[1] = termBasis[1];
    term->driftStartTime = (double)flameTimeSigned;
    term->localPos[2] = termBasis[2];

    /* field_94: adsGate ? 0 : (alpha*0.7 + 0.3)*900.0
         * (0x3002531e CMP [0xa0],eax(=0); JE picks compute) (bf24=0.7,bea0=0.3,c0d8=900) */
    if (adsGate != 0) {
        term->driftSpeed = 0.0f; /* 0x3002532a store eax(=0) */
    } else {
        term->driftSpeed = (float)(((long double)alpha * (long double)0.699999988f + (long double)0.30000001f) * (long double)900.0f);
    }

    /* drift basis: field_88/8c/90 = field_ac/b0/b4 = basisFullF; field_b8=alpha;
         * normalize; scale by field_94 and add vDir; normalize.
         * (0x30025351..0x300253df) */
    term->driftDir[0] = basisFullF[0];
    term->axisDir[0] = basisFullF[0];
    term->driftDir[1] = basisFullF[1];
    term->axisDir[1] = basisFullF[1];
    term->driftDir[2] = basisFullF[2];
    term->axisDir[2] = basisFullF[2];
    term->soundAmpRate = alpha;
    VectorNormalize(&term->driftDir[0]); /* 0x30025399 */
    term->driftDir[0] = (float)((long double)term->driftDir[0] * term->driftSpeed + vDir[0]); /* 0x300253a0 (+[0x20]) */
    term->driftDir[1] = (float)((long double)term->driftSpeed * term->driftDir[1] + vDir[1]); /* 0x300253ae (+[0x24]) */
    term->driftDir[2] = (float)((long double)term->driftDir[2] * term->driftSpeed + vDir[2]); /* 0x300253c4 (+[0x28]) */
    VectorNormalize(&term->driftDir[0]); /* 0x300253da */

    /* field_d0 = field_80 (double); field_c0/c4/c8 = field_70/74/78
         * (0x300253df..0x30025402) */
    term->endTimeCopy = term->driftStartTime;
    term->posCopy[0] = term->localPos[0];
    term->posCopy[1] = term->localPos[1];
    term->posCopy[2] = term->localPos[2];

    /* field_34 = field_38 = cent[0]; field_3c = cent[+0xcc]
         * (0x30025408..0x30025416) */
    term->ownerInfoIndex = owner2;
    term->ownerClientNum = owner2;
    term->centFlags = cent->currentState.weapon; /* +0xcc */

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    {
        int32_t rr = coduo_fp_to_i32_extended((long double)0.0 / (long double)timescale_vmCvar.value) % 360;
        term->radiusBaseA = (float)rr;
    }

    /* field_9c = (rand()/32768)*360.0 ; field_a0=0 ; field_bc=1.0 ;
         * field_b8=alpha ; field_100/104/108 = dirCopy (0x30025440..0x30025498) */
    {
        int32_t r9 = coduo_crt_rand(); /* 0x30025440 */
        term->soundAmpRate = alpha; /* 0x3002545c (overwrites earlier) */
        term->deadFlag = 0; /* 0x3002546c */
        term->alpha = 1.0f; /* 0x30025476 */
        term->emitBasis[0] = dirCopy[0]; /* [0x54] */
        term->emitBasis[1] = dirCopy[1]; /* [0x58] */
        term->emitBasis[2] = dirCopy[2]; /* [0x5c] */
        term->radiusBaseB = (float)((long double)r9 * (long double)3.0517578125e-05f * (long double)360.0f); /* 0x30025492 */
    }

    /* field_f0 = field_f8 = (double)cg_flameTime ; field_130 = -1.0 ;
         * field_10c/110/114 = emitBasis {[0x2c],[0x30],[0x34]}
         * (0x3002549e..0x300254d4) (c0a8 = -1.0 double) */
    term->endTimeCopy2 = (double)flameTimeSigned;
    term->endTimeCopy3 = (double)flameTimeSigned;
    term->lifeStartTime = -1.0;
    term->emitOrigin[0] = termBasis[0]; /* [0x2c] */
    term->emitOrigin[1] = termBasis[1]; /* [0x30] */
    term->emitOrigin[2] = termBasis[2]; /* [0x34] */

    info->ownerChunk = term; /* 0x300254da MOV [EBP+0x40],EDI */
    goto region_c_infoWriteback; /* fall through to 0x300254dd */
}

region_c_outOfChunks:
    /* "Out of flame chunks" spawn-fail path (0x30025201..0x3002521a).
     * Reached from the main-loop alloc-fail exit (0x30024957 JE 0x30025201) and
     * from the terminal-chunk spawn failure (0x30025282 JE 0x30025203): print
     * once, then early epilogue + RET (no info writeback). */
    Com_PrintMessage("Out of flame chunks\n"); /* 0x30025203 PUSH 0x300777b8; CALL 0x3002b2b0 */
    return; /* 0x30025210..0x3002521a: POP regs; ADD ESP,0x158; RET */

region_c_infoWriteback:
    /* info-writeback (0x300254dd..0x30025553), then common epilogue (0x30025556).
     * Copies the freshly-computed emitter frame (dir + origin + spawn velocity) and
     * the current time/count back into the per-owner cgFlameInfo record so the next
     * frame continues the flame run. Each store is a bit-for-bit dword copy from a
     * Region-A frame slot (proven at 0x300254e4..0x30025553):
     *   info->activeFlag        = arg3          ([ESP+0x178])
     *   info->prevDir         = dirCopy       ([ESP+0x54/58/5c])
     *   info->prevEmitOrigin  = emitOrigin    ([ESP+0x38/3c/40])
     *   info->prevSpawnVelA   = vDir          ([ESP+0x20/24/28])
     *   info->prevSpawnVel    = vDir          (same triple copied again)
     *   info->emitDir         = dirCopy       ([ESP+0x54/58/5c] reused)
     *   info->clientFrame     = cg_clientFrame (0x3002553e/50)
     *   info->lastEmitTime    = cg_flameTime  (0x30025547/53) */
    info->activeFlag = arg3; /* +0x60 */
    info->prevDir[0] = dirCopy[0]; /* +0x04 */
    info->prevDir[1] = dirCopy[1]; /* +0x08 */
    info->prevDir[2] = dirCopy[2]; /* +0x0c */
    info->prevEmitOrigin[0] = emitOrigin[0]; /* +0x10 */
    info->prevEmitOrigin[1] = emitOrigin[1]; /* +0x14 */
    info->prevEmitOrigin[2] = emitOrigin[2]; /* +0x18 */
    info->prevSpawnVelA[0] = vDir[0]; /* +0x1c */
    info->prevSpawnVelA[1] = vDir[1]; /* +0x20 */
    info->prevSpawnVelA[2] = vDir[2]; /* +0x24 */
    info->prevSpawnVel[0] = vDir[0]; /* +0x28 */
    info->prevSpawnVel[1] = vDir[1]; /* +0x2c */
    info->prevSpawnVel[2] = vDir[2]; /* +0x30 */
    info->emitDir[0] = dirCopy[0]; /* +0x34 */
    info->emitDir[1] = dirCopy[1]; /* +0x38 */
    info->emitDir[2] = dirCopy[2]; /* +0x3c */
    info->clientFrame = cg_clientFrame; /* +0x00 */
    info->lastEmitTime = coduo_int32_from_bits(cg_flameTime); /* +0x54 */

    /* common epilogue 0x30025556..0x30025560: POP EDI/ESI/EBP/EBX; ADD ESP,0x158; RET. */
    return;
}
