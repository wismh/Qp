#include "compiler/compile.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>

#ifndef QPLUS_SOURCE_DIR
#define QPLUS_SOURCE_DIR "."
#endif

TEST(E2E, CompileAddExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_add";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "add.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "add.h";
    const auto source = out_dir / "add.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t add(std::int32_t a, std::int32_t b);"), std::string::npos);
    EXPECT_NE(header_text.find("namespace qplus"), std::string::npos);

    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("#include \"add.h\""), std::string::npos);
    EXPECT_NE(source_text.find("a + b"), std::string::npos);
}

TEST(E2E, CompileVec2ExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_vec2";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "vec2.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "vec2.h";
    const auto source = out_dir / "vec2.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("struct Vec2"), std::string::npos);
    EXPECT_NE(header_text.find("Vec2 add(Vec2 other) const;"), std::string::npos);
    EXPECT_NE(header_text.find("float add_x(Vec2 a, Vec2 b);"), std::string::npos);
}

TEST(E2E, CompileShapeExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_shape";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "shape.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "shape.h";
    const auto source = out_dir / "shape.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("struct Shape"), std::string::npos);
    EXPECT_NE(header_text.find("Point operator+(Point self, Point other);"), std::string::npos);
    EXPECT_NE(header_text.find("String greet(String name);"), std::string::npos);
    EXPECT_NE(header_text.find("float area(Shape s);"), std::string::npos);
}

TEST(E2E, CompileCollectionsExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_collections";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "collections.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "collections.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("enum class Color"), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t sum2(List<std::int32_t> xs)"), std::string::npos);
    EXPECT_NE(header_text.find("Dict<String, std::int32_t>"), std::string::npos);
}

TEST(E2E, CompileDictForExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_dict_for";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const auto input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "dict_for.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto source = out_dir / "dict_for.cpp";
    ASSERT_TRUE(std::filesystem::exists(source));
    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("for (const auto& [k, v] : "), std::string::npos);
}

TEST(E2E, CompileExternFnExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_extern_fn";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "extern_fn.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "extern_fn.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("extern \"C\""), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t host_add"), std::string::npos);
}

TEST(E2E, CompileControlExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_control";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "control.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "control.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const auto header_text = std::string((std::istreambuf_iterator<char>(header_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("namespace math"), std::string::npos);
    EXPECT_NE(header_text.find("template <typename T, typename U>"), std::string::npos);
}

TEST(E2E, CompileExternObjExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_extern_obj";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "extern_obj.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "extern_obj.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const auto header_text = std::string((std::istreambuf_iterator<char>(header_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_EQ(header_text.find("struct Test"), std::string::npos);
    EXPECT_NE(header_text.find("extern Test test_object;"), std::string::npos);
    EXPECT_NE(header_text.find("qplus_host.h"), std::string::npos);
}

TEST(E2E, CompileModsExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_mods";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "mods.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "mods.h";
    const auto source = out_dir / "mods.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const auto header_text = std::string((std::istreambuf_iterator<char>(header_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("namespace math"), std::string::npos);
    EXPECT_NE(header_text.find("namespace vec"), std::string::npos);
    EXPECT_NE(header_text.find("namespace util"), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t run()"), std::string::npos);

    std::ifstream source_in(source);
    const auto source_text = std::string((std::istreambuf_iterator<char>(source_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("math.qp"), std::string::npos);
    EXPECT_NE(source_text.find("vec.qp"), std::string::npos);
    EXPECT_NE(source_text.find("mod.qp"), std::string::npos);
}

TEST(E2E, CompileModTypesExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_mod_types";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "mod_types.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "mod_types.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const auto header_text = std::string((std::istreambuf_iterator<char>(header_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("namespace ecs"), std::string::npos);
    EXPECT_NE(header_text.find("struct World"), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t run()"), std::string::npos);
}

TEST(E2E, CompileUseStaticsExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_use_statics";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "use_statics.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "use_statics.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const auto header_text = std::string((std::istreambuf_iterator<char>(header_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("namespace counter"), std::string::npos);
    EXPECT_NE(header_text.find("hits"), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t run()"), std::string::npos);
}

TEST(E2E, CompileNestedEnumExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_nested_enum";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "nested_enum.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "nested_enum.h";
    const auto source = out_dir / "nested_enum.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));
    std::ifstream header_in(header);
    const auto header_text = std::string((std::istreambuf_iterator<char>(header_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("namespace gfx"), std::string::npos);
    EXPECT_NE(header_text.find("enum class Color"), std::string::npos);
    std::ifstream source_in(source);
    const auto source_text = std::string((std::istreambuf_iterator<char>(source_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("gfx::Color::Red"), std::string::npos);
    EXPECT_EQ(source_text.find("gfx::Color{gfx::Color::Red"), std::string::npos);
}

TEST(E2E, CompileEngineApiExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_engine_api";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "engine_api.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "engine_api.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const auto header_text = std::string((std::istreambuf_iterator<char>(header_in)),
                                         std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("extern World world;"), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t host_bonus();"), std::string::npos);
    EXPECT_EQ(header_text.find("extern engine::World"), std::string::npos);
    EXPECT_NE(header_text.find("namespace engine"), std::string::npos);
}

TEST(E2E, CompileEarlyReturnExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_early_return";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "early_return.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "early_return.h";
    const auto source = out_dir / "early_return.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t abs(std::int32_t x)"), std::string::npos);

    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_EQ(source_text.find("([&]()"), std::string::npos);
    EXPECT_NE(source_text.find("if ("), std::string::npos);
}

TEST(E2E, CompileCastsExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_casts";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "casts.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "casts.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int64_t widen(std::int32_t x)"), std::string::npos);
}

TEST(E2E, CompileNullableExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_nullable";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "nullable.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "nullable.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t or_zero(std::int32_t* p)"), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t* none()"), std::string::npos);
}

TEST(E2E, CompileHeapExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_heap";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "heap.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "heap.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("Point* origin()"), std::string::npos);
    EXPECT_NE(header_text.find("T* alloc(T value)"), std::string::npos);
}

TEST(E2E, CompileIfLetExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_if_let";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "if_let.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "if_let.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t or_zero(std::int32_t* p)"), std::string::npos);
}

TEST(E2E, CompileNullOpsExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_null_ops";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "null_ops.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "null_ops.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t* get_x(Point* p)"), std::string::npos);
}

