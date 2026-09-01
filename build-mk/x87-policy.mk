# ---------------------------------------------------------------------------
# Shared x87 floating-point faithfulness policy (engine + game DLL).
#
# The original Call of Duty: United Offensive binaries were 32-bit x86 and did
# all floating-point math on the x87 FPU (80-bit internal precision, with
# specific rounding-to-32-bit-float at store points). A modern 64-bit build
# uses SSE for float math instead, which rounds differently. For most code the
# difference is a sub-ULP cosmetic drift, but in the collision / patch-winding
# geometry it can flip a discrete decision (e.g. whether a bevel plane is
# added), producing a build that is NOT bit-identical to the original and can,
# in principle, differ in in-game collision behavior.
#
# `-mfpmath=387` makes GCC emit x87 float math on x86-64 and is required to
# reproduce the original evaluation model.  It is necessary but not sufficient:
# modern GCC can still alter individual opcodes, dependencies, spills, and ABI
# boundaries even at -O0.  Per-site fidelity is established by the native x87
# compiler-realization audit in docs/x87-compiler-realization-audit.md; the
# mod-script smoke suite is a supplemental runtime gate.  The flag is only
# available with GCC on x86/x86-64 (and MinGW on Windows). clang does not
# implement it; non-x86 architectures (e.g. Apple Silicon / ARM) have no x87
# unit at all.
#
# This policy therefore refuses, by default, to produce a build that cannot use
# the required native x87 evaluation model, and tells the user how to select an
# x87-capable build (GCC on x86) or explicitly opt into a non-exact build.  The
# policy gate does not itself certify the emitted per-function graph.
#
# Consumers set (before including this file):
#   X87_POLICY_ARCH   := $(shell uname -m)   # the target architecture
#   X87_POLICY_CC     := $(CC)               # the compiler to probe
# This file then defines, for the consumer to use:
#   X87_FLOAT_FLAGS   -> compiler flags to add on x86-64 native/shared builds
#                        (`-mfpmath=387` when faithful, empty when relaxed)
#   X87_ALLOW_CPPFLAGS-> `-DCODUO_ENGINE_ALLOW_NON_X87_FLOAT=1` in relaxed mode
#                        (softens the source-level x87-required #error guards)
#
# User-facing knob:
#   CODUO_FP_FAITHFUL = auto (default) | strict | relaxed
#     auto    : be bit-exact if this toolchain can; otherwise hard-error.
#     strict  : require bit-exact; hard-error if this toolchain cannot.
#     relaxed : permit a build without whole-program native x87. Consumers may
#               select EMULATE_X87 source bodies, but any remaining plain C uses
#               SSE / native arm64 FP, so this is not yet a bit-exact rebuild.
#
# See docs/fp-faithfulness.md for the full explanation.
# ---------------------------------------------------------------------------

CODUO_FP_FAITHFUL ?= auto

# Is the target an x86 family architecture (the only place x87 exists)?
ifeq ($(filter i386 i486 i586 i686 x86_64 amd64,$(X87_POLICY_ARCH)),)
X87_POLICY_IS_X86 := 0
else
X87_POLICY_IS_X86 := 1
endif

# Does the chosen compiler accept -mfpmath=387? Probe once. (Empty unless yes.)
X87_POLICY_MFPMATH_OK := $(shell printf 'int main(void){return 0;}' \
	| $(X87_POLICY_CC) -mfpmath=387 -x c - -c -o /dev/null >/dev/null 2>&1 \
	&& echo yes)

# A 32-bit x86 build already uses x87 for everything, so -mfpmath=387 is a
# no-op there and faithfulness is automatic regardless of compiler.
ifeq ($(filter i386 i486 i586 i686,$(X87_POLICY_ARCH)),)
X87_POLICY_IS_I386 := 0
else
X87_POLICY_IS_I386 := 1
endif

