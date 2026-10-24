#-----------------------------------------------------------------
# Build targets
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Client modules
#-----------------------------------------------------------------
if(CODUOMP_BUILD_MODULES)

	#-------------------------------------------------------------
	# uo_cgame_mp_x86
	#-------------------------------------------------------------
	coduomp_absolute_sources(CODUOMP_CGAME_ABSOLUTE ${CODUOMP_CGAME_SOURCES})
	add_library(uo_cgame_mp_x86 SHARED
		${CODUOMP_CGAME_ABSOLUTE}
		"${CMAKE_SOURCE_DIR}/build-mk/abi/cgame/uo_cgame_mp_x86.def")
	coduomp_target_output(uo_cgame_mp_x86 uo)
	coduomp_apply_common(uo_cgame_mp_x86)
	set_target_properties(uo_cgame_mp_x86 PROPERTIES
		OUTPUT_NAME "uo_cgame_mp_x86"
		PREFIX ""
		FOLDER "modules")
	target_compile_definitions(uo_cgame_mp_x86 PRIVATE
		WINDOWS_BEHAVIOR
		EMULATE_X87=${CODUOMP_EMULATE_X87})
	target_include_directories(uo_cgame_mp_x86 PRIVATE
		"${CMAKE_SOURCE_DIR}/src/client/cgame/bindings"
		"${CMAKE_SOURCE_DIR}/src/qcommon/bindings/default"
		"${CMAKE_SOURCE_DIR}/src")
	coduomp_group_sources(${CODUOMP_CGAME_ABSOLUTE})

	#-------------------------------------------------------------
	# uo_ui_mp_x86
	#-------------------------------------------------------------
	coduomp_absolute_sources(CODUOMP_UI_ABSOLUTE ${CODUOMP_UI_SOURCES})
	add_library(uo_ui_mp_x86 SHARED ${CODUOMP_UI_ABSOLUTE})
	coduomp_target_output(uo_ui_mp_x86 uo)
	coduomp_apply_common(uo_ui_mp_x86)
	set_target_properties(uo_ui_mp_x86 PROPERTIES
		OUTPUT_NAME "uo_ui_mp_x86"
		PREFIX ""
		FOLDER "modules")
	target_compile_definitions(uo_ui_mp_x86 PRIVATE
		WINDOWS_BEHAVIOR
		EMULATE_X87=${CODUOMP_EMULATE_X87})
	target_include_directories(uo_ui_mp_x86 PRIVATE
		"${CMAKE_SOURCE_DIR}/src/client/ui/bindings"
		"${CMAKE_SOURCE_DIR}/src/qcommon/bindings/default"
		"${CMAKE_SOURCE_DIR}/src")
	target_link_libraries(uo_ui_mp_x86 PRIVATE ${CODUOMP_UI_SYSTEM_LIBS})
	coduomp_group_sources(${CODUOMP_UI_ABSOLUTE})

	#-------------------------------------------------------------
	# uo_game_mp_x86
	#-------------------------------------------------------------
	coduomp_absolute_sources(CODUOMP_GAME_ABSOLUTE ${CODUOMP_GAME_SOURCES})
	# The reconstructed libm bodies need an 80-bit long double and GNU x87
	# inline assembly. MSVC has neither, so it uses the CRT entry points the
	# retail modules were built against (src/compat/libm/coduo_libm_msvc.c).
	list(FILTER CODUOMP_GAME_ABSOLUTE EXCLUDE REGEX
		"/src/compat/libm/coduo_libm_(asin|ftol|sincos|sqrt)[.]c$")
	list(APPEND CODUOMP_GAME_ABSOLUTE
		"${CMAKE_SOURCE_DIR}/src/compat/libm/coduo_libm_msvc.c")
	add_library(uo_game_mp_x86 SHARED
		${CODUOMP_GAME_ABSOLUTE}
		"${CMAKE_SOURCE_DIR}/build-mk/abi/game/uo_game_mp_x86.def")
	coduomp_target_output(uo_game_mp_x86 uo)
	coduomp_apply_common(uo_game_mp_x86)
	set_target_properties(uo_game_mp_x86 PROPERTIES
		OUTPUT_NAME "uo_game_mp_x86"
		PREFIX ""
		FOLDER "modules")
	# The module exports dllEntry/vmMain through the ABI definition file in
	# build-mk/abi. MSVC has no 80-bit long double, so the retail long-double
	# equivalence assert is explicitly relaxed for this build.
	target_compile_definitions(uo_game_mp_x86 PRIVATE
		WINDOWS_BEHAVIOR
		EMULATE_X87=${CODUOMP_EMULATE_X87}
		GAME_REQUIRE_X87_LONG_DOUBLE=0
		GAME_ALLOW_NON_EQUIVALENT_LONG_DOUBLE=1)
	target_include_directories(uo_game_mp_x86 PRIVATE
		"${CMAKE_SOURCE_DIR}/src/server/game/bindings"
		"${CMAKE_SOURCE_DIR}/src/qcommon/bindings/default"
		"${CMAKE_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/vendor/softfloat/source/include")
	coduomp_group_sources(${CODUOMP_GAME_ABSOLUTE})

endif()

