# -- QUIC transport --
#
# quic/quic/ is the contract every caller sees; the directories beside it are
# the engines implementing it. Each appends its own sources, include directory
# and transport library to the one target below.

add_library(${PROJECT_NAME}-quic STATIC)
set_target_properties(${PROJECT_NAME}-quic PROPERTIES C_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
apply_target_flags(${PROJECT_NAME}-quic)

set(QUIC_DEPENDENCIES_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dependencies"
    CACHE PATH "Directory holding optional QUIC library submodules")
set(QUIC_DEPENDENCIES_OUTPUT_DIRECTORY "${QUIC_DEPENDENCIES_DIRECTORY}"
    CACHE PATH "Directory the optional QUIC libraries are built into")

add_subdirectory(quic)
add_subdirectory(null)
add_subdirectory(ossl)
add_subdirectory(ngtcp2)
