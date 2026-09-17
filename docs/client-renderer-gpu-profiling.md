# Renderer GPU profiling

The client has an opt-in OpenGL GPU-timing and renderer-batch diagnostic. It
is excluded from normal builds and must be enabled with the compiler gate
`CODUOMP_RENDERER_GPU_PROFILE=1`.

Use a dedicated build directory. Make does not track build-variable changes as
object dependencies, so reusing a directory previously built without the flag
can leave a mixture of instrumented and uninstrumented objects.

```sh
make client \
  BUILD_DIR=.workbench/build/gpu-profile \
  CODUOMP_RENDERER_GPU_PROFILE=1
```

The flag is supported by the native client-engine build and the MinGW client
engine builds. Runtime GPU timing additionally requires the OpenGL driver to
expose `GL_ARB_timer_query` or `GL_EXT_timer_query` and the associated query
entry points. The client prints a warning and leaves profiling inactive when
they are unavailable.

## Running a capture

Development captures should use an isolated profile. For example:

```sh
make client-test-run \
  BUILD_DIR=.workbench/build/gpu-profile \
  CODUOMP_RENDERER_GPU_PROFILE=1 \
  CLIENT_DATA_PATH="/absolute/path/to/Call of Duty UO" \
  CLIENT_TEST_WORK_DIR=.workbench/runtime/gpu-profile/work \
  CLIENT_TEST_HOME_PATH=.workbench/runtime/gpu-profile/home \
  CLIENT_ARGS="+set r_gpuProfile 2 +set r_gpuProfileDetail 1"
```

The diagnostic writes `gpu_profile.log` directly below `fs_homepath`. The file
is opened in append mode, so use a fresh isolated home path when a capture
should contain only one run. No records are written to the game console for
each frame.

The runtime controls exist only in an instrumented build:

- `r_gpuProfile 0` disables capture. Pending asynchronous results are drained,
  the current summary is written, and the log is flushed.
- `r_gpuProfile 1` writes individual records only for frames whose measured
  GPU time reaches `r_gpuProfileSlowMsec`. Periodic summaries still include
  every captured frame.
- `r_gpuProfile 2` writes an individual record for every completed frame.
- `r_gpuProfileSlowMsec` sets the mode-1 threshold in milliseconds and defaults
  to `4.0`.
- `r_gpuProfileDetail 0` uses one low-overhead timer scope for the backend
  frame. It is useful for detecting slow GPU frames but cannot attribute time
  to renderer phases.
- `r_gpuProfileDetail 1` times adjacent backend command groups by phase. This
  is the recommended level for ordinary captures and frame-stability work.
- `r_gpuProfileDetail 2` starts a query for each tessellation batch. It enables
  per-material GPU timing but is deliberately invasive and should not be used
  to judge normal FPS or frame stability.

Timer results are polled asynchronously. The render thread does not wait for a
query result; a full query or delayed-frame pool produces dropped samples
instead of a GPU synchronization stall. Check `dropped=0` before treating a
capture as complete.

## Reading the log

The log is line-oriented text. Each line begins with a stable record name and
continues with `key=value` fields, making it suitable for `rg`, `awk`, or a
small parser. A `GPU_PROFILE_LOG` line identifies the format version, and
`GPU_PROFILE_BEGIN`/`GPU_PROFILE_END` delimit capture sessions in the appended
file.

The most useful records are:

- `GPU_PROFILE` reports measured GPU milliseconds by renderer phase.
  `gameplay=1` means the frame submitted world geometry; use it to exclude
  menus and most loading frames. These values are scoped GPU-command time, not
  CPU frame time or an FPS-derived wall-clock duration.
- `GPU_PROFILE_BATCHES` counts logical tessellation submissions by phase.
- `GPU_PROFILE_DRAWS` counts actual OpenGL draw calls and separates portal
  rendering from the main scene.
- `GPU_PROFILE_BATCH_TOP` lists the three materials with the most draw calls.
  Each value is `shader:draw_calls/batches`.
- `GPU_PROFILE_SCENE_TOP` ranks scene materials by draw calls without allowing
  2D font or HUD work to hide them.
- `GPU_PROFILE_ENTITY_TOP` ranks materials whose batches are split by entity
  changes.
- `GPU_PROFILE_BREAKS` counts sort-key fields contributing to batch
  transitions: shader, vertex storage, dynamic light, secondary batch flag,
  and entity. These counters intentionally overlap when several fields change
  at one transition.
- `GPU_PROFILE_BREAK_MASKS` gives mutually exclusive `shader_only`,
  `entity_only`, `shader_entity`, and `other` transition counts.
- `GPU_PROFILE_TOP` is emitted when detail level 2 has per-batch timing and
  lists the three most expensive materials by GPU time.
- `GPU_PROFILE_SUMMARY`, `GPU_PROFILE_CENSUS_SUMMARY`, and
  `GPU_PROFILE_CENSUS_BREAK_MASKS` aggregate up to 250 frames. A partial
  summary is also written when capture stops or the renderer shuts down.

In the census summary, `exact_shader` counts shader transitions whose states
compare as merge-compatible, while `overflow` counts submissions forced by
tessellation capacity rather than a sort-key change. High `shader` counts with
low `exact_shader` counts generally mean that reducing batches requires a real
material or texture-layout change, not merely canonicalizing duplicate shader
objects.

For repeatable comparisons, keep resolution, display, view position, demo,
`r_swapInterval`, `r_finish`, and the detail level fixed. Compare census counts
first; compare timing only between runs made with the same profiling detail
and build configuration.
