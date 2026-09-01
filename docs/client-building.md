# Client building

Work-in-progress source for the Call of Duty: United Offensive multiplayer
executable, client-game module, and user-interface module.

This package contains maintained C and C++ source only. It does not include the
retail game, game data, proprietary binaries, generated decompiler output, or
private recovery records. You need a legally owned Call of Duty: United
Offensive installation to run the resulting client.

## Included components

- `src/client/engine/`: multiplayer executable code, including the renderer,
  client networking, platform boundaries, and listen-server interface code.
- `src/client/cgame/`: client-game code responsible for snapshots, prediction,
  effects, sound, HUD behavior, and other in-game presentation.
- `src/client/ui/`: multiplayer menus, server browser, controls, and frontend
  UI.
- `src/client/common/`, `src/client/math/`, and `src/client/menu/`: code shared
  only by client-side targets.
- `src/qcommon/`, `src/math/`, `src/collision/`, `src/filesystem/`,
  `src/animation/`, `src/scripting/`, and `src/sound/alias/`: common engine
  subsystems used by the executable and modules.
- `src/server/engine/`: server engine code also used by the executable's
  embedded listen server.
- `build-mk/`: explicit target source manifests and target build rules.

## Build

The native executable currently supports Apple Silicon macOS and 32-bit or
64-bit x86 Linux.

### Debian 12

Install the compiler and development packages:

```sh
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev libjpeg-dev \
  libcurl4-openssl-dev libminizip-dev zlib1g-dev libgl1-mesa-dev \
  libopenal-dev libsndfile1-dev
```

For a 32-bit build on an x86-64 Debian host, also enable i386 packages and
install the multilib compilers and 32-bit dependencies:

```sh
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install gcc-multilib g++-multilib pkg-config libsdl2-dev:i386 \
  libjpeg-dev:i386 libcurl4-openssl-dev:i386 libminizip-dev:i386 \
  zlib1g-dev:i386 libgl1-mesa-dev:i386 libopenal-dev:i386 \
  libsndfile1-dev:i386
```

### macOS

On a fresh Mac, install Apple's Command Line Tools (the full Xcode application
also provides them):

```sh
xcode-select --install
```

