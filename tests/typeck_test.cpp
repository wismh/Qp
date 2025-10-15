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

TEST(Typeck, ForDictBindings) {
    auto compiled = qpc::test::compile_string(R"(
        fn sum(m: {i32: i32}) -> i32 {
            let mut s = 0;
            for (k, v) in m {
                s = s + k + v;
            }
            s
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, ForDictNeedsPair) {
    auto compiled = qpc::test::compile_string(R"(
        fn sum(m: {i32: i32}) -> i32 {
            let mut s = 0;
            for x in m {
                s = s + x;
            }
            s
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("'(key, value)'"), std::string::npos);
}

TEST(Typeck, ForPairOnListIsError) {
    auto compiled = qpc::test::compile_string(R"(
        fn sum(xs: [i32]) -> i32 {
            let mut s = 0;
            for (k, v) in xs {
                s = s + k;
            }
            s
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("2-tuple element"), std::string::npos);
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

TEST(Typeck, GenericMethodRefCallback) {
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
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, AssociatedCallbackInfersTypeArgs) {
    auto compiled = qpc::test::compile_string(R"(
        trait Component {}
        struct Transform { x: i32 }
        struct Sprite { id: i32 }
        struct World {}
        impl Component for Transform {}
        impl Component for Sprite {}
        impl World {
            fn for_each<T: Component, U: Component>(f: fn(T, U) -> i32) -> i32 { 2 }
        }
        fn run() -> i32 {
            World.for_each(|t: Transform, s: Sprite| t.x + s.id)
        }
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

TEST(Typeck, CoerceValueToNullable) {
    auto compiled = qpc::test::compile_string(R"(
        fn wrap(n: i32) -> i32? { n }
        fn take(p: i32?) -> i32 { if let v = p { v } else { 0 } }
        fn run() -> i32 { take(7) }
        fn either(flag: bool) -> i32? { if flag { 1 } else { null } }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, CoerceNullableMismatchIsError) {
    auto compiled = qpc::test::compile_string("fn f(x: i32) -> f32? { x }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("expected"), std::string::npos);
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

TEST(Typeck, NestedModuleTypesResolve) {
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
        fn run() -> i32 {
            make().get_id()
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, NestedModulePathType) {
    auto compiled = qpc::test::compile_string(R"(
        mod ecs {
            pub struct World { id: i32 }
        }
        fn run(w: ecs::World) -> i32 { w.id }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, UseImportsModuleStatic) {
    auto compiled = qpc::test::compile_string(R"(
        mod ecs {
            pub let mut hits: i32 = 1;
            pub fn bump() { hits = hits + 1; }
        }
        use ecs::*;
        fn run() -> i32 {
            bump();
            hits = hits + 1;
            hits
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, ModuleStaticPathAccess) {
    auto compiled = qpc::test::compile_string(R"(
        mod ecs {
            pub let mut hits: i32 = 1;
        }
        fn run() -> i32 {
            ecs::hits = ecs::hits + 2;
            ecs::hits
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, NestedModuleEnumUse) {
    auto compiled = qpc::test::compile_string(R"(
        mod gfx {
            pub enum Color { Red, Green }
            pub fn red() -> Color { Color::Red }
        }
        use gfx::*;
        fn run() -> Color { red() }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, NestedExternModuleUse) {
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
        }
        use engine::*;
        fn run() -> i32 { world.step() + host_tick() }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, ToStringBuiltin) {
    auto compiled = qpc::test::compile_string(R"(
        enum Color { Red, Green }
        fn run(n: i32, flag: bool, c: Color, s: string) -> string {
            to_string(n) + to_string(flag) + to_string(c) + to_string(s) + to_string('x')
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, ToStringWrongTypeIsError) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { x: i32 }
        fn f(p: Point) -> string { to_string(p) }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot convert"), std::string::npos);
}

TEST(Typeck, ToStringArityIsError) {
    auto compiled = qpc::test::compile_string("fn f() -> string { to_string() }");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("expects 1"), std::string::npos);
}

TEST(Typeck, StringInterpolation) {
    auto compiled = qpc::test::compile_string(R"(
        fn status(hp: i32) -> string { "hp = ${hp}" }
        fn greet(name: string) -> string { "hi ${name}" }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, FunctionOverloadByType) {
    auto compiled = qpc::test::compile_string(R"(
        fn abs(x: i32) -> i32 { if x < 0 { -x } else { x } }
        fn abs(x: f32) -> f32 { if x < 0.0 { -x } else { x } }
        fn run() -> i32 { abs(-3) }
        fn runf() -> f32 { abs(-2.5) }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, FunctionOverloadByArity) {
    auto compiled = qpc::test::compile_string(R"(
        fn add(a: i32) -> i32 { a }
        fn add(a: i32, b: i32) -> i32 { a + b }
        fn run() -> i32 { add(1) + add(2, 3) }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, MethodOverload) {
    auto compiled = qpc::test::compile_string(R"(
        struct Counter { mut n: i32 }
        impl Counter {
            fn bump(mut self) { self.n = self.n + 1; }
            fn bump(mut self, by: i32) { self.n = self.n + by; }
        }
        fn run() -> i32 {
            let mut c = Counter { n: 0 };
            c.bump();
            c.bump(4);
            c.n
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, DuplicateOverloadParamsIsError) {
    auto compiled = qpc::test::compile_string(R"(
        fn f(x: i32) -> i32 { x }
        fn f(x: i32) -> f32 { 0.0 }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("duplicate function"), std::string::npos);
}

TEST(Typeck, AmbiguousOverloadIsError) {
    auto compiled = qpc::test::compile_string(R"(
        fn f(a: [i32]) -> i32 { 1 }
        fn f(a: [f32]) -> i32 { 2 }
        fn run() -> i32 { f([]) }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("ambiguous"), std::string::npos);
}

TEST(Typeck, ExternCOverloadIsError) {
    auto compiled = qpc::test::compile_string(R"(
        extern "C" {
            fn f(x: i32) -> i32;
            fn f(x: i64) -> i64;
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot overload extern \"C\""), std::string::npos);
}

TEST(Typeck, TupleLiteralAndIndex) {
    auto compiled = qpc::test::compile_string(R"(
        fn f() -> i32 {
            let p: (i32, i32) = (2, 3);
            p.0 + p.1
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, TupleArityMismatchIsError) {
    auto compiled = qpc::test::compile_string(R"(
        fn f() -> i32 {
            let p: (i32, i32) = (1, 2, 3);
            p.0
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("expected '(i32, i32)'"), std::string::npos);
}

TEST(Typeck, ForUnpacksTupleList) {
    auto compiled = qpc::test::compile_string(R"(
        fn sum(xs: [(i32, i32)]) -> i32 {
            let mut s = 0;
            for (a, b) in xs {
                s = s + a + b;
            }
            s
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, CustomIteratorFor) {
    auto compiled = qpc::test::compile_string(R"(
        struct Counter { mut n: i32 }
        impl Counter {
            fn next(mut self) -> i32? {
                if self.n <= 0 { return null; }
                self.n = self.n - 1;
                self.n
            }
        }
        fn sum() -> i32 {
            let c = Counter { n: 3 };
            let mut s = 0;
            for x in c {
                s = s + x;
            }
            s
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, CustomIteratorUnpack) {
    auto compiled = qpc::test::compile_string(R"(
        struct Query<A, B> { mut i: i32, n: i32, a: [A], b: [B] }
        impl Query<A, B> {
            fn next(mut self) -> (A, B)? {
                if self.i >= self.n { return null; }
                let item = (self.a[self.i], self.b[self.i]);
                self.i = self.i + 1;
                item
            }
        }
        fn sum() -> i32 {
            let q = Query { i: 0, n: 1, a: [2], b: [3] };
            let mut s = 0;
            for (t, b) in q {
                s = s + t + b;
            }
            s
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, StructWithoutNextIsNotIterable) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { x: i32 }
        fn f(p: Point) -> i32 {
            for x in p { }
            0
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("iterator"), std::string::npos);
}

TEST(Typeck, NamedFnAsValue) {
    auto compiled = qpc::test::compile_string(R"(
        fn inc(x: i32) -> i32 { x + 1 }
        fn apply(f: fn(i32) -> i32, x: i32) -> i32 { f(x) }
        fn run() -> i32 { apply(inc, 4) }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, NamedFnLetBinding) {
    auto compiled = qpc::test::compile_string(R"(
        fn inc(x: i32) -> i32 { x + 1 }
        fn run() -> i32 {
            let f = inc;
            f(4)
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, GenericFnValueNeedsExpectedType) {
    auto compiled = qpc::test::compile_string(R"(
        fn id<T>(x: T) -> T { x }
        fn run() -> i32 {
            let f = id;
            f(1)
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot infer"), std::string::npos);
}

TEST(Typeck, GenericFnValueFromAnnotation) {
    auto compiled = qpc::test::compile_string(R"(
        fn id<T>(x: T) -> T { x }
        fn run() -> i32 {
            let f: fn(i32) -> i32 = id;
            f(1)
        }
    )");
    EXPECT_TRUE(compiled.result.ok) << first_error(compiled.diags);
}

TEST(Typeck, OverloadedFnValueNeedsAnnotation) {
    auto compiled = qpc::test::compile_string(R"(
        fn abs(x: i32) -> i32 { x }
        fn abs(x: f32) -> f32 { x }
        fn run() -> i32 {
            let f = abs;
            f(1)
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot infer"), std::string::npos);
}

TEST(Typeck, MethodWithSelfIsNotAValue) {
    auto compiled = qpc::test::compile_string(R"(
        struct Point { x: i32 }
        impl Point {
            fn mag(self) -> i32 { self.x }
        }
        fn run() -> i32 {
            let f = Point::mag;
            0
        }
    )");
    EXPECT_FALSE(compiled.result.ok);
    EXPECT_NE(first_error(compiled.diags).find("cannot use method"), std::string::npos);
}
