# -- Null QUIC engine --

file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.c)
target_sources(${PROJECT_NAME}-quic PRIVATE ${sources})
target_include_directories(${PROJECT_NAME}-quic PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
