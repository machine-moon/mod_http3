# -- ngtcp2 v1.25.0 --

if(TARGET ngtcp2)
  return()
endif()

set(NGTCP2_VERSION_MIN "1.25.0")

if(WITH_NGTCP2)
  find_library(NGTCP2_LIBRARY NAMES ngtcp2
    PATHS "${WITH_NGTCP2}/lib" "${WITH_NGTCP2}/lib64" NO_DEFAULT_PATH)
  find_library(NGTCP2_CRYPTO_OSSL_LIBRARY NAMES ngtcp2_crypto_ossl
    PATHS "${WITH_NGTCP2}/lib" "${WITH_NGTCP2}/lib64" NO_DEFAULT_PATH)
  if(NOT NGTCP2_LIBRARY OR NOT NGTCP2_CRYPTO_OSSL_LIBRARY)
    message(FATAL_ERROR
        "[ngtcp2] error: ngtcp2 or ngtcp2_crypto_ossl not found at WITH_NGTCP2=${WITH_NGTCP2}. Ensure ngtcp2 was built with OpenSSL support (--with-openssl)."
    )
  endif()
  set(NGTCP2_OUTPUT_DIRECTORY "${WITH_NGTCP2}")
else()

  set(NGTCP2_DIRECTORY "${QUIC_DEPENDENCIES_DIRECTORY}/ngtcp2")
  set(NGTCP2_OUTPUT_DIRECTORY "${QUIC_DEPENDENCIES_OUTPUT_DIRECTORY}/ngtcp2-dist")

  # Build ngtcp2 from source if not already done
  if(NOT EXISTS "${NGTCP2_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${NGTCP2_DIRECTORY}")
    file(MAKE_DIRECTORY "${NGTCP2_OUTPUT_DIRECTORY}/logs")

    if(EXISTS "${NGTCP2_DIRECTORY}/Makefile")
      file(REMOVE "${NGTCP2_DIRECTORY}/Makefile")
    endif()

    message(STATUS "[ngtcp2] Configuring -> ${NGTCP2_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND autoreconf -i
      WORKING_DIRECTORY "${NGTCP2_DIRECTORY}"
      RESULT_VARIABLE _NGTCP2_AUTORECONF_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-autoreconf.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-autoreconf.log")
    if(NOT _NGTCP2_AUTORECONF_RESULT EQUAL 0)
      message(FATAL_ERROR "[ngtcp2] error: autoreconf failed -- see ${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-autoreconf.log")
    endif()

    # ngtcp2 locates OpenSSL through pkg-config; point it at the one we built.
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E env
              "PKG_CONFIG_PATH=${OPENSSL_OUTPUT_DIRECTORY}/lib64/pkgconfig:${OPENSSL_OUTPUT_DIRECTORY}/lib/pkgconfig"
              ./configure --prefix=${NGTCP2_OUTPUT_DIRECTORY} --enable-lib-only --with-openssl
      WORKING_DIRECTORY "${NGTCP2_DIRECTORY}"
      RESULT_VARIABLE _NGTCP2_CONFIGURE_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log")
    if(NOT _NGTCP2_CONFIGURE_RESULT EQUAL 0)
      message(FATAL_ERROR "[ngtcp2] error: configure failed -- see ${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log")
    endif()

    message(STATUS "[ngtcp2] Cleaning workspace")
    execute_process(
      COMMAND make clean
      WORKING_DIRECTORY "${NGTCP2_DIRECTORY}"
      RESULT_VARIABLE _NGTCP2_CLEAN_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-clean.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-clean.log")
    if(NOT _NGTCP2_CLEAN_RESULT EQUAL 0)
      message(FATAL_ERROR "[ngtcp2] error: make clean failed -- see ${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-clean.log")
    endif()

    message(STATUS "[ngtcp2] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND make -j${DEPENDENCIES_PARALLEL}
      WORKING_DIRECTORY "${NGTCP2_DIRECTORY}"
      RESULT_VARIABLE _NGTCP2_BUILD_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log")
    if(NOT _NGTCP2_BUILD_RESULT EQUAL 0)
      message(FATAL_ERROR "[ngtcp2] error: build failed -- see ${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log")
    endif()

    message(STATUS "[ngtcp2] Installing to ${NGTCP2_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND make install
      WORKING_DIRECTORY "${NGTCP2_DIRECTORY}"
      RESULT_VARIABLE _NGTCP2_INSTALL_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-install.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-install.log")
    if(NOT _NGTCP2_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "[ngtcp2] error: install failed -- see ${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-install.log")
    endif()

    string(TIMESTAMP _NGTCP2_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${NGTCP2_OUTPUT_DIRECTORY}/.done" "${_NGTCP2_DONE_TIME}")
  endif()

  # Find the ngtcp2 we just built
  find_library(NGTCP2_LIBRARY NAMES ngtcp2 PATHS "${NGTCP2_OUTPUT_DIRECTORY}/lib" "${NGTCP2_OUTPUT_DIRECTORY}/lib64" NO_DEFAULT_PATH)
  find_library(NGTCP2_CRYPTO_OSSL_LIBRARY NAMES ngtcp2_crypto_ossl PATHS "${NGTCP2_OUTPUT_DIRECTORY}/lib" "${NGTCP2_OUTPUT_DIRECTORY}/lib64" NO_DEFAULT_PATH)
  if(NOT NGTCP2_LIBRARY OR NOT NGTCP2_CRYPTO_OSSL_LIBRARY)
    message(FATAL_ERROR "[ngtcp2] error: ngtcp2 or ngtcp2_crypto_ossl missing after build.")
  endif()
endif()

# Verify version is >= NGTCP2_VERSION_MIN
file(READ "${NGTCP2_OUTPUT_DIRECTORY}/include/ngtcp2/version.h" _NGTCP2_VERSION_H_CONTENT)
string(REGEX MATCH "#define NGTCP2_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${_NGTCP2_VERSION_H_CONTENT}")
set(NGTCP2_VERSION "${CMAKE_MATCH_1}")

if(NOT NGTCP2_VERSION OR NGTCP2_VERSION VERSION_LESS NGTCP2_VERSION_MIN)
  message(FATAL_ERROR
    "[ngtcp2] error: could not determine a valid version\n"
    "  NGTCP2_INCLUDE_DIR = ${NGTCP2_OUTPUT_DIRECTORY}/include\n"
    "  NGTCP2_VERSION     = ${NGTCP2_VERSION}\n"
    "  NGTCP2_VERSION_MIN = ${NGTCP2_VERSION_MIN}")
endif()

message(STATUS "[ngtcp2] found (${NGTCP2_VERSION}): ${NGTCP2_OUTPUT_DIRECTORY}")

add_library(ngtcp2 INTERFACE)
target_include_directories(ngtcp2 SYSTEM INTERFACE "${NGTCP2_OUTPUT_DIRECTORY}/include")
target_link_libraries(ngtcp2 INTERFACE "${NGTCP2_CRYPTO_OSSL_LIBRARY}" "${NGTCP2_LIBRARY}" openssl)

# Extras

find_library(NGTCP2_STATIC_LIBRARY NAMES libngtcp2.a
  PATHS "${NGTCP2_OUTPUT_DIRECTORY}/lib" "${NGTCP2_OUTPUT_DIRECTORY}/lib64" NO_DEFAULT_PATH NO_CACHE)
find_library(NGTCP2_CRYPTO_OSSL_STATIC_LIBRARY NAMES libngtcp2_crypto_ossl.a
  PATHS "${NGTCP2_OUTPUT_DIRECTORY}/lib" "${NGTCP2_OUTPUT_DIRECTORY}/lib64" NO_DEFAULT_PATH NO_CACHE)

if(NGTCP2_STATIC_LIBRARY AND NGTCP2_CRYPTO_OSSL_STATIC_LIBRARY)
  add_library(ngtcp2_static INTERFACE)
  target_include_directories(ngtcp2_static SYSTEM INTERFACE "${NGTCP2_OUTPUT_DIRECTORY}/include")
  target_link_libraries(ngtcp2_static INTERFACE "${NGTCP2_CRYPTO_OSSL_STATIC_LIBRARY}" "${NGTCP2_STATIC_LIBRARY}" openssl_static)
endif()
