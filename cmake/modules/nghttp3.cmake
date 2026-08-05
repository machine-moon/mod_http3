# -- nghttp3 v1.18.0 --

if(TARGET nghttp3)
  return()
endif()

set(NGHTTP3_VERSION_MIN "1.18.0")

if(WITH_NGHTTP3)
  find_library(NGHTTP3_LIBRARY NAMES nghttp3
    PATHS "${WITH_NGHTTP3}/lib" "${WITH_NGHTTP3}/lib64" NO_DEFAULT_PATH)
  if(NOT NGHTTP3_LIBRARY)
    message(FATAL_ERROR
        "[nghttp3] error: nghttp3 not found at WITH_NGHTTP3=${WITH_NGHTTP3}."
    )
  endif()
  set(NGHTTP3_OUTPUT_DIRECTORY "${WITH_NGHTTP3}")
else()

  set(NGHTTP3_DIRECTORY "${DEPENDENCIES_DIRECTORY}/nghttp3")
  set(NGHTTP3_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/nghttp3-dist")

  # Build nghttp3 from source if not already done
  if(NOT EXISTS "${NGHTTP3_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${NGHTTP3_DIRECTORY}")
    require_initialized_submodule("${NGHTTP3_DIRECTORY}/lib/sfparse")
    file(MAKE_DIRECTORY "${NGHTTP3_OUTPUT_DIRECTORY}/logs")

    if(EXISTS "${NGHTTP3_DIRECTORY}/Makefile")
      file(REMOVE "${NGHTTP3_DIRECTORY}/Makefile")
    endif()

    message(STATUS "[nghttp3] Configuring -> ${NGHTTP3_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND autoreconf -i
      WORKING_DIRECTORY "${NGHTTP3_DIRECTORY}"
      RESULT_VARIABLE _NGHTTP3_AUTORECONF_RESULT
      OUTPUT_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-autoreconf.log"
      ERROR_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-autoreconf.log")
    if(NOT _NGHTTP3_AUTORECONF_RESULT EQUAL 0)
      message(FATAL_ERROR "[nghttp3] error: autoreconf failed -- see ${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-autoreconf.log")
    endif()

    execute_process(
      COMMAND ./configure --prefix=${NGHTTP3_OUTPUT_DIRECTORY} --enable-lib-only #--enable-debug
      WORKING_DIRECTORY "${NGHTTP3_DIRECTORY}"
      RESULT_VARIABLE _NGHTTP3_CONFIGURE_RESULT
      OUTPUT_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-configure.log"
      ERROR_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-configure.log")
    if(NOT _NGHTTP3_CONFIGURE_RESULT EQUAL 0)
      message(FATAL_ERROR "[nghttp3] error: configure failed -- see ${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-configure.log")
    endif()

    message(STATUS "[nghttp3] Cleaning workspace")
    execute_process(
      COMMAND make clean
      WORKING_DIRECTORY "${NGHTTP3_DIRECTORY}"
      RESULT_VARIABLE _NGHTTP3_CLEAN_RESULT
      OUTPUT_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-clean.log"
      ERROR_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-clean.log")
    if(NOT _NGHTTP3_CLEAN_RESULT EQUAL 0)
      message(FATAL_ERROR "[nghttp3] error: make clean failed -- see ${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-clean.log")
    endif()

    message(STATUS "[nghttp3] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND make -j${DEPENDENCIES_PARALLEL}
      WORKING_DIRECTORY "${NGHTTP3_DIRECTORY}"
      RESULT_VARIABLE _NGHTTP3_BUILD_RESULT
      OUTPUT_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-build.log"
      ERROR_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-build.log")
    if(NOT _NGHTTP3_BUILD_RESULT EQUAL 0)
      message(FATAL_ERROR "[nghttp3] error: build failed -- see ${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-build.log")
    endif()

    message(STATUS "[nghttp3] Installing to ${NGHTTP3_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND make install
      WORKING_DIRECTORY "${NGHTTP3_DIRECTORY}"
      RESULT_VARIABLE _NGHTTP3_INSTALL_RESULT
      OUTPUT_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-install.log"
      ERROR_FILE "${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-install.log")
    if(NOT _NGHTTP3_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "[nghttp3] error: install failed -- see ${NGHTTP3_OUTPUT_DIRECTORY}/logs/nghttp3-install.log")
    endif()

    string(TIMESTAMP _NGHTTP3_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${NGHTTP3_OUTPUT_DIRECTORY}/.done" "${_NGHTTP3_DONE_TIME}")
  endif()

  # Find the nghttp3 we just built
  find_library(NGHTTP3_LIBRARY NAMES nghttp3 PATHS "${NGHTTP3_OUTPUT_DIRECTORY}/lib" NO_DEFAULT_PATH)
endif()

# Verify version is >= NGHTTP3_VERSION_MIN
file(READ "${NGHTTP3_OUTPUT_DIRECTORY}/include/nghttp3/version.h" _NGHTTP3_VERSION_H_CONTENT)
string(REGEX MATCH "#define NGHTTP3_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${_NGHTTP3_VERSION_H_CONTENT}")
set(NGHTTP3_VERSION "${CMAKE_MATCH_1}")

if(NOT NGHTTP3_VERSION OR NGHTTP3_VERSION VERSION_LESS NGHTTP3_VERSION_MIN)
  message(FATAL_ERROR
    "[nghttp3] error: could not determine a valid version\n"
    "  NGHTTP3_INCLUDE_DIR = ${NGHTTP3_INCLUDE_DIR}\n"
    "  NGHTTP3_VERSION     = ${NGHTTP3_VERSION}\n"
    "  NGHTTP3_VERSION_MIN = ${NGHTTP3_VERSION_MIN}")
endif()

message(STATUS "[nghttp3] found (${NGHTTP3_VERSION}): ${NGHTTP3_OUTPUT_DIRECTORY}")

add_library(nghttp3 INTERFACE)
target_include_directories(nghttp3 SYSTEM INTERFACE "${NGHTTP3_OUTPUT_DIRECTORY}/include")
target_link_libraries(nghttp3 INTERFACE "${NGHTTP3_LIBRARY}")

# Extras

find_library(NGHTTP3_STATIC_LIBRARY NAMES libnghttp3.a
  PATHS "${NGHTTP3_OUTPUT_DIRECTORY}/lib" NO_DEFAULT_PATH NO_CACHE)

if(NGHTTP3_STATIC_LIBRARY)
  add_library(nghttp3_static INTERFACE)
  target_include_directories(nghttp3_static SYSTEM INTERFACE "${NGHTTP3_OUTPUT_DIRECTORY}/include")
  target_link_libraries(nghttp3_static INTERFACE "${NGHTTP3_STATIC_LIBRARY}")
endif()
