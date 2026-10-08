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
      COMMAND "${CMAKE_COMMAND}" -S "${NGTCP2_DIRECTORY}" -B "${CMAKE_BINARY_DIR}/deps/ngtcp2" -G "${CMAKE_GENERATOR}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -DVCPKG_APPLOCAL_DEPS=OFF
        "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DCMAKE_INSTALL_PREFIX=${NGTCP2_OUTPUT_DIRECTORY}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
        "-DOPENSSL_ROOT_DIR=${OPENSSL_OUTPUT_DIRECTORY}"
        -DENABLE_OPENSSL=ON
        -DENABLE_LIB_ONLY=ON
        -DBUILD_TESTING=OFF
        -DENABLE_SHARED_LIB=ON
        -DENABLE_STATIC_LIB=ON
      RESULT_VARIABLE _NGTCP2_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log")
    if(NOT _NGTCP2_RESULT EQUAL 0)
      file(READ "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-configure.log" _NGTCP2_LOG)
      message(FATAL_ERROR "[ngtcp2] error: configure failed\n${_NGTCP2_LOG}")
    endif()

    message(STATUS "[ngtcp2] Building (${DEPENDENCIES_PARALLEL} jobs)")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/deps/ngtcp2" --parallel ${DEPENDENCIES_PARALLEL} --config ${CMAKE_BUILD_TYPE} --target install
      RESULT_VARIABLE _NGTCP2_RESULT
      OUTPUT_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log"
      ERROR_FILE "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log")
    if(NOT _NGTCP2_RESULT EQUAL 0)
      file(READ "${NGTCP2_OUTPUT_DIRECTORY}/logs/ngtcp2-build.log" _NGTCP2_LOG)
      message(FATAL_ERROR "[ngtcp2] error: build failed\n${_NGTCP2_LOG}")
    endif()

    string(TIMESTAMP _NGTCP2_DONE_TIME "%Y-%b-%d_%H-%M-%S")
    file(WRITE "${NGTCP2_OUTPUT_DIRECTORY}/.done" "${_NGTCP2_DONE_TIME}")
  endif()
endif()
