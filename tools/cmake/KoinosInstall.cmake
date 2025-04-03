include(CMakePackageConfigHelpers)

function(koinos_install)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs TARGETS)
  cmake_parse_arguments(KOINOS_INSTALL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  foreach(TARGET ${KOINOS_INSTALL_TARGETS})
    if (NOT TARGET ${TARGET})
      message(FATAL_ERROR "A non-existent or invalid target has been passed to koinos_install(): ${TARGET}")
    endif()

    get_target_property(TARGET_TYPE ${TARGET} TYPE)
    if (NOT TARGET_TYPE STREQUAL "EXECUTABLE")
      set_target_properties(${TARGET} PROPERTIES PREFIX "libkoinos_")
    endif()
  endforeach()

  include(GNUInstallDirs)

  export(
    TARGETS ${KOINOS_INSTALL_TARGETS}
    NAMESPACE Koinos::
    FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-targets.cmake)

  install(
    TARGETS ${KOINOS_INSTALL_TARGETS}
    EXPORT ${PROJECT_NAME}-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

  install(
    EXPORT ${PROJECT_NAME}-targets
    NAMESPACE Koinos::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

  configure_package_config_file(
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Templates/project.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

  install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
endfunction()