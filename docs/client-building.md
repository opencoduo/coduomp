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

The executable supports Apple Silicon macOS and 32-bit or 64-bit x86 Linux
and Windows.

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
its architecture-matched cgame, UI, and listen-server game modules:

```sh
make
```

The executable is written to `.workbench/build/client/native/CoDUOMP`, and its
modules are staged under `.workbench/build/client/native/uo/`.

Linux source builds support classic Minizip and Minizip-ng's compatibility
API. Install the matching development package that provides `minizip.pc`;
the build uses its installed headers and links its library through
`pkg-config`. Linux packages do not bundle Minizip. Keep the runtime library
matched to the implementation used to build the executable: the two providers
can use the same shared-library name with different public record layouts.

### Windows with clang-cl or MSVC

Clang is the preferred Windows compiler. The `clang-cl-x64` preset supports
80-bit `WIDE_FLOAT` storage and the existing native x87 arithmetic paths.
MSVC remains supported, but its `long double` is binary64 on both x86 and x64.
We accept the resulting narrowing; MSVC builds do not promise extended-precision
fidelity, even with `/arch:IA32` or optimization disabled.

Use Visual Studio 2022 17.9 or newer with the C++ desktop workload and CMake 4.4 or newer.
The CMake build reads the Makefile source manifests and builds the executable,
all three modules, and the dedicated server. The dedicated server uses
`cmake/msvc-posix/` for the POSIX names the MinGW build gets from its C library;
see `server-building.md`. The checked-in CMake presets generate Visual
Studio/MSBuild projects. Select the `msvc` preset for a **32-bit x86** build,
which uses `/arch:IA32` for compiler-generated x87 arithmetic:

```sh
cmake --preset msvc
cmake --build --preset msvc --parallel 2
```

Every Windows build defaults to Miniaudio, regardless of compiler or
architecture. Every Windows x86 build also includes optional Miles support;
no audio build flags or retail
DLL are needed to compile it. At startup, the executable checks for the retail
`mss32.dll` beside itself. If the DLL loads with the required exports, `Miles`
appears alongside `Miniaudio` in the audio backend selector. Keep the retail
`miles/` provider directory there too when using Miles. If Miles cannot load
or initialize, the client falls back to Miniaudio. The existing
`mss_3d_provider` setting and retail provider names remain supported.

All x64 builds use Miniaudio because the retail Miles DLL is 32-bit.
These build settings are shared by `stock` and `master`.

Explicitly select **64-bit x64** with:

```sh
cmake --preset msvc-x64
cmake --build --preset msvc-x64 --parallel 2
```

**MSVC x64 cannot generate x87 arithmetic from C/C++.** Its compiler-generated
floating-point expressions use SSE2 or newer instructions, so rounding and
intermediate results can differ from the original game. Configuration emits
a warning on this target. The x64 build uses Miniaudio.

For **64-bit Windows with native x87 in the existing explicit extended
expressions**, prefer Clang 22 or newer in clang-cl mode. Install LLVM and
Visual Studio's Clang tools support, then select the `ClangCL` toolset:

```sh
cmake --preset clang-cl-x64
cmake --build --preset clang-cl-x64 --parallel 2
```

The selected toolset must actually resolve to Clang 22 or newer; CMake checks
the compiler version. This profile uses `/fp:strict`, `-mlong-double-80`,
`-ffp-eval-method=source`, and `-ffp-contract=off`. Existing `WIDE_FLOAT`,
`long double`, and native x87 helpers use hardware x87.
Ordinary `float`/`double` expressions still use SSE, so this does not turn the
whole program into an x87 build. Assignment to `WIDE_FLOAT` alone does not
widen an expression computed entirely from narrower operands; existing casts
and explicit arithmetic boundaries determine which operations are extended.

The shared C math layer (`src/compat/libm/coduo_wide_math.h`) reconstructs
arithmetic cores from the original x87 instructions. Water log/exp and flame
power callers use it on every compiler. C expresses the arithmetic, domain
checks, exponent splitting, and power sign handling; the portability header
contains only hardware instruction and control-word adapters. The clang-cl
SDK header maps extended math names to these shared functions and handles
SDK ABI differences. No math dependency or software emulation is added.

