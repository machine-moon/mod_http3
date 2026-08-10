# -- Apache Portable Runtime (APR) v1.7.0 --

if(TARGET apr)
  return()
endif()

set(APR_VERSION_MIN "1.7.0")

if(WIN32)
  include(windows/apr)
else()
  include(unix/apr)
endif()
