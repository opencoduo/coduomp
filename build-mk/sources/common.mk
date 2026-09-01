# Explicit common-source ownership. Keep target selection in the target manifests;
# this file only names the complete source sets owned by each semantic subsystem.

LAYOUT_QCOMMON_C_SOURCES := \
	src/qcommon/com_command_handlers.c \
	src/qcommon/com_config.c \
	src/qcommon/com_event_loop.c \
	src/qcommon/com_event_queue.c \
	src/qcommon/com_frame.c \
	src/qcommon/com_lifecycle.c \
	src/qcommon/com_memory.c \
	src/qcommon/com_parse.c \
	src/qcommon/com_redirect.c \
	src/qcommon/com_sprintf.c \
	src/qcommon/com_startup_commands.c \
	src/qcommon/com_string.c \
	src/qcommon/com_time.c \
	src/qcommon/huffman.c \
	src/qcommon/hunk_alloc.c \
	src/qcommon/hunk_diagnostics.c \
	src/qcommon/hunk_state.c \
	src/qcommon/hunk_touch.c \
	src/qcommon/info.c \
	src/qcommon/msg_base.c \
	src/qcommon/msg_delta.c \
	src/qcommon/msg_huffman.c \
	src/qcommon/net_compare.c \
	src/qcommon/net_loopback.c \
	src/qcommon/net_oob.c \
	src/qcommon/net_profile.c \
	src/qcommon/net_text.c \
	src/qcommon/netchan.c \
	src/qcommon/precompiler_core.c \
	src/qcommon/precompiler_define.c \
	src/qcommon/precompiler_directives.c \
	src/qcommon/precompiler_evaluate.c \
	src/qcommon/precompiler_script.c \
	src/qcommon/precompiler_source.c \
	src/qcommon/precompiler_tokenizer.c \
	src/qcommon/q_bits.c \
	src/qcommon/q_checksum.c \
	src/qcommon/q_command.c \
	src/qcommon/q_cpu.c \
	src/qcommon/q_cvar.c \
	src/qcommon/q_endian.c \
	src/qcommon/q_filter.c \
	src/qcommon/q_localized_float.c \
	src/qcommon/q_memory.c \
	src/qcommon/q_path.c \
	src/qcommon/q_shared_misc.c \
	src/qcommon/q_string.c \
	src/qcommon/q_temp.c \
	src/qcommon/vm_runtime.c

LAYOUT_CRT_COMPAT_C_SOURCES := \
	src/compat/crt/format_compat.c \
	src/compat/crt/msvc_compat.c \
	src/compat/crt/qsort_compat.c \
	src/compat/crt/random_compat.c

LAYOUT_MATH_C_SOURCES := \
	src/math/angle_delta.c \
	src/math/angle_mod.c \
	src/math/angle_normalization.c \
	src/math/angle_subtract.c \
	src/math/angle_vectors.c \
	src/math/angles_to_axis.c \
	src/math/axis_quaternion.c \
	src/math/axis_to_angles.c \
	src/math/box_distance.c \
	src/math/box_on_plane_side.c \
	src/math/byte_directions.c \
	src/math/color_math.c \
	src/math/convert_quat_to_mat.c \
	src/math/cross_product.c \
	src/math/direction_angles.c \
	src/math/dobj_matrix_core.c \
	src/math/dobj_skel_matrix_transform_vector43.c \
	src/math/dobj_skel_matrix.c \
	src/math/float_round_nearest.c \
	src/math/lean_math.c \
	src/math/local_matrix_transform_vector43.c \
	src/math/matrix_inverse.c \
	src/math/matrix_inverse44.c \
	src/math/matrix_multiply.c \
	src/math/matrix_multiply34.c \
	src/math/matrix_multiply43.c \
	src/math/matrix_transform_vector.c \
	src/math/matrix_transform_vector43_equals.c \
	src/math/matrix_transform_vector43.c \
	src/math/matrix_transpose_transform_vector.c \
	src/math/normal_to_latlong.c \
	src/math/orientation_transform.c \
	src/math/pitch_for_yaw_on_normal.c \
	src/math/q_float_primitives.c \
	src/math/q_math.c \
	src/math/q_random.c \
	src/math/q_rint.c \
	src/math/q_round.c \
	src/math/q_shared_random.c \
	src/math/quat_multiply.c \
	src/math/quaternion_trace.c \
	src/math/radius_from_bounds.c \
	src/math/round_float.c \
	src/math/set_plane_signbits.c \
	src/math/vector_angle_multiply.c \
	src/math/vector_compare_epsilon.c \
	src/math/vector_lane_ops.c \
	src/math/vector_normalization_geometry.c \
	src/math/vector_polar.c \
	src/math/vector_projection.c \
	src/math/vector_rotate_angles.c \
	src/math/vector_rotation.c \
	src/math/vector_snap.c \
	src/math/xsurface_matrix.c

