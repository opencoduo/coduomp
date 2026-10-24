#-----------------------------------------------------------------
# CoDUOMP Visual Studio build
#
#   mkdir build && cd build
#   cmake -G "Visual Studio 17 2022" -A Win32 -DBUNDLED_LIBS=YES ..
#   MSBuild.exe CoDUOMP.sln /m /p:Configuration=Release /p:Platform=Win32
#
# A single-configuration generator also works:
#
#   cmake -G "NMake Makefiles" -DBUNDLED_LIBS=YES .. && nmake
#
# The Makefiles (Makefile + build-mk/) stay authoritative for the other
# platforms. This project provides the equivalent MSVC solution so the recovered
# source can be edited, navigated, debugged and built from Visual Studio with
# cl.exe. Source membership is read from build-mk/ instead of being duplicated
# here.
#
# The recovered sources are shared with the GCC-compatible Makefile builds.
# MSVC-specific adaptations stay next to the code they affect: the
# offsetof(<type>, <member>[<index>]) layout checks are GCC/clang-only, the game
# module selects src/compat/libm/coduo_libm_msvc.c because MSVC has no 80-bit
# long double or GNU inline assembly, and cmake/CoduompMsvcCompat.h supplies the
# Microsoft CRT names the shared sources use. The dedicated server target is off
# by default because its Windows build relies on the Makefile's MinGW target.
#
# Targets:
#   CoDUOMP                   client executable
#   uo_cgame_mp_x86           client game module
#   uo_ui_mp_x86              client UI module
#   uo_game_mp_x86            listen-server game module
#   coduo_dedicated_recovered dedicated server executable
#
# Third-party libraries are built from libs/ by default. The first configure
# fetches the pinned archives into libs/<name> and later configures reuse them,
# so builds work offline once the tree is populated. Pass
# -DBUNDLED_LIBS=OFF to use system libraries instead (or supply a prefix with
# -DCODUOMP_DEP_PREFIX=<path>).
#
# The retail client is a 32-bit x86 PE image. Generate the Win32 configuration
# (cmake -A Win32) so float expressions use the x87 FPU like the original and
# the module names match what the executable loads. A 64-bit configuration is
# accepted for editing, but MSVC cannot produce an 80-bit long double or x87
# code there, so its floating-point results are not retail-equivalent.
#
# Please use TABs to indent! (x4)
#-----------------------------------------------------------------
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

# Deprecation diagnostics from the bundled third-party projects (SDL2 still
# declares pre-3.10 compatibility) are not actionable here. The unified
# diagnostic API exists in CMake 4.4; older CMake reads the variable that
# libs/CMakeLists.txt sets for the same purpose.
if(POLICY CMP0218)
	cmake_policy(SET CMP0218 NEW)
endif()
if(COMMAND cmake_diagnostic)
	cmake_diagnostic(SET CMD_DEPRECATED IGNORE)
endif()

# Single-configuration generators (NMake Makefiles) need an explicit build type.
# CMake seeds "Debug" itself inside project(); choose Release before that, so
# the etlegacy-style `cmake -G "NMake Makefiles" .. && nmake` builds the release
# configuration the Makefile targets use. An explicit -DCMAKE_BUILD_TYPE wins.
if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build")
endif()

project(CoDUOMP C CXX RC)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Group the generated projects and their sources like upstream etlegacy does.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

#-----------------------------------------------------------------
# Options
#-----------------------------------------------------------------
option(CODUOMP_BUILD_CLIENT		"Build the CoDUOMP client executable"						ON)
option(CODUOMP_BUILD_MODULES	"Build the cgame, UI and listen-server game modules"		ON)
# The standalone server reproduces the Linux server and relies on POSIX
# headers (unistd.h, dirent.h, sys/time.h) that MSVC does not provide. Its
# Windows build is the Makefile's MinGW target; enable this once an MSVC POSIX
# shim exists.
option(CODUOMP_BUILD_SERVER		"Build the dedicated server executable"					OFF)

option(CODUOMP_STATIC_VCRT		"Link the MSVC runtime statically (/MT)"					ON)
option(BUNDLED_LIBS				"Build the third-party libraries bundled in libs/"			ON)
option(CODUOMP_CURL_STATIC		"Define CURL_STATICLIB for a static libcurl"				ON)
option(CODUOMP_RENDERER_GPU_PROFILE	"Compile the asynchronous renderer GPU timing path"		OFF)
option(CODUOMP_ENABLE_PUNKBUSTER	"Compile PunkBuster recovery code into the server"		OFF)
option(CODUOMP_DISABLE_SERVER_AUTH	"Build the server without external CD-key authorization"	ON)

set(CODUOMP_DEP_PREFIX "" CACHE PATH
	"Prefix containing MSVC dependencies (include/, lib/)")

#-----------------------------------------------------------------
# Configuration
#-----------------------------------------------------------------
include(cmake/CoduompManifest.cmake)
include(cmake/CoduompCommon.cmake)
include(cmake/CoduompPlatform.cmake)
include(cmake/CoduompBundledLibs.cmake)
include(cmake/CoduompDependencies.cmake)
include(cmake/CoduompSources.cmake)
include(cmake/CoduompTargets.cmake)

#-----------------------------------------------------------------
# Summary
#-----------------------------------------------------------------
if(CMAKE_GENERATOR_PLATFORM)
	message(STATUS "Generator:      ${CMAKE_GENERATOR} (${CMAKE_GENERATOR_PLATFORM})")
else()
	message(STATUS "Generator:      ${CMAKE_GENERATOR}")
endif()
message(STATUS "Target:         ${CODUOMP_ARCH} (${CODUOMP_POINTER_BITS}-bit pointers)")
message(STATUS "Modules:        ${CODUOMP_BUILD_MODULES}")
if(CODUOMP_BUILD_CLIENT AND NOT CODUOMP_HAVE_CLIENT_DEPS)
	message(STATUS "Client:         ${CODUOMP_BUILD_CLIENT} (skipped by default, missing ${CODUOMP_MISSING_CLIENT_DEPS})")
else()
	message(STATUS "Client:         ${CODUOMP_BUILD_CLIENT}")
endif()
message(STATUS "Server:         ${CODUOMP_BUILD_SERVER}")
if(BUNDLED_LIBS)
	message(STATUS "Libraries:      bundled (libs/)")
else()
	message(STATUS "Libraries:      system")
endif()
message(STATUS "Output:         ${CODUOMP_BIN_ROOT}/<config>")
message(STATUS "")
