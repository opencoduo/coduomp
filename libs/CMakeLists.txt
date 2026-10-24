#-----------------------------------------------------------------
# Bundled third-party libraries
#
# Build every dependency the Windows build needs from this directory, like the
# etlegacy libs/ tree. Each library is pinned to a release commit and fetched
# from its GitHub archive on the first configure; later configures reuse the
# checkout, so a build works offline once populated. Delete a directory to
# re-fetch it.
#
#   zlib       https://github.com/madler/zlib
#   minizip    built from zlib/contrib/minizip
#   sdl2       https://github.com/libsdl-org/SDL (SDL2 branch)
#   jpeg-turbo https://github.com/libjpeg-turbo/libjpeg-turbo
#   curl       https://github.com/curl/curl
#
# Targets exposed to the rest of the build:
#   ZLIB::ZLIBSTATIC, coduomp_minizip, SDL2::SDL2-static, coduomp_jpeg, libcurl
#
# Disable with -DBUNDLED_LIBS=OFF to use system libraries instead.
#-----------------------------------------------------------------

include(FetchContent)
include(ExternalProject)

# SDL2 and zlib declare old CMake compatibility ranges; CMake 4 refuses to
# configure a project that asks for less than 3.5 unless the floor is raised.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# FetchContent_Populate() is the only way to obtain a source tree without
# adding it as a subdirectory, which libjpeg-turbo requires.
if(POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()

# Downloaded third-party projects are not part of this project's test matrix,
# and their CMake files predate current policies; keep the configure output
# focused on this project.
set(BUILD_TESTING OFF CACHE BOOL "Build third-party tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build third-party shared libraries" FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1)
# CMake older than 4.4 (including the version shipped with Visual Studio)
# controls deprecation messages through this variable.
set(CMAKE_WARN_DEPRECATED OFF)

# coduomp_bundled_fetch(<name> <directory> <url> <sha256>)
#
# Populate libs/<directory> from a pinned GitHub archive when it is missing.
# Archives pin the exact bytes (URL + URL_HASH) and need no git installation,
# so a clean checkout only requires CMake and curl.
function(coduomp_bundled_fetch name directory url sha256)
	set(_source "${CMAKE_CURRENT_LIST_DIR}/${directory}")
	if(EXISTS "${_source}/CMakeLists.txt")
		message(STATUS "Using bundled ${name} at libs/${directory}")
	else()
		message(STATUS "Downloading ${name} into libs/${directory}")
	endif()
	FetchContent_Declare(${name}
		URL "${url}"
		URL_HASH "SHA256=${sha256}"
		SOURCE_DIR "${_source}"
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE
		DOWNLOAD_NO_PROGRESS TRUE)
	FetchContent_Populate(${name})
endfunction()

#-----------------------------------------------------------------
# zlib
#-----------------------------------------------------------------
# zlib v1.3.2
coduomp_bundled_fetch(zlib zlib
	"https://github.com/madler/zlib/archive/da607da739fa6047df13e66a2af6b8bec7c2a498.zip"
	"21DE001766ABB1F5BFDFB3C23AA42D30A6BBD7EDDD37131D5E7FE89C5EC068F8")
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Build zlib shared library" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "Build zlib static library" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Build zlib tests" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "Install zlib" FORCE)
add_subdirectory(
	"${CMAKE_CURRENT_LIST_DIR}/zlib"
	"${CMAKE_BINARY_DIR}/libs/zlib"
	EXCLUDE_FROM_ALL)

#-----------------------------------------------------------------
# minizip (zlib/contrib/minizip)
#
# zlib's contributed CMake project refuses 32-bit Windows builds ("libminizip
# needs 64bit") and this project builds Win32, so compile the same sources as a
# project-owned static library.
#-----------------------------------------------------------------
set(_minizip_dir "${CMAKE_CURRENT_LIST_DIR}/zlib/contrib/minizip")
set(_minizip_sources
	"${_minizip_dir}/ioapi.c"
	"${_minizip_dir}/mztools.c"
	"${_minizip_dir}/unzip.c"
	"${_minizip_dir}/zip.c")
if(WIN32)
	list(APPEND _minizip_sources "${_minizip_dir}/iowin32.c")
