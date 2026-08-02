cmake_minimum_required(VERSION 3.16)

project(
    CodeQualityAnalyzer
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CODEQA_BUILD_TESTS "Build CodeQA tests" ON)
option(CODEQA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

configure_file(
    ${CMAKE_SOURCE_DIR}/include/codeqa/application/Version.h.in
    ${CMAKE_BINARY_DIR}/generated/codeqa/application/Version.h
    @ONLY
)

add_library(codeqa_core STATIC
    src/application/GetApplicationInfo.cpp
)
target_include_directories(codeqa_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
)

enable_testing()

if(CODEQA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
