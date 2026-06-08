# -- Apache Portable Runtime (APR) v1.7.0 --

if(TARGET apr)
  return()
endif()

set(APR_DIRECTORY "${DEPENDENCIES_DIRECTORY}/apr")
set(APR_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/apr-dist")

set(APR_VERSION_MIN "1.7.0")

# -- Find APR config tool --

if(BUILD_HTTPD)

  # Build apr from source if not already done
  if(NOT EXISTS "${APR_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${APR_DIRECTORY}")
    file(MAKE_DIRECTORY "${APR_OUTPUT_DIRECTORY}/logs")

    if(EXISTS "${APR_DIRECTORY}/Makefile")

      message(STATUS "[apr] Cleaning previous build artifacts")

      execute_process(
        COMMAND make distclean
        WORKING_DIRECTORY "${APR_DIRECTORY}"
        RESULT_VARIABLE _APR_DISTCLEAN_RESULT
        OUTPUT_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-distclean.log"
        ERROR_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-distclean.log")
      if(NOT _APR_DISTCLEAN_RESULT EQUAL 0)
        message(FATAL_ERROR "[apr] error: distclean failed -- see ${APR_OUTPUT_DIRECTORY}/logs/apr-distclean.log")
      endif()
    endif()

    message(STATUS "[apr] Configuring -> ${APR_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND ./buildconf
      WORKING_DIRECTORY "${APR_DIRECTORY}"
      RESULT_VARIABLE _APR_BUILDCONF_RESULT
      OUTPUT_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-buildconf.log"
      ERROR_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-buildconf.log")
    if(NOT _APR_BUILDCONF_RESULT EQUAL 0)
      message(FATAL_ERROR "[apr] error: buildconf failed -- see ${APR_OUTPUT_DIRECTORY}/logs/apr-buildconf.log")
    endif()

    execute_process(
      COMMAND ./configure --prefix=${APR_OUTPUT_DIRECTORY}
      WORKING_DIRECTORY "${APR_DIRECTORY}"
      RESULT_VARIABLE _APR_CONFIGURE_RESULT
      OUTPUT_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-configure.log"
      ERROR_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-configure.log")
    if(NOT _APR_CONFIGURE_RESULT EQUAL 0)
      message(FATAL_ERROR "[apr] error: configure failed -- see ${APR_OUTPUT_DIRECTORY}/logs/apr-configure.log")
    endif()

    message(STATUS "[apr] Building (${DEPENDENCIES_PARALLEL} jobs)")

    execute_process(
      COMMAND make -j${DEPENDENCIES_PARALLEL}
      WORKING_DIRECTORY "${APR_DIRECTORY}"
      RESULT_VARIABLE _APR_BUILD_RESULT
      OUTPUT_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-build.log"
      ERROR_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-build.log")
    if(NOT _APR_BUILD_RESULT EQUAL 0)
      message(FATAL_ERROR "[apr] error: build failed -- see ${APR_OUTPUT_DIRECTORY}/logs/apr-build.log")
    endif()

    message(STATUS "[apr] Installing to ${APR_OUTPUT_DIRECTORY}")

    execute_process(
      COMMAND make install
      WORKING_DIRECTORY "${APR_DIRECTORY}"
      RESULT_VARIABLE _APR_INSTALL_RESULT
      OUTPUT_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-install.log"
      ERROR_FILE "${APR_OUTPUT_DIRECTORY}/logs/apr-install.log")
    if(NOT _APR_INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "[apr] error: install failed -- see ${APR_OUTPUT_DIRECTORY}/logs/apr-install.log")
    endif()

    string(TIMESTAMP _APR_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${APR_OUTPUT_DIRECTORY}/.done" "${_APR_DONE_TIME}")
  endif()

  # Find the apr we just built

  find_program(
    APR_CONFIG_EXECUTABLE
    NAMES apr-1-config apr-config
    HINTS "${APR_OUTPUT_DIRECTORY}/bin"
    NO_DEFAULT_PATH REQUIRED NO_CACHE)
elseif(WITH_APR)
  find_program(APR_CONFIG_EXECUTABLE NAMES apr-1-config apr-config HINTS "${WITH_APR}/bin" NO_DEFAULT_PATH NO_CACHE)
  if(NOT APR_CONFIG_EXECUTABLE)
    message(FATAL_ERROR
        "[apr] error: apr-config not found at WITH_APR=${WITH_APR}."
    )
  endif()
elseif(WITH_HTTPD)
  # Query apxs to find APR location
  find_program(_APXS_EXECUTABLE NAMES apxs apxs2 HINTS "${WITH_HTTPD}/bin" NO_DEFAULT_PATH NO_CACHE)
  if(NOT _APXS_EXECUTABLE)
    message(FATAL_ERROR
        "[apr] error: apxs not found at WITH_HTTPD=${WITH_HTTPD}."
    )
  endif()

  execute_process(
    COMMAND "${_APXS_EXECUTABLE}" -q APR_BINDIR
    OUTPUT_VARIABLE _APR_BINDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _APR_BINDIR_RESULT
  )

  if(NOT _APR_BINDIR_RESULT EQUAL 0 OR NOT _APR_BINDIR)
    message(FATAL_ERROR
        "[apr] error: apxs could not query APR_BINDIR\n"
        "  apxs = ${_APXS_EXECUTABLE}\n"
        "  result = ${_APR_BINDIR_RESULT}"
    )
  endif()

  find_program(APR_CONFIG_EXECUTABLE NAMES apr-1-config apr-config HINTS "${_APR_BINDIR}" NO_DEFAULT_PATH NO_CACHE)
  if(NOT APR_CONFIG_EXECUTABLE)
    message(FATAL_ERROR
        "[apr] error: apr-config not found at APR_BINDIR=${_APR_BINDIR} (queried from apxs)."
    )
  endif()
else()
  message(FATAL_ERROR
      "[apr] error: set BUILD_HTTPD=ON to build from source or provide WITH_APR=/path/to/apr or WITH_HTTPD=/path/to/httpd."
  )
endif()

# -- Extract APR information --

# version
execute_process(
  COMMAND "${APR_CONFIG_EXECUTABLE}" --version
  OUTPUT_VARIABLE APR_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE APR_VERSION_RESULT
)
if(NOT APR_VERSION_RESULT EQUAL 0 OR NOT APR_VERSION OR APR_VERSION VERSION_LESS APR_VERSION_MIN)
  message(FATAL_ERROR
    "[apr] error: APR_CONFIG did not report a valid version\n"
    "  APR_CONFIG  = ${APR_CONFIG_EXECUTABLE}\n"
    "  APR_VERSION = ${APR_VERSION}\n"
    "  result      = ${APR_VERSION_RESULT}")
endif()

# include dir
execute_process(
  COMMAND "${APR_CONFIG_EXECUTABLE}" --includedir
  OUTPUT_VARIABLE APR_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE APR_INCLUDEDIR_RESULT
)
if(NOT APR_INCLUDEDIR_RESULT EQUAL 0 OR NOT APR_INCLUDE_DIR OR NOT EXISTS "${APR_INCLUDE_DIR}/apr.h")
  message(FATAL_ERROR
    "[apr] error: APR_CONFIG did not report a valid include dir\n"
    "  APR_CONFIG = ${APR_CONFIG_EXECUTABLE}\n"
    "  includedir = ${APR_INCLUDE_DIR}\n"
    "  result     = ${APR_INCLUDEDIR_RESULT}")
endif()

# link flags
execute_process(
  COMMAND "${APR_CONFIG_EXECUTABLE}" --link-ld --libs
  OUTPUT_VARIABLE APR_LINK_FLAGS
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE APR_LINK_FLAGS_RESULT
)
if(NOT APR_LINK_FLAGS_RESULT EQUAL 0 OR NOT APR_LINK_FLAGS)
  message(FATAL_ERROR
    "[apr] error: APR_CONFIG did not report valid link flags\n"
    "  APR_CONFIG = ${APR_CONFIG_EXECUTABLE}\n"
    "  link flags = ${APR_LINK_FLAGS}\n"
    "  result     = ${APR_LINK_FLAGS_RESULT}")
endif()

message(STATUS "[apr] found (${APR_VERSION}): ${APR_INCLUDE_DIR}")

separate_arguments(APR_LINK_FLAGS_LIST UNIX_COMMAND "${APR_LINK_FLAGS}")
string(REGEX MATCH "-L([^ \t]+)" APR_L_MATCH "${APR_LINK_FLAGS}")
set(APR_LIB_DIR "${CMAKE_MATCH_1}")

add_library(apr INTERFACE)
target_include_directories(apr SYSTEM INTERFACE "${APR_INCLUDE_DIR}")
target_link_options(apr INTERFACE ${APR_LINK_FLAGS_LIST})
if(APR_LIB_DIR)
  target_link_options(apr INTERFACE "-Wl,-rpath,${APR_LIB_DIR}")
endif()