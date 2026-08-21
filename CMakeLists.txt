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

set(tomlplusplus_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/googletest)
add_subdirectory(third_party/benchmark)
add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/tomlplusplus)

add_library(qplus_compiler
    src/compiler/source.cpp
    src/compiler/diagnostic.cpp
    src/compiler/lexer/lexer.cpp
    src/compiler/parser/parser.cpp
    src/compiler/parser/parser_core.cpp
    src/compiler/parser/parse_item.cpp
    src/compiler/parser/parse_stmt.cpp
    src/compiler/parser/parse_expr.cpp
    src/compiler/lower/lower.cpp
    src/compiler/typeck/typeck.cpp
    src/compiler/typeck/type_checker.cpp
    src/compiler/typeck/collect.cpp
    src/compiler/typeck/check_stmt.cpp
    src/compiler/typeck/check_expr.cpp
    src/compiler/codegen/cpp_backend.cpp
    src/compiler/packages/packages.cpp
    src/compiler/compile/compile.cpp
)
target_include_directories(qplus_compiler PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(qplus_compiler PUBLIC spdlog::spdlog tomlplusplus::tomlplusplus)

add_executable(qpc src/qpc/main.cpp)
target_link_libraries(qpc PRIVATE qplus_compiler)

include(GoogleTest)

add_executable(qplus_tests
    tests/log_silence.cpp
    tests/lexer_test.cpp
    tests/parser_test.cpp
    tests/typeck_test.cpp
    tests/codegen_test.cpp
    tests/packages_test.cpp
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

set(REF_CAPTURE_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_ref_capture)
set(REF_CAPTURE_GEN_H ${REF_CAPTURE_GEN_DIR}/ref_capture.h)
set(REF_CAPTURE_GEN_CPP ${REF_CAPTURE_GEN_DIR}/ref_capture.cpp)

add_custom_command(
    OUTPUT ${REF_CAPTURE_GEN_H} ${REF_CAPTURE_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${REF_CAPTURE_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/ref_capture.qp -o ${REF_CAPTURE_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/ref_capture.qp
    COMMENT "qpc compile examples/ref_capture.qp"
    VERBATIM
)

add_executable(ref_capture_host
    examples/ref_capture_host/main.cpp
    ${REF_CAPTURE_GEN_CPP}
)
target_include_directories(ref_capture_host PRIVATE ${REF_CAPTURE_GEN_DIR})

add_test(NAME ref_capture_host COMMAND ref_capture_host)

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

set(CASTS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_casts)
set(CASTS_GEN_H ${CASTS_GEN_DIR}/casts.h)
set(CASTS_GEN_CPP ${CASTS_GEN_DIR}/casts.cpp)

add_custom_command(
    OUTPUT ${CASTS_GEN_H} ${CASTS_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CASTS_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/casts.qp -o ${CASTS_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/casts.qp
    COMMENT "qpc compile examples/casts.qp"
    VERBATIM
)

add_executable(casts_host
    examples/casts_host/main.cpp
    ${CASTS_GEN_CPP}
)
target_include_directories(casts_host PRIVATE ${CASTS_GEN_DIR})

add_test(NAME casts_host COMMAND casts_host)

set(NULLABLE_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_nullable)
set(NULLABLE_GEN_H ${NULLABLE_GEN_DIR}/nullable.h)
set(NULLABLE_GEN_CPP ${NULLABLE_GEN_DIR}/nullable.cpp)

add_custom_command(
    OUTPUT ${NULLABLE_GEN_H} ${NULLABLE_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NULLABLE_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/nullable.qp -o ${NULLABLE_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/nullable.qp
    COMMENT "qpc compile examples/nullable.qp"
    VERBATIM
)

add_executable(nullable_host
    examples/nullable_host/main.cpp
    ${NULLABLE_GEN_CPP}
)
target_include_directories(nullable_host PRIVATE ${NULLABLE_GEN_DIR})

add_test(NAME nullable_host COMMAND nullable_host)

set(HEAP_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_heap)
set(HEAP_GEN_H ${HEAP_GEN_DIR}/heap.h)
set(HEAP_GEN_CPP ${HEAP_GEN_DIR}/heap.cpp)

add_custom_command(
    OUTPUT ${HEAP_GEN_H} ${HEAP_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HEAP_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/heap.qp -o ${HEAP_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/heap.qp
    COMMENT "qpc compile examples/heap.qp"
    VERBATIM
)

add_executable(heap_host
    examples/heap_host/main.cpp
    ${HEAP_GEN_CPP}
)
target_include_directories(heap_host PRIVATE ${HEAP_GEN_DIR})

add_test(NAME heap_host COMMAND heap_host)

set(IF_LET_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_if_let)
set(IF_LET_GEN_H ${IF_LET_GEN_DIR}/if_let.h)
set(IF_LET_GEN_CPP ${IF_LET_GEN_DIR}/if_let.cpp)

add_custom_command(
    OUTPUT ${IF_LET_GEN_H} ${IF_LET_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${IF_LET_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/if_let.qp -o ${IF_LET_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/if_let.qp
    COMMENT "qpc compile examples/if_let.qp"
    VERBATIM
)

add_executable(if_let_host
    examples/if_let_host/main.cpp
    ${IF_LET_GEN_CPP}
)
target_include_directories(if_let_host PRIVATE ${IF_LET_GEN_DIR})

add_test(NAME if_let_host COMMAND if_let_host)

set(NULL_OPS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_null_ops)
set(NULL_OPS_GEN_H ${NULL_OPS_GEN_DIR}/null_ops.h)
set(NULL_OPS_GEN_CPP ${NULL_OPS_GEN_DIR}/null_ops.cpp)

add_custom_command(
    OUTPUT ${NULL_OPS_GEN_H} ${NULL_OPS_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NULL_OPS_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/null_ops.qp -o ${NULL_OPS_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/null_ops.qp
    COMMENT "qpc compile examples/null_ops.qp"
    VERBATIM
)

add_executable(null_ops_host
    examples/null_ops_host/main.cpp
    ${NULL_OPS_GEN_CPP}
)
target_include_directories(null_ops_host PRIVATE ${NULL_OPS_GEN_DIR})

add_test(NAME null_ops_host COMMAND null_ops_host)

set(TRY_NULL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_try_null)
set(TRY_NULL_GEN_H ${TRY_NULL_GEN_DIR}/try_null.h)
set(TRY_NULL_GEN_CPP ${TRY_NULL_GEN_DIR}/try_null.cpp)

add_custom_command(
    OUTPUT ${TRY_NULL_GEN_H} ${TRY_NULL_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TRY_NULL_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/try_null.qp -o ${TRY_NULL_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/try_null.qp
    COMMENT "qpc compile examples/try_null.qp"
    VERBATIM
)

add_executable(try_null_host
    examples/try_null_host/main.cpp
    ${TRY_NULL_GEN_CPP}
)
target_include_directories(try_null_host PRIVATE ${TRY_NULL_GEN_DIR})

add_test(NAME try_null_host COMMAND try_null_host)

set(PAIR_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_pair)
set(PAIR_GEN_H ${PAIR_GEN_DIR}/pair.h)
set(PAIR_GEN_CPP ${PAIR_GEN_DIR}/pair.cpp)

add_custom_command(
    OUTPUT ${PAIR_GEN_H} ${PAIR_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PAIR_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/pair.qp -o ${PAIR_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/pair.qp
    COMMENT "qpc compile examples/pair.qp"
    VERBATIM
)

add_executable(pair_host
    examples/pair_host/main.cpp
    ${PAIR_GEN_CPP}
)
target_include_directories(pair_host PRIVATE ${PAIR_GEN_DIR})

add_test(NAME pair_host COMMAND pair_host)

set(DYN_AREA_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_dyn_area)
set(DYN_AREA_GEN_H ${DYN_AREA_GEN_DIR}/dyn_area.h)
set(DYN_AREA_GEN_CPP ${DYN_AREA_GEN_DIR}/dyn_area.cpp)

add_custom_command(
    OUTPUT ${DYN_AREA_GEN_H} ${DYN_AREA_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DYN_AREA_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/dyn_area.qp -o ${DYN_AREA_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/dyn_area.qp
    COMMENT "qpc compile examples/dyn_area.qp"
    VERBATIM
)

add_executable(dyn_area_host
    examples/dyn_area_host/main.cpp
    ${DYN_AREA_GEN_CPP}
)
target_include_directories(dyn_area_host PRIVATE ${DYN_AREA_GEN_DIR})

add_test(NAME dyn_area_host COMMAND dyn_area_host)

set(MATH_FNS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_math_fns)
set(MATH_FNS_GEN_H ${MATH_FNS_GEN_DIR}/math_fns.h)
set(MATH_FNS_GEN_CPP ${MATH_FNS_GEN_DIR}/math_fns.cpp)

add_custom_command(
    OUTPUT ${MATH_FNS_GEN_H} ${MATH_FNS_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MATH_FNS_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/math_fns.qp -o ${MATH_FNS_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/math_fns.qp
    COMMENT "qpc compile examples/math_fns.qp"
    VERBATIM
)

add_executable(math_fns_host
    examples/math_fns_host/main.cpp
    ${MATH_FNS_GEN_CPP}
)
target_include_directories(math_fns_host PRIVATE ${MATH_FNS_GEN_DIR})

add_test(NAME math_fns_host COMMAND math_fns_host)

set(EACH_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_each)
set(EACH_GEN_H ${EACH_GEN_DIR}/each.h)
set(EACH_GEN_CPP ${EACH_GEN_DIR}/each.cpp)

add_custom_command(
    OUTPUT ${EACH_GEN_H} ${EACH_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EACH_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/each.qp -o ${EACH_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/each.qp
    COMMENT "qpc compile examples/each.qp"
    VERBATIM
)

add_executable(each_host
    examples/each_host/main.cpp
    ${EACH_GEN_CPP}
)
target_include_directories(each_host PRIVATE ${EACH_GEN_DIR})

add_test(NAME each_host COMMAND each_host)

set(DICT_FOR_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_dict_for)
set(DICT_FOR_GEN_H ${DICT_FOR_GEN_DIR}/dict_for.h)
set(DICT_FOR_GEN_CPP ${DICT_FOR_GEN_DIR}/dict_for.cpp)

add_custom_command(
    OUTPUT ${DICT_FOR_GEN_H} ${DICT_FOR_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DICT_FOR_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/dict_for.qp -o ${DICT_FOR_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/dict_for.qp
    COMMENT "qpc compile examples/dict_for.qp"
    VERBATIM
)

add_executable(dict_for_host
    examples/dict_for_host/main.cpp
    ${DICT_FOR_GEN_CPP}
)
target_include_directories(dict_for_host PRIVATE ${DICT_FOR_GEN_DIR})

add_test(NAME dict_for_host COMMAND dict_for_host)

set(MOD_TYPES_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_mod_types)
set(MOD_TYPES_GEN_H ${MOD_TYPES_GEN_DIR}/mod_types.h)
set(MOD_TYPES_GEN_CPP ${MOD_TYPES_GEN_DIR}/mod_types.cpp)

add_custom_command(
    OUTPUT ${MOD_TYPES_GEN_H} ${MOD_TYPES_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MOD_TYPES_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/mod_types.qp -o ${MOD_TYPES_GEN_DIR}
    DEPENDS qpc
        ${CMAKE_SOURCE_DIR}/examples/mod_types.qp
        ${CMAKE_SOURCE_DIR}/examples/ecs.qp
    COMMENT "qpc compile examples/mod_types.qp"
    VERBATIM
)

add_executable(mod_types_host
    examples/mod_types_host/main.cpp
    ${MOD_TYPES_GEN_CPP}
)
target_include_directories(mod_types_host PRIVATE ${MOD_TYPES_GEN_DIR})

add_test(NAME mod_types_host COMMAND mod_types_host)

set(USE_STATICS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_use_statics)
set(USE_STATICS_GEN_H ${USE_STATICS_GEN_DIR}/use_statics.h)
set(USE_STATICS_GEN_CPP ${USE_STATICS_GEN_DIR}/use_statics.cpp)

add_custom_command(
    OUTPUT ${USE_STATICS_GEN_H} ${USE_STATICS_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${USE_STATICS_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/use_statics.qp -o ${USE_STATICS_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/use_statics.qp
    COMMENT "qpc compile examples/use_statics.qp"
    VERBATIM
)

add_executable(use_statics_host
    examples/use_statics_host/main.cpp
    ${USE_STATICS_GEN_CPP}
)
target_include_directories(use_statics_host PRIVATE ${USE_STATICS_GEN_DIR})

add_test(NAME use_statics_host COMMAND use_statics_host)

set(NESTED_ENUM_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_nested_enum)
set(NESTED_ENUM_GEN_H ${NESTED_ENUM_GEN_DIR}/nested_enum.h)
set(NESTED_ENUM_GEN_CPP ${NESTED_ENUM_GEN_DIR}/nested_enum.cpp)

add_custom_command(
    OUTPUT ${NESTED_ENUM_GEN_H} ${NESTED_ENUM_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NESTED_ENUM_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/nested_enum.qp -o ${NESTED_ENUM_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/nested_enum.qp
    COMMENT "qpc compile examples/nested_enum.qp"
    VERBATIM
)

add_executable(nested_enum_host
    examples/nested_enum_host/main.cpp
    ${NESTED_ENUM_GEN_CPP}
)
target_include_directories(nested_enum_host PRIVATE ${NESTED_ENUM_GEN_DIR})

add_test(NAME nested_enum_host COMMAND nested_enum_host)

set(ENGINE_API_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_engine_api)
set(ENGINE_API_GEN_H ${ENGINE_API_GEN_DIR}/engine_api.h)
set(ENGINE_API_GEN_CPP ${ENGINE_API_GEN_DIR}/engine_api.cpp)

add_custom_command(
    OUTPUT ${ENGINE_API_GEN_H} ${ENGINE_API_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ENGINE_API_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/engine_api.qp -o ${ENGINE_API_GEN_DIR}
    DEPENDS qpc
        ${CMAKE_SOURCE_DIR}/examples/engine_api.qp
        ${CMAKE_SOURCE_DIR}/examples/engine.qp
    COMMENT "qpc compile examples/engine_api.qp"
    VERBATIM
)

add_executable(engine_api_host
    examples/engine_api_host/main.cpp
    ${ENGINE_API_GEN_CPP}
)
target_include_directories(engine_api_host PRIVATE
    ${ENGINE_API_GEN_DIR}
    ${CMAKE_SOURCE_DIR}/examples/engine_api_host
)

add_test(NAME engine_api_host COMMAND engine_api_host)

set(TO_STRING_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_to_string)
set(TO_STRING_GEN_H ${TO_STRING_GEN_DIR}/to_string.h)
set(TO_STRING_GEN_CPP ${TO_STRING_GEN_DIR}/to_string.cpp)

add_custom_command(
    OUTPUT ${TO_STRING_GEN_H} ${TO_STRING_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TO_STRING_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/to_string.qp -o ${TO_STRING_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/to_string.qp
    COMMENT "qpc compile examples/to_string.qp"
    VERBATIM
)

add_executable(to_string_host
    examples/to_string_host/main.cpp
    ${TO_STRING_GEN_CPP}
)
target_include_directories(to_string_host PRIVATE ${TO_STRING_GEN_DIR})

add_test(NAME to_string_host COMMAND to_string_host)

set(INTERP_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_interp)
set(INTERP_GEN_H ${INTERP_GEN_DIR}/interp.h)
set(INTERP_GEN_CPP ${INTERP_GEN_DIR}/interp.cpp)

add_custom_command(
    OUTPUT ${INTERP_GEN_H} ${INTERP_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INTERP_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/interp.qp -o ${INTERP_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/interp.qp
    COMMENT "qpc compile examples/interp.qp"
    VERBATIM
)

add_executable(interp_host
    examples/interp_host/main.cpp
    ${INTERP_GEN_CPP}
)
target_include_directories(interp_host PRIVATE ${INTERP_GEN_DIR})

add_test(NAME interp_host COMMAND interp_host)

set(COERCE_NULL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_coerce_null)
set(COERCE_NULL_GEN_H ${COERCE_NULL_GEN_DIR}/coerce_null.h)
set(COERCE_NULL_GEN_CPP ${COERCE_NULL_GEN_DIR}/coerce_null.cpp)

add_custom_command(
    OUTPUT ${COERCE_NULL_GEN_H} ${COERCE_NULL_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COERCE_NULL_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/coerce_null.qp -o ${COERCE_NULL_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/coerce_null.qp
    COMMENT "qpc compile examples/coerce_null.qp"
    VERBATIM
)

add_executable(coerce_null_host
    examples/coerce_null_host/main.cpp
    ${COERCE_NULL_GEN_CPP}
)
target_include_directories(coerce_null_host PRIVATE ${COERCE_NULL_GEN_DIR})

add_test(NAME coerce_null_host COMMAND coerce_null_host)

set(PACKAGES_APP_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_packages_app)
set(PACKAGES_APP_GEN_H ${PACKAGES_APP_GEN_DIR}/app.h)
set(PACKAGES_APP_GEN_CPP ${PACKAGES_APP_GEN_DIR}/app.cpp)

add_custom_command(
    OUTPUT ${PACKAGES_APP_GEN_H} ${PACKAGES_APP_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PACKAGES_APP_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/packages_app/app.qp -o ${PACKAGES_APP_GEN_DIR}
    DEPENDS qpc
        ${CMAKE_SOURCE_DIR}/examples/packages_app/app.qp
        ${CMAKE_SOURCE_DIR}/examples/packages_app/packages.toml
        ${CMAKE_SOURCE_DIR}/examples/packages_lib/math/mod.qp
    COMMENT "qpc compile examples/packages_app/app.qp"
    VERBATIM
)

add_executable(packages_app_host
    examples/packages_app_host/main.cpp
    ${PACKAGES_APP_GEN_CPP}
)
target_include_directories(packages_app_host PRIVATE ${PACKAGES_APP_GEN_DIR})

add_test(NAME packages_app_host COMMAND packages_app_host)

set(OVERLOAD_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_overload)
set(OVERLOAD_GEN_H ${OVERLOAD_GEN_DIR}/overload.h)
set(OVERLOAD_GEN_CPP ${OVERLOAD_GEN_DIR}/overload.cpp)

add_custom_command(
    OUTPUT ${OVERLOAD_GEN_H} ${OVERLOAD_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OVERLOAD_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/overload.qp -o ${OVERLOAD_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/overload.qp
    COMMENT "qpc compile examples/overload.qp"
    VERBATIM
)

add_executable(overload_host
    examples/overload_host/main.cpp
    ${OVERLOAD_GEN_CPP}
)
target_include_directories(overload_host PRIVATE ${OVERLOAD_GEN_DIR})

add_test(NAME overload_host COMMAND overload_host)

set(TUPLE_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_tuple)
set(TUPLE_GEN_H ${TUPLE_GEN_DIR}/tuple.h)
set(TUPLE_GEN_CPP ${TUPLE_GEN_DIR}/tuple.cpp)

add_custom_command(
    OUTPUT ${TUPLE_GEN_H} ${TUPLE_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TUPLE_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/tuple.qp -o ${TUPLE_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/tuple.qp
    COMMENT "qpc compile examples/tuple.qp"
    VERBATIM
)

add_executable(tuple_host
    examples/tuple_host/main.cpp
    ${TUPLE_GEN_CPP}
)
target_include_directories(tuple_host PRIVATE ${TUPLE_GEN_DIR})

add_test(NAME tuple_host COMMAND tuple_host)

set(QUERY_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_query)
set(QUERY_GEN_H ${QUERY_GEN_DIR}/query.h)
set(QUERY_GEN_CPP ${QUERY_GEN_DIR}/query.cpp)

add_custom_command(
    OUTPUT ${QUERY_GEN_H} ${QUERY_GEN_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${QUERY_GEN_DIR}
    COMMAND $<TARGET_FILE:qpc> compile ${CMAKE_SOURCE_DIR}/examples/query.qp -o ${QUERY_GEN_DIR}
    DEPENDS qpc ${CMAKE_SOURCE_DIR}/examples/query.qp
    COMMENT "qpc compile examples/query.qp"
    VERBATIM
)

add_executable(query_host
    examples/query_host/main.cpp
    ${QUERY_GEN_CPP}
)
target_include_directories(query_host PRIVATE ${QUERY_GEN_DIR})

add_test(NAME query_host COMMAND query_host)
