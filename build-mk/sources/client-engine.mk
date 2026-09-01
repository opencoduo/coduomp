include build-mk/sources/common.mk

CLIENT_ENGINE_C_SOURCES := \
	src/client/engine/animation/dobj_core.c \
	src/client/engine/animation/dobj_matrix.c \
	src/client/engine/animation/xanim_globals.c \
	src/client/engine/botlib/libvar.c \
	src/client/engine/botlib/log.c \
	src/client/engine/botlib/memory.c \
	src/client/engine/client/cgame_frame.c \
	src/client/engine/client/cgame_init.c \
	src/client/engine/client/cgame_syscalls.c \
	src/client/engine/client/cinematic.c \
	src/client/engine/client/client_lifecycle.c \
	src/client/engine/client/client_netchan.c \
	src/client/engine/client/command_completion.c \
	src/client/engine/client/connectionless.c \
	src/client/engine/client/console_animation.c \
	src/client/engine/client/console_clear.c \
	src/client/engine/client/console_commands.c \
	src/client/engine/client/console_draw_messages.c \
	src/client/engine/client/console_draw_solid.c \
	src/client/engine/client/console_message_window.c \
	src/client/engine/client/console_print.c \
	src/client/engine/client/console_resize.c \
	src/client/engine/client/console_scroll.c \
	src/client/engine/client/debug_lines.c \
	src/client/engine/client/demo_recording.c \
	src/client/engine/client/dobj_client.c \
	src/client/engine/client/download_protocol.c \
	src/client/engine/client/download.c \
	src/client/engine/client/field_input.c \
	src/client/engine/client/key_input.c \
	src/client/engine/client/reliable_commands.c \
	src/client/engine/client/screen.c \
	src/client/engine/client/server_browser.c \
	src/client/engine/client/server_commands.c \
	src/client/engine/client/server_message_entities.c \
	src/client/engine/client/snapshots.c \
	src/client/engine/client/statmon.c \
	src/client/engine/com_error.c \
	src/client/engine/com_frame.c \
	src/client/engine/com_globals.c \
	src/client/engine/com_print.c \
	src/client/engine/com_startup.c \
	src/client/engine/com_weapon_memory.c \
	src/client/engine/effects/fx_archive.c \
	src/client/engine/effects/fx_bolt.c \
	src/client/engine/effects/fx_model.c \
	src/client/engine/effects/fx_runtime.c \
	src/client/engine/field_completion.c \
	src/client/engine/filesystem/filesystem_archive_services.c \
	src/client/engine/filesystem/filesystem_cdkey.c \
	src/client/engine/filesystem/filesystem_globals.c \
	src/client/engine/filesystem/server_namespace.c \
	src/client/engine/localization/seh_text.c \
	src/client/engine/math/noise.c \
	src/client/engine/math/vector_math.c \
	src/client/engine/memory/hunk.c \
	src/client/engine/networking/net_address.c \
	src/client/engine/networking/net_channel.c \
	src/client/engine/networking/net_oob_services.c \
	src/client/engine/platform/case_sensitive_fs.c \
	src/client/engine/platform/crt_boundary.c \
	src/client/engine/platform/dynamic_library_boundary.c \
	src/client/engine/platform/floating_point_boundary.c \
	src/client/engine/platform/libwww_curl_boundary.c \
	src/client/engine/platform/minizip_boundary.c \
	src/client/engine/platform/punkbuster_boundary.c \
	src/client/engine/platform/sdl_platform.c \
	src/client/engine/renderer/backend_2d.c \
	src/client/engine/renderer/backend_commands.c \
	src/client/engine/renderer/backend_diagnostics.c \
	src/client/engine/renderer/backend_draw_surfs.c \
	src/client/engine/renderer/backend_entity_surfaces.c \
	src/client/engine/renderer/backend_flares.c \
	src/client/engine/renderer/backend_geometry.c \
	src/client/engine/renderer/backend_lighting.c \
	src/client/engine/renderer/backend_shader_stage.c \
	src/client/engine/renderer/backend_sky.c \
	src/client/engine/renderer/backend_static_model.c \
	src/client/engine/renderer/backend_view.c \
	src/client/engine/renderer/backend_xmodel.c \
	src/client/engine/renderer/cinematic.c \
	src/client/engine/renderer/gl_debug_strings.c \
	src/client/engine/renderer/gl_debug_wrappers.c \
	src/client/engine/renderer/gl_draw.c \
	src/client/engine/renderer/gl_error_controller.c \
	src/client/engine/renderer/gl_error_wrappers.c \
	src/client/engine/renderer/gl_logging_controller.c \
	src/client/engine/renderer/gl_profile.c \
	src/client/engine/renderer/gl_state.c \
	src/client/engine/renderer/platform_wgl.c \
	src/client/engine/renderer/punkbuster_gl.c \
	src/client/engine/renderer/qgl_dispatch.c \
	src/client/engine/renderer/renderer_api.c \
	src/client/engine/renderer/renderer_cull.c \
	src/client/engine/renderer/renderer_debug.c \
	src/client/engine/renderer/renderer_dlights.c \
	src/client/engine/renderer/renderer_dpvs.c \
	src/client/engine/renderer/renderer_draw_surfs.c \
	src/client/engine/renderer/renderer_entity_lighting.c \
	src/client/engine/renderer/renderer_fog.c \
	src/client/engine/renderer/renderer_font.c \
	src/client/engine/renderer/renderer_frame_commands.c \
	src/client/engine/renderer/renderer_freetype.c \
	src/client/engine/renderer/renderer_image.c \
	src/client/engine/renderer/renderer_immediate.c \
	src/client/engine/renderer/renderer_init.c \
	src/client/engine/renderer/renderer_light_visibility.c \
	src/client/engine/renderer/renderer_lightmap.c \
	src/client/engine/renderer/renderer_marks.c \
	src/client/engine/renderer/renderer_model_bounds.c \
	src/client/engine/renderer/renderer_model_load.c \
	src/client/engine/renderer/renderer_model_optimize.c \
	src/client/engine/renderer/renderer_model_refresh.c \
	src/client/engine/renderer/renderer_model_register.c \
	src/client/engine/renderer/renderer_model_texcoords.c \
	src/client/engine/renderer/renderer_orientation.c \
	src/client/engine/renderer/renderer_pick_shader.c \
	src/client/engine/renderer/renderer_register.c \
	src/client/engine/renderer/renderer_registration.c \
	src/client/engine/renderer/renderer_scene.c \
	src/client/engine/renderer/renderer_screenshot.c \
	src/client/engine/renderer/renderer_shader_lifecycle.c \
	src/client/engine/renderer/renderer_shader_parse.c \
	src/client/engine/renderer/renderer_shader_queries.c \
	src/client/engine/renderer/renderer_shadows.c \
	src/client/engine/renderer/renderer_startup.c \
	src/client/engine/renderer/renderer_static_model_build.c \
	src/client/engine/renderer/renderer_static_model_cache.c \
	src/client/engine/renderer/renderer_static_model_cells.c \
	src/client/engine/renderer/renderer_static_model_draw.c \
	src/client/engine/renderer/renderer_static_model_optimize.c \
	src/client/engine/renderer/renderer_static_model_register.c \
	src/client/engine/renderer/renderer_static_model_shader.c \
	src/client/engine/renderer/renderer_text.c \
	src/client/engine/renderer/renderer_texture_remap.c \
	src/client/engine/renderer/renderer_timing.c \
	src/client/engine/renderer/renderer_transform.c \
	src/client/engine/renderer/renderer_vbo.c \
	src/client/engine/renderer/renderer_water.c \
	src/client/engine/renderer/renderer_world_load.c \
	src/client/engine/renderer/renderer_world_optimize.c \
	src/client/engine/renderer/renderer_world.c \
	src/client/engine/renderer/renderer_xmodel_surface_optimize.c \
	src/client/engine/renderer/wgl_debug_wrappers.c \
	src/client/engine/scripting/script_global_storage.c \
	src/client/engine/scripting/script_variable_client.c \
	src/client/engine/server_startup_services.c \
	src/client/engine/server/server_client_message_services.c \
	src/client/engine/server/sv_game_syscalls.c \
	src/client/engine/server/sv_netchan.c \
	src/client/engine/server/sv_operator_commands.c \
	src/client/engine/server/sv_world.c \
	src/client/engine/sound/miles_boundary.c \
	src/client/engine/sound/miles_miniaudio_provider.c \
	src/client/engine/sound/miles_null_backend.c \
	src/client/engine/sound/miles_openal_backend.c \
	src/client/engine/sound/sound_alias_diagnostics.c \
	src/client/engine/sound/sound_alias_localize_command.c \
	src/client/engine/sound/sound_alias_runtime.c \
	src/client/engine/surface_types.c \
	src/client/engine/system_console.c \
	src/client/engine/system_cpu.c \
	src/client/engine/system_event.c \
	src/client/engine/system_fatal.c \
	src/client/engine/system_files.c \
	src/client/engine/system_info.c \
	src/client/engine/system_input.c \
	src/client/engine/system_localization.c \
	src/client/engine/system_main.c \
	src/client/engine/system_platform.c \
	src/client/engine/system_process_lock.c \
	src/client/engine/system_time.c \
	src/client/engine/system_video_memory.c \
	src/client/engine/ui/ui_client_state.c \
	src/client/engine/ui/ui_module_loader.c \
	src/client/engine/ui/ui_shutdown.c \
	src/client/engine/ui/ui_syscalls.c

