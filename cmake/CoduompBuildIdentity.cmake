# NOT_FROM_ORIGINAL_SOURCE: build metadata shared by all supported generators.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CODUOMP_IDENTITY_DIR "${CMAKE_BINARY_DIR}/generated/build-identity")
add_custom_target(coduomp_build_identity
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build_identity.py"
        --source "${CMAKE_SOURCE_DIR}"
        --output "${CODUOMP_IDENTITY_DIR}/coduomp_build_identity.h"
    BYPRODUCTS "${CODUOMP_IDENTITY_DIR}/coduomp_build_identity.h"
    VERBATIM)

# NOT_FROM_ORIGINAL_SOURCE: order generation before compiler dependency scans.
function(coduomp_apply_build_identity target)
    add_dependencies(${target} coduomp_build_identity)
    target_include_directories(${target} PRIVATE "${CODUOMP_IDENTITY_DIR}")
endfunction()
