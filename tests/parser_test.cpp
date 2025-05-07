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

TEST(Parser, StructImplAndFieldAccess) {
    auto parsed = qpc::test::parse_string(R"(
        struct Vec2 { mut x: f32, mut y: f32 }
        impl Vec2 {
            fn add(self, other: Vec2) -> Vec2 {
                Vec2 { x: self.x + other.x, y: self.y + other.y }
            }
        }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.structs.size(), 1u);
    EXPECT_EQ(parsed.ast.structs[0].name, "Vec2");
    ASSERT_EQ(parsed.ast.structs[0].fields.size(), 2u);
    EXPECT_TRUE(parsed.ast.structs[0].fields[0].mut);
    ASSERT_EQ(parsed.ast.impls.size(), 1u);
    ASSERT_EQ(parsed.ast.impls[0].methods.size(), 1u);
    EXPECT_EQ(parsed.ast.impls[0].methods[0].self_param, qpc::SelfParam::Value);
    ASSERT_TRUE(parsed.ast.impls[0].methods[0].body.tail);
}

TEST(Parser, EnumMatchAndTraitImpl) {
    auto parsed = qpc::test::parse_string(R"(
        enum Shape { None, Circle { r: f32 }, Rect { w: f32, h: f32 } }
        variant Opt { None, Some(i32) }
        impl Add for Point {
            fn add(self, other: Point) -> Point { other }
        }
        fn area(s: Shape) -> f32 {
            match s {
                None => 0.0,
                Circle { r } => r,
                Some(x) => 1.0,
            }
        }
        fn hi() -> string { "ok" }
        fn flag() -> bool { true }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.enums.size(), 2u);
    EXPECT_EQ(parsed.ast.enums[0].name, "Shape");
    ASSERT_EQ(parsed.ast.enums[0].variants.size(), 3u);
    EXPECT_EQ(parsed.ast.enums[0].variants[0].name, "None");
    EXPECT_EQ(parsed.ast.enums[0].variants[1].name, "Circle");
    ASSERT_EQ(parsed.ast.enums[0].variants[1].fields.size(), 1u);
    EXPECT_EQ(parsed.ast.enums[1].name, "Opt");
    ASSERT_EQ(parsed.ast.enums[1].variants.size(), 2u);
    EXPECT_TRUE(parsed.ast.enums[1].variants[1].tuple);
    ASSERT_EQ(parsed.ast.impls.size(), 1u);
    ASSERT_TRUE(parsed.ast.impls[0].trait_name);
    EXPECT_EQ(*parsed.ast.impls[0].trait_name, "Add");
    EXPECT_EQ(parsed.ast.impls[0].type_name, "Point");
    ASSERT_EQ(parsed.ast.functions.size(), 3u);
    ASSERT_TRUE(parsed.ast.functions[0].body.tail);
}
