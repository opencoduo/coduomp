CC ?= cc
CXX ?= c++
AR ?= ar

include build-mk/sources/server-engine.mk

BUILD_DIR ?= .workbench/build/server/objects
TARGET := $(BUILD_DIR)/libcoduo_lnxded_recovered.a
ifneq ($(filter native-macos-exe,$(MAKECMDGOALS)),)
CODUO_FP_FAITHFUL ?= relaxed
EMULATE_X87 ?= 1
EMU_X87_BACKEND ?= softfloat
NATIVE_MACOS_X87_VARIANT := $(if $(filter softfloat,$(EMU_X87_BACKEND)),,-$(EMU_X87_BACKEND)$(if $(filter 1,$(EMU_X87_EXACT_GEOMETRY)),-exact-geometry))
EXE_BUILD_DIR ?= .workbench/build/server/native-macos-x87$(NATIVE_MACOS_X87_VARIANT)
endif
EXE_BUILD_DIR ?= .workbench/build/server/native
EXE_TARGET := $(EXE_BUILD_DIR)/coduo_lnxded_recovered
EXE_I386_BUILD_DIR ?= .workbench/build/server/linux-i386
EXE_I386_TARGET := $(EXE_I386_BUILD_DIR)/coduo_lnxded_recovered
WINDOWS_I386_BUILD_DIR ?= .workbench/build/server/windows-i386
WINDOWS_I386_TARGET := $(WINDOWS_I386_BUILD_DIR)/coduo_dedicated_recovered.exe
WINDOWS_I686_BUILD_DIR ?= .workbench/build/server/windows-i686
WINDOWS_I686_TARGET := $(WINDOWS_I686_BUILD_DIR)/coduo_dedicated_recovered.exe
CODUO_ENABLE_PUNKBUSTER ?= 0
CODUO_DISABLE_SERVER_AUTH ?= 0
QCOMMON_DIR := src/qcommon
QCOMMON_CSRCS := $(LAYOUT_QCOMMON_C_SOURCES)
CRT_COMPAT_DIR := src/compat/crt
CRT_COMPAT_CSRCS := $(LAYOUT_CRT_COMPAT_C_SOURCES)
MATH_DIR := src/math
MATH_CSRCS := $(filter-out \
	$(MATH_DIR)/local_matrix_transform_vector43.c, \
	$(LAYOUT_MATH_C_SOURCES))
