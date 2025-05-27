#include "helpers.hpp"

#include <gtest/gtest.h>
#include <string>

static std::string first_error(const qpc::DiagnosticEngine& diags) {
    if (diags.all().empty()) {
        return {};
    }
    return diags.all().front().message;
}

TEST(Typeck, I32AddOk) {
    auto compiled = qpc::test::compile_string("fn add(a: i32, b: i32) -> i32 { a + b }");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, MixedI32F32IsError) {
    auto compiled = qpc::test::compile_string("fn add(a: i32, b: f32) -> f32 { a + b }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot apply operator"), std::string::npos);
}

TEST(Typeck, UnknownIdentifier) {
    auto compiled = qpc::test::compile_string("fn f() -> i32 { x }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("unknown identifier"), std::string::npos);
}

TEST(Typeck, ArityMismatch) {
    auto compiled = qpc::test::compile_string(R"(
        fn id(x: i32) -> i32 { x }
        fn g() -> i32 { id() }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("expects 1 argument"), std::string::npos);
}

TEST(Typeck, ImmutableAssignIsError) {
    auto compiled = qpc::test::compile_string(R"(
        fn f() -> i32 {
            let x = 1;
            x = 2;
            x
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("immutable"), std::string::npos);
}

TEST(Typeck, StructLiteralAndMethodCall) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        impl Point {
            fn add(self, other: Point) -> Point {
                Point { x: self.x + other.x, y: self.y + other.y }
            }
        }
        fn sum_x(a: Point, b: Point) -> i32 { a.add(b).x }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, MissingStructFieldIsError) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { x: i32, y: i32 }
        fn f() -> Point { Point { x: 1 } }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("missing field"), std::string::npos);
}

TEST(Typeck, ImmutableFieldAssignIsError) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { x: i32 }
        impl Point {
            fn set(mut self, v: i32) { self.x = v; }
        }
        fn f() { let mut p = Point { x: 1 }; p.set(2); }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("immutable field"), std::string::npos);
}

TEST(Typeck, EnumMatchAndPrimitivesOk) {
    auto compiled = qpc::test::compile_string(R"(
        enum Color { Red, Green, Blue }
        variant Shape { None, Circle { r: f32 } }
        variant Opt { None, Some(i32) }
        fn area(s: Shape) -> f32 {
            match s {
                None => 0.0,
                Circle { r } => r,
            }
        }
        fn unwrap_or(v: Opt, d: i32) -> i32 {
            match v {
                None => d,
                Some(x) => x,
            }
        }
        fn is_red(c: Color) -> i32 {
            match c {
                Red => 1,
                _ => 0,
            }
        }
        fn hi(name: string) -> string { "hello, " + name }
        fn flag(b: bool) -> bool { b }
        fn as_byte(x: byte) -> u8 { x }
        fn wide(n: i64) -> i64 { n + 1i64 }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, NonExhaustiveMatchIsError) {
    auto compiled = qpc::test::compile_string(R"(
        variant Shape { None, Circle { r: f32 } }
        fn area(s: Shape) -> f32 {
            match s { None => 0.0 }
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("non-exhaustive"), std::string::npos);
}

TEST(Typeck, CollectionsOk) {
    auto compiled = qpc::test::compile_string(R"(
        fn sum(xs: [i32]) -> i32 { xs[0] + xs[1] }
        fn at(buf: [i32; 2]) -> i32 { buf[1] }
        fn hp(m: {string: i32}) -> i32 { m["hp"] }
        fn build() -> i32 {
            let xs = [1, 2];
            let buf: [i32; 2] = [3, 4];
            let m = {"hp": 10};
            sum(xs) + at(buf) + hp(m)
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, OperatorImplAllowsPlus) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { x: i32, y: i32 }
        impl Add for Point {
            fn add(self, other: Point) -> Point {
                Point { x: self.x + other.x, y: self.y + other.y }
            }
        }
        fn add_x(a: Point, b: Point) -> i32 { (a + b).x }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, MissingOperatorImplIsError) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { x: i32, y: i32 }
        fn add_x(a: Point, b: Point) -> i32 { (a + b).x }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("does not implement Add"), std::string::npos);
}

TEST(Typeck, ExternCallOk) {
    auto compiled = qpc::test::compile_string(R"(
        extern "C" {
            fn c_mul(a: i32, b: i32) -> i32;
        }
        extern {
            fn host_add(a: i32, b: i32) -> i32;
            fn host_greet(name: string) -> string;
        }
        fn via_c(a: i32, b: i32) -> i32 { c_mul(a, b) }
        fn via_host(a: i32, b: i32) -> i32 { host_add(a, b) }
        fn hello(name: string) -> string { host_greet(name) }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, ExternCRejectsString) {
    auto compiled = qpc::test::compile_string(R"(
        extern "C" {
            fn puts(s: string) -> i32;
        }
        fn f() -> i32 { puts("x") }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("extern \"C\""), std::string::npos);
}

TEST(Typeck, IfWhileForAndCompare) {
    auto compiled = qpc::test::compile_string(R"(
        fn clamp(x: i32, lo: i32, hi: i32) -> i32 {
            if x < lo { lo } else if x > hi { hi } else { x }
        }
        fn sum_n(n: i32) -> i32 {
            let mut s = 0;
            let mut i = 0;
            while i < n {
                s = s + i;
                i = i + 1;
            }
            s
        }
        fn sum_range() -> i32 {
            let mut s = 0;
            for i in 0..4 {
                s = s + i;
            }
            s
        }
        fn flags(a: i32, b: bool) -> bool {
            a == 1 && !b || a != 2
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, ModUseGenericsAndAssociated) {
    auto compiled = qpc::test::compile_string(R"(
        let mut hits = 0;
        mod math {
            pub fn min(a: i32, b: i32) -> i32 { if a < b { a } else { b } }
            pub fn id<T>(x: T) -> T { x }
        }
        use math::*;
        trait Component {}
        struct Transform { x: i32 }
        struct Sprite { id: i32 }
        struct World {}
        impl Component for Transform {}
        impl Component for Sprite {}
        impl World {
            fn for_each<T: Component, U: Component>() -> i32 { 2 }
        }
        fn run() -> i32 {
            hits = hits + 1;
            min(id<i32>(1), World.for_each<Transform, Sprite>())
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, TraitBoundIsError) {
    auto compiled = qpc::test::compile_string(R"(
        trait Component {}
        struct World {}
        impl World {
            fn for_each<T: Component>() -> i32 { 1 }
        }
        fn run() -> i32 { World.for_each<i32>() }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("does not implement"), std::string::npos);
}

TEST(Typeck, BreakOutsideLoopIsError) {
    auto compiled = qpc::test::compile_string("fn f() { break; }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("break/continue"), std::string::npos);
}