TEST(E2E, CompileTryExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_try_null";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "try_null.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "try_null.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t* sum(std::int32_t* a, std::int32_t* b)"), std::string::npos);
}

TEST(E2E, CompilePairExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_pair";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "pair.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "pair.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("template <typename T>"), std::string::npos);
    EXPECT_NE(header_text.find("struct Pair"), std::string::npos);
}

TEST(E2E, FileModCycleIsError) {
    const auto dir = std::filesystem::temp_directory_path() / "qplus_e2e_mod_cycle";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream{(dir / "a.qp").string()} << "mod a;\n";
    }

    qpc::DiagnosticEngine diags;
    EXPECT_FALSE(qpc::compile_file(dir / "a.qp", dir / "out", diags));
    ASSERT_FALSE(diags.all().empty());
    EXPECT_NE(diags.all().front().message.find("cyclic module"), std::string::npos);
}

TEST(E2E, FileModAmbiguousIsError) {
    const auto dir = std::filesystem::temp_directory_path() / "qplus_e2e_mod_ambiguous";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "foo");
    {
        std::ofstream{(dir / "root.qp").string()} << "mod foo;\n";
        std::ofstream{(dir / "foo.qp").string()} << "pub fn f() -> i32 { 1 }\n";
        std::ofstream{(dir / "foo" / "mod.qp").string()} << "pub fn f() -> i32 { 2 }\n";
    }

    qpc::DiagnosticEngine diags;
    EXPECT_FALSE(qpc::compile_file(dir / "root.qp", dir / "out", diags));
    ASSERT_FALSE(diags.all().empty());
    EXPECT_NE(diags.all().front().message.find("has both"), std::string::npos);
}

TEST(E2E, CompileClosuresExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_closures";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "closures.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "closures.h";
    const auto source = out_dir / "closures.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t apply_add()"), std::string::npos);
    EXPECT_NE(header_text.find("Fn<std::int32_t(std::int32_t)> f"), std::string::npos);
    EXPECT_NE(header_text.find("using Fn = std::function<T>;"), std::string::npos);
}

TEST(E2E, CompileEachExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_each";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const auto input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "each.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "each.h";
    const auto source = out_dir / "each.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("Fn<void(T)> f"), std::string::npos);
    EXPECT_NE(header_text.find("template <typename U>"), std::string::npos);

    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("[&]"), std::string::npos);
    EXPECT_NE(source_text.find("zip<std::int32_t>"), std::string::npos);
}

TEST(E2E, CompileRefCaptureExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_ref_capture";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "ref_capture.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "ref_capture.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t bump()"), std::string::npos);
}

TEST(E2E, CompileDynAreaExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_dyn_area";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "dyn_area.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "dyn_area.h";
    const auto source = out_dir / "dyn_area.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("struct dyn_Area"), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t area_of(dyn_Area d)"), std::string::npos);
}

