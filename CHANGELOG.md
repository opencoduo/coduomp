# Changelog

## Changes from stock

This is a feature-oriented summary of lasting, user-visible differences from the stock Call of Duty: United Offensive 1.51 multiplayer client and dedicated server. Silent security hardening, source-recovery work, build plumbing, and fixes that only restore stock behavior are intentionally omitted.

### Client

- Suppresses duplicate local-player firing effects caused by prediction replay, including recoil, muzzle flashes, shot sounds and brass; the same filter covers replayed reload and rechamber sounds.
- Spawn reload animation is fixed, it won't play anymore
- Local server map list limit increased to 2048
- Sound Options has independent Effects Volume and Music Volume sliders. The existing `mss_volume` cvar controls effects/SFX, and the new `musicVolume` cvar controls music; there is no separate `sfxVolume` cvar.

### Server browser

- Server and map names are fitted to their columns by rendered pixel width, preventing wide or colored text from overlapping neighboring fields.
- Server refreshes distinguish human players from bots reported with the conventional 999 ping. The player column shows the human count against the server capacity and appends the bot count in subdued text; remembered bot counts remain stable while later refresh responses arrive.
- Server-name sorting follows visible names, ignoring color escapes and ordering digits consistently. Final roster updates preserve the selected row and viewport while applying deterministic tie-breaking.
- Favorites retain their last known or user-supplied hostname when a refresh times out instead of falling back to the numeric address.

### Mods/Server

- Attached models reload correctly when detached
- Per-map `<map>_<gametype>.cfg` files load by the visible map name even when the map asset name contains color escapes.
- Servers can set `g_allowGlobalChat 0` to disable player general chat while preserving team, squad, private, scripted, and server-console messages. General chat remains enabled by default.

### Platforms and display

- Extends client support to macOS and Linux.
- The graphics menu adds a Current Display option and filters modern presets through 3840 x 2160 to resolutions reported by the primary display.
- Windowed, exclusive fullscreen, and borderless desktop modes are available.
- Widescreen support covers the gameplay FOV, menus, HUD, reticles, optical overlays, and full-screen effects; classic fitted 4:3 remains selectable.
- The Graphics page has an 80-120 horizontal field-of-view slider below Brightness, with its current value beside the meter. `cg_fov` is the displayed view angle directly. Changing the resolution or Fill Screen/Classic 4:3 presentation resets it to the aspect-aware default only when the effective aspect ratio changes; right-clicking the slider restores that same default.
- Mods that replace the HUD menu file (e.g. Reign of the Undead) are detected and render their HUD and in-game menu screens with the stock full-width presentation so their authored layout stays intact, while the world keeps the widescreen FOV; `cg_modHudPresentation` can force stretched, centered, or anchored HUD presentation.
- Retina and other high-DPI displays are supported.
- Alt-Tab from fullscreen is enabled by default on Windows.
- New Apple Silicon profiles default to 2560 x 1440, high graphics settings, and a 250 FPS cap.

### Servers, mods, and configuration

- The **Start New Server** menu's local catalog now supports up to 2,048 maps, replacing stock's 128-map table, 64-arena staging limit, and 1 KiB filename list so large custom-map installs no longer hide later maps, including stock maps. Server broadcast protocol limits remain unchanged.
- Connecting no longer fails with the common `MAX_GAMESTATE_CHARS exceeded` error: the client's retained configstring pool grows from 20,480 to 32,768 bytes, enough for any gamestate a protocol-22 server can physically deliver (a gamestate must fit one 32,768-byte network message, which spends more bytes per configstring than the pool does). The same enlarged pool bounds configstring updates during play, and a server that composes an oversized gamestate logs a clear console warning while behaving exactly as before.
- Downloads are enabled by default unless the user has explicitly disabled them.
- Server Cache isolates each remote server's settings and downloaded content so servers cannot overwrite one another's same-named PK3s.
- Server mod reset independently restores the process-start `fs_game` after disconnecting, including when Server Cache is disabled.
- Cached server mods appear in the Mods list, and checksum-matching installed PK3s are reused instead of downloaded again.
- Server Cache can be disabled under **Options -> System -> Advanced**.
- `promoteserverconfig` saves the current isolated server profile as the global configuration.
- `clearserverconfigs` clears all per-server configurations while preserving downloaded content.
- Server Cache uses `server-cache/<server-name>/` as the sole namespace boundary, with ordinary relative filesystem paths directly below it and no version, endpoint hash, or separate content/state trees.
- New profiles default to `snaps 30`, `cl_maxpackets 125`, and `rate 30000` instead of the retail `20`, `30`, and `25000` values.

### Demo recording and playback

- **Record Demo** under **Options -> Miscellaneous** can bind one or two keys to start an auto-named demo and stop the active recording.
- `listdemos [mod_folder]` lists available recordings using command-ready demo, mod-folder, and optional server names. `playdemo <demo_name> [mod_folder] [server_name]` loads top-level, ordinary-mod, or server-scoped recordings with the required mod and map content.
- Demo playback provides pause/play (**Space**), five-second rewind/forward (**Left/Right**), paused previous/next snapshot stepping (**Comma/Period**), and a 0.125x/0.25x/0.5x/1x/2x/4x/8x speed cycle traversed forward with **F** and backward with **D**.
- The on-screen playback tools show the current time, duration, speed, control legend, and a mouse-draggable timeline. Set the archived `cl_demoControlOverlay` cvar to `0` to hide them or `1` to show them.
- The decoded timeline supports in-place backward seeking without reloading the map. Recorded playback suppresses live-network lag, connection-interruption, and timeout behavior.
- See [Demo Playback Controls](docs/client-demo-playback.md) for command and cvar details.

### Console and input

- Windows mouse input uses Raw Input by default, bypassing the system's **Enhance pointer precision** acceleration without changing the Windows setting; **Raw Input Active** under **Options -> Advanced** can disable it.
- The console key can be rebound from the options menu, including rebinding the backtick key itself.
- Native Linux/macOS clients support Ctrl-V or Command-V clipboard paste.
- Ctrl-W deletes the previous console word and treats underscores as word delimiters.
- Console scrollback is four times larger.