Native x87 is selected by capability: GCC/Clang x86 with an 80-bit carrier,
and MSVC x86 with its binary64 carrier. MSVC x86 still introduces binary64
stores at the instruction adapters; native instructions alone do not establish
extended-precision parity. MSVC x64 and non-x86 targets use platform math.
Unsupported domains also use platform math. The power core preserves the
original binary64 base store and CRT control-word adjustment; water log/exp
keep the caller's control word. Existing `CoduoLibm` double boundaries remain
unchanged. These cores do not reproduce the complete original CRT's exception
handling or optional SSE2 power dispatch. Runtime parity remains unverified.
Windows APIs and dependencies keep their normal Microsoft ABI; the SDK is
not a general 80-bit long-double runtime.

Recovered CRT atan2 callers use a separate shared entry from inline `FPATAN`
callers. On native x87 it preserves the CRT's temporary precision setting,
positive-quadrant calculation, signed axis results and live extended return
through the caller's explicit rounding point. Non-x87 targets use native
`atan2l`; no attempt is made to reproduce x87 rounding there.

Do not add `-ffp-eval-method=extended` or disable SSE to this profile. Tested
Clang 19.1.7 and 22.1.8 global extended evaluation mispasses variadic doubles
and rejects ordinary C++ float initializer lists; disabling SSE breaks the
Windows x64 argument convention. Source-type evaluation avoids those failures.
This profile has been built with the native Linux clang-cl driver and
Microsoft SDK/linker through Wine; native Visual Studio IDE execution and
runtime arithmetic parity remain unverified.

The presets keep separate build directories at `.workbench/build/<preset-name>`; never change the architecture of an existing
build directory. Without presets, specify `-A Win32` or `-A x64` explicitly
for a Visual Studio generator, whose own default follows the host architecture.
For Ninja or NMake, use the matching x86 or x64 developer command prompt and
pass `-DCMAKE_BUILD_TYPE=Release` or `Debug` instead of `-A`. In Visual Studio's
CMake interface, select the desired MSVC or clang-cl entry from the
configure-preset selector.

Outputs are `bin/<configuration>/CoDUOMP.exe` and the cgame, UI, and game DLLs
under `bin/<configuration>/uo/`, relative to the build directory. The dedicated
server is staged as `bin/<configuration>/coduo_dedicated_recovered.exe`. Keep
the executable and modules from the same build and architecture together.

Architecture selection and source-path fidelity are separate. MSVC x86 can
retain extended intermediates in x87 registers even though its C `long double`
has the same storage format as `double`. All builds use native arithmetic;
software x87 is unsupported. The original Windows client and server binaries
are the behavioral authority on every host. `WIDE_FLOAT` and explicit stores
preserve the intended precision and rounding boundaries within each platform's
native types. Selecting x86 alone does not establish whole-program arithmetic
parity.

The default `CODUO_FP_FAITHFUL=auto` permits all three presets and reports
these limits; `relaxed` remains accepted for existing build commands. `strict`
rejects these Windows CMake profiles because whole-program parity remains
unverified. A separate `relaxed` acknowledgement is not needed to configure
the x64 preset; choosing it emits the architecture warning above.

Pinned, hash-checked zlib, SDL2, libjpeg-turbo, curl, and GL headers download
under the build directory. Set `BUNDLED_LIBS=OFF` and `CODUOMP_DEP_PREFIX` to use
matching MSVC dependencies instead. `CODUOMP_BUILD_CLIENT=OFF` builds the
modules and the dedicated server without downloading client dependencies; the
server links zlib only. Missing dependencies for an enabled target are a
configuration error.

The CRT is static by default (`/MT` for Release, `/MTd` for Debug).
`CODUOMP_STATIC_VCRT=OFF` selects the DLL CRT consistently for the product and
bundled libraries. JPEG uses its upstream-required separate CMake build;
configuration, toolchain, and CRT selection are passed to that child build.
No JPEG preconfiguration or source patch is necessary.

