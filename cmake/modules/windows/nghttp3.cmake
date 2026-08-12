# -- nghttp3 (Windows) --

if(WITH_NGHTTP3)
  set(NGHTTP3_OUTPUT_DIRECTORY "${WITH_NGHTTP3}")
else()
  set(NGHTTP3_DIRECTORY "${DEPENDENCIES_DIRECTORY}/nghttp3")
  set(NGHTTP3_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/nghttp3-dist")

  if(NOT EXISTS "${NGHTTP3_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${NGHTTP3_DIRECTORY}")
    require_initialized_submodule("${NGHTTP3_DIRECTORY}/lib/sfparse")
    file(MAKE_DIRECTORY "${NGHTTP3_OUTPUT_DIRECTORY}/logs")

    message(STATUS "[nghttp3] Configuring -> ${NGHTTP3_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -S "${NGHTTP3_DIRECTORY}" -B "${CMAKE_BINARY_DIR}/deps/nghttp3" -G "${CMAKE_GENERATOR}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -DVCPKG_APPLOCAL_DEPS=OFF
        "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DCMAKE_INSTALL_PREFIX=${NGHTTP3_OUTPUT_DIRECTORY}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
        -DENABLE_LIB_ONLY=ON
        -DENABLE_SHARED_LIB=ON
        -DENABLE_STATIC_LIB=ON
      RESULT_VARIABLE _NGHTTP3_CONFIGURE_RESULT
      OUTPUT_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-configure.log"
      ERROR_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-configure.log")
    if(NOT _NGHTTP3_CONFIGURE_RESULT EQUAL 0)
      file(READ "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-configure.log" _NGHTTP3_LOG)
      message(FATAL_ERROR "[nghttp3] error: configure failed\n${_NGHTTP3_LOG}")
    endif()

    message(STATUS "[nghttp3] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/deps/nghttp3" --parallel ${DEPENDENCIES_PARALLEL} --config ${CMAKE_BUILD_TYPE}
      RESULT_VARIABLE _NGHTTP3_BUILD_RESULT
      OUTPUT_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-build.log"
      ERROR_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-build.log")
    if(NOT _NGHTTP3_BUILD_RESULT EQUAL 0)
      file(READ "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-build.log" _NGHTTP3_LOG)
      message(FATAL_ERROR "[nghttp3] error: build failed\n${_NGHTTP3_LOG}")
    endif()

    message(STATUS "[nghttp3] Installing to ${NGHTTP3_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}/deps/nghttp3" --config ${CMAKE_BUILD_TYPE}
      RESULT_VARIABLE _NGHTTP3_INSTALL_RESULT
      OUTPUT_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-install.log"
      ERROR_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-install.log")
    if(NOT _NGHTTP3_INSTALL_RESULT EQUAL 0)
      file(READ "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-install.log" _NGHTTP3_LOG)
      message(FATAL_ERROR "[nghttp3] error: install failed\n${_NGHTTP3_LOG}")
    endif()

    string(TIMESTAMP _NGHTTP3_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${NGHTTP3_OUTPUT_DIRECTORY}/.done" "${_NGHTTP3_DONE_TIME}")
  endif()
endif()

file(READ "${NGHTTP3_OUTPUT_DIRECTORY}/include/nghttp3/version.h" _NGHTTP3_VERSION_H_CONTENT)
string(REGEX MATCH "#define NGHTTP3_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${_NGHTTP3_VERSION_H_CONTENT}")
set(NGHTTP3_VERSION "${CMAKE_MATCH_1}")

if(NOT NGHTTP3_VERSION OR NGHTTP3_VERSION VERSION_LESS NGHTTP3_VERSION_MIN)
  message(FATAL_ERROR
    "[nghttp3] error: need >= ${NGHTTP3_VERSION_MIN}, found '${NGHTTP3_VERSION}' in ${NGHTTP3_OUTPUT_DIRECTORY}")
endif()

# The import library first: nghttp3.h declares dllimport unless NGHTTP3_STATICLIB is set.
find_library(NGHTTP3_LIBRARY NAMES nghttp3.dll nghttp3
  PATHS "${NGHTTP3_OUTPUT_DIRECTORY}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)

message(STATUS "[nghttp3] found (${NGHTTP3_VERSION}): ${NGHTTP3_OUTPUT_DIRECTORY}")

add_library(nghttp3 INTERFACE)
target_include_directories(nghttp3 SYSTEM INTERFACE "${NGHTTP3_OUTPUT_DIRECTORY}/include")
target_link_libraries(nghttp3 INTERFACE "${NGHTTP3_LIBRARY}")

# Extras

find_library(NGHTTP3_STATIC_LIBRARY NAMES libnghttp3.a
  PATHS "${NGHTTP3_OUTPUT_DIRECTORY}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH NO_CACHE)

if(NGHTTP3_STATIC_LIBRARY)
  add_library(nghttp3_static INTERFACE)
  target_include_directories(nghttp3_static SYSTEM INTERFACE "${NGHTTP3_OUTPUT_DIRECTORY}/include")
  target_link_libraries(nghttp3_static INTERFACE "${NGHTTP3_STATIC_LIBRARY}")
endif()
