find_program(CLANG_FORMAT clang-format)

if (NOT CLANG_FORMAT)
  message(DEBUG "Configuring project without formatting targets, install 'clang-format' to enable")
else()
  add_custom_target(format.check)
  add_custom_Target(format.fix)
endif()

function(koinos_add_format)
  set(options)
  set(oneValueArgs TARGET)
  set(multiValueArgs EXCLUDE)
  cmake_parse_arguments(KOINOS_ADD_FORMAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if (NOT TARGET ${KOINOS_ADD_FORMAT_TARGET})
    message(FATAL_ERROR "A non-existent or invalid target has been passed to koinos_add_format(): ${KOINOS_ADD_FORMAT_TARGET}")
  endif()

  if (NOT CLANG_FORMAT)
    message(DEBUG "No formatting target created for ${KOINOS_ADD_FORMAT_TARGET}, install 'clang-format' to enable")
  else()
    get_target_property(TARGET_SOURCES ${KOINOS_ADD_FORMAT_TARGET} SOURCES)
    get_target_property(TARGET_SOURCE_DIR ${KOINOS_ADD_FORMAT_TARGET} SOURCE_DIR)

    if (NOT TARGET_SOURCES)
      message(FATAL_ERROR "No sources associated with given target: ${KOINOS_ADD_FORMAT_TARGET}")
    endif()

    foreach(SOURCE_FILE ${TARGET_SOURCES})
      if (${SOURCE_FILE} IN_LIST KOINOS_ADD_FORMAT_EXCLUDE)
        continue()
      endif()

      if (IS_ABSOLUTE ${SOURCE_FILE})
        list(APPEND SOURCE_FORMAT_LIST ${SOURCE_FILE})
      else()
        list(APPEND SOURCE_FORMAT_LIST "${TARGET_SOURCE_DIR}/${SOURCE_FILE}")
      endif()
    endforeach()

    add_custom_target(
      ${KOINOS_ADD_FORMAT_TARGET}.format.check
      COMMAND ${CLANG_FORMAT} --style=file:${PROJECT_SOURCE_DIR}/.clang-format --dry-run --Werror ${SOURCE_FORMAT_LIST}
    )
    add_dependencies(format.check ${KOINOS_ADD_FORMAT_TARGET}.format.check)

    add_custom_target(
      ${KOINOS_ADD_FORMAT_TARGET}.format.fix
      COMMAND ${CLANG_FORMAT} --style=file:${PROJECT_SOURCE_DIR}/.clang-format -i ${SOURCE_FORMAT_LIST}
    )
    add_dependencies(format.fix ${KOINOS_ADD_FORMAT_TARGET}.format.fix)
  endif()
endfunction()
