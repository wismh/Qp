cmake_minimum_required(VERSION 3.24)
project(qplus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

enable_testing()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/googletest)
add_subdirectory(third_party/spdlog)

add_library(qplus_compiler
    src/compiler/source.cpp
    src/compiler/diagnostic.cpp
    src/compiler/lexer.cpp
    src/compiler/parser.cpp
    src/compiler/lower.cpp
    src/compiler/typeck.cpp
    src/compiler/cpp_backend.cpp
    src/compiler/compile.cpp
)
target_include_directories(qplus_compiler PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(qplus_compiler PUBLIC spdlog::spdlog)

add_executable(qpc src/qpc/main.cpp)
target_link_libraries(qpc PRIVATE qplus_compiler)

include(GoogleTest)

add_executable(qplus_tests
    tests/log_silence.cpp
    tests/lexer_test.cpp
    tests/parser_test.cpp
    tests/typeck_test.cpp
    tests/codegen_test.cpp
    tests/e2e_test.cpp
)
target_link_libraries(qplus_tests PRIVATE qplus_compiler GTest::gtest_main)
target_include_directories(qplus_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_compile_definitions(qplus_tests PRIVATE QPLUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(qplus_tests DISCOVERY_MODE PRE_TEST)
