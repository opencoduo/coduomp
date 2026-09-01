# CD-key location on macOS and Linux

The native macOS and Linux client stores the primary Call of Duty: United
Offensive CD key in a file named `codkey`. The filename is lowercase and has no
extension. It lives directly below the client's writable `fs_homepath`, not in
the retail `main/` or `uo/` data directory.

Do not put a CD key in a command line, configuration file, build variable,
issue report, or log. Enter it through the multiplayer UI when possible. If
migrating an existing `codkey` file, copy the complete file without printing
its contents.

## Default locations

### macOS

```text
~/Library/Application Support/OpenCoDUO/codkey
```

### Linux

When `XDG_DATA_HOME` is set to an absolute path:

```text
$XDG_DATA_HOME/opencoduo/codkey
```

Otherwise the client uses:

```text
~/.local/share/opencoduo/codkey
```

## Custom `fs_homepath`

An explicit startup setting overrides the default:

```sh
./CoDUOMP +set fs_homepath "/absolute/path/to/profile"
```

The primary key then belongs at:

```text
/absolute/path/to/profile/codkey
```

This also applies when `CLIENT_HOME_PATH` is supplied to `make client-run`.
That target requires the selected home path to remain below the repository's
`.workbench/runtime/` directory.

## File contents

The client reads a 20-byte payload at the beginning of `codkey`: 16 key bytes
followed by four checksum bytes. Text written after those bytes is ignored
while reading. A file containing only the printed 16-character key is
incomplete and will not validate; use the multiplayer UI or copy the complete
file from an existing installation.

The file contains private credential material. Restrict its permissions to the
owning account after copying it:

```sh
chmod 600 "/absolute/path/to/codkey"
```

## `UO_CODKEY` environment variable

On macOS and Linux, the primary key can instead be supplied through
`UO_CODKEY`. Its value must be exactly the same 20-character payload used at
the beginning of `codkey`: 16 key characters followed immediately by the
four-character checksum, with no spaces, hyphens, or other separators.

```sh
UO_CODKEY="<20-character-key-and-checksum>" ./CoDUOMP
```

A valid `UO_CODKEY` takes precedence over `<fs_homepath>/codkey` for that
process. An unset, empty, malformed, or invalid value falls back to the file.
The environment value is never printed or copied into `codkey`. Entering a new
key through the multiplayer UI still writes the normal file, but a valid
`UO_CODKEY` will take precedence again the next time the client starts.

Environment variables may be inherited by child processes and can be exposed
by shell history, process inspection, crash collection, or service
configuration. The permission-restricted `codkey` file is the safer default
for ordinary desktop use.

## Mod-specific keys

The primary UO key always remains directly below `fs_homepath`. If an
`fs_game` mod requests its own unique key, that independent key is stored at:

```text
<fs_homepath>/<fs_game>/codkey
```

That mod-specific file is not a replacement for the primary
`<fs_homepath>/codkey`.
