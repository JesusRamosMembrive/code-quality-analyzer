add_executable(codeqa_tests
    main.cpp
    unit/SmokeTest.cpp
    unit/CoreModelsTest.cpp
)

target_include_directories(codeqa_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(codeqa_tests PRIVATE codeqa_core)

add_test(NAME codeqa_tests COMMAND codeqa_tests)