endif()
add_library(coduomp_minizip STATIC ${_minizip_sources})
target_include_directories(coduomp_minizip PUBLIC "${_minizip_dir}")
target_link_libraries(coduomp_minizip PUBLIC ZLIB::ZLIBSTATIC)

#-----------------------------------------------------------------
# SDL2
#-----------------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "Build SDL2 shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL2 static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Build SDL2 tests" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL2 tests" FORCE)
# SDL's Direct3D renderer defines a global MatrixMultiply that collides with the
# engine's recovered math symbol. The client only needs the window/GL layer.
set(SDL_RENDER_D3D OFF CACHE BOOL "Build the SDL Direct3D renderer" FORCE)
set(SDL_RENDER_D3D11 OFF CACHE BOOL "Build the SDL Direct3D 11 renderer" FORCE)
# SDL2 2.30.9
coduomp_bundled_fetch(sdl2 sdl2
	"https://github.com/libsdl-org/SDL/archive/c98c4fbff6d8f3016a3ce6685bf8f43433c3efcc.zip"
	"516E788F6BCD74247AA392D656B1EF89B8A416DD66549AC6825993AC9B1068FA")
add_subdirectory(
	"${CMAKE_CURRENT_LIST_DIR}/sdl2"
	"${CMAKE_BINARY_DIR}/libs/sdl2"
	EXCLUDE_FROM_ALL)

#-----------------------------------------------------------------
# libjpeg-turbo
#
# Upstream refuses add_subdirectory() by design and asks downstream builds to
# use ExternalProject_Add(), so build it as the standalone project it expects.
#-----------------------------------------------------------------
# libjpeg-turbo 3.1.4
coduomp_bundled_fetch(jpeg_turbo jpeg-turbo
	"https://github.com/libjpeg-turbo/libjpeg-turbo/archive/e352b02f794f701407b39af08576035ba3360d60.zip"
	"BAC5A0E60BB5D7C33F197911576250D20EFA90F66520B9F0401C81242329D6F8")
set(_jpeg_source "${CMAKE_CURRENT_LIST_DIR}/jpeg-turbo")
set(_jpeg_binary "${CMAKE_BINARY_DIR}/libs/jpeg-turbo-build")
get_property(_jpeg_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_jpeg_multi_config)
	set(_jpeg_archive "${_jpeg_binary}/$<CONFIG>/jpeg-static.lib")
else()
	set(_jpeg_archive "${_jpeg_binary}/jpeg-static.lib")
endif()
ExternalProject_Add(coduomp_jpeg_turbo
	SOURCE_DIR "${_jpeg_source}"
	BINARY_DIR "${_jpeg_binary}"
	DOWNLOAD_COMMAND ""
	UPDATE_COMMAND ""
	CMAKE_ARGS
		-DENABLE_SHARED=OFF
		-DENABLE_STATIC=ON
		-DWITH_SIMD=OFF
		-DWITH_TURBOJPEG=OFF
		-DWITH_TOOLS=OFF
		-DWITH_TESTS=OFF
		-DWITH_CRT_DLL=OFF
	BUILD_BYPRODUCTS "${_jpeg_archive}"
	INSTALL_COMMAND ""
	LOG_CONFIGURE ON
	LOG_BUILD ON)
add_library(coduomp_jpeg INTERFACE)
target_include_directories(coduomp_jpeg INTERFACE
	"${_jpeg_source}/src"
	"${_jpeg_binary}")
target_link_libraries(coduomp_jpeg INTERFACE "${_jpeg_archive}")
add_dependencies(coduomp_jpeg coduomp_jpeg_turbo)

#-----------------------------------------------------------------
# OpenGL / KHR extension headers (Khronos registries)
#
# The Windows SDK ships GL/gl.h but not GL/glext.h, GL/wglext.h or the
# KHR/khrplatform.h they include. Pin the registry commits and fetch the three
# headers next to the other bundled sources.
#-----------------------------------------------------------------
set(_gl_dir "${CMAKE_CURRENT_LIST_DIR}/gl")
set(_gl_files
	"GL/glext.h|OpenGL-Registry|6af574a14089ccfee87efe230ebcdd8742859813|D7B6F15B1BB209E08BD6ADA36983F8BB40429E66C24083AA2474D55205D97047"
	"GL/wglext.h|OpenGL-Registry|6af574a14089ccfee87efe230ebcdd8742859813|F6830DD670574C46DD58B119A21D857EDC1DD161CBA23E8644DFFB6920999F68"
	"KHR/khrplatform.h|EGL-Registry|e964211ff5f7c2bcada786954a16d5b813379395|7B1E01AAA7AD8F6FC34B5C7BDF79EBF5189BB09E2C4D2E79FC5D350623D11E83")
