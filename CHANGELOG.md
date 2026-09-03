# Changelog

## Changes from stock

This is a feature-oriented summary of lasting, user-visible differences from the stock Call of Duty: United Offensive 1.51 multiplayer client and dedicated server. Silent security hardening, source-recovery work, build plumbing, and fixes that only restore stock behavior are intentionally omitted.

### Client

- Spawn reload animation is fixed, it won't play anymore
- Local server map list limit increased to 2048

### Server browser

- Server refreshes distinguish human players from bots reported with the conventional 999 ping. The player column shows the human count against the server capacity and appends the bot count in subdued text; remembered bot counts remain stable while later refresh responses arrive.
- Server-name sorting follows visible names, ignoring color escapes and ordering digits consistently. Final roster updates preserve the selected row and viewport while applying deterministic tie-breaking.
- Favorites retain their last known or user-supplied hostname when a refresh times out instead of falling back to the numeric address.

### Mods/Server

- Attached models reload correctly when detached

### Platforms and display

- Extends client support to macOS and Linux.
- The graphics menu adds a Current Display option and filters modern presets through 3840 x 2160 to resolutions reported by the primary display.
- Windowed, exclusive fullscreen, and borderless desktop modes are available.
- Widescreen support covers the gameplay FOV, menus, HUD, reticles, optical overlays, and full-screen effects; classic fitted 4:3 remains selectable.
- Mods that replace the HUD menu file (e.g. Reign of the Undead) are detected and render their HUD with the stock full-width presentation so their authored layout stays intact, while the world keeps the widescreen FOV; `cg_modHudPresentation` can force stretched, centered, or anchored HUD presentation.
- Retina and other high-DPI displays are supported.
- Alt-Tab from fullscreen is enabled by default on Windows.
- New Apple Silicon profiles default to 2560 x 1440, high graphics settings, and a 250 FPS cap.

### Servers, mods, and configuration

- The **Start New Server** menu's local catalog now supports up to 2,048 maps, replacing stock's 128-map table, 64-arena staging limit, and 1 KiB filename list so large custom-map installs no longer hide later maps, including stock maps. Server broadcast protocol limits remain unchanged.
- `MAX_GAMESTATE_CHARS` is increased from 20,480 to 32,768 bytes, allowing larger custom-map and mod gamestates on existing protocol-22 servers.
- Downloads are enabled by default unless the user has explicitly disabled them.
- Server Cache isolates each remote server's settings and downloaded content so servers cannot overwrite one another's same-named PK3s.
- Server mod reset independently restores the process-start `fs_game` after disconnecting, including when Server Cache is disabled.
- Cached server mods appear in the Mods list, and checksum-matching installed PK3s are reused instead of downloaded again.
- Server Cache can be disabled under **Options -> System -> Advanced**.
- `promoteserverconfig` saves the current isolated server profile as the global configuration.
- `clearserverconfigs` clears all per-server configurations while preserving downloaded content.
- New profiles default to `snaps 30`, `cl_maxpackets 125`, and `rate 30000` instead of the retail `20`, `30`, and `25000` values.

### Console and input

- The console key can be rebound from the options menu, including rebinding the backtick key itself.
- Native Linux/macOS clients support Ctrl-V or Command-V clipboard paste.
- Ctrl-W deletes the previous console word and treats underscores as word delimiters.
- Console scrollback is four times larger.
