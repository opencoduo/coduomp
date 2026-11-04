#-----------------------------------------------------------------
# Build identity
#
# The startup banner and the client footer report the source revision. The
# Makefile targets run `git describe --always --dirty` when make parses the
# makefile; this module does the same when CMake configures, and configure is
# re-run whenever git moves HEAD or rewrites the index, so a CMake build never
# keeps reporting an older revision.
#
# Nothing has to be passed on the command line: the revision is resolved from
# the source tree itself. CMake only falls back to "unknown" for an exported
# source archive, where there is nothing to report.
#
# The variable is named CODUOMP_BUILD_GIT_REVISION, not CODUOMP_GIT_REVISION,
# because CoduompSources.cmake evaluates the build-mk manifests into this
# scope and coduomp_manifest_reset() unsets every variable they create
# (build-mk defines CODUOMP_GIT_REVISION for the Makefile targets).
#-----------------------------------------------------------------

set(CODUOMP_BUILD_GIT_REVISION "unknown")

if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
    # ZERO_CHECK re-runs configure when one of these files changes, which
    # refreshes the definition below and recompiles the affected units.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/.git/HEAD"
        "${CMAKE_SOURCE_DIR}/.git/index")
endif()

# coduomp_git_revision_from_refs(<out-var>)
#
# Read the checked-out commit without running git. Windows installations
# frequently have Git installed without git.exe on PATH, and a source tree
# still records its revision in .git even then. Packed refs and a detached
# HEAD are handled; the "-dirty" marker needs the git executable and is
# therefore absent on this path.
function(coduomp_git_revision_from_refs out_var)
    set(_git_dir "${CMAKE_SOURCE_DIR}/.git")
    set(_revision "")

    if(EXISTS "${_git_dir}/HEAD")
        file(READ "${_git_dir}/HEAD" _head)
        string(STRIP "${_head}" _head)

        if(_head MATCHES "^ref: *(.+)$")
            set(_ref "${CMAKE_MATCH_1}")
            if(EXISTS "${_git_dir}/${_ref}")
                file(READ "${_git_dir}/${_ref}" _revision)
                string(STRIP "${_revision}" _revision)
            elseif(EXISTS "${_git_dir}/packed-refs")
                file(READ "${_git_dir}/packed-refs" _packed)
                if(_packed MATCHES "(^|\n)([0-9a-f]+) ${_ref}(\n|$)")
                    set(_revision "${CMAKE_MATCH_2}")
                endif()
            endif()
        elseif(_head MATCHES "^[0-9a-f]+$")
            set(_revision "${_head}")
        endif()
    endif()

    if(_revision MATCHES "^[0-9a-f]+$")
        string(SUBSTRING "${_revision}" 0 7 _revision)
        set(${out_var} "${_revision}" PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

find_program(CODUOMP_GIT_EXECUTABLE NAMES git)
if(CODUOMP_GIT_EXECUTABLE)
    execute_process(
        COMMAND "${CODUOMP_GIT_EXECUTABLE}" describe --always --dirty
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _coduomp_git_status
        OUTPUT_VARIABLE _coduomp_git_describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_coduomp_git_status EQUAL 0 AND _coduomp_git_describe)
        set(CODUOMP_BUILD_GIT_REVISION "${_coduomp_git_describe}")
    endif()
endif()

if(CODUOMP_BUILD_GIT_REVISION STREQUAL "unknown")
    coduomp_git_revision_from_refs(_coduomp_refs_revision)
    if(_coduomp_refs_revision)
        set(CODUOMP_BUILD_GIT_REVISION "${_coduomp_refs_revision}")
    endif()
endif()

unset(_coduomp_git_status)
unset(_coduomp_git_describe)
unset(_coduomp_refs_revision)

message(STATUS "Build revision: ${CODUOMP_BUILD_GIT_REVISION}")

# coduomp_apply_build_identity(<target>)
#
# Define CODUOMP_GIT_REVISION for a target that displays the identity.
function(coduomp_apply_build_identity target)
    target_compile_definitions(${target} PRIVATE
        CODUOMP_GIT_REVISION="${CODUOMP_BUILD_GIT_REVISION}")
endfunction()
