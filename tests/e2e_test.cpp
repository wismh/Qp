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