LAYOUT_COLLISION_C_SOURCES := \
	src/collision/collision_area.c \
	src/collision/collision_box_trace.c \
	src/collision/collision_capsule_traces.c \
	src/collision/collision_entity_traversal.c \
	src/collision/collision_geometry.c \
	src/collision/collision_globals.c \
	src/collision/collision_leaf_queries.c \
	src/collision/collision_leaf_traces.c \
	src/collision/collision_map_load.c \
	src/collision/collision_patch_build.c \
	src/collision/collision_patch_dispatch.c \
	src/collision/collision_patch_trace.c \
	src/collision/collision_point_contents.c \
	src/collision/collision_queries.c \
	src/collision/collision_server_entity.c \
	src/collision/collision_server_trace_callbacks.c \
	src/collision/collision_server_trace.c \
	src/collision/collision_sight_trace_through_brush.c \
	src/collision/collision_static_model_trace.c \
	src/collision/collision_static_models.c \
	src/collision/collision_terrain_dispatch.c \
	src/collision/collision_terrain_point.c \
	src/collision/collision_terrain_position.c \
	src/collision/collision_terrain_sphere.c \
	src/collision/collision_test_box_in_brush.c \
	src/collision/collision_trace_bounds.c \
	src/collision/collision_trace_entry.c \
	src/collision/collision_trace_through_brush.c \
	src/collision/collision_transforms.c \
	src/collision/collision_tree_traces.c \
	src/collision/collision_triangle_soup.c \
	src/collision/collision_world_sector.c \
	src/collision/winding.c

LAYOUT_FILESYSTEM_C_SOURCES := \
	src/filesystem/filesystem_catalog_core.c \
	src/filesystem/filesystem_commands.c \
	src/filesystem/filesystem_compare.c \
	src/filesystem/filesystem_directory.c \
	src/filesystem/filesystem_handles.c \
	src/filesystem/filesystem_host_files.c \
	src/filesystem/filesystem_list.c \
	src/filesystem/filesystem_lookup.c \
	src/filesystem/filesystem_loose_files.c \
	src/filesystem/filesystem_open_read.c \
	src/filesystem/filesystem_open_wrappers.c \
	src/filesystem/filesystem_open_write.c \
	src/filesystem/filesystem_pack.c \
	src/filesystem/filesystem_path_build.c \
	src/filesystem/filesystem_path_order.c \
	src/filesystem/filesystem_path_security.c \
	src/filesystem/filesystem_pure.c \
	src/filesystem/filesystem_read_file.c \
	src/filesystem/filesystem_read_lifecycle.c \
	src/filesystem/filesystem_read.c \
	src/filesystem/filesystem_searchpaths.c \
	src/filesystem/filesystem_seek.c \
	src/filesystem/filesystem_short_path.c \
	src/filesystem/filesystem_shutdown.c \
	src/filesystem/filesystem_startup.c \
	src/filesystem/filesystem_stream.c \
	src/filesystem/filesystem_write.c

LAYOUT_ANIMATION_C_SOURCES := \
	src/animation/animation_diagnostics.c \
	src/animation/dobj_accessors.c \
	src/animation/dobj_core.c \
	src/animation/dobj_model_queries.c \
	src/animation/dobj_pool.c \
	src/animation/dobj_surfaces.c \
	src/animation/xanim_asset_load.c \
	src/animation/xanim_eval_output.c \
	src/animation/xanim_eval.c \
	src/animation/xanim_keys.c \
	src/animation/xanim_notetrack.c \
	src/animation/xanim_notify_timing.c \
	src/animation/xanim_pool_accessors.c \
	src/animation/xanim_pool_lifecycle.c \
	src/animation/xanim_pool_nodes.c \
	src/animation/xanim_runtime.c \
	src/animation/xanim_serialization.c \
	src/animation/xanim_state.c \
	src/animation/xanim_tree_clear.c \
	src/animation/xanim_tree_queries.c \
	src/animation/xanim_tree.c \
	src/animation/xmodel_accessors.c \
	src/animation/xmodel_assets.c \
	src/animation/xmodel_endian.c \
	src/animation/xmodel_lifecycle.c \
	src/animation/xmodel_quaternion.c

