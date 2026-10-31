# Building

Linux dedicated-server and game-module builds use the maintained Make targets.
Use `make server32` or `make server64` to build both components for the selected
architecture. Windows builds with MinGW also use Make.

The public tree contains two independently useful target families:

- `build-mk/server.mk` builds the standalone server from
  `src/server/standalone/`, `src/server/engine/`, and the common engine source.
- `build-mk/game.mk` builds the server game module from `src/server/game/` and
  its shared BG/qcommon/math dependencies.

Top-level Make targets compile in parallel by default, with one job per
detected processor. `make JOBS=N <target>` selects a budget and `JOBS=1`
builds serially. An existing `make -jN` jobserver takes precedence over
`JOBS` and is shared by recursive builds. Direct `make -f build-mk/...`
invocations need an explicit `-jN` to compile in parallel.

## CD-key Authorization

On `master`, dedicated and listen servers skip external CD-key authorization
by default. Dedicated-server builds can enable authorization with `AUTH=1`:

```sh
make server64 AUTH=1
make server32 AUTH=1
```

When invoking `build-mk/server.mk` directly, set
`CODUO_DISABLE_SERVER_AUTH=0` to enable authorization. The `stock` branch keeps
its original authorization defaults.

## Requirements

- `make`
- C and C++ compilers such as `cc`, `c++`, `clang`, or `gcc`
- `ar`
- For Linux builds, GCC/G++ and zlib development files; 32-bit builds also
  require the multilib toolchain and 32-bit zlib development library
- For Windows cross-builds, an architecture-matched MinGW compiler,
  `i686-w64-mingw32-gcc` or `x86_64-w64-mingw32-gcc`, and its matching `g++`
- MinGW-compatible zlib headers and library for the Windows engine

## Linux dedicated server (Make)

Run these commands from the repository root on Linux. For a complete 64-bit
dedicated server and its matching game module:

```sh
make server64
```

For the 32-bit engine and module, with a multilib toolchain and 32-bit zlib
development library installed:

```sh
make server32
```

