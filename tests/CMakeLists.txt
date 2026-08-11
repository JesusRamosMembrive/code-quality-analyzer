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
    unit/DetectFileLanguageTest.cpp
    unit/TreeSitterSmokeTest.cpp
    unit/SelectGrammarTest.cpp
    unit/TreeSitterParserTest.cpp
    unit/FrontendPublicApiTest.cpp
    unit/StructuralModelsTest.cpp
    unit/EmbeddedQueriesTest.cpp
    unit/StructuralExtractorTest.cpp
    unit/ParseSourceFileTest.cpp
    unit/CorpusParsingTest.cpp
    unit/CorpusStructureTest.cpp
    unit/ModelTypesTest.cpp
    unit/ModelPublicApiTest.cpp
    unit/ProjectModelBuilderTest.cpp
    unit/AnalyzeProjectTest.cpp
    unit/CorpusModelTest.cpp
    unit/MetricsModelsTest.cpp
    unit/CalculateFunctionMetricsTest.cpp
    unit/MeasureProjectTest.cpp
    unit/CorpusMetricsTest.cpp
    unit/ReadFileContentsTest.cpp
    unit/RuleTest.cpp
    unit/RulesTest.cpp
    unit/ApplyRulesTest.cpp
    unit/CorpusFindingsTest.cpp
    unit/ConfigModelsTest.cpp
    unit/DefaultConfigTest.cpp
    unit/MergeConfigTest.cpp
    unit/ConfigJsonTest.cpp
    unit/LoadProjectConfigTest.cpp
    unit/EnumJsonTest.cpp
    unit/RunAnalysisTest.cpp
)

target_include_directories(codeqa_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party
)

target_compile_definitions(codeqa_tests PRIVATE
    CODEQA_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus"
)

target_link_libraries(codeqa_tests PRIVATE
    codeqa_core tree_sitter tree_sitter_c tree_sitter_cpp nlohmann_json
)

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

add_test(
    NAME codeqa_integration_discover
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=discover$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/integration/fixtures/discover-sample"
        "-DCODEQA_EXPECTED_OUTPUT=Files discovered: 3"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)

add_test(
    NAME codeqa_integration_discover_excludes_build
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=discover$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/integration/fixtures/discover-sample"
        "-DCODEQA_EXPECTED_OUTPUT=Excluded files: 1"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)

add_test(
    NAME codeqa_integration_discover_language
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=discover$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/integration/fixtures/discover-sample"
        "-DCODEQA_EXPECTED_OUTPUT=util.h  Unknown (confidence 0.50) - ambiguous header extension"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)

add_test(
    NAME codeqa_integration_lang_override
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=discover$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/integration/fixtures/discover-sample$<SEMICOLON>--lang-override$<SEMICOLON>util.h=cpp"
        "-DCODEQA_EXPECTED_OUTPUT=util.h  C++ (confidence 1.00) - path override"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)

add_test(
    NAME codeqa_corpus_integrity
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_CORPUS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/corpus
        -P ${CMAKE_CURRENT_SOURCE_DIR}/corpus/CheckCorpusIntegrity.cmake
)

add_test(
    NAME codeqa_integration_parse_corpus_truncated
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=parse$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/corpus"
        "-DCODEQA_EXPECTED_OUTPUT=  line 8: error"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)

add_test(
    NAME codeqa_integration_parse_corpus_unclosed_brace
    COMMAND ${CMAKE_COMMAND}
        -DCODEQA_EXE=$<TARGET_FILE:codeqa>
        "-DCODEQA_ARGS=parse$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/corpus"
        "-DCODEQA_EXPECTED_OUTPUT=  line 13: error"
        -DCODEQA_EXPECTED_EXIT_CODE=0
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckCliOutput.cmake
)
