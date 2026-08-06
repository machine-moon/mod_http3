# -- Apache httpd (trunk MMN 20211221+ preferred; 2.4.52+ via compat layer) --

if(TARGET httpd)
  return()
endif()

# 2.4.52 is the floor: ap_create_request (2.4.49), the child_stopping hook
# (2.4.49) and ap_thread_current (2.4.52) must exist. Against a 2.4.x server
# mod_http3 uses its response compat path (see mod_http3/include/h3_compat.h).
# The MMN floor stays on the 2.4.x major (20120211); trunk reports 20211221 and
# compares greater, so both satisfy it.
set(HTTPD_VERSION_MIN "2.4.52")
set(HTTPD_MMN_MIN "20120211")

if(WIN32)
  include(windows/httpd)
else()
  include(unix/httpd)
endif()
