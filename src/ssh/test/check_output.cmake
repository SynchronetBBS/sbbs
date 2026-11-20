# Strict output tests require both a successful process and the expected text.
# PASS_REGULAR_EXPRESSION alone causes CTest to ignore nonzero process exits.
if(NOT DEFINED TEST_COMMAND OR "${TEST_COMMAND}" STREQUAL ""
    OR NOT DEFINED TEST_REQUIRED_OUTPUT OR "${TEST_REQUIRED_OUTPUT}" STREQUAL "")
	message(FATAL_ERROR "A command and required output pattern are mandatory")
endif()

execute_process(COMMAND ${TEST_COMMAND}
	RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
message("${output}${errors}")
if(NOT "${result}" STREQUAL "0")
	message(FATAL_ERROR "Test process did not succeed: ${result}")
endif()
if("${output}${errors}" MATCHES "AddressSanitizer|LeakSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer|runtime error:")
	message(FATAL_ERROR "Test process emitted a sanitizer diagnostic")
endif()
if(NOT "${output}${errors}" MATCHES "${TEST_REQUIRED_OUTPUT}")
	message(FATAL_ERROR "Test process did not emit its required output")
endif()
