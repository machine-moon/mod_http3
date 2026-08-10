# -- Function to apply compile, warning, and sanitizer flags to a target
function(apply_target_flags target)

  # Assertions
  if(NOT TARGET ${target})
    message(FATAL_ERROR "${target} is not a target, no flags can be added.")
  endif()

  if((ENABLE_ASAN OR ENABLE_UBSAN) AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Sanitizers require Debug build type. Current build type: ${CMAKE_BUILD_TYPE}")
  endif()

  if(MSVC AND ENABLE_UBSAN)
    message(FATAL_ERROR "ENABLE_UBSAN is unavailable with MSVC; only ENABLE_ASAN is.")
  endif()

  # Set scope based on target type (INTERFACE vs non-INTERFACE)
  get_target_property(_type ${target} TYPE)
  if(_type STREQUAL "INTERFACE_LIBRARY")
    set(scope "INTERFACE")
  else()
    set(scope "PRIVATE")
  endif()

  if(MSVC)

    # -- Compile flags --
    if(ENABLE_ASAN)
      set(FLAGS_DEBUG "/Od;/Zi")
    else()
      set(FLAGS_DEBUG "/Od;/Zi;/RTC1")
    endif()
    set(FLAGS_RELEASE "/O2;/Zi")

    # -- Warning flags --
    set(_WARNINGS
        /W4 # Baseline reasonable warnings
        /w14242 # Conversion from 'type1' to 'type2', possible loss of data
        /w14254 # Bitfield conversion, possible loss of data
        /w14287 # Unsigned/negative constant mismatch
        /w14296 # Expression is always 'boolean_value'
        /w14311 # Pointer truncation from 'type1' to 'type2'
        /w14456 # Declaration hides previous local declaration
        /w14457 # Declaration hides function parameter
        /w14459 # Declaration hides global declaration
        /w14477 # Format string does not match the argument
        /w14545 # Expression before comma evaluates to a function missing an argument list
        /w14546 # Function call before comma missing argument list
        /w14547 # Operator before comma has no effect; expected one with a side-effect
        /w14549 # Operator before comma has no effect; did you intend 'operator'?
        /w14555 # Expression has no effect; expected one with a side-effect
        /w14619 # Pragma warning: there is no warning number
        /w14826 # Conversion is sign-extended, which may cause unexpected behaviour
        /w14905 # Wide string literal cast to 'LPSTR'
        /w14906 # String literal cast to 'LPWSTR'
    )
    if(ENABLE_WERROR)
      list(APPEND _WARNINGS /WX)
    endif()

    # -- Other flags --
    set(_OTHER
        /Zc:preprocessor # Conformant preprocessor, for the variadic macros in h3_check.h
        /FIh3_os.h # winsock2.h must precede the windows.h that OpenSSL pulls in
        /D_CRT_SECURE_NO_WARNINGS # getenv/fopen are used as documented; the "safe" variants are MSVC-only
    )

    # -- Sanitizer flags --
    if(ENABLE_ASAN)
      target_compile_options(${target} ${scope} /fsanitize=address)
      target_link_options(${target} ${scope} /INCREMENTAL:NO)
    endif()

  else()

    # -- Compile flags --
    set(FLAGS_DEBUG "-g3;-O0")
    set(FLAGS_RELEASE "-g;-O3")

    # -- Warning flags --
    set(_WARNINGS
        -Wall # Enable all standard warnings
        -Wextra # Reasonable and standard
        -Wshadow # Warn if a variable declaration shadows one from a parent context
        -Wcast-align # Warn for potential performance problem casts
        -Wno-unused-function # Warn on unused functions
        -Wpedantic # Warn if non-standard C is used
        -Wconversion # Warn on type conversions that may lose data
        -Wsign-conversion # Warn on sign conversions
        -Wnull-dereference # Warn if a null dereference is detected
        -Wdouble-promotion # Warn if float is implicitly promoted to double
        -Wformat=2 # Warn on security issues around functions that format output (ie printf)
        -Wmisleading-indentation # Warn if indentation implies blocks where blocks do not exist
        # GCC Exclusive Warnings:
        -Wduplicated-cond # Warn if if/else chain has duplicated conditions
        -Wduplicated-branches # Warn if if/else branches have duplicated code
        -Wlogical-op # Warn about logical operations being used where bitwise were probably wanted
    )
    if(ENABLE_WERROR)
      list(APPEND _WARNINGS -Werror)
    endif()

    # -- Other flags --
    set(_OTHER "")

    # -- Sanitizer flags --
    set(_sanitize_parts "")
    if(ENABLE_UBSAN)
      list(APPEND _sanitize_parts "undefined")
    endif()
    if(ENABLE_ASAN)
      list(APPEND _sanitize_parts "address")
    endif()

    if(_sanitize_parts)
      list(JOIN _sanitize_parts "," _sanitize_value)
      target_compile_options(${target} ${scope} -fsanitize=${_sanitize_value} -fno-omit-frame-pointer)
      target_link_options(${target} ${scope} -fsanitize=${_sanitize_value})
    endif()

  endif()

  # -- Apply compile and warning flags --
  target_compile_options(
    ${target}
    ${scope}
    $<$<CONFIG:Debug>:${FLAGS_DEBUG}>
    $<$<CONFIG:Release>:${FLAGS_RELEASE}>
    ${_WARNINGS}
    ${_OTHER})

endfunction()
