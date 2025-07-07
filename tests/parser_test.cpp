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
    EXPECT_EQ(fn.params[0].ty.name, "i32");
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
        enum Color { Red, Green = 2, Blue }
        variant Shape { None, Circle { r: f32 }, Rect { w: f32, h: f32 } }
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
    ASSERT_EQ(parsed.ast.enums.size(), 1u);
    EXPECT_EQ(parsed.ast.enums[0].name, "Color");
    ASSERT_EQ(parsed.ast.enums[0].members.size(), 3u);
    EXPECT_EQ(*parsed.ast.enums[0].members[1].value, 2);
    ASSERT_EQ(parsed.ast.variants.size(), 2u);
    EXPECT_EQ(parsed.ast.variants[0].name, "Shape");
    ASSERT_EQ(parsed.ast.variants[0].variants.size(), 3u);
    EXPECT_EQ(parsed.ast.variants[0].variants[1].name, "Circle");
    ASSERT_EQ(parsed.ast.variants[0].variants[1].fields.size(), 1u);
    EXPECT_EQ(parsed.ast.variants[1].name, "Opt");
    EXPECT_TRUE(parsed.ast.variants[1].variants[1].tuple);
    ASSERT_EQ(parsed.ast.impls.size(), 1u);
    ASSERT_TRUE(parsed.ast.impls[0].trait_name);
    EXPECT_EQ(*parsed.ast.impls[0].trait_name, "Add");
    EXPECT_EQ(parsed.ast.impls[0].type_name, "Point");
    ASSERT_EQ(parsed.ast.functions.size(), 3u);
    ASSERT_TRUE(parsed.ast.functions[0].body.tail);
}

TEST(Parser, Collections) {
    auto parsed = qpc::test::parse_string(R"(
        fn f(xs: [i32], buf: [i32; 2], m: {string: i32}) -> i32 {
            let a = [1, 2];
            let d = {"hp": 10};
            xs[0] + buf[1] + m["hp"] + a[0] + d["hp"]
        }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.functions.size(), 1u);
    EXPECT_EQ(parsed.ast.functions[0].params[0].ty.kind, qpc::TypeExpr::Kind::List);
    EXPECT_EQ(parsed.ast.functions[0].params[1].ty.kind, qpc::TypeExpr::Kind::Array);
    EXPECT_EQ(parsed.ast.functions[0].params[1].ty.array_len, 2u);
    EXPECT_EQ(parsed.ast.functions[0].params[2].ty.kind, qpc::TypeExpr::Kind::Dict);
}

TEST(Parser, EnumWithFieldsIsError) {
    auto parsed = qpc::test::parse_string("enum Shape { Circle { r: f32 } }");
    EXPECT_TRUE(parsed.diags.has_errors());
    EXPECT_NE(parsed.diags.all().front().message.find("use 'variant'"), std::string::npos);
}

TEST(Parser, ExternBlocks) {
    auto parsed = qpc::test::parse_string(R"(
        extern "C" {
            fn c_mul(a: i32, b: i32) -> i32;
        }
        extern {
            fn host_add(a: i32, b: i32) -> i32;
            fn host_greet(name: string) -> string;
        }
        fn use_host(a: i32, b: i32) -> i32 { host_add(a, b) }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.functions.size(), 4u);
    EXPECT_TRUE(parsed.ast.functions[0].is_extern);
    EXPECT_EQ(parsed.ast.functions[0].abi, qpc::Abi::C);
    EXPECT_EQ(parsed.ast.functions[0].name, "c_mul");
    EXPECT_TRUE(parsed.ast.functions[1].is_extern);
    EXPECT_EQ(parsed.ast.functions[1].abi, qpc::Abi::Qplus);
    EXPECT_EQ(parsed.ast.functions[1].name, "host_add");
    EXPECT_TRUE(parsed.ast.functions[2].is_extern);
    EXPECT_FALSE(parsed.ast.functions[3].is_extern);
}

TEST(Parser, ExternBodyIsError) {
    auto parsed = qpc::test::parse_string("extern { fn foo() -> i32 { 1 } }");
    EXPECT_TRUE(parsed.diags.has_errors());
    EXPECT_NE(parsed.diags.all().front().message.find("cannot have a body"), std::string::npos);
}

