# - Add tests using boost::test
#
# Add this line to your test files in place of including a basic boost test header:
#  #include <BoostTestTargetConfig.h>
#
# If you cannot do that and must use the included form for a given test,
# include the line
#  // OVERRIDE_BOOST_TEST_INCLUDED_WARNING
# in the same file with the boost test include.
#
#  include(BoostTestTargets)
#  add_boost_test(<testdriver_name> SOURCES <source1> [<more sources...>]
#   [FAIL_REGULAR_EXPRESSION <additional fail regex>]
#   [LAUNCHER <generic launcher script>]
#   [LIBRARIES <library> [<library>...]]
#   [RESOURCES <resource> [<resource>...]]
#   [TESTS <testcasename> [<testcasename>...]])
#
#  If for some reason you need access to the executable target created,
#  it can be found in ${${testdriver_name}_TARGET_NAME} as specified when
#  you called add_boost_test
#
# Requires CMake 2.6 or newer (uses the 'function' command)
#
# Requires:
# 	GetForceIncludeDefinitions
# 	CopyResourcesToBuildTree
#
# Original Author:
# 2009-2010 Ryan Pavlik <rpavlik@iastate.edu> <abiryan@ryand.net>
# http://academic.cleardefinition.com
# Iowa State University HCI Graduate Program/VRAC
#
# Copyright Iowa State University 2009-2010.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

function(koinos_parse_unit_tests RESULT)
   set(SOURCES)
	foreach(_element ${ARGN})
      list(APPEND SOURCES "${_element}")
	endforeach()

   set(tests)

   foreach(src ${SOURCES})
      file(READ ${src} thefile)
      string(REGEX MATCH "BOOST_FIXTURE_TEST_SUITE\\([A-Za-z0-9_,<> ]*\\)" test_suite "${thefile}" )

      if( NOT (test_suite STREQUAL "") )
         string(SUBSTRING "${test_suite}" 25 -1 test_suite)
         string(FIND "${test_suite}" "," comma_loc )
         string(SUBSTRING "${test_suite}" 0 ${comma_loc} test_suite)
         string(STRIP "${test_suite}" test_suite)

         string( REGEX MATCHALL "BOOST_AUTO_TEST_CASE\\([A-Za-z0-9_,<> ]*\\)" cases "${thefile}" )

         foreach( test_case ${cases} )
            string(SUBSTRING "${test_case}" 21 -1 test_case)
            string(FIND "${test_case}" ")" paren_loc )
            string(SUBSTRING "${test_case}" 0 ${paren_loc} test_case)
            string(STRIP "${test_case}" test_case)

            list(APPEND tests "${test_suite}/${test_case}")
         endforeach()
      endif()
   endforeach()

   set(${RESULT} ${tests} PARENT_SCOPE)
endfunction()

