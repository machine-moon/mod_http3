# -- probe httpd build for pyhttpd config --
find_program(APXS_EXECUTABLE NAMES apxs
  HINTS "${HTTPD_OUTPUT_DIRECTORY}/bin"
  NO_CACHE)
if(NOT APXS_EXECUTABLE)
  message(FATAL_ERROR "[test] apxs not found in ${HTTPD_OUTPUT_DIRECTORY}/bin - cannot configure pytest")
endif()

# apxs_query(<apxs-var> <out-var>) - sets <out-var> to `apxs -q <apxs-var>`
function(apxs_query name out)
  execute_process(COMMAND "${APXS_EXECUTABLE}" -q ${name}
    OUTPUT_VARIABLE value OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

apxs_query(prefix HTTPD_PREFIX)
apxs_query(progname HTTPD_NAME)
apxs_query(LIBEXECDIR LIBEXECDIR)
apxs_query(APR_BINDIR APR_BINDIR)
if(NOT HTTPD_VERSION)
  apxs_query(HTTPD_VERSION HTTPD_VERSION)
endif()
if(NOT HTTPD_PREFIX OR NOT HTTPD_VERSION OR NOT LIBEXECDIR)
  message(FATAL_ERROR "[test] ${APXS_EXECUTABLE} could not report prefix/HTTPD_VERSION/LIBEXECDIR")
endif()
if(NOT HTTPD_NAME)
  set(HTTPD_NAME "httpd")
endif()

# -- module lists from the installed DSOs --
file(GLOB _dso_files RELATIVE "${LIBEXECDIR}" "${LIBEXECDIR}/mod_*.so")
if(NOT _dso_files)
  message(FATAL_ERROR "[test] no mod_*.so found under ${LIBEXECDIR}")
endif()
set(DSO_MODULES "")
set(MPM_MODULES "")
foreach(_dso_file IN LISTS _dso_files)
  string(REGEX REPLACE "^mod_(.+)\\.so$" "\\1" _module "${_dso_file}")
  if(_module MATCHES "^mpm_")
    list(APPEND MPM_MODULES "${_module}")
  else()
    list(APPEND DSO_MODULES "${_module}")
  endif()
endforeach()
list(JOIN DSO_MODULES " " DSO_MODULES)
list(JOIN MPM_MODULES " " MPM_MODULES)

# -- curl for the pytest clients --
find_program(CURL_BIN NAMES curl HINTS "${CURL}" "${CURL}/.." "${CURL}/bin" "/opt/curl/bin" "/usr/bin" "/usr/local/bin" NO_DEFAULT_PATH NO_CACHE)
if(NOT CURL_BIN)
  message(WARNING "[test] curl not found - pytest clients will use 'curl' from PATH at runtime")
  set(CURL_BIN "curl")
else()
  execute_process(COMMAND "${CURL_BIN}" -V
    OUTPUT_VARIABLE _CURL_RESULT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(NOT _CURL_RESULT MATCHES "HTTP3")
    message(WARNING
      "[test] ${CURL_BIN} is built without HTTP/3 support")
  endif()
endif()

# -- pyhttpd config --
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pyhttpd/config.ini.in
               ${CMAKE_CURRENT_SOURCE_DIR}/pyhttpd/config.ini @ONLY)

# -- pytest target --
find_program(PYTEST_EXECUTABLE NAMES pytest py.test)
if(PYTEST_EXECUTABLE)
  add_custom_target(pytest
    COMMAND ${PYTEST_EXECUTABLE} -v
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    COMMENT "[pytest] Running pytest test suite..."
    VERBATIM
  )
else()
  message(STATUS "pytest not found - 'cmake --build build --target pytest' will not be available")
  message(STATUS "Install pytest with: pip install pytest")
  message(STATUS "Reconfigure or run tests manually: cd test && pytest -v")
endif()

# -- unit tests --
file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "unit/*.c")
add_executable(${PROJECT_NAME}_tests ${test_sources})
target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${PROJECT_NAME}-deps)

# ngtcp2 is PRIVATE to quic/ngtcp2/, so the dependency test asks for it by name.
if(ENABLE_NGTCP2)
  target_link_libraries(${PROJECT_NAME}_tests PRIVATE ngtcp2)
endif()
target_include_directories(${PROJECT_NAME}_tests PRIVATE ${CMAKE_SOURCE_DIR}/mod_http3/include ${CMAKE_CURRENT_SOURCE_DIR}/unit)

# -- register with CTest --
add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)
