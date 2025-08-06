#include "helpers.hpp"

#include <gtest/gtest.h>
#include <variant>

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

TEST(Parser, ClosureAndFnType) {
    auto parsed = qpc::test::parse_string(R"(
        fn apply(f: fn(i32) -> i32, x: i32) -> i32 {
            let add = |a: i32, b: i32| a + b;
            let k = || 1;
            f(x) + add(1, 2) + k() + (|n: i32| n)(3)
        }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.functions.size(), 1u);
    EXPECT_EQ(parsed.ast.functions[0].params[0].ty.kind, qpc::TypeExpr::Kind::Fn);
    ASSERT_EQ(parsed.ast.functions[0].body.stmts.size(), 2u);
    const auto* let0 = std::get_if<qpc::StmtLet>(&parsed.ast.functions[0].body.stmts[0]->kind);
    ASSERT_NE(let0, nullptr);
    EXPECT_TRUE(std::holds_alternative<qpc::ExprClosure>(let0->init->kind));
    const auto* clo = std::get_if<qpc::ExprClosure>(&let0->init->kind);
    ASSERT_NE(clo, nullptr);
    ASSERT_EQ(clo->params.size(), 2u);
    EXPECT_EQ(clo->params[0].name, "a");
    const auto* let1 = std::get_if<qpc::StmtLet>(&parsed.ast.functions[0].body.stmts[1]->kind);
    ASSERT_NE(let1, nullptr);
    const auto* empty = std::get_if<qpc::ExprClosure>(&let1->init->kind);
    ASSERT_NE(empty, nullptr);
    EXPECT_TRUE(empty->params.empty());
}

TEST(Parser, ClosureMissingParamTypeIsError) {
    auto parsed = qpc::test::parse_string("fn f() -> i32 { let g = |x| x; g(1) }");
    EXPECT_TRUE(parsed.diags.has_errors());
}

TEST(Parser, AsCast) {
    auto parsed = qpc::test::parse_string("fn widen(x: i32) -> i64 { x as i64 }");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_TRUE(parsed.ast.functions[0].body.tail);
    EXPECT_TRUE(std::holds_alternative<qpc::ExprCast>(parsed.ast.functions[0].body.tail->kind));
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

TEST(Parser, NewStruct) {
    auto parsed = qpc::test::parse_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        fn origin() -> Point? { new Point { x: 0, y: 0 } }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_TRUE(parsed.ast.functions[0].body.tail);
    EXPECT_TRUE(std::holds_alternative<qpc::ExprNew>(parsed.ast.functions[0].body.tail->kind));
}

TEST(Parser, IfLet) {
    auto parsed = qpc::test::parse_string("fn or_zero(p: i32?) -> i32 { if let v = p { v } else { 0 } }");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_TRUE(parsed.ast.functions[0].body.tail);
    const auto* iff = std::get_if<qpc::ExprIf>(&parsed.ast.functions[0].body.tail->kind);
    ASSERT_NE(iff, nullptr);
    EXPECT_EQ(iff->let_name, "v");
}

TEST(Parser, NullSafeAndCoalesce) {
    auto parsed = qpc::test::parse_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        fn get_x(p: Point?) -> i32? { p?.x }
        fn or_x(p: Point?, d: i32) -> i32 { p?.x ?? d }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    const auto* field = std::get_if<qpc::ExprField>(&parsed.ast.functions[0].body.tail->kind);
    ASSERT_NE(field, nullptr);
    EXPECT_TRUE(field->null_safe);
    EXPECT_TRUE(std::holds_alternative<qpc::ExprCoalesce>(parsed.ast.functions[1].body.tail->kind));
}

TEST(Parser, GenericStruct) {
    auto parsed = qpc::test::parse_string(R"(
        struct Pair<T> { mut a: T, mut b: T }
        impl Pair<T> {
            fn first(self) -> T { self.a }
        }
        fn make(x: i32) -> Pair<i32> { Pair { a: x, b: x } }
    )");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_EQ(parsed.ast.structs[0].type_params.size(), 1u);
    EXPECT_EQ(parsed.ast.structs[0].type_params[0].name, "T");
    ASSERT_EQ(parsed.ast.impls[0].type_params.size(), 1u);
    EXPECT_EQ(parsed.ast.functions[0].return_ty->args.size(), 1u);
}

TEST(Parser, TryOperator) {
    auto parsed = qpc::test::parse_string("fn sum(a: i32?, b: i32?) -> i32? { a? + b? }");
    ASSERT_FALSE(parsed.diags.has_errors()) << parsed.diags.all().front().message;
    ASSERT_TRUE(parsed.ast.functions[0].body.tail);
    EXPECT_TRUE(std::holds_alternative<qpc::ExprBinary>(parsed.ast.functions[0].body.tail->kind));
}