SOUND_ALIAS_DIR := src/sound/alias
SOUND_ALIAS_CSRCS := $(LAYOUT_SOUND_ALIAS_C_SOURCES)
ANIMATION_DIR := src/animation
ANIMATION_CSRCS := $(LAYOUT_ANIMATION_C_SOURCES)
COLLISION_DIR := src/collision
COLLISION_CSRCS := $(LAYOUT_COLLISION_C_SOURCES)
FILESYSTEM_DIR := src/filesystem
FILESYSTEM_CSRCS := $(LAYOUT_FILESYSTEM_C_SOURCES)
SERVER_DIR := src/server/engine
SERVER_CSRCS := $(LAYOUT_SERVER_ENGINE_C_SOURCES)
SCRIPT_DIR := src/scripting
SCRIPT_CSRCS := $(LAYOUT_SCRIPT_C_SOURCES)
SCRIPT_CXXSRCS := $(LAYOUT_SCRIPT_CXX_SOURCES)
STANDALONE_DIR := src/server/standalone
POSIX_GENERATED_DIR := src/scripting/generated/posix
POSIX_GENERATED_CSRCS := $(LAYOUT_POSIX_GENERATED_C_SOURCES)
CODUO_DEFAULT_BEHAVIOR := linux
include build-mk/platform-behavior.mk
ALL_CSRCS := $(STANDALONE_SERVER_C_SOURCES)
PUNKBUSTER_RECOVERY_SRCS := $(STANDALONE_DIR)/punkbuster/pb_server.c
PUNKBUSTER_DISABLED_SRCS := $(STANDALONE_DIR)/punkbuster/pb_disabled.c
ifeq ($(CODUO_ENABLE_PUNKBUSTER),1)
STANDALONE_CSRCS := $(filter-out $(PUNKBUSTER_DISABLED_SRCS),$(ALL_CSRCS))
else
STANDALONE_CSRCS := $(filter-out $(PUNKBUSTER_RECOVERY_SRCS),$(ALL_CSRCS))
endif
CXXSRCS := $(STANDALONE_SERVER_CXX_SOURCES)
OBJS := $(patsubst $(STANDALONE_DIR)/%.c,$(BUILD_DIR)/%.o,$(STANDALONE_CSRCS)) \
        $(patsubst $(STANDALONE_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CXXSRCS)) \
        $(patsubst $(POSIX_GENERATED_DIR)/%.c,$(BUILD_DIR)/generated-posix/%.o,$(POSIX_GENERATED_CSRCS)) \
        $(patsubst $(QCOMMON_DIR)/%.c,$(BUILD_DIR)/qcommon/%.o,$(QCOMMON_CSRCS)) \
	$(patsubst $(CRT_COMPAT_DIR)/%.c,$(BUILD_DIR)/src/compat/crt/%.o,$(CRT_COMPAT_CSRCS)) \
        $(patsubst $(MATH_DIR)/%.c,$(BUILD_DIR)/math/%.o,$(MATH_CSRCS)) \
        $(patsubst $(SOUND_ALIAS_DIR)/%.c,$(BUILD_DIR)/sound-alias/%.o,$(SOUND_ALIAS_CSRCS)) \
        $(patsubst $(ANIMATION_DIR)/%.c,$(BUILD_DIR)/src/animation/%.o,$(ANIMATION_CSRCS)) \
        $(patsubst $(COLLISION_DIR)/%.c,$(BUILD_DIR)/src/collision/%.o,$(COLLISION_CSRCS)) \
        $(patsubst $(FILESYSTEM_DIR)/%.c,$(BUILD_DIR)/src/filesystem/%.o,$(FILESYSTEM_CSRCS)) \
        $(patsubst $(SERVER_DIR)/%.c,$(BUILD_DIR)/shared-server/%.o,$(SERVER_CSRCS)) \
        $(patsubst $(SCRIPT_DIR)/%.c,$(BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CSRCS)) \
        $(patsubst $(SCRIPT_DIR)/%.cpp,$(BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CXXSRCS))
SHARED_OBJS := $(patsubst $(STANDALONE_DIR)/%.c,$(BUILD_DIR)/shared/%.o,$(STANDALONE_CSRCS)) \
               $(patsubst $(STANDALONE_DIR)/%.cpp,$(BUILD_DIR)/shared/%.o,$(CXXSRCS)) \
               $(patsubst $(POSIX_GENERATED_DIR)/%.c,$(BUILD_DIR)/shared/generated-posix/%.o,$(POSIX_GENERATED_CSRCS)) \
               $(patsubst $(QCOMMON_DIR)/%.c,$(BUILD_DIR)/shared/src/qcommon/%.o,$(QCOMMON_CSRCS)) \
	       $(patsubst $(CRT_COMPAT_DIR)/%.c,$(BUILD_DIR)/shared/src/compat/crt/%.o,$(CRT_COMPAT_CSRCS)) \
               $(patsubst $(MATH_DIR)/%.c,$(BUILD_DIR)/shared/src/math/%.o,$(MATH_CSRCS)) \
               $(patsubst $(SOUND_ALIAS_DIR)/%.c,$(BUILD_DIR)/shared/src/sound/alias/%.o,$(SOUND_ALIAS_CSRCS)) \
               $(patsubst $(ANIMATION_DIR)/%.c,$(BUILD_DIR)/shared/src/animation/%.o,$(ANIMATION_CSRCS)) \
               $(patsubst $(COLLISION_DIR)/%.c,$(BUILD_DIR)/shared/src/collision/%.o,$(COLLISION_CSRCS)) \
               $(patsubst $(FILESYSTEM_DIR)/%.c,$(BUILD_DIR)/shared/src/filesystem/%.o,$(FILESYSTEM_CSRCS)) \
               $(patsubst $(SERVER_DIR)/%.c,$(BUILD_DIR)/shared/shared-server/%.o,$(SERVER_CSRCS)) \
               $(patsubst $(SCRIPT_DIR)/%.c,$(BUILD_DIR)/shared/src/scripting/%.o,$(SCRIPT_CSRCS)) \
               $(patsubst $(SCRIPT_DIR)/%.cpp,$(BUILD_DIR)/shared/src/scripting/%.o,$(SCRIPT_CXXSRCS))
