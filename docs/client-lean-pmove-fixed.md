# Lean Prediction with `pmove_fixed`

This note describes a `master` product fix. `stock` intentionally keeps the
original behavior.

## Symptom

With `pmove_fixed` enabled, holding `+leanleft` or `+leanright` (default Q and E)
made the first-person camera judder while leaning. The motion looked like a low
frame rate or repeated frame jumps even when frame times were stable. Setting
`pmove_fixed` back to `0` removed the effect.

## Root cause

`CG_PredictPlayerState_Internal` rebuilds `cg_predictedPlayerState` from
`cg_nextSnap->ps` and then replays buffered usercmds through `Pmove`. When
`pmove_fixed` is nonzero it first runs a pre-pass over the entire usercmd ring:

```text
for each buffered command:
    if pmove_fixed:
        PM_UpdateViewAngles(...)
    if the command is old or ahead of the latest command:
        continue
    Pmove(...)
```

`PM_UpdateViewAngles` refreshes the absolute command view angles, but it also
calls `PM_UpdateLean`. Lean is incremental and uses the shared `pml.msec` left by
the previous `PmoveSingle`. The pre-pass therefore applied a lean step to every
buffered command, including commands that had already been simulated and
commands that `Pmove` would skip. `Pmove` then advanced lean again for the
commands it actually simulated.

Per rendered frame this could apply up to the full command history worth of lean.
The server only advances lean for the commands it simulates, so the client ran
ahead. Each incoming snapshot reset `cg_predictedPlayerState.leanFraction` to
the server value, and the camera snapped back. The resulting sawtooth in the
view origin was the visible judder.

## Fix

- `PM_UpdateViewAngles` keeps its original signature and behavior.
- Its shared body is an internal helper with an `updateLean` switch.
- `coduomp_pm_update_view_angles_without_lean` runs the same view-angle,
  stance, ladder, vehicle, and prone updates without advancing lean.
- The `pmove_fixed` prediction pre-pass uses the no-lean entry point. Lean
  advances only inside the `Pmove` calls that simulate commands.

This preserves the pre-pass's purpose, keeping absolute view angles current even
when no command is simulated, without double-counting the incremental lean
state.

Changed files:

- `src/bg/bg_update_view_angles.c`
- `src/bg/bg_pmove.h`
- `src/client/cgame/prediction/cg_predictplayerstate_internal.c`

## Minimal test

A quick in-game test on any server:

1. Join a server.
2. Open the console and set:
   ```text
   set pmove_fixed 1
   set pmove_msec 8
   ```
3. Hold E (`+leanright`) for about a second, release, then repeat with Q
   (`+leanleft`).
4. Before the fix the camera judders while leaning. After the fix it moves
   smoothly.
5. Use `set pmove_fixed 0` as the control case. It is smooth before and after
   the fix.

To test without using the persistent system profile, start an isolated client
with the Make test launcher:

```sh
make client-test-run CLIENT_DATA_PATH=/path/to/retail/root
```

Then in the client console, set `pmove_fixed 1` and `pmove_msec 8`, start a
local server with `map mp_chateau`, choose a team, and hold the lean keys. If
the local server is already running, `connect localhost` joins it.

For a Windows CMake build, launch the built client from an isolated work
directory with `fs_basepath`, `fs_cdpath`, and `fs_homepath` set before the
server-start command, then apply the same console settings. Do not use the
persistent system profile.

### Numeric check

To verify prediction rather than appearance, temporarily log
`cg_predictedPlayerState.leanFraction` once per rendered frame while lean is
held. After the fix the value moves monotonically toward the held direction
between release events. It must not step back toward zero when a new snapshot
arrives. The test log used for this fix showed no backward steps during a
continuous hold.

## Validation

- Built the 32-bit and 64-bit MSVC client and game targets.
- `tools/check-source-policy.sh` passed.
- Runtime reproduction with `pmove_fixed 1`: judder before, smooth after;
  `pmove_fixed 0` behavior unchanged.
