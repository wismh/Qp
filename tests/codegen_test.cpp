#include "helpers.hpp"

#include <gtest/gtest.h>
#include <string>

TEST(Codegen, EmitsAddSignatureAndReturn) {
    auto compiled = qpc::test::compile_string("pub fn add(a: i32, b: i32) -> i32 { a + b }");
    ASSERT_TRUE(compiled.result.ok);
    EXPECT_NE(compiled.result.output.header.find("std::int32_t add"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::int32_t a"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("return"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("a + b"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("namespace qplus"), std::string::npos);
}

TEST(Codegen, LetAndReturn) {
    auto compiled = qpc::test::compile_string(R"(
        fn add(a: i32, b: i32) -> i32 {
            let c = a + b;
            return c;
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("std::int32_t c = "), std::string::npos);
}

TEST(Codegen, F32Function) {
    auto compiled = qpc::test::compile_string("fn scale(x: f32) -> f32 { x * 2.0 }");
    ASSERT_TRUE(compiled.result.ok);
    EXPECT_NE(compiled.result.output.header.find("float scale(float x)"), std::string::npos);
}
