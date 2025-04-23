set(CURRENT_LIST_DIR ${CMAKE_CURRENT_LIST_DIR})
if (NOT DEFINED GIT_PRE_CONFIGURE_DIR)
  set(GIT_PRE_CONFIGURE_DIR ${PROJECT_SOURCE_DIR}/tools/templates)
endif ()

if (NOT DEFINED GIT_POST_CONFIGURE_DIR)
  set(GIT_POST_CONFIGURE_DIR ${CMAKE_BINARY_DIR}/generated)
endif ()

set(GIT_PRE_CONFIGURE_FILE ${GIT_PRE_CONFIGURE_DIR}/git_version.cpp.in)
set(GIT_POST_CONFIGURE_FILE ${GIT_POST_CONFIGURE_DIR}/git_version.cpp)

function(koinos_git_write git_hash)
  file(WRITE ${CMAKE_BINARY_DIR}/git-state.txt ${git_hash})
endfunction()

function(koinos_git_read git_hash)
  if (EXISTS ${CMAKE_BINARY_DIR}/git-state.txt)
    file(STRINGS ${CMAKE_BINARY_DIR}/git-state.txt CONTENT)
    list(GET CONTENT 0 var)

    set(${git_hash} ${var} PARENT_SCOPE)
    endif ()
endfunction()

function(koinos_check_git_version)
  # Get the latest abbreviated commit hash of the working branch
  execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  koinos_git_read(GIT_HASH_CACHE)
  if (NOT EXISTS ${GIT_POST_CONFIGURE_DIR})
    file(MAKE_DIRECTORY ${GIT_POST_CONFIGURE_DIR})
  endif ()

  if (NOT EXISTS ${GIT_POST_CONFIGURE_DIR}/git_version.h)
    file(COPY ${GIT_PRE_CONFIGURE_DIR}/git_version.h DESTINATION ${GIT_POST_CONFIGURE_DIR})
  endif()

  if (NOT DEFINED GIT_HASH_CACHE)
    set(GIT_HASH_CACHE "INVALID")
  endif ()

  # Only update the git_version.cpp if the hash has changed. This will
  # prevent us from rebuilding the project more than we need to.
  if (NOT ${GIT_HASH} STREQUAL ${GIT_HASH_CACHE} OR NOT EXISTS ${GIT_POST_CONFIGURE_FILE})
    # Set che GIT_HASH_CACHE variable the next build won't have
    # to regenerate the source file.
    koinos_git_write(${GIT_HASH})

    configure_file(${GIT_PRE_CONFIGURE_FILE} ${GIT_POST_CONFIGURE_FILE} @ONLY)
  endif ()

endfunction()

function(koinos_add_git_target)
  add_custom_target(check_git COMMAND ${CMAKE_COMMAND}
    -DRUN_CHECK_GIT_VERSION=1
    -DGIT_PRE_CONFIGURE_DIR=${GIT_PRE_CONFIGURE_DIR}
    -DGIT_POST_CONFIGURE_FILE=${GIT_POST_CONFIGURE_DIR}
    -DGIT_HASH_CACHE=${GIT_HASH_CACHE}
    -P ${CURRENT_LIST_DIR}/Git.cmake
    BYPRODUCTS ${GIT_POST_CONFIGURE_FILE}
  )

  add_library(git_version)

  target_sources(git_version
    PUBLIC
      FILE_SET git_version_headers
      TYPE HEADERS
      BASE_DIRS ${CMAKE_BINARY_DIR}/generated
      FILES
        ${CMAKE_BINARY_DIR}/generated/git_version.h
    PRIVATE
      ${CMAKE_BINARY_DIR}/generated/git_version.cpp)

  add_dependencies(git_version check_git)
  add_library(Koinos::git ALIAS git_version)

  koinos_check_git_version()
endfunction()

# This is used to run this function from an external cmake process.
if (RUN_CHECK_GIT_VERSION)
  koinos_check_git_version()
endif ()