LAYOUT_SCRIPT_C_SOURCES := \
	src/scripting/script_anim_runtime.c \
	src/scripting/script_array.c \
	src/scripting/script_code_emit.c \
	src/scripting/script_compile_developer.c \
	src/scripting/script_compile_expr.c \
	src/scripting/script_compile_load.c \
	src/scripting/script_compile_statements.c \
	src/scripting/script_error_reporting.c \
	src/scripting/script_import_fields.c \
	src/scripting/script_memory.c \
	src/scripting/script_notify.c \
	src/scripting/script_runtime_callbacks.c \
	src/scripting/script_runtime_state.c \
	src/scripting/script_serialization.c \
	src/scripting/script_source_positions.c \
	src/scripting/script_string.c \
	src/scripting/script_temp_memory.c \
	src/scripting/script_thread.c \
	src/scripting/script_usage.c \
	src/scripting/script_value_refs.c \
	src/scripting/script_value.c \
	src/scripting/script_variable.c \
	src/scripting/script_yy_runtime.c \
	src/scripting/script_yy_tokens.c

LAYOUT_SCRIPT_CXX_SOURCES := \
	src/scripting/script_error_exception.cpp \
	src/scripting/script_vm.cpp

LAYOUT_SOUND_ALIAS_C_SOURCES := \
	src/sound/alias/sound_alias_build.c \
	src/sound/alias/sound_alias_csv.c \
	src/sound/alias/sound_alias_globals.c \
	src/sound/alias/sound_alias_loadspec.c \
	src/sound/alias/sound_alias_localize.c \
	src/sound/alias/sound_alias_lookup.c \
	src/sound/alias/sound_alias_parse.c \
	src/sound/alias/sound_alias_permanent.c \
	src/sound/alias/sound_alias_picker.c \
	src/sound/alias/sound_alias_runtime.c \
	src/sound/alias/sound_alias_subtitle_refs.c