foreach(_gl_entry IN LISTS _gl_files)
	string(REPLACE "|" ";" _gl_parts "${_gl_entry}")
	list(GET _gl_parts 0 _gl_path)
	list(GET _gl_parts 1 _gl_repo)
	list(GET _gl_parts 2 _gl_commit)
	list(GET _gl_parts 3 _gl_sha256)
	if(NOT EXISTS "${_gl_dir}/${_gl_path}")
		message(STATUS "Downloading ${_gl_path} into libs/gl")
		get_filename_component(_gl_parent "${_gl_dir}/${_gl_path}" DIRECTORY)
		file(MAKE_DIRECTORY "${_gl_parent}")
		file(DOWNLOAD
			"https://raw.githubusercontent.com/KhronosGroup/${_gl_repo}/${_gl_commit}/api/${_gl_path}"
			"${_gl_dir}/${_gl_path}"
			EXPECTED_HASH SHA256=${_gl_sha256}
			STATUS _gl_status)
		list(GET _gl_status 0 _gl_code)
		if(NOT _gl_code EQUAL 0)
			message(FATAL_ERROR
				"CoDUOMP: failed to download ${_gl_path}: ${_gl_status}")
		endif()
	endif()
endforeach()

#-----------------------------------------------------------------
# libcurl
#-----------------------------------------------------------------
# Schannel provides HTTPS without a separate TLS library. The recovered client
# needs FTP, HTTP and HTTPS; the remaining protocols and optional features are
# disabled so the bundled build has no further dependencies.
set(CURL_USE_SCHANNEL ON CACHE BOOL "Use Schannel for TLS" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "Use OpenSSL for TLS" FORCE)
set(CURL_USE_MBEDTLS OFF CACHE BOOL "Use mbedTLS for TLS" FORCE)
set(CURL_USE_WOLFSSL OFF CACHE BOOL "Use wolfSSL for TLS" FORCE)
set(CURL_USE_GNUTLS OFF CACHE BOOL "Use GnuTLS for TLS" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "Use libpsl" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "Use libssh2" FORCE)
set(CURL_USE_GSSAPI OFF CACHE BOOL "Use GSSAPI" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "Use zlib for content encoding" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "Use brotli" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "Use zstd" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "Disable LDAP" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "Disable LDAPS" FORCE)
set(CURL_DISABLE_TELNET ON CACHE BOOL "Disable TELNET" FORCE)
set(CURL_DISABLE_DICT ON CACHE BOOL "Disable DICT" FORCE)
set(CURL_DISABLE_SMB ON CACHE BOOL "Disable SMB" FORCE)
set(CURL_DISABLE_SMTP ON CACHE BOOL "Disable SMTP" FORCE)
set(CURL_DISABLE_POP3 ON CACHE BOOL "Disable POP3" FORCE)
set(CURL_DISABLE_IMAP ON CACHE BOOL "Disable IMAP" FORCE)
set(CURL_DISABLE_MQTT ON CACHE BOOL "Disable MQTT" FORCE)
set(CURL_DISABLE_RTSP ON CACHE BOOL "Disable RTSP" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "Build the curl tool" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build curl examples" FORCE)
# No Perl in the build environment and no manuals in a game dependency.
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "Build libcurl man pages" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "Build misc man pages" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "Build the curl manual" FORCE)
# curl 8.19.0
coduomp_bundled_fetch(curl curl
	"https://github.com/curl/curl/archive/8c908d2d0a6d32abdedda2c52e90bd56ec76c24d.zip"
	"084E07B0F62FEDB97D5D8E569F4ECC36B025D69657C0C400B89E71C0E24FE753")
add_subdirectory(
	"${CMAKE_CURRENT_LIST_DIR}/curl"
	"${CMAKE_BINARY_DIR}/libs/curl"
	EXCLUDE_FROM_ALL)
