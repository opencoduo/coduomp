include build-mk/sources/client-engine.mk

BUILD_DIR ?= build
MINGW32_CC ?= i686-w64-mingw32-gcc
MINGW32_CXX ?= i686-w64-mingw32-g++
MINGW32_DLLTOOL ?= i686-w64-mingw32-dlltool
MINGW32_OBJDUMP ?= i686-w64-mingw32-objdump
MINGW32_WINDRES ?= i686-w64-mingw32-windres
MINGW32_BUILD_DIR ?= $(BUILD_DIR)/mingw-i686
MINGW32_OUTPUT ?= $(MINGW32_BUILD_DIR)/CoDUOMP.exe
MINGW64_CC ?= x86_64-w64-mingw32-gcc
MINGW64_CXX ?= x86_64-w64-mingw32-g++
MINGW64_DLLTOOL ?= x86_64-w64-mingw32-dlltool
MINGW64_OBJDUMP ?= x86_64-w64-mingw32-objdump
MINGW64_WINDRES ?= x86_64-w64-mingw32-windres
MINGW64_BUILD_DIR ?= $(BUILD_DIR)/mingw-x86_64
MINGW64_OUTPUT ?= $(MINGW64_BUILD_DIR)/CoDUOMP.exe
MINGW32_OBJECT_DIR := $(MINGW32_BUILD_DIR)/coduomp-objects
MINGW32_C_OBJECTS := $(patsubst %.c,$(MINGW32_OBJECT_DIR)/%.c.o,$(CLIENT_ENGINE_TARGET_C_SOURCES))
MINGW32_CXX_OBJECTS := $(patsubst %.cpp,$(MINGW32_OBJECT_DIR)/%.cpp.o,$(CLIENT_ENGINE_TARGET_CXX_SOURCES))
MINGW32_PROFILE_HOT_C_OBJECTS := $(patsubst %.c,$(MINGW32_OBJECT_DIR)/%.c.o,$(CLIENT_ENGINE_PROFILE_HOT_C_SOURCES))
MINGW32_RESOURCE_SOURCE := src/client/engine/platform/windows_resources.rc
MINGW32_ICON_SOURCE := assets/windows/coduomp.ico
MINGW32_RESOURCE_OBJECT := $(MINGW32_BUILD_DIR)/coduomp-resources.o
MINGW32_MSS_DEF := $(MINGW32_BUILD_DIR)/mss32.generated.def
MINGW32_MSS_IMPORT_LIB := $(MINGW32_BUILD_DIR)/libmss32.a
MSS32_DLL ?=
MINGW32_DEP_PREFIX ?=
MINGW64_DEP_PREFIX ?=
MINGW32_DEP_CPPFLAGS ?=
MINGW32_DEP_LDFLAGS ?=
RECOVERY_POLICY_CPPFLAGS :=
MINGW32_TARGET_PATTERN ?= i?86-w64-mingw32*
MINGW32_TARGET_DESCRIPTION ?= i686 MinGW-w64
MINGW32_ARCHIVE_FORMAT ?= pe-i386
MINGW32_REQUIRE_MSS ?= 1
MINGW32_AUDIO_CPPFLAGS ?=
MINGW32_COMMON_CFLAGS ?= -g -O0
MINGW32_ENGINE_CFLAGS := $(filter-out -O%,$(MINGW32_COMMON_CFLAGS))
MINGW32_FLOAT_FLAGS := -O0 -mfpmath=387 -fexcess-precision=fast
MINGW32_ARCH_FLAGS := -msse
MINGW32_CPPFLAGS := -Isrc \
	-iquote src/client/engine/bindings \
	-iquote src/qcommon/bindings/default \
	-DWINDOWS_BEHAVIOR \
	-DCURL_STATICLIB \
	-DCASE_SENSITIVE_FS=0 \
	$(RECOVERY_POLICY_CPPFLAGS) \
	$(if $(MINGW32_DEP_PREFIX),-I$(MINGW32_DEP_PREFIX)/include) \
	$(if $(MINGW32_DEP_PREFIX),-I$(MINGW32_DEP_PREFIX)/include/SDL2) \
	$(MINGW32_AUDIO_CPPFLAGS) \
	$(MINGW32_DEP_CPPFLAGS)
