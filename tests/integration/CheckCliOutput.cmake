# Runs the real codeqa executable and checks stdout and exit code.
# Required variables (passed via -D on the command line):
#   CODEQA_EXE               - path to the codeqa executable
#   CODEQA_ARGS               - single argument to pass to it
#   CODEQA_EXPECTED_OUTPUT    - substring that must appear in stdout
#   CODEQA_EXPECTED_EXIT_CODE - expected process exit code

execute_process(
    COMMAND ${CODEQA_EXE} ${CODEQA_ARGS}
    OUTPUT_VARIABLE actualOutput
    RESULT_VARIABLE actualExitCode
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT actualExitCode EQUAL CODEQA_EXPECTED_EXIT_CODE)
    message(FATAL_ERROR
        "Expected exit code ${CODEQA_EXPECTED_EXIT_CODE} but got ${actualExitCode}. "
        "Output was:\n${actualOutput}")
endif()

string(FIND "${actualOutput}" "${CODEQA_EXPECTED_OUTPUT}" matchPosition)

if(matchPosition EQUAL -1)
    message(FATAL_ERROR
        "Expected output to contain:\n${CODEQA_EXPECTED_OUTPUT}\n"
        "Actual output was:\n${actualOutput}")
endif()

message(STATUS "CLI integration check passed.")