TEST(Parser, ControlModUseAndGenerics) {
    auto parsed = qpc::test::parse_string(R"(
        let mut hits = 0;
        mod math {
            pub fn min(a: i32, b: i32) -> i32 { if a < b { a } else { b } }
        }
        use math::min;
        trait Component {}
        fn sum(xs: [i32]) -> i32 {
            let mut s = 0;
            for i in 0..3 {
                s = s + i;
            }
            while s < 10 {
                s = s + 1;
            }
            if s > 0 { s } else { 0 }
        }
        impl World {
            fn for_each<T: Component, U: Component>() -> i32 { 1 }
        }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.statics.size(), 1u);
    EXPECT_TRUE(parsed.ast.statics[0].mut);
    EXPECT_EQ(parsed.ast.statics[0].name, "hits");
    ASSERT_EQ(parsed.ast.mods.size(), 1u);
    EXPECT_EQ(parsed.ast.mods[0].name, "math");
    ASSERT_EQ(parsed.ast.uses.size(), 1u);
    EXPECT_EQ(parsed.ast.uses[0].path.back(), "min");
    ASSERT_EQ(parsed.ast.traits.size(), 1u);
    EXPECT_EQ(parsed.ast.traits[0].name, "Component");
    ASSERT_EQ(parsed.ast.impls.size(), 1u);
    ASSERT_EQ(parsed.ast.impls[0].methods.size(), 1u);
    ASSERT_EQ(parsed.ast.impls[0].methods[0].type_params.size(), 2u);
    EXPECT_EQ(parsed.ast.impls[0].methods[0].type_params[0].name, "T");
    EXPECT_EQ(*parsed.ast.impls[0].methods[0].type_params[0].bound, "Component");
}

TEST(Parser, ExternOpaqueStructImplAndStatic) {
    auto parsed = qpc::test::parse_string(R"(
        extern {
            pub struct Test;
            impl Test {
                pub fn add<T>(self, a: T, b: T) -> T;
                pub fn created() -> i32;
            }
            pub let mut test_object: Test;
        }
        fn test() -> i32 { test_object.add(3, 5) }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.structs.size(), 1u);
    EXPECT_TRUE(parsed.ast.structs[0].pub);
    EXPECT_TRUE(parsed.ast.structs[0].opaque);
    EXPECT_TRUE(parsed.ast.structs[0].is_extern);
    EXPECT_EQ(parsed.ast.structs[0].name, "Test");
    ASSERT_EQ(parsed.ast.impls.size(), 1u);
    ASSERT_EQ(parsed.ast.impls[0].methods.size(), 2u);
    EXPECT_TRUE(parsed.ast.impls[0].methods[0].is_extern);
    EXPECT_EQ(parsed.ast.impls[0].methods[0].self_param, qpc::SelfParam::Value);
    EXPECT_EQ(parsed.ast.impls[0].methods[0].name, "add");
    ASSERT_EQ(parsed.ast.impls[0].methods[0].type_params.size(), 1u);
    EXPECT_EQ(parsed.ast.impls[0].methods[1].self_param, qpc::SelfParam::None);
    ASSERT_EQ(parsed.ast.statics.size(), 1u);
    EXPECT_TRUE(parsed.ast.statics[0].is_extern);
    EXPECT_TRUE(parsed.ast.statics[0].mut);
    EXPECT_EQ(parsed.ast.statics[0].name, "test_object");
}

TEST(Parser, ExternCRejectsStruct) {
    auto parsed = qpc::test::parse_string(R"(
        extern "C" {
            struct Test;
        }
    )");
    EXPECT_TRUE(parsed.diags.has_errors());
    EXPECT_NE(parsed.diags.all().front().message.find("can only declare functions"), std::string::npos);
}

TEST(Parser, ExternStructMustBeOpaque) {
    auto parsed = qpc::test::parse_string("extern { struct Test { x: i32 } }");
    EXPECT_TRUE(parsed.diags.has_errors());
    EXPECT_NE(parsed.diags.all().front().message.find("';' after opaque struct"), std::string::npos);
}

TEST(Parser, ExternStaticCannotHaveInitializer) {
    auto parsed = qpc::test::parse_string("extern { struct Test; let mut x: Test = Test {}; }");
    EXPECT_TRUE(parsed.diags.has_errors());
    EXPECT_NE(parsed.diags.all().front().message.find("cannot have an initializer"), std::string::npos);
}

TEST(Parser, FileModVsInline) {
    auto parsed = qpc::test::parse_string(R"(
        pub mod math;
        mod util {
            pub fn id(x: i32) -> i32 { x }
        }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.mods.size(), 2u);
    EXPECT_TRUE(parsed.ast.mods[0].pub);
    EXPECT_TRUE(parsed.ast.mods[0].file);
    EXPECT_EQ(parsed.ast.mods[0].name, "math");
    EXPECT_FALSE(parsed.ast.mods[1].file);
    EXPECT_EQ(parsed.ast.mods[1].name, "util");
    ASSERT_EQ(parsed.ast.mods[1].body->functions.size(), 1u);
}

TEST(Parser, NullableAndNull) {
    auto parsed = qpc::test::parse_string(R"(
        fn or_zero(p: i32?) -> i32 {
            if p == null { 0 } else { p! }
        }
        fn none() -> i32? { null }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    EXPECT_EQ(parsed.ast.functions[0].params[0].ty.kind, qpc::TypeExpr::Kind::Nullable);
    EXPECT_TRUE(std::holds_alternative<qpc::LitNull>(parsed.ast.functions[1].body.tail->kind));
}
