if(NOT CODUOMP_BUILD_CLIENT)
    return()
endif()

#-----------------------------------------------------------------
# Third-party dependencies
#
# The Makefiles obtain these libraries from pkg-config (native builds) or from
# a MinGW dependency prefix. Neither works for MSVC, so this module prefers a
# CMake package (vcpkg or an installed config) and otherwise searches the prefix
# supplied with -DCODUOMP_DEP_PREFIX=<prefix> (include/ and lib/, x86 libraries
# for a Win32 configuration).
#
# MinGW archives are rejected: their objects carry a different C ABI and
# runtime, so an accidental match would only produce confusing link errors.
#-----------------------------------------------------------------

# coduomp_reject_mingw_archive(<cache-var>)
function(coduomp_reject_mingw_archive cache_var)
	if(NOT DEFINED ${cache_var})
		return()
	endif()
	set(_path "${${cache_var}}")
	if(_path STREQUAL "")
		return()
	endif()
	if(_path MATCHES "[.][dD][lL][lL][.][aA]$" OR _path MATCHES "[.][aA]$")
		message(WARNING
			"CoDUOMP: ignoring MinGW archive ${_path} (${cache_var}); "
			"MSVC needs a COFF .lib built for the MSVC runtime")
		unset(${cache_var})
		unset(${cache_var} CACHE)
	endif()
endfunction()

# coduomp_resolve_dependency(<key>
#     [PACKAGE <find_package-name> [TARGET <imported-target>]]
#     HEADER <header-path> LIBRARIES <library-name>... [PATH_SUFFIXES <dir>...])
#
# Produces CODUOMP_<KEY>_FOUND, CODUOMP_<KEY>_INCLUDE_DIRS and
# CODUOMP_<KEY>_LIBRARIES in the caller's scope.
function(coduomp_resolve_dependency key)
	cmake_parse_arguments(ARG "" "PACKAGE;TARGET;HEADER"
		"LIBRARIES;PATH_SUFFIXES" ${ARGN})

	set(_include_dirs "")
	set(_libraries "")
	set(_found FALSE)

	if(ARG_PACKAGE)
		find_package(${ARG_PACKAGE} QUIET)
		if(ARG_TARGET AND TARGET ${ARG_TARGET})
			set(_libraries "${ARG_TARGET}")
			set(_found TRUE)
		endif()
	endif()

	if(NOT _found)
		set(_hints "")
		if(CODUOMP_DEP_PREFIX)
			list(APPEND _hints "${CODUOMP_DEP_PREFIX}")
		endif()
		set(_include_suffixes include include/SDL2 include/curl include/minizip)
		list(APPEND _include_suffixes ${ARG_PATH_SUFFIXES})
		set(_library_suffixes lib lib/${CODUOMP_ARCH} lib/Release lib/Debug)

		find_path(CODUOMP_${key}_INCLUDE_DIR
			NAMES ${ARG_HEADER}
			HINTS ${_hints}
			PATH_SUFFIXES ${_include_suffixes})
		find_library(CODUOMP_${key}_LIBRARY
			NAMES ${ARG_LIBRARIES}
			HINTS ${_hints}
			PATH_SUFFIXES ${_library_suffixes})
		coduomp_reject_mingw_archive(CODUOMP_${key}_LIBRARY)

		if(CODUOMP_${key}_INCLUDE_DIR AND CODUOMP_${key}_LIBRARY)
			set(_include_dirs "${CODUOMP_${key}_INCLUDE_DIR}")
			set(_libraries "${CODUOMP_${key}_LIBRARY}")
			set(_found TRUE)
		endif()
	endif()

	set(CODUOMP_${key}_FOUND "${_found}" PARENT_SCOPE)
	set(CODUOMP_${key}_INCLUDE_DIRS "${_include_dirs}" PARENT_SCOPE)
	set(CODUOMP_${key}_LIBRARIES "${_libraries}" PARENT_SCOPE)
endfunction()

if(CODUOMP_DEP_PREFIX)
	list(PREPEND CMAKE_PREFIX_PATH "${CODUOMP_DEP_PREFIX}")
endif()

# cmake/CoduompBundledLibs.cmake already supplied the targets for every bundled
# library; only look for system copies of the remaining ones. The client-only
# dependencies are skipped entirely when the client is not built.
if(CODUOMP_BUILD_CLIENT)
	if(NOT CODUOMP_SDL2_FOUND)
		coduomp_resolve_dependency(SDL2
			PACKAGE SDL2 TARGET SDL2::SDL2
			HEADER SDL.h
			LIBRARIES SDL2 SDL2-static
			PATH_SUFFIXES include/SDL2)
	endif()
	if(NOT CODUOMP_JPEG_FOUND)
		coduomp_resolve_dependency(JPEG
			PACKAGE JPEG TARGET JPEG::JPEG
			HEADER jpeglib.h
			LIBRARIES jpeg jpeg-static)
	endif()
	if(NOT CODUOMP_CURL_FOUND)
		coduomp_resolve_dependency(CURL
			PACKAGE CURL TARGET CURL::libcurl
			HEADER curl/curl.h
			LIBRARIES libcurl curl)
	endif()
	if(NOT CODUOMP_MINIZIP_FOUND)
		coduomp_resolve_dependency(MINIZIP
			HEADER unzip.h
			LIBRARIES minizip minizip-ng
			PATH_SUFFIXES include/minizip)
	endif()
endif()
if(NOT CODUOMP_ZLIB_FOUND)
	coduomp_resolve_dependency(ZLIB
		PACKAGE ZLIB TARGET ZLIB::ZLIB
		HEADER zlib.h
		LIBRARIES zlib zlibstatic zlib1)
endif()

# The client executable needs all of them; the dedicated server needs only zlib.
set(CODUOMP_HAVE_CLIENT_DEPS TRUE)
set(CODUOMP_MISSING_CLIENT_DEPS "")
foreach(_dependency SDL2 JPEG CURL MINIZIP ZLIB)
	if(NOT CODUOMP_${_dependency}_FOUND)
		set(CODUOMP_HAVE_CLIENT_DEPS FALSE)
		list(APPEND CODUOMP_MISSING_CLIENT_DEPS ${_dependency})
	endif()
endforeach()

if(CODUOMP_BUILD_CLIENT AND NOT CODUOMP_HAVE_CLIENT_DEPS)
    message(FATAL_ERROR
        "Missing client dependencies: ${CODUOMP_MISSING_CLIENT_DEPS}. "
        "Use BUNDLED_LIBS=ON or provide matching MSVC libraries via CODUOMP_DEP_PREFIX.")
endif()

if(CODUOMP_BUILD_SERVER AND NOT CODUOMP_ZLIB_FOUND)
    message(FATAL_ERROR
        "The dedicated server needs zlib. "
        "Use BUNDLED_LIBS=ON or provide a matching MSVC zlib via CODUOMP_DEP_PREFIX.")
endif()
