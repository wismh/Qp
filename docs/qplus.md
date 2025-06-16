# Q+

Q+ is a Rust-like language without ownership, lifetimes, or class inheritance. Files use `.qp`. The `qpc` compiler translates them to C++ in the `qplus` namespace.

This guide is how to write programs. Compiler design: [`qplus-language.md`](qplus-language.md).

---

## Quick start

```qp
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}
```

```text
qpc compile app.qp -o gen/
```

That writes `app.h` and `app.cpp` under `gen/`. From C++, call `qplus::add(1, 2)`.

Names: `snake_case` for functions and variables, `PascalCase` for types. Comments: `//` and `/* */`.

---

## Functions and variables

The last expression in a block is the function's value. `return` works too.

```qp
pub fn min(a: i32, b: i32) -> i32 {
    if a < b { a } else { b }
}

fn steps(n: i32) -> i32 {
    let mut i = 0;
    i = i + n;
    return i;
}
```

- `let x = 1` — immutable binding
- `let mut y = 2` — can be assigned again
- `pub` — visible outside the module and from C++
- without `pub` — module-private
- file-level `let` / `let mut` is a global

You can omit the type on `let` when the initializer makes it obvious. Parameter types and `->` are required.

---

## Types

| Type | Example |
|---|---|
| `bool` | `true`, `false` |
| `i8` `i16` `i32` `i64` | `10`, `10i64` |
| `u8` `u16` `u32` `u64` | `1u8` |
| `f32` `f64` | `3.0` is `f32`, `3.0f64` |
| `char` | `'a'` |
| `string` | `"hello"` |
| `()` | unit; you can omit `-> ()` |

Strings concatenate with `+`:

```qp
fn greet(name: string) -> string {
    "hello, " + name
}
```

---

## Control flow

`if` is an expression. Both branches must have the same type when the `if` produces a value.

```qp
fn clamp(x: i32, lo: i32, hi: i32) -> i32 {
    if x < lo { lo } else if x > hi { hi } else { x }
}
```

Comparison: `== != < <= > >=`. Logic: `&& || !`.

```qp
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
    for i in 0..5 {
        s = s + i;
    }
    s
}

fn sum_list(xs: [i32]) -> i32 {
    let mut s = 0;
    for x in xs {
        s = s + x;
    }
    s
}
```

`0..5` is a half-open range: `0, 1, 2, 3, 4`. Loops may use `break` and `continue`.

---

## `struct` and `impl`

A `struct` is data only. Methods go in `impl`.

```qp
pub struct Vec2 {
    mut x: f32,
    mut y: f32,
}

impl Vec2 {
    pub fn add(self, other: Vec2) -> Vec2 {
        Vec2 { x: self.x + other.x, y: self.y + other.y }
    }

    pub fn scale(mut self, s: f32) {
        self.x = self.x * s;
        self.y = self.y * s;
    }

    pub fn origin() -> Vec2 {
        Vec2 { x: 0.0, y: 0.0 }
    }
}

fn demo(a: Vec2, b: Vec2) -> f32 {
    a.add(b).x
}

fn start() -> Vec2 {
    Vec2.origin()
}
```

- a field without `mut` cannot be assigned
- `self` — method on a value; `mut self` — may mutate the receiver
- no `self` — associated function: `Vec2.origin()`
- literal: `Vec2 { x: 1.0, y: 2.0 }`

There is no class inheritance.

---

## `enum` and `variant`

`enum` is a C-style list of integer constants.

```qp
pub enum Color {
    Red,
    Green = 2,
    Blue,
}

fn color_code(c: Color) -> i32 {
    match c {
        Red => 1,
        Green => 2,
        Blue => 3,
    }
}
```

If you need fields, use `variant`:

```qp
pub variant Shape {
    None,
    Circle { r: f32 },
    Rect { w: f32, h: f32 },
}

fn area(s: Shape) -> f32 {
    match s {
        None => 0.0,
        Circle { r } => 3.141592 * r * r,
        Rect { w, h } => w * h,
    }
}
```

`match` must cover every variant.

---

## Collections

```qp
let xs: [i32] = [1, 2, 3];           // list
let buf: [i32; 2] = [10, 20];        // fixed-size array
let hp: i32 = stats["hp"];           // dictionary
let stats: {string: i32} = {"hp": 7};
```

| Syntax | Meaning |
|---|---|
| `[T]` | list |
| `[T; N]` | array of `N` elements |
| `{K: V}` | dictionary |

