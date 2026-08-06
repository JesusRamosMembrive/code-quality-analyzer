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
option(CODEQA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

include(cmake/CompilerWarnings.cmake)

configure_file(
    ${PROJECT_SOURCE_DIR}/include/codeqa/application/Version.h.in
    ${PROJECT_BINARY_DIR}/generated/codeqa/application/Version.h
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

add_library(codeqa_core STATIC
    src/application/GetApplicationInfo.cpp
    src/application/DiscoverProjectSources.cpp
    src/application/DetectFileLanguage.cpp
    src/cli/RunApplication.cpp
    src/frontend/SelectGrammar.cpp
    src/frontend/TreeSitterParser.cpp
    src/frontend/ParseSourceFile.cpp
    src/infrastructure/ClassifyExtension.cpp
    src/infrastructure/DefaultExclusions.cpp
    src/infrastructure/DiscoverSourceFiles.cpp
    src/infrastructure/GlobMatcher.cpp
)
target_include_directories(codeqa_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(codeqa_core PUBLIC tree_sitter tree_sitter_c tree_sitter_cpp)
# codeqa_core and codeqa opt into strict warnings; codeqa_tests deliberately does not,
# since the vendored third_party/doctest/doctest.h itself emits warnings (e.g. MSVC C5285)
# that would turn into hard errors under CODEQA_WARNINGS_AS_ERRORS=ON.
codeqa_enable_warnings(codeqa_core)

add_executable(codeqa src/cli/main.cpp)
target_link_libraries(codeqa PRIVATE codeqa_core)
codeqa_enable_warnings(codeqa)

enable_testing()

if(CODEQA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
