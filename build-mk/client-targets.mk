.PHONY: client-native-link client-windows-cross-link client-win32-abi-link \
	coduomp-native-link

include build-mk/sources/cgame.mk
include build-mk/sources/client-engine.mk

PKG_CONFIG ?= pkg-config

CLIENT_C_SOURCES := $(CGAME_C_SOURCES)
SHARED_MATH_DIR := src/math
SHARED_MATH_C_SOURCES := $(SHARED_MATH_DIR)/q_math.c \
	$(SHARED_MATH_DIR)/box_distance.c \
	$(SHARED_MATH_DIR)/q_rint.c \
	$(SHARED_MATH_DIR)/vector_rotate_angles.c \
	$(SHARED_MATH_DIR)/float_round_nearest.c \
	$(SHARED_MATH_DIR)/round_float.c \
	$(SHARED_MATH_DIR)/lean_math.c \
	$(SHARED_MATH_DIR)/angle_vectors.c \
	$(SHARED_MATH_DIR)/angle_mod.c \
	$(SHARED_MATH_DIR)/angle_normalization.c \
	$(SHARED_MATH_DIR)/angle_delta.c \
	$(SHARED_MATH_DIR)/angle_subtract.c \
	$(SHARED_MATH_DIR)/radius_from_bounds.c \
	$(SHARED_MATH_DIR)/color_math.c \
	$(SHARED_MATH_DIR)/vector_normalization_geometry.c \
	$(SHARED_MATH_DIR)/direction_angles.c \
	$(SHARED_MATH_DIR)/axis_to_angles.c \
	$(SHARED_MATH_DIR)/angles_to_axis.c \
	$(SHARED_MATH_DIR)/pitch_for_yaw_on_normal.c \
	$(SHARED_MATH_DIR)/vector_snap.c \
	$(SHARED_MATH_DIR)/orientation_transform.c \
	$(SHARED_MATH_DIR)/matrix_transform_vector43_equals.c \
	$(SHARED_MATH_DIR)/vector_compare_epsilon.c \
	$(SHARED_MATH_DIR)/cross_product.c \
	$(SHARED_MATH_DIR)/quat_multiply.c \
	$(SHARED_MATH_DIR)/byte_directions.c \
	$(SHARED_MATH_DIR)/set_plane_signbits.c \
	$(SHARED_MATH_DIR)/box_on_plane_side.c \
	$(SHARED_MATH_DIR)/matrix_transpose_transform_vector.c \
	$(SHARED_MATH_DIR)/matrix_transform_vector43.c \
	$(SHARED_MATH_DIR)/local_matrix_transform_vector43.c \
	$(SHARED_MATH_DIR)/dobj_skel_matrix.c \
	$(SHARED_MATH_DIR)/matrix_inverse.c \
	$(SHARED_MATH_DIR)/matrix_inverse44.c \
	$(SHARED_MATH_DIR)/matrix_multiply.c \
	$(SHARED_MATH_DIR)/matrix_multiply43.c \
	$(SHARED_MATH_DIR)/matrix_transform_vector.c \
	$(SHARED_MATH_DIR)/vector_rotation.c \
	$(SHARED_MATH_DIR)/normal_to_latlong.c \
	$(SHARED_MATH_DIR)/vector_polar.c \
	$(SHARED_MATH_DIR)/q_random.c \
	$(SHARED_MATH_DIR)/q_float_primitives.c \
	$(SHARED_MATH_DIR)/vector_angle_multiply.c \
	$(SHARED_MATH_DIR)/axis_quaternion.c \
	$(SHARED_MATH_DIR)/convert_quat_to_mat.c \
	$(SHARED_MATH_DIR)/quaternion_trace.c \
	$(SHARED_MATH_DIR)/q_shared_random.c \
	$(SHARED_MATH_DIR)/vector_projection.c
CLIENT_SHARED_MATH_C_SOURCES := $(SHARED_MATH_C_SOURCES)
CLIENT_MODULE_MATH_DIR := src/client/math
CLIENT_MODULE_MATH_C_SOURCES := \
	$(CLIENT_MODULE_MATH_DIR)/client_geometry.c \
	$(CLIENT_MODULE_MATH_DIR)/client_dobj_matrix.c \
	$(CLIENT_MODULE_MATH_DIR)/client_quaternion.c \
	$(CLIENT_MODULE_MATH_DIR)/client_rotation.c \
	$(CLIENT_MODULE_MATH_DIR)/client_rounding.c \
	$(CLIENT_MODULE_MATH_DIR)/client_vector_util.c
