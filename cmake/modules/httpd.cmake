# -- Apache httpd 2.4.69+ (stable) or 2.5.1+ (devel) --

if(TARGET httpd)
  return()
endif()

set(HTTPD_STABLE_VERSION_MIN "2.4.69")
set(HTTPD_STABLE_MMN_MIN "20120211")

set(HTTPD_DEVEL_VERSION_MIN "2.5.1")
set(HTTPD_DEVEL_MMN_MIN "20211221")

if(WIN32)
  include(windows/httpd)
else()
  include(unix/httpd)
endif()
