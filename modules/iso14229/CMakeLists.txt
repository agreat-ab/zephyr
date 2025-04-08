if(CONFIG_ISO14229)
  # Create the interface library
  zephyr_interface_library_named(iso14229)

  # Add include directories to the interface
  target_include_directories(iso14229 INTERFACE
    ${ZEPHYR_CURRENT_MODULE_DIR}/src
  )

  zephyr_compile_definitions(iso14229 INTERFACE
    UDS_TP_ISOTP_C
    UDS_SYS=UDS_SYS_UNIX
    UDS_LOG_LEVEL=CONFIG_ISO14229_LOG_LEVEL
    UDS_CUSTOM_MILLIS
  )

  set_property(GLOBAL PROPERTY CSTD c11)



  # Set up paths for amalgamated files
  set(AMALGAMATED_C ${CMAKE_CURRENT_BINARY_DIR}/iso14229.c)
  set(AMALGAMATED_H ${CMAKE_CURRENT_BINARY_DIR}/iso14229.h)

  # Create a directory for headers in the Zephyr structure
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/zephyr/lib/iso14229)
  set(ZEPHYR_HEADER ${CMAKE_CURRENT_BINARY_DIR}/include/zephyr/lib/iso14229/iso14229.h)

  # First, create the main library
  zephyr_library_named(iso14229_lib)

  # Add the amalgamation command as a source to be generated
  file(GLOB_RECURSE ISO14229_SOURCES "${ZEPHYR_CURRENT_MODULE_DIR}/src/**/*.c" "${ZEPHYR_CURRENT_MODULE_DIR}/src/**/*.h")

  # Create the custom command to generate the amalgamated files
  add_custom_command(
    OUTPUT ${AMALGAMATED_C} ${AMALGAMATED_H}
    COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_CURRENT_MODULE_DIR}/amalgamate.py
            --out_c ${AMALGAMATED_C}
            --out_h ${AMALGAMATED_H}
            ${ISO14229_SOURCES}
    DEPENDS ${ISO14229_SOURCES} ${ZEPHYR_CURRENT_MODULE_DIR}/amalgamate.py
    WORKING_DIRECTORY ${ZEPHYR_CURRENT_MODULE_DIR}
    COMMENT "Generating amalgamated iso14229 files"
    VERBATIM
  )

  # Copy the header to the Zephyr include structure
  add_custom_command(
    OUTPUT ${ZEPHYR_HEADER}
    COMMAND ${CMAKE_COMMAND} -E copy ${AMALGAMATED_H} ${ZEPHYR_HEADER}
    DEPENDS ${AMALGAMATED_H}
    COMMENT "Copying header to Zephyr include structure"
    VERBATIM
  )

  # Add the generated source to the library
  zephyr_library_sources(${AMALGAMATED_C})
  zephyr_library_sources(server.c)

  # Make sure the header is generated before compilation starts
  add_custom_target(iso14229_headers
    DEPENDS ${ZEPHYR_HEADER}
    COMMENT "Ensuring ISO14229 headers are available"
  )
  add_dependencies(iso14229_lib iso14229_headers)

  # Set compile definitions
  zephyr_library_compile_definitions(UDS_TP_ISOTP_C)
  zephyr_library_compile_definitions(UDS_SYS=UDS_SYS_UNIX)
  zephyr_library_compile_definitions(UDS_LOG_LEVEL=CONFIG_ISO14229_LOG_LEVEL)

  # Add the include path for the generated header
  zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

  # Add the include path for the module
  zephyr_library_include_directories(include)
  zephyr_include_directories(include)

  # Link the library with the interface
  zephyr_library_link_libraries(iso14229)

  # Add source directories for IDE indexing
  if(NOT CONFIG_ISO14229_MINIMAL_BUILD)
    target_include_directories(iso14229 INTERFACE
      ${ZEPHYR_CURRENT_MODULE_DIR}/src
    )
    target_include_directories(iso14229 INTERFACE
      ${ZEPHYR_CURRENT_MODULE_DIR}/include
    )
  endif()
endif()