EXE_OBJS := $(patsubst $(STANDALONE_DIR)/%.c,$(EXE_BUILD_DIR)/%.o,$(STANDALONE_CSRCS)) \
            $(patsubst $(STANDALONE_DIR)/%.cpp,$(EXE_BUILD_DIR)/%.o,$(CXXSRCS)) \
            $(patsubst $(POSIX_GENERATED_DIR)/%.c,$(EXE_BUILD_DIR)/generated-posix/%.o,$(POSIX_GENERATED_CSRCS)) \
            $(patsubst $(QCOMMON_DIR)/%.c,$(EXE_BUILD_DIR)/qcommon/%.o,$(QCOMMON_CSRCS)) \
	    $(patsubst $(CRT_COMPAT_DIR)/%.c,$(EXE_BUILD_DIR)/src/compat/crt/%.o,$(CRT_COMPAT_CSRCS)) \
            $(patsubst $(MATH_DIR)/%.c,$(EXE_BUILD_DIR)/math/%.o,$(MATH_CSRCS)) \
            $(patsubst $(SOUND_ALIAS_DIR)/%.c,$(EXE_BUILD_DIR)/sound-alias/%.o,$(SOUND_ALIAS_CSRCS)) \
            $(patsubst $(ANIMATION_DIR)/%.c,$(EXE_BUILD_DIR)/src/animation/%.o,$(ANIMATION_CSRCS)) \
            $(patsubst $(COLLISION_DIR)/%.c,$(EXE_BUILD_DIR)/src/collision/%.o,$(COLLISION_CSRCS)) \
            $(patsubst $(FILESYSTEM_DIR)/%.c,$(EXE_BUILD_DIR)/src/filesystem/%.o,$(FILESYSTEM_CSRCS)) \
            $(patsubst $(SERVER_DIR)/%.c,$(EXE_BUILD_DIR)/shared-server/%.o,$(SERVER_CSRCS)) \
            $(patsubst $(SCRIPT_DIR)/%.c,$(EXE_BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CSRCS)) \
            $(patsubst $(SCRIPT_DIR)/%.cpp,$(EXE_BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CXXSRCS))
EXE_I386_OBJS := $(patsubst $(STANDALONE_DIR)/%.c,$(EXE_I386_BUILD_DIR)/%.o,$(STANDALONE_CSRCS)) \
                 $(patsubst $(STANDALONE_DIR)/%.cpp,$(EXE_I386_BUILD_DIR)/%.o,$(CXXSRCS)) \
                 $(patsubst $(POSIX_GENERATED_DIR)/%.c,$(EXE_I386_BUILD_DIR)/generated-posix/%.o,$(POSIX_GENERATED_CSRCS)) \
                 $(patsubst $(QCOMMON_DIR)/%.c,$(EXE_I386_BUILD_DIR)/qcommon/%.o,$(QCOMMON_CSRCS)) \
		 $(patsubst $(CRT_COMPAT_DIR)/%.c,$(EXE_I386_BUILD_DIR)/src/compat/crt/%.o,$(CRT_COMPAT_CSRCS)) \
                 $(patsubst $(MATH_DIR)/%.c,$(EXE_I386_BUILD_DIR)/math/%.o,$(MATH_CSRCS)) \
                 $(patsubst $(SOUND_ALIAS_DIR)/%.c,$(EXE_I386_BUILD_DIR)/sound-alias/%.o,$(SOUND_ALIAS_CSRCS)) \
                 $(patsubst $(ANIMATION_DIR)/%.c,$(EXE_I386_BUILD_DIR)/src/animation/%.o,$(ANIMATION_CSRCS)) \
                 $(patsubst $(COLLISION_DIR)/%.c,$(EXE_I386_BUILD_DIR)/src/collision/%.o,$(COLLISION_CSRCS)) \
                 $(patsubst $(FILESYSTEM_DIR)/%.c,$(EXE_I386_BUILD_DIR)/src/filesystem/%.o,$(FILESYSTEM_CSRCS)) \
                 $(patsubst $(SERVER_DIR)/%.c,$(EXE_I386_BUILD_DIR)/shared-server/%.o,$(SERVER_CSRCS)) \
                 $(patsubst $(SCRIPT_DIR)/%.c,$(EXE_I386_BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CSRCS)) \
                 $(patsubst $(SCRIPT_DIR)/%.cpp,$(EXE_I386_BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CXXSRCS))
