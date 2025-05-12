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
        enum Shape { None, Circle { r: f32 } }
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
        fn hi(name: string) -> string { "hello, " + name }
        fn flag(b: bool) -> bool { b }
        fn as_byte(x: byte) -> u8 { x }
        fn wide(n: i64) -> i64 { n + 1i64 }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, NonExhaustiveMatchIsError) {
    auto compiled = qpc::test::compile_string(R"(
        enum Shape { None, Circle { r: f32 } }
        fn area(s: Shape) -> f32 {
            match s { None => 0.0 }
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("non-exhaustive"), std::string::npos);
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
