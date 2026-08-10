# -- Apache httpd v2.4.x (20211221) --

if(TARGET httpd)
  return()
endif()

set(HTTPD_VERSION_MIN "2.4.x")
set(HTTPD_MMN_MIN "20211221")

if(WIN32)
  include(windows/httpd)
else()
  include(unix/httpd)
endif()
