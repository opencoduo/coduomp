#-----------------------------------------------------------------
# Platform settings
#
# MSVC and clang-cl generate the Windows client, modules, and dedicated
# server. The native branch mirrors build-mk/server.mk for the Linux dedicated
# server: same compiler flags, x87 floating-point policy, and link libraries.
#-----------------------------------------------------------------

set(CODUO_FP_FAITHFUL auto CACHE STRING "Floating-point policy: auto, strict, or relaxed")
set_property(CACHE CODUO_FP_FAITHFUL PROPERTY STRINGS auto strict relaxed)
if(NOT CODUO_FP_FAITHFUL MATCHES "^(auto|strict|relaxed)$")
    message(FATAL_ERROR "CODUO_FP_FAITHFUL must be auto, strict, or relaxed.")
endif()

if(MSVC)

if(CODUO_FP_FAITHFUL STREQUAL "strict")
    message(FATAL_ERROR
        "The current Windows CMake source paths have not established whole-program "
        "floating-point parity. MSVC x86 supports x87 arithmetic, but its "
        "long double storage is binary64 and some operations use host math. "
        "Clang-cl uses x87 only for explicit extended expressions. "
        "Use the default auto policy or relaxed to build with these limits.")
endif()
string(TOUPPER "${MSVC_C_ARCHITECTURE_ID}" _msvc_arch)
if(_msvc_arch STREQUAL "X86" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CODUOMP_ARCH x86)
    set(CODUOMP_32BIT_X86 TRUE)
elseif(_msvc_arch STREQUAL "X64")
    set(CODUOMP_ARCH x64)
    set(CODUOMP_32BIT_X86 FALSE)
else()
    message(FATAL_ERROR "This Windows CMake build supports only x86 and x64.")
