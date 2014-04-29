#
# This file defines a number of data-driven tests on the core EUDAQ library using the
# Python ctypes wrapper. If you have access the reference files you
# can run the tests by running 'make test' in the CMake build directory
#

# =============================================================================
# =============================================================================
# Test 1: Dummy data production through Python scripts w/ output verification
# =============================================================================
# =============================================================================

  FIND_PACKAGE(PythonInterp)


  SET( testdir "${PROJECT_BINARY_DIR}/Testing/tests" )
  SET( datadir "${PROJECT_SOURCE_DIR}/data" )
  SET( testscriptsdir "${PROJECT_SOURCE_DIR}/etc/tests" )

  #SET( referencedatadir "/afs/desy.de/group/telescopes/EudaqTestData" )
  SET( referencedatadir "${PROJECT_SOURCE_DIR}/data" )

  # all this regular expressions must be matched for the tests to pass.
  # the order of the expressions must be matched in the test execution!
  # additional statements can be defined for each test individually
  SET( pass_regex_1 "Successfullly started run!" )
  SET( pass_regex_1 "Successfullly finished run!" )

  SET( generic_fail_regex "ERROR" "CRITICAL" "segmentation violation")


# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#  STEP 1: EXECUTE DUMMY RUN PRODUCING FIXED DATA SET
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


    ADD_TEST( NAME DummyDataProductionRun 
              WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/python"
	      COMMAND ${PYTHON_EXECUTABLE} run_test.py )
    SET_TESTS_PROPERTIES (DummyDataProductionRun PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${pass_regex_1}.*${pass_regex_2}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
    )


    # now check if the expected output files exist and look ok
    ADD_TEST( NAME DummyDataProductionRefComp 
    	      COMMAND ${PYTHON_EXECUTABLE} ${testscriptsdir}/compareRawFiles.py ${datadir} ${referencedatadir}/dummyrefrun.raw
	      )
    SET_TESTS_PROPERTIES (DummyDataProductionRefComp PROPERTIES DEPENDS DummyDataProductionRun)




