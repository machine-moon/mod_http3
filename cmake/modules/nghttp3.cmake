# -- nghttp3 v1.18.0 --

if(TARGET nghttp3)
  return()
endif()

set(NGHTTP3_VERSION_MIN "1.18.0")

if(WIN32)
  include(windows/nghttp3)
else()
  include(unix/nghttp3)
endif()
