# Building

The public tree contains two independently useful target families:

- `build-mk/server.mk` builds the standalone server from
  `src/server/standalone/`, `src/server/engine/`, and the common engine source.
- `build-mk/game.mk` builds the server game module from `src/server/game/` and
  its shared BG/qcommon/math dependencies.

## Requirements

- `make`
- C and C++ compilers such as `cc`, `c++`, `clang`, or `gcc`
- `ar`
- For Windows cross-builds, a 32-bit MinGW compiler such as
  `i686-w64-mingw32-gcc` plus its matching `g++`
- MinGW-compatible zlib headers and library for the Windows engine

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
make game32
```

Output:

```text
build/game/linux-i386/game.mp.uo.i386.so
```

This target uses `-m32`, so it requires a Linux toolchain with 32-bit compiler
and library support installed.

For local shared-object experiments using the host toolchain:

```sh
make game64
```

Output:

```text
build/game/linux-x86_64/game.mp.uo.x86_64.so
```

For a local dedicated-engine shared-object link check, use the target Makefile:

```sh
make -f build-mk/server.mk shared-check-link CODUO_FP_FAITHFUL=relaxed
```

For a native dedicated-server executable:

```sh
make engine64
```

Output:

```text
build/server/linux-x86_64-auth/coduo_lnxded_recovered
```

## Windows Dedicated Server

The top-level Windows targets build a PE32 console engine and its matching PE32
server game DLL:

```sh
make windows-i386
make windows-i686
```

Outputs:

```text
build/server/windows-i386/coduo_dedicated_recovered.exe
build/game/windows-i386/uo_game_mp_x86.dll
build/server/windows-i686/coduo_dedicated_recovered.exe
build/game/windows-i686/uo_game_mp_x86.dll
```

Both targets use the 32-bit MinGW ABI and force `-O0 -mfpmath=387
-fexcess-precision=fast` in both engine and module C/C++ compilation. The
Windows engine establishes the retail-compatible 53-bit x87 precision mode at
startup and restores it after loading the game DLL. The distinction between
the targets is the generated instruction baseline: `-march=i386` or
`-march=i686`.

The engine needs MinGW zlib. If it is not installed in the compiler's default
search path, point `WINDOWS_DEP_PREFIX` at a prefix containing `include/zlib.h`
and `lib/libz.a`:

```sh
make windows-i686 WINDOWS_DEP_PREFIX=/opt/mingw-i686
```

Override the compiler names when necessary:

```sh
make windows-i386 \
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
