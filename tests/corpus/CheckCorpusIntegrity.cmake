# Verifies the corpus is internally consistent, without needing the analyzer.
# Run as a CTest test via `cmake -P`. Required variable:
#   CODEQA_CORPUS_DIR - absolute path to tests/corpus
#
# Checks:
#   1. Every fixture source (.c, .cpp, .h, .hpp) has a sibling .parsing.json,
#      and vice versa.
#   2. Every expectation file declares the required top-level fields: functions,
#      classes, includes, and diagnostics.
#   3. Every construct ID cited by a fixture exists in SUPPORTED-CONSTRUCTS.md.
#   4. No fixture cites an OUT-* ID (those are exclusions, never coverage).
#   5. Every documented non-OUT-* ID is covered by at least one fixture.

if(NOT DEFINED CODEQA_CORPUS_DIR)
    message(FATAL_ERROR "CODEQA_CORPUS_DIR must be defined.")
endif()

# --- Collect the documented IDs -------------------------------------------

# The ID regex must accept both three-segment IDs (C-FUNC-01, CPP-PARAM-02) and
# two-segment ones (CTRL-01, OUT-03). A regex requiring three segments would make
# every CTRL-* and OUT-* ID invisible to this checker on BOTH sides — documented
# and cited — leaving the control-flow constructs silently unverified.
set(CODEQA_ID_PATTERN "[A-Z][A-Z-]*-[0-9][0-9]")

file(READ "${CODEQA_CORPUS_DIR}/SUPPORTED-CONSTRUCTS.md" constructsDocument)
string(REGEX MATCHALL "`${CODEQA_ID_PATTERN}`" documentedIdTokens "${constructsDocument}")

set(documentedIds "")
foreach(token IN LISTS documentedIdTokens)
    string(REPLACE "`" "" identifier "${token}")
    list(APPEND documentedIds "${identifier}")
endforeach()
list(REMOVE_DUPLICATES documentedIds)

list(LENGTH documentedIds documentedIdCount)
if(documentedIdCount EQUAL 0)
    message(FATAL_ERROR "No construct IDs found in SUPPORTED-CONSTRUCTS.md.")
endif()

# --- Check 1: fixture sources and expectation files pair up ---------------

file(GLOB_RECURSE fixtureSources
    "${CODEQA_CORPUS_DIR}/*.c"
    "${CODEQA_CORPUS_DIR}/*.cpp"
    "${CODEQA_CORPUS_DIR}/*.h"
    "${CODEQA_CORPUS_DIR}/*.hpp")
file(GLOB_RECURSE expectationFiles "${CODEQA_CORPUS_DIR}/*.parsing.json")

set(failures "")

foreach(source IN LISTS fixtureSources)
    get_filename_component(sourceDirectory "${source}" DIRECTORY)
    get_filename_component(sourceStem "${source}" NAME_WE)
    set(expected "${sourceDirectory}/${sourceStem}.parsing.json")

    if(NOT EXISTS "${expected}")
        list(APPEND failures "Fixture '${source}' has no '${sourceStem}.parsing.json'.")
    endif()
endforeach()

foreach(expectation IN LISTS expectationFiles)
    get_filename_component(expectationDirectory "${expectation}" DIRECTORY)
    get_filename_component(expectationName "${expectation}" NAME)
    string(REPLACE ".parsing.json" "" expectationStem "${expectationName}")

    if(NOT EXISTS "${expectationDirectory}/${expectationStem}.c"
        AND NOT EXISTS "${expectationDirectory}/${expectationStem}.cpp"
        AND NOT EXISTS "${expectationDirectory}/${expectationStem}.h"
        AND NOT EXISTS "${expectationDirectory}/${expectationStem}.hpp")
        list(APPEND failures "Expectation '${expectation}' has no matching .c, .cpp, .h, or .hpp fixture.")
    endif()
endforeach()

# --- Check 2: required top-level fields are present -----------------------

# README.md requires every expectation file to declare functions, classes, includes,
# and diagnostics -- including the explicit "diagnostics": [] on fixtures with no
# expected diagnostics, which is a deliberate assertion, not an omission. A
# simple key-presence regex is all CMake 3.16 offers (no JSON parsing until
# Fase 4); it cannot validate structure or values, only that the key exists.
set(CODEQA_REQUIRED_FIELDS "functions" "classes" "includes" "diagnostics")

foreach(expectation IN LISTS expectationFiles)
    file(READ "${expectation}" expectationText)

    foreach(requiredField IN LISTS CODEQA_REQUIRED_FIELDS)
        string(REGEX MATCH "\"${requiredField}\"[ \t\r\n]*:" fieldMatch "${expectationText}")
        if(NOT fieldMatch)
            list(APPEND failures
                "Expectation '${expectation}' is missing required field '${requiredField}'.")
        endif()
    endforeach()
endforeach()

# --- Check 2b: every parameter that declares "passing" also declares "type" -
# Fase 6a requires an exact "type" (declaredType) per parameter so the C++
# runner can compare it exhaustively; a regex-only presence count is enough
# to catch a fixture where a parameter object was extended with "passing" but
# never got "type" added alongside it.

