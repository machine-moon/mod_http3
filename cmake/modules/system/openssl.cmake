# -- OpenSSL v3.5.0+ --

if(TARGET openssl)
  return()
endif()

set(OPENSSL_VERSION_MIN "3.5.0")

if(WITH_SSL)
  find_package(OpenSSL QUIET COMPONENTS Crypto SSL PATHS "${WITH_SSL}" NO_DEFAULT_PATH)
  if(NOT OpenSSL_FOUND)
    message(FATAL_ERROR
        "[openssl] error: OpenSSL not found at WITH_SSL=${WITH_SSL}."
    )
  endif()
  set(OPENSSL_OUTPUT_DIRECTORY "${WITH_SSL}")
else()

  set(OPENSSL_DIRECTORY "${DEPENDENCIES_DIRECTORY}/openssl")
  set(OPENSSL_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/openssl-dist")

  # Build OpenSSL from source if not already done
  if(NOT EXISTS "${OPENSSL_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${OPENSSL_DIRECTORY}")
    file(MAKE_DIRECTORY "${OPENSSL_OUTPUT_DIRECTORY}/logs")


    if(EXISTS "${OPENSSL_DIRECTORY}/Makefile")

      message(STATUS "[openssl] Cleaning previous build artifacts")

      execute_process(
        COMMAND make distclean
        WORKING_DIRECTORY "${OPENSSL_DIRECTORY}"
        RESULT_VARIABLE _OPENSSL_DISTCLEAN_RESULT
        OUTPUT_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-distclean.log"
        ERROR_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-distclean.log")
      if(NOT _OPENSSL_DISTCLEAN_RESULT EQUAL 0)
        message(FATAL_ERROR "[openssl] error: distclean failed -- see ${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-distclean.log")
      endif()
    endif()

    message(STATUS "[openssl] Configuring -> ${OPENSSL_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND ./config --prefix=${OPENSSL_OUTPUT_DIRECTORY} --openssldir=${OPENSSL_OUTPUT_DIRECTORY}/ssl shared
      WORKING_DIRECTORY "${OPENSSL_DIRECTORY}"
      RESULT_VARIABLE _OPENSSL_CONFIG_RESULT
      OUTPUT_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-configure.log"
      ERROR_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-configure.log")
    if(NOT _OPENSSL_CONFIG_RESULT EQUAL 0)
      message(FATAL_ERROR "[openssl] error: configure failed -- see ${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-configure.log")
    endif()

    message(STATUS "[openssl] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND make -j${DEPENDENCIES_PARALLEL}
      WORKING_DIRECTORY "${OPENSSL_DIRECTORY}"
      RESULT_VARIABLE _OPENSSL_BUILD_RESULT
      OUTPUT_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-build.log"
      ERROR_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-build.log")
    if(NOT _OPENSSL_BUILD_RESULT EQUAL 0)
      message(FATAL_ERROR "[openssl] error: build failed -- see ${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-build.log")
    endif()

    message(STATUS "[openssl] Installing to ${OPENSSL_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND make install_sw
      WORKING_DIRECTORY "${OPENSSL_DIRECTORY}"
      RESULT_VARIABLE _OPENSSL_INSTALL_RESULT
      OUTPUT_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-install.log"
      ERROR_FILE "${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-install.log")
    if(NOT _OPENSSL_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "[openssl] error: install failed -- see ${OPENSSL_OUTPUT_DIRECTORY}/logs/openssl-install.log")
    endif()

    # Copy private headers not installed by make install_sw; symlink the openssl/ mirror
    # so both #include "internal/foo.h" and #include "openssl/internal/foo.h" resolve.
    message(STATUS "[openssl] Copying private headers to install prefix")
    file(COPY "${OPENSSL_DIRECTORY}/include/internal"
         DESTINATION "${OPENSSL_OUTPUT_DIRECTORY}/include")
    file(COPY "${OPENSSL_DIRECTORY}/include/crypto"
         DESTINATION "${OPENSSL_OUTPUT_DIRECTORY}/include")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink
        "../internal" "${OPENSSL_OUTPUT_DIRECTORY}/include/openssl/internal"
      RESULT_VARIABLE _OSSL_SYMLINK_INTERNAL)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink
        "../crypto" "${OPENSSL_OUTPUT_DIRECTORY}/include/openssl/crypto"
      RESULT_VARIABLE _OSSL_SYMLINK_CRYPTO)
    if(NOT _OSSL_SYMLINK_INTERNAL EQUAL 0 OR NOT _OSSL_SYMLINK_CRYPTO EQUAL 0)
      message(FATAL_ERROR "[openssl] error: failed to symlink private headers under include/openssl/")
    endif()

    string(TIMESTAMP _OPENSSL_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${OPENSSL_OUTPUT_DIRECTORY}/.done" "${_OPENSSL_DONE_TIME}")
  endif()

  # Find the OpenSSL we just built

  set(OPENSSL_ROOT_DIR "${OPENSSL_OUTPUT_DIRECTORY}")
  set(OpenSSL_DIR "${OPENSSL_OUTPUT_DIRECTORY}/lib64/cmake/OpenSSL")
  find_package(OpenSSL REQUIRED QUIET COMPONENTS Crypto SSL PATHS "${OPENSSL_OUTPUT_DIRECTORY}" NO_DEFAULT_PATH)
endif()

# Verify version is >= 3.5.0
if(OPENSSL_VERSION VERSION_LESS OPENSSL_VERSION_MIN)
  message(FATAL_ERROR
      "[openssl] error: found version ${OPENSSL_VERSION} but require >= ${OPENSSL_VERSION_MIN}."
  )
endif()

message(STATUS "[openssl] found (${OPENSSL_VERSION}): ${OPENSSL_INCLUDE_DIR}")

add_library(openssl INTERFACE)
target_link_libraries(openssl INTERFACE OpenSSL::Crypto OpenSSL::SSL)

# Extras

# Check for OpenSSL static libraries
get_filename_component(OPENSSL_LIBRARY_PATH "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
find_library(OPENSSL_SSL_STATIC NAMES libssl.a HINTS "${OPENSSL_LIBRARY_PATH}" NO_DEFAULT_PATH NO_CACHE)
find_library(OPENSSL_CRYPTO_STATIC NAMES libcrypto.a HINTS "${OPENSSL_LIBRARY_PATH}" NO_DEFAULT_PATH NO_CACHE)

if(OPENSSL_SSL_STATIC AND OPENSSL_CRYPTO_STATIC)
  add_library(openssl_static INTERFACE)
  target_include_directories(openssl_static SYSTEM INTERFACE "${OPENSSL_INCLUDE_DIR}")
  target_link_libraries(openssl_static INTERFACE "${OPENSSL_SSL_STATIC}" "${OPENSSL_CRYPTO_STATIC}" ${CMAKE_DL_LIBS})
endif()

# Check for Openssl include/internal headers.
if(NOT EXISTS "${OPENSSL_INCLUDE_DIR}/internal" OR NOT IS_DIRECTORY "${OPENSSL_INCLUDE_DIR}/internal")
  set(MISSING_OPENSSL_INTERNAL TRUE)
endif()

# Check for Openssl include/crypto headers.
if(NOT EXISTS "${OPENSSL_INCLUDE_DIR}/crypto" OR NOT IS_DIRECTORY "${OPENSSL_INCLUDE_DIR}/crypto")
  set(MISSING_OPENSSL_CRYPTO TRUE)
endif()