Install [Homebrew](https://brew.sh/) if it is not already available, then
install the remaining build dependencies:

```sh
brew install pkgconf sdl2 jpeg-turbo minizip
```

`pkgconf` supplies the `pkg-config` command used by the Makefiles. The macOS SDK
provides Clang, `make`, libcurl, zlib, and the OpenGL, OpenAL, and AudioToolbox
frameworks; they do not need separate Homebrew packages.

After installing the dependencies for your platform, build the executable and
its architecture-matched cgame/UI modules:

```sh
make
```

The executable is written to `.workbench/build/client/native/CoDUOMP`, and its
modules are staged under `.workbench/build/client/native/uo/`.

### macOS application bundle

On Apple Silicon macOS, build a Finder-launchable application and a ZIP for
basic distribution with:

```sh
make macos-app
make macos-zip
```

The outputs are `.workbench/build/macos/OpenCoDUO.app` and
`.workbench/build/macos/opencoduo-macos-arm64.zip`. The application contains
the engine, the reconstructed cgame/UI modules, and the open-source runtime
libraries it needs. It does not contain retail game data.

At launch, Open CoD:UO first reuses its saved game-data location. It can then
discover Steam app 2640 in Steam's configured macOS libraries or a conventional
legacy installation under `/Applications` or `~/Applications`. If discovery
does not find a complete installation, a native folder picker asks for the
parent containing both `main/pak0.pk3` and `uo/pakuo00.pk3`. The selected path
is remembered for later launches, and the retail files remain in place. A
command-line `+set fs_cdpath /path/to/install` overrides this onboarding.

The default `MACOS_SIGN_IDENTITY=-` applies an ad-hoc signature. It requires no
Apple developer account. A Mac that receives the ZIP over the internet may
still require its user to approve the unnotarized app through Finder's
**Open** action or macOS Privacy & Security. To use an installed Developer ID
certificate instead, supply its complete identity:

```sh
make macos-zip \
  MACOS_SIGN_IDENTITY="Developer ID Application: Organization Name (TEAMID)"
```

The packaging target strips debug and local-symbol data, rewrites bundled
Mach-O dependencies to portable locations, and runs a privacy gate. The gate
rejects embedded personal/build-host paths, debug debris, symlinks, and
nonportable dependencies or runtime paths. Run it independently with:

```sh
make macos-privacy-check
```

The ZIP target also rejects unexpected archive entries and omits resource-fork,
quarantine, ACL, `.DS_Store`, AppleDouble, and `__MACOSX` metadata. Set
`MACOS_BUNDLE_VERSION` and `MACOS_BUNDLE_BUILD` when preparing a numbered
release.

On Linux, the explicit architecture targets build the complete executable,
cgame, UI, and listen-server game-module set. They use GCC multilib mode and
keep their outputs separate:

```sh
make client-linux32
make client-linux64
```

Create complete archives with all three architecture-matched multiplayer
modules under `uo/` with:

```sh
make client-linux32-package
make client-linux64-package
```

The resulting archives are written under `.workbench/build/linux/`. They
contain the reconstructed executable and modules, but no retail game data or
Linux system libraries. Package staging strips debug and local-symbol data and
runs a privacy gate that rejects embedded personal, source-workspace, and
build-host paths.

To launch it as the normal system game, point `CLIENT_DATA_PATH` at a legally
owned retail installation root containing both `main/` and `uo/`:

```sh
make client-run \
  CLIENT_DATA_PATH="/absolute/path/to/Call of Duty UO" \
  CLIENT_ARGS="+set r_fullscreen 0 +set r_mode 6"
```

`client-run` uses the persistent system profile. On macOS this is
`~/Library/Application Support/OpenCoDUO/`, so normal configuration, favorites,
downloads, and screenshots remain available.

Automated and development tests must instead use `make client-test-run` with
the same `CLIENT_DATA_PATH` and optional `CLIENT_ARGS`. That target forces
`fs_homepath` below `.workbench/runtime/` and will not access the personal
profile. Agents must never invoke `client-run` for testing.

#### macOS Wi-Fi latency

Apple Wireless Direct Link (AWDL) can cause short periodic Wi-Fi latency spikes
on the Mac running the game. If the client shows regular lagometer spikes over
Wi-Fi, temporarily disable the AWDL and low-latency WLAN interfaces before
playing:

```sh
sudo ifconfig awdl0 down
sudo ifconfig llw0 down
```

This changes only the local Mac; it does not reconfigure the router or other
devices on the network. AirDrop, Handoff, Universal Control, and related
peer-to-peer features may be unavailable while the interfaces are disabled.
Restore them after playing with:

```sh
sudo ifconfig awdl0 up
sudo ifconfig llw0 up
```

Ethernet avoids this Wi-Fi-specific behavior without disabling those macOS
features.

See [CD-key location on macOS and Linux](../CDKEY.md) for the native `codkey`
filename, default locations, custom `fs_homepath` behavior, and safe migration
guidance, including the optional `UO_CODKEY` environment override.

The client uses a native OpenAL compatibility backend for the original Miles
audio boundary. Linux uses OpenAL Soft for playback and libsndfile for streamed
audio decoding; EFX reverb is enabled when the selected OpenAL device supports
it, with ordinary dry playback as the fallback.

Linux builds default `CASE_SENSITIVE_FS=1`. This preserves the original
case-insensitive game-file lookup on a case-sensitive host filesystem without
renaming retail or mod files. Set `CASE_SENSITIVE_FS=1` explicitly when
building for a case-sensitive macOS volume; ordinary macOS builds default it
to `0`.

The recovered executable, cgame, UI, and listen-server game module can also be
cross-compiled for 32-bit or 64-bit Windows with MinGW-w64. These are MinGW
targets, not builds performed with a native Windows compiler. The i686
executable uses the original Miles boundary and therefore requires the user to
provide `mss32.dll` from a legally owned retail installation. The build reads
its export table to generate a MinGW import library; packages do not copy or
redistribute the DLL.

```sh
make client-windows-i686 \
  MSS32_DLL="/path/to/retail/mss32.dll" \
  MINGW32_DEP_PREFIX="/path/to/i686-mingw-prefix"
```

For an installable ZIP containing the executable and all three recovered
multiplayer modules under `uo/`, run:

```sh
make client-windows-i686-package \
  MSS32_DLL="/path/to/retail/mss32.dll" \
  MINGW32_DEP_PREFIX="/path/to/i686-mingw-prefix"
```

Build the experimental PE32+ package with:

```sh
make client-windows-x86_64-package \
  MINGW64_DEP_PREFIX="/path/to/x86_64-mingw-prefix"
```

Each dependency prefix must contain matching static MinGW builds of
libjpeg-turbo, libcurl, Minizip, zlib, and SDL2. The build verifies that the
selected compilers target i686 MinGW-w64 and rejects incompatible static
archives found in the dependency prefix. The prefix may reside on a mounted
Windows filesystem as long as its libraries target the same i686 MinGW ABI.
The Windows system libraries required by static libcurl are added even when
`MINGW32_LIBS` is overridden.

Windows package staging strips debug and local-symbol data, repeats the MinGW
runtime-DLL import audit on the staged files, and applies the same release
privacy gate used by Linux packages. Static dependency archives must therefore
be built without retained absolute source paths; use compiler file/debug prefix
maps or an equivalent reproducible-build setting when producing them.

The i686 build is pinned to `-O0 -mfpmath=387
-fexcess-precision=fast`. At runtime the executable selects the retail
client's 53-bit x87 precision mode during startup.

The Windows executable also retains the retail PE stack configuration: an
8 MiB reserve with a 4 KiB initial commit. This is required by the original
renderer paths that use 2 MiB stack-local vertex staging areas. The
`mingw32` build verifies both PE header values after linking.

The retail cgame and UI DLLs contain x87 control-word helpers only in their
statically linked MSVC runtimes. Their process-attach paths pass zero to the
conditional precision hook and skip it, inheriting the executable's mode.
The recovered cgame/UI sources therefore do not install a separate control
word; the recovered executable owns the process setting.

The x86-64 build uses the explicit no-audio compatibility backend because a
64-bit process cannot load the retail 32-bit `mss32.dll`. It is suitable for
testing and online play without sound, but should not be presented as having
audio parity with the other releases. Compiler commands can be overridden
with `MINGW32_CC`, `MINGW32_CXX`, `MINGW32_DLLTOOL`, `MINGW32_OBJDUMP`,
`MINGW32_STRIP`, `MINGW64_CC`, `MINGW64_CXX`, `MINGW64_OBJDUMP`,
`MINGW64_STRIP`, `CC`, and `CXX`; dependency flags can be overridden with
`MINGW32_DEP_CPPFLAGS`, `MINGW32_DEP_LDFLAGS`, and `MINGW32_LIBS`. Run
`make help` for the primary build options.

### Four-platform release build

`make release-builds` runs on macOS, builds the macOS arm64 ZIP locally, and
uses SSH to build Linux x86-64 and both Windows packages in a fresh directory
on a configured Linux host. It archives the clean current commit and refuses
to overwrite an existing local release artifact. Supply the five required
`RELEASE_REMOTE_*` settings listed by `make help`; all four outputs are copied
to `RELEASE_OUTPUT_DIR` (default `.workbench/build/release`). Hostnames,
accounts, and remote paths remain invocation-time values and are not stored in
the repository.

## Project status

The goal is functional equivalence with the original client, not a
byte-identical rebuild. Source reconstruction is complete, but the result is
still experimental and may contain reconstruction or original-product bugs.
Some original edge cases are deliberately preserved for compatibility.

Bug reports are most useful when they include the module, platform, exact build
command, and a short reproduction using an unmodified retail installation or a
clearly identified mod.