### macOS application bundle

On Apple Silicon macOS, build a Finder-launchable application and a ZIP for
basic distribution with:

```sh
make macos-app
make macos-zip
```

The outputs are `.workbench/build/macos/OpenCoDUO.app` and
`.workbench/build/macos/opencoduo-macos-arm64.zip`. The application contains
the engine, the reconstructed cgame/UI and listen-server game modules, and the
open-source runtime libraries it needs. It does not contain retail game data.

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
Mach-O dependencies to portable locations, and checks the package. The check
rejects debug debris, symlinks, unexpected extended attributes, and nonportable
dependencies or runtime paths. It does not inspect the builder's identity or
scan embedded strings for personal paths. Run it independently with:

```sh
make macos-package-check
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
Linux system libraries. Package staging strips debug and local-symbol data
and checks for build debris and symlinks. It does not require a hostname utility
or scan embedded strings for the builder's identity or paths.

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

For compiler-gated OpenGL GPU timing, renderer-phase attribution, and batch
census captures, see [Renderer GPU profiling](client-renderer-gpu-profiling.md).

#### Display modes

Graphics options provide four **Display Mode** choices. Select a **Monitor**
and **Resolution**, then apply the settings to restart the renderer.

| Display Mode | `r_fullscreen` | Behavior |
| --- | --- | --- |
| Windowed | `0` | A decorated window on the selected monitor. |
| Fullscreen | `1` | The existing desktop fullscreen mode; the desktop resolution stays unchanged. |
| Borderless | `2` | A borderless window covering the selected monitor. |
| Exclusive Fullscreen | `3` | Switches the selected monitor to the requested resolution (native display resolution with Fit Screen), with `r_displayRefresh` selecting a refresh rate when nonzero. |

The default remains Fullscreen. `r_display` selects the monitor from zero;
for example, `set r_display 1`, `set r_fullscreen 3`, then `vid_restart` uses
exclusive fullscreen on the second monitor. The display mode is restored
when exclusive fullscreen exits. On macOS, standard exclusive fullscreen on
the primary display can also capture and blank the other connected displays.
Use Fullscreen or Borderless when the other monitors should remain visible.

The **Gameplay View** selector also applies on a renderer restart:

| Gameplay View | `r_aspectMode` | Behavior |
| --- | --- | --- |
| Fill Screen (Wider FOV) | `0` | The default native-aspect world view with proportional HUD scaling. |
| Classic 4:3 (Letterboxed) | `1` | Fits the world and menus into a 4:3 area with black bars. |
| Fit Screen | `2` | Uses the full screen and the original stretched HUD/menu canvas, including when the selected resolution has a different aspect ratio from the display. |

Fit Screen uses the original horizontal FOV default without widescreen
widening. In Exclusive Fullscreen, the game still renders at the selected
resolution and stretches that image into the display's native resolution;
this also prevents the display itself from adding bars around a 4:3 mode.
If the native resolution cannot be queried, the desktop resolution is used.
Its selection persists when switching mods.

#### Mod HUD presentation

With `r_aspectMode 0`, `cg_modHudPresentation` selects the cgame HUD layout:

| Value | Behavior |
| --- | --- |
| `0` | Default: anchor stock HUD menus; stretch mod-authored HUD menus. |
| `1` | Stretch HUD positions, graphics, and text across the screen. |
| `2` | Keep the HUD on a centered, proportional 640×480 canvas. |
| `3` | Use the widescreen anchor model, including server HUD edge snapping when `cg_wideHudElems 1`. |
| `4` | Stretch HUD positions and graphics across the screen; keep text proportional to screen height. |

Use `set cg_modHudPresentation 4` for the continuous 640×480 mapping used by
some CoD adapters. Text is measured at its proportional width before center or
right alignment. Backgrounds and icons stretch horizontally, and outer margins
grow with screen width. This mode does not classify center regions or snap HUD
items to the edges. It covers cgame menus and server HUD elements; separate UI
menus retain their existing presentation.

The setting is archived and can be changed during play without restarting.
`cg_modHudPresentation 0` restores automatic selection. Classic 4:3 retains its
fitted viewport, and Fit Screen retains its original fully stretched canvas.

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

Native clients use the built-in Miniaudio backend by default. OpenAL remains a
deprecated backup backend on macOS and Linux; Linux uses libsndfile for its
streamed-audio decoding, and EFX reverb is enabled when the selected OpenAL
device supports it. The stock-named `mss_3d_provider` cvar selects the complete
audio backend on these clients.

Linux builds default `CASE_SENSITIVE_FS=1`. This preserves the original
case-insensitive game-file lookup on a case-sensitive host filesystem without
renaming retail or mod files. Set `CASE_SENSITIVE_FS=1` explicitly when
building for a case-sensitive macOS volume; ordinary macOS builds default it
to `0`.

The recovered executable, cgame, UI, and listen-server game module can also be
cross-compiled for 32-bit or 64-bit Windows with MinGW-w64. These are MinGW
targets, not builds performed with a native Windows compiler. The i686
executable automatically detects the retail `mss32.dll` at runtime and falls
back to Miniaudio when Miles is unavailable. Keep `mss32.dll` and its `miles/`
providers beside the executable to use Miles. Packages do not redistribute
these retail files, and compilation does not require them.

```sh
make client-windows-i686 \
  MINGW32_DEP_PREFIX="/path/to/i686-mingw-prefix"
