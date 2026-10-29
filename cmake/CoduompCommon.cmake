#-----------------------------------------------------------------
# Common build settings
#-----------------------------------------------------------------

set(CODUOMP_CLANG_CL FALSE)
set(CODUOMP_NATIVE FALSE)
if(MSVC)
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
	   AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"
	   AND CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		if(CMAKE_C_COMPILER_VERSION VERSION_LESS 22 OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22)
			message(FATAL_ERROR "The clang-cl x64 profile requires Clang 22 or newer.")
		endif()
		set(CODUOMP_CLANG_CL TRUE)
		# Keep ordinary float/double calls on the Windows ABI. Global extended
		# evaluation mispasses variadic doubles in the tested Clang releases.
		set(CODUOMP_EXTENDED_FLAGS /fp:strict /clang:-mlong-double-80
			/clang:-ffp-eval-method=source /clang:-ffp-contract=off)
	elseif(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR MSVC_VERSION LESS 1939)
		message(FATAL_ERROR
			"This Windows CMake project requires MSVC 19.39+ or clang-cl 22+ for x64. "
			"Use the Makefile targets for other Windows toolchains.")
	endif()
else()
	# The native generator stays on Linux and mirrors build-mk/server.mk for the
	# dedicated server. The client executable and its modules are Windows-only.
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
		message(FATAL_ERROR
			"CoDUOMP: the native CMake build supports Linux. "
			"Use the Makefile targets for the other platforms.")
	endif()
	set(CODUOMP_NATIVE TRUE)
endif()

# Language levels used by the Makefile targets: C11 everywhere, C++11 for the
# native server (build-mk/server.mk) and C++17 for the MSVC client.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(CODUOMP_NATIVE)
	set(CMAKE_CXX_STANDARD 11)
else()
	set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Retail modules link the CRT statically, so keep that property here too.
if(MSVC)
	if(CODUOMP_STATIC_VCRT)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	else()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	endif()
endif()

#-----------------------------------------------------------------
# Output layout
#
# Everything stays inside the CMake binary directory (.workbench/build/<name>
# in the documented invocation). The executable and its modules are staged
# together because the engine resolves cgame/ui/game from <basepath>/uo/.
#-----------------------------------------------------------------
set(CODUOMP_BIN_ROOT "${CMAKE_BINARY_DIR}/bin")
set(CODUOMP_LIB_ROOT "${CMAKE_BINARY_DIR}/lib")

#-----------------------------------------------------------------
# Helpers
#-----------------------------------------------------------------

# coduomp_target_output(<target> [<subdir>])
#
# Stage the target in bin/<config>[/<subdir>] and its import library in
# lib/<config>, keeping the layout of every configuration predictable. The
# runtime directory also receives native shared libraries, which CMake keeps in
# LIBRARY_OUTPUT_DIRECTORY rather than RUNTIME_OUTPUT_DIRECTORY.
function(coduomp_target_output target)
	set(_subdir "${ARGN}")
	if(_subdir)
		set(_runtime "${CODUOMP_BIN_ROOT}/$<CONFIG>/${_subdir}")
	else()
		set(_runtime "${CODUOMP_BIN_ROOT}/$<CONFIG>")
	endif()
	set_target_properties(${target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${_runtime}"
		LIBRARY_OUTPUT_DIRECTORY "${_runtime}"
		ARCHIVE_OUTPUT_DIRECTORY "${CODUOMP_LIB_ROOT}/$<CONFIG>")
	set(${target}_RUNTIME_DIR "${_runtime}" PARENT_SCOPE)
endfunction()

# coduomp_apply_common(<target>)
#
# Keep compiler policy on product targets; dependency projects own their flags.
function(coduomp_apply_common target)
	if(MSVC)
		target_compile_options(${target} PRIVATE
			/W1 /utf-8 /wd5287 /Od
			# Script error propagation deliberately crosses extern "C" entries.
			$<$<COMPILE_LANGUAGE:CXX>:/EHsc->)
		if(CODUOMP_32BIT_X86)
			target_compile_options(${target} PRIVATE /arch:IA32)
		endif()
		target_compile_definitions(${target} PRIVATE
			_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
			_WINSOCK_DEPRECATED_NO_WARNINGS)
		if(CODUOMP_CLANG_CL)
			foreach(_flag IN LISTS CODUOMP_EXTENDED_FLAGS)
				target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C,CXX>:${_flag}>")
			endforeach()
			target_compile_options(${target} PRIVATE
				"$<$<COMPILE_LANGUAGE:C,CXX>:/clang:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>")
			target_include_directories(${target} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/src/compat/clangcl")
		endif()
	else()
		target_compile_options(${target} PRIVATE
			-Wall -Wextra -Werror -pedantic
			-fsigned-char -fno-short-enums -fwrapv
			${CODUOMP_FP_FLAGS}
			# The scripting sources are C++; the Makefile target compiles them
			# with -Wno-pedantic because the recovered headers use C idioms.
			$<$<COMPILE_LANGUAGE:CXX>:-Wno-pedantic>
			$<$<COMPILE_LANGUAGE:CXX>:-Wno-c99-extensions>)
		target_compile_definitions(${target} PRIVATE
			${CODUOMP_PLATFORM_DEFINES})
	endif()
endfunction()

# coduomp_absolute_sources(<out-var> <source>...)
function(coduomp_absolute_sources out_var)
	set(_absolute "")
	foreach(_source IN LISTS ARGN)
		if(IS_ABSOLUTE "${_source}")
			set(_path "${_source}")
		else()
			set(_path "${CMAKE_SOURCE_DIR}/${_source}")
		endif()
		cmake_path(NORMAL_PATH _path)
		list(APPEND _absolute "${_path}")
	endforeach()
	list(REMOVE_DUPLICATES _absolute)
	set(${out_var} "${_absolute}" PARENT_SCOPE)
endfunction()

# coduomp_group_sources(<source>...)
#
# Recreate the source tree in the Visual Studio filters.
function(coduomp_group_sources)
	if(ARGN)
		source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${ARGN})
	endif()
endfunction()
