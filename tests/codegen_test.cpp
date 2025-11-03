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

TEST(Codegen, ForDictStructuredBinding) {
    auto compiled = qpc::test::compile_string(R"(
        fn sum(m: {i32: i32}) -> i32 {
            let mut s = 0;
            for (k, v) in m {
                s = s + k + v;
            }
            s
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("for (const auto& [k, v] : "), std::string::npos);
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

TEST(Codegen, RefClosureCapture) {
    auto compiled = qpc::test::compile_string(R"(
        fn bump() -> i32 {
            let mut n = 0;
            let f = ref || { n = n + 1; };
            f();
            n
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("[&]"), std::string::npos);
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

TEST(Codegen, NullableNullAndUnwrap) {
    auto compiled = qpc::test::compile_string(R"(
        fn or_zero(p: i32?) -> i32 {
            if p == null { 0 } else { p! }
        }
        fn none() -> i32? { null }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("std::int32_t* p"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::int32_t* none()"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("T unwrap(T* p)"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("nullptr"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("unwrap(p)"), std::string::npos);
}

TEST(Codegen, NewAllocAndGc) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        fn origin() -> Point? { new Point { x: 1, y: 2 } }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("T* alloc(T value)"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("gc_collect"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("alloc(Point{"), std::string::npos);
}

TEST(Codegen, IfLet) {
    auto compiled = qpc::test::compile_string(
        "fn or_zero(p: i32?) -> i32 { if let v = p { v } else { 0 } }");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("!= nullptr"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("const auto v = *"), std::string::npos);
}

TEST(Codegen, NullSafeAndCoalesce) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        fn get_x(p: Point?) -> i32? { p?.x }
        fn or_x(p: Point?, d: i32) -> i32 { p?.x ?? d }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("->x"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("== nullptr"), std::string::npos);
}

TEST(Codegen, TryOperator) {
    auto compiled = qpc::test::compile_string(R"(
        fn sum(a: i32?, b: i32?) -> i32? {
            if a? + b? == 0 { null } else { a }
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("== nullptr"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("return nullptr;"), std::string::npos);
}

TEST(Codegen, CoerceValueToNullable) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn wrap(n: i32) -> i32? { n }
        pub fn take(p: i32?) -> i32 { if let v = p { v } else { 0 } }
        pub fn run() -> i32 { take(7) }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("nullable_of("), std::string::npos);
}

TEST(Codegen, GenericStructTemplate) {
    auto compiled = qpc::test::compile_string(R"(
        struct Pair<T> { mut a: T, mut b: T }
        impl Pair<T> {
            fn first(self) -> T { self.a }
        }
        fn make(x: i32) -> Pair<i32> { Pair { a: x, b: x } }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("template <typename T>"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("struct Pair"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("Pair<std::int32_t>"), std::string::npos);
}

TEST(Codegen, GenericMethodRefCallback) {
    auto compiled = qpc::test::compile_string(R"(
        struct Pair<T> { mut a: T, mut b: T }
        impl Pair<T> {
            fn each(self, f: fn(T) -> ()) {
                f(self.a);
                f(self.b);
            }
            fn zip<U>(self, other: U, f: fn(T, U) -> i32) -> i32 {
                f(self.a, other) + f(self.b, other)
            }
        }
        fn sum_each(p: Pair<i32>) -> i32 {
            let mut s = 0;
            p.each(ref |x: i32| { s = s + x; });
            s
        }
        fn zip_mul(p: Pair<i32>) -> i32 {
            p.zip(4, |x: i32, y: i32| x * y)
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("Fn<void(T)> f"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("template <typename U>"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("[&]"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("zip<std::int32_t>"), std::string::npos);
}

TEST(Codegen, DynTraitVtable) {
    auto compiled = qpc::test::compile_string(R"(
        trait Area {
            fn area(self) -> i32;
        }
        struct Rect { w: i32, h: i32 }
        impl Area for Rect {
            fn area(self) -> i32 { self.w * self.h }
        }
        fn area_of(d: dyn Area) -> i32 { d.area() }
        pub fn run() -> i32 { area_of(Rect { w: 3, h: 4 }) }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("struct Area_vtable"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("struct dyn_Area"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("make_dyn_Area"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::int32_t area_of(dyn_Area d)"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("make_dyn_Area("), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("d.area()"), std::string::npos);
}

TEST(Codegen, MathBuiltinsUseCmath) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn hypot(x: f32, y: f32) -> f32 { sqrt(x * x + y * y) }
        pub fn wrap(x: f32) -> f32 { fmod(x, 2.0) }
        pub fn log1() -> f32 { ln(1.0) }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("#include <cmath>"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("std::sqrt"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("std::fmod"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("std::log"), std::string::npos);
}

TEST(Codegen, NestedModuleStructTypes) {
    auto compiled = qpc::test::compile_string(R"(
        mod ecs {
            pub struct Id { v: i32 }
            pub struct World { mut id: i32, tag: Id }
            impl World {
                fn get_id(self) -> i32 { self.id + self.tag.v }
            }
            pub fn make() -> World { World { id: 1, tag: Id { v: 2 } } }
        }
        use ecs::*;
        pub fn run() -> i32 { make().get_id() }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("namespace ecs"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("struct Id"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("struct World"), std::string::npos);
}

TEST(Codegen, UseModuleStatic) {
    auto compiled = qpc::test::compile_string(R"(
        mod ecs {
            pub let mut hits: i32 = 1;
        }
        use ecs::*;
        pub fn run() -> i32 {
            hits = hits + 1;
            ecs::hits
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("hits = "), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("hits = (hits + "), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("ecs::hits"), std::string::npos);
}

TEST(Codegen, NestedModuleCEnum) {
    auto compiled = qpc::test::compile_string(R"(
        mod gfx {
            pub enum Color { Red, Green }
            pub fn red() -> Color { Color::Red }
        }
        use gfx::*;
        pub fn run() -> Color { red() }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("namespace gfx"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("enum class Color"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("gfx::Color::Red"), std::string::npos);
    EXPECT_EQ(compiled.result.output.source.find("gfx::Color{gfx::Color::Red"), std::string::npos);
}

TEST(Codegen, NestedExternHoistedToRoot) {
    auto compiled = qpc::test::compile_string(R"(
        mod engine {
            extern {
                pub struct World;
                impl World {
                    pub fn step(self) -> i32;
                }
                pub let mut world: World;
                fn host_tick() -> i32;
            }
            pub fn go() -> i32 { world.step() + host_tick() }
        }
        use engine::*;
        pub fn run() -> i32 { go() }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("extern World world;"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::int32_t host_tick();"), std::string::npos);
    EXPECT_EQ(compiled.result.output.header.find("extern engine::World"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("namespace engine"), std::string::npos);
    // extern decls appear before nested namespace, not as qplus::engine::…
    const auto eng = compiled.result.output.header.find("namespace engine");
    const auto world_ext = compiled.result.output.header.find("extern World world;");
    const auto tick_ext = compiled.result.output.header.find("std::int32_t host_tick();");
    ASSERT_NE(eng, std::string::npos);
    ASSERT_NE(world_ext, std::string::npos);
    ASSERT_NE(tick_ext, std::string::npos);
    EXPECT_LT(world_ext, eng);
    EXPECT_LT(tick_ext, eng);
}

TEST(Codegen, ToStringBuiltin) {
    auto compiled = qpc::test::compile_string(R"(
        enum Color { Red }
        pub fn run(n: i32) -> string {
            to_string(n) + to_string(true) + to_string(Color::Red)
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("inline String to_string(bool v)"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("to_string("), std::string::npos);
}

TEST(Codegen, TypeIdAndReflect) {
    auto compiled = qpc::test::compile_string(R"(
        struct Health { hp: i32, max: i32 }
        pub fn ping() -> i32 { 1 }
        pub fn run() -> string {
            type_name<Health>() + field_name<Health>(0) + fn_name(ping)
        }
        pub fn n() -> i32 { field_count<Health>() }
        pub fn id() -> u64 { type_id<Health>() }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("String(\"Health\")"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("String(\"hp\")"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("String(\"ping\")"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("ull"), std::string::npos);
}

TEST(Codegen, StringInterpolation) {
    auto compiled = qpc::test::compile_string("pub fn status(hp: i32) -> string { \"hp = ${hp}\" }");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("to_string("), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("String(\"hp = \")"), std::string::npos);
}

TEST(Codegen, FunctionOverloads) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn abs(x: i32) -> i32 { if x < 0 { -x } else { x } }
        pub fn abs(x: f32) -> f32 { if x < 0.0 { -x } else { x } }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    const auto& h = compiled.result.output.header;
    const auto first = h.find("std::int32_t abs(std::int32_t");
    const auto second = h.find("float abs(float");
    ASSERT_NE(first, std::string::npos);
    ASSERT_NE(second, std::string::npos);
}

TEST(Codegen, TupleStdGetAndUnpack) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn first(p: (i32, i32)) -> i32 { p.0 }
        pub fn sum(xs: [(i32, i32)]) -> i32 {
            let mut s = 0;
            for (a, b) in xs {
                s = s + a + b;
            }
            s
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("#include <tuple>"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::tuple<std::int32_t, std::int32_t>"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("std::get<0>("), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("for (const auto& [a, b] : "), std::string::npos);
}

TEST(Codegen, CustomIteratorNextLoop) {
    auto compiled = qpc::test::compile_string(R"(
        struct Counter { mut n: i32 }
        impl Counter {
            fn next(mut self) -> i32? {
                if self.n <= 0 { return null; }
                self.n = self.n - 1;
                self.n
            }
        }
        pub fn sum() -> i32 {
            let c = Counter { n: 2 };
            let mut s = 0;
            for x in c {
                s = s + x;
            }
            s
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find(".next()"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("== nullptr"), std::string::npos);
}

TEST(Codegen, NamedFnValueCast) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn inc(x: i32) -> i32 { x + 1 }
        pub fn apply(f: fn(i32) -> i32, x: i32) -> i32 { f(x) }
        pub fn run() -> i32 { apply(inc, 4) }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("static_cast<"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("inc"), std::string::npos);
}

TEST(Codegen, TypeParamPackTemplate) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn count<...T>() -> i32 { 0 }
        pub fn run() -> i32 { count<i32, i32>() }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("typename... T"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("count<std::int32_t, std::int32_t>"), std::string::npos);
}

TEST(Codegen, PackExpandFnAndTuple) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn apply<...Cs>(f: fn(Cs...) -> i32, ...xs: Cs) -> i32 { f(xs...) }
        pub fn as_tuple<...Cs>(...xs: Cs) -> (Cs...) { xs... }
        pub fn add(a: i32, b: i32) -> i32 { a + b }
        pub fn run() -> i32 {
            let t = as_tuple<i32, i32>(3, 4);
            apply<i32, i32>(add, 1, 2) + t.0
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("Cs..."), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("f(xs...)"), std::string::npos);
    EXPECT_NE(compiled.result.output.header.find("std::make_tuple(xs...)"), std::string::npos);
}

TEST(Codegen, MutForAndMutParamRef) {
    auto compiled = qpc::test::compile_string(R"(
        pub fn bump(mut n: i32) { n = n + 1; }
        pub fn run() -> i32 {
            let mut xs = [1, 2];
            for mut x in xs { x = x + 1; }
            let mut pairs = [(1, 2)];
            for (mut a, mut b) in pairs { a = a + 1; }
            xs[0]
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.header.find("std::int32_t& n"), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("for (auto& x : "), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("for (auto& [a, b] : "), std::string::npos);
}

TEST(Codegen, MutIteratorNoCopy) {
    auto compiled = qpc::test::compile_string(R"(
        struct Counter { mut n: i32 }
        impl Counter {
            fn next(mut self) -> i32? {
                if self.n <= 0 { return null; }
                self.n = self.n - 1;
                self.n
            }
        }
        pub fn run() -> i32 {
            let mut c = Counter { n: 2 };
            for mut x in c { x = x; }
            0
        }
    )");
    ASSERT_TRUE(compiled.result.ok) << compiled.diags.all().front().message;
    EXPECT_NE(compiled.result.output.source.find("auto&& "), std::string::npos);
    EXPECT_NE(compiled.result.output.source.find("auto& x = *"), std::string::npos);
}
