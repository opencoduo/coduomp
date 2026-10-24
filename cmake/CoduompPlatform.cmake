#-----------------------------------------------------------------
# MSVC platform settings
#-----------------------------------------------------------------

math(EXPR CODUOMP_POINTER_BITS "8 * ${CMAKE_SIZEOF_VOID_P}")

# Only a 32-bit x86 target reproduces the retail floating-point model: it is
# natively all-x87, so every float expression evaluates in 80-bit and narrows at
# the store points the sources encode.
set(CODUOMP_32BIT_X86 FALSE)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	if(NOT DEFINED MSVC_C_ARCHITECTURE_ID OR MSVC_C_ARCHITECTURE_ID STREQUAL "X86")
		set(CODUOMP_32BIT_X86 TRUE)
	endif()
endif()

if(MSVC_C_ARCHITECTURE_ID STREQUAL "X86")
	set(CODUOMP_ARCH "x86")
elseif(MSVC_C_ARCHITECTURE_ID STREQUAL "X64")
	set(CODUOMP_ARCH "x64")
elseif(MSVC_C_ARCHITECTURE_ID STREQUAL "ARM64")
	set(CODUOMP_ARCH "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(CODUOMP_ARCH "x86")
else()
	set(CODUOMP_ARCH "x64")
endif()

# EMULATE_X87 selects the source-level software-x87 variants. Native x87 codegen
# is available here, so the recovered canonical bodies are used unchanged.
set(CODUOMP_EMULATE_X87 0)

if(CODUOMP_32BIT_X86)
	# Retail x87 code generation: /arch:IA32 disables SSE float math.
	add_compile_options(/arch:IA32)
	set(CODUOMP_FLOAT_MODEL "native x87 (/arch:IA32)")
else()
	message(WARNING
		"CoDUOMP: this is not a Win32 configuration. MSVC cannot emit x87 code "
		"or an 80-bit long double on 64-bit targets, so floating-point results "
		"are not retail-equivalent. Re-run CMake with -A Win32 for the faithful "
		"32-bit configuration.")
	set(CODUOMP_FLOAT_MODEL "native SSE (not retail-equivalent)")
endif()

# The recovered source uses the same warning discipline as the Makefiles, but
# MSVC's warning set differs, so the level stays low (as in the parent project)
# and warnings are not errors.
add_compile_options(/W1 /utf-8)

add_compile_definitions(
	_CRT_SECURE_NO_WARNINGS
	_CRT_NONSTDC_NO_DEPRECATE
	_WINSOCK_DEPRECATED_NO_WARNINGS)

# System libraries required by the client executable. The static SDL2, libjpeg,
# libcurl and minizip builds used by the Makefiles need these as well.
set(CODUOMP_CLIENT_SYSTEM_LIBS
	opengl32 winmm ws2_32 advapi32 crypt32 bcrypt
	user32 gdi32 shell32 version ole32 oleaut32 uuid
	imm32 setupapi iphlpapi secur32 wldap32 ddraw dxguid)

set(CODUOMP_UI_SYSTEM_LIBS user32 gdi32)
set(CODUOMP_SERVER_SYSTEM_LIBS ws2_32)