SHARED_CLIENT_COMMON_DIR := src/client/common
SHARED_CLIENT_COMMON_C_SOURCES := \
	$(SHARED_CLIENT_COMMON_DIR)/client_common.c \
	$(SHARED_CLIENT_COMMON_DIR)/client_legacy_crt.c \
	$(SHARED_CLIENT_COMMON_DIR)/localized_float.c
CODUOMP_SHARED_MATH_C_SOURCES := $(SHARED_MATH_C_SOURCES) \
	$(SHARED_MATH_DIR)/dobj_matrix_core.c \
	$(SHARED_MATH_DIR)/xsurface_matrix.c \
	$(SHARED_MATH_DIR)/q_round.c \
	$(SHARED_MATH_DIR)/vector_lane_ops.c
CODUOMP_CLIENT_MATH_C_SOURCES := \
	$(CLIENT_MODULE_MATH_DIR)/client_dobj_matrix.c \
	$(CLIENT_MODULE_MATH_DIR)/client_vector_util.c
CLIENT_SHARED_QCOMMON_C_SOURCES := src/qcommon/com_parse.c \
	src/qcommon/com_sprintf.c \
	src/qcommon/info.c \
	src/qcommon/q_bits.c \
	src/qcommon/q_endian.c \
	src/qcommon/q_localized_float.c \
	src/qcommon/q_path.c \
	src/qcommon/q_shared_misc.c \
	src/qcommon/q_string.c \
	src/qcommon/q_temp.c
SHARED_CRT_COMPAT_DIR := src/compat/crt
SHARED_CRT_COMPAT_C_SOURCES := $(LAYOUT_CRT_COMPAT_C_SOURCES)
CLIENT_SHARED_BG_DIR := src/bg
CLIENT_SHARED_BG_C_SOURCES := $(LAYOUT_BG_C_SOURCES)
CLIENT_SHARED_UI_DIR := src/client/menu
CLIENT_SHARED_UI_C_SOURCES := $(CLIENT_SHARED_UI_DIR)/ui_parse.c \
	$(CLIENT_SHARED_UI_DIR)/ui_bindings.c \
	$(CLIENT_SHARED_UI_DIR)/ui_runtime.c \
	$(CLIENT_SHARED_UI_DIR)/ui_geometry.c \
	$(CLIENT_SHARED_UI_DIR)/ui_focus.c \
	$(CLIENT_SHARED_UI_DIR)/ui_motion.c \
	$(CLIENT_SHARED_UI_DIR)/ui_capture.c \
	$(CLIENT_SHARED_UI_DIR)/ui_key.c \
	$(CLIENT_SHARED_UI_DIR)/ui_window_paint.c \
	$(CLIENT_SHARED_UI_DIR)/ui_menu_queries.c \
	$(CLIENT_SHARED_UI_DIR)/ui_menu_stack.c \
	$(CLIENT_SHARED_UI_DIR)/ui_menu_visibility.c \
	$(CLIENT_SHARED_UI_DIR)/ui_script_menu_navigation.c \
	$(CLIENT_SHARED_UI_DIR)/ui_script_cvar_commands.c \
	$(CLIENT_SHARED_UI_DIR)/ui_script_list_commands.c \
	$(CLIENT_SHARED_UI_DIR)/ui_script_visual_commands.c \
	$(CLIENT_SHARED_UI_DIR)/ui_script_dispatch.c \
	$(CLIENT_SHARED_UI_DIR)/ui_keyword_hash.c \
	$(CLIENT_SHARED_UI_DIR)/ui_keyword_tables.c \
	$(CLIENT_SHARED_UI_DIR)/ui_keyword_dispatch.c \
	$(CLIENT_SHARED_UI_DIR)/ui_content_cache.c \
	$(CLIENT_SHARED_UI_DIR)/ui_color.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_multi.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_yesno.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_slider.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_text_color.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_text_extents.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_text_autowrap.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_text_wrap.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_text.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_textfield_key.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_textfield_paint.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_enable_cvar.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_ownerdraw_key.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_ownerdraw_paint.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_action.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_asset_paint.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_model.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_listbox_geometry.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_listbox_key.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_listbox_paint.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_bind_key.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_bind_paint.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_paint.c \
	$(CLIENT_SHARED_UI_DIR)/ui_menu_paint_all.c \
	$(CLIENT_SHARED_UI_DIR)/ui_parse_direct_handlers.c \
	$(CLIENT_SHARED_UI_DIR)/ui_parse_color_range.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_parse_origin.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_parse_textfile.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_parse_model_animplay.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_parse_columns.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_parse_cvarstrlist.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_parse_cvarfloatlist.c \
	$(CLIENT_SHARED_UI_DIR)/ui_menu_parse_font.c \
	$(CLIENT_SHARED_UI_DIR)/ui_item_post_parse.c \
	$(CLIENT_SHARED_UI_DIR)/ui_menu_post_parse.c \
	$(CLIENT_SHARED_UI_DIR)/ui_memory.c \
	$(CLIENT_SHARED_UI_DIR)/ui_display_mouse.c \
	$(CLIENT_SHARED_UI_DIR)/ui_display_cursor.c \
	$(CLIENT_SHARED_UI_DIR)/ui_display_key.c \
	$(CLIENT_SHARED_UI_DIR)/ui_common_leaves.c \
	$(CLIENT_SHARED_UI_DIR)/ui_menu_feeder_scroll.c \
	$(CLIENT_SHARED_UI_DIR)/ui_menu_feeder_selection.c