# Two independent outputs:
#
#   X87_FLOAT_FLAGS    -> `-mfpmath=387` when we can & want a bit-exact build
#                         (the collision/patch-winding fix). Empty otherwise.
#
#   X87_ALLOW_CPPFLAGS -> `-DCODUO_ENGINE_ALLOW_NON_X87_FLOAT=1`. This enables
#                         the C fallbacks for operations whose inline x87 asm is
#                         unavailable on the target (notably fistp-based
#                         rounding and FPU control-word handling). Native
#                         fsincos activates on both i386 and x86-64. Other asm
#                         guards remain i386-only, so every non-i386 target still
#                         receives this define in every mode. (This is why it is
#                         orthogonal to the faithful/relaxed choice below.)
X87_FLOAT_FLAGS :=
X87_ALLOW_CPPFLAGS :=
ifeq ($(X87_POLICY_IS_I386),0)
X87_ALLOW_CPPFLAGS := -DCODUO_ENGINE_ALLOW_NON_X87_FLOAT=1
endif

define X87_POLICY_UNFAITHFUL_ERROR
This build cannot reproduce the original 32-bit engine's x87 floating-point
behavior on this toolchain (arch '$(X87_POLICY_ARCH)', compiler '$(X87_POLICY_CC)').
Float math (notably collision / patch-winding geometry) WILL deviate from the
reference binary, so this is NOT a bit-exact rebuild.

  For an EXACT rebuild:   use GCC on x86 / x86-64 (or MinGW on Windows).
  To build anyway here:   re-run with  CODUO_FP_FAITHFUL=relaxed
                          (acknowledges a not-yet-whole-program-exact build)

See docs/fp-faithfulness.md for details.
endef

# The faithful modes add -mfpmath=387 to make the plain-C float math x87-exact.
# A 32-bit x86 target is already all-x87, so the flag is unnecessary there.
ifeq ($(CODUO_FP_FAITHFUL),relaxed)
  # Acknowledged non-exact build: leave X87_FLOAT_FLAGS empty. The consumer may
  # enable EMULATE_X87 bodies; all other arithmetic remains SSE/native arm64 FP.
else ifeq ($(filter-out auto strict,$(CODUO_FP_FAITHFUL)),)
  # -fexcess-precision=fast: the original was built by a compiler that rounds
  # each float assignment to 32 bits (the traditional / pre-C99 behavior). With
  # x87 codegen, modern GCC's -std=c11 default (-fexcess-precision=standard)
  # instead keeps the 80-bit x87 value live across a `float` assignment and only
  # narrows at an observable point — so a `float f = <double expr>; ... f - k`
  # subtracts the 80-bit value, not the stored float. That diverges from stock by
  # 1 ULP on round-to-even ties (found in the capsule trace, where sqrt() yields
  # a double). `fast` restores stock's per-assignment rounding. (The EMULATE_X87
  # build is unaffected: it rounds explicitly at every store via x87f_store_f32.)
  # -O0 is FORCED on the faithful native x87 build: at -O2 the compiler keeps a
  # named `float` local live in an 80-bit x87 register instead of narrowing it to
  # float32 at the assignment, so the recon's float-spill points (which encode
  # stock's rounding) are not honored and the build diverges from stock. -O0
  # spills every local, restoring the narrowing. Verified on the game_mp_uo
  # common_math differential harness; see docs/x87-O0-required-O2-excess-precision.md.
  # (The EMULATE_X87 / shipped build is -O-independent — it rounds explicitly via
  # x87f_store_f32 — so it is NOT pinned here and can be optimized.)
  ifeq ($(X87_POLICY_IS_I386),1)
    # 32-bit x86 is x87 already; still needs fast for per-assignment rounding.
    X87_FLOAT_FLAGS := -O0 -fexcess-precision=fast
  else ifeq ($(X87_POLICY_MFPMATH_OK),yes)
    X87_FLOAT_FLAGS := -O0 -mfpmath=387 -fexcess-precision=fast
  else
    $(error $(X87_POLICY_UNFAITHFUL_ERROR))
  endif
else
  $(error CODUO_FP_FAITHFUL must be auto, strict, or relaxed (got '$(CODUO_FP_FAITHFUL)'))
endif
