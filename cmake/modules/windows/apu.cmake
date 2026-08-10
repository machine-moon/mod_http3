# -- Apache Portable Runtime Utils (APU) (Windows) --

include(apr)

if(WITH_APU)
  set(APU_OUTPUT_DIRECTORY "${WITH_APU}")
elseif(WITH_APR OR WITH_HTTPD)
  set(APU_OUTPUT_DIRECTORY "${APR_OUTPUT_DIRECTORY}")
else()
  set(APU_DIRECTORY "${DEPENDENCIES_DIRECTORY}/apr-util")
  set(APU_OUTPUT_DIRECTORY "${APR_OUTPUT_DIRECTORY}")

  file(REMOVE "${APU_DIRECTORY}/include/apu.h" "${APU_DIRECTORY}/include/apr_ldap.h")

  if(NOT EXISTS "${APU_OUTPUT_DIRECTORY}/.apu-done")
    require_initialized_submodule("${APU_DIRECTORY}")
    file(MAKE_DIRECTORY "${APU_OUTPUT_DIRECTORY}/logs")

    message(STATUS "[apr-util] Configuring -> ${APU_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -S "${APU_DIRECTORY}" -B "${CMAKE_BINARY_DIR}/deps/apr-util" -G "${CMAKE_GENERATOR}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DCMAKE_INSTALL_PREFIX=${APU_OUTPUT_DIRECTORY}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
        -DAPR_INCLUDE_DIR=${APR_OUTPUT_DIRECTORY}/include
        -DAPR_LIBRARIES=${APR_LIBRARY}
        -DAPU_HAVE_ODBC=OFF
        -DAPU_HAVE_CRYPTO=OFF
      RESULT_VARIABLE _APU_CONFIGURE_RESULT
      OUTPUT_FILE "${APU_OUTPUT_DIRECTORY}/logs/apr-util-configure.log"
      ERROR_FILE "${APU_OUTPUT_DIRECTORY}/logs/apr-util-configure.log")
    if(NOT _APU_CONFIGURE_RESULT EQUAL 0)
      file(READ "${APU_OUTPUT_DIRECTORY}/logs/apr-util-configure.log" _APR_UTIL_LOG)
      message(FATAL_ERROR "[apr-util] error: configure failed\n${_APR_UTIL_LOG}")
    endif()

    message(STATUS "[apr-util] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/deps/apr-util" --parallel ${DEPENDENCIES_PARALLEL} --config ${CMAKE_BUILD_TYPE}
      RESULT_VARIABLE _APU_BUILD_RESULT
      OUTPUT_FILE "${APU_OUTPUT_DIRECTORY}/logs/apr-util-build.log"
      ERROR_FILE "${APU_OUTPUT_DIRECTORY}/logs/apr-util-build.log")
    if(NOT _APU_BUILD_RESULT EQUAL 0)
      file(READ "${APU_OUTPUT_DIRECTORY}/logs/apr-util-build.log" _APR_UTIL_LOG)
      message(FATAL_ERROR "[apr-util] error: build failed\n${_APR_UTIL_LOG}")
    endif()

    message(STATUS "[apr-util] Installing to ${APU_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}/deps/apr-util" --config ${CMAKE_BUILD_TYPE}
      RESULT_VARIABLE _APU_INSTALL_RESULT
      OUTPUT_FILE "${APU_OUTPUT_DIRECTORY}/logs/apr-util-install.log"
      ERROR_FILE "${APU_OUTPUT_DIRECTORY}/logs/apr-util-install.log")
    if(NOT _APU_INSTALL_RESULT EQUAL 0)
      file(READ "${APU_OUTPUT_DIRECTORY}/logs/apr-util-install.log" _APR_UTIL_LOG)
      message(FATAL_ERROR "[apr-util] error: install failed\n${_APR_UTIL_LOG}")
    endif()

    string(TIMESTAMP _APU_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${APU_OUTPUT_DIRECTORY}/.apu-done" "${_APU_DONE_TIME}")
  endif()
endif()

if(NOT EXISTS "${APU_OUTPUT_DIRECTORY}/include/apu.h")
  message(FATAL_ERROR "[apu] error: ${APU_OUTPUT_DIRECTORY}/include/apu.h not found.")
endif()

file(READ "${APU_OUTPUT_DIRECTORY}/include/apu_version.h" _apu_version_h)
string(REGEX MATCH "#define APU_MAJOR_VERSION[ \t]+([0-9]+)" _ "${_apu_version_h}")
set(_apu_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define APU_MINOR_VERSION[ \t]+([0-9]+)" _ "${_apu_version_h}")
set(_apu_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define APU_PATCH_VERSION[ \t]+([0-9]+)" _ "${_apu_version_h}")
set(APU_VERSION "${_apu_major}.${_apu_minor}.${CMAKE_MATCH_1}")

if(NOT APU_VERSION OR APU_VERSION VERSION_LESS APU_VERSION_MIN)
  message(FATAL_ERROR "[apu] error: need >= ${APU_VERSION_MIN}, found '${APU_VERSION}' in ${APU_OUTPUT_DIRECTORY}")
endif()

find_library(APU_LIBRARY NAMES libaprutil-1
  PATHS "${APU_OUTPUT_DIRECTORY}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)

message(STATUS "[apu] found (${APU_VERSION}): ${APU_OUTPUT_DIRECTORY}")

add_library(apu INTERFACE)
target_include_directories(apu SYSTEM INTERFACE "${APU_OUTPUT_DIRECTORY}/include")
target_link_libraries(apu INTERFACE "${APU_LIBRARY}")