CLIENT_ALL_C_SOURCES := $(CLIENT_C_SOURCES) $(CLIENT_SHARED_QCOMMON_C_SOURCES) \
	$(SHARED_CRT_COMPAT_C_SOURCES) \
	$(CLIENT_SHARED_MATH_C_SOURCES) $(CLIENT_MODULE_MATH_C_SOURCES) \
	$(SHARED_CLIENT_COMMON_C_SOURCES) \
	$(CLIENT_SHARED_BG_C_SOURCES) \
	$(CLIENT_SHARED_UI_C_SOURCES)
# Client targets use native floating-point arithmetic.  Set this at the
# compile boundary because shared translation units can include the generic
# platform policy before a client-owned umbrella header establishes it.
override CLIENT_FP_CPPFLAGS := -DEMULATE_X87=0
CLIENT_INCLUDE_FLAGS := -iquote src/client/cgame/bindings \
	-iquote src/qcommon/bindings/default \
	-Isrc
CLIENT_WINDOWS_CC ?= x86_64-w64-mingw32-gcc
CLIENT_WIN32_CC ?= i686-w64-mingw32-gcc
CLIENT_WINDOWS_DEF := build-mk/abi/cgame/uo_cgame_mp_x86.def
CLIENT_WINDOWS_LIBRARY ?= .workbench/build/cgame/windows-x86_64/uo_cgame_mp_x86.dll
CLIENT_WIN32_LIBRARY ?= .workbench/build/cgame/windows-i686/uo_cgame_mp_x86.dll
SHARED_QCOMMON_DIR := src/qcommon
SHARED_QCOMMON_C_SOURCES := $(SHARED_QCOMMON_DIR)/com_parse.c \
	$(SHARED_QCOMMON_DIR)/com_memory.c \
	$(SHARED_QCOMMON_DIR)/com_redirect.c \
	$(SHARED_QCOMMON_DIR)/com_sprintf.c \
	$(SHARED_QCOMMON_DIR)/com_config.c \
	$(SHARED_QCOMMON_DIR)/com_command_handlers.c \
	$(SHARED_QCOMMON_DIR)/com_frame.c \
	$(SHARED_QCOMMON_DIR)/com_event_queue.c \
	$(SHARED_QCOMMON_DIR)/com_event_loop.c \
	$(SHARED_QCOMMON_DIR)/com_lifecycle.c \
	$(SHARED_QCOMMON_DIR)/com_startup_commands.c \
	$(SHARED_QCOMMON_DIR)/com_string.c \
	$(SHARED_QCOMMON_DIR)/com_time.c \
	$(SHARED_QCOMMON_DIR)/huffman.c \
	$(SHARED_QCOMMON_DIR)/hunk_alloc.c \
	$(SHARED_QCOMMON_DIR)/hunk_diagnostics.c \
	$(SHARED_QCOMMON_DIR)/hunk_state.c \
	$(SHARED_QCOMMON_DIR)/hunk_touch.c \
	$(SHARED_QCOMMON_DIR)/info.c \
	$(SHARED_QCOMMON_DIR)/msg_base.c \
	$(SHARED_QCOMMON_DIR)/msg_delta.c \
	$(SHARED_QCOMMON_DIR)/msg_huffman.c \
	$(SHARED_QCOMMON_DIR)/net_compare.c \
	$(SHARED_QCOMMON_DIR)/net_loopback.c \
	$(SHARED_QCOMMON_DIR)/net_oob.c \
	$(SHARED_QCOMMON_DIR)/net_text.c \
	$(SHARED_QCOMMON_DIR)/net_profile.c \
	$(SHARED_QCOMMON_DIR)/netchan.c \
	$(SHARED_QCOMMON_DIR)/precompiler_core.c \
	$(SHARED_QCOMMON_DIR)/precompiler_define.c \
	$(SHARED_QCOMMON_DIR)/precompiler_directives.c \
	$(SHARED_QCOMMON_DIR)/precompiler_evaluate.c \
	$(SHARED_QCOMMON_DIR)/precompiler_script.c \
	$(SHARED_QCOMMON_DIR)/precompiler_source.c \
	$(SHARED_QCOMMON_DIR)/precompiler_tokenizer.c \
	$(SHARED_QCOMMON_DIR)/q_bits.c \
	$(SHARED_QCOMMON_DIR)/q_checksum.c \
	$(SHARED_QCOMMON_DIR)/q_cpu.c \
	$(SHARED_QCOMMON_DIR)/q_endian.c \
	$(SHARED_QCOMMON_DIR)/q_localized_float.c \
	$(SHARED_QCOMMON_DIR)/q_filter.c \
	$(SHARED_QCOMMON_DIR)/q_memory.c \
	$(SHARED_QCOMMON_DIR)/q_path.c \
	$(SHARED_QCOMMON_DIR)/q_shared_misc.c \
	$(SHARED_QCOMMON_DIR)/q_cvar.c \
	$(SHARED_QCOMMON_DIR)/q_command.c \
	$(SHARED_QCOMMON_DIR)/q_string.c \
	$(SHARED_QCOMMON_DIR)/q_temp.c \
	$(SHARED_QCOMMON_DIR)/vm_runtime.c
