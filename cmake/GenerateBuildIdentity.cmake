# NOT_FROM_ORIGINAL_SOURCE: build tooling, independent of runtime behavior.
set(revision "unknown")
# Require metadata in this source root: an archive nested in another checkout
# must not accidentally report the enclosing repository's identity.
if(EXISTS "${SOURCE_ROOT}/.git")
    find_program(CODUOMP_GIT_EXECUTABLE git)
    if(CODUOMP_GIT_EXECUTABLE)
        # Use a bounded commit identifier so arbitrary tag names cannot expand UI text.
        execute_process(COMMAND "${CODUOMP_GIT_EXECUTABLE}" -C "${SOURCE_ROOT}"
            describe --always --dirty --match "" --abbrev=12
            RESULT_VARIABLE status OUTPUT_VARIABLE candidate
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(status EQUAL 0 AND NOT candidate STREQUAL "")
            set(revision "${candidate}")
        endif()
    endif()
    # Without Git, do not assert that a checkout is clean.
endif()
# Publish only changed generated content, preserving unchanged timestamps.
file(CONFIGURE OUTPUT "${OUTPUT_HEADER}" CONTENT
"/* Generated build metadata; do not edit. */
#ifndef CODUOMP_BUILD_IDENTITY_H
#define CODUOMP_BUILD_IDENTITY_H
#define CODUOMP_GIT_REVISION \"@revision@\"
#endif
" @ONLY NEWLINE_STYLE UNIX)
