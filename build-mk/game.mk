CC ?= cc
AR ?= ar

include build-mk/sources/game.mk

BUILD_DIR ?= .workbench/build/game/objects
TARGET := $(BUILD_DIR)/libgame_mp_uo_recovered.a
SHARED_BUILD_DIR ?= .workbench/build/game/shared
SHARED_TARGET := $(SHARED_BUILD_DIR)/game.mp.uo.x86_64.so
NATIVE64_BUILD_DIR ?= .workbench/build/game/native64
NATIVE64_TARGET := $(NATIVE64_BUILD_DIR)/libgame_mp_uo_recovered.a
NATIVE64_SHARED_BUILD_DIR ?= .workbench/build/game/native64-shared
NATIVE64_SHARED_TARGET := $(NATIVE64_SHARED_BUILD_DIR)/game.mp.uo.x86_64.so
I386_SHARED_BUILD_DIR ?= .workbench/build/game/linux-i386
I386_SHARED_TARGET ?= $(I386_SHARED_BUILD_DIR)/game.mp.uo.i386.so
I386_CFLAGS ?= -m32 -march=i386
I386_LDFLAGS ?= -m32
# Stock i386 uses x87 implicitly, but it still needs the same per-assignment
# binary32 spills and unoptimized local-store shape as the original build.
# -march=i386 also preserves the retail status-word comparison lowering instead
# of allowing modern GCC to select the later FCOMI/FUCOMI instruction family.
I386_FLOAT_FLAGS := -O0 -fexcess-precision=fast
MINGW32_CC ?= i686-w64-mingw32-gcc
MINGW64_CC ?= x86_64-w64-mingw32-gcc
WINDOWS_I386_BUILD_DIR ?= .workbench/build/game/windows-i386
WINDOWS_I386_TARGET ?= $(WINDOWS_I386_BUILD_DIR)/uo_game_mp_x86.dll
WINDOWS_I686_BUILD_DIR ?= .workbench/build/game/windows-i686
WINDOWS_I686_TARGET ?= $(WINDOWS_I686_BUILD_DIR)/uo_game_mp_x86.dll
WINDOWS_X86_64_BUILD_DIR ?= .workbench/build/game/windows-x86_64
WINDOWS_X86_64_TARGET ?= $(WINDOWS_X86_64_BUILD_DIR)/uo_game_mp_x86.dll
WINDOWS_I386_CFLAGS ?= -march=i386
WINDOWS_I686_CFLAGS ?= -march=i686
WINDOWS_X86_64_CFLAGS ?= -m64
WINDOWS_FLOAT_FLAGS := -O0 -mfpmath=387 -fexcess-precision=fast
WINDOWS_X86_64_FLOAT_FLAGS := -O0 -mfpmath=387 -fexcess-precision=fast
WINDOWS_LDFLAGS ?= -shared -static-libgcc
WINDOWS_LIBS ?= -lm
WINDOWS_MODULE_DEF := build-mk/abi/game/uo_game_mp_x86.def
GAME_DIR := src/server/game
SRCS := $(GAME_C_SOURCES)
QCOMMON_DIR := src/qcommon
QCOMMON_SRCS := $(QCOMMON_DIR)/com_parse.c \
	$(QCOMMON_DIR)/com_sprintf.c \
	$(QCOMMON_DIR)/info.c \
	$(QCOMMON_DIR)/q_bits.c \
	$(QCOMMON_DIR)/q_endian.c \
	$(QCOMMON_DIR)/q_localized_float.c \
	$(QCOMMON_DIR)/q_path.c \
	$(QCOMMON_DIR)/q_shared_misc.c \
	$(QCOMMON_DIR)/q_string.c \
	$(QCOMMON_DIR)/q_temp.c