SHARED_SOUND_ALIAS_DIR := src/sound/alias
SHARED_SOUND_ALIAS_C_SOURCES := $(LAYOUT_SOUND_ALIAS_C_SOURCES)
SHARED_ANIMATION_DIR := src/animation
SHARED_ANIMATION_C_SOURCES := $(LAYOUT_ANIMATION_C_SOURCES)
SHARED_COLLISION_DIR := src/collision
SHARED_COLLISION_C_SOURCES := $(LAYOUT_COLLISION_C_SOURCES)
SHARED_FILESYSTEM_DIR := src/filesystem
SHARED_FILESYSTEM_C_SOURCES := $(LAYOUT_FILESYSTEM_C_SOURCES)
SHARED_SERVER_DIR := src/server/engine
SHARED_SERVER_C_SOURCES := $(LAYOUT_SERVER_ENGINE_C_SOURCES)
SHARED_SCRIPT_DIR := src/scripting
SHARED_SCRIPT_C_SOURCES := $(LAYOUT_SCRIPT_C_SOURCES)
SHARED_SCRIPT_CXX_SOURCES := $(LAYOUT_SCRIPT_CXX_SOURCES)
CODUO_DEFAULT_BEHAVIOR := windows
# The retail client and its embedded server are one Windows executable. Do not
# permit a command-line override to compile shared server code as Linux inside
# a client build.
override CODUO_BEHAVIOR := windows
include build-mk/platform-behavior.mk
CODUOMP_DISABLE_AUDIO ?= 0
RECOVERY_POLICY_CPPFLAGS :=
ifeq ($(CODUOMP_DISABLE_AUDIO),1)
CODUOMP_AUDIO_CPPFLAGS := -DCODUOMP_DISABLE_AUDIO
endif
CODUOMP_C_SOURCES := $(CLIENT_ENGINE_C_SOURCES)
CODUOMP_CXX_SOURCES := $(CLIENT_ENGINE_CXX_SOURCES)
CODUOMP_WINDOWS_GENERATED_DIR := src/scripting/generated/windows
CODUOMP_WINDOWS_GENERATED_C_SOURCES := $(LAYOUT_WINDOWS_GENERATED_C_SOURCES)
CODUOMP_JPEG_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags libjpeg 2>/dev/null)
CODUOMP_JPEG_LIBS ?= $(shell $(PKG_CONFIG) --libs libjpeg 2>/dev/null)
CODUOMP_SDL_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
CODUOMP_SDL_LIBS ?= $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)
CODUOMP_CURL_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags libcurl 2>/dev/null)
CODUOMP_CURL_LIBS ?= $(shell $(PKG_CONFIG) --libs libcurl 2>/dev/null)
CODUOMP_MINIZIP_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags minizip 2>/dev/null)
CODUOMP_MINIZIP_LIBS ?= $(shell $(PKG_CONFIG) --libs minizip 2>/dev/null)
ifeq ($(shell uname -s),Darwin)
CODUOMP_NATIVE_PLATFORM_LIBS := -framework OpenGL -framework CoreGraphics \
	-framework AppKit -framework Foundation
