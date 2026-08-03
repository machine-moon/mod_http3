# -- ngtcp2 QUIC engine --

if(NOT ENABLE_NGTCP2)
  return()
endif()

include(ngtcp2)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.c)
target_sources(${PROJECT_NAME}-quic PRIVATE ${sources})
target_link_libraries(${PROJECT_NAME}-quic PRIVATE ngtcp2)
target_compile_definitions(${PROJECT_NAME}-quic PUBLIC H3_ENABLE_NGTCP2)
target_include_directories(${PROJECT_NAME}-quic
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