Reading `xs[i]` or `map[key]` panics if the index or key is missing.

---

## Generics and `trait`

Type parameters go in `<T>`. If `T` appears in the arguments, you can omit it at the call site. Otherwise write it: `id<i32>(1)` or `World.for_each<Transform, Sprite>()`.

```qp
fn id<T>(x: T) -> T {
    x
}

fn same(x: i32) -> i32 {
    id(x)
}
```

A `trait` is a bound on a type, not a class hierarchy.

```qp
trait Component {}

struct Transform { x: i32 }
struct Sprite { id: i32 }
struct World {}

impl Component for Transform {}
impl Component for Sprite {}

impl World {
    fn for_each<T: Component, U: Component>() -> i32 {
        2
    }
}

fn world_count() -> i32 {
    World.for_each<Transform, Sprite>()
}
```

Operators on your types use `impl Add for Point` (and `Sub`, `Mul`, `Neg`, …):

```qp
pub struct Point {
    mut x: i32,
    mut y: i32,
}

impl Add for Point {
    fn add(self, other: Point) -> Point {
        Point { x: self.x + other.x, y: self.y + other.y }
    }
}

fn add_x(a: Point, b: Point) -> i32 {
    (a + b).x
}
```

---

## Modules

A module is a namespace. `use` only affects names in Q+; it does not change the generated C++ ABI.

Inline:

```qp
mod math {
    pub fn min(a: i32, b: i32) -> i32 {
        if a < b { a } else { b }
    }
}

use math::*;
```

A separate file is `mod math;`. Next to the file you compile:

```text
app.qp          →  qpc compile app.qp -o gen/
math.qp         ←  mod math;
math/vec.qp     ←  inside math: mod vec;
util/mod.qp     ←  or a directory with mod.qp:  mod util;
```

Rules:

- `mod math;` looks for `math.qp` **or** `math/mod.qp`
- both files at once is an error
- nested `mod vec;` inside `math` looks for `math/vec.qp` or `math/vec/mod.qp`
- `use math::min;` or `use math::*;`

Example: [`examples/mods.qp`](../examples/mods.qp).

---

## C++: calling Q+ and being called from it

Generated code lives in `namespace qplus`. Host C++ can call `pub fn` and use `pub struct`.

```cpp
#include "vec2.h"

int main() {
    qplus::Vec2 a{.x = 1.0f, .y = 2.0f};
    qplus::Vec2 b{.x = 3.0f, .y = 4.0f};
    auto c = a.add(b);
    return qplus::add_x(a, b) == 4.0f ? 0 : 1;
}
```

For Q+ to call the host, use `extern`. There is no body in the `.qp` file.

```qp
extern "C" {
    fn c_mul(a: i32, b: i32) -> i32;
}

extern {
    fn host_add(a: i32, b: i32) -> i32;
    fn host_greet(name: string) -> string;
}
```

- `extern "C"` — free functions with C ABI types only (`i32`, not `string`)
- `extern { }` — C++ in `qplus::`

Opaque host type, methods, and a global:

```qp
extern {
    pub struct Test;
    impl Test {
        pub fn add<T>(self, a: T, b: T) -> T;
        pub fn created() -> i32;
    }
    pub let mut test_object: Test;
}

fn demo() -> i32 {
    test_object.add(3, 5)
}

fn made() -> i32 {
    Test.created()
}
```

The host puts the complete type in `qplus_host.h` on the include path, inside `namespace qplus`. The generated header includes it. `let` without `=` becomes `extern Test test_object;`; the host provides the definition.

Q+ uses `self`, not `self: Test`. A global is `let mut test_object: Test`, not C++ type-first order.

---

## Compilation

```text
qpc compile <file.qp> -o <dir> [-v]
```

One input `.qp` (and its `mod foo;` files) becomes `<stem>.h` / `<stem>.cpp`. Then build with a normal C++ compiler together with the host.

Generated `.cpp` has `#line` back to the `.qp` file so the debugger shows Q+.

Examples live in [`examples/`](../examples/).

---

## Not yet

These are in the language design, but do not use them yet:

- `T?`, `null`, `new`, `if let`
- closures
- generic `struct`
- a separate package system / a JIT or WASM backend of its own
- string interpolation, tuples `(A, B)`, `Result` / `?`

There is no borrow checker, no classes, and no C++ exceptions in the Q+ public ABI.
