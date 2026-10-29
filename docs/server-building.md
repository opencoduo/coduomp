# Building

The public tree contains three independently useful target families:

- `build-mk/server.mk` builds the standalone server from
  `src/server/standalone/`, `src/server/engine/`, and the common engine source.
- `build-mk/game.mk` builds the server game module from `src/server/game/` and
  its shared BG/qcommon/math dependencies.
- `CMakeLists.txt` builds the dedicated server and its game module with the
  Visual Studio toolchain on Windows and with GCC on Linux; see
  [Native Linux build (CMake)](#native-linux-build-cmake) and
  [Windows with MSVC or clang-cl](#windows-with-msvc-or-clang-cl).

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
- For Windows cross-builds, an architecture-matched MinGW compiler,
  `i686-w64-mingw32-gcc` or `x86_64-w64-mingw32-gcc`, and its matching `g++`
- MinGW-compatible zlib headers and library for the Windows engine
- For native Windows builds, CMake 3.24+ and MSVC 19.39+ or clang-cl 22+
- For the native Linux CMake build, CMake 3.24+ and GCC on x86 or x86-64;
  the `linux-clang-64bit` preset additionally supports Clang with the documented
  relaxed floating-point policy

## Compile Checks

```sh
make -f build-mk/game.mk check CODUO_FP_FAITHFUL=relaxed
make -f build-mk/server.mk objects CODUO_FP_FAITHFUL=relaxed
```

The game-module check compiles source into a static archive. The
dedicated-engine check compiles maintained engine code into a static archive.

## Shared Module Builds

For server deployment, build the 32-bit Linux module:

```sh
make server-game32
```

Output:

```text
.workbench/build/game/linux-i386/game.mp.uo.i386.so
```

This target uses `-m32`, so it requires a Linux toolchain with 32-bit compiler
and library support installed.

For local shared-object experiments using the host toolchain:

```sh
make server-game64
```

Output:

```text
.workbench/build/game/linux-x86_64/game.mp.uo.x86_64.so
```

For a local dedicated-engine shared-object link check, use the target Makefile:

```sh
make -f build-mk/server.mk shared-check-link CODUO_FP_FAITHFUL=relaxed
```

For a native dedicated-server executable:

```sh
make server-engine64
```

Output:

```text
.workbench/build/server/linux-x86_64-noauth/coduo_lnxded_recovered
```

The engine loads its game module at runtime, so a deployable Linux server
needs both the executable and the matching module. `make server64` builds both
(the engine plus `server-game64`); `make server-engine64` builds only the
engine. The Makefile stages the module in `.workbench/build/game/`, so copy
`game.mp.uo.x86_64.so` into the server's `uo/` directory before starting it.
The CMake build does this staging automatically.

## Native Linux build (CMake)

`CMakeLists.txt` builds the dedicated server natively with GCC by default.
A clean Ubuntu installation needs CMake 3.24 or newer and the compiler
toolchain; the bundled zlib in `libs/` is used by default, so no development
packages are required:

```sh
sudo apt-get update
sudo apt-get install -y cmake build-essential
```

Configure and build the 64-bit server with the recommended GCC preset:

```sh
cmake --preset linux-gcc-64bit
cmake --build --preset linux-gcc-64bit --parallel
```

That is the complete native Linux build. The equivalent explicit form is only
needed when selecting another build directory or generator:

```sh
cmake -S . -B .workbench/build/linux-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .workbench/build/linux-cmake --parallel
```

The Unix Makefiles generator is single-configuration: the build type is chosen
at configure time with `CMAKE_BUILD_TYPE`, which defaults to `Release` in the
preset. Build a debug binary with `cmake --preset linux-gcc-64bit -DCMAKE_BUILD_TYPE=Debug`;
`--config` only selects a configuration with multi-configuration generators
such as Visual Studio. Both configurations use the `build-mk/server.mk` flags
and x87 policy, so Debug differs only by its debug information.

Outputs:

```text
.workbench/build/linux-gcc-64bit/bin/Release/coduo_dedicated_recovered
.workbench/build/linux-gcc-64bit/bin/Release/uo/game.mp.uo.x86_64.so
```

With `CMAKE_BUILD_TYPE=Debug` the same files are staged under
`.workbench/build/linux-gcc-64bit/bin/Debug`.

The server target depends on the native game module, so one build produces
both. The engine loads `uo/game.mp.uo.<arch>.so` at runtime, so the module is
required as well as the executable; copying only the executable produces a
server that fails when it starts the first map. The native configuration builds
the architecture the compiler targets, which is x86-64 on a standard host. Use
the Makefile `server32` target for the i386 engine and module; both
architectures speak the same network protocol.

The faithful x87 policy pins the build to `-O0 -mfpmath=387
-fexcess-precision=fast` on x86-64, so GCC is required for the faithful model.
Configure with `-DCODUO_FP_FAITHFUL=relaxed` to accept a non-x87 build on
another toolchain or architecture.

### Linux Clang build (alternative)

GCC is the recommended compiler for every Linux build. Clang cannot emit GCC's
`-mfpmath=387` model on x86-64, so a Clang build is an acknowledged non-exact
floating-point build: collision and patch-winding geometry can round at
different intermediate steps and change discrete decisions. Use
`linux-clang-64bit` only for compile testing, porting and tooling work. It
selects the relaxed policy explicitly and needs the Clang package and its C++
front end:

```sh
sudo apt-get update
sudo apt-get install -y cmake clang
cmake --preset linux-clang-64bit
cmake --build --preset linux-clang-64bit --parallel
```

Outputs are the same as the GCC build under
`.workbench/build/linux-clang-64bit/bin/Release/`. A Clang server is fine for
connectivity, protocol and asset testing, but keep the GCC preset for a
float-faithful server. A missing Clang is reported at configure time with the
package name above.

The client executable and its Windows modules are only generated by the Visual
Studio configuration.

`BUNDLED_LIBS=OFF` links a system zlib (`zlib1g-dev`) instead. If the compiler or
`make` is missing, configure stops and names the package to install.

### Running the CMake-built server

No configuration files are needed for a first test: these arguments set
everything the server needs, and it writes its own configuration into
`fs_homepath`.

```sh
cd .workbench/build/linux-gcc-64bit/bin/Release

# Directory that contains the game data, with main/ and uo/ directly below it.
GAME_DATA=/path/to/call-of-duty-uo

# Keep runtime state out of the game-data directory and load the module from it.
mkdir -p /tmp/coduomp-home/uo
cp uo/game.mp.uo.x86_64.so /tmp/coduomp-home/uo/

./coduo_dedicated_recovered \
  +set dedicated 2 \
  +set net_ip "localhost" \
  +set net_port 28960 \
  +set sv_hostname "test" \
  +set sv_pure 0 \
  +set fs_basepath "$GAME_DATA" \
  +set fs_homepath /tmp/coduomp-home \
  +map mp_carentan
```

`fs_basepath` is the directory that contains `main/` and `uo/`. Setting
`fs_homepath` to a temporary directory keeps the game-data tree untouched. On a
case-sensitive filesystem the game-data directories must be named `main` and
`uo` in lower case. `mp_carentan` ships with the game; replace it with another
map shipped in the game data. Stop the server with Ctrl+C.

## Windows with MSVC or clang-cl

The same CMake target builds a dedicated engine and matching game DLL for
all supported Windows compiler configurations:

| Preset | Architecture | Compiler |
| --- | --- | --- |
| `windows-msvc-32bit` | x86 (32-bit) | MSVC (recommended) |
| `windows-msvc-64bit` | x86-64 | MSVC |
| `windows-clangcl-64bit` | x86-64 | clang-cl 22+ |

The 32-bit preset is the recommended Windows build: `/arch:IA32` keeps
compiler-generated arithmetic on the x87 unit and the 32-bit module matches the
shipped module's instruction baseline. The x64 presets are alternatives for
64-bit hosts; see [client build limits](client-building.md#windows-with-clang-cl-or-msvc)
for their floating-point differences.

Each preset builds the client, its three modules, and the dedicated server by
default. Select any row's preset for both commands:

```sh
cmake --preset windows-msvc-32bit
cmake --build --preset windows-msvc-32bit --parallel 2
```

For a server-only build, disable the client and its optional modules. The
server still builds its required game DLL; only zlib is fetched as a dependency:

```sh
cmake --preset windows-msvc-32bit -DCODUOMP_BUILD_CLIENT=OFF -DCODUOMP_BUILD_MODULES=OFF
cmake --build --preset windows-msvc-32bit --target coduo_dedicated_recovered --parallel 2
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

The Linux server needs no explicit stack reservation: the original 1.51b
`coduo_lnxded` has no `PT_GNU_STACK` program header and does not call
`pthread_attr_setstacksize`, so it runs on the process stack size supplied by
the host (8 MiB on a stock Linux/Ubuntu limit). The native CMake server uses
the same default. The Windows `8 MiB`/`4 KiB` reservation described below
exists only because MSVC's default reservation is 1 MiB.

Both Windows engine build systems reserve 8 MiB of stack with a 4 KiB initial commit.
MSVC's binary64 `long double` limits and clang-cl's explicit extended-arithmetic
profile also apply to the server; see [client build limits](client-building.md#windows-with-clang-cl-or-msvc).

## Windows Dedicated Server

The top-level Windows targets build the engine and matching game DLL.
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

## 32-bit Module Notes

The original Linux game module is i386. Building the 32-bit shared module
requires a Linux toolchain with i386 support.

## Build Outputs

Build products are intentionally ignored. Clean them with ordinary non-forced
file deletion or by removing the generated build directories manually.