LAYOUT_BG_C_SOURCES := \
	src/bg/bg_accelerate.c \
	src/bg/bg_ads.c \
	src/bg/bg_aim_spread.c \
	src/bg/bg_air_move.c \
	src/bg/bg_animation_conditions.c \
	src/bg/bg_animation_controller_lerp.c \
	src/bg/bg_animation_errors.c \
	src/bg/bg_animation_execution.c \
	src/bg/bg_animation_finalize.c \
	src/bg/bg_animation_lookup.c \
	src/bg/bg_animation_player_conditions.c \
	src/bg/bg_animation_script_data.c \
	src/bg/bg_animation_script_parser.c \
	src/bg/bg_animation_slots.c \
	src/bg/bg_animation_stance.c \
	src/bg/bg_animation_tree_lookup.c \
	src/bg/bg_bob.c \
	src/bg/bg_bullet_spread.c \
	src/bg/bg_check_duck.c \
	src/bg/bg_check_prone_valid.c \
	src/bg/bg_cmd_scale_walk.c \
	src/bg/bg_cmd_scale.c \
	src/bg/bg_crash_land.c \
	src/bg/bg_dead_move.c \
	src/bg/bg_fatigue.c \
	src/bg/bg_fly_move.c \
	src/bg/bg_foliage.c \
	src/bg/bg_footstep_events.c \
	src/bg/bg_footsteps.c \
	src/bg/bg_friction.c \
	src/bg/bg_ground_correct_all_solid.c \
	src/bg/bg_ground_trace_missed.c \
	src/bg/bg_ground_trace.c \
	src/bg/bg_init_weapon_strings.c \
	src/bg/bg_item_lookup.c \
	src/bg/bg_jump_launch.c \
	src/bg/bg_jump.c \
	src/bg/bg_ladder.c \
	src/bg/bg_lean.c \
	src/bg/bg_load_anim_for_index.c \
	src/bg/bg_load_anim_tree_instances.c \
	src/bg/bg_movement_flags.c \
	src/bg/bg_movement_speed.c \
	src/bg/bg_movement_timers.c \
	src/bg/bg_noclip_move.c \
	src/bg/bg_parse_commands.c \
	src/bg/bg_parse_condition_bits.c \
	src/bg/bg_parse_conditions.c \
	src/bg/bg_player_angles.c \
	src/bg/bg_player_animation.c \
	src/bg/bg_player_controller_apply.c \
	src/bg/bg_player_controllers.c \
	src/bg/bg_player_state_to_entity_state_extrapolate.c \
	src/bg/bg_player_state_to_entity_state.c \
	src/bg/bg_pmove_core.c \
	src/bg/bg_pmove_driver.c \
	src/bg/bg_pmove_helpers.c \
	src/bg/bg_pmove_single.c \
	src/bg/bg_pmove_state.c \
	src/bg/bg_predictable_events.c \
	src/bg/bg_prone_movement.c \
	src/bg/bg_set_movement_dir.c \
	src/bg/bg_setup_anim_note_types.c \
	src/bg/bg_slide_move.c \
	src/bg/bg_surface_events.c \
	src/bg/bg_trajectory.c \
	src/bg/bg_ufo_move.c \
	src/bg/bg_update_view_angles.c \
	src/bg/bg_vehicle.c \
	src/bg/bg_verify_prone_position.c \
	src/bg/bg_view_angles.c \
	src/bg/bg_view_damage.c \
	src/bg/bg_view_velocity.c \
	src/bg/bg_viewheight_adjust.c \
	src/bg/bg_viewheight_lerp_time.c \
	src/bg/bg_viewheight_lerp.c \
	src/bg/bg_viewheight_table_lerp.c \
	src/bg/bg_viewheight_tables.c \
	src/bg/bg_walk_move.c \
	src/bg/bg_water.c \
	src/bg/bg_weapon_ammo.c \
	src/bg/bg_weapon_angles.c \
	src/bg/bg_weapon_animation_state.c \
	src/bg/bg_weapon_debug.c \
	src/bg/bg_weapon_driver.c \
	src/bg/bg_weapon_firing.c \
	src/bg/bg_weapon_interrupt.c \
	src/bg/bg_weapon_inventory.c \
	src/bg/bg_weapon_names.c \
	src/bg/bg_weapon_parse.c \
	src/bg/bg_weapon_position_base_angles.c \
	src/bg/bg_weapon_position_base.c \
	src/bg/bg_weapon_position_bob.c \
	src/bg/bg_weapon_position_damage.c \
	src/bg/bg_weapon_position_idle.c \
	src/bg/bg_weapon_position_recoil.c \
	src/bg/bg_weapon_rechamber.c \
	src/bg/bg_weapon_recoil_single.c \
	src/bg/bg_weapon_recoil.c \
	src/bg/bg_weapon_registry.c \
	src/bg/bg_weapon_reload.c \
	src/bg/bg_weapon_setup.c \
	src/bg/bg_weapon_spread.c \
	src/bg/bg_weapon_sway.c \
	src/bg/bg_weapon_transition.c

LAYOUT_LIBM_C_SOURCES := \
	src/compat/libm/coduo_libm_asin.c \
	src/compat/libm/coduo_libm_ftol.c \
	src/compat/libm/coduo_libm_sincos.c \
	src/compat/libm/coduo_libm_sqrt.c

LAYOUT_CLIENT_COMMON_C_SOURCES := \
	src/client/common/client_common.c \
	src/client/common/client_legacy_crt.c \
	src/client/common/localized_float.c

LAYOUT_CLIENT_MATH_C_SOURCES := \
	src/client/math/client_dobj_matrix.c \
	src/client/math/client_geometry.c \
	src/client/math/client_quaternion.c \
	src/client/math/client_rotation.c \
	src/client/math/client_rounding.c \
	src/client/math/client_vector_util.c

