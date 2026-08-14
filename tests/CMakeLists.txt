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
    unit/CorpusReportTest.cpp
    unit/ConfigModelsTest.cpp
    unit/DefaultConfigTest.cpp
    unit/MergeConfigTest.cpp
    unit/ConfigJsonTest.cpp
    unit/LoadProjectConfigTest.cpp
    unit/EnumJsonTest.cpp
    unit/RunAnalysisTest.cpp
    unit/TextReportTest.cpp
    unit/FileJsonTest.cpp
    unit/ReportJsonTest.cpp
    unit/ReportDocumentTest.cpp
    unit/ApplicationFacadeTest.cpp
    unit/SaveProjectConfigTest.cpp
    unit/DeleteSnapshotTest.cpp
    unit/HistoryTest.cpp
    unit/SaveSnapshotTest.cpp
    unit/ListSnapshotsTest.cpp
    unit/LoadSnapshotTest.cpp
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

add_test(
    NAME codeqa_validation_report_sample_rule
    COMMAND ${CMAKE_COMMAND}
        "-DCODEQA_CMAKE=${CMAKE_COMMAND}"
        "-DCODEQA_REPORT_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/corpus/GenerateValidationReport.cmake"
        "-DCODEQA_LEDGER_FILE=${CMAKE_CURRENT_SOURCE_DIR}/corpus/validation/fixtures/sample.ledger.json"
        "-DCODEQA_EXPECTED_OUTPUT=Rule: complexity.cyclomatic"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckValidationReportOutput.cmake
)

add_test(
    NAME codeqa_validation_report_sample_precision
    COMMAND ${CMAKE_COMMAND}
        "-DCODEQA_CMAKE=${CMAKE_COMMAND}"
        "-DCODEQA_REPORT_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/corpus/GenerateValidationReport.cmake"
        "-DCODEQA_LEDGER_FILE=${CMAKE_CURRENT_SOURCE_DIR}/corpus/validation/fixtures/sample.ledger.json"
        "-DCODEQA_EXPECTED_OUTPUT=Precision: 75 %"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckValidationReportOutput.cmake
)

add_test(
    NAME codeqa_validation_report_combined_ledgers
    COMMAND ${CMAKE_COMMAND}
        "-DCODEQA_CMAKE=${CMAKE_COMMAND}"
        "-DCODEQA_REPORT_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/corpus/GenerateValidationReport.cmake"
        "-DCODEQA_LEDGER_FILE=${CMAKE_CURRENT_SOURCE_DIR}/corpus/validation/fixtures/sample.ledger.json$<SEMICOLON>${CMAKE_CURRENT_SOURCE_DIR}/corpus/validation/fixtures/sample2.ledger.json"
        "-DCODEQA_EXPECTED_OUTPUT=Examples reviewed: 8"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/integration/CheckValidationReportOutput.cmake
)

if(WIN32 AND CODEQA_BUILD_DESKTOP)
    set(CODEQA_DESKTOP_QML_TEST_MAIN
        ${CMAKE_CURRENT_BINARY_DIR}/DesktopShellQuickTestMain.cpp
    )
    file(GENERATE OUTPUT ${CODEQA_DESKTOP_QML_TEST_MAIN} CONTENT
        "#include <QtQuickTest/quicktest.h>\n\nQUICK_TEST_MAIN(DesktopShell)\n"
    )

    qt_add_executable(codeqa_desktop_qml_smoke ${CODEQA_DESKTOP_QML_TEST_MAIN})
    target_compile_definitions(codeqa_desktop_qml_smoke PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/desktop/qml"
    )
    target_link_libraries(codeqa_desktop_qml_smoke PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickTest
    )
    set(CODEQA_DESKTOP_QML_TEST_MAIN_QML
        ${CMAKE_CURRENT_BINARY_DIR}/desktop-qml/Main.qml
    )
    configure_file(
        ${PROJECT_SOURCE_DIR}/src/desktop/qml/Main.qml
        ${CODEQA_DESKTOP_QML_TEST_MAIN_QML}
        COPYONLY
    )
    file(GENERATE
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/desktop-qml/qmldir
        CONTENT "module CodeQA.Desktop\nMain 1.0 Main.qml\n"
    )
    qt_add_resources(codeqa_desktop_qml_smoke desktop_shell_resources
        PREFIX "/qt/qml/CodeQA/Desktop"
        BASE ${CMAKE_CURRENT_BINARY_DIR}/desktop-qml
        FILES
            ${CODEQA_DESKTOP_QML_TEST_MAIN_QML}
            ${CMAKE_CURRENT_BINARY_DIR}/desktop-qml/qmldir
    )

    add_test(
        NAME codeqa_desktop_qml_smoke
        COMMAND codeqa_desktop_qml_smoke
    )
    set_tests_properties(codeqa_desktop_qml_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/bin"
    )
endif()
