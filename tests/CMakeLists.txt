add_executable(codeqa_tests
    main.cpp
    unit/SmokeTest.cpp
    unit/CoreModelsTest.cpp
    unit/GetApplicationInfoTest.cpp
    unit/RunApplicationTest.cpp
    unit/InfrastructureModelsTest.cpp
    unit/GlobMatcherTest.cpp
    unit/DiscoverSourceFilesTest.cpp
    unit/DiscoverProjectSourcesTest.cpp
)

target_include_directories(codeqa_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(codeqa_tests PRIVATE codeqa_core)

add_test(NAME codeqa_tests COMMAND codeqa_tests)

add_test(
    NAME codeqa_integration_version
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        -DCODEQA_ARGS=--version
        "-DCODEQA_EXPECTED_OUTPUT=Code Quality Analyzer ${PROJECT_VERSION}"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)

add_test(
    NAME codeqa_integration_help
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        -DCODEQA_ARGS=--help
        "-DCODEQA_EXPECTED_OUTPUT=Usage:"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)
