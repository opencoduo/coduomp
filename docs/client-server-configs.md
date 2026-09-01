# Per-server Client Configuration

The default CoDUOMP client isolates configuration changes made while connected
to an eligible remote server. This prevents a server-specific name, sensitivity,
binding, mod setting, or downloaded file from silently changing the normal
client profile or another server's files.

For each server endpoint, the client uses a directory below `fs_homepath` with
this layout:

```text
server-cache/v1/<server-name>-<endpoint-id>/
  state/<game>/uoconfig_mp.cfg
  content/<game>/...
```

`state` contains that server's configuration. `content` contains files
downloaded from that server or reused from an ordinary game root. On a normal
disconnect, the client restores the global configuration that was active
before the connection.

Before downloading a referenced PK3, the client compares the server's expected
checksum with PK3s already installed in the ordinary game roots. A matching
non-official PK3 is copied into the active server's `content` directory and
used after the same filesystem restart that would load a download. Official
PK3s remain mounted in their canonical `main` or `uo` game directory so a
server alias cannot change their stock search priority. A copied PK3 is staged
under a temporary name; a failed copy is discarded and the normal download
path remains available.

The main-screen Mods list also includes cached non-basegame directories that
contain PK3s. Their labels use `server-name/mod`, while their launch paths
remain scoped to the corresponding `server-cache/v1` content directory.

Server Cache is enabled by default. It can be changed from **Options → System
→ Advanced**. Disabling it prevents new connections from activating an
isolated server namespace and hides cached server mods from the Mods list;
existing cache files remain on disk.

These paths and commands are an improved CoDUOMP compatibility feature. They
are not available in the stock source line.

## Keep the Current Server Settings Globally

After changing settings while connected to a server, enter this command in the
client console:

```text
promoteserverconfig
```

The command writes the current bindings and all archived cvars to the global
`uoconfig_mp.cfg` for the game directory that was active before the connection.
It also updates the current connection's restore point, so the promoted values
remain active after disconnecting or switching directly to another server.
For example, `sensitivity` is archived, so its current value is promoted and
becomes the mouse sensitivity used after leaving the server.

Promotion replaces the complete generated global `uoconfig_mp.cfg`; it is not a
selective merge of only the most recently changed setting. Run it only when the
current server profile contains everything that should become the new global
default.

The command takes no arguments and requires an active isolated server
connection. Otherwise the console reports:

```text
No isolated server configuration is active.
```

## Clear Every Per-server Configuration

From the main menu, after disconnecting from a server, enter:

```text
clearserverconfigs
```

The command removes every `.cfg` file below the `state` directories for all
isolated servers. It preserves:

- the global `uoconfig_mp.cfg`;
- downloaded server files below each `content` directory; and
- the server-cache directory structure.

It does not alter settings already loaded in memory. Running it while connected
does remove the files immediately, but the active server can create its config
again during a later automatic write or normal disconnect. Run it from the main
menu when the intent is to leave every server profile cleared.

The console reports how many files were removed and separately reports any
files it could not remove.
