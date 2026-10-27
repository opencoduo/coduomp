# MSVC x86 supports x87 arithmetic through /arch:IA32; MSVC x64 does not.
# All targets use native arithmetic. Explicit wide carriers and rounding
# boundaries preserve the source contract within the compiler's native types.
set(CODUO_FP_FAITHFUL auto CACHE STRING "Floating-point policy: auto, strict, or relaxed")
set_property(CACHE CODUO_FP_FAITHFUL PROPERTY STRINGS auto strict relaxed)
if(NOT CODUO_FP_FAITHFUL MATCHES "^(auto|strict|relaxed)$")
    message(FATAL_ERROR "CODUO_FP_FAITHFUL must be auto, strict, or relaxed.")
endif()
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
    message(STATUS "MSVC x86 uses native x87 with binary64 carriers; prefer clang-cl-x64 for 80-bit WIDE_FLOAT storage.")
else()
    set(CODUOMP_FLOAT_MODEL "native SSE2 arithmetic; binary64 long double")
    message(WARNING
        "MSVC x64 cannot generate x87 arithmetic from C/C++. Native floating-point "
        "expressions use SSE2 or newer instructions and may differ from the original "
        "game's x87 results. Its widest standard type is binary64. "
        "Prefer clang-cl-x64 for native x87 in explicit extended expressions. "
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