function(koinos_add_test _name)
	# parse arguments
	set(_nowhere)
	set(_curdest _nowhere)
	set(_val_args
		SOURCES
		FAIL_REGULAR_EXPRESSION
		LAUNCHER
		LIBRARIES
		RESOURCES
		TESTS)
	set(_bool_args
		USE_COMPILED_LIBRARY)
	foreach(_arg ${_val_args} ${_bool_args})
		set(${_arg})
	endforeach()
	foreach(_element ${ARGN})
		list(FIND _val_args "${_element}" _val_arg_find)
		list(FIND _bool_args "${_element}" _bool_arg_find)
		if("${_val_arg_find}" GREATER "-1")
			set(_curdest "${_element}")
		elseif("${_bool_arg_find}" GREATER "-1")
			set("${_element}" ON)
			set(_curdest _nowhere)
		else()
			list(APPEND ${_curdest} "${_element}")
		endif()
	endforeach()

	if(_nowhere)
		message(FATAL_ERROR "Syntax error in use of add_boost_test!")
	endif()

	if(NOT SOURCES)
		message(FATAL_ERROR
			"Syntax error in use of add_boost_test: at least one source file required!")
	endif()

  set(includeType)
  foreach(src ${SOURCES})
    file(READ ${src} thefile)
    if("${thefile}" MATCHES ".*BoostTestTargetConfig.h.*")
      set(includeType CONFIGURED)
      set(includeFileLoc ${src})
      break()
    elseif("${thefile}" MATCHES ".*boost/test/included/unit_test.hpp.*")
      set(includeType INCLUDED)
      set(includeFileLoc ${src})
      set(_boosttesttargets_libs)	# clear this out - linking would be a bad idea
      break()
    endif()
  endforeach()

  if(NOT _boostTestTargetsNagged${_name} STREQUAL "${includeType}")
    if("${includeType}" STREQUAL "CONFIGURED")
      message(STATUS
        "Test '${_name}' uses the CMake-configurable form of the boost test framework - congrats! (Including File: ${includeFileLoc})")
    elseif("${includeType}" STREQUAL "INCLUDED")
      message("In test '${_name}': ${includeFileLoc} uses the 'included' form of the boost unit test framework.")
    else()
      message("In test '${_name}': Didn't detect the CMake-configurable boost test include.")
      message("Please replace your existing boost test include in that test with the following:")
      message("  \#include <BoostTestTargetConfig.h>")
      message("Once you've saved your changes, re-run CMake. (See BoostTestTargets.cmake for more info)")
    endif()
  endif()
  set(_boostTestTargetsNagged${_name}
    "${includeType}"
    CACHE
    INTERNAL
    ""
    FORCE)


  if(RESOURCES)
    list(APPEND SOURCES ${RESOURCES})
  endif()

  # Generate a unique target name, using the relative binary dir
  # and provided name. (transform all / into _ and remove all other
  # non-alphabet characters)
  file(RELATIVE_PATH
    targetpath
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  string(REGEX REPLACE "[^A-Za-z/_]" "" targetpath "${targetpath}")
  string(REPLACE "/" "_" targetpath "${targetpath}")

  set(_target_name ${_name})
  set(${_name}_TARGET_NAME "${_target_name}" PARENT_SCOPE)

  # Build the test.
  add_executable(${_target_name} ${SOURCES})

  list(APPEND LIBRARIES ${_boosttesttargets_libs})

  if(LIBRARIES)
    target_link_libraries(${_target_name} ${LIBRARIES})
  endif()

  if(RESOURCES)
    set_property(TARGET ${_target_name} PROPERTY RESOURCE ${RESOURCES})
    copy_resources_to_build_tree(${_target_name})
  endif()

  if(NOT Boost_TEST_FLAGS)
#			set(Boost_TEST_FLAGS --catch_system_error=yes --output_format=XML)
    set(Boost_TEST_FLAGS --catch_system_error=yes)
  endif()

  # TODO: Figure out why only recent boost handles individual test running properly

  if(LAUNCHER)
    set(_test_command ${LAUNCHER} "\$<TARGET_FILE:${_target_name}>")
  else()
    set(_test_command ${_target_name})
  endif()

  get_target_property(TARGET_SOURCES ${_name} SOURCES)
  koinos_parse_unit_tests(TESTS ${TARGET_SOURCES})

  if(TESTS)
    foreach(_test ${TESTS})
      add_test(
        ${_name}-${_test}
        ${_test_command} --run_test=${_test} ${Boost_TEST_FLAGS}
      )
      if(FAIL_REGULAR_EXPRESSION)
        set_tests_properties(${_name}-${_test}
          PROPERTIES
          FAIL_REGULAR_EXPRESSION
          "${FAIL_REGULAR_EXPRESSION}")
      endif()
    endforeach()
  else()
    add_test(
      ${_name}-boost_test
      ${_test_command} ${Boost_TEST_FLAGS}
    )
    if(FAIL_REGULAR_EXPRESSION)
      set_tests_properties(${_name}-boost_test
        PROPERTIES
        FAIL_REGULAR_EXPRESSION
        "${FAIL_REGULAR_EXPRESSION}")
    endif()
  endif()

  # CppCheck the test if we can.
  if(COMMAND add_cppcheck)
    add_cppcheck(${_target_name} STYLE UNUSED_FUNCTIONS)
  endif()

endfunction()
