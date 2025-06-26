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

set(COLLECTIONS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_collections)
set(COLLECTIONS_GEN_H ${COLLECTIONS_GEN_DIR}/collections.h)
set(COLLECTIONS_GEN_CPP ${COLLECTIONS_GEN_DIR}/collections.cpp)

add_custom_command(
    OUTPUT ${COLLECTIONS_GEN_H} ${COLLECTIONS_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COLLECTIONS_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/collections.qp -o ${COLLECTIONS_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/collections.qp
    COMMENT "qpc compile examples/collections.qp"
    VERBATIM
)

add_executable(collections_host
    examples/collections_host/main.cpp
    ${COLLECTIONS_GEN_CPP}
)
target_include_directories(collections_host PRIVATE ${COLLECTIONS_GEN_DIR})

add_test(NAME collections_host COMMAND collections_host)

set(EXTERN_FN_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_extern_fn)
set(EXTERN_FN_GEN_H ${EXTERN_FN_GEN_DIR}/extern_fn.h)
set(EXTERN_FN_GEN_CPP ${EXTERN_FN_GEN_DIR}/extern_fn.cpp)

add_custom_command(
    OUTPUT ${EXTERN_FN_GEN_H} ${EXTERN_FN_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXTERN_FN_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/extern_fn.qp -o ${EXTERN_FN_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/extern_fn.qp
    COMMENT "qpc compile examples/extern_fn.qp"
    VERBATIM
)

add_executable(extern_host
    examples/extern_host/main.cpp
    ${EXTERN_FN_GEN_CPP}
)
target_include_directories(extern_host PRIVATE ${EXTERN_FN_GEN_DIR})

add_test(NAME extern_host COMMAND extern_host)

set(CONTROL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_control)
set(CONTROL_GEN_H ${CONTROL_GEN_DIR}/control.h)
set(CONTROL_GEN_CPP ${CONTROL_GEN_DIR}/control.cpp)

add_custom_command(
    OUTPUT ${CONTROL_GEN_H} ${CONTROL_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CONTROL_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/control.qp -o ${CONTROL_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/control.qp
    COMMENT "qpc compile examples/control.qp"
    VERBATIM
)

add_executable(control_host
    examples/control_host/main.cpp
    ${CONTROL_GEN_CPP}
)
target_include_directories(control_host PRIVATE ${CONTROL_GEN_DIR})

add_test(NAME control_host COMMAND control_host)

set(EXTERN_OBJ_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_extern_obj)
set(EXTERN_OBJ_GEN_H ${EXTERN_OBJ_GEN_DIR}/extern_obj.h)
set(EXTERN_OBJ_GEN_CPP ${EXTERN_OBJ_GEN_DIR}/extern_obj.cpp)

add_custom_command(
    OUTPUT ${EXTERN_OBJ_GEN_H} ${EXTERN_OBJ_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXTERN_OBJ_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/extern_obj.qp -o ${EXTERN_OBJ_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/extern_obj.qp
    COMMENT "qpc compile examples/extern_obj.qp"
    VERBATIM
)

add_executable(extern_obj_host
    examples/extern_obj_host/main.cpp
    ${EXTERN_OBJ_GEN_CPP}
)
target_include_directories(extern_obj_host PRIVATE
    ${EXTERN_OBJ_GEN_DIR}
    ${CMAKE_SOURCE_DIR}/examples/extern_obj_host
)

add_test(NAME extern_obj_host COMMAND extern_obj_host)

set(MODS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_mods)
set(MODS_GEN_H ${MODS_GEN_DIR}/mods.h)
set(MODS_GEN_CPP ${MODS_GEN_DIR}/mods.cpp)

add_custom_command(
    OUTPUT ${MODS_GEN_H} ${MODS_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MODS_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/mods.qp -o ${MODS_GEN_DIR}
    DEPENDS qpc
        ${CMAKE_SOURCE_DIR}/examples/mods.qp
        ${CMAKE_SOURCE_DIR}/examples/math.qp
        ${CMAKE_SOURCE_DIR}/examples/math/vec.qp
        ${CMAKE_SOURCE_DIR}/examples/util/mod.qp
    COMMENT "qpc compile examples/mods.qp"
    VERBATIM
)

add_executable(mods_host
    examples/mods_host/main.cpp
    ${MODS_GEN_CPP}
)
target_include_directories(mods_host PRIVATE ${MODS_GEN_DIR})

add_test(NAME mods_host COMMAND mods_host)

set(CLOSURES_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_closures)
set(CLOSURES_GEN_H ${CLOSURES_GEN_DIR}/closures.h)
set(CLOSURES_GEN_CPP ${CLOSURES_GEN_DIR}/closures.cpp)

add_custom_command(
    OUTPUT ${CLOSURES_GEN_H} ${CLOSURES_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CLOSURES_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/closures.qp -o ${CLOSURES_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/closures.qp
    COMMENT "qpc compile examples/closures.qp"
    VERBATIM
)

add_executable(closures_host
    examples/closures_host/main.cpp
    ${CLOSURES_GEN_CPP}
)
target_include_directories(closures_host PRIVATE ${CLOSURES_GEN_DIR})

add_test(NAME closures_host COMMAND closures_host)

set(EARLY_RETURN_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_early_return)
set(EARLY_RETURN_GEN_H ${EARLY_RETURN_GEN_DIR}/early_return.h)
set(EARLY_RETURN_GEN_CPP ${EARLY_RETURN_GEN_DIR}/early_return.cpp)

add_custom_command(
    OUTPUT ${EARLY_RETURN_GEN_H} ${EARLY_RETURN_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EARLY_RETURN_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/early_return.qp -o ${EARLY_RETURN_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/early_return.qp
    COMMENT "qpc compile examples/early_return.qp"
    VERBATIM
)

add_executable(early_return_host
    examples/early_return_host/main.cpp
    ${EARLY_RETURN_GEN_CPP}
)
target_include_directories(early_return_host PRIVATE ${EARLY_RETURN_GEN_DIR})

add_test(NAME early_return_host COMMAND early_return_host)
