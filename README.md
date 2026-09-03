# coduomp

Reconstructed source for Call of Duty: United Offensive
multiplayer client and dedicated-server components.

This repository contains source and public build dependencies only. It does
not contain game data, binaries, private binary-analysis records, generated
decompiler output, or recovery ledgers.

See [the changelog](CHANGELOG.md) for a concise list of user-visible differences from the stock game.

## Branches

- `stock` is the hardened stock-compatible baseline. It preserves legitimate
  original behavior while carrying shared hardening and required platform,
  ABI, and compatibility support.
- `master` descends from `stock` and contains the improved product: intentional
  product changes, ordinary original-game bug fixes, features, and optional
  enhancements.

See [the branch policy](docs/branch-policy.md) for the exact boundary. Run
`make policy-check` before committing or releasing either branch.

## Layout

- `src/client/engine/`: multiplayer client executable.
- `src/client/cgame/`: client-game module.
- `src/client/ui/`: user-interface module.
- `src/server/standalone/`: dedicated-server process and platform boundaries.
- `src/server/engine/`: server engine shared with the client's listen server.
- `src/server/game/`: server game module.
- `src/`: common qcommon, math, BG, collision, filesystem, animation,
  scripting, sound, and compatibility subsystems.
- `build-mk/`: explicit source manifests and component build rules.
- `vendor/`: tracked third-party source required by the builds.

## Build

The default target builds the native client executable, cgame, and UI:

```sh
make
```

Useful client targets:

```sh
make client
make macos-app
make macos-zip
make client-linux32
make client-linux64
make client-linux32-package
make client-linux64-package
make client-windows-i686-package \
  MSS32_DLL=/path/to/mss32.dll \
  MINGW32_DEP_PREFIX=/path/to/i686-mingw-prefix
make client-windows-x86_64-package \
  MINGW64_DEP_PREFIX=/path/to/x86_64-mingw-prefix
```

The Windows i686 build uses the retail 32-bit Miles library. Every other
client uses the built-in Miniaudio backend by default, with OpenAL retained as
a deprecated backup where it is available. Package targets create complete
architecture-matched client/module archives without retail game data. The
macOS target creates a Finder-launchable application bundle.

From macOS, `make release-builds` creates the macOS arm64 package locally and
dispatches Linux x86-64 plus Windows i686/x86-64 builds to a configured Linux
build host. Its host and path settings are intentionally supplied at invocation
time and are never stored in the repository. Run `make help` for the required
`RELEASE_REMOTE_*` options.

The Linux dedicated-server targets build the engine and matching game module:

```sh
make server64
make server32
```

Windows dedicated-server targets are independent of the Windows client target:

```sh
make server-windows-i386
make server-windows-i686
```

Generated build, package, and runtime files stay under `.workbench/`. Use
`make help` for build options. Linux i386 and MinGW targets require their
corresponding multilib or cross-compilation toolchains. Native builds on a
non-x87 host must explicitly select `CODUO_FP_FAITHFUL=relaxed`; exact server
builds use GCC on x86/x86-64 or the original i386 target. Release package
targets strip staging copies and reject embedded personal or build-host paths.

To build and run the native client as the normal system game, point it at a
game-data root containing `main/` and `uo/`:

```sh
make client-run \
  CLIENT_DATA_PATH="/absolute/path/to/Call of Duty UO" \
  CLIENT_ARGS="+set r_fullscreen 1"
```

`client-run` uses the system's persistent OpenCoDUO profile, including normal
configuration, favorites, downloads, and screenshots. On macOS this is
`~/Library/Application Support/OpenCoDUO/`.

Automated and development tests must use the isolated launcher instead:

```sh
make client-test-run \
  CLIENT_DATA_PATH="/absolute/path/to/Call of Duty UO" \
  CLIENT_ARGS="+set r_fullscreen 0"
```

`client-test-run` always supplies an explicit `fs_homepath` below
`.workbench/runtime/`; it will refuse a runtime or home directory outside that
root. Agents must never use `client-run` for testing.

To launch an existing native build in isolation without rebuilding it, run the
following from the repository root:

```sh
CODUOMP_ROOT="$(pwd -P)"
mkdir -p "$CODUOMP_ROOT/.workbench/runtime/manual/home"
cd "$CODUOMP_ROOT/.workbench/runtime/manual"

"$CODUOMP_ROOT/.workbench/build/client/native/CoDUOMP" \
  +set fs_basepath "$CODUOMP_ROOT/.workbench/build/client/native" \
  +set fs_cdpath "/absolute/path/to/Call of Duty UO" \
  +set fs_homepath "$CODUOMP_ROOT/.workbench/runtime/manual/home"
```

`fs_basepath` locates the reconstructed native cgame and UI modules staged
under `.workbench/build/client/native/uo/`. `fs_cdpath` locates the game-data
`main/` and `uo/` directories. `fs_homepath` keeps generated configuration,
logs, downloads, and caches isolated from personal profiles.

See [per-server client configuration](docs/client-server-configs.md) for the
console commands that promote the current server profile to the global config
or clear every isolated server config while preserving downloaded content.

See [client build notes](docs/client-building.md),
[server build notes](docs/server-building.md), and
[the project boundary](docs/project-boundary.md) for platform details and
repository scope.

## Status

The goal is functional equivalence rather than a byte-identical rebuild.
Source reconstruction is complete, while validation and original-product bug
triage continue.
