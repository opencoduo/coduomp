# Renderer frame profiling

The client has opt-in CPU frame timing, OpenGL GPU timing, and renderer-batch
diagnostics. They are excluded from normal builds and must be enabled with the compiler gate
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
engine builds. Modes 1 and 2 additionally require the OpenGL driver to expose
`GL_ARB_timer_query` or `GL_EXT_timer_query` and the associated query entry
points. The client prints a warning and leaves those modes inactive when the
entry points are unavailable. CPU-only mode 3 does not use OpenGL timer
queries.

## Running a capture

Development captures should use an isolated profile. For example:

```sh
make client-test-run \
  BUILD_DIR=.workbench/build/gpu-profile \
  CODUOMP_RENDERER_GPU_PROFILE=1 \
  CLIENT_DATA_PATH="/absolute/path/to/Call of Duty UO" \
  CLIENT_TEST_WORK_DIR=.workbench/runtime/gpu-profile/work \
  CLIENT_TEST_HOME_PATH=.workbench/runtime/gpu-profile/home \
  CLIENT_ARGS="+set r_gpuProfile 3"
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
- `r_gpuProfile 3` is the low-overhead CPU frame-stability mode. It writes one
  timing record per frame without creating OpenGL timer queries or collecting
  shader, batch, draw-call, or merge census data. Use this mode for p95/p99
  wall-clock analysis and presentation stalls.
- `r_gpuProfileSlowMsec` sets the mode-1 threshold in milliseconds and defaults
  to `4.0`.
- `r_gpuProfileDetail 0` uses one low-overhead timer scope for the backend
  frame. It is useful for detecting slow GPU frames but cannot attribute time
  to renderer phases.
- `r_gpuProfileDetail 1` times adjacent backend command groups by phase in GPU
  modes 1 and 2. Use it only for a short GPU-attribution capture.
- `r_gpuProfileDetail 2` starts a query for each tessellation batch. It enables
  per-material GPU timing but is deliberately invasive and should not be used
  to judge normal FPS or frame stability.

Timer results are polled asynchronously. The render thread does not wait for a
query result; a full query or delayed-frame pool produces dropped samples
instead of a GPU synchronization stall. Check `dropped=0` before treating a
GPU capture as complete.

On Apple's OpenGL implementation, even asynchronous timer-query scopes can
perturb command submission and produce long stalls that do not occur in mode
3. Do not use modes 1 or 2 to judge ordinary frame stability on macOS. First
locate a wall-clock problem with mode 3, then use a short mode-1 or mode-2
capture only when GPU attribution is still necessary.

## Reading the log

The log is line-oriented text. Each line begins with a stable record name and
continues with `key=value` fields, making it suitable for `rg`, `awk`, or a
small parser. A `GPU_PROFILE_LOG` line identifies the format version, and
`GPU_PROFILE_BEGIN`/`GPU_PROFILE_END` delimit capture sessions in the appended
file.

The most useful records are:

- `GPU_PROFILE` reports both CPU fields and, in modes 1 and 2, measured GPU
  milliseconds by renderer phase. `gameplay=1` means the frame submitted world
  geometry; use it to exclude menus and most loading frames. `refdef_time` is
  the demo/game timestamp used to compare the same event across repeated runs.
- `frame_interval_cpu_ms` is the start-to-start interval used for frame-pacing
  percentiles. It includes frame limiting and work after the preceding timing
  record closed. `frame_cpu_ms` measures from early client-frame processing
  through synchronous renderer completion.
- `pre_frontend_cpu_ms`, `frontend_cpu_ms`, and `backend_cpu_ms` split that CPU
  work. `cgame_cpu_ms` and `render_scene_cpu_ms` further isolate cgame frame
  construction and renderer visibility/sorting work. `dpvs_setup_cpu_ms`,
  `model_filter_cpu_ms`, `world_traversal_cpu_ms`, `entities_cpu_ms`, and
  `sort_cpu_ms` divide the repeatable scene-construction work without changing
  visibility behavior. The `brush_entities`, `xmodel_entities`,
  `static_entities`, and `effect_entities` CPU/count fields further attribute
  `R_AddEntitySurfaces` without changing what it submits. The
  `static_cache_build` and `static_lighting` CPU/count fields distinguish
  static-model cache population from steady-state surface submission.
- `finish_cpu_ms` is time blocked in an explicit `glFinish`, while
  `present_cpu_ms` is time inside the platform buffer-swap/presentation call.
  `scene_submit_cpu_ms`, `2d_submit_cpu_ms`, `clear_submit_cpu_ms`, and
  `misc_submit_cpu_ms` divide synchronous backend command execution. These
  submission fields are CPU time, not GPU execution time.
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
and build configuration. Discard loading and the first warm-up frames, and
repeat deterministic demos: a spike at the same `refdef_time` is evidence for
content-dependent work, while presentation spikes that move between timestamps
are usually platform scheduling or drawable back-pressure.
