# CoD:UO Dedicated Engine

This directory contains maintained source for the Call of Duty: United Offensive
dedicated-server engine. It covers server-side runtime systems such as
commands, cvars, filesystem access, networking, collision, animation/model
services, scripting support, and game-module host calls.

The exported tree is organized as normal buildable C/C++ source:

- `src/server/standalone/`: process and dedicated-platform code.
- `src/server/engine/`: server engine code shared with an embedded listen
  server.
- common subsystems live directly below `src/`.
- `build-mk/server.mk`: compile and link targets for the maintained engine.

## Build

```sh
make -f build-mk/server.mk objects CODUO_FP_FAITHFUL=relaxed
make -f build-mk/server.mk shared-check-link CODUO_FP_FAITHFUL=relaxed
make windows-i386 WINDOWS_DEP_PREFIX=/opt/mingw-i686
make windows-i686 WINDOWS_DEP_PREFIX=/opt/mingw-i686
```

The default `check` target compiles the maintained source into a static archive.
The shared link check permits unresolved engine-internal symbols while more
subsystems are completed.

The original Linux server is an i386 program. Build targets that use `-m32`
require a Linux toolchain with 32-bit compiler and library support installed.

The Windows targets use 32-bit MinGW and produce the engine under
`build/server/windows-i386/` or `build/server/windows-i686/`. They require a matching
MinGW zlib; use `WINDOWS_DEP_PREFIX` when it is outside the compiler's default
search path. The executable is a console application and loads
`uo/uo_game_mp_x86.dll`.

## Game-type voting

The improved game module accepts a whitespace-separated `g_voteGameTypes`
allowlist. For example:

```text
set g_voteGameTypes "tdm dm ctf dom"
```

The list applies to both `callvote g_gametype` and `callvote typemap`, with
case-insensitive, whole-name matching. An empty value (the default) allows
every installed game type. Changes take effect without restarting the map.
The ordinary `g_allowVoteGameType` and `g_allowVoteTypeMap` switches still
control whether those vote commands are enabled. Direct administrator
commands and configured rotations are unaffected. Client menus can still
display installed game types that the server will reject for voting.
