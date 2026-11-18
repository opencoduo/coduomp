# NOT_FROM_ORIGINAL_SOURCE: build metadata shared by all supported generators.
set(CODUOMP_IDENTITY_DIR "${CMAKE_BINARY_DIR}/generated/build-identity")
add_custom_target(coduomp_build_identity
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        "-DOUTPUT_HEADER=${CODUOMP_IDENTITY_DIR}/coduomp_build_identity.h"
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateBuildIdentity.cmake"
    BYPRODUCTS "${CODUOMP_IDENTITY_DIR}/coduomp_build_identity.h"
    VERBATIM)

# NOT_FROM_ORIGINAL_SOURCE: order generation before compiler dependency scans.
function(coduomp_apply_build_identity target)
    add_dependencies(${target} coduomp_build_identity)
    target_include_directories(${target} PRIVATE "${CODUOMP_IDENTITY_DIR}")
endfunction()
