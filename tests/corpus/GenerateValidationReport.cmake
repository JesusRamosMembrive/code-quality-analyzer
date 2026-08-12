# Aggregates one or more validation ledger JSON files (see
# tests/corpus/VALIDATION-METHODOLOGY.md for the schema) into a per-rule
# precision report. Not part of the build or of any ctest run against real
# data -- a maintainer runs this by hand against
# tests/corpus/validation/*.ledger.json (its own synthetic-fixture test is
# in tests/integration/CheckValidationReportOutput.cmake).
#
# Deliberately requires CMake >= 3.19 for string(JSON ...), unlike the rest
# of this project's cmake_minimum_required(VERSION 3.16) floor -- this
# script is never invoked by the configure/build step, only manually or by
# its own test, so the higher floor doesn't affect anyone building codeqa.
#
# Required variable:
#   CODEQA_LEDGER_FILES - semicolon-separated list of ledger JSON file paths

# A standalone `cmake -P` script does not inherit cmake_minimum_required(VERSION 3.16)
# from the root CMakeLists.txt, so without this call CMP0054 defaults to OLD: a quoted
# string like "accepted" in an if() condition gets dereferenced as a variable name if one
# happens to be in scope (and this script later declares a local variable literally named
# `accepted`), silently breaking every STREQUAL comparison against that literal. Setting
# the policy version here also enforces the string(JSON ...) floor from the comment above.
cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED CODEQA_LEDGER_FILES)
    message(FATAL_ERROR "CODEQA_LEDGER_FILES must be defined.")
endif()

# CMake has no map type. Collect every (ruleId, verdict) pair across all
# ledgers as two parallel flat lists first, then aggregate per rule in a
# second pass -- simpler and less error-prone than maintaining running
# counts indexed into parallel lists.
set(allRuleIds "")
set(allVerdicts "")

foreach(ledgerFile IN LISTS CODEQA_LEDGER_FILES)
    if(NOT EXISTS "${ledgerFile}")
        message(FATAL_ERROR "Ledger file '${ledgerFile}' does not exist.")
    endif()

    file(READ "${ledgerFile}" ledgerText)

    string(JSON findingsCount ERROR_VARIABLE lengthError LENGTH "${ledgerText}" "findings")
    if(lengthError)
        message(FATAL_ERROR "Ledger '${ledgerFile}' is not valid JSON or has no 'findings' array: ${lengthError}")
    endif()

    if(findingsCount GREATER 0)
        math(EXPR lastIndex "${findingsCount} - 1")

        foreach(index RANGE ${lastIndex})
            string(JSON ruleId GET "${ledgerText}" "findings" ${index} "ruleId")
            string(JSON verdict GET "${ledgerText}" "findings" ${index} "decision" "verdict")

            if(NOT verdict STREQUAL "accepted" AND NOT verdict STREQUAL "rejected")
                message(FATAL_ERROR
                    "Ledger '${ledgerFile}' finding ${index} has an unknown decision.verdict "
                    "'${verdict}' (expected 'accepted' or 'rejected').")
            endif()

            list(APPEND allRuleIds "${ruleId}")
            list(APPEND allVerdicts "${verdict}")
        endforeach()
    endif()
endforeach()

list(LENGTH allRuleIds totalFindings)

if(totalFindings EQUAL 0)
    message(STATUS "No findings in the given ledger(s).")
    return()
endif()

math(EXPR lastFindingIndex "${totalFindings} - 1")

set(uniqueRuleIds ${allRuleIds})
list(REMOVE_DUPLICATES uniqueRuleIds)
list(SORT uniqueRuleIds)

foreach(ruleId IN LISTS uniqueRuleIds)
    set(accepted 0)
    set(rejected 0)

    foreach(index RANGE ${lastFindingIndex})
        list(GET allRuleIds ${index} currentRuleId)

        if(currentRuleId STREQUAL ruleId)
            list(GET allVerdicts ${index} currentVerdict)

            if(currentVerdict STREQUAL "accepted")
                math(EXPR accepted "${accepted} + 1")
            else()
                math(EXPR rejected "${rejected} + 1")
            endif()
        endif()
    endforeach()

    math(EXPR reviewed "${accepted} + ${rejected}")
    math(EXPR precisionPercent "(${accepted} * 100) / ${reviewed}")

    message(STATUS "Rule: ${ruleId}")
    message(STATUS "Examples reviewed: ${reviewed}")
    message(STATUS "Accepted: ${accepted}")
    message(STATUS "Rejected: ${rejected}")
    message(STATUS "Precision: ${precisionPercent} %")
    message(STATUS "")
endforeach()
