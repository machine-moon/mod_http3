# -- The engine-agnostic contract --

include(openssl)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.c)
target_sources(${PROJECT_NAME}-quic PRIVATE ${sources})
target_link_libraries(${PROJECT_NAME}-quic PRIVATE openssl)
target_include_directories(${PROJECT_NAME}-quic
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