CRT_COMPAT_DIR := src/compat/crt
CRT_COMPAT_SRCS := $(LAYOUT_CRT_COMPAT_C_SOURCES)
MATH_DIR := src/math
MATH_SRCS := $(MATH_DIR)/q_math.c \
	$(MATH_DIR)/box_distance.c \
	$(MATH_DIR)/q_rint.c \
	$(MATH_DIR)/vector_rotate_angles.c \
	$(MATH_DIR)/round_float.c \
	$(MATH_DIR)/lean_math.c \
	$(MATH_DIR)/angle_vectors.c \
	$(MATH_DIR)/angle_mod.c \
	$(MATH_DIR)/angle_normalization.c \
	$(MATH_DIR)/angle_delta.c \
	$(MATH_DIR)/angle_subtract.c \
	$(MATH_DIR)/radius_from_bounds.c \
	$(MATH_DIR)/color_math.c \
	$(MATH_DIR)/vector_normalization_geometry.c \
	$(MATH_DIR)/direction_angles.c \
	$(MATH_DIR)/axis_to_angles.c \
	$(MATH_DIR)/angles_to_axis.c \
	$(MATH_DIR)/pitch_for_yaw_on_normal.c \
	$(MATH_DIR)/vector_snap.c \
	$(MATH_DIR)/orientation_transform.c \
	$(MATH_DIR)/matrix_transform_vector43_equals.c \
	$(MATH_DIR)/vector_compare_epsilon.c \
	$(MATH_DIR)/cross_product.c \
	$(MATH_DIR)/quat_multiply.c \
	$(MATH_DIR)/byte_directions.c \
	$(MATH_DIR)/set_plane_signbits.c \
	$(MATH_DIR)/matrix_transpose_transform_vector.c \
	$(MATH_DIR)/matrix_transform_vector43.c \
	$(MATH_DIR)/dobj_skel_matrix_transform_vector43.c \
	$(MATH_DIR)/dobj_skel_matrix.c \
	$(MATH_DIR)/matrix_inverse.c \
	$(MATH_DIR)/matrix_inverse44.c \
	$(MATH_DIR)/matrix_multiply34.c \
	$(MATH_DIR)/matrix_multiply.c \
	$(MATH_DIR)/matrix_multiply43.c \
	$(MATH_DIR)/matrix_transform_vector.c \
	$(MATH_DIR)/vector_rotation.c \
	$(MATH_DIR)/normal_to_latlong.c \
	$(MATH_DIR)/vector_polar.c \
	$(MATH_DIR)/q_random.c \
	$(MATH_DIR)/q_float_primitives.c \
	$(MATH_DIR)/vector_angle_multiply.c \
	$(MATH_DIR)/axis_quaternion.c \
	$(MATH_DIR)/convert_quat_to_mat.c \
	$(MATH_DIR)/quaternion_trace.c \
	$(MATH_DIR)/q_shared_random.c \
	$(MATH_DIR)/vector_lane_ops.c \
	$(MATH_DIR)/vector_projection.c
BG_DIR := src/bg
BG_SRCS := $(LAYOUT_BG_C_SOURCES)
CODUO_DEFAULT_BEHAVIOR := linux
include build-mk/platform-behavior.mk

# Shared reconstruction of the CRT math routines (src/compat/libm). Built from
# here so the server carries no dependency on the platform libm; see
# docs/x87-transcendental-reconstruction-scope.md.
LIBM_DIR := src/compat/libm
LIBM_SRCS := $(LAYOUT_LIBM_C_SOURCES)

OBJS := $(patsubst $(GAME_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS)) \
        $(patsubst $(LIBM_DIR)/%.c,$(BUILD_DIR)/libm_%.o,$(LIBM_SRCS)) \
	$(patsubst $(CRT_COMPAT_DIR)/%.c,$(BUILD_DIR)/crt_compat_%.o,$(CRT_COMPAT_SRCS)) \
        $(patsubst $(QCOMMON_DIR)/%.c,$(BUILD_DIR)/qcommon_%.o,$(QCOMMON_SRCS)) \
        $(patsubst $(MATH_DIR)/%.c,$(BUILD_DIR)/math_%.o,$(MATH_SRCS)) \
        $(patsubst $(BG_DIR)/%.c,$(BUILD_DIR)/bg_%.o,$(BG_SRCS))
