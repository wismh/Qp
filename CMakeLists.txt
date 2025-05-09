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

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_USE_BUNDLED_GTEST OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/googletest)
add_subdirectory(third_party/benchmark)
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

add_executable(qplus_bench bench/compile_bench.cpp)
target_link_libraries(qplus_bench PRIVATE qplus_compiler benchmark::benchmark)
target_compile_definitions(qplus_bench PRIVATE QPLUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

set(ADD_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(ADD_GEN_H ${ADD_GEN_DIR}/add.h)
set(ADD_GEN_CPP ${ADD_GEN_DIR}/add.cpp)

add_custom_command(
    OUTPUT ${ADD_GEN_H} ${ADD_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ADD_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/add.qp -o ${ADD_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/add.qp
    COMMENT "qpc compile examples/add.qp"
    VERBATIM
)

add_executable(add_host
    examples/host/main.cpp
    ${ADD_GEN_CPP}
)
target_include_directories(add_host PRIVATE ${ADD_GEN_DIR})

add_test(NAME add_host COMMAND add_host)

set(VEC2_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_vec2)
set(VEC2_GEN_H ${VEC2_GEN_DIR}/vec2.h)
set(VEC2_GEN_CPP ${VEC2_GEN_DIR}/vec2.cpp)

add_custom_command(
    OUTPUT ${VEC2_GEN_H} ${VEC2_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${VEC2_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/vec2.qp -o ${VEC2_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/vec2.qp
    COMMENT "qpc compile examples/vec2.qp"
    VERBATIM
)

add_executable(vec2_host
    examples/vec2_host/main.cpp
    ${VEC2_GEN_CPP}
)
target_include_directories(vec2_host PRIVATE ${VEC2_GEN_DIR})

add_test(NAME vec2_host COMMAND vec2_host)

set(SHAPE_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_shape)
set(SHAPE_GEN_H ${SHAPE_GEN_DIR}/shape.h)
set(SHAPE_GEN_CPP ${SHAPE_GEN_DIR}/shape.cpp)

add_custom_command(
    OUTPUT ${SHAPE_GEN_H} ${SHAPE_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHAPE_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/shape.qp -o ${SHAPE_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/shape.qp
    COMMENT "qpc compile examples/shape.qp"
    VERBATIM
)

add_executable(shape_host
    examples/shape_host/main.cpp
    ${SHAPE_GEN_CPP}
)
target_include_directories(shape_host PRIVATE ${SHAPE_GEN_DIR})

add_test(NAME shape_host COMMAND shape_host)