```

For an installable ZIP containing the executable and all three recovered
multiplayer modules under `uo/`, run:

```sh
make client-windows-i686-package \
  MINGW32_DEP_PREFIX="/path/to/i686-mingw-prefix"
```

Build the experimental PE32+ package with:

```sh
make client-windows-x86_64-package \
  MINGW64_DEP_PREFIX="/path/to/x86_64-mingw-prefix"
```

Each dependency prefix must contain matching static MinGW builds of
libjpeg-turbo, libcurl, Minizip, zlib, and SDL2. Libcurl must enable FTP, HTTP,
and HTTPS and include its `bin/curl-config` script so the build can verify
download protocol support. The build verifies that the
selected compilers target i686 MinGW-w64 and rejects incompatible static
archives found in the dependency prefix. The prefix may reside on a mounted
Windows filesystem as long as its libraries target the same i686 MinGW ABI.
The Windows system libraries required by static libcurl are added even when
`MINGW32_LIBS` is overridden.

Windows package staging strips debug and local-symbol data, repeats the MinGW
runtime-DLL import audit on the staged files, and checks for build debris and
symlinks. It does not require a hostname utility or scan embedded strings for
the builder's identity or paths. When distributing builds, use compiler
file/debug prefix maps or equivalent settings in static dependencies to avoid
retaining absolute source paths.

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

The x86-64 build uses the built-in Miniaudio backend because a 64-bit process
cannot load the retail 32-bit `mss32.dll`. Compiler commands can be overridden
with `MINGW32_CC`, `MINGW32_CXX`, `MINGW32_DLLTOOL`, `MINGW32_OBJDUMP`,
`MINGW32_STRIP`, `MINGW64_CC`, `MINGW64_CXX`, `MINGW64_OBJDUMP`,
`MINGW64_STRIP`, `CC`, and `CXX`; dependency flags can be overridden with
`MINGW32_DEP_CPPFLAGS`, `MINGW32_DEP_LDFLAGS`, and `MINGW32_LIBS`. Run
`make help` for the primary build options.

### Developer packages and official releases

The platform package targets build distributable archives on their supported
hosts. They check package structure and runtime dependencies independently of
the builder's username, home directory, or hostname. Official releases use a
separate maintainer workflow with additional privacy checks on the configured
release hosts.

## Project status

The goal is functional equivalence with the original client, not a
byte-identical rebuild. Source reconstruction is complete, but the result is
still experimental and may contain reconstruction or original-product bugs.
Some original edge cases are deliberately preserved for compatibility.

Bug reports are most useful when they include the module, platform, exact build
command, and a short reproduction using an unmodified retail installation or a
clearly identified mod.
