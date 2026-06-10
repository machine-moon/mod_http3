# -- Apache Portable Runtime Utils (APU) v1.6.0 --

if(TARGET apu)
  return()
endif()

set(APU_VERSION_MIN "1.6.0")

# -- Find APU config tool --

if(WITH_APU)
  find_program(APU_CONFIG_EXECUTABLE NAMES apu-1-config apu-config HINTS "${WITH_APU}/bin" NO_DEFAULT_PATH NO_CACHE)
  if(NOT APU_CONFIG_EXECUTABLE)
    message(FATAL_ERROR
        "[apu] error: apu-config not found at WITH_APU=${WITH_APU}."
    )
  endif()
  set(APU_OUTPUT_DIRECTORY "${WITH_APU}")
elseif(WITH_APR)
  find_program(APU_CONFIG_EXECUTABLE NAMES apu-1-config apu-config HINTS "${WITH_APR}/bin" NO_DEFAULT_PATH NO_CACHE)
  if(NOT APU_CONFIG_EXECUTABLE)
    message(FATAL_ERROR
        "[apu] error: apu-config not found at WITH_APR=${WITH_APR}."
    )
  endif()
  set(APU_OUTPUT_DIRECTORY "${WITH_APR}")
elseif(WITH_HTTPD)
  # Query apxs to find APU location
  find_program(_APXS_EXECUTABLE NAMES apxs apxs2 HINTS "${WITH_HTTPD}/bin" NO_DEFAULT_PATH NO_CACHE)
  if(NOT _APXS_EXECUTABLE)
    message(FATAL_ERROR
        "[apu] error: apxs not found at WITH_HTTPD=${WITH_HTTPD}."
    )
  endif()

  execute_process(
    COMMAND "${_APXS_EXECUTABLE}" -q APU_BINDIR
    OUTPUT_VARIABLE _APU_BINDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _APU_BINDIR_RESULT
  )

  if(NOT _APU_BINDIR_RESULT EQUAL 0 OR NOT _APU_BINDIR)
    message(FATAL_ERROR
        "[apu] error: apxs could not query APU_BINDIR\n"
        "  apxs = ${_APXS_EXECUTABLE}\n"
        "  result = ${_APU_BINDIR_RESULT}"
    )
  endif()

  find_program(APU_CONFIG_EXECUTABLE NAMES apu-1-config apu-config HINTS "${_APU_BINDIR}" NO_DEFAULT_PATH NO_CACHE)
  if(NOT APU_CONFIG_EXECUTABLE)
    message(FATAL_ERROR
        "[apu] error: apu-config not found at APU_BINDIR=${_APU_BINDIR} (queried from apxs)."
    )
  endif()
  set(APU_OUTPUT_DIRECTORY "${_APU_BINDIR}")