MINGW32_CFLAGS := -std=c11 $(MINGW32_ENGINE_CFLAGS) -fwrapv \
	$(MINGW32_FLOAT_FLAGS) $(MINGW32_ARCH_FLAGS) -MMD -MP
MINGW32_CXXFLAGS := -std=c++17 $(MINGW32_ENGINE_CFLAGS) -fwrapv \
	$(MINGW32_FLOAT_FLAGS) $(MINGW32_ARCH_FLAGS) -MMD -MP
MINGW32_LDFLAGS ?= -mwindows -static-libgcc -static-libstdc++
MINGW32_STACK_RESERVE := 0x800000
MINGW32_STACK_RESERVE_PE := 00800000
MINGW32_STACK_COMMIT_PE := 00001000
MINGW32_REQUIRED_SYSTEM_LIBS := -liphlpapi -lsecur32 -lwldap32
MINGW32_LIBS ?= -ljpeg -lcurl -lminizip -lz -lmingw32 -lSDL2main -lSDL2 \
	-lopengl32 -lwinmm -limm32 -lsetupapi \
	-lws2_32 -lbcrypt -ladvapi32 -lcrypt32 -luser32 -lgdi32 \
	-lshell32 -lversion -lole32 -loleaut32 -luuid -ldinput8 -lddraw -ldxguid -lm

.PHONY: all mingw32 mingw64 mingw32-stack-check check-mingw32-deps \
	mingw32-mss-import clean help

all: mingw32

ifeq ($(filter $(MINGW32_REQUIRE_MSS),0 1),)
$(error MINGW32_REQUIRE_MSS must be 1 or 0)
endif

ifeq ($(MINGW32_REQUIRE_MSS),1)
MINGW32_AUDIO_PREREQUISITE := mingw32-mss-import
MINGW32_AUDIO_LINK_INPUT := $(MINGW32_MSS_IMPORT_LIB)
else
MINGW32_AUDIO_PREREQUISITE := check-mingw32-deps
MINGW32_AUDIO_LINK_INPUT :=
endif

mingw32: mingw32-stack-check

mingw64:
	$(MAKE) -f build-mk/client-engine-mingw.mk mingw32 \
		MINGW32_CC="$(MINGW64_CC)" MINGW32_CXX="$(MINGW64_CXX)" \
		MINGW32_DLLTOOL="$(MINGW64_DLLTOOL)" \
		MINGW32_OBJDUMP="$(MINGW64_OBJDUMP)" \
		MINGW32_WINDRES="$(MINGW64_WINDRES)" \
		MINGW32_BUILD_DIR="$(MINGW64_BUILD_DIR)" \
		MINGW32_OUTPUT="$(MINGW64_OUTPUT)" \
		MINGW32_DEP_PREFIX="$(MINGW64_DEP_PREFIX)" \
		MINGW32_TARGET_PATTERN='x86_64-w64-mingw32*' \
		MINGW32_TARGET_DESCRIPTION='x86_64 MinGW-w64' \
		MINGW32_ARCHIVE_FORMAT=pe-x86-64 \
		MINGW32_REQUIRE_MSS=0 \
		MINGW32_AUDIO_CPPFLAGS=-DCODUOMP_DISABLE_AUDIO \
		MINGW32_FLOAT_FLAGS='-O0 -mfpmath=387 -fexcess-precision=fast' MINGW32_ARCH_FLAGS=-m64 \
		MINGW32_STACK_RESERVE_PE=0000000000800000 \
		MINGW32_STACK_COMMIT_PE=0000000000001000

