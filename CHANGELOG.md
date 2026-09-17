# Changelog

## Changes from stock

This is a feature-oriented summary of lasting, user-visible differences from the stock Call of Duty: United Offensive 1.51 multiplayer client and dedicated server. Silent security hardening, source-recovery work, build plumbing, and fixes that only restore stock behavior are intentionally omitted.

### Client

- Suppresses duplicate local-player firing effects caused by prediction replay, including recoil, muzzle flashes, shot sounds and brass; the same filter covers replayed reload and rechamber sounds.
- Spawn reload animation is fixed, it won't play anymore
- Local server map list limit increased to 2048
- Sound Options has independent Effects Volume and Music Volume sliders. The existing `mss_volume` cvar controls effects/SFX, and the new `musicVolume` cvar controls music; both use a 0-1 range, with 0 muting that category. There is no separate `sfxVolume` cvar. The retired `playMusic` switch is replaced by the music slider; existing effects-volume and music-mute preferences seed the first independent music setting.

### Server browser

- Server and map names are fitted to their columns by rendered pixel width, preventing wide or colored text from overlapping neighboring fields.
- Server refreshes distinguish human players from bots reported with the conventional 999 ping. The player column shows the human count against the server capacity and appends the bot count in subdued text. Learned bot counts persist across client launches and remain stable while each refresh obtains a current roster; only a new valid roster replaces the cached count.
- Server-name sorting follows visible names, ignoring color escapes and ordering digits consistently. Final roster updates preserve the selected row and viewport while applying deterministic tie-breaking.
- Favorites retain their last known or user-supplied hostname when a refresh times out instead of falling back to the numeric address.

### Mods/Server

- Attached models reload correctly when detached
- Per-map `<map>_<gametype>.cfg` files load by the visible map name even when the map asset name contains color escapes.
- **Server configuration:** add `set g_allowGlobalChat 0` to the server config or enter it in the server console to disable player general chat while preserving team, squad, private, scripted, and server-console messages. Set it to `1` to enable general chat again; `1` is the default.
- Dedicated and listen servers skip external CD-key authorization by default.

### Platforms and display

- Extends client support to macOS and Linux.
- The graphics menu adds a Current Display option and filters modern presets through 3840 x 2160 to resolutions reported by the selected display.
- The Monitor selector chooses the display used for window placement, fullscreen presentation, and resolution discovery on Windows, macOS, and Linux. Its archived `r_display` cvar uses a zero-based index (default `0`); apply the graphics settings or run `vid_restart` after changing it. An unavailable selection falls back to the first display.
- `r_fullscreen` selects Windowed (`0`), Fullscreen (`1`, the default), or Borderless (`2`). Fullscreen keeps the OS display mode and other monitors active. On macOS and Linux, Fullscreen enters desktop fullscreen while Borderless remains an undecorated desktop-sized window.
- Widescreen support covers the gameplay FOV, menus, HUD, reticles, optical overlays, and full-screen effects; classic fitted 4:3 remains selectable.
- The Graphics page has an 80-120 horizontal field-of-view slider below Brightness, with its current value beside the meter. `cg_fov` is the displayed view angle directly. Changing the resolution or Fill Screen/Classic 4:3 presentation resets it to the aspect-aware default only when the effective aspect ratio changes; right-clicking the slider restores that same default.
- Startup initializes FOV from the active display aspect (about 96.42 degrees at 16:9), upgrades the legacy 80-degree default once, and preserves custom FOV across map loads, server mod-config reloads, and servers with cheats disabled.
- Mods that replace the HUD menu file (e.g. Reign of the Undead) are detected and render their HUD and in-game menu screens with the stock full-width presentation so their authored layout stays intact, while the world keeps the widescreen FOV; `cg_modHudPresentation` can force stretched, centered, or anchored HUD presentation.
- Retina and other high-DPI displays are supported. macOS fullscreen modes avoid oversized drawables caused by scaled Retina backing surfaces.
- Alt-Tab from fullscreen is enabled by default on Windows.
- New Apple Silicon profiles default to 2560 x 1440, high graphics settings, and a 250 FPS cap.
- Apple Silicon defaults to streamed, interleaved vertex uploads (`r_vbo_stream_draw 1` and `r_vbo_interleave 1`); these saved settings take effect on renderer restart. New Apple Silicon profiles also set `r_finish 0` to avoid forcing GPU synchronization each frame.
- 64-bit clients require at least 256 MiB of hunk memory (`com_hunkMegs`), including when an older saved setting requested less.

### Servers, mods, and configuration

