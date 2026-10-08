# -- ngtcp2 --
if(TARGET ngtcp2)
  return()
endif()

set(NGTCP2_VERSION_MIN "1.25.0")

if(WIN32)
  include(windows/ngtcp2)
else()
  include(unix/ngtcp2)
endif()

find_library(NGTCP2_LIBRARY NAMES ngtcp2
  PATHS "${NGTCP2_OUTPUT_DIRECTORY}/lib" "${NGTCP2_OUTPUT_DIRECTORY}/lib64" NO_DEFAULT_PATH)
find_library(NGTCP2_CRYPTO_OSSL_LIBRARY NAMES ngtcp2_crypto_ossl
  PATHS "${NGTCP2_OUTPUT_DIRECTORY}/lib" "${NGTCP2_OUTPUT_DIRECTORY}/lib64" NO_DEFAULT_PATH)
if(NOT NGTCP2_LIBRARY OR NOT NGTCP2_CRYPTO_OSSL_LIBRARY)
  message(FATAL_ERROR
    "[ngtcp2] error: ngtcp2 or ngtcp2_crypto_ossl not found in ${NGTCP2_OUTPUT_DIRECTORY}. "
    "ngtcp2 must be built with OpenSSL support.")
endif()

file(READ "${NGTCP2_OUTPUT_DIRECTORY}/include/ngtcp2/version.h" _NGTCP2_VERSION_H_CONTENT)
string(REGEX MATCH "#define NGTCP2_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${_NGTCP2_VERSION_H_CONTENT}")
set(NGTCP2_VERSION "${CMAKE_MATCH_1}")

if(NOT NGTCP2_VERSION OR NGTCP2_VERSION VERSION_LESS NGTCP2_VERSION_MIN)
  message(FATAL_ERROR
    "[ngtcp2] error: need >= ${NGTCP2_VERSION_MIN}, found ${NGTCP2_VERSION} in ${NGTCP2_OUTPUT_DIRECTORY}")
endif()

message(STATUS "[ngtcp2] found (${NGTCP2_VERSION}): ${NGTCP2_OUTPUT_DIRECTORY}")

add_library(ngtcp2 INTERFACE)
target_include_directories(ngtcp2 SYSTEM INTERFACE "${NGTCP2_OUTPUT_DIRECTORY}/include")
target_link_libraries(ngtcp2 INTERFACE "${NGTCP2_CRYPTO_OSSL_LIBRARY}" "${NGTCP2_LIBRARY}" openssl)
