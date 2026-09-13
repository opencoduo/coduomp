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

`listdemos [mod_folder]` prints the command needed to play each available demo.
The displayed demo name omits the `.dm_3` file suffix because `playdemo` adds
it automatically.

During playback, **Space** pauses or resumes, **Left/Right** seeks five seconds,
**Comma/Period** steps to the previous or next snapshot and remains paused, and
**F/D** moves forward or backward through the 0.125x, 0.25x, 0.5x, 1x, 2x, 4x,
and 8x speed cycle. Playback starts at 1x. The on-screen controls also provide a
mouse-draggable timeline. Network lag and connection-interruption indicators
are hidden during demo playback because recorded snapshots do not represent a
live connection.

The same actions are available as bindable console commands:

| Command | Action |
| --- | --- |
| `demopause` | Pause or resume playback. |
| `demorewind` | Seek backward five seconds. |
| `demoforward` | Seek forward five seconds. |
| `demoframeback` | Move to the previous recorded snapshot and remain paused. |
| `demoframestep` | Advance one recorded snapshot and remain paused. |
| `demospeeddown` | Move backward through the playback-speed cycle. |
| `demofastforward` | Move forward through the playback-speed cycle. |

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