# NOT_FROM_ORIGINAL_SOURCE: the stock source line owns the stock-behavior
# providers for filesystem and renderer policy.
CLIENT_ENGINE_C_SOURCES += \
	src/client/engine/filesystem/server_namespace_provider.c \
	src/client/engine/renderer/platform_gamma.c \
	src/client/engine/renderer/renderer_color_mappings.c

CLIENT_ENGINE_CXX_SOURCES := \
	src/client/engine/effects/fx_add.cpp \
	src/client/engine/effects/fx_api.cpp \
	src/client/engine/effects/fx_effect.cpp \
	src/client/engine/effects/fx_memory.cpp \
	src/client/engine/effects/fx_particle.cpp \
	src/client/engine/effects/fx_primitive_archive.cpp \
	src/client/engine/effects/fx_primitive_cull.cpp \
	src/client/engine/effects/fx_primitive_draw.cpp \
	src/client/engine/effects/fx_primitive_lifecycle.cpp \
	src/client/engine/effects/fx_primitive_memory.cpp \
	src/client/engine/effects/fx_primitive_template.cpp \
	src/client/engine/effects/fx_primitive_typeids.cpp \
	src/client/engine/effects/fx_primitive_update.cpp \
	src/client/engine/effects/fx_scheduler.cpp \
	src/client/engine/effects/fx_system_archive.cpp \
	src/client/engine/localization/string_ed_api.cpp \
	src/client/engine/localization/string_ed_package.cpp \
	src/client/engine/parser/generic_parser.cpp

