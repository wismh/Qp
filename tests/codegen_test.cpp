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

TEST(Codegen, StructAndMethod) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        impl Point {
            fn add(self, other: Point) -> Point {
                Point { x: self.x + other.x, y: self.y + other.y }
            }
            fn scale(mut self, s: i32) { self.x = self.x * s; }
        }
        fn origin() -> Point { Point { x: 0, y: 0 } }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("struct Point"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::int32_t x;"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("Point add(Point other) const;"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("void scale(std::int32_t s);"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("Point self = *this;"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("Point& self = *this;"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find(".x = "), std::string::npos);
}

TEST(Codegen, EnumMatchStringAndOperator) {
    auto compiled = qpc::test::compile_string(R"(
        enum Color { Red, Green, Blue }
        variant Shape { None, Circle { r: f32 } }
        fn area(s: Shape) -> f32 {
            match s {
                None => 0.0,
                Circle { r } => r,
            }
        }
        fn hi(name: string) -> string { "hello, " + name }
        struct Point { x: i32, y: i32 }
        impl Add for Point {
            fn add(self, other: Point) -> Point {
                Point { x: self.x + other.x, y: self.y + other.y }
            }
        }
        fn add_x(a: Point, b: Point) -> i32 { (a + b).x }
        fn flag(b: bool) -> bool { true }
        fn as_byte(x: byte) -> u8 { x }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("enum class Color"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("struct Shape"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::variant<None, Circle>"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("using String = std::string;"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("Point operator+(Point self, Point other);"),
              std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("String hi(String name);"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("bool flag(bool b);"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::uint8_t as_byte(std::uint8_t x);"),
              std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("std::holds_alternative<Shape::None>"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("std::get_if<Shape::Circle>"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("String(\"hello, \")"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("operator+(Point self, Point other)"), std::string::npos);
}

TEST(Codegen, CollectionsAndCEnum) {
    auto compiled = qpc::test::compile_string(R"(
        enum Color { Red, Green }
        fn first(xs: [i32], buf: [i32; 2], m: {string: i32}) -> i32 {
            xs[0] + buf[1] + m["hp"]
        }
        fn red() -> Color { Color::Red }
        fn make() -> [i32] { [1, 2, 3] }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("enum class Color"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("using List = std::vector<T>;"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("List<std::int32_t> xs"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("Array<std::int32_t, 2>"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("Dict<String, std::int32_t>"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("Color::Red"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("List<std::int32_t>{"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find(".at("), std::string::npos);
}

TEST(Codegen, ExternDeclarations) {
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
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("extern \"C\""), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::int32_t c_mul(std::int32_t a, std::int32_t b);"),
              std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::int32_t host_add(std::int32_t a, std::int32_t b);"),
              std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("String host_greet(String name);"), std::string::npos);
    EXPECT_EQ(compiled.result.output.source.find("std::int32_t host_add"), std::string::npos);
    EXPECT_EQ(compiled.result.output.source.find("std::int32_t c_mul"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("c_mul(a, b)"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("host_add(a, b)"), std::string::npos);
}

TEST(Codegen, ControlModGenericsAndStatics) {
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
        fn sum_range() -> i32 {
            let mut s = 0;
            for i in 0..3 {
                s = s + i;
            }
            s
        }
        fn run() -> i32 {
            hits = hits + 1;
            min(id<i32>(1), World.for_each<Transform, Sprite>())
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("namespace math"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("using namespace math;"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("inline std::int32_t hits"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("template <typename T>"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("template <typename T, typename U>"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("static std::int32_t for_each()"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("for (std::int32_t i = "), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("World::for_each<Transform, Sprite>()"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("id<std::int32_t>(1)"), std::string::npos);
}

TEST(Codegen, ExternOpaqueMethodsAndStatic) {
    auto compiled = qpc::test::compile_string(R"(
        extern {
            pub struct Test;
            impl Test {
                pub fn add<T>(self, a: T, b: T) -> T;
                pub fn created() -> i32;
            }
            pub let mut test_object: Test;
        }
        fn test() -> i32 { test_object.add(3, 5) }
        fn created() -> i32 { Test.created() }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_EQ(compiled.result.output.header.find("struct Test"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("extern Test test_object;"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("__has_include(\"qplus_host.h\")"), std::string::npos);
    EXPECT_EQ(compiled.result.output.source.find("Test::add"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("test_object.add<std::int32_t>(3, 5)"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("Test::created()"), std::string::npos);
}

TEST(Codegen, ClosureLambdaAndFnAlias) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn twice(n: i32) -> i32 {
            let f = |x: i32| x + x;
            f(n)
        }
        pub fn apply(g: fn(i32) -> i32, x: i32) -> i32 { g(x) }
        pub fn one() -> i32 { let k = || 1; k() + (|| 2)() }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("#include <functional>"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("using Fn = std::function<T>;"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("Fn<std::int32_t(std::int32_t)> g"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("[=]"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("Fn<std::int32_t(std::int32_t)>"), std::string::npos);
}

TEST(Codegen, ReturnInIfIsNotALambda) {
    auto compiled = qpc::test::compile_string(R"(
        fn abs(x: i32) -> i32 {
            if x < 0 {
                return 0 - x;
            }
            x
        }
        fn pick(c: bool, a: i32, b: i32) -> i32 {
            if c { a } else { b }
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_EQ(compiled.result.output.source.find("([&]()"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("if ("), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("return (0 - x);"), std::string::npos);
}

TEST(Codegen, AsCast) {
    auto compiled = qpc::test::compile_string("fn widen(x: i32) -> i64 { x as i64 }");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("static_cast<std::int64_t>(x)"), std::string::npos);
}
