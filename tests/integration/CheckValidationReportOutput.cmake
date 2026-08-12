# Runs GenerateValidationReport.cmake against a fixed ledger and checks its output for an
# expected substring. Mirrors CheckCliOutput.cmake's pattern for the codeqa binary itself.
# Required variables (passed via -D on the command line):
#   CODEQA_CMAKE            - path to a CMake executable able to run GenerateValidationReport.cmake
#   CODEQA_REPORT_SCRIPT     - path to GenerateValidationReport.cmake
#   CODEQA_LEDGER_FILE       - path to the ledger to run it against
#   CODEQA_EXPECTED_OUTPUT   - substring that must appear in the script's output

execute_process(
    COMMAND ${CODEQA_CMAKE} "-DCODEQA_LEDGER_FILES=${CODEQA_LEDGER_FILE}" -P ${CODEQA_REPORT_SCRIPT}
    OUTPUT_VARIABLE actualOutput
    ERROR_VARIABLE actualError
    RESULT_VARIABLE actualExitCode
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT actualExitCode EQUAL 0)
    message(FATAL_ERROR
        "Expected exit code 0 but got ${actualExitCode}.\nstdout:\n${actualOutput}\nstderr:\n${actualError}")
endif()

# message(STATUS ...) is documented to print to stdout, but this checks both streams --
# cheap insurance against relying on an exact channel for a message() mode.
set(combinedOutput "${actualOutput}${actualError}")
string(FIND "${combinedOutput}" "${CODEQA_EXPECTED_OUTPUT}" matchPosition)

if(matchPosition EQUAL -1)
    message(FATAL_ERROR
        "Expected output to contain:\n${CODEQA_EXPECTED_OUTPUT}\n"
        "Actual stdout was:\n${actualOutput}\nActual stderr was:\n${actualError}")
endif()

message(STATUS "Validation report output check passed.")