ifneq ($(CODUOMP_DISABLE_AUDIO),1)
CODUOMP_NATIVE_PLATFORM_LIBS += -framework OpenAL -framework AudioToolbox \
	-framework CoreAudio -framework AudioUnit -framework CoreFoundation
endif
CODUOMP_MACOS_OBJC_SOURCES := \
	src/client/engine/platform/macos_app_bundle.m
else
ifeq ($(CODUOMP_DISABLE_AUDIO),1)
CODUOMP_NATIVE_PLATFORM_LIBS := -lm
else
CODUOMP_NATIVE_PLATFORM_LIBS := $(shell $(PKG_CONFIG) --libs openal sndfile 2>/dev/null) -lm -ldl -pthread
endif
CODUOMP_MACOS_OBJC_SOURCES :=
endif
CODUOMP_NATIVE_WIDTH_WARNINGS := -Wshorten-64-to-32 \
	-Wpointer-to-int-cast -Wint-to-pointer-cast
CODUOMP_INTEGER_FLAGS := -fwrapv
# The original i386 renderer evaluates x87 multiply/add/subtract instructions
# separately.  Apple Clang contracts even -O0 expressions by default, which
# otherwise turns recovered operation graphs into arm64 FMADD/FMSUB sequences.
CODUOMP_FLOAT_FLAGS := -ffp-contract=off
# Keep tentative globals in normal zero-fill storage.  Darwin ld64 otherwise
# treats the native renderer's 0xf00098-byte tess global as a large COMMON
# allocation and requests 32 KiB section alignment, above arm64 Mach-O's
# 16 KiB segment limit.  This does not change the global's declared alignment,
# layout, or zero-initialization semantics.
CODUOMP_STORAGE_FLAGS := -fno-common
CODUOMP_NATIVE_BUILD_DIR ?= .workbench/build/client-engine/native-stock
CODUOMP_NATIVE_EXECUTABLE ?= $(CODUOMP_NATIVE_BUILD_DIR)/CoDUOMP
CODUOMP_INCLUDE_FLAGS := -Isrc \
	-iquote src/client/engine/bindings \
	-iquote src/qcommon/bindings/default
CODUOMP_NATIVE_CFLAGS := -g -O0 $(CODUOMP_INTEGER_FLAGS) \
	$(CODUOMP_FLOAT_FLAGS) $(CODUOMP_STORAGE_FLAGS) \
	$(CLIENT_FP_CPPFLAGS) \
	$(RECOVERY_POLICY_CPPFLAGS) \
	$(CODUOMP_AUDIO_CPPFLAGS) \
	$(PLATFORM_BEHAVIOR_CPPFLAGS) \
	$(CODUOMP_INCLUDE_FLAGS) \
	$(CODUOMP_JPEG_CFLAGS) $(CODUOMP_SDL_CFLAGS) \
	$(CODUOMP_CURL_CFLAGS) $(CODUOMP_MINIZIP_CFLAGS) \
	-MMD -MP
