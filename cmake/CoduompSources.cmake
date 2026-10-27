#-----------------------------------------------------------------
# Source membership
#
# build-mk/ owns every target's source list. Read the same manifests the
# Makefiles include instead of keeping a second copy here.
#-----------------------------------------------------------------

# Client executable and client-module membership:
#   CLIENT_ENGINE_TARGET_*       CoDUOMP executable
#   CLIENT_ALL_C_SOURCES         uo_cgame_mp_x86
coduomp_manifest_read("build-mk/client-targets.mk")
set(CODUOMP_CGAME_SOURCES		${CLIENT_ALL_C_SOURCES})
set(CODUOMP_CLIENT_C_SOURCES	${CLIENT_ENGINE_TARGET_C_SOURCES})
set(CODUOMP_CLIENT_CXX_SOURCES	${CLIENT_ENGINE_TARGET_CXX_SOURCES})
coduomp_manifest_reset()

# UI module membership: ALL_C_SOURCES = UI_C_SOURCES + SHARED_C_SOURCES.
coduomp_manifest_read("build-mk/ui.mk")
set(CODUOMP_UI_SOURCES ${ALL_C_SOURCES})
coduomp_manifest_reset()

# Game module membership: the subsystem lists are the target's own selection,
# plus the libm and BG ownership lists from the shared manifests.
coduomp_manifest_read("build-mk/game.mk")
set(CODUOMP_GAME_SOURCES
	${SRCS}
	${LIBM_SRCS}
	${CRT_COMPAT_SRCS}
	${QCOMMON_SRCS}
	${MATH_SRCS}
	${BG_SRCS})
coduomp_manifest_reset()

# Dedicated server membership: mirror the native composition in build-mk/server.mk.
coduomp_manifest_read("build-mk/sources/server-engine.mk")
set(CODUOMP_SERVER_SOURCES
	${STANDALONE_SERVER_C_SOURCES}
	${STANDALONE_SERVER_CXX_SOURCES}
	${LAYOUT_POSIX_GENERATED_C_SOURCES}
	${LAYOUT_QCOMMON_C_SOURCES}
	${LAYOUT_CRT_COMPAT_C_SOURCES}
	${LAYOUT_MATH_C_SOURCES}
	${LAYOUT_SOUND_ALIAS_C_SOURCES}
	${LAYOUT_ANIMATION_C_SOURCES}
	${LAYOUT_COLLISION_C_SOURCES}
	${LAYOUT_FILESYSTEM_C_SOURCES}
	${LAYOUT_SERVER_ENGINE_C_SOURCES}
	${LAYOUT_SCRIPT_C_SOURCES}
	${LAYOUT_SCRIPT_CXX_SOURCES})
if(CODUOMP_ENABLE_PUNKBUSTER)
	list(REMOVE_ITEM CODUOMP_SERVER_SOURCES
		"src/server/standalone/punkbuster/pb_disabled.c")
else()
	list(REMOVE_ITEM CODUOMP_SERVER_SOURCES
		"src/server/standalone/punkbuster/pb_server.c")
endif()
# The standalone engine excludes the module-local helper owned by the client
# game module (see build-mk/server.mk).
list(REMOVE_ITEM CODUOMP_SERVER_SOURCES
	"src/math/local_matrix_transform_vector43.c")
coduomp_manifest_reset()
