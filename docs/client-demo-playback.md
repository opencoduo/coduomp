# Demo Playback Controls

On `master`, play a top-level demo with:

```text
playdemo <demo_name>
```

For a demo recorded with mod content, add its mod folder. Add the server name
only when more than one cached server has that mod folder:

```text
playdemo <demo_name> <mod_folder>
playdemo <demo_name> <mod_folder> <server_name>
```

`listdemos [mod_folder]` prints the arguments needed to play each available
demo.

During playback, **Space** pauses or resumes, **Left/Right** seeks five seconds,
**.** advances one snapshot while paused, and **F** cycles through 1x, 2x, 4x,
and 8x playback. The on-screen controls also provide a mouse-draggable timeline.

The same actions are available as bindable console commands:

| Command | Action |
| --- | --- |
| `demopause` | Pause or resume playback. |
| `demorewind` | Seek backward five seconds. |
| `demoforward` | Seek forward five seconds. |
| `demoframestep` | Advance one recorded snapshot and remain paused. |
| `demofastforward` | Cycle the playback speed through 1x, 2x, 4x, and 8x. |

## Scrubber overlay

`cl_demoControlOverlay` controls the on-screen playback legend and timeline:

| Value | Behavior |
| --- | --- |
| `1` | Show the playback controls and enable mouse scrubbing. This is the default. |
| `0` | Hide the playback controls and disable mouse scrubbing. Keyboard and console playback controls remain available. |

The setting is archived in the client profile. To hide the controls and save
the choice:

```text
seta cl_demoControlOverlay 0
```

To show them again:

```text
seta cl_demoControlOverlay 1
```