SHARED_OBJS := $(patsubst $(GAME_DIR)/%.c,$(SHARED_BUILD_DIR)/%.o,$(SRCS)) \
        $(patsubst $(LIBM_DIR)/%.c,$(SHARED_BUILD_DIR)/libm_%.o,$(LIBM_SRCS)) \
	$(patsubst $(CRT_COMPAT_DIR)/%.c,$(SHARED_BUILD_DIR)/crt_compat_%.o,$(CRT_COMPAT_SRCS)) \
        $(patsubst $(QCOMMON_DIR)/%.c,$(SHARED_BUILD_DIR)/qcommon_%.o,$(QCOMMON_SRCS)) \
        $(patsubst $(MATH_DIR)/%.c,$(SHARED_BUILD_DIR)/math_%.o,$(MATH_SRCS)) \
        $(patsubst $(BG_DIR)/%.c,$(SHARED_BUILD_DIR)/bg_%.o,$(BG_SRCS))
NATIVE64_OBJS := $(patsubst $(GAME_DIR)/%.c,$(NATIVE64_BUILD_DIR)/%.o,$(SRCS)) \
        $(patsubst $(LIBM_DIR)/%.c,$(NATIVE64_BUILD_DIR)/libm_%.o,$(LIBM_SRCS)) \
	$(patsubst $(CRT_COMPAT_DIR)/%.c,$(NATIVE64_BUILD_DIR)/crt_compat_%.o,$(CRT_COMPAT_SRCS)) \
        $(patsubst $(QCOMMON_DIR)/%.c,$(NATIVE64_BUILD_DIR)/qcommon_%.o,$(QCOMMON_SRCS)) \
        $(patsubst $(MATH_DIR)/%.c,$(NATIVE64_BUILD_DIR)/math_%.o,$(MATH_SRCS)) \
        $(patsubst $(BG_DIR)/%.c,$(NATIVE64_BUILD_DIR)/bg_%.o,$(BG_SRCS))
NATIVE64_SHARED_OBJS := $(patsubst $(GAME_DIR)/%.c,$(NATIVE64_SHARED_BUILD_DIR)/%.o,$(SRCS)) \
        $(patsubst $(LIBM_DIR)/%.c,$(NATIVE64_SHARED_BUILD_DIR)/libm_%.o,$(LIBM_SRCS)) \
	$(patsubst $(CRT_COMPAT_DIR)/%.c,$(NATIVE64_SHARED_BUILD_DIR)/crt_compat_%.o,$(CRT_COMPAT_SRCS)) \
        $(patsubst $(QCOMMON_DIR)/%.c,$(NATIVE64_SHARED_BUILD_DIR)/qcommon_%.o,$(QCOMMON_SRCS)) \
        $(patsubst $(MATH_DIR)/%.c,$(NATIVE64_SHARED_BUILD_DIR)/math_%.o,$(MATH_SRCS)) \
        $(patsubst $(BG_DIR)/%.c,$(NATIVE64_SHARED_BUILD_DIR)/bg_%.o,$(BG_SRCS))
I386_SHARED_OBJS := $(patsubst $(GAME_DIR)/%.c,$(I386_SHARED_BUILD_DIR)/%.o,$(SRCS)) \
        $(patsubst $(LIBM_DIR)/%.c,$(I386_SHARED_BUILD_DIR)/libm_%.o,$(LIBM_SRCS)) \
	$(patsubst $(CRT_COMPAT_DIR)/%.c,$(I386_SHARED_BUILD_DIR)/crt_compat_%.o,$(CRT_COMPAT_SRCS)) \
        $(patsubst $(QCOMMON_DIR)/%.c,$(I386_SHARED_BUILD_DIR)/qcommon_%.o,$(QCOMMON_SRCS)) \
        $(patsubst $(MATH_DIR)/%.c,$(I386_SHARED_BUILD_DIR)/math_%.o,$(MATH_SRCS)) \
        $(patsubst $(BG_DIR)/%.c,$(I386_SHARED_BUILD_DIR)/bg_%.o,$(BG_SRCS))
