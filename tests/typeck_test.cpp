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

TEST(Typeck, ExternMethodAndInferredGeneric) {
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
        fn explicit() -> i32 { test_object.add<i32>(1, 2) }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, InferGenericFreeFn) {
    auto compiled = qpc::test::compile_string(R"(
        fn id<T>(x: T) -> T { x }
        fn f() -> i32 { id(1) }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, OpaqueFieldAccessIsError) {
    auto compiled = qpc::test::compile_string(R"(
        extern {
            struct Test;
            let mut x: Test;
        }
        fn f() -> i32 { x.c }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("opaque"), std::string::npos);
}

TEST(Typeck, OpaqueConstructIsError) {
    auto compiled = qpc::test::compile_string(R"(
        extern { struct Test; }
        fn f() -> Test { Test {} }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("opaque"), std::string::npos);
}

TEST(Typeck, CannotInferTypeArgument) {
    auto compiled = qpc::test::compile_string(R"(
        extern {
            struct Test;
            impl Test {
                fn wrap<T>() -> i32;
            }
        }
        fn f() -> i32 { Test.wrap() }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot infer type argument"), std::string::npos);
}

TEST(Typeck, MissingFileModuleIsError) {
    auto compiled = qpc::test::compile_string("mod no_such_qplus_file_mod;");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot find module"), std::string::npos);
}

TEST(Typeck, DuplicateModuleIsError) {
    auto compiled = qpc::test::compile_string("mod a { fn f() {} } mod a { fn g() {} }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("duplicate module"), std::string::npos);
}

TEST(Typeck, ClosureCallAndCapture) {
    auto compiled = qpc::test::compile_string(R"(
        fn apply(f: fn(i32) -> i32, x: i32) -> i32 { f(x) }
        fn twice(n: i32) -> i32 {
            let f = |x: i32| x + x;
            f(n)
        }
        fn capture(n: i32) -> i32 {
            let f = || n;
            f()
        }
        fn go() -> i32 {
            apply(|x: i32| x + 1, 4) + twice(4) + capture(1) + (|x: i32| x)(2)
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, RefCaptureAssign) {
    auto compiled = qpc::test::compile_string(R"(
        fn bump() -> i32 {
            let mut n = 0;
            let f = ref || { n = n + 1; };
            f();
            n
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, CopyCaptureAssignIsError) {
    auto compiled = qpc::test::compile_string(R"(
        fn bump() -> i32 {
            let mut n = 0;
            let f = || { n = n + 1; };
            f();
            n
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("'ref' closure"), std::string::npos);
}

TEST(Typeck, NullableNullAndUnwrap) {
    auto compiled = qpc::test::compile_string(R"(
        fn or_zero(p: i32?) -> i32 {
            if p == null { 0 } else { p! }
        }
        fn none() -> i32? { null }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, GenericStructOk) {
    auto compiled = qpc::test::compile_string(R"(
        struct Pair<T> { mut a: T, mut b: T }
        impl Pair<T> {
            fn first(self) -> T { self.a }
        }
        fn make(x: i32, y: i32) -> Pair<i32> { Pair { a: x, b: y } }
        fn get(p: Pair<i32>) -> i32 { p.first() + p.b }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, GenericStructNeedsArgs) {
    auto compiled = qpc::test::compile_string(R"(
        struct Pair<T> { a: T, b: T }
        fn f(p: Pair) -> i32 { 0 }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("expects 1 type argument"), std::string::npos);
}

TEST(Typeck, NewStructIsNullable) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        fn origin() -> Point? { new Point { x: 0, y: 0 } }
        fn get_x(p: Point?) -> i32 { if p == null { 0 } else { p!.x } }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, TryPropagatesNull) {
    auto compiled = qpc::test::compile_string(R"(
        fn sum(a: i32?, b: i32?) -> i32? {
            if a? + b? == 0 { null } else { a }
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, NullSafeAndCoalesce) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        fn get_x(p: Point?) -> i32? { p?.x }
        fn or_x(p: Point?, d: i32) -> i32 { p?.x ?? d }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, NewCannotImplicitlyUnwrap) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        fn origin() -> Point { new Point { x: 0, y: 0 } }
    )");
    EXPECT_FALSE(compiled.result.ok);
}

TEST(Typeck, IfLetBindsInner) {
    auto compiled = qpc::test::compile_string(
        "fn or_zero(p: i32?) -> i32 { if let v = p { v } else { 0 } }");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, IfLetRequiresNullable) {
    auto compiled = qpc::test::compile_string("fn f(x: i32) -> i32 { if let v = x { v } else { 0 } }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("if-let"), std::string::npos);
}

TEST(Typeck, QuestionDotRequiresNullable) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { mut x: i32, mut y: i32 }
        fn f(p: Point) -> i32? { p?.x }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("'?.'"), std::string::npos);
}

TEST(Typeck, TryRequiresNullableReturn) {
    auto compiled = qpc::test::compile_string("fn f(a: i32?) -> i32 { a? }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("'?'"), std::string::npos);
}

TEST(Typeck, AsCastOk) {
    auto compiled = qpc::test::compile_string(R"(
        enum Color { Red, Green }
        fn widen(x: i32) -> i64 { x as i64 }
        fn trunc(x: f32) -> i32 { x as i32 }
        fn flag(b: bool) -> i32 { b as i32 }
        fn code(c: Color) -> i32 { c as i32 }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, ClosureArityMismatch) {
    auto compiled = qpc::test::compile_string(R"(
        fn f() -> i32 {
            let add = |a: i32, b: i32| a + b;
            add(1)
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("expects 2 argument"), std::string::npos);
}

TEST(Typeck, LocalIsNotCallable) {
    auto compiled = qpc::test::compile_string("fn f() -> i32 { let x = 1; x(2) }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("is not callable"), std::string::npos);
}

TEST(Typeck, ReturnInIfUnifiesWithElse) {
    auto compiled = qpc::test::compile_string(R"(
        fn abs(x: i32) -> i32 {
            if x < 0 {
                return 0 - x;
            }
            x
        }
        fn sign(x: i32) -> i32 {
            if x < 0 {
                return 0 - 1;
            } else if x == 0 {
                return 0;
            } else {
                1
            }
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, AsCastStringIsError) {
    auto compiled = qpc::test::compile_string("fn f(s: string) -> i32 { s as i32 }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot cast"), std::string::npos);
}

TEST(Typeck, NullNeedsNullable) {
    auto compiled = qpc::test::compile_string("fn f() -> i32 { null }");
    EXPECT_FALSE(compiled.result.ok);
}

TEST(Typeck, UnwrapNonNullableIsError) {
    auto compiled = qpc::test::compile_string("fn f(x: i32) -> i32 { x! }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("unwrap"), std::string::npos);
}

TEST(Typeck, DynTraitCoerceAndDispatch) {
    auto compiled = qpc::test::compile_string(R"(
        trait Area {
            fn area(self) -> i32;
        }
        struct Rect { w: i32, h: i32 }
        impl Area for Rect {
            fn area(self) -> i32 { self.w * self.h }
        }
        fn area_of(d: dyn Area) -> i32 { d.area() }
        fn run() -> i32 { area_of(Rect { w: 3, h: 4 }) }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, DynTraitMissingImplIsError) {
    auto compiled = qpc::test::compile_string(R"(
        trait Area {
            fn area(self) -> i32;
        }
        struct Rect { w: i32, h: i32 }
        fn area_of(d: dyn Area) -> i32 { d.area() }
        fn run() -> i32 { area_of(Rect { w: 3, h: 4 }) }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("does not implement"), std::string::npos);
}

TEST(Typeck, DynUnknownTraitIsError) {
    auto compiled = qpc::test::compile_string("fn area_of(d: dyn Area) -> i32 { 0 }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("unknown trait"), std::string::npos);
}

TEST(Typeck, MathBuiltinsOk) {
    auto compiled = qpc::test::compile_string(R"(
        fn hypot(x: f32, y: f32) -> f32 { sqrt(x * x + y * y) }
        fn wrap(x: f32) -> f32 { fmod(x, 360.0) }
        fn trig(x: f32) -> f32 { sin(x) + cos(x) + ln(1.0) }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, MathBuiltinWrongTypeIsError) {
    auto compiled = qpc::test::compile_string("fn f(x: i32) -> f32 { sin(x) }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("argument"), std::string::npos);
}

TEST(Typeck, MathBuiltinArityIsError) {
    auto compiled = qpc::test::compile_string("fn f(x: f32) -> f32 { fmod(x) }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("expects 2"), std::string::npos);
}
