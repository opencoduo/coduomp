/* coduo_libm — reconstruction of the math routines CoD:UO actually calls.
 *
 * Purpose: remove the dependency on whatever libm the host platform supplies.
 * A dynamically linked libm is not a fixed target — glibc's implementations
 * changed across versions and differ from the MSVC CRT the retail binaries
 * statically linked — so building against the platform's libm cannot reproduce
 * stock behaviour reliably. These routines are reconstructed from the machine
 * code of the statically linked MSVC 7.1 CRT in the retail Windows binaries
 * (see docs/x87-transcendental-reconstruction-scope.md).
 *
 * REFERENCE: the statically linked MSVC 7.1 CRT is the project's reference
 * implementation for all of these, on every target including the Linux server
 * reconstruction. It is the only candidate that is actually pinned: it is
 * compiled into the retail binary, so its machine code is recoverable and
 * cannot drift. The Linux side cannot serve as a reference — libm there is
 * dynamically linked and unbundled, so its behaviour is whatever the deployment
 * host happens to ship (glibc 2.36 on the current reference host), and the
 * period glibc the original servers ran is not available. Where the two
 * implementations differ this is an accepted, documented deviation from a
 * modern Linux stock server; see
 * docs/platform-discrepancies/libm-platform-disagreement.md.
 *
 * Every routine here is written once and compiles two ways:
 *   EMULATE_X87 == 0  native long double, for x87 targets
 *   EMULATE_X87 == 1  SoftFloat extFloat80_t via the x87f_* shim
 * Both must produce bit-identical results.
 *
 * Precision: the ambient x87 control word is 0x137F (64-bit extended,
 * round-to-nearest, exceptions masked), reasserted per frame. Routines that
 * change it say so in their own comment. Neither routine here changes it.
 */
#ifndef CODUO_LIBM_H
#define CODUO_LIBM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MSVC _ftol2: convert to int64 with truncation toward zero.
 *
 * Reconstructed from uo_gamex86.dll 0x2008a1ac (175 call sites — every
 * (int)float-expression cast in the server game module). The CRT does not
 * change the rounding mode to truncate; it converts with the ambient
 * round-to-nearest via FISTP and then corrects by one using the sign of the
 * residual, so the correction is reproduced here rather than replaced with a
 * plain C cast. A C cast would agree on all finite in-range inputs but not on
 * overflow, where the x87 indefinite value is produced instead of UB. */
int64_t CoduoLibm_FloatToInt64(double value);

/* Convenience wrapper for the common 32-bit consumer: takes the low dword of
 * the int64 result, matching callers that use only EAX. */
int32_t CoduoLibm_FloatToInt32(double value);

/* sqrt. Reconstructed from uo_gamex86.dll 0x2008a640 (41 call sites).
 *
 * Runs at the ambient 64-bit precision: the CRT's control-word helper
 * (0x2008aa15) preserves the PC field and only forces round-to-nearest and
 * masked exceptions, so the square root is taken to 64-bit extended precision
 * and narrowed to double on return. That double rounding is stock behaviour
 * and is deliberately preserved.
 *
 * Edge cases follow the machine code exactly:
 *   +inf      -> +inf   (FSQRT is skipped)
 *   -0.0      -> -0.0   (FSQRT is skipped)
 *   x < 0     -> indefinite QNaN (domain error), including -inf
 *   NaN       -> quieted NaN
 */
double CoduoLibm_Sqrt(double value);

/* glibc/i386 sqrt: FSQRT at 53-bit precision, i.e. the correctly-rounded double
 * root from a single rounding. This does NOT equal CoduoLibm_Sqrt (the MSVC CRT
 * computes at 64-bit and double-rounds); the two disagree for about 1 input in
 * 4100 (measured 9749 of 40e6).
 *
 * NOT the project reference.  Recovered Linux-platform bodies call this only
 * where original machine code proves the glibc sqrt boundary; Windows-reference
 * bodies use CoduoLibm_Sqrt or a direct x87 operation.  It also backs the measurements in
 * docs/platform-discrepancies/libm-platform-disagreement.md. */
double CoduoLibm_SqrtGlibc(double value);

/* asin / acos. Reconstructed from uo_gamex86.dll 0x2008a700 and 0x2008a0e0.
 *
 * Both compute sqrt((1+x)*(1-x)) and feed it to FPATAN, differing only in
 * operand order. The (1+x)(1-x) factoring is stock and is not interchangeable
 * with 1-x*x. Run at the ambient 64-bit precision.
 *
 *   |x| > 1  -> indefinite QNaN (domain error)
 *   x == +1  -> asin +pi/2, acos +0.0
 *   x == -1  -> asin -pi/2, acos pi
 *   NaN      -> quieted
 */
double CoduoLibm_Asin(double value);
double CoduoLibm_Acos(double value);

/* sin / cos. Reconstructed from uo_gamex86.dll 0x2008a2e0 and 0x2008a230.
 *
 * The only CRT routines that change precision control: both force CW 0x027F
 * (PC = 53-bit) for the computation and restore on exit. If FSIN/FCOS set C2
 * (|x| >= 2^63) the argument is reduced by FPREM1 against pi*2^62 -- an exact
 * multiple of 2*pi -- and retried.
 *
 *   +-inf -> indefinite QNaN (domain error)
 *   NaN   -> quieted
 */
double CoduoLibm_Sin(double value);
double CoduoLibm_Cos(double value);

/* ---------------------------------------------------------------------------
 * EMULATION STATUS
 *
 * A routine is "emulated" when the EMULATE_X87 build reproduces it exactly
 * rather than deferring to the host. Routines that depend on an x87
 * transcendental cannot be emulated until that instruction is reverse
 * engineered, so on EMULATE_X87 builds they call the platform libm. That is a
 * known, deliberate gap: those builds are not bit-exact for these functions,
 * while native x87 builds (32-bit, and 64-bit with -mfpmath=387) are.
 *
 *   routine                 native x87   EMULATE_X87   blocked on
 *   ----------------------------------------------------------------
 *   CoduoLibm_FloatToInt64  exact        exact         -
 *   CoduoLibm_FloatToInt32  exact        exact         -
 *   CoduoLibm_Sqrt          exact        exact         -
 *   CoduoLibm_SqrtGlibc     exact        exact         -
 *   CoduoLibm_Asin          exact        host libm     FPATAN
 *   CoduoLibm_Acos          exact        host libm     FPATAN
 *   CoduoLibm_Sin           exact        host libm     FSIN
 *   CoduoLibm_Cos           exact        host libm     FCOS
 *
 * See docs/platform-discrepancies/libm-platform-disagreement.md for what
 * "host libm" costs.
 * ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* CODUO_LIBM_H */
