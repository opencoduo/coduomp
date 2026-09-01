# Client Recovery Instructions

Follow the repository-root `AGENTS.md` for recovery, audit, validation, and
workbench policy.

Scope client work from the user's current request, a runtime/debugging symptom,
a specific original-machine-code range, or a narrow subsystem boundary. Do not
use the archived Ghidra, mcode, coverage, manifest, or historical recovery
records as the current workflow unless the user explicitly requests historical
artifact maintenance.

The original Windows i386 client machine code is authoritative for behavior,
layout, access widths, signedness, branches, call and argument order, constants,
calling convention, and x87 behavior. Use direct binary inspection to prove any
behavioral change. Related server source, Mac symbols, inherited engine names,
and prior recovery records are corroboration only.

Keep `src/client/cgame/` compileable. Prefer typed structures, fields, arrays, enums,
and named constants over pointer arithmetic, address-shaped placeholders, and
machine-shaped pseudo-source. Put declarations shared by multiple client source
files in the appropriate maintained header rather than duplicating local
declarations.

Do not reconstruct statically linked compiler, CRT, or third-party runtime code
as original client game source. Mark compatibility or portability helpers
according to the repository rules for `NOT_FROM_ORIGINAL_SOURCE` code.