Each target builds both components. The engine and module must have matching
architectures; keep the module from the same build with the executable when
deploying. The component output paths are listed below. See the
[Linux dependency instructions](../README.md#debian-or-ubuntu-linux-x86-64)
for Debian/Ubuntu setup.

### Build individual Linux components

Use these targets when building components separately. The full server
targets above already build both the engine and the game module.

To build only the 32-bit Linux game module:

```sh
make server-game32
```

Output:

```text
.workbench/build/game/linux-i386/game.mp.uo.i386.so
```

This target uses `-m32`, so it requires a Linux toolchain with 32-bit compiler
and library support installed.

To build only the 64-bit Linux game module:

```sh
make server-game64
```

Output:

```text
.workbench/build/game/linux-x86_64/game.mp.uo.x86_64.so
```

To build only the 64-bit Linux dedicated-server executable:

```sh
make server-engine64
```

Output:

```text
.workbench/build/server/linux-x86_64-noauth/coduo_lnxded_recovered
```

Use `make server-engine32` for the 32-bit executable. Its engine output
directory is `linux-i386-<auth-mode>`. For either architecture, the suffix is
`auth` when `AUTH=1` and `noauth` when `AUTH=0`.

## Compile Checks

These developer checks compile archives or check shared-library linking.
Use `make server32` or `make server64` above to build a complete Linux server.

```sh
make -f build-mk/game.mk check CODUO_FP_FAITHFUL=relaxed
make -f build-mk/server.mk objects CODUO_FP_FAITHFUL=relaxed
make -f build-mk/server.mk shared-check-link CODUO_FP_FAITHFUL=relaxed
```

The game-module check and dedicated-engine object target compile maintained
source into static archives. The shared link check produces a shared library,
not a runnable dedicated-server executable.

## Windows with MSVC or clang-cl

The Windows SDK requirements and warning policy are shared with the client; see
[Windows compiler requirements](client-building.md#windows-with-clang-cl-or-msvc).

CMake is supported only for native Windows builds with MSVC or clang-cl.
Use CMake 4.4+ and MSVC 19.39+ or clang-cl 22+. The server target builds the
dedicated engine and matching game DLL for the configurations below:

| Preset | Architecture | Compiler |
| --- | --- | --- |
| `msvc` | x86 (32-bit) | MSVC |
| `msvc-x64` | x86-64 | MSVC |
| `clang-cl-x64` | x86-64 | clang-cl 22+ |

Each preset builds the client, its three modules, and the dedicated server by
default. Select any row's preset for both commands:

```sh
cmake --preset msvc
cmake --build --preset msvc --parallel 2
```

The Visual Studio MSVC presets apply this budget to compiler processes within
each project and build project references sequentially. `--parallel` without
a number uses the processor count. See the [client build guide](client-building.md#windows-with-clang-cl-or-msvc)
for serial builds, PDB coordination, and other generators/toolsets.

For a server-only build, disable the client and its optional modules. The
server still builds its required game DLL; only zlib is fetched as a dependency:

```sh
cmake --preset msvc -DCODUOMP_BUILD_CLIENT=OFF -DCODUOMP_BUILD_MODULES=OFF
cmake --build --preset msvc --target coduo_dedicated_recovered --parallel 2
```

Outputs are `.workbench/build/<preset>/bin/Release/coduo_dedicated_recovered.exe`
and `bin/Release/uo/uo_game_mp_x86.dll` in that same build directory. The DLL
filename is the same on x86 and x64; the engine and DLL must have matching
architectures. Selecting the server target builds both files.

`BUNDLED_LIBS=OFF` with `CODUOMP_DEP_PREFIX` selects architecture-matched MSVC
libraries instead. A server-only configuration then resolves only zlib.
`CODUOMP_BUILD_SERVER=OFF` retains a client/module-only configuration.
`CODUOMP_DISABLE_SERVER_AUTH=ON` disables external CD-key authorization.
Its default preserves the branch's Makefile authorization setting. The Windows
CMake server uses the disabled PunkBuster provider; the recovered dynamic
PunkBuster loader currently requires POSIX services.

Both Windows engine build systems reserve 8 MiB of stack with a 4 KiB initial commit.
MSVC's binary64 `long double` limits and clang-cl's explicit extended-arithmetic
profile also apply to the server; see [client build limits](client-building.md#windows-with-clang-cl-or-msvc).

## Windows dedicated server with MinGW (Make)

The top-level MinGW Make targets build the engine and matching game DLL.
`server-windows` builds both 32-bit and 64-bit products:

```sh
make server-windows
make server-windows-i686
make server-windows-x86_64
make server-windows-i386
```

Outputs:

```text
.workbench/build/server/windows-i386/coduo_dedicated_recovered.exe
.workbench/build/game/windows-i386/uo_game_mp_x86.dll
.workbench/build/server/windows-i686/coduo_dedicated_recovered.exe
.workbench/build/game/windows-i686/uo_game_mp_x86.dll
.workbench/build/server/windows-x86_64/coduo_dedicated_recovered.exe
.workbench/build/game/windows-x86_64/uo_game_mp_x86.dll
```

The i386 and i686 targets use the 32-bit MinGW ABI; x86-64 uses PE32+.
All targets use `-O0 -mfpmath=387
-fexcess-precision=fast` in both engine and module C/C++ compilation. The
Windows engine establishes the retail-compatible 53-bit x87 precision mode at
startup and restores it after loading the game DLL. The distinction between
the targets is the generated instruction baseline: `-march=i386` or
`-march=i686`.

The engine needs MinGW zlib. If it is not installed in the compiler's default
search path, point `MINGW32_DEP_PREFIX` or `MINGW64_DEP_PREFIX` at a prefix containing `include/zlib.h`
and `lib/libz.a`:

```sh
make server-windows-i686 MINGW32_DEP_PREFIX=/opt/mingw-i686
make server-windows-x86_64 MINGW64_DEP_PREFIX=/opt/mingw-x86_64
```

Override the compiler names when necessary:

```sh
make server-windows-i386 \
  MINGW32_CC=i386-mingw32-gcc \
  MINGW32_CXX=i386-mingw32-g++ \
  WINDOWS_DEP_PREFIX=/opt/mingw-i386
```

The generated DLL exports only `dllEntry` and `vmMain`, matching the retail
module. Copy the DLL into the game's `uo` directory when testing.
Place the engine executable beside the `main/` and `uo/` game-data directories,
or pass an explicit `+set fs_basepath`. The engine loads
`uo/uo_game_mp_x86.dll` through its Win32 loader path.

One behavior deliberately differs from the retail Linux server: localized pak
acceptance uses the Windows engine's fourteen-entry language table. The retail
Linux server accepts only `localized_english_*`, so a non-English install cannot
mount the localized pak that carries `default_mp.cfg` and startup fails with
`Couldn't load default_mp.cfg`. The standalone server mounts the localized pak
for every supported language. When `cl_language` is not passed on the command
line, startup falls back to the installed localized language that carries
`default_mp.cfg`; pass `+set cl_language <index>` to choose a language
explicitly. `fs_ignoreLocalized 1` still disables localized assets.

Localized paks are excluded from the pure-server pak list (`sv_pakNames`), so a
client does not need the server's localized paks to pass `sv_pure` checks.

## 32-bit Module Notes

The original Linux game module is i386. Building the 32-bit shared module
requires a Linux toolchain with i386 support.

## Build Outputs

Build products are intentionally ignored. Clean them with ordinary non-forced
file deletion or by removing the generated build directories manually.
