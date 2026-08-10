# -- OpenSSL (Windows) --

if(WITH_SSL)
  set(OPENSSL_OUTPUT_DIRECTORY "${WITH_SSL}")
else()
  set(OPENSSL_DIRECTORY "${DEPENDENCIES_DIRECTORY}/openssl")
  set(OPENSSL_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/openssl-dist")
  set(_openssl_build_dir "${CMAKE_BINARY_DIR}/deps/openssl")

  if(NOT EXISTS "${OPENSSL_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${OPENSSL_DIRECTORY}")
    file(MAKE_DIRECTORY "${OPENSSL_OUTPUT_DIRECTORY}/logs")
    file(MAKE_DIRECTORY "${_openssl_build_dir}")

    find_program(PERL_EXECUTABLE NAMES perl REQUIRED)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(_openssl_target "VC-WIN64A")
    else()
      set(_openssl_target "VC-WIN32")
    endif()

    message(STATUS "[openssl] Configuring (${_openssl_target}) -> ${OPENSSL_OUTPUT_DIRECTORY}")
    execute_process(
      COMMAND "${PERL_EXECUTABLE}" "${OPENSSL_DIRECTORY}/Configure"
        ${_openssl_target}
        --prefix=${OPENSSL_OUTPUT_DIRECTORY}
        --openssldir=${OPENSSL_OUTPUT_DIRECTORY}/ssl
        # httpd's FIND_PACKAGE(OpenSSL) only looks in lib/, not lib64/.
        --libdir=lib
        shared no-tests no-docs
      WORKING_DIRECTORY "${_openssl_build_dir}"
      RESULT_VARIABLE _OPENSSL_CONFIG_RESULT
      OUTPUT_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-configure.log"
      ERROR_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-configure.log")
    if(NOT _OPENSSL_CONFIG_RESULT EQUAL 0)
      file(READ "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-configure.log" _OPENSSL_LOG)
      message(FATAL_ERROR "[openssl] error: configure failed\n${_OPENSSL_LOG}")
    endif()

    message(STATUS "[openssl] Building")
    execute_process(
      COMMAND nmake
      WORKING_DIRECTORY "${_openssl_build_dir}"
      RESULT_VARIABLE _OPENSSL_BUILD_RESULT
      OUTPUT_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-build.log"
      ERROR_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-build.log")
    if(NOT _OPENSSL_BUILD_RESULT EQUAL 0)
      file(READ "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-build.log" _OPENSSL_LOG)
      message(FATAL_ERROR "[openssl] error: build failed\n${_OPENSSL_LOG}")
    endif()

    message(STATUS "[openssl] Installing to ${OPENSSL_OUTPUT_DIRECTORY}")
    execute_process(
      COMMAND nmake install_sw
      WORKING_DIRECTORY "${_openssl_build_dir}"
      RESULT_VARIABLE _OPENSSL_INSTALL_RESULT
      OUTPUT_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-install.log"
      ERROR_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-install.log")
    if(NOT _OPENSSL_INSTALL_RESULT EQUAL 0)
      file(READ "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-install.log" _OPENSSL_LOG)
      message(FATAL_ERROR "[openssl] error: install failed\n${_OPENSSL_LOG}")
    endif()

    # Copied, not symlinked: the tree has to survive being carried onto Windows.
    message(STATUS "[openssl] Copying private headers to install prefix")
    file(COPY "${OPENSSL_DIRECTORY}/include/internal" DESTINATION "${OPENSSL_OUTPUT_DIRECTORY}/include")
    file(COPY "${OPENSSL_DIRECTORY}/include/crypto" DESTINATION "${OPENSSL_OUTPUT_DIRECTORY}/include")
    file(COPY "${OPENSSL_OUTPUT_DIRECTORY}/include/internal" DESTINATION "${OPENSSL_OUTPUT_DIRECTORY}/include/openssl")
    file(COPY "${OPENSSL_OUTPUT_DIRECTORY}/include/crypto" DESTINATION "${OPENSSL_OUTPUT_DIRECTORY}/include/openssl")

    string(TIMESTAMP _OPENSSL_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${OPENSSL_OUTPUT_DIRECTORY}/.done" "${_OPENSSL_DONE_TIME}")
  endif()
endif()

set(OPENSSL_INCLUDE_DIR "${OPENSSL_OUTPUT_DIRECTORY}/include")
if(NOT EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
  message(FATAL_ERROR "[openssl] error: ${OPENSSL_INCLUDE_DIR}/openssl/ssl.h not found.")
endif()

file(READ "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h" _opensslv_h)
string(REGEX MATCH "#[ \t]*define OPENSSL_VERSION_STR[ \t]+\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _ "${_opensslv_h}")
set(OPENSSL_VERSION "${CMAKE_MATCH_1}")
if(NOT OPENSSL_VERSION OR OPENSSL_VERSION VERSION_LESS OPENSSL_VERSION_MIN)
  message(FATAL_ERROR
    "[openssl] error: need >= ${OPENSSL_VERSION_MIN}, found '${OPENSSL_VERSION}' in ${OPENSSL_OUTPUT_DIRECTORY}")
endif()

find_library(OPENSSL_SSL_LIBRARY NAMES ssl libssl
  PATHS "${OPENSSL_OUTPUT_DIRECTORY}/lib" "${OPENSSL_OUTPUT_DIRECTORY}/lib64" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED NO_CACHE)
find_library(OPENSSL_CRYPTO_LIBRARY NAMES crypto libcrypto
  PATHS "${OPENSSL_OUTPUT_DIRECTORY}/lib" "${OPENSSL_OUTPUT_DIRECTORY}/lib64" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED NO_CACHE)

message(STATUS "[openssl] found (${OPENSSL_VERSION}): ${OPENSSL_OUTPUT_DIRECTORY}")

add_library(openssl INTERFACE)
target_include_directories(openssl SYSTEM INTERFACE "${OPENSSL_INCLUDE_DIR}")
target_link_libraries(openssl INTERFACE
  "${OPENSSL_SSL_LIBRARY}" "${OPENSSL_CRYPTO_LIBRARY}" ws2_32 crypt32 bcrypt)

# Extras

get_filename_component(OPENSSL_LIBRARY_PATH "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
find_library(OPENSSL_SSL_STATIC NAMES libssl.a HINTS "${OPENSSL_LIBRARY_PATH}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH NO_CACHE)
find_library(OPENSSL_CRYPTO_STATIC NAMES libcrypto.a HINTS "${OPENSSL_LIBRARY_PATH}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH NO_CACHE)

if(OPENSSL_SSL_STATIC AND OPENSSL_CRYPTO_STATIC)
  add_library(openssl_static INTERFACE)
  target_include_directories(openssl_static SYSTEM INTERFACE "${OPENSSL_INCLUDE_DIR}")
  target_link_libraries(openssl_static INTERFACE
    "${OPENSSL_SSL_STATIC}" "${OPENSSL_CRYPTO_STATIC}" ws2_32 crypt32 bcrypt)
endif()

if(NOT IS_DIRECTORY "${OPENSSL_INCLUDE_DIR}/internal")
  set(MISSING_OPENSSL_INTERNAL TRUE)
endif()
if(NOT IS_DIRECTORY "${OPENSSL_INCLUDE_DIR}/crypto")
  set(MISSING_OPENSSL_CRYPTO TRUE)
endif()
