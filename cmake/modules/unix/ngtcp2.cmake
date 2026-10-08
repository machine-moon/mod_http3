if(WITH_NGTCP2)
  set(NGTCP2_OUTPUT_DIRECTORY "${WITH_NGTCP2}")
else()
  set(NGTCP2_DIRECTORY "${DEPENDENCIES_DIRECTORY}/ngtcp2")
  set(NGTCP2_OUTPUT_DIRECTORY "${DEPENDENCIES_OUTPUT_DIRECTORY}/ngtcp2-dist")

  if(NOT EXISTS "${NGTCP2_OUTPUT_DIRECTORY}/.done")
    require_initialized_submodule("${NGTCP2_DIRECTORY}")
    file(MAKE_DIRECTORY "${NGTCP2_OUTPUT_DIRECTORY}/logs")

    message(STATUS "[ngtcp2] Configuring -> ${NGTCP2_OUTPUT_DIRECTORY}")
    execute_process(
      COMMAND autoreconf -i
      WORKING_DIRECTORY "${NGTCP2_DIRECTORY}"
      RESULT_VARIABLE _NGTCP2_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-autoreconf.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-autoreconf.log")
    if(NOT _NGTCP2_RESULT EQUAL 0)
      message(FATAL_ERROR "[ngtcp2] error: autoreconf failed -- see ${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-autoreconf.log")
    endif()

    # ngtcp2 finds OpenSSL through pkg-config; point it at the one we use.
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E env
              "PKG_CONFIG_PATH=${OPENSSL_OUTPUT_DIRECTORY}/lib64/pkgconfig:${OPENSSL_OUTPUT_DIRECTORY}/lib/pkgconfig"
              ./configure --prefix=${NGTCP2_OUTPUT_DIRECTORY} --enable-lib-only --with-openssl
      WORKING_DIRECTORY "${NGTCP2_DIRECTORY}"
      RESULT_VARIABLE _NGTCP2_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log")
    if(NOT _NGTCP2_RESULT EQUAL 0)
      message(FATAL_ERROR "[ngtcp2] error: configure failed -- see ${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log")
    endif()

    message(STATUS "[ngtcp2] Building (${DEPENDENCIES_PARALLEL} jobs)")
    execute_process(
      COMMAND make -j${DEPENDENCIES_PARALLEL} install
      WORKING_DIRECTORY "${NGTCP2_DIRECTORY}"
      RESULT_VARIABLE _NGTCP2_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log")
    if(NOT _NGTCP2_RESULT EQUAL 0)
      message(FATAL_ERROR "[ngtcp2] error: build failed -- see ${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log")
    endif()

    string(TIMESTAMP _NGTCP2_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${NGTCP2_OUTPUT_DIRECTORY}/.done" "${_NGTCP2_DONE_TIME}")
  endif()
endif()
