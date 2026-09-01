# Vendored: Berkeley SoftFloat

Upstream: https://github.com/ucb-bar/berkeley-softfloat-3
Release:  **3e** (the final release; this package is frozen)
Snapshot commit: `a0c6494cdc11865811dec815d5c0049fba9d82a8` (2025-03-07)
License:  **BSD 3-clause** (John R. Hauser / UC Berkeley) — see `COPYING.txt`.
          Permissive; no copyleft. Safe to vendor and statically link.

## Why it is here

The x87 emulation work (`EMULATE_X87`) needs a bit-exact software
implementation of 80-bit extended-precision arithmetic to reproduce the
original engine's x87 float math on platforms with no x87 unit (arm64 / Apple
Silicon). SoftFloat's `extF80_*` operations provide exactly that, IEEE-correct
and battle-tested (QEMU/Bochs/86Box use it). See
`../../docs/platform-discrepancies/fp-emulation-design.md`.

## Do NOT update

SoftFloat 3e is release-frozen (no meaningful upstream changes; there is no 3f).
We snapshot 3e once and never pull updates — this is a faithfulness-critical
dependency where the exact bytes that validated a build must stay pinned. If a
real defect is ever found, patch it locally and note it here, do not re-vendor a
moving target.

## What we actually build

The whole `source/` tree is vendored for provenance and to avoid guessing
SoftFloat's internal (`s_*`) dependency closure, but our build compiles **only
the subset we use** (selected in the consuming Makefiles, gated by
`EMULATE_X87`):

- The **`extF80_*`** operations (the struct form, NOT the `extF80M_*` array form
  and NOT the C `long double` — SoftFloat's own `extFloat80_t` struct is portable
  and works on arm64, which has no native 80-bit type), plus their transitive
  `s_*` support routines and the `i32/i64/ui32/ui64` conversions.
- The **`source/8086/`** specialization (`specialize.h` + its `s_*` files). NOTE:
  `8086` selects **Intel x87 NaN / special-case *semantics*** — it is the FP
  behavior we reproduce, NOT the host architecture. We compile the `8086`
  specialization on every host (arm64 included), because we are emulating x87 on
  that host, not emulating the host's own FP. Do not switch to `ARM-VFPv2/` for
  an ARM build — that would reproduce ARM's FP conventions, the opposite of what
  we want.
- The `f16/f64/f128/bf16` format files are **not** built (we do not use those
  formats); they sit in the tree uncompiled.

The unused files are harmless dead weight (source is tracked; build artifacts and
binaries are not, so repo size is not a concern).

## The build config: `build/coduo-x87/`

`build/coduo-x87/` holds our static-library build config, and it is
**upstream's `template-FAST_INT64/Makefile` instantiated**, not a hand-written
one — the `==>` placeholder lines are activated (they already default to
`SPECIALIZE_TYPE ?= 8086` and `-DSOFTFLOAT_FAST_INT64`, exactly what we want) and
`-ffunction-sections -fdata-sections` is added so the consumer's static link can
GC unused code. It uses SoftFloat's own validated `OBJS_PRIMITIVES /
OBJS_SPECIALIZE / OBJS_OTHERS` object lists — NOT a `wildcard` of `source/`.

Do NOT switch to a `wildcard`-based file list: SoftFloat has two mutually
exclusive build modes (`FAST_INT64` vs not) with different file sets, and mixing
them (e.g. compiling the not-FAST_INT64 `s_*ExtF80M.c` files under FAST_INT64)
produces undefined-symbol errors. The curated object list is the correct set.

**One deviation from the stock template list:** the 5 bfloat16 object lines
(`bf16_isSignalingNaN`, `bf16_to_f32`, `f32_to_bf16`, `s_normSubnormalBF16Sig`,
`s_roundPackToBF16`) are removed. The stock `template-FAST_INT64` list includes
bf16, but bf16's NaN helpers (`softfloat_*BF16UI`) exist only in the `8086-SSE`
specialization, not the pure `8086` one we use — so bf16 does not compile under
`8086`, and we never use bfloat16 anyway. `build/coduo-x87/platform.h` is our
platform config (little-endian + GCC builtins; portable to x86-64 and arm64).

Build (either arch): `make -C vendor/softfloat/build/coduo-x87` →
`softfloat.a`. Verified building on arm64 (Apple Silicon) and x86-64, and
functionally (80-bit add + store-to-f32 correct).
