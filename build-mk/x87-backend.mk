# ---------------------------------------------------------------------------
# x87 emulation BACKEND flags.
#
# This is orthogonal to x87-policy.mk. That file is about the *native* x86
# faithful path (does GCC emit x87? -mfpmath=387 / -fexcess-precision=fast).
# This file is about the *emulated* path (EMULATE_X87=1, e.g. arm64), and picks
# what the x87f_* primitives are backed by:
#
#   EMU_X87_BACKEND = softfloat (default) | double
#     softfloat : Berkeley SoftFloat, 80-bit, bit-exact to x87. Links
#                 vendor/softfloat; the consumer adds -I.../source/include and
#                 the softfloat.a to its link line. Slow (~50-100x).
#     double    : native double, 53-bit, a fast approximation. No library, no
#                 extra include. NOT bit-exact — measure drift with
#                 analysis/tools/x87_backend_flip_rate.sh.
#
# FP-environment rules that apply to BOTH emulated backends:
#   * NEVER pass -ffast-math / -funsafe-math-optimizations. They enable
#     flush-to-zero (x87 never flushes denormals) and reassociation (breaks the
#     op-for-op rounding the emulation reproduces). This is the one "compiler
#     flag" rule for correctness — it is a rule about a flag you must NOT set.
#   * There is no compiler flag that *disables* flush-to-zero: FTZ (SSE MXCSR) /
#     FZ (NEON FPCR) is runtime CPU state, not codegen. The process default is
#     already FZ=0 (IEEE, denormals honored), which is what x87 does — so as long
#     as -ffast-math is avoided, nothing needs clearing. The double backend's
#     x87f_init() clears it defensively if you call it once at startup.
#
# double-backend-only:
#   * -ffp-contract=off keeps results reproducible across NEON and x86 (so a
#     multiply-add is not fused to an FMA on one platform but not the other).
#     (For this collision math it rarely changes a value, since float-derived
#     products are already exact in double, but set it so the two agree.)
#   * RECOMMENDED with the double backend: EMU_X87_EXACT_GEOMETRY=1 (an engine
#     Makefile knob). The collision-geometry CONSTRUCTION runs once at map load,
#     so its FP cost is amortized to nothing; this forces just those TUs to the
#     exact SoftFloat backend while the per-frame trace path stays native-double.
#     Result: the loaded geometry is BIT-IDENTICAL to a full-exact build (no ULP
#     drift in built planes/spheres/cylinders) and traces are still fast.
#     Measured (x87_backend_geometry_check.sh / x87_backend_flip_rate.sh): pure
#     double drifts 1 curved patch on 1 of 16 maps; the hybrid is bit-identical
#     on all 16 with 0 trace flips / 68,369. Consumers using it must still link
#     softfloat.a and add -I<softfloat>/source/include (the construction TUs
#     include softfloat.h).
#
# Consumer usage (emulated build):
#   include build-mk/x87-backend.mk
#   ... $(CC) ... $(X87_BACKEND_CPPFLAGS) $(X87_BACKEND_CFLAGS) -DEMULATE_X87=1 ...
# ---------------------------------------------------------------------------

EMU_X87_BACKEND ?= softfloat

X87_BACKEND_CPPFLAGS :=
X87_BACKEND_CFLAGS :=

ifeq ($(EMU_X87_BACKEND),double)
  X87_BACKEND_CPPFLAGS := -DEMULATE_X87_BACKEND=EMU_X87_DOUBLE
  X87_BACKEND_CFLAGS := -ffp-contract=off
else ifeq ($(EMU_X87_BACKEND),softfloat)
  # Default backend: the header selects SoftFloat with no extra define. The
  # consumer is responsible for -I<softfloat>/source/include and linking
  # softfloat.a (it owns where the vendored library lives).
else
  $(error EMU_X87_BACKEND must be softfloat or double (got '$(EMU_X87_BACKEND)'))
endif