- The **Start New Server** menu's local catalog now supports up to 2,048 maps, replacing stock's 128-map table, 64-arena staging limit, and 1 KiB filename list so large custom-map installs no longer hide later maps, including stock maps. Server broadcast protocol limits remain unchanged.
- Connecting no longer fails with the common `MAX_GAMESTATE_CHARS exceeded` error: the client's retained configstring pool grows from 20,480 to 32,768 bytes, enough for any gamestate a protocol-22 server can physically deliver (a gamestate must fit one 32,768-byte network message, which spends more bytes per configstring than the pool does). The same enlarged pool bounds configstring updates during play, and a server that composes an oversized gamestate logs a clear console warning while behaving exactly as before.
- Downloads are enabled by default unless the user has explicitly disabled them.
- Server Cache isolates each remote server's settings and downloaded content so servers cannot overwrite one another's same-named PK3s.
- Server mod reset independently restores the process-start `fs_game` after disconnecting, including when Server Cache is disabled.
- Cached server mods appear in the Mods list, and checksum-matching PK3s from ordinary game roots or other server caches are reused instead of downloaded again. Cache matches may have different filenames or mod directories; the source server's configuration and unrelated content remain isolated.
- Server Cache can be disabled under **Options -> System -> Advanced**.
- `promoteserverconfig` saves the current isolated server profile as the global configuration.
- `clearserverconfigs` clears all per-server configurations while preserving downloaded content.
- Server Cache uses `server-cache/<server-name>/` as the sole namespace boundary, with ordinary relative filesystem paths directly below it and no version, endpoint hash, or separate content/state trees.
- New profiles default to `snaps 30`, `cl_maxpackets 125`, and `rate 30000` instead of the retail `20`, `30`, and `25000` values.

### Demo recording and playback

- **Record Demo** under **Options -> Miscellaneous** can bind one or two keys to start an auto-named demo and stop the active recording. The same action is available as the bindable `togglerecord` console command. An on-screen indicator shows recording size; `cg_showdemoname 1` also shows the demo name.
- `listdemos [mod_folder]` lists available recordings using command-ready demo, mod-folder, and optional server names. `playdemo <demo_name> [mod_folder] [server_name]` loads top-level, ordinary-mod, or server-scoped recordings with the required mod and map content.
- Demo playback provides pause/play (**Space**), five-second rewind/forward (**Left/Right**), paused previous/next snapshot stepping (**Comma/Period**), and a 0.125x/0.25x/0.5x/1x/2x/4x/8x speed cycle traversed forward with **F** and backward with **D**.
- These playback actions can also be bound through `demopause`, `demorewind`, `demoforward`, `demoframeback`, `demoframestep`, `demofastforward`, and `demospeeddown`. `cl_demoPlaybackSpeed` sets the demo-only multiplier, clamped to 0.125-8 without changing global `timescale`; it is not archived and resets to `1` for each playback session.
- The on-screen playback tools show the current time, duration, speed, control legend, and a mouse-draggable timeline. Set the archived `cl_demoControlOverlay` cvar to `0` to hide them or `1` to show them.
- The decoded timeline supports in-place backward seeking without reloading the map. Recorded playback suppresses live-network lag, connection-interruption, and timeout behavior.
- See [Demo Playback Controls](docs/client-demo-playback.md) for command and cvar details.

### Console and input

- Windows mouse input uses Raw Input by default, bypassing the system's **Enhance pointer precision** acceleration without changing the Windows setting; **Raw Input Active** under **Options -> Advanced** can disable it. The archived `in_rawInput` cvar selects this path (`1`, the default) or legacy mouse movement (`0`) and takes effect immediately; the read-only `in_rawInputActive` cvar reports whether Raw Input is actually available.
- The console key can be rebound from the options menu, including rebinding the backtick key itself.
- Native Linux/macOS clients support Ctrl-V or Command-V clipboard paste.
- Ctrl-W deletes the previous console word and treats underscores as word delimiters.
- Console scrollback is four times larger.

### Optional renderer profiling

- Custom builds made with `CODUOMP_RENDERER_GPU_PROFILE=1` expose GPU timing controls. These diagnostics and cvars are excluded from normal release builds.
- `r_gpuProfile` selects off (`0`, the default), slow-frame logging (`1`), or every-frame logging (`2`). `r_gpuProfileSlowMsec` sets the slow-frame threshold in milliseconds (default `4.0`).
- `r_gpuProfileDetail` selects whole-frame timing (`0`, the default), command-phase timing (`1`), or invasive per-batch timing (`2`). Profiling controls are temporary and are not saved to the client configuration.
