#include "helpers.hpp"

#include <gtest/gtest.h>

TEST(Parser, PubFnAdd) {
    auto parsed = qpc::test::parse_string("pub fn add(a: i32, b: i32) -> i32 { a + b }");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.functions.size(), 1u);
    const auto& fn = parsed.ast.functions[0];
    EXPECT_TRUE(fn.pub);
    EXPECT_EQ(fn.name, "add");
    ASSERT_EQ(fn.params.size(), 2u);
    EXPECT_EQ(fn.params[0].name, "a");
    EXPECT_EQ(fn.params[0].ty, "i32");
    EXPECT_EQ(fn.params[1].name, "b");
    ASSERT_TRUE(fn.body.tail);
}

TEST(Parser, LetMutAndReturn) {
    auto parsed = qpc::test::parse_string(R"(
        fn sum(a: i32, b: i32) -> i32 {
            let mut c = a;
            c = c + b;
            return c;
        }
    )");
    ASSERT_FALSE(parsed.diags.has_errors());
    ASSERT_EQ(parsed.ast.functions.size(), 1u);
    EXPECT_EQ(parsed.ast.functions[0].body.stmts.size(), 3u);
    EXPECT_FALSE(parsed.ast.functions[0].body.tail);
}

TEST(Parser, NestedCalls) {
    auto parsed = qpc::test::parse_string(R"(
        fn id(x: i32) -> i32 { x }
        fn wrap(x: i32) -> i32 { id(id(x)) }
    )");
    ASSERT_FALSE(parsed.diags.has_errors());
    ASSERT_EQ(parsed.ast.functions.size(), 2u);
    ASSERT_TRUE(parsed.ast.functions[1].body.tail);
}

TEST(Parser, MissingParenIsError) {
    auto parsed = qpc::test::parse_string("fn add(a: i32 -> i32 { a }");
    EXPECT_TRUE(parsed.diags.has_errors());
}
