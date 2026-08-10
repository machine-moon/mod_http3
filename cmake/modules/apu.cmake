# -- Apache Portable Runtime Utils (APU) v1.6.0 --

if(TARGET apu)
  return()
endif()

set(APU_VERSION_MIN "1.6.0")

if(WIN32)
  include(windows/apu)
else()
  include(unix/apu)
endif()