CODUOMP_NATIVE_C_OBJECTS := $(patsubst src/client/engine/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/%.c.o,$(CODUOMP_C_SOURCES)) \
	$(patsubst $(CODUOMP_WINDOWS_GENERATED_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/generated-windows/%.c.o,$(CODUOMP_WINDOWS_GENERATED_C_SOURCES)) \
	$(patsubst $(SHARED_CLIENT_COMMON_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/client/common/%.c.o,$(SHARED_CLIENT_COMMON_C_SOURCES)) \
	$(patsubst $(CLIENT_MODULE_MATH_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/client/math/%.c.o,$(CODUOMP_CLIENT_MATH_C_SOURCES)) \
	$(patsubst $(SHARED_QCOMMON_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/qcommon/%.c.o,$(SHARED_QCOMMON_C_SOURCES)) \
	$(patsubst $(SHARED_CRT_COMPAT_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/compat/crt/%.c.o,$(SHARED_CRT_COMPAT_C_SOURCES)) \
	$(patsubst $(SHARED_MATH_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/math/%.c.o,$(CODUOMP_SHARED_MATH_C_SOURCES)) \
	$(patsubst $(SHARED_SOUND_ALIAS_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/sound/alias/%.c.o,$(SHARED_SOUND_ALIAS_C_SOURCES)) \
	$(patsubst $(SHARED_ANIMATION_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/animation/%.c.o,$(SHARED_ANIMATION_C_SOURCES)) \
	$(patsubst $(SHARED_COLLISION_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/collision/%.c.o,$(SHARED_COLLISION_C_SOURCES)) \
	$(patsubst $(SHARED_FILESYSTEM_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/filesystem/%.c.o,$(SHARED_FILESYSTEM_C_SOURCES)) \
	$(patsubst $(SHARED_SERVER_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/shared-server/%.c.o,$(SHARED_SERVER_C_SOURCES)) \
	$(patsubst $(SHARED_SCRIPT_DIR)/%.c,$(CODUOMP_NATIVE_BUILD_DIR)/src/scripting/%.c.o,$(SHARED_SCRIPT_C_SOURCES))
CODUOMP_NATIVE_CXX_OBJECTS := $(patsubst src/client/engine/%.cpp,$(CODUOMP_NATIVE_BUILD_DIR)/%.cpp.o,$(CODUOMP_CXX_SOURCES)) \
	$(patsubst $(SHARED_SCRIPT_DIR)/%.cpp,$(CODUOMP_NATIVE_BUILD_DIR)/src/scripting/%.cpp.o,$(SHARED_SCRIPT_CXX_SOURCES))
CODUOMP_NATIVE_OBJC_OBJECTS := $(patsubst src/client/engine/%.m,$(CODUOMP_NATIVE_BUILD_DIR)/%.m.o,$(CODUOMP_MACOS_OBJC_SOURCES))
ifeq ($(shell uname -s),Darwin)
CLIENT_NATIVE_LIBRARY := .workbench/build/cgame/native/libcoduo_cgame.dylib
CLIENT_NATIVE_LDFLAGS := -dynamiclib
else
CLIENT_NATIVE_LIBRARY := .workbench/build/cgame/native/libcoduo_cgame.so
CLIENT_NATIVE_LDFLAGS := -shared -Wl,--no-undefined
endif
# The original i386 cgame evaluates the recovered view-origin and view-axis
# multiply/add/subtract graphs as separate x87 instructions.  Apple Clang
# otherwise contracts those expressions into arm64 FMADD/FMSUB instructions.
CLIENT_NATIVE_FLOAT_FLAGS := -ffp-contract=off
client-native-link:
	@mkdir -p $(dir $(CLIENT_NATIVE_LIBRARY))
	$(CC) -std=c11 $(CLIENT_FP_CPPFLAGS) $(RECOVERY_POLICY_CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) -g -O0 $(CLIENT_NATIVE_FLOAT_FLAGS) -fno-common -fPIC -fvisibility=hidden $(CLIENT_INCLUDE_FLAGS) $(CLIENT_ALL_C_SOURCES) $(CLIENT_NATIVE_LDFLAGS) -lm -o $(CLIENT_NATIVE_LIBRARY)

client-windows-cross-link:
	@mkdir -p $(dir $(CLIENT_WINDOWS_LIBRARY))
	$(CLIENT_WINDOWS_CC) -std=c11 $(CLIENT_FP_CPPFLAGS) $(RECOVERY_POLICY_CPPFLAGS) -DWINDOWS_BEHAVIOR $(CLIENT_INCLUDE_FLAGS) $(CLIENT_ALL_C_SOURCES) -shared -static-libgcc $(CLIENT_WINDOWS_DEF) -Wl,--no-undefined -lm -o $(CLIENT_WINDOWS_LIBRARY)

client-win32-abi-link:
	@mkdir -p $(dir $(CLIENT_WIN32_LIBRARY))
	$(CLIENT_WIN32_CC) -std=c11 $(CLIENT_FP_CPPFLAGS) $(RECOVERY_POLICY_CPPFLAGS) -DWINDOWS_BEHAVIOR $(CLIENT_INCLUDE_FLAGS) $(CLIENT_ALL_C_SOURCES) -shared -static-libgcc $(CLIENT_WINDOWS_DEF) -Wl,--no-undefined -lm -o $(CLIENT_WIN32_LIBRARY)

# Compiler-policy changes in these files must invalidate existing native
# objects.  Dependency files cover source/header changes but do not record the
# command-line flags that produced an object.
CODUOMP_NATIVE_CONFIG_INPUTS := build-mk/client-targets.mk \
	build-mk/sources/client-engine.mk \
	build-mk/platform-behavior.mk
$(CODUOMP_NATIVE_C_OBJECTS) $(CODUOMP_NATIVE_CXX_OBJECTS) \
	$(CODUOMP_NATIVE_OBJC_OBJECTS): \
	$(CODUOMP_NATIVE_CONFIG_INPUTS)

$(CODUOMP_NATIVE_BUILD_DIR)/%.c.o: src/client/engine/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) \
		$(CODUOMP_NATIVE_C_OPTIMIZATION_FLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/generated-windows/%.c.o: $(CODUOMP_WINDOWS_GENERATED_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/qcommon/%.c.o: $(SHARED_QCOMMON_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/compat/crt/%.c.o: $(SHARED_CRT_COMPAT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/client/common/%.c.o: $(SHARED_CLIENT_COMMON_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/client/math/%.c.o: $(CLIENT_MODULE_MATH_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/math/%.c.o: $(SHARED_MATH_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) \
		$(CODUOMP_NATIVE_C_OPTIMIZATION_FLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/sound/alias/%.c.o: $(SHARED_SOUND_ALIAS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/animation/%.c.o: $(SHARED_ANIMATION_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) \
		$(CODUOMP_NATIVE_C_OPTIMIZATION_FLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/collision/%.c.o: $(SHARED_COLLISION_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) \
		$(CODUOMP_NATIVE_C_OPTIMIZATION_FLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/filesystem/%.c.o: $(SHARED_FILESYSTEM_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/shared-server/%.c.o: $(SHARED_SERVER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/scripting/%.c.o: $(SHARED_SCRIPT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/%.cpp.o: src/client/engine/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/%.m.o: src/client/engine/%.m
	@mkdir -p $(dir $@)
	$(CC) $(CODUOMP_NATIVE_CFLAGS) $(CODUOMP_NATIVE_OBJC_OPTIMIZATION_FLAGS) -fobjc-arc -c $< -o $@

$(CODUOMP_NATIVE_BUILD_DIR)/src/scripting/%.cpp.o: $(SHARED_SCRIPT_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 $(CODUOMP_NATIVE_CFLAGS) -c $< -o $@

coduomp-native-link: $(CODUOMP_NATIVE_C_OBJECTS) $(CODUOMP_NATIVE_CXX_OBJECTS) \
	$(CODUOMP_NATIVE_OBJC_OBJECTS)
	$(CXX) $(CODUOMP_NATIVE_C_OBJECTS) $(CODUOMP_NATIVE_CXX_OBJECTS) \
		$(CODUOMP_NATIVE_OBJC_OBJECTS) \
		$(CODUOMP_JPEG_LIBS) $(CODUOMP_SDL_LIBS) \
		$(CODUOMP_CURL_LIBS) $(CODUOMP_MINIZIP_LIBS) -lz \
		$(CODUOMP_NATIVE_PLATFORM_LIBS) \
		-o $(CODUOMP_NATIVE_EXECUTABLE)
-include $(CODUOMP_NATIVE_C_OBJECTS:.o=.d)
-include $(CODUOMP_NATIVE_CXX_OBJECTS:.o=.d)
-include $(CODUOMP_NATIVE_OBJC_OBJECTS:.o=.d)