mingw32-stack-check: $(MINGW32_OUTPUT)
	@stack_reserve="$$($(MINGW32_OBJDUMP) -x "$<" | \
		awk '$$1 == "SizeOfStackReserve" { print $$2; exit }')"; \
	stack_commit="$$($(MINGW32_OBJDUMP) -x "$<" | \
		awk '$$1 == "SizeOfStackCommit" { print $$2; exit }')"; \
	if test "$$stack_reserve" != "$(MINGW32_STACK_RESERVE_PE)" || \
	    test "$$stack_commit" != "$(MINGW32_STACK_COMMIT_PE)"; then \
		echo "error: unexpected PE stack reserve/commit: $$stack_reserve/$$stack_commit" >&2; \
		echo "expected: $(MINGW32_STACK_RESERVE_PE)/$(MINGW32_STACK_COMMIT_PE)" >&2; \
		exit 2; \
	fi

check-mingw32-deps:
	@for tool in "$(firstword $(MINGW32_CC))" \
	    "$(firstword $(MINGW32_CXX))" \
	    "$(firstword $(MINGW32_OBJDUMP))" \
	    "$(firstword $(MINGW32_WINDRES))" \
	    $(if $(filter 1,$(MINGW32_REQUIRE_MSS)),"$(firstword $(MINGW32_DLLTOOL))"); do \
		if ! command -v "$$tool" >/dev/null 2>&1; then \
			echo "error: $(MINGW32_TARGET_DESCRIPTION) build tool not found: $$tool" >&2; \
			exit 2; \
		fi; \
	done
	@cc_target="$$($(MINGW32_CC) -dumpmachine 2>/dev/null)" || exit 2; \
	cxx_target="$$($(MINGW32_CXX) -dumpmachine 2>/dev/null)" || exit 2; \
	case "$$cc_target" in \
		$(MINGW32_TARGET_PATTERN)) ;; \
		*) echo "error: $(MINGW32_CC) targets $$cc_target, not $(MINGW32_TARGET_DESCRIPTION)" >&2; exit 2 ;; \
	esac; \
	if test "$$cxx_target" != "$$cc_target"; then \
		echo "error: MinGW C/C++ target mismatch: $$cc_target vs $$cxx_target" >&2; \
		exit 2; \
	fi
	@if test -n "$(MINGW32_DEP_PREFIX)"; then \
		if test ! -d "$(MINGW32_DEP_PREFIX)/include" || \
		    test ! -d "$(MINGW32_DEP_PREFIX)/lib"; then \
			echo "error: MINGW32_DEP_PREFIX must contain include/ and lib/: $(MINGW32_DEP_PREFIX)" >&2; \
			exit 2; \
		fi; \
		for archive in libjpeg.a libcurl.a libminizip.a libz.a libSDL2.a libSDL2main.a; do \
			path="$(MINGW32_DEP_PREFIX)/lib/$$archive"; \
			if test ! -f "$$path"; then \
				echo "error: required dependency archive is missing: $$path" >&2; \
				exit 2; \
			fi; \
			if ! $(MINGW32_OBJDUMP) -f "$$path" 2>/dev/null | \
			    grep -Fq 'file format $(MINGW32_ARCHIVE_FORMAT)'; then \
				echo "error: dependency archive is not $(MINGW32_TARGET_DESCRIPTION)-compatible: $$path" >&2; \
				exit 2; \
			fi; \
		done; \
	fi