TEST(E2E, CompileMathFnsExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_math_fns";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "math_fns.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "math_fns.h";
    const auto source = out_dir / "math_fns.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("#include <cmath>"), std::string::npos);
    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("std::sqrt"), std::string::npos);
    EXPECT_NE(source_text.find("std::fmod"), std::string::npos);
}

TEST(E2E, CompileToStringExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_to_string";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "to_string.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "to_string.h";
    const auto source = out_dir / "to_string.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("String label(std::int32_t n)"), std::string::npos);
    EXPECT_NE(header_text.find("inline String to_string(bool v)"), std::string::npos);
}

TEST(E2E, CompileInterpExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_interp";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "interp.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto source = out_dir / "interp.cpp";
    ASSERT_TRUE(std::filesystem::exists(source));
    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("to_string("), std::string::npos);
    EXPECT_NE(source_text.find("String(\"hp = \")"), std::string::npos);
}

TEST(E2E, CompileCoerceNullExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_coerce_null";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "coerce_null.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto source = out_dir / "coerce_null.cpp";
    ASSERT_TRUE(std::filesystem::exists(source));
    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("nullable_of("), std::string::npos);
}

TEST(E2E, CompileOverloadExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_overload";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "overload.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "overload.h";
    const auto source = out_dir / "overload.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("abs(std::int32_t"), std::string::npos);
    EXPECT_NE(header_text.find("abs(float"), std::string::npos);
}

TEST(E2E, CompilePackagesAppExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_packages_app";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const std::filesystem::path input =
        std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "packages_app" / "app.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "app.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("namespace math"), std::string::npos);
    EXPECT_NE(header_text.find("std::int32_t add("), std::string::npos);
}

TEST(E2E, PackageDependencyResolvesOutsideTree) {
    const auto root = std::filesystem::temp_directory_path() / "qplus_e2e_pkg_dep";
    std::filesystem::remove_all(root);
    const auto app = root / "app";
    const auto lib = root / "lib" / "math";
    std::filesystem::create_directories(app);
    std::filesystem::create_directories(lib);
    {
        std::ofstream{(app / "packages.toml").string()} << R"(
[dependencies]
math = { path = "../lib/math" }
)";
        std::ofstream{(app / "main.qp").string()} << "mod math;\nuse math::*;\npub fn run() -> i32 { add(1, 2) }\n";
        std::ofstream{(lib / "mod.qp").string()} << "pub fn add(a: i32, b: i32) -> i32 { a + b }\n";
    }

    qpc::DiagnosticEngine diags;
    ASSERT_TRUE(qpc::compile_file(app / "main.qp", root / "out", diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);
}

TEST(E2E, CompileTupleExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_tuple";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const auto input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "tuple.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "tuple.h";
    const auto source = out_dir / "tuple.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(source));

    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::tuple<std::int32_t, std::int32_t>"), std::string::npos);
    EXPECT_NE(header_text.find("#include <tuple>"), std::string::npos);
}

TEST(E2E, CompileQueryExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_query";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const auto input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "query.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto source = out_dir / "query.cpp";
    ASSERT_TRUE(std::filesystem::exists(source));
    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find(".next()"), std::string::npos);
}

TEST(E2E, CompileFnValueExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_fn_value";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const auto input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "fn_value.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto source = out_dir / "fn_value.cpp";
    ASSERT_TRUE(std::filesystem::exists(source));
    std::ifstream source_in(source);
    const std::string source_text((std::istreambuf_iterator<char>(source_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(source_text.find("static_cast<"), std::string::npos);
}

TEST(E2E, CompilePackExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_pack";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const auto input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "pack.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "pack.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("typename... T"), std::string::npos);
}

TEST(E2E, CompileMutIterExampleToFiles) {
    const auto out_dir = std::filesystem::temp_directory_path() / "qplus_e2e_mut_iter";
    std::filesystem::remove_all(out_dir);

    qpc::DiagnosticEngine diags;
    const auto input = std::filesystem::path(QPLUS_SOURCE_DIR) / "examples" / "mut_iter.qp";
    ASSERT_TRUE(qpc::compile_file(input, out_dir, diags))
        << (diags.all().empty() ? "compile failed" : diags.all().front().message);

    const auto header = out_dir / "mut_iter.h";
    ASSERT_TRUE(std::filesystem::exists(header));
    std::ifstream header_in(header);
    const std::string header_text((std::istreambuf_iterator<char>(header_in)),
                                  std::istreambuf_iterator<char>());
    EXPECT_NE(header_text.find("std::int32_t& n"), std::string::npos);
}