# Complete CoDUOMP target membership. These are target selections rather than
# subsystem ownership; the complete ownership lists remain in common.mk.
CLIENT_ENGINE_CLIENT_MATH_C_SOURCES := \
	src/client/math/client_dobj_matrix.c \
	src/client/math/client_vector_util.c

CLIENT_ENGINE_MATH_C_SOURCES := \
	src/math/q_math.c \
	src/math/box_distance.c \
	src/math/q_rint.c \
	src/math/vector_rotate_angles.c \
	src/math/float_round_nearest.c \
	src/math/round_float.c \
	src/math/lean_math.c \
	src/math/angle_vectors.c \
	src/math/angle_mod.c \
	src/math/angle_normalization.c \
	src/math/angle_delta.c \
	src/math/angle_subtract.c \
	src/math/radius_from_bounds.c \
	src/math/color_math.c \
	src/math/vector_normalization_geometry.c \
	src/math/direction_angles.c \
	src/math/axis_to_angles.c \
	src/math/angles_to_axis.c \
	src/math/pitch_for_yaw_on_normal.c \
	src/math/vector_snap.c \
	src/math/orientation_transform.c \
	src/math/matrix_transform_vector43_equals.c \
	src/math/vector_compare_epsilon.c \
	src/math/cross_product.c \
	src/math/quat_multiply.c \
	src/math/byte_directions.c \
	src/math/set_plane_signbits.c \
	src/math/box_on_plane_side.c \
	src/math/matrix_transpose_transform_vector.c \
	src/math/matrix_transform_vector43.c \
	src/math/local_matrix_transform_vector43.c \
	src/math/dobj_skel_matrix.c \
	src/math/matrix_inverse.c \
	src/math/matrix_inverse44.c \
	src/math/matrix_multiply.c \
	src/math/matrix_multiply43.c \
	src/math/matrix_transform_vector.c \
	src/math/vector_rotation.c \
	src/math/normal_to_latlong.c \
	src/math/vector_polar.c \
	src/math/q_random.c \
	src/math/q_float_primitives.c \
	src/math/vector_angle_multiply.c \
	src/math/axis_quaternion.c \
	src/math/convert_quat_to_mat.c \
	src/math/quaternion_trace.c \
	src/math/q_shared_random.c \
	src/math/vector_projection.c \
	src/math/dobj_matrix_core.c \
	src/math/xsurface_matrix.c \
	src/math/q_round.c \
	src/math/vector_lane_ops.c