LAYOUT_CLIENT_MENU_C_SOURCES := \
	src/client/menu/ui_bindings.c \
	src/client/menu/ui_capture.c \
	src/client/menu/ui_color.c \
	src/client/menu/ui_common_leaves.c \
	src/client/menu/ui_content_cache.c \
	src/client/menu/ui_display_cursor.c \
	src/client/menu/ui_display_key.c \
	src/client/menu/ui_display_mouse.c \
	src/client/menu/ui_focus.c \
	src/client/menu/ui_geometry.c \
	src/client/menu/ui_item_action.c \
	src/client/menu/ui_item_asset_paint.c \
	src/client/menu/ui_item_bind_key.c \
	src/client/menu/ui_item_bind_paint.c \
	src/client/menu/ui_item_enable_cvar.c \
	src/client/menu/ui_item_listbox_geometry.c \
	src/client/menu/ui_item_listbox_key.c \
	src/client/menu/ui_item_listbox_paint.c \
	src/client/menu/ui_item_model.c \
	src/client/menu/ui_item_multi.c \
	src/client/menu/ui_item_ownerdraw_key.c \
	src/client/menu/ui_item_ownerdraw_paint.c \
	src/client/menu/ui_item_paint.c \
	src/client/menu/ui_item_parse_columns.c \
	src/client/menu/ui_item_parse_cvarfloatlist.c \
	src/client/menu/ui_item_parse_cvarstrlist.c \
	src/client/menu/ui_item_parse_model_animplay.c \
	src/client/menu/ui_item_parse_origin.c \
	src/client/menu/ui_item_parse_textfile.c \
	src/client/menu/ui_item_post_parse.c \
	src/client/menu/ui_item_slider.c \
	src/client/menu/ui_item_text_autowrap.c \
	src/client/menu/ui_item_text_color.c \
	src/client/menu/ui_item_text_extents.c \
	src/client/menu/ui_item_text_wrap.c \
	src/client/menu/ui_item_text.c \
	src/client/menu/ui_item_textfield_key.c \
	src/client/menu/ui_item_textfield_paint.c \
	src/client/menu/ui_item_yesno.c \
	src/client/menu/ui_key.c \
	src/client/menu/ui_keyword_dispatch.c \
	src/client/menu/ui_keyword_hash.c \
	src/client/menu/ui_keyword_tables.c \
	src/client/menu/ui_memory.c \
	src/client/menu/ui_menu_feeder_scroll.c \
	src/client/menu/ui_menu_feeder_selection.c \
	src/client/menu/ui_menu_paint_all.c \
	src/client/menu/ui_menu_parse_font.c \
	src/client/menu/ui_menu_post_parse.c \
	src/client/menu/ui_menu_queries.c \
	src/client/menu/ui_menu_stack.c \
	src/client/menu/ui_menu_visibility.c \
	src/client/menu/ui_motion.c \
	src/client/menu/ui_parse_color_range.c \
	src/client/menu/ui_parse_direct_handlers.c \
	src/client/menu/ui_parse.c \
	src/client/menu/ui_runtime.c \
	src/client/menu/ui_script_cvar_commands.c \
	src/client/menu/ui_script_dispatch.c \
	src/client/menu/ui_script_list_commands.c \
	src/client/menu/ui_script_menu_navigation.c \
	src/client/menu/ui_script_visual_commands.c \
	src/client/menu/ui_window_paint.c

LAYOUT_SERVER_ENGINE_C_SOURCES := \
	src/server/engine/server_authorize.c \
	src/server/engine/server_client_gamestate.c \
	src/server/engine/server_client_maintenance.c \
	src/server/engine/server_client_message.c \
	src/server/engine/server_client_release.c \
	src/server/engine/server_commands.c \
	src/server/engine/server_configstrings.c \
	src/server/engine/server_connect.c \
	src/server/engine/server_dobj.c \
	src/server/engine/server_download.c \
	src/server/engine/server_frame.c \
	src/server/engine/server_game_bridge.c \
	src/server/engine/server_game_data.c \
	src/server/engine/server_game_hunk.c \
	src/server/engine/server_game_lifecycle.c \
	src/server/engine/server_game_queries.c \
	src/server/engine/server_game_syscalls.c \
	src/server/engine/server_lifecycle.c \
	src/server/engine/server_master.c \
	src/server/engine/server_netchan.c \
	src/server/engine/server_operator_clients.c \
	src/server/engine/server_operator_maps.c \
	src/server/engine/server_operator_runtime.c \
	src/server/engine/server_packet.c \
	src/server/engine/server_punkbuster_queries.c \
	src/server/engine/server_snapshot_archive.c \
	src/server/engine/server_snapshot_send.c \
	src/server/engine/server_startup.c \
	src/server/engine/server_xmodel.c

LAYOUT_WINDOWS_GENERATED_C_SOURCES := \
	src/scripting/generated/windows/script_parse_nodes.c \
	src/scripting/generated/windows/script_yy_ast_builders.c \
	src/scripting/generated/windows/script_yy_parse.c \
	src/scripting/generated/windows/script_yy_tables.c

LAYOUT_POSIX_GENERATED_C_SOURCES := \
	src/scripting/generated/posix/script_yy_builders.c \
	src/scripting/generated/posix/script_yy_input.c \
	src/scripting/generated/posix/script_yy_parse.c \
	src/scripting/generated/posix/script_yy_tables.c
