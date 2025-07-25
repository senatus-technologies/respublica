find_program(CLANG_FORMAT clang-format)

if (NOT CLANG_FORMAT)
  message(DEBUG "Configuring project without formatting targets, install 'clang-format' to enable")
else()
  add_custom_target(format.check)
  add_custom_Target(format.fix)
endif()

function(respublica_add_format)
  set(options)
  set(oneValueArgs TARGET)
  set(multiValueArgs EXCLUDE)
  cmake_parse_arguments(RESPUBLICA_ADD_FORMAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if (NOT TARGET ${RESPUBLICA_ADD_FORMAT_TARGET})
    message(FATAL_ERROR "A non-existent or invalid target has been passed to respublica_add_format(): ${RESPUBLICA_ADD_FORMAT_TARGET}")
  endif()

  if (NOT CLANG_FORMAT)
    message(DEBUG "No formatting target created for ${RESPUBLICA_ADD_FORMAT_TARGET}, install 'clang-format' to enable")
  else()
    get_target_property(TARGET_SOURCES ${RESPUBLICA_ADD_FORMAT_TARGET} SOURCES)
    get_target_property(TARGET_SOURCE_DIR ${RESPUBLICA_ADD_FORMAT_TARGET} SOURCE_DIR)
    get_target_property(TARGET_HEADER_SETS ${RESPUBLICA_ADD_FORMAT_TARGET} HEADER_SETS)
    get_target_property(TARGET_INTERFACE_HEADER_SETS ${RESPUBLICA_ADD_FORMAT_TARGET} INTERFACE_HEADER_SETS)

    foreach(HEADER_SET_NAME ${TARGET_HEADER_SETS})
      set(HEADER_SET_PROPERTY "HEADER_SET_${HEADER_SET_NAME}")
      get_target_property(TARGET_HEADERS ${RESPUBLICA_ADD_FORMAT_TARGET} ${HEADER_SET_PROPERTY})
      list(APPEND TARGET_SOURCES ${TARGET_HEADERS})
    endforeach()

    foreach(HEADER_SET_NAME ${TARGET_INTERFACE_HEADER_SETS})
      set(HEADER_SET_PROPERTY "HEADER_SET_${HEADER_SET_NAME}")
      get_target_property(TARGET_HEADERS ${RESPUBLICA_ADD_FORMAT_TARGET} ${HEADER_SET_PROPERTY})
      list(APPEND TARGET_SOURCES ${TARGET_HEADERS})
    endforeach()


    if (NOT TARGET_SOURCES)
      message(FATAL_ERROR "No sources associated with given target: ${RESPUBLICA_ADD_FORMAT_TARGET}")
    endif()

    foreach(SOURCE_FILE ${TARGET_SOURCES})
      if (${SOURCE_FILE} IN_LIST RESPUBLICA_ADD_FORMAT_EXCLUDE)
        continue()
      endif()

      if (IS_ABSOLUTE ${SOURCE_FILE})
        list(APPEND SOURCE_FORMAT_LIST ${SOURCE_FILE})
      else()
        list(APPEND SOURCE_FORMAT_LIST "${TARGET_SOURCE_DIR}/${SOURCE_FILE}")
      endif()
    endforeach()

    add_custom_target(
      ${RESPUBLICA_ADD_FORMAT_TARGET}.format.check
      COMMAND ${CLANG_FORMAT} --style=file:${PROJECT_SOURCE_DIR}/.clang-format --dry-run --Werror ${SOURCE_FORMAT_LIST}
    )
    add_dependencies(format.check ${RESPUBLICA_ADD_FORMAT_TARGET}.format.check)

    add_custom_target(
      ${RESPUBLICA_ADD_FORMAT_TARGET}.format.fix
      COMMAND ${CLANG_FORMAT} --style=file:${PROJECT_SOURCE_DIR}/.clang-format -i ${SOURCE_FORMAT_LIST}
    )
    add_dependencies(format.fix ${RESPUBLICA_ADD_FORMAT_TARGET}.format.fix)
  endif()
endfunction()