#-----------------------------------------------------------------
# CoDUOMP (client executable)
#-----------------------------------------------------------------
if(CODUOMP_BUILD_CLIENT)

	coduomp_absolute_sources(CODUOMP_CLIENT_ABSOLUTE
		${CODUOMP_CLIENT_C_SOURCES}
		${CODUOMP_CLIENT_CXX_SOURCES}
		"src/client/engine/platform/windows_resources.rc")

	if(CODUOMP_HAVE_CLIENT_DEPS)
		add_executable(CoDUOMP WIN32 ${CODUOMP_CLIENT_ABSOLUTE})
	else()
		# The project stays in the solution, but MSBuild skips it until the
		# dependencies are provided.
		add_executable(CoDUOMP WIN32 EXCLUDE_FROM_ALL ${CODUOMP_CLIENT_ABSOLUTE})
		coduomp_exclude_from_build(CoDUOMP)
	endif()

	coduomp_target_output(CoDUOMP)
	coduomp_apply_common(CoDUOMP)
	set_target_properties(CoDUOMP PROPERTIES
		OUTPUT_NAME "CoDUOMP"
		FOLDER "client"
		VS_DEBUGGER_WORKING_DIRECTORY "${CODUOMP_BIN_ROOT}/$<CONFIG>")

	target_compile_definitions(CoDUOMP PRIVATE
		WINDOWS_BEHAVIOR
		CASE_SENSITIVE_FS=0
		EMULATE_X87=${CODUOMP_EMULATE_X87}
		AUDIO_BACKEND_MINIAUDIO
		$<$<BOOL:${CODUOMP_CURL_STATIC}>:CURL_STATICLIB>
		$<$<BOOL:${CODUOMP_RENDERER_GPU_PROFILE}>:CODUOMP_RENDERER_GPU_PROFILE=1>)

	target_include_directories(CoDUOMP PRIVATE
		"${CMAKE_SOURCE_DIR}/src/client/engine/bindings"
		"${CMAKE_SOURCE_DIR}/src/qcommon/bindings/default"
		"${CMAKE_SOURCE_DIR}/src"
		# windows_resources.rc references assets/windows/coduomp.ico.
		"${CMAKE_SOURCE_DIR}"
		${CODUOMP_GL_INCLUDE_DIR}
		${CODUOMP_SDL2_INCLUDE_DIRS}
		${CODUOMP_JPEG_INCLUDE_DIRS}
		${CODUOMP_CURL_INCLUDE_DIRS}
		${CODUOMP_MINIZIP_INCLUDE_DIRS}
		${CODUOMP_ZLIB_INCLUDE_DIRS})

	if(CODUOMP_HAVE_CLIENT_DEPS)
		target_link_libraries(CoDUOMP PRIVATE
			${CODUOMP_SDL2_LIBRARIES}
			${CODUOMP_JPEG_LIBRARIES}
			${CODUOMP_CURL_LIBRARIES}
			${CODUOMP_MINIZIP_LIBRARIES}
			${CODUOMP_ZLIB_LIBRARIES}
			${CODUOMP_CLIENT_SYSTEM_LIBS})
	endif()

	# The retail executable reserves 8 MiB of stack with a 4 KiB initial commit;
	# renderer paths stage multi-megabyte vertex arrays on the stack.
	target_link_options(CoDUOMP PRIVATE "/STACK:8388608,4096")

	if(TARGET uo_cgame_mp_x86)
		add_dependencies(CoDUOMP uo_cgame_mp_x86 uo_ui_mp_x86 uo_game_mp_x86)
	endif()

	coduomp_group_sources(${CODUOMP_CLIENT_ABSOLUTE})

	set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT CoDUOMP)

endif()

#-----------------------------------------------------------------
# coduo_dedicated_recovered (dedicated server)
#-----------------------------------------------------------------
if(CODUOMP_BUILD_SERVER)

	coduomp_absolute_sources(CODUOMP_SERVER_ABSOLUTE ${CODUOMP_SERVER_SOURCES})

	if(CODUOMP_ZLIB_FOUND)
		add_executable(coduo_dedicated_recovered ${CODUOMP_SERVER_ABSOLUTE})
	else()
		add_executable(coduo_dedicated_recovered EXCLUDE_FROM_ALL
			${CODUOMP_SERVER_ABSOLUTE})
		coduomp_exclude_from_build(coduo_dedicated_recovered)
	endif()

	coduomp_target_output(coduo_dedicated_recovered)
	coduomp_apply_common(coduo_dedicated_recovered)
	set_target_properties(coduo_dedicated_recovered PROPERTIES
		OUTPUT_NAME "coduo_dedicated_recovered"
		FOLDER "server"
		VS_DEBUGGER_WORKING_DIRECTORY "${CODUOMP_BIN_ROOT}/$<CONFIG>")

	# The standalone engine reproduces the retail Linux server's behavior even
	# when it is built as a Windows executable, exactly like the Makefile's
	# windows-i686 target.
	target_compile_definitions(coduo_dedicated_recovered PRIVATE
		LINUX_BEHAVIOR
		_WIN32_WINNT=0x0601
		CODUO_ENABLE_PUNKBUSTER=$<BOOL:${CODUOMP_ENABLE_PUNKBUSTER}>
		CODUO_DISABLE_SERVER_AUTH=$<BOOL:${CODUOMP_DISABLE_SERVER_AUTH}>
		EMULATE_X87=${CODUOMP_EMULATE_X87}
		CODUO_ENGINE_ALLOW_NON_X87_FLOAT=1)

	target_include_directories(coduo_dedicated_recovered PRIVATE
		"${CMAKE_SOURCE_DIR}/src/server/standalone/bindings"
		"${CMAKE_SOURCE_DIR}/src/qcommon/bindings/default"
		"${CMAKE_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/vendor/softfloat/source/include"
		${CODUOMP_ZLIB_INCLUDE_DIRS})

	if(CODUOMP_ZLIB_FOUND)
		target_link_libraries(coduo_dedicated_recovered PRIVATE
			${CODUOMP_ZLIB_LIBRARIES}
			${CODUOMP_SERVER_SYSTEM_LIBS})
	endif()

	coduomp_group_sources(${CODUOMP_SERVER_ABSOLUTE})

endif()
