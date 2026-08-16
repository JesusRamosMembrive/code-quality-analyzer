cmake_minimum_required(VERSION 3.16)

project(
    CodeQualityAnalyzer
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CODEQA_BUILD_TESTS "Build CodeQA tests" ON)
option(CODEQA_BUILD_DESKTOP "Build the Qt 6 desktop application" ${WIN32})
option(CODEQA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

include(cmake/CompilerWarnings.cmake)

configure_file(
    ${PROJECT_SOURCE_DIR}/include/codeqa/application/Version.h.in
    ${PROJECT_BINARY_DIR}/generated/codeqa/application/Version.h
    @ONLY
)

set(CODEQA_QUERY_FILES
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/functions.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/types.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/includes.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/declarations.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/functions.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/types.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/includes.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/declarations.scm
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CODEQA_QUERY_FILES})

file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/functions.scm CODEQA_C_FUNCTIONS_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/types.scm CODEQA_C_TYPES_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/includes.scm CODEQA_C_INCLUDES_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/declarations.scm CODEQA_C_DECLARATIONS_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/functions.scm CODEQA_CPP_FUNCTIONS_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/types.scm CODEQA_CPP_TYPES_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/includes.scm CODEQA_CPP_INCLUDES_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/declarations.scm CODEQA_CPP_DECLARATIONS_QUERY)

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/generated/codeqa/frontend)
configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/EmbeddedQueries.h.in
    ${PROJECT_BINARY_DIR}/generated/codeqa/frontend/EmbeddedQueries.h
    @ONLY
)

# Vendored third-party C libraries. Deliberately excluded from
# codeqa_enable_warnings: parser.c files are generated code and would not
# survive -Werror. Same rule already applied to doctest.
#
# The runtime builds from lib.c alone: it #includes every other .c file in
# lib/src, which is tree-sitter's documented single-translation-unit build.
add_library(tree_sitter STATIC
    third_party/tree-sitter/lib/src/lib.c
)
target_include_directories(tree_sitter
    PUBLIC ${PROJECT_SOURCE_DIR}/third_party/tree-sitter/lib/include
    PRIVATE ${PROJECT_SOURCE_DIR}/third_party/tree-sitter/lib/src
)

add_library(tree_sitter_c STATIC
    third_party/tree-sitter-c/src/parser.c
)
target_include_directories(tree_sitter_c PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/tree-sitter-c/src
)

add_library(tree_sitter_cpp STATIC
    third_party/tree-sitter-cpp/src/parser.c
    third_party/tree-sitter-cpp/src/scanner.c
)
target_include_directories(tree_sitter_cpp PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/tree-sitter-cpp/src
)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/third_party
)

add_library(codeqa_core STATIC
    src/application/GetApplicationInfo.cpp
    src/application/DiscoverProjectSources.cpp
    src/application/DetectFileLanguage.cpp
    src/application/AnalyzeProject.cpp
    src/application/MeasureProject.cpp
    src/application/ApplyRules.cpp
    src/application/LoadProjectConfig.cpp
    src/application/ReportDocumentFactory.cpp
    src/application/ApplicationFacade.cpp
    src/application/SaveProjectConfig.cpp
    src/application/DeleteSnapshot.cpp
    src/application/SnapshotPath.cpp
    src/cli/RunApplication.cpp
    src/frontend/SelectGrammar.cpp
    src/frontend/TreeSitterParser.cpp
    src/frontend/StructuralExtractor.cpp
    src/frontend/ParseSourceFile.cpp
    src/infrastructure/ClassifyExtension.cpp
    src/infrastructure/DefaultExclusions.cpp
    src/infrastructure/DiscoverSourceFiles.cpp
    src/infrastructure/GlobMatcher.cpp
    src/infrastructure/ReadFileContents.cpp
    src/model/ProjectModelBuilder.cpp
    src/metrics/CalculateFunctionMetrics.cpp
    src/rules/Rules.cpp
    src/config/DefaultConfig.cpp
    src/config/MergeConfig.cpp
    src/config/ConfigJson.cpp
    src/core/EnumJson.cpp
    src/application/RunAnalysis.cpp
    src/application/ReportDocumentAnalysis.cpp
    src/report/TextReport.cpp
    src/report/MarkdownReport.cpp
    src/report/FileJson.cpp
    src/report/ReportJson.cpp
    src/history/CompareSnapshots.cpp
    src/application/SaveSnapshot.cpp
    src/application/ListSnapshots.cpp
    src/application/LoadSnapshot.cpp
)
target_include_directories(codeqa_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(codeqa_core PUBLIC tree_sitter tree_sitter_c tree_sitter_cpp nlohmann_json)
# codeqa_core and codeqa opt into strict warnings; codeqa_tests deliberately does not,
# since the vendored third_party/doctest/doctest.h itself emits warnings (e.g. MSVC C5285)
# that would turn into hard errors under CODEQA_WARNINGS_AS_ERRORS=ON.
codeqa_enable_warnings(codeqa_core)

add_executable(codeqa src/cli/main.cpp)
target_link_libraries(codeqa PRIVATE codeqa_core)
codeqa_enable_warnings(codeqa)

if(WIN32 AND CODEQA_BUILD_DESKTOP)
    find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
        QuickControls2
        QuickDialogs2
        QuickEffects
        Concurrent
        Test
        QuickTest
    )
    qt_standard_project_setup()
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)

    add_subdirectory(src/desktop)
endif()

enable_testing()

if(CODEQA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
