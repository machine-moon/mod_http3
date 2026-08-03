# -- QUIC transport engines --

include(openssl)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.c)

add_library(${PROJECT_NAME}-quic STATIC ${sources})
target_link_libraries(${PROJECT_NAME}-quic PRIVATE openssl)
target_include_directories(${PROJECT_NAME}-quic
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(${PROJECT_NAME}-quic PROPERTIES C_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
apply_target_flags(${PROJECT_NAME}-quic)

set(QUIC_DEPENDENCIES_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dependencies"
    CACHE PATH "Directory holding optional QUIC library submodules")
set(QUIC_DEPENDENCIES_OUTPUT_DIRECTORY "${QUIC_DEPENDENCIES_DIRECTORY}"
    CACHE PATH "Directory the optional QUIC libraries are built into")

add_subdirectory(ossl)
add_subdirectory(ngtcp2)
