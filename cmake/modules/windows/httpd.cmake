# -- Apache httpd (Windows) --

include(apr)
include(apu)

if(WITH_HTTPD)
  set(HTTPD_OUTPUT_DIRECTORY "${WITH_HTTPD}")
else()
  if(NOT TARGET openssl)
    message(FATAL_ERROR "[httpd] error: building httpd from source requires openssl to be built first")
  endif()

  set(HTTPD_DIRECTORY "${DEPENDENCIES_DIRECTORY}/httpd")
  set(HTTPD_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/httpd-dist")

  # find_package() reroots under CMAKE_FIND_ROOT_PATH, so name our prefixes there.
  set(_httpd_find_roots "${OPENSSL_OUTPUT_DIRECTORY}" "${APR_OUTPUT_DIRECTORY}")
  list(APPEND _httpd_find_roots ${CMAKE_FIND_ROOT_PATH})
  list(REMOVE_DUPLICATES _httpd_find_roots)

  # A ";" in an execute_process argument splits argv, so pass lists by -C script.
  set(_httpd_initial_cache "${CMAKE_BINARY_DIR}/deps/httpd-initial-cache.cmake")
  file(WRITE "${_httpd_initial_cache}"
    "set(CMAKE_FIND_ROOT_PATH \"${_httpd_find_roots}\" CACHE STRING \"\" FORCE)\n"
    "set(APR_LIBRARIES \"${APR_LIBRARY};${APU_LIBRARY}\" CACHE STRING \"\" FORCE)\n")

  # Same shadowing as APR: a POSIX configure leaves copies in include/.
  file(REMOVE "${HTTPD_DIRECTORY}/include/ap_config_auto.h"
              "${HTTPD_DIRECTORY}/include/ap_config_layout.h")

  if(NOT EXISTS "${HTTPD_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${HTTPD_DIRECTORY}")
    file(MAKE_DIRECTORY "${HTTPD_OUTPUT_DIRECTORY}/logs")

    message(STATUS "[httpd] Configuring -> ${HTTPD_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -S "${HTTPD_DIRECTORY}" -B "${CMAKE_BINARY_DIR}/deps/httpd" -G "${CMAKE_GENERATOR}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DCMAKE_INSTALL_PREFIX=${HTTPD_OUTPUT_DIRECTORY}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
        -C "${_httpd_initial_cache}"
        -DAPR_INCLUDE_DIR=${APR_OUTPUT_DIRECTORY}/include
        -DOPENSSL_ROOT_DIR=${OPENSSL_OUTPUT_DIRECTORY}
        -DINSTALL_MANUAL=OFF
      RESULT_VARIABLE _HTTPD_CONFIGURE_RESULT
      OUTPUT_FILE "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-configure.log"
      ERROR_FILE "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-configure.log")
    if(NOT _HTTPD_CONFIGURE_RESULT EQUAL 0)
      file(READ "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-configure.log" _HTTPD_LOG)
      message(FATAL_ERROR "[httpd] error: configure failed\n${_HTTPD_LOG}")
    endif()

    message(STATUS "[httpd] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/deps/httpd" --parallel ${DEPENDENCIES_PARALLEL} --config ${CMAKE_BUILD_TYPE}
      RESULT_VARIABLE _HTTPD_BUILD_RESULT
      OUTPUT_FILE "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-build.log"
      ERROR_FILE "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-build.log")
    if(NOT _HTTPD_BUILD_RESULT EQUAL 0)
      file(READ "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-build.log" _HTTPD_LOG)
      message(FATAL_ERROR "[httpd] error: build failed\n${_HTTPD_LOG}")
    endif()

    message(STATUS "[httpd] Installing to ${HTTPD_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}/deps/httpd" --config ${CMAKE_BUILD_TYPE}
      RESULT_VARIABLE _HTTPD_INSTALL_RESULT
      OUTPUT_FILE "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-install.log"
      ERROR_FILE "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-install.log")
    if(NOT _HTTPD_INSTALL_RESULT EQUAL 0)
      file(READ "${HTTPD_OUTPUT_DIRECTORY}/logs/httpd-install.log" _HTTPD_LOG)
      message(FATAL_ERROR "[httpd] error: install failed\n${_HTTPD_LOG}")
    endif()

    string(TIMESTAMP _HTTPD_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${HTTPD_OUTPUT_DIRECTORY}/.done" "${_HTTPD_DONE_TIME}")
  endif()
endif()

if(NOT EXISTS "${HTTPD_OUTPUT_DIRECTORY}/include/httpd.h")
  message(FATAL_ERROR "[httpd] error: ${HTTPD_OUTPUT_DIRECTORY}/include/httpd.h not found.")
endif()

set(HTTPD_INCLUDE_DIR "${HTTPD_OUTPUT_DIRECTORY}/include")

file(READ "${HTTPD_INCLUDE_DIR}/ap_release.h" _ap_release_h)
string(REGEX MATCH "#define AP_SERVER_MAJORVERSION_NUMBER[ \t]+([0-9]+)" _ "${_ap_release_h}")
set(_httpd_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define AP_SERVER_MINORVERSION_NUMBER[ \t]+([0-9]+)" _ "${_ap_release_h}")
set(_httpd_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define AP_SERVER_PATCHLEVEL_NUMBER[ \t]+([0-9]+)" _ "${_ap_release_h}")
set(HTTPD_VERSION "${_httpd_major}.${_httpd_minor}.${CMAKE_MATCH_1}")

file(READ "${HTTPD_INCLUDE_DIR}/ap_mmn.h" _ap_mmn_h)
string(REGEX MATCH "#define MODULE_MAGIC_NUMBER_MAJOR[ \t]+([0-9]+)" _ "${_ap_mmn_h}")
set(HTTPD_MMN "${CMAKE_MATCH_1}")

if(NOT HTTPD_MMN OR HTTPD_MMN LESS HTTPD_MMN_MIN)
  message(WARNING
    "[httpd] warning: MODULE_MAGIC_NUMBER_MAJOR=${HTTPD_MMN} is less than the required minimum ${HTTPD_MMN_MIN}\n"
    "  Some features may be unavailable.")
endif()

# A DLL resolves every symbol at link time, so ap_* cannot be left to the loader.
find_library(HTTPD_LIBRARY NAMES libhttpd
  PATHS "${HTTPD_OUTPUT_DIRECTORY}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED NO_CACHE)

message(STATUS "[httpd] found (${HTTPD_VERSION}): ${HTTPD_OUTPUT_DIRECTORY}")

add_library(httpd INTERFACE)
target_link_libraries(httpd INTERFACE "${HTTPD_LIBRARY}" apr apu)
target_include_directories(httpd SYSTEM INTERFACE "${HTTPD_INCLUDE_DIR}")
