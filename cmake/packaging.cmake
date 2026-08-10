# -- CPack packaging --

# -- Metadata --
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION "HTTP/3 module for Apache httpd")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${CPACK_PACKAGE_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "Humanity")
set(CPACK_PACKAGE_CONTACT "https://github.com/machine-moon/mod_http3/issues")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/machine-moon/mod_http3")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
set(CPACK_PACKAGE_RELOCATABLE FALSE)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
  set(PACKAGE_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
  set(PACKAGE_ARCH "aarch64")
else()
  set(PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}-${PACKAGE_ARCH}" PACKAGE_SYSTEM_METADATA)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_SOURCE_PACKAGE_FILE_NAME}-${PACKAGE_SYSTEM_METADATA}")

set(CPACK_GENERATOR "TGZ")
if(NOT WIN32)
  set(CPACK_GENERATOR "${CPACK_GENERATOR};DEB;RPM")
endif()

# -- Source archives --
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/[.]git/"
    "/[.]gitmodules$"
    "/[.]cache/"
    "/[.]pytest_cache/"
    "/[.]ruff_cache/"
    "/__pycache__/"
    "/[.]TODO$"
    "/build/"
    "/build-[^/]+/"
    "/dist/"
    "/dependencies/"
    "/certs/"
    "/pyhttpd/config[.]ini$"
    "/test/gen/"
    "/[.]libs/"
    "[.]o$"
    "[.]a$"
    "[.]so$"
    "[.]lo$"
    "[.]la$"
    "[.]lai$"
    "[.]slo$"
    "[.]log$"
    "[.]tar[.]gz$"
    "[.]zip$"
    "[.]rpm$"
    "[.]deb$"
    "[.]sha256$")

# -- Components --
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENTS_ALL generic)

# -- RPM package --
install(TARGETS ${PROJECT_NAME}-lib
  LIBRARY DESTINATION lib64/httpd/modules
  COMPONENT rpm EXCLUDE_FROM_ALL)
install(FILES "${CMAKE_SOURCE_DIR}/cmake/packaging/rpm/10-h3.conf"
  DESTINATION /etc/httpd/conf.modules.d
  COMPONENT rpm EXCLUDE_FROM_ALL)
install(FILES "${CMAKE_SOURCE_DIR}/CHANGES"
  "${CMAKE_SOURCE_DIR}/NOTICE"
  "${CMAKE_SOURCE_DIR}/AUTHORS"
  DESTINATION share/doc/mod_http3
  COMPONENT rpm EXCLUDE_FROM_ALL)
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
  DESTINATION share/licenses/mod_http3
  COMPONENT rpm EXCLUDE_FROM_ALL)

set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_SUMMARY "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_RELOCATABLE FALSE)
set(CPACK_RPM_PACKAGE_REQUIRES "httpd")
set(CPACK_RPM_USER_FILELIST
  "%config(noreplace) /etc/httpd/conf.modules.d/10-h3.conf"
  "%license /usr/share/licenses/mod_http3/LICENSE")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
  "/etc/httpd" "/etc/httpd/conf.modules.d"
  "/usr/lib64/httpd" "/usr/lib64/httpd/modules"
  "/usr/share/licenses")

# -- DEB package --
install(TARGETS ${PROJECT_NAME}-lib
  LIBRARY DESTINATION lib/apache2/modules
  COMPONENT deb EXCLUDE_FROM_ALL)
install(FILES "${CMAKE_SOURCE_DIR}/cmake/packaging/deb/http3.load"
  DESTINATION /etc/apache2/mods-available
  COMPONENT deb EXCLUDE_FROM_ALL)
install(FILES "${CMAKE_SOURCE_DIR}/CHANGES"
  "${CMAKE_SOURCE_DIR}/NOTICE"
  "${CMAKE_SOURCE_DIR}/AUTHORS"
  DESTINATION share/doc/mod_http3
  COMPONENT deb EXCLUDE_FROM_ALL)
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
  RENAME copyright
  DESTINATION share/doc/mod_http3
  COMPONENT deb EXCLUDE_FROM_ALL)

set(CPACK_DEBIAN_PACKAGE_NAME "mod_http3")
set(CPACK_DEBIAN_PACKAGE_SECTION "httpd")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "machine-moon <https://github.com/machine-moon/mod_http3/issues>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_DEBIAN_PACKAGE_SUMMARY "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "apache2")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)


if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
else()
  message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# -- CPack configuration file --
configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/packaging/CPackProjectConfig.cmake"
  "${CMAKE_BINARY_DIR}/CPackProjectConfig.cmake"
  @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackProjectConfig.cmake")


include(CPack)

add_custom_target(release DEPENDS ${PROJECT_NAME}-lib package)