WINDOWS_I386_OBJS := $(patsubst $(STANDALONE_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/%.o,$(STANDALONE_CSRCS)) \
                     $(patsubst $(STANDALONE_DIR)/%.cpp,$(WINDOWS_I386_BUILD_DIR)/%.o,$(CXXSRCS)) \
                     $(patsubst $(POSIX_GENERATED_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/generated-posix/%.o,$(POSIX_GENERATED_CSRCS)) \
                     $(patsubst $(QCOMMON_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/qcommon/%.o,$(QCOMMON_CSRCS)) \
		     $(patsubst $(CRT_COMPAT_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/src/compat/crt/%.o,$(CRT_COMPAT_CSRCS)) \
                     $(patsubst $(MATH_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/math/%.o,$(MATH_CSRCS)) \
                     $(patsubst $(SOUND_ALIAS_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/sound-alias/%.o,$(SOUND_ALIAS_CSRCS)) \
                     $(patsubst $(ANIMATION_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/src/animation/%.o,$(ANIMATION_CSRCS)) \
                     $(patsubst $(COLLISION_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/src/collision/%.o,$(COLLISION_CSRCS)) \
                     $(patsubst $(FILESYSTEM_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/src/filesystem/%.o,$(FILESYSTEM_CSRCS)) \
                     $(patsubst $(SERVER_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/shared-server/%.o,$(SERVER_CSRCS)) \
                     $(patsubst $(SCRIPT_DIR)/%.c,$(WINDOWS_I386_BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CSRCS)) \
                     $(patsubst $(SCRIPT_DIR)/%.cpp,$(WINDOWS_I386_BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CXXSRCS))
WINDOWS_I686_OBJS := $(patsubst $(STANDALONE_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/%.o,$(STANDALONE_CSRCS)) \
                     $(patsubst $(STANDALONE_DIR)/%.cpp,$(WINDOWS_I686_BUILD_DIR)/%.o,$(CXXSRCS)) \
                     $(patsubst $(POSIX_GENERATED_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/generated-posix/%.o,$(POSIX_GENERATED_CSRCS)) \
                     $(patsubst $(QCOMMON_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/qcommon/%.o,$(QCOMMON_CSRCS)) \
		     $(patsubst $(CRT_COMPAT_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/src/compat/crt/%.o,$(CRT_COMPAT_CSRCS)) \
                     $(patsubst $(MATH_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/math/%.o,$(MATH_CSRCS)) \
                     $(patsubst $(SOUND_ALIAS_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/sound-alias/%.o,$(SOUND_ALIAS_CSRCS)) \
                     $(patsubst $(ANIMATION_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/src/animation/%.o,$(ANIMATION_CSRCS)) \
                     $(patsubst $(COLLISION_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/src/collision/%.o,$(COLLISION_CSRCS)) \
                     $(patsubst $(FILESYSTEM_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/src/filesystem/%.o,$(FILESYSTEM_CSRCS)) \
                     $(patsubst $(SERVER_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/shared-server/%.o,$(SERVER_CSRCS)) \
                     $(patsubst $(SCRIPT_DIR)/%.c,$(WINDOWS_I686_BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CSRCS)) \
                     $(patsubst $(SCRIPT_DIR)/%.cpp,$(WINDOWS_I686_BUILD_DIR)/src/scripting/%.o,$(SCRIPT_CXXSRCS))
DEPS := $(OBJS:.o=.d) $(SHARED_OBJS:.o=.d) $(EXE_OBJS:.o=.d) \
        $(EXE_I386_OBJS:.o=.d) $(WINDOWS_I386_OBJS:.o=.d) \
        $(WINDOWS_I686_OBJS:.o=.d)

CPPFLAGS += -iquote $(STANDALONE_DIR)/bindings \
	-iquote src/qcommon/bindings/default \
	-Isrc \
	-Ivendor/softfloat/source/include \
	-DCODUO_ENABLE_PUNKBUSTER=$(CODUO_ENABLE_PUNKBUSTER) \
	-DCODUO_DISABLE_SERVER_AUTH=$(CODUO_DISABLE_SERVER_AUTH)
# Stock i386 uses signed plain char, 32-bit enum-backed domains, and wrapping
# ADD/SUB/IMUL semantics. GCC, Clang, and MinGW support explicit switches for
# those rules, preventing host defaults or inherited flags from changing them.
CFLAGS += -std=c11 -Wall -Wextra -Werror -pedantic -fsigned-char \
	-fno-short-enums -fwrapv -MMD -MP
CXXFLAGS += -std=c++11 -Wall -Wextra -Werror -pedantic -Wno-c99-extensions \
	-fsigned-char -fno-short-enums -fwrapv -MMD -MP
NATIVE_CPPFLAGS ?=
NATIVE_CPPFLAGS += $(PLATFORM_BEHAVIOR_CPPFLAGS)
NATIVE_CXXFLAGS ?=
SHARED_CFLAGS += -fPIC
SHARED_CXXFLAGS += -fPIC
SHARED_LD ?= $(CXX)
SHARED_LDFLAGS += -shared
EXE_LD ?= $(CXX)
EXE_I386_CC ?= gcc -m32
EXE_I386_CXX ?= g++ -m32
EXE_I386_LD ?= $(EXE_I386_CXX)
EXE_I386_LIBS ?= -ldl -lm -pthread -lstdc++ -lz
MINGW32_CC ?= i686-w64-mingw32-gcc
MINGW32_CXX ?= i686-w64-mingw32-g++
WINDOWS_DEP_PREFIX ?=
WINDOWS_DEP_CPPFLAGS := $(if $(strip $(WINDOWS_DEP_PREFIX)),-I$(WINDOWS_DEP_PREFIX)/include)
WINDOWS_DEP_LDFLAGS := $(if $(strip $(WINDOWS_DEP_PREFIX)),-L$(WINDOWS_DEP_PREFIX)/lib)
WINDOWS_COMMON_CPPFLAGS := -D_WIN32_WINNT=0x0601 $(PLATFORM_BEHAVIOR_CPPFLAGS) \
	$(WINDOWS_DEP_CPPFLAGS)
WINDOWS_COMMON_FLOAT_FLAGS := -O0 -mfpmath=387 -fexcess-precision=fast
WINDOWS_COMMON_LDFLAGS := $(WINDOWS_DEP_LDFLAGS) -static-libgcc -static-libstdc++
WINDOWS_COMMON_LIBS ?= -lws2_32 -lz

UNAME_S := $(shell uname -s)
HOST_MACHINE := $(shell uname -m)
ifneq ($(filter native-macos-exe,$(MAKECMDGOALS)),)
ifneq ($(UNAME_S),Darwin)
$(error native-macos-exe requires a Darwin host (detected '$(UNAME_S)'))
endif
endif

# x87 floating-point faithfulness policy (shared with the game DLL). Decides
# whether the native/exe build reproduces the original's x87 float rounding
# (-mfpmath=387 on GCC/x86), errors, or is an acknowledged non-exact build.
# See build-mk/x87-policy.mk and docs/components/server/fp-faithfulness.md.
X87_POLICY_ARCH := $(HOST_MACHINE)
X87_POLICY_CC := $(CC)
include build-mk/x87-policy.mk
NATIVE_CPPFLAGS += $(X87_ALLOW_CPPFLAGS)
ifeq ($(CODUO_FP_FAITHFUL),relaxed)
ifeq ($(X87_POLICY_IS_X86),1)
EMULATE_X87 ?= 0
else
EMULATE_X87 ?= 1
endif
else
EMULATE_X87 ?= 0
endif
ifneq ($(filter-out 0 1,$(EMULATE_X87)),)
$(error EMULATE_X87 must be 0 or 1 (got '$(EMULATE_X87)'))
endif
NATIVE_CPPFLAGS += -DEMULATE_X87=$(EMULATE_X87)
NATIVE_FLOAT_FLAGS := $(X87_FLOAT_FLAGS)

ifeq ($(EMULATE_X87),1)
include build-mk/x87-backend.mk
NATIVE_CPPFLAGS += $(X87_BACKEND_CPPFLAGS)
NATIVE_FLOAT_FLAGS += $(X87_BACKEND_CFLAGS)

ifeq ($(EMU_X87_BACKEND),softfloat)
NATIVE_NEEDS_SOFTFLOAT := 1
endif
ifeq ($(EMU_X87_EXACT_GEOMETRY),1)
NATIVE_NEEDS_SOFTFLOAT := 1
endif
endif

ifeq ($(NATIVE_NEEDS_SOFTFLOAT),1)
SERVER_SOFTFLOAT_BUILD_DIR ?= .workbench/build/server/softfloat-coduo-x87
NATIVE_SOFTFLOAT_LIBRARY ?= $(SERVER_SOFTFLOAT_BUILD_DIR)/softfloat.a
SOFTFLOAT_SOURCE_DIR := $(abspath vendor/softfloat/source)
SOFTFLOAT_PLATFORM_DIR := $(abspath vendor/softfloat/build/coduo-x87)
SOFTFLOAT_BUILD_INPUTS := $(SOFTFLOAT_PLATFORM_DIR)/Makefile \
	$(SOFTFLOAT_PLATFORM_DIR)/platform.h \
	$(wildcard $(SOFTFLOAT_SOURCE_DIR)/*.c) \
	$(wildcard $(SOFTFLOAT_SOURCE_DIR)/8086/*.[ch]) \
	$(wildcard $(SOFTFLOAT_SOURCE_DIR)/include/*.h)
else
NATIVE_SOFTFLOAT_LIBRARY ?=
endif

# Phase-split for the fast (EMU_X87_DOUBLE) backend. The collision-geometry
# CONSTRUCTION (patch/soup build + its vector helpers) runs ONCE at map load, so
# its FP cost is amortized to nothing; only the per-frame TRACE path is hot.
# With EMU_X87_EXACT_GEOMETRY=1 these construction TUs are forced to the exact
# SoftFloat backend regardless of the query backend, so a double-backend build
# still loads BIT-IDENTICAL geometry (no ULP drift in the built planes) while
# traces stay native-double fast. The consumer must still link softfloat.a and
# add its -I (the construction TUs include softfloat.h). A trailing -D wins, so
# this overrides the backend the consumer set in CFLAGS for just these files.
ifeq ($(EMU_X87_EXACT_GEOMETRY),1)
EMU_X87_GEOMETRY_TUS := src/collision/collision_patch_build \
    src/collision/collision_triangle_soup \
    core_math/vector_normalize \
    core_math/vector_normalize2 math/cross_product
$(foreach t,$(EMU_X87_GEOMETRY_TUS),$(eval \
    $(EXE_BUILD_DIR)/$(t).o: NATIVE_FLOAT_FLAGS += \
        -UEMULATE_X87_BACKEND -DEMULATE_X87_BACKEND=EMU_X87_SOFTFLOAT))
endif

ifeq ($(UNAME_S),Darwin)
SHARED_EXT ?= dylib
UNRESOLVED_ENGINE_LDFLAGS ?= -Wl,-undefined,dynamic_lookup
ENGINE_IMPORT_LIBS ?= -lm -pthread -lz
else
SHARED_EXT ?= so
UNRESOLVED_ENGINE_LDFLAGS ?=
ENGINE_IMPORT_LIBS ?= -ldl -lm -pthread -lstdc++ -lz
CPPFLAGS += -D_GNU_SOURCE
NATIVE_CXXFLAGS += -Wno-pedantic
endif

SHARED_TARGET := $(BUILD_DIR)/libcoduo_lnxded_recovered.$(SHARED_EXT)

.PHONY: all check objects shared shared-check-link exe exe-i386 windows \
	windows-i386 windows-i686 native64-check native64-shared native64-exe native-macos-exe \
	status clean help

all: objects

check: objects status

objects: $(TARGET)

shared: $(SHARED_TARGET)

shared-check-link: shared

exe: $(EXE_TARGET)

exe-i386: $(EXE_I386_TARGET)

windows: windows-i686

windows-i386: $(WINDOWS_I386_TARGET)

windows-i686: $(WINDOWS_I686_TARGET)

native64-check: check

native64-shared: shared-check-link

native64-exe: exe

native-macos-exe: native64-exe

status:
	python3 analysis/tools/check_engine_recovery_status.py

clean:
	rm -r $(BUILD_DIR)

help:
	@printf '%s\n' \
	  'Targets: check native64-check native64-shared native64-exe native-macos-exe shared-check-link exe exe-i386 windows-i386 windows-i686 clean' \
	  'Options: CODUO_ENABLE_PUNKBUSTER=1 compiles PunkBuster recovery code.' \
	  '         CODUO_DISABLE_SERVER_AUTH=1 disables external server authorization.' \
	  '         CODUO_FP_FAITHFUL=auto|strict|relaxed controls x87 float faithfulness;' \
	  '           native-macos-exe selects relaxed plus EMULATE_X87=1.' \
	  '         EMU_X87_BACKEND=softfloat|double selects the emulated-site backend.' \
	  '         WINDOWS_DEP_PREFIX=/path supplies MinGW zlib headers and libraries.' \
	  '           See docs/fp-faithfulness.md.'

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(EXE_I386_BUILD_DIR):
	mkdir -p $(EXE_I386_BUILD_DIR)

$(EXE_BUILD_DIR):
	mkdir -p $(EXE_BUILD_DIR)

$(WINDOWS_I386_BUILD_DIR):
	mkdir -p $(WINDOWS_I386_BUILD_DIR)

$(WINDOWS_I686_BUILD_DIR):
	mkdir -p $(WINDOWS_I686_BUILD_DIR)

ifeq ($(NATIVE_NEEDS_SOFTFLOAT),1)
$(NATIVE_SOFTFLOAT_LIBRARY): $(SOFTFLOAT_BUILD_INPUTS)
	mkdir -p $(dir $@)
	$(MAKE) -C $(dir $@) -f $(SOFTFLOAT_PLATFORM_DIR)/Makefile \
		SOURCE_DIR=$(SOFTFLOAT_SOURCE_DIR) \
		PLATFORM_DIR=$(SOFTFLOAT_PLATFORM_DIR) softfloat.a
endif

$(BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/generated-posix/%.o: $(POSIX_GENERATED_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/qcommon/%.o: $(QCOMMON_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/src/compat/crt/%.o: $(CRT_COMPAT_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/math/%.o: $(MATH_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/sound-alias/%.o: $(SOUND_ALIAS_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/src/animation/%.o: $(ANIMATION_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/src/collision/%.o: $(COLLISION_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/src/filesystem/%.o: $(FILESYSTEM_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared-server/%.o: $(SERVER_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CXXFLAGS) $(NATIVE_CXXFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CXXFLAGS) $(NATIVE_CXXFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/%.o: $(STANDALONE_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/generated-posix/%.o: $(POSIX_GENERATED_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/qcommon/%.o: $(QCOMMON_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/compat/crt/%.o: $(CRT_COMPAT_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/math/%.o: $(MATH_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/sound/alias/%.o: $(SOUND_ALIAS_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/animation/%.o: $(ANIMATION_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/collision/%.o: $(COLLISION_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/filesystem/%.o: $(FILESYSTEM_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/shared-server/%.o: $(SERVER_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/scripting/%.o: $(SCRIPT_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(SHARED_CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/src/scripting/%.o: $(SCRIPT_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CXXFLAGS) $(NATIVE_CXXFLAGS) $(SHARED_CXXFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(BUILD_DIR)/shared/%.o: $(STANDALONE_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CXXFLAGS) $(NATIVE_CXXFLAGS) $(SHARED_CXXFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/generated-posix/%.o: $(POSIX_GENERATED_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/qcommon/%.o: $(QCOMMON_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/src/compat/crt/%.o: $(CRT_COMPAT_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/math/%.o: $(MATH_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/sound-alias/%.o: $(SOUND_ALIAS_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/src/animation/%.o: $(ANIMATION_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/src/collision/%.o: $(COLLISION_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/src/filesystem/%.o: $(FILESYSTEM_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/shared-server/%.o: $(SERVER_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CXXFLAGS) $(NATIVE_CXXFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

$(EXE_BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(NATIVE_CPPFLAGS) $(CXXFLAGS) $(NATIVE_CXXFLAGS) $(NATIVE_FLOAT_FLAGS) -c $< -o $@

# 32-bit i386 is always x87; -fexcess-precision=fast gives stock's per-assignment
# float rounding (the -std=c11 default -fexcess-precision=standard keeps 80-bit
# x87 values live across float assignments and diverges from stock on FP ties —
# see build-mk/x87-policy.mk). The retail i386 graph lowers comparisons
# through the x87 status word; -march=i386 prevents modern GCC from substituting
# the later FCOMI/FUCOMI instruction family.
$(EXE_I386_BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/generated-posix/%.o: $(POSIX_GENERATED_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/qcommon/%.o: $(QCOMMON_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/src/compat/crt/%.o: $(CRT_COMPAT_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/math/%.o: $(MATH_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/sound-alias/%.o: $(SOUND_ALIAS_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/src/animation/%.o: $(ANIMATION_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/src/collision/%.o: $(COLLISION_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/src/filesystem/%.o: $(FILESYSTEM_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/shared-server/%.o: $(SERVER_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.c
	mkdir -p $(dir $@)
	$(EXE_I386_CC) $(CPPFLAGS) $(CFLAGS) -march=i386 \
		$(PLATFORM_BEHAVIOR_CPPFLAGS) -fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(EXE_I386_CXX) $(CPPFLAGS) $(filter-out -pedantic,$(CXXFLAGS)) \
		-Wno-pedantic -march=i386 $(PLATFORM_BEHAVIOR_CPPFLAGS) \
		-fexcess-precision=fast -c $< -o $@

$(EXE_I386_BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(EXE_I386_CXX) $(CPPFLAGS) $(filter-out -pedantic,$(CXXFLAGS)) \
		-Wno-pedantic -march=i386 $(PLATFORM_BEHAVIOR_CPPFLAGS) \
		-fexcess-precision=fast -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/generated-posix/%.o: $(POSIX_GENERATED_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/qcommon/%.o: $(QCOMMON_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/src/compat/crt/%.o: $(CRT_COMPAT_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/math/%.o: $(MATH_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/sound-alias/%.o: $(SOUND_ALIAS_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/src/animation/%.o: $(ANIMATION_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/src/collision/%.o: $(COLLISION_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/src/filesystem/%.o: $(FILESYSTEM_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/shared-server/%.o: $(SERVER_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i386 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(MINGW32_CXX) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) \
		$(filter-out -pedantic,$(CXXFLAGS)) -Wno-pedantic -march=i386 \
		$(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I386_BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(MINGW32_CXX) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) \
		$(filter-out -pedantic,$(CXXFLAGS)) -Wno-pedantic -march=i386 \
		$(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/generated-posix/%.o: $(POSIX_GENERATED_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/qcommon/%.o: $(QCOMMON_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/src/compat/crt/%.o: $(CRT_COMPAT_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/math/%.o: $(MATH_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/sound-alias/%.o: $(SOUND_ALIAS_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/src/animation/%.o: $(ANIMATION_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/src/collision/%.o: $(COLLISION_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/src/filesystem/%.o: $(FILESYSTEM_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/shared-server/%.o: $(SERVER_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.c
	mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) $(CFLAGS) \
		-march=i686 $(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/src/scripting/%.o: $(SCRIPT_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(MINGW32_CXX) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) \
		$(filter-out -pedantic,$(CXXFLAGS)) -Wno-pedantic -march=i686 \
		$(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(WINDOWS_I686_BUILD_DIR)/%.o: $(STANDALONE_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(MINGW32_CXX) $(CPPFLAGS) $(WINDOWS_COMMON_CPPFLAGS) \
		$(filter-out -pedantic,$(CXXFLAGS)) -Wno-pedantic -march=i686 \
		$(WINDOWS_COMMON_FLOAT_FLAGS) -c $< -o $@

$(TARGET): $(OBJS) | $(BUILD_DIR)
	@if [ -e $@ ]; then rm $@; fi
	$(AR) rcs $@ $^

$(SHARED_TARGET): $(SHARED_OBJS) $(NATIVE_SOFTFLOAT_LIBRARY) | $(BUILD_DIR)
	$(SHARED_LD) $(SHARED_LDFLAGS) $(UNRESOLVED_ENGINE_LDFLAGS) -o $@ $^ $(ENGINE_IMPORT_LIBS)

$(EXE_TARGET): $(EXE_OBJS) $(NATIVE_SOFTFLOAT_LIBRARY) | $(EXE_BUILD_DIR)
	$(EXE_LD) -o $@ $^ $(ENGINE_IMPORT_LIBS)

$(EXE_I386_TARGET): $(EXE_I386_OBJS) | $(EXE_I386_BUILD_DIR)
	$(EXE_I386_LD) -o $@ $^ $(EXE_I386_LIBS)

$(WINDOWS_I386_TARGET): $(WINDOWS_I386_OBJS) | $(WINDOWS_I386_BUILD_DIR)
	$(MINGW32_CXX) -o $@ $^ $(WINDOWS_COMMON_LDFLAGS) $(WINDOWS_COMMON_LIBS)

$(WINDOWS_I686_TARGET): $(WINDOWS_I686_OBJS) | $(WINDOWS_I686_BUILD_DIR)
	$(MINGW32_CXX) -o $@ $^ $(WINDOWS_COMMON_LDFLAGS) $(WINDOWS_COMMON_LIBS)

-include $(DEPS)
