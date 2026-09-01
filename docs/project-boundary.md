# Project Boundary

This repository is the maintained public source for the reconstructed CoD:UO
multiplayer client, dedicated server, and game modules. After its initial
fresh-history checkpoint, ordinary source development occurs here.

It includes maintained, compileable source:

- server game-module C sources and headers under `src/server/game/`;
- dedicated process and common server-engine C/C++ sources under
  `src/server/standalone/` and `src/server/engine/`;
- public build and status documentation.

It intentionally excludes private recovery workspace material:

- local agent instructions, private goals, and working notes;
- operational host notes and server-access material;
- private analysis directories, including generated output, Ghidra scripts, evidence notes,
  task ledgers, unknown ledgers, overlays, schemas, and manifests;
- retail binaries, raw proprietary payloads, Ghidra databases, and bulk
  generated decompiler output;
- local build artifacts and caches.

The public source is intended to be understandable and buildable without the
private analysis workspace. The private historical repository may retain
original-machine-code evidence, provisional analysis, and embargoed security
notes, but it is not a synchronized source mirror. New product work and public
fixes are developed and committed here.