WINDOWS_I386_OBJS := $(patsubst $(GAME_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/%.o,$(SRCS)) \
        $(patsubst $(LIBM_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/libm_%.o,$(LIBM_SRCS)) \
	$(patsubst $(CRT_COMPAT_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/crt_compat_%.o,$(CRT_COMPAT_SRCS)) \
        $(patsubst $(QCOMMON_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/qcommon_%.o,$(QCOMMON_SRCS)) \
        $(patsubst $(MATH_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/math_%.o,$(MATH_SRCS)) \
        $(patsubst $(BG_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/bg_%.o,$(BG_SRCS))
WINDOWS_I686_OBJS := $(patsubst $(GAME_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/%.o,$(SRCS)) \
        $(patsubst $(LIBM_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/libm_%.o,$(LIBM_SRCS)) \
	$(patsubst $(CRT_COMPAT_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/crt_compat_%.o,$(CRT_COMPAT_SRCS)) \
        $(patsubst $(QCOMMON_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/qcommon_%.o,$(QCOMMON_SRCS)) \
        $(patsubst $(MATH_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/math_%.o,$(MATH_SRCS)) \
        $(patsubst $(BG_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/bg_%.o,$(BG_SRCS))
WINDOWS_X86_64_SRCS := $(SRCS) $(LIBM_SRCS) $(CRT_COMPAT_SRCS) \
	$(QCOMMON_SRCS) $(MATH_SRCS) $(BG_SRCS)
DEPS := $(OBJS:.o=.d)
SHARED_DEPS := $(SHARED_OBJS:.o=.d)
NATIVE64_DEPS := $(NATIVE64_OBJS:.o=.d)
NATIVE64_SHARED_DEPS := $(NATIVE64_SHARED_OBJS:.o=.d)
I386_SHARED_DEPS := $(I386_SHARED_OBJS:.o=.d)
WINDOWS_I386_DEPS := $(WINDOWS_I386_OBJS:.o=.d)
WINDOWS_I686_DEPS := $(WINDOWS_I686_OBJS:.o=.d)

CPPFLAGS += -iquote $(GAME_DIR)/bindings \
	-iquote src/qcommon/bindings/default \
	-Isrc \
            -Ivendor/softfloat/source/include
# Stock i386 uses signed plain char, 32-bit enum-backed domains, and wrapping
# ADD/SUB/IMUL semantics. GCC, Clang, and MinGW support explicit switches for
# those rules, preventing host defaults or inherited flags from changing them.
CFLAGS += -std=c99 -Wall -Wextra -Werror -pedantic -fsigned-char \
	-fno-short-enums -fwrapv -MMD -MP

# x87 floating-point faithfulness policy (shared with the engine, and the single
# source of truth for FP faithfulness across both). Governs whether the x86-64
# library reproduces the original's x87 float rounding (-mfpmath=387 on GCC/x86)
# or is an acknowledged non-exact build. See build-mk/x87-policy.mk and
# docs/components/server/fp-faithfulness.md.
X87_POLICY_ARCH := $(shell uname -m)
X87_POLICY_CC := $(CC)
include build-mk/x87-policy.mk
# A relaxed (acknowledged non-exact) build also relaxes the source-level x87
# long-double equivalence assert in recovered_game.h, since a non-x87 host
# cannot satisfy it. (This is the axis the retired NATIVE64_FP_MODE knob used to
# control; it is now derived from CODUO_FP_FAITHFUL like everything else.)
ifeq ($(CODUO_FP_FAITHFUL),relaxed)
X87_ALLOW_CPPFLAGS += -DGAME_REQUIRE_X87_LONG_DOUBLE=0 -DGAME_ALLOW_NON_EQUIVALENT_LONG_DOUBLE=1
endif

# Non-x86 hosts select the software-x87 source bodies automatically. Their
# native shared-library links therefore need the matching host SoftFloat
# archive; original i386 and Windows targets continue to use native x87.
ifeq ($(X87_POLICY_IS_X86),0)
NATIVE_SOFTFLOAT_LIBRARY ?= vendor/softfloat/build/coduo-x87/softfloat.a
else
NATIVE_SOFTFLOAT_LIBRARY ?=
endif

SHARED_CFLAGS += -fPIC
SHARED_LD ?= $(CC)
SHARED_LDFLAGS += -shared
SHARED_LIBS += -lm
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
STRICT_UNDEFINED_LDFLAGS ?= -Wl,-undefined,error
else
STRICT_UNDEFINED_LDFLAGS ?= -Wl,--no-undefined
endif

.PHONY: all check shared shared-check shared-check-link shared-i386 \
	native64-check native64-shared windows windows-i386 windows-i686 \
	windows-x86_64

all: $(TARGET)

check: all

shared: $(SHARED_TARGET)

shared-i386: $(I386_SHARED_TARGET)

native64-check: $(NATIVE64_TARGET)

native64-shared: $(NATIVE64_SHARED_TARGET)

windows: windows-i686

windows-i386: $(WINDOWS_I386_TARGET)

windows-i686: $(WINDOWS_I686_TARGET)

windows-x86_64: $(WINDOWS_X86_64_TARGET)

shared-check: shared-check-link

shared-check-link: override SHARED_LDFLAGS += $(STRICT_UNDEFINED_LDFLAGS)
shared-check-link: $(SHARED_OBJS)
	$(SHARED_LD) $(SHARED_LDFLAGS) -o $(SHARED_TARGET) $^ \
		$(NATIVE_SOFTFLOAT_LIBRARY) $(SHARED_LIBS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SHARED_BUILD_DIR):
	mkdir -p $(SHARED_BUILD_DIR)

$(NATIVE64_BUILD_DIR):
	mkdir -p $(NATIVE64_BUILD_DIR)

$(NATIVE64_SHARED_BUILD_DIR):
	mkdir -p $(NATIVE64_SHARED_BUILD_DIR)

$(I386_SHARED_BUILD_DIR):
	mkdir -p $(I386_SHARED_BUILD_DIR)

$(WINDOWS_I386_BUILD_DIR):
	mkdir -p $(WINDOWS_I386_BUILD_DIR)

$(WINDOWS_I686_BUILD_DIR):
	mkdir -p $(WINDOWS_I686_BUILD_DIR)

$(WINDOWS_X86_64_BUILD_DIR):
	mkdir -p $(WINDOWS_X86_64_BUILD_DIR)

$(BUILD_DIR)/%.o: $(GAME_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(SHARED_BUILD_DIR)/%.o: $(GAME_DIR)/%.c | $(SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_BUILD_DIR)/%.o: $(GAME_DIR)/%.c | $(NATIVE64_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_SHARED_BUILD_DIR)/%.o: $(GAME_DIR)/%.c | $(NATIVE64_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

# 32-bit x86 already uses x87 for all float math. I386_FLOAT_FLAGS still
# supplies the separately required stock spill/rounding policy.
$(I386_SHARED_BUILD_DIR)/%.o: $(GAME_DIR)/%.c | $(I386_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(I386_CFLAGS) \
		$(I386_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/%.o: $(GAME_DIR)/%.c | $(WINDOWS_I386_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I386_CFLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/%.o: $(GAME_DIR)/%.c | $(WINDOWS_I686_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I686_CFLAGS) -c $< -o $@

# Shared libm objects, prefixed so they cannot collide with src/ basenames.
$(BUILD_DIR)/libm_%.o: $(LIBM_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(SHARED_BUILD_DIR)/libm_%.o: $(LIBM_DIR)/%.c | $(SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_BUILD_DIR)/libm_%.o: $(LIBM_DIR)/%.c | $(NATIVE64_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_SHARED_BUILD_DIR)/libm_%.o: $(LIBM_DIR)/%.c | $(NATIVE64_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(I386_SHARED_BUILD_DIR)/libm_%.o: $(LIBM_DIR)/%.c | $(I386_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(I386_CFLAGS) \
		$(I386_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/libm_%.o: $(LIBM_DIR)/%.c | $(WINDOWS_I386_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I386_CFLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/libm_%.o: $(LIBM_DIR)/%.c | $(WINDOWS_I686_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I686_CFLAGS) -c $< -o $@

# Shared CRT compatibility objects, kept separate from qcommon ownership.
$(BUILD_DIR)/crt_compat_%.o: $(CRT_COMPAT_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(SHARED_BUILD_DIR)/crt_compat_%.o: $(CRT_COMPAT_DIR)/%.c | $(SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_BUILD_DIR)/crt_compat_%.o: $(CRT_COMPAT_DIR)/%.c | $(NATIVE64_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_SHARED_BUILD_DIR)/crt_compat_%.o: $(CRT_COMPAT_DIR)/%.c | $(NATIVE64_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(I386_SHARED_BUILD_DIR)/crt_compat_%.o: $(CRT_COMPAT_DIR)/%.c | $(I386_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(I386_CFLAGS) \
		$(I386_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/crt_compat_%.o: $(CRT_COMPAT_DIR)/%.c | $(WINDOWS_I386_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I386_CFLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/crt_compat_%.o: $(CRT_COMPAT_DIR)/%.c | $(WINDOWS_I686_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I686_CFLAGS) -c $< -o $@

# Shared qcommon objects, prefixed so they cannot collide with src/ basenames.
$(BUILD_DIR)/qcommon_%.o: $(QCOMMON_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(SHARED_BUILD_DIR)/qcommon_%.o: $(QCOMMON_DIR)/%.c | $(SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_BUILD_DIR)/qcommon_%.o: $(QCOMMON_DIR)/%.c | $(NATIVE64_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_SHARED_BUILD_DIR)/qcommon_%.o: $(QCOMMON_DIR)/%.c | $(NATIVE64_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(I386_SHARED_BUILD_DIR)/qcommon_%.o: $(QCOMMON_DIR)/%.c | $(I386_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(I386_CFLAGS) \
		$(I386_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/qcommon_%.o: $(QCOMMON_DIR)/%.c | $(WINDOWS_I386_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I386_CFLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/qcommon_%.o: $(QCOMMON_DIR)/%.c | $(WINDOWS_I686_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I686_CFLAGS) -c $< -o $@

# Shared math objects, prefixed so they cannot collide with src/ basenames.
$(BUILD_DIR)/math_%.o: $(MATH_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(SHARED_BUILD_DIR)/math_%.o: $(MATH_DIR)/%.c | $(SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_BUILD_DIR)/math_%.o: $(MATH_DIR)/%.c | $(NATIVE64_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_SHARED_BUILD_DIR)/math_%.o: $(MATH_DIR)/%.c | $(NATIVE64_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(I386_SHARED_BUILD_DIR)/math_%.o: $(MATH_DIR)/%.c | $(I386_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(I386_CFLAGS) \
		$(I386_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/math_%.o: $(MATH_DIR)/%.c | $(WINDOWS_I386_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I386_CFLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/math_%.o: $(MATH_DIR)/%.c | $(WINDOWS_I686_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I686_CFLAGS) -c $< -o $@

# Shared BG objects, prefixed so they cannot collide with src/ basenames.
$(BUILD_DIR)/bg_%.o: $(BG_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(SHARED_BUILD_DIR)/bg_%.o: $(BG_DIR)/%.c | $(SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_BUILD_DIR)/bg_%.o: $(BG_DIR)/%.c | $(NATIVE64_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(NATIVE64_SHARED_BUILD_DIR)/bg_%.o: $(BG_DIR)/%.c | $(NATIVE64_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(X87_ALLOW_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(X87_FLOAT_FLAGS) -c $< -o $@

$(I386_SHARED_BUILD_DIR)/bg_%.o: $(BG_DIR)/%.c | $(I386_SHARED_BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(I386_CFLAGS) \
		$(I386_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/bg_%.o: $(BG_DIR)/%.c | $(WINDOWS_I386_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I386_CFLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/bg_%.o: $(BG_DIR)/%.c | $(WINDOWS_I686_BUILD_DIR)
	$(MINGW32_CC) $(CPPFLAGS) $(PLATFORM_BEHAVIOR_CPPFLAGS) $(CFLAGS) $(WINDOWS_FLOAT_FLAGS) \
		$(WINDOWS_I686_CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

$(SHARED_TARGET): $(SHARED_OBJS)
	$(SHARED_LD) $(SHARED_LDFLAGS) -o $@ $^ \
		$(NATIVE_SOFTFLOAT_LIBRARY) $(SHARED_LIBS)

$(NATIVE64_TARGET): $(NATIVE64_OBJS)
	$(AR) rcs $@ $^

$(NATIVE64_SHARED_TARGET): $(NATIVE64_SHARED_OBJS)
	$(SHARED_LD) $(SHARED_LDFLAGS) -o $@ $^ \
		$(NATIVE_SOFTFLOAT_LIBRARY) $(SHARED_LIBS)

$(I386_SHARED_TARGET): $(I386_SHARED_OBJS)
	$(SHARED_LD) $(I386_LDFLAGS) $(SHARED_LDFLAGS) -o $@ $^ $(SHARED_LIBS)

$(WINDOWS_I386_TARGET): $(WINDOWS_I386_OBJS) $(WINDOWS_MODULE_DEF)
	$(MINGW32_CC) $(WINDOWS_LDFLAGS) $(WINDOWS_I386_CFLAGS) \
		-o $@ $(WINDOWS_I386_OBJS) $(WINDOWS_MODULE_DEF) $(WINDOWS_LIBS)

$(WINDOWS_I686_TARGET): $(WINDOWS_I686_OBJS) $(WINDOWS_MODULE_DEF)
	$(MINGW32_CC) $(WINDOWS_LDFLAGS) $(WINDOWS_I686_CFLAGS) \
		-o $@ $(WINDOWS_I686_OBJS) $(WINDOWS_MODULE_DEF) $(WINDOWS_LIBS)

$(WINDOWS_X86_64_TARGET): $(WINDOWS_X86_64_SRCS) $(WINDOWS_MODULE_DEF) | $(WINDOWS_X86_64_BUILD_DIR)
	$(MINGW64_CC) $(CPPFLAGS) -DWINDOWS_BEHAVIOR \
		$(filter-out -MMD -MP,$(CFLAGS)) $(WINDOWS_X86_64_FLOAT_FLAGS) \
		$(WINDOWS_X86_64_CFLAGS) $(WINDOWS_X86_64_SRCS) \
		$(WINDOWS_LDFLAGS) -Wl,--no-undefined \
		-o $@ $(WINDOWS_MODULE_DEF) $(WINDOWS_LIBS)

-include $(DEPS) $(SHARED_DEPS) $(NATIVE64_DEPS) $(NATIVE64_SHARED_DEPS) \
	$(I386_SHARED_DEPS) $(WINDOWS_I386_DEPS) $(WINDOWS_I686_DEPS)