else()

  set(APU_DIRECTORY "${DEPENDENCIES_DIRECTORY}/apr-util")
  set(APU_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/apr-util-dist")

  # Build apu from source if not already done
  if(NOT EXISTS "${APU_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${APU_DIRECTORY}")
    file(MAKE_DIRECTORY "${APU_OUTPUT_DIRECTORY}/logs")

    if(EXISTS "${APU_DIRECTORY}/Makefile")

      message(STATUS "[apu] Cleaning previous build artifacts")

      execute_process(
        COMMAND make distclean
        WORKING_DIRECTORY "${APU_DIRECTORY}"
        RESULT_VARIABLE _APU_DISTCLEAN_RESULT
        OUTPUT_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-distclean.log"
        ERROR_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-distclean.log")
      if(NOT _APU_DISTCLEAN_RESULT EQUAL 0)
        message(FATAL_ERROR "[apu] error: distclean failed -- see ${APU_OUTPUT_DIRECTORY}/logs/apu-distclean.log")
      endif()
    endif()

    message(STATUS "[apu] Configuring -> ${APU_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND ./buildconf --with-apr=${APR_DIRECTORY}
      WORKING_DIRECTORY "${APU_DIRECTORY}"
      RESULT_VARIABLE _APU_BUILDCONF_RESULT
      OUTPUT_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-buildconf.log"
      ERROR_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-buildconf.log")
    if(NOT _APU_BUILDCONF_RESULT EQUAL 0)
      message(FATAL_ERROR "[apu] error: buildconf failed -- see ${APU_OUTPUT_DIRECTORY}/logs/apu-buildconf.log")
    endif()

    execute_process(
      COMMAND ./configure --with-apr=${APR_OUTPUT_DIRECTORY} --prefix=${APU_OUTPUT_DIRECTORY}
      WORKING_DIRECTORY "${APU_DIRECTORY}"
      RESULT_VARIABLE _APU_CONFIGURE_RESULT
      OUTPUT_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-configure.log"
      ERROR_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-configure.log")
    if(NOT _APU_CONFIGURE_RESULT EQUAL 0)
      message(FATAL_ERROR "[apu] error: configure failed -- see ${APU_OUTPUT_DIRECTORY}/logs/apu-configure.log")
    endif()

    message(STATUS "[apu] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND make -j${DEPENDENCIES_PARALLEL}
      WORKING_DIRECTORY "${APU_DIRECTORY}"
      RESULT_VARIABLE _APU_BUILD_RESULT
      OUTPUT_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-build.log"
      ERROR_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-build.log")
    if(NOT _APU_BUILD_RESULT EQUAL 0)
      message(FATAL_ERROR "[apu] error: build failed -- see ${APU_OUTPUT_DIRECTORY}/logs/apu-build.log")
    endif()

    message(STATUS "[apu] Installing to ${APU_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND make install
      WORKING_DIRECTORY "${APU_DIRECTORY}"
      RESULT_VARIABLE _APU_INSTALL_RESULT
      OUTPUT_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-install.log"
      ERROR_FILE "${APU_OUTPUT_DIRECTORY}/logs/apu-install.log")
    if(NOT _APU_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "[apu] error: install failed -- see ${APU_OUTPUT_DIRECTORY}/logs/apu-install.log")
    endif()

    string(TIMESTAMP _APU_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${APU_OUTPUT_DIRECTORY}/.done" "${_APU_DONE_TIME}")
  endif()

  # Find the apu we just built

  find_program(
    APU_CONFIG_EXECUTABLE
    NAMES apu-1-config apu-config
    HINTS "${APU_OUTPUT_DIRECTORY}/bin"
    NO_DEFAULT_PATH REQUIRED NO_CACHE)
endif()

# -- Extract APU information --

# version
execute_process(
  COMMAND "${APU_CONFIG_EXECUTABLE}" --version
  OUTPUT_VARIABLE APU_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE APU_VERSION_RESULT
)
if(NOT APU_VERSION_RESULT EQUAL 0 OR NOT APU_VERSION OR APU_VERSION VERSION_LESS APU_VERSION_MIN)
  message(FATAL_ERROR
    "[apu] error: APU_CONFIG did not report a valid version\n"
    "  APU_CONFIG  = ${APU_CONFIG_EXECUTABLE}\n"
    "  APU_VERSION = ${APU_VERSION}\n"
    "  result      = ${APU_VERSION_RESULT}")
endif()

# include dir
execute_process(
  COMMAND "${APU_CONFIG_EXECUTABLE}" --includedir
  OUTPUT_VARIABLE APU_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE APU_INCLUDEDIR_RESULT
)
if(NOT APU_INCLUDEDIR_RESULT EQUAL 0 OR NOT APU_INCLUDE_DIR OR NOT EXISTS "${APU_INCLUDE_DIR}/apu.h")
  message(FATAL_ERROR
    "[apu] error: APU_CONFIG did not report a valid include dir\n"
    "  APU_CONFIG = ${APU_CONFIG_EXECUTABLE}\n"
    "  includedir = ${APU_INCLUDE_DIR}\n"
    "  result     = ${APU_INCLUDEDIR_RESULT}")
endif()

# link flags
execute_process(
  COMMAND "${APU_CONFIG_EXECUTABLE}" --link-ld --libs
  OUTPUT_VARIABLE APU_LINK_FLAGS
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE APU_LINK_FLAGS_RESULT
)
if(NOT APU_LINK_FLAGS_RESULT EQUAL 0 OR NOT APU_LINK_FLAGS)
  message(FATAL_ERROR
    "[apu] error: APU_CONFIG did not report valid link flags\n"
    "  APU_CONFIG = ${APU_CONFIG_EXECUTABLE}\n"
    "  link flags = ${APU_LINK_FLAGS}\n"
    "  result     = ${APU_LINK_FLAGS_RESULT}")
endif()

message(STATUS "[apu] found (${APU_VERSION}): ${APU_INCLUDE_DIR}")

separate_arguments(APU_LINK_FLAGS_LIST UNIX_COMMAND "${APU_LINK_FLAGS}")
string(REGEX MATCH "-L([^ \t]+)" APU_L_MATCH "${APU_LINK_FLAGS}")
set(APU_LIB_DIR "${CMAKE_MATCH_1}")

add_library(apu INTERFACE)
target_include_directories(apu SYSTEM INTERFACE "${APU_INCLUDE_DIR}")
target_link_options(apu INTERFACE ${APU_LINK_FLAGS_LIST})
if(APU_LIB_DIR)
  target_link_options(apu INTERFACE "-Wl,-rpath,${APU_LIB_DIR}")
endif()