# -- Apache Portable Runtime (APR) (Windows) --

if(WITH_APR)
  set(APR_OUTPUT_DIRECTORY "${WITH_APR}")
elseif(WITH_HTTPD)
  set(APR_OUTPUT_DIRECTORY "${WITH_HTTPD}")
else()
  set(APR_DIRECTORY "${DEPENDENCIES_DIRECTORY}/apr")
  set(APR_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/apr-dist")

  # A POSIX-generated apr.h in the source tree shadows this build's.
  file(REMOVE "${APR_DIRECTORY}/include/apr.h")

  if(NOT EXISTS "${APR_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${APR_DIRECTORY}")
    file(MAKE_DIRECTORY "${APR_OUTPUT_DIRECTORY}/logs")

    message(STATUS "[apr] Configuring -> ${APR_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -S "${APR_DIRECTORY}" -B "${CMAKE_BINARY_DIR}/deps/apr" -G "${CMAKE_GENERATOR}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DCMAKE_INSTALL_PREFIX=${APR_OUTPUT_DIRECTORY}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
        -DAPR_BUILD_SHARED=ON
        -DAPR_HAVE_IPV6=ON
        -DAPR_INSTALL_PRIVATE_H=ON
        -DAPR_BUILD_TESTAPR=OFF
      RESULT_VARIABLE _APR_CONFIGURE_RESULT
      OUTPUT_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-configure.log"
      ERROR_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-configure.log")
    if(NOT _APR_CONFIGURE_RESULT EQUAL 0)
      file(READ "${APR_OUTPUT_DIRECTORY}/logs/apr-configure.log" _APR_LOG)
      message(FATAL_ERROR "[apr] error: configure failed\n${_APR_LOG}")
    endif()

    message(STATUS "[apr] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/deps/apr" --parallel ${DEPENDENCIES_PARALLEL} --config ${CMAKE_BUILD_TYPE}
      RESULT_VARIABLE _APR_BUILD_RESULT
      OUTPUT_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-build.log"
      ERROR_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-build.log")
    if(NOT _APR_BUILD_RESULT EQUAL 0)
      file(READ "${APR_OUTPUT_DIRECTORY}/logs/apr-build.log" _APR_LOG)
      message(FATAL_ERROR "[apr] error: build failed\n${_APR_LOG}")
    endif()

    message(STATUS "[apr] Installing to ${APR_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}/deps/apr" --config ${CMAKE_BUILD_TYPE}
      RESULT_VARIABLE _APR_INSTALL_RESULT
      OUTPUT_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-install.log"
      ERROR_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-install.log")
    if(NOT _APR_INSTALL_RESULT EQUAL 0)
      file(READ "${APR_OUTPUT_DIRECTORY}/logs/apr-install.log" _APR_LOG)
      message(FATAL_ERROR "[apr] error: install failed\n${_APR_LOG}")
    endif()

    string(TIMESTAMP _APR_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${APR_OUTPUT_DIRECTORY}/.done" "${_APR_DONE_TIME}")
  endif()
endif()

if(NOT EXISTS "${APR_OUTPUT_DIRECTORY}/include/apr.h")
  message(FATAL_ERROR "[apr] error: ${APR_OUTPUT_DIRECTORY}/include/apr.h not found.")
endif()

# apr-1-config is a shell script the Windows build does not produce.
file(READ "${APR_OUTPUT_DIRECTORY}/include/apr_version.h" _apr_version_h)
string(REGEX MATCH "#define APR_MAJOR_VERSION[ \t]+([0-9]+)" _ "${_apr_version_h}")
set(_apr_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define APR_MINOR_VERSION[ \t]+([0-9]+)" _ "${_apr_version_h}")
set(_apr_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define APR_PATCH_VERSION[ \t]+([0-9]+)" _ "${_apr_version_h}")
set(APR_VERSION "${_apr_major}.${_apr_minor}.${CMAKE_MATCH_1}")

if(NOT APR_VERSION OR APR_VERSION VERSION_LESS APR_VERSION_MIN)
  message(FATAL_ERROR "[apr] error: need >= ${APR_VERSION_MIN}, found '${APR_VERSION}' in ${APR_OUTPUT_DIRECTORY}")
endif()

# The import library, not the archive: httpd links the same DLL and APR has global state.
find_library(APR_LIBRARY NAMES libapr-1
  PATHS "${APR_OUTPUT_DIRECTORY}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)

message(STATUS "[apr] found (${APR_VERSION}): ${APR_OUTPUT_DIRECTORY}")

add_library(apr INTERFACE)
target_include_directories(apr SYSTEM INTERFACE "${APR_OUTPUT_DIRECTORY}/include")
target_link_libraries(apr INTERFACE "${APR_LIBRARY}" ws2_32 mswsock)
