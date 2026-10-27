#-----------------------------------------------------------------
# Bundled libraries
#
# libs/CMakeLists.txt holds every third-party dependency the Windows build
# needs and fetches the pinned sources under the CMake binary tree when they are missing.
# This module starts that build and publishes the targets through the same
# CODUOMP_<LIB>_* variables the system-library path uses.
#-----------------------------------------------------------------

if(NOT BUNDLED_LIBS)
	return()
endif()

if(NOT CODUOMP_BUILD_CLIENT AND NOT CODUOMP_BUILD_SERVER)
	return()
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/libs/CMakeLists.txt")
	message(FATAL_ERROR
		"CoDUOMP: BUNDLED_LIBS is enabled but "
		"${CMAKE_SOURCE_DIR}/libs/CMakeLists.txt is missing. "
		"Disable it with -DBUNDLED_LIBS=OFF to use system libraries.")
endif()

include("${CMAKE_SOURCE_DIR}/libs/CMakeLists.txt")

# zlib (the client executable and the dedicated server both link it)
set(CODUOMP_ZLIB_FOUND TRUE)
set(CODUOMP_ZLIB_INCLUDE_DIRS "")
set(CODUOMP_ZLIB_LIBRARIES ZLIB::ZLIBSTATIC)

# Client-only dependencies. A server-only configuration does not fetch or
# build minizip, SDL2, libjpeg-turbo, libcurl or the GL headers.
if(CODUOMP_BUILD_CLIENT)

# minizip (from the vendored zlib)
set(CODUOMP_MINIZIP_FOUND TRUE)
set(CODUOMP_MINIZIP_INCLUDE_DIRS "")
set(CODUOMP_MINIZIP_LIBRARIES coduomp_minizip)

# SDL2
set(CODUOMP_SDL2_FOUND TRUE)
set(CODUOMP_SDL2_INCLUDE_DIRS "")
set(CODUOMP_SDL2_LIBRARIES SDL2::SDL2-static)

# libjpeg-turbo (ExternalProject; the interface target carries headers and the
# per-configuration archive)
set(CODUOMP_JPEG_FOUND TRUE)
set(CODUOMP_JPEG_INCLUDE_DIRS "")
set(CODUOMP_JPEG_LIBRARIES coduomp_jpeg)

# libcurl: its static-link contract travels with the dependency target.
add_library(coduomp_curl INTERFACE)
target_link_libraries(coduomp_curl INTERFACE libcurl)
target_compile_definitions(coduomp_curl INTERFACE CURL_STATICLIB)
set(CODUOMP_CURL_FOUND TRUE)
set(CODUOMP_CURL_INCLUDE_DIRS "")
set(CODUOMP_CURL_LIBRARIES coduomp_curl)

# Khronos GL extension headers fetched into the binary-tree dependency cache
set(CODUOMP_GL_INCLUDE_DIR "${_gl_dir}")

endif()
