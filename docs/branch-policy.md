# Stock and Master Branch Policy

This repository has two maintained public products:

- `stock`: a hardened stock-compatible baseline; and
- `master`: the customized/improved product, based on `stock`.

## Stock boundary

Stock preserves behavior for legitimate supported inputs, including gameplay,
presentation, defaults, network and file formats, VM and module interfaces,
ordering, arithmetic, precision, and valid-input failure behavior. The original
Windows i386 client and Linux i386 server/module behavior define the historical
platform contract.

Stock also carries implementation changes required to realize that contract on
supported systems:

- shared security hardening;
- pointer-width and calling-convention adaptations for wider hosts;
- required platform and library compatibility layers; and
- narrowly scoped platform adaptations when the original route is unavailable
  or impractical and the replacement preserves observable results.

A hardening check may reject malformed input, but it must not reject legitimate
stock maps, mods, packages, servers, or player behavior. Treat a false positive
as an unresolved bug in the check.

## Improved boundary

Improved descends from stock and contains intentional product changes:

- ordinary fixes for original-game bugs that are not shared hardening;
- features and expanded mod support;
- presentation, branding, UI, and usability changes;
- changed defaults, limits, policies, or diagnostics;
- optional compatibility behavior; and
- general performance optimizations.

The `stock..master` diff is the maintained description of customization.
Do not recreate a mixed source tree with `STRICT_STOCK`, another preprocessor
selector, a make variable, or a runtime switch.

## Public explanations

Keep public comments useful to developers. State exact invariants, bounds,
ownership, sequencing, ABI requirements, portability decisions, and failure
semantics. Preserve `NOT_FROM_ORIGINAL_SOURCE` provenance where applicable.

Keep sensitive failure discovery and exploitation details out of public source,
issues, and commit messages. Do not publish private report paths, exploit
recipes, severity assessments, vulnerable-retail trigger comparisons, or
labels such as `SECURITY_PATCH`. Use a private maintainer channel for those
details and express the public rule directly.

Run `make policy-check` on both branches before release.