endif()
if(CODUOMP_CLANG_CL)
    if(CODUOMP_32BIT_X86)
        message(FATAL_ERROR "Use MSVC for x86; the clang-cl profile supports only x64.")
    endif()
    include(CheckCSourceCompiles)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    string(JOIN " " CMAKE_REQUIRED_FLAGS ${CODUOMP_EXTENDED_FLAGS})
    set(_wide_contract "#include <float.h>
#if !defined(_M_X64) || LDBL_MANT_DIG != 64 || LDBL_MAX_EXP != 16384 || FLT_EVAL_METHOD != 0
#error Incorrect extended type or expression evaluation mode
#endif
int main(void) { typedef char carrier[(sizeof(long double) == 16) ? 1 : -1]; return 0; }")
    check_c_source_compiles("${_wide_contract}" CODUOMP_C_EXTENDED_ABI)
    check_cxx_source_compiles("${_wide_contract}" CODUOMP_CXX_EXTENDED_ABI)
    cmake_pop_check_state()
    if(NOT CODUOMP_C_EXTENDED_ABI OR NOT CODUOMP_CXX_EXTENDED_ABI)
        message(FATAL_ERROR "clang-cl requires x64, 80-bit long double, and source-type evaluation in C and C++.")
    endif()
    set(CODUOMP_FLOAT_MODEL "native x87 for explicit WIDE_FLOAT/long double; SSE for ordinary float/double")
    message(STATUS "clang-cl x64 preserves the Windows calling ABI and uses the existing explicit x87 source paths.")
elseif(CODUOMP_32BIT_X86)
    set(CODUOMP_FLOAT_MODEL "native x87 compiler arithmetic (/arch:IA32); binary64 long double storage")
    message(STATUS "MSVC x86 uses native x87 with binary64 carriers; prefer windows-clangcl-64bit for 80-bit WIDE_FLOAT storage.")
else()
    set(CODUOMP_FLOAT_MODEL "native SSE2 arithmetic; binary64 long double")
    message(WARNING
        "MSVC x64 cannot generate x87 arithmetic from C/C++. Native floating-point "
        "expressions use SSE2 or newer instructions and may differ from the original "
        "game's x87 results. Its widest standard type is binary64. "
        "Prefer windows-clangcl-64bit for native x87 in explicit extended expressions. "
        "See docs/client-building.md for the architecture and fidelity limits.")
endif()
message(STATUS "Whole-program floating-point parity remains unverified on these Windows CMake targets.")

# System libraries required by the client executable. The static SDL2, libjpeg,
# libcurl and minizip builds used by the Makefiles need these as well.
set(CODUOMP_CLIENT_SYSTEM_LIBS
	opengl32 winmm ws2_32 advapi32 crypt32 bcrypt
	user32 gdi32 shell32 version ole32 oleaut32 uuid
	imm32 setupapi iphlpapi secur32 wldap32 ddraw dxguid)

set(CODUOMP_UI_SYSTEM_LIBS user32 gdi32)

# System libraries required by the dedicated server executable.
set(CODUOMP_SERVER_SYSTEM_LIBS ws2_32)

else()

#-----------------------------------------------------------------
# Native GCC/Clang settings
#-----------------------------------------------------------------

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
		message(FATAL_ERROR
			"CoDUOMP: the native build supports x86 and x86-64 targets "
			"(detected '${CMAKE_SYSTEM_PROCESSOR}').")
	endif()
	set(CODUOMP_ARCH "x86")
	set(CODUOMP_32BIT_X86 TRUE)
	set(CODUOMP_MODULE_ARCH "i386")
else()
	if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
		message(FATAL_ERROR
			"CoDUOMP: the native build supports x86 and x86-64 targets "
			"(detected '${CMAKE_SYSTEM_PROCESSOR}').")
	endif()
	set(CODUOMP_ARCH "x64")
	set(CODUOMP_32BIT_X86 FALSE)
	set(CODUOMP_MODULE_ARCH "x86_64")
endif()

# x87 floating-point policy (build-mk/x87-policy.mk). The original is a 32-bit
# x87 binary, so the faithful build pins -O0 and either uses the native x87 unit
# (32-bit x86) or forces x87 evaluation with -mfpmath=387, and rounds every
# float assignment to 32 bits. A target without an x87 unit cannot reproduce the
# model; CODUO_FP_FAITHFUL=relaxed acknowledges that difference explicitly.
include(CheckCCompilerFlag)
# Keep this name outside the CODUOMP_* variables that build-mk manifests
# assign: client-targets.mk defines CODUOMP_FLOAT_FLAGS for the macOS client
# and coduomp_manifest_reset() removes it after the manifest is read, which
# would silently drop the x87 flags from every native target.
set(CODUOMP_FP_FLAGS "")
if(CODUO_FP_FAITHFUL STREQUAL "relaxed")
	set(CODUOMP_FLOAT_MODEL "native arithmetic (relaxed)")
else()
	if(CODUOMP_32BIT_X86)
		set(CODUOMP_FP_FLAGS -O0 -fexcess-precision=fast)
		set(CODUOMP_FLOAT_MODEL "native x87 (32-bit)")
	else()
		check_c_compiler_flag("-mfpmath=387" CODUOMP_HAVE_MFPMATH_387)
		if(CODUOMP_HAVE_MFPMATH_387)
			set(CODUOMP_FP_FLAGS -O0 -mfpmath=387 -fexcess-precision=fast)
			set(CODUOMP_FLOAT_MODEL "native x87 (-mfpmath=387)")
		endif()
	endif()
	if(NOT CODUOMP_FP_FLAGS)
		message(FATAL_ERROR
			"CoDUOMP: a faithful server build needs GCC on x86 or x86-64 "
			"(-mfpmath=387); '${CMAKE_C_COMPILER_ID}' on "
			"'${CMAKE_SYSTEM_PROCESSOR}' cannot reproduce the original x87 "
			"floating-point model. Build with GCC instead, or configure with "
			"-DCODUO_FP_FAITHFUL=relaxed.")
	endif()
endif()

# The faithful model is pinned to -O0 and the Makefile build defines no NDEBUG,
# so keep the selected configuration from adding an optimization level or
# disabling assertions behind the target's back. Third-party code built in this
# tree (the bundled zlib) follows the same configuration flags.
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")

# _GNU_SOURCE is the Linux define the Makefile server target uses.
# CODUO_ENGINE_ALLOW_NON_X87_FLOAT selects the C fallbacks for operations whose
# inline x87 assembly needs a 32-bit target (build-mk/x87-policy.mk).
set(CODUOMP_PLATFORM_DEFINES _GNU_SOURCE)
if(NOT CODUOMP_32BIT_X86)
	list(APPEND CODUOMP_PLATFORM_DEFINES CODUO_ENGINE_ALLOW_NON_X87_FLOAT=1)
endif()

# A relaxed build also relaxes the game module's source-level x87 long-double
# equivalence assert, exactly as build-mk/game.mk does.
if(CODUO_FP_FAITHFUL STREQUAL "relaxed")
	list(APPEND CODUOMP_PLATFORM_DEFINES
		GAME_REQUIRE_X87_LONG_DOUBLE=0
		GAME_ALLOW_NON_EQUIVALENT_LONG_DOUBLE=1)
endif()

# The Makefile server links the C++ runtime through g++ and imports dl, libm,
# pthread and zlib.
set(CODUOMP_SERVER_SYSTEM_LIBS dl m pthread)

endif()
