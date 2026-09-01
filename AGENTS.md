# Public Repository Instructions

## Branch contract

- Read `docs/branch-policy.md` before changing behavior or moving code between
  branches.
- `stock` is the hardened stock-compatible baseline. Preserve legitimate
  original gameplay, presentation, defaults, protocols, formats, ordering,
  arithmetic, precision, and supported-content behavior.
- `stock` may contain shared hardening, required 64-bit and ABI adaptations,
  required compatibility layers, and platform-local adaptations that preserve
  observable results.
- `master` descends from `stock` and owns the improved product's intentional
  non-stock behavior: ordinary original-game bug fixes, features, changed
  defaults or limits, presentation changes, optional compatibility, and general
  optimizations.
- Make changes in the correct concrete branch. Never add `STRICT_STOCK` or an
  equivalent compile-time or runtime selector that recombines the branches.

## Security and provenance

- Shared security hardening belongs in both branches. A stock false positive
  against legitimate supported content is a bug and must be resolved before
  release.
- Public comments should state the maintained invariant directly: bounds,
  ownership, type or ABI requirements, sequencing, and failure behavior.
- Do not add exploit recipes, severity assessments, private report paths,
  vulnerable-retail trigger comparisons, or labels such as `SECURITY_PATCH`.
  Coordinate sensitive details with maintainers through a private channel.
- Preserve `NOT_FROM_ORIGINAL_SOURCE` on non-original helpers and adaptations.
  New non-original functions must carry that marker and use a lower-case
  project, compatibility, or adaptation namespace such as `coduomp_*`.
- Do not add private analysis records, retail binaries, generated decompiler
  output, source-recovery ledgers, or machine-specific workspace paths.

## Change discipline

- Do not delete an existing code comment unless the complete code section it
  describes is also removed. Rewrite an inaccurate comment in place.
- Keep nearby C/C++ formatting; do not mechanically hard-wrap unaffected code.
- Put generated builds, packages, logs, caches, and disposable runtime state
  under the repository-root `.workbench/`; do not create ignored output beside
  maintained source.
- Release tooling must not embed a maintainer's username, home directory,
  hostname, workspace name, network address, or private remote path. Keep
  machine-specific release settings as invocation-time values and run the
  maintained package privacy/import audits.
- Validate source-policy invariants with `make policy-check` before committing.
- Build and test every affected supported target in proportion to the change.
- `client-run` is the user-facing normal-game launcher and intentionally uses
  the persistent system profile. Agents must never invoke it. Native client
  tests must use `client-test-run`, which supplies an explicit disposable
  `fs_homepath`; never run a test client against a personal game profile.