CLIENT_ENGINE_TARGET_C_SOURCES := \
	$(CLIENT_ENGINE_C_SOURCES) \
	$(LAYOUT_WINDOWS_GENERATED_C_SOURCES) \
	$(LAYOUT_CLIENT_COMMON_C_SOURCES) \
	$(CLIENT_ENGINE_CLIENT_MATH_C_SOURCES) \
	$(LAYOUT_QCOMMON_C_SOURCES) \
	$(LAYOUT_CRT_COMPAT_C_SOURCES) \
	$(CLIENT_ENGINE_MATH_C_SOURCES) \
	$(LAYOUT_SOUND_ALIAS_C_SOURCES) \
	$(LAYOUT_ANIMATION_C_SOURCES) \
	$(LAYOUT_COLLISION_C_SOURCES) \
	$(LAYOUT_FILESYSTEM_C_SOURCES) \
	$(LAYOUT_SERVER_ENGINE_C_SOURCES) \
	$(LAYOUT_SCRIPT_C_SOURCES)

CLIENT_ENGINE_TARGET_CXX_SOURCES := \
	$(CLIENT_ENGINE_CXX_SOURCES) \
	$(LAYOUT_SCRIPT_CXX_SOURCES)

# Native OpenGL profiles attributed most non-idle main-thread leaf time to
# collision, DPVS/static-model visibility, shared angle/rotation math,
# skeletal evaluation, cgame syscall/key-binding dispatch, draw-surface
# sorting, and the generic shader and XModel CPU loops.
# The native audio thread also spent most of its active time in Miniaudio's
# mixing and resampling loops.
# Keep this target-independent so every supported client-engine toolchain
# applies the same measured optimization policy to the same recovered sources.
# Strict aliasing remains disabled because these interfaces carry retail binary
# layouts; floating-point contraction and wrapping behavior remain controlled by
# each target's existing fidelity flags.
CLIENT_ENGINE_PROFILE_HOT_C_SOURCES := \
	src/math/angle_vectors.c \
	src/math/box_on_plane_side.c \
	src/collision/collision_patch_trace.c \
	src/collision/collision_capsule_traces.c \
	src/collision/collision_box_trace.c \
	src/collision/collision_leaf_traces.c \
	src/collision/collision_point_contents.c \
	src/collision/collision_terrain_point.c \
	src/collision/collision_terrain_position.c \
	src/collision/collision_terrain_sphere.c \
	src/collision/collision_trace_bounds.c \
	src/collision/collision_trace_entry.c \
	src/collision/collision_trace_through_brush.c \
	src/collision/collision_transforms.c \
	src/collision/collision_tree_traces.c \
	src/animation/dobj_core.c \
	src/animation/xmodel_accessors.c \
	src/client/engine/client/cgame_syscalls.c \
	src/client/engine/client/key_input.c \
	src/client/engine/renderer/backend_shader_stage.c \
	src/client/engine/renderer/backend_static_model.c \
	src/client/engine/renderer/backend_xmodel.c \
	src/client/engine/renderer/renderer_draw_surfs.c \
	src/client/engine/renderer/renderer_dpvs.c \
	src/client/engine/renderer/renderer_light_visibility.c \
	src/client/engine/sound/miles_miniaudio_provider.c

# These translation units are dominated by filesystem/archive ingestion and
# map or asset decoding. This tier adds no renderer geometry construction,
# visibility, or draw sources beyond the independently measured hot list.
CLIENT_ENGINE_LOAD_TIME_C_SOURCES := \
	src/filesystem/filesystem_open_read.c \
	src/filesystem/filesystem_pack.c \
	src/filesystem/filesystem_read.c \
	src/filesystem/filesystem_read_file.c \
	src/filesystem/filesystem_seek.c \
	src/filesystem/filesystem_stream.c \
	src/collision/collision_map_load.c \
	src/animation/xanim_asset_load.c \
	src/animation/xmodel_assets.c \
	src/animation/xmodel_endian.c \
	src/sound/alias/sound_alias_loadspec.c \
	src/client/engine/filesystem/filesystem_archive_services.c \
	src/client/engine/renderer/renderer_image.c \
	src/client/engine/renderer/renderer_lightmap.c \
	src/client/engine/renderer/renderer_model_load.c \
	src/client/engine/renderer/renderer_model_register.c \
	src/client/engine/renderer/renderer_shader_parse.c \
	src/client/engine/renderer/renderer_world_load.c

CLIENT_ENGINE_SELECTIVE_O2_C_SOURCES := \
	$(CLIENT_ENGINE_PROFILE_HOT_C_SOURCES) \
	$(CLIENT_ENGINE_LOAD_TIME_C_SOURCES)
CLIENT_ENGINE_SELECTIVE_O2_CFLAGS := -O2 -fno-strict-aliasing
