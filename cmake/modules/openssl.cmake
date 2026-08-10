# -- OpenSSL v3.5.0+ --

if(TARGET openssl)
  return()
endif()

set(OPENSSL_VERSION_MIN "3.5.0")

if(WIN32)
  include(windows/openssl)
else()
  include(unix/openssl)
endif()
