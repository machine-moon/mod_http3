cmake_minimum_required(VERSION 3.26)

project(mod_http3 VERSION 0.0.37)

# -- Compiler and Build Type Checks --
if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
  message(FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}. Only GCC is supported for now.")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# -- CMake configuration --
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Release")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_FIND_PACKAGE_RESOLVE_SYMLINKS TRUE)

# -- CMake Modules Path --
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/utils")

# -- Global CMake Defaults --
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# -- Dependencies --
set(DEPENDENCIES_DIRECTORY "${CMAKE_SOURCE_DIR}/dependencies" CACHE PATH "Directory for dependencies")
set(DEPENDENCIES_OUTPUT_DIRECTORY "${DEPENDENCIES_DIRECTORY}" CACHE PATH "Directory for placing built dependencies")
set(DEPENDENCIES_PARALLEL 6 CACHE STRING "Number of parallel jobs to use when building dependencies")

# -- C standard --
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# -- Project options --
option(BUILD_MODULE "Build Apache DSO module" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test suite" ON)

option(ENABLE_UBSAN "Undefined Behavior Sanitizer" OFF)
option(ENABLE_ASAN "Address Sanitizer" OFF)
option(ENABLE_WERROR "Treat warnings as errors" OFF)

set(WITH_NGHTTP3 "" CACHE PATH "Path to nghttp3 installation prefix")
set(WITH_SSL "" CACHE PATH "Path to OpenSSL installation prefix")
set(WITH_HTTPD "" CACHE PATH "Path to httpd installation prefix (includes APR/APU)")
set(WITH_APR "" CACHE PATH "Path to APR installation prefix")
set(WITH_APU "" CACHE PATH "Path to APR-util installation prefix")

# -- Generate h3_version.h --
include(version.h)

# -- Utility includes --
include(helpers)
include(flags)

# -- Dependencies Library --
add_library(${PROJECT_NAME}-deps INTERFACE)
include(nghttp3)
include(openssl)
include(httpd)
target_link_libraries(${PROJECT_NAME}-deps INTERFACE nghttp3 openssl httpd)

# -- Core object library --
file(GLOB_RECURSE sources CONFIGURE_DEPENDS mod_http3/src/*.c)
add_library(${PROJECT_NAME}-core OBJECT ${sources})
target_link_libraries(${PROJECT_NAME}-core PUBLIC ${PROJECT_NAME}-deps)
set_target_properties(${PROJECT_NAME}-core PROPERTIES C_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
target_include_directories(${PROJECT_NAME}-core PUBLIC mod_http3/include)
apply_target_flags(${PROJECT_NAME}-core)

# -- Dynamic Shared Object (DSO) --
if(BUILD_MODULE)
  add_library(${PROJECT_NAME}-lib MODULE $<TARGET_OBJECTS:${PROJECT_NAME}-core>)
  target_link_libraries(${PROJECT_NAME}-lib PRIVATE ${PROJECT_NAME}-deps)

  set_target_properties(${PROJECT_NAME}-lib PROPERTIES PREFIX "" OUTPUT_NAME "${PROJECT_NAME}")
  apply_target_flags(${PROJECT_NAME}-lib)
endif()

# -- Examples --
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# -- Tests --
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# -- Generic installation --
install(TARGETS ${PROJECT_NAME}-lib
  LIBRARY DESTINATION lib64/httpd/modules
  COMPONENT generic)
install(FILES "${CMAKE_SOURCE_DIR}/CHANGES"
  "${CMAKE_SOURCE_DIR}/NOTICE"
  "${CMAKE_SOURCE_DIR}/AUTHORS"
  DESTINATION share/doc/mod_http3
  COMPONENT generic)
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
  DESTINATION share/licenses/mod_http3
  COMPONENT generic)

# -- Summary --
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "")
message(STATUS "  Version        : ${PROJECT_VERSION}")
message(STATUS "  Platform       : ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler       : ${CMAKE_C_COMPILER_ID} (${CMAKE_C_COMPILER_VERSION})")
message(STATUS "  Build Type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "  BUILD_MODULE   : ${BUILD_MODULE}")
message(STATUS "  BUILD_EXAMPLES : ${BUILD_EXAMPLES}")
message(STATUS "  BUILD_TESTS    : ${BUILD_TESTS}")
message(STATUS "")
message(STATUS "  ENABLE_UBSAN   : ${ENABLE_UBSAN}")
message(STATUS "  ENABLE_ASAN    : ${ENABLE_ASAN}")
message(STATUS "  ENABLE_WERROR  : ${ENABLE_WERROR}")
message(STATUS "")

# -- CPACK --
include(packaging)