mingw32-mss-import: check-mingw32-deps
	@mkdir -p $(MINGW32_BUILD_DIR)
	@if test -z "$(MSS32_DLL)"; then \
		echo 'MSS32_DLL must name the user-supplied retail mss32.dll'; \
		exit 2; \
	fi
	@if test ! -f "$(MSS32_DLL)"; then \
		echo 'MSS32_DLL does not exist: $(MSS32_DLL)'; \
		exit 2; \
	fi
	@if ! $(MINGW32_OBJDUMP) -f "$(MSS32_DLL)" | \
		grep -q 'file format pei-i386'; then \
		echo 'MSS32_DLL is not a 32-bit i386 PE DLL: $(MSS32_DLL)'; \
		exit 2; \
	fi
	@printf 'LIBRARY mss32.dll\nEXPORTS\n' > $(MINGW32_MSS_DEF)
	@$(MINGW32_OBJDUMP) -p "$(MSS32_DLL)" | \
		sed -n 's/.*[[:space:]]\(_AIL_[[:alnum:]_]*@[[:digit:]][[:digit:]]*\)$$/    \1/p' | \
		sort -u >> $(MINGW32_MSS_DEF)
	@if ! grep -Fxq '    _AIL_startup@0' $(MINGW32_MSS_DEF); then \
		echo 'MSS32_DLL does not expose the expected Miles stdcall API'; \
		exit 2; \
	fi
	$(MINGW32_DLLTOOL) --no-leading-underscore -D mss32.dll \
		-d $(MINGW32_MSS_DEF) -l $(MINGW32_MSS_IMPORT_LIB)

$(MINGW32_PROFILE_HOT_C_OBJECTS): \
	MINGW32_C_OPTIMIZATION_FLAGS := $(CLIENT_ENGINE_PROFILE_HOT_CFLAGS)

$(MINGW32_C_OBJECTS) $(MINGW32_CXX_OBJECTS): \
	build-mk/client-engine-mingw.mk build-mk/sources/client-engine.mk | \
	$(MINGW32_AUDIO_PREREQUISITE)

$(MINGW32_OBJECT_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(MINGW32_CC) $(CPPFLAGS) $(MINGW32_CPPFLAGS) $(CFLAGS) \
		$(MINGW32_CFLAGS) $(MINGW32_C_OPTIMIZATION_FLAGS) -c $< -o $@

$(MINGW32_OBJECT_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(MINGW32_CXX) $(CPPFLAGS) $(MINGW32_CPPFLAGS) $(CXXFLAGS) \
		$(MINGW32_CXXFLAGS) -c $< -o $@

$(MINGW32_RESOURCE_OBJECT): $(MINGW32_RESOURCE_SOURCE) \
		$(MINGW32_ICON_SOURCE) build-mk/client-engine-mingw.mk
	@mkdir -p $(dir $@)
	$(MINGW32_WINDRES) --input-format=rc --output-format=coff \
		--include-dir=. $< $@

$(MINGW32_OUTPUT): $(MINGW32_C_OBJECTS) $(MINGW32_CXX_OBJECTS) \
		$(MINGW32_RESOURCE_OBJECT) $(MINGW32_AUDIO_PREREQUISITE) \
		build-mk/client-engine-mingw.mk
	@mkdir -p $(dir $@)
	$(MINGW32_CXX) $(MINGW32_LDFLAGS) \
		-Wl,--stack,$(MINGW32_STACK_RESERVE) \
		$(if $(MINGW32_DEP_PREFIX),-L$(MINGW32_DEP_PREFIX)/lib) \
		$(MINGW32_DEP_LDFLAGS) $(MINGW32_C_OBJECTS) \
		$(MINGW32_CXX_OBJECTS) $(MINGW32_RESOURCE_OBJECT) \
		$(MINGW32_AUDIO_LINK_INPUT) \
		$(MINGW32_LIBS) $(MINGW32_REQUIRED_SYSTEM_LIBS) \
		$(LDLIBS) -o $@

clean:
	@if [ -d "$(BUILD_DIR)" ]; then rm -r "$(BUILD_DIR)"; fi

help:
	@printf '%s\n' \
	  'Targets: mingw32 mingw64 clean' \
	  'MSS32_DLL=path names the user-supplied retail mss32.dll.' \
	  'MINGW32_DEP_PREFIX=path names the i686 dependency prefix.' \
	  'MINGW64_DEP_PREFIX=path names the x86_64 dependency prefix.' \
	  'The mingw64 target uses the explicit no-audio compatibility backend.'

-include $(MINGW32_C_OBJECTS:.o=.d)
-include $(MINGW32_CXX_OBJECTS:.o=.d)