foreach(expectation IN LISTS expectationFiles)
    file(READ "${expectation}" expectationText)

    string(REGEX MATCHALL "\"passing\"[ \t\r\n]*:" passingMatches "${expectationText}")
    string(REGEX MATCHALL "\"type\"[ \t\r\n]*:" typeMatches "${expectationText}")
    list(LENGTH passingMatches passingCount)
    list(LENGTH typeMatches typeCount)

    if(NOT passingCount EQUAL typeCount)
        list(APPEND failures
            "Expectation '${expectation}' has ${passingCount} parameter(s) with 'passing' but ${typeCount} with 'type'.")
    endif()
endforeach()

# --- Check 2c: every *.metrics.json has a sibling source ------------------
# Unlike *.parsing.json, *.metrics.json is optional per fixture (Fase 6b
# only adds it to complexity/, nesting/ and parameters/) -- so only the
# metrics-to-source direction is checked, never the reverse.

file(GLOB_RECURSE metricsFiles "${CODEQA_CORPUS_DIR}/*.metrics.json")

foreach(metricsFile IN LISTS metricsFiles)
    get_filename_component(metricsDirectory "${metricsFile}" DIRECTORY)
    get_filename_component(metricsName "${metricsFile}" NAME)
    string(REPLACE ".metrics.json" "" metricsStem "${metricsName}")

    if(NOT EXISTS "${metricsDirectory}/${metricsStem}.c"
        AND NOT EXISTS "${metricsDirectory}/${metricsStem}.cpp")
        list(APPEND failures
            "Metrics expectation '${metricsFile}' has no matching .c or .cpp fixture.")
    endif()
endforeach()

# --- Check 2d: every *.findings.json has a sibling source -----------------
# Optional per fixture, same rule as *.metrics.json: only the
# findings-to-source direction is checked, never the reverse.

file(GLOB_RECURSE findingsFiles "${CODEQA_CORPUS_DIR}/*.findings.json")

foreach(findingsFile IN LISTS findingsFiles)
    get_filename_component(findingsDirectory "${findingsFile}" DIRECTORY)
    get_filename_component(findingsName "${findingsFile}" NAME)
    string(REPLACE ".findings.json" "" findingsStem "${findingsName}")

    if(NOT EXISTS "${findingsDirectory}/${findingsStem}.c"
        AND NOT EXISTS "${findingsDirectory}/${findingsStem}.cpp")
        list(APPEND failures
            "Findings expectation '${findingsFile}' has no matching .c or .cpp fixture.")
    endif()
endforeach()

# --- Checks 3 and 4: cited IDs exist, and are never OUT-* ------------------

set(citedIds "")

foreach(expectation IN LISTS expectationFiles)
    file(READ "${expectation}" expectationText)

    # The constructs array is the first [...] block after the "constructs" key.
    string(REGEX MATCH "\"constructs\"[ \t\r\n]*:[ \t\r\n]*\\[([^]]*)\\]"
        constructsMatch "${expectationText}")

    if(NOT constructsMatch)
        list(APPEND failures "Expectation '${expectation}' has no 'constructs' field.")
        continue()
    endif()

    # Extract every QUOTED STRING in the constructs array, not just the substrings
    # that already happen to look like a well-formed ID. Matching CODEQA_ID_PATTERN
    # directly here would silently skip anything that doesn't already have the
    # right shape -- wrong case ("cpp-func-99"), wrong digit count ("CPP-FUNC-1"),
    # or outright garbage ("TOTALLY_BOGUS") -- defeating the very check that is
    # supposed to catch those typos.
    string(REGEX MATCHALL "\"[^\"]*\"" fixtureIdTokens "${CMAKE_MATCH_1}")

    set(fixtureIds "")
    foreach(token IN LISTS fixtureIdTokens)
        string(REGEX REPLACE "^\"|\"$" "" identifier "${token}")
        list(APPEND fixtureIds "${identifier}")
    endforeach()

    if(NOT fixtureIds)
        list(APPEND failures "Expectation '${expectation}' has an empty 'constructs' field.")
        continue()
    endif()

    foreach(identifier IN LISTS fixtureIds)
        if(identifier MATCHES "^OUT-")
            list(APPEND failures
                "Expectation '${expectation}' cites '${identifier}', but OUT-* IDs are exclusions.")
        elseif(NOT identifier IN_LIST documentedIds)
            list(APPEND failures
                "Expectation '${expectation}' cites '${identifier}', which is not in SUPPORTED-CONSTRUCTS.md.")
        endif()

        list(APPEND citedIds "${identifier}")
    endforeach()
endforeach()

list(REMOVE_DUPLICATES citedIds)

# --- Check 5: every documented non-OUT ID is covered by a fixture ---------

foreach(identifier IN LISTS documentedIds)
    if(identifier MATCHES "^OUT-")
        continue()
    endif()

    if(NOT identifier IN_LIST citedIds)
        list(APPEND failures "Construct '${identifier}' is documented but no fixture covers it.")
    endif()
endforeach()

# --- Report ---------------------------------------------------------------

if(failures)
    list(JOIN failures "\n  " failureText)
    message(FATAL_ERROR "Corpus integrity check failed:\n  ${failureText}")
endif()

list(LENGTH fixtureSources fixtureCount)
list(LENGTH citedIds citedIdCount)
message(STATUS
    "Corpus integrity check passed: ${fixtureCount} fixture(s), "
    "${citedIdCount} construct ID(s) cited, ${documentedIdCount} documented "
    "(the documented total includes OUT-* exclusions, which are never cited).")
