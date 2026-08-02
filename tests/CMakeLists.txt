add_executable(codeqa_tests
    main.cpp
    unit/SmokeTest.cpp
)

target_include_directories(codeqa_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

add_test(NAME codeqa_tests COMMAND codeqa_tests)
