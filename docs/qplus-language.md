# Q+ Language Design

User guide: [`qplus.md`](qplus.md). This file is the core language and compiler spec.

**Q+** (QPlus). Files: `.qp`. Compiler: `qpc`.

Q+ is a general-purpose scripting language with Rust-like syntax and no ownership or lifetimes. Null is allowed through `T?`. Polymorphism is `trait` + `impl`, not class inheritance.

This is the spec for the language core and three compilation paths. It is not tied to a particular runtime or engine.

---

## 1. Goals

1. Write code faster and safer than C++, without a borrow checker.
2. A Rust-like surface: `fn`, `let`/`mut`, `struct`/`impl`, `match`, modules.
3. Three practical artifacts from one frontend:
   - **C++ source** → a normal C++ compiler for native desktop;
   - **the same C++** → WASM (Emscripten / clang);
   - **LLVM JIT** — editor, debug, hot reload.

### Out of core (v0)

Ownership, lifetimes, manual `free`, class inheritance, Rust-level macros, a preprocessor, exceptions as the main control-flow tool.

Framework pieces (services, containers, ECS, entity scripts) are not part of the language. Those can be a library later.

---

## 2. Three backends, one frontend

```
.qp ──► lexer ──► parser ──► HIR ──► typeck
                                      │
                    ┌─────────────────┼─────────────────┐
                    ▼                 ▼                 ▼
              CppBackend        LlvmJitBackend     (no separate
              .h / .cpp          LLVM IR + JIT      WasmBackend)
                    │                 │
                    ▼                 ▼
         cl / clang++ / gcc      editor process
                    │            hot reload fn-body
          ┌─────────┴─────────┐
          ▼                   ▼
     native exe/lib      emcc / clang
                         --target=wasm32
                              ▼
                            .wasm
```

The frontend (lexer, parser, HIR, type checking) is shared. Only code generators diverge.

### 2.1 AOT: Q+ → C++

The main release path.

```
qpc build src/ -o gen/          # emit C++
clang++ gen/*.cpp runtime/*.cpp -o app
```

Why C++, not LLVM objects first:

- native desktop uses **the same** toolchain as the rest of the project (MSVC / clang / gcc);
- WASM does not need a second Q+ code generator;
- generated C++ is readable; breakpoints work (with `#line` back to `.qp`);
- ABI with existing C++ is natural (types, calling convention, linking).

Codegen must be deterministic and reasonably readable. Names: `qplus::mod_name::TypeName`. Files: one `.qp` module → a `.h` / `.cpp` pair (or `.hpp` by project policy).

`#line 42 "src/math.qp"` on each generated function so the debugger shows Q+.

### 2.2 WASM: C++ → wasm, not Q+ → wasm

Do **not** add a Q+ → WASM backend until the C++ path is proven not to be a burden.

```
qpc build src/ -o gen/
emcc gen/*.cpp runtime/*.cpp -o app.wasm
# or
clang++ --target=wasm32-wasi ...
```

WASM limits (linear memory, no typical C++ exceptions, a different linker) live in the **runtime** (`runtime/wasm/`), not in the language syntax. The language is one.

If a thinner wasm is needed later (smaller runtime, GC proposal), that is a third backend from the same HIR. Not now.

### 2.3 JIT: LLVM for editor / debug / hot reload

Release C++ cannot replace functions on the fly. That needs a separate LLVM backend:

```
HIR ──► LLVM IR ──► ORC JIT ──► function address in the editor process
```

The editor and debug session **run Q+ through the JIT**, not through prebuilt C++. Hot reload:

| Change | Behavior |
|---|---|
| Body of a `fn` / method | Recompile the function, swap it in the JIT. State stays live. |
| New `fn` in a module | Add the symbol. |
| New `struct` field / layout change | Not safe on live objects → full module or session reload. |
| `fn` signature change | Reload the module; old call sites are invalid. |
| `trait` change | Reload every impl. |

JIT and the C++ backend must share **one ABI** for runtime types (`qplus::String`, `Vec<T>`, GC object header) so a C++ host can call JIT code and vice versa.

In editor mode, `qpc` keeps an incremental module graph: a file changes → typeck that module and its importers → codegen only dirty `fn`s.

### 2.4 Runtime (C++)

One library, linked into both AOT and the JIT host:

- allocator / GC (or ARC + cycle check — decided in §13);
- `String`, `Vec<T>`, `Map<K,V>`, `Set<T>`;
- panic (stack, message, `#line`);
- nullable pointer helpers;
- JIT entry: `qplus_jit_lookup("mod.fn")`.

Without this, generated C++ is not self-contained.

---

## 3. Language principles

**Explicit null.** `T` is never `null`. `T?` may be. There is no “every reference is nullable”.

**struct is data, impl is behavior.** No `class`, no constructor monsters, no inheritance.

**Values copy; a reference is a separate type.** Primitives, `struct`, and `enum` are value types. The heap is `T?` / `new` (see §5.2).

**The last expression of a block is its value.** `return` exists too.

---

## 4. Lexical syntax

Comments: `//`, `/* */`. Unicode identifiers. Convention: `snake_case` for values and functions, `PascalCase` for types. Keywords are English.

Strings: `"hello"`, interpolation `"hp = ${hp}"` (each `${expr}` is `to_string(expr)`), raw `#"path\raw"#`.

Numbers: `10`, `10_000`, `0xFF`, `3.14` (default **`f32`**), `3.14f64`, suffixes `i32`/`u64`.

```qp
fn clamp(x: f32, a: f32, b: f32) -> f32 {
    if x < a { a } else if x > b { b } else { x }
}
```

---

## 5. Types

### 5.1 Primitives

| Type | Meaning |
|---|---|
| `bool` | `true` / `false` |
| `i8` `i16` `i32` `i64` | signed |
| `u8` `u16` `u32` `u64` | unsigned |
| `f32` `f64` | float; literal `3.0` → `f32` |
| `char` | Unicode scalar |
| `string` | UTF-8, immutable |
| `()` | unit |
| `!` | never |

Inference is local. Signatures of `fn`s, public fields, and `pub` items are required.

```qp
let hp = 100;            // i32
let speed = 6.0;         // f32
let speed: f64 = 6.0;
let name = "Ada";        // string
```

`as` is an explicit conversion: numeric types, `bool`/`char` to an integer, integer to `char`, and a C-style `enum` to an integer. There is no implicit widening.

```qp
let n: i64 = 1 as i64;
let c: i32 = Color::Red as i32;
```

### 5.2 Null and the heap

`null` is only for `T?`.

```qp
let p: Point? = null;
let p = new Point { x: 1.0, y: 2.0 };   // Point?
let stack = Point { x: 1.0, y: 2.0 };   // Point (value)

let a: Point = p;        // compile error
let a = p!;              // panic if null
let a = p ?? stack;      // fallback, type Point
let x = p?.x;            // f32?
```

| Syntax | Meaning |
|---|---|
| `T?` | nullable reference to a heap `T` |
| `new T { ... }` | allocate; result is `T?` (or `T` if non-null `new` is added later — not in v0) |
| `x?.field` / `x?.method()` | null-safe; result is `U?` |
| `x ?? y` | if `x` is null, `y` |
| `x!` | assert non-null |
| `x?` | propagate null from a function that returns `U?` |
| `if let v = x { }` | in the branch, `v: T` |

There is an implicit `T` → `T?`: the value is copied onto the Q+ heap (`nullable_of` / `alloc`). Prefer `new` when constructing a struct in one step. `T?` → `T` stays explicit (`!`, `??`, `if let`).

`T?` in C++: `T*` plus a contract. `null` is `nullptr`. `p!` is `unwrap(p)` (abort if null). `new T { }` is `qplus::alloc(T{...})`. A value `T` where `T?` is expected is `qplus::nullable_of(T{...})`. Value `T` in C++: `struct T` by value.

v0 GC is conservative mark-sweep over `alloc` objects. Host C++ must keep live `T*` on the stack (or they may be collected).

### 5.3 `struct`

A named product. Value type: assignment copies fields.

```qp
pub struct Vec2 {
    mut x: f32,
    mut y: f32,
}

impl Vec2 {
    pub const ZERO: Vec2 = Vec2 { x: 0.0, y: 0.0 };

    pub fn length(self) -> f32 {
        sqrt(self.x * self.x + self.y * self.y)
    }

    pub fn add(self, other: Vec2) -> Vec2 {
        Vec2 { x: self.x + other.x, y: self.y + other.y }
    }

    pub fn scale(mut self, s: f32) {
        self.x *= s;
        self.y *= s;
    }
}
```

- Fields are immutable by default; `mut` allows writes.
- `self` is a copy of the receiver.
- `mut self` is an exclusive view; changes are visible to the caller (for values, like Rust `&mut self` without lifetimes: a pointer in the generated C++).
- No `struct` inheritance.
- Update: `Vec2 { x: 1.0, ..v }`.

In C++:

```cpp
struct Vec2 {
    float x;
    float y;
    float length() const;
    Vec2 add(Vec2 other) const;
    void scale(float s);
};
```

### 5.4 `enum`

A C-style list of named constants. No fields.

```qp
pub enum Color {
    Red,
    Green = 2,
    Blue,
}

fn is_red(c: Color) -> bool {
    match c {
        Red => true,
        _ => false,
    }
}
```

In C++: `enum class Color : std::int32_t { Red = 0, Green = 2, Blue = 3 };`. Values: `Color::Red`.

If you need fields, use `variant`.

### 5.4.1 `variant`

An ADT, like Rust `enum`.

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

In C++: `std::variant` of inner structs.

### 5.5 Collections

```qp
let xs: [i32] = [1, 2, 3];     // Vec
let buf: [i32; 4] = [0, 0, 0, 0];
let pair: (f32, f32) = (1.0, 2.0);
let map: {string: i32} = {"hp": 10};
```

| Q+ | C++ runtime |
|---|---|
| `[T]` | `qplus::List<T>` (`std::vector`) |
| `[T; N]` | `qplus::Array<T, N>` (`std::array`) |
| `{K: V}` | `qplus::Dict<K,V>` (`std::map`) |
| `#{T}` | `qplus::Set<T>` |
| `(A, B)` | `std::tuple` or a struct |

### 5.6 Generics and `trait`

```qp
pub trait Add {
    fn add(self, other: Self) -> Self;
}

impl Add for Vec2 {
    fn add(self, other: Vec2) -> Vec2 {
        Vec2 { x: self.x + other.x, y: self.y + other.y }
    }
}

fn twice<T: Add>(x: T) -> T {
    x.add(x)
}

pub struct Pair<T> {
    mut a: T,
    mut b: T,
}

impl Pair<T> {
    fn first(self) -> T {
        self.a
    }

    fn each(self, f: fn(T) -> ()) {
        f(self.a);
        f(self.b);
    }

    fn zip<U>(self, other: U, f: fn(T, U) -> i32) -> i32 {
        f(self.a, other) + f(self.b, other)
    }
}
```

A generic `struct` is monomorphized (`template <typename T> struct Pair`). Write `Pair<i32>` in types. A literal `Pair { a: 1, b: 2 }` infers `T` from the fields; `Pair<i32> { a: 1, b: 2 }` is explicit.

A method may take a callback `fn(...)`. Extra type parameters on the method (`zip<U>`) are inferred from the arguments, including from the callback's `fn` type. There are no C++-style packs; several type parameters is the v0 form of "variadic".

Monomorphization in C++ (templates) and in the LLVM JIT (a copy of the function per type set). Dynamic dispatch: `dyn Trait` is a fat pointer `(data*, vtable*)`. A value of a type that `impl`s the trait coerces to `dyn Trait` at an expected type (arguments, returns, `let` annotations). v0 only dispatches `self` methods, not `mut self`. The payload is copied onto the Q+ heap so the fat pointer stays valid after the call.

```qp
trait Area {
    fn area(self) -> i32;
}

struct Rect { w: i32, h: i32 }
impl Area for Rect {
    fn area(self) -> i32 { self.w * self.h }
}

fn area_of(d: dyn Area) -> i32 {
    d.area()
}
```

Associated types:

```qp
pub trait Pool {
    type Item;
    fn take(mut self) -> Self.Item?;
}
```

### 5.7 `type` and `Result`

```qp
type Meters = f32;

pub variant Result<T, E> {
    Ok(T),
    Err(E),
}

fn parse_i32(s: string) -> Result<i32, string> {
    ...
}

fn load(path: string) -> Result<string, string> {
    let text = read_file(path)?;
    Ok(text)
}
```

`?` on `Result` propagates `Err`. `?` on `T?` propagates `null`. Do not mix them in one `fn` unless the return type allows it (`Result<T?, E>`).

### 5.8 Math

The compiler provides float math as free functions. Argument types are `f32` by default; if any argument is `f64`, the call is `f64`. A user `fn` of the same name shadows the builtin.

Unary: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sqrt`, `abs`, `floor`, `ceil`, `exp`, `ln`, `log2`.
Binary: `atan2`, `fmod`, `pow`.

```qp
fn hypot(x: f32, y: f32) -> f32 {
    sqrt(x * x + y * y)
}

fn wrap(x: f32) -> f32 {
    fmod(x, 360.0)
}
```

There is no separate math package; these names are always in scope.

### 5.9 `to_string`

`to_string(x) -> string` converts a value for display and concatenation. Always in scope; a user `fn` of the same name shadows it.

Accepted arguments: integers, floats, `bool`, `char`, `string`, and C-style `enum` (as its integer discriminant). Other types are a type error.

```qp
fn label(n: i32) -> string {
    "n=" + to_string(n)
}
```

`bool` becomes `"true"` / `"false"`. `string` is returned unchanged. Interpolation (`"hp = ${hp}"`) uses `to_string` on each `${...}` expression.

---

## 6. Variables and control flow

```qp
let x = 1;
let mut y = 2;
y = 3;

const MAX: i32 = 4;

if y > 2 { ... } else { ... }

`return` leaves the enclosing function, including from an `if` branch. Do not treat `if` as a C++ lambda; it is ordinary control flow.

while y > 0 { y -= 1; }

for i in 0..10 { ... }
for item in xs { ... }
for (k, v) in stats { ... }

loop {
    if done { break; }
}

match n {
    0 => "zero",
    1..10 => "small",
    n if n % 2 == 0 => "even",
    _ => "other",
}
```

A `mut x: T` parameter is locally mutable. `mut self` — see §5.3. `for (k, v) in dict` binds the key and value; both are immutable. Tuples and `.enumerate()` are not in v0.

---

## 7. Functions, visibility, modules

```qp
pub fn min(a: i32, b: i32) -> i32 {
    if a < b { a } else { b }
}
```

Visibility: module by default; `pub` is outside.

```qp
mod math;
mod util { pub fn id<T>(x: T) -> T { x } }

use math::Vec2;
use util::*;
```

A module becomes a C++ namespace. `use` does not affect ABI, only names in Q+.

Types declared in a nested module are visible inside that module by their short name (`World`). From outside, write a path (`ecs::World`) or `use ecs::World` / `use ecs::*`. Sibling modules do not see each other's types without `use`.

Module `let` / `let mut` globals (statics) follow the same rule: short name inside the module, `ecs::hits` or `use ecs::hits` / `use ecs::*` from outside.

`extern` / `extern "C"` may live in a nested module (for example `mod engine;` + `use engine::*`). Host symbols still bind in `namespace qplus` under the short name (`qplus::World`, not `qplus::engine::World`).

`mod math { ... }` — body in this file. `mod math;` — a file module:

- next to the compilation root (`app.qp`): `math.qp` or `math/mod.qp`;
- inside module `math`: `math/vec.qp` or `math/vec/mod.qp`.

Both files at once is an error. Neither is an error. A cycle (`a` → `b` → `a`) is an error.

### 7.0 External packages (`packages.toml`)

To pull a module from outside the current tree, put a `packages.toml` in the project (or a parent directory). `qpc` walks upward from the compiled `.qp` and loads the first one it finds.

```toml
[package]
name = "app"

[dependencies]
math = { path = "../libs/math" }
```

`path` is a directory (relative to the `packages.toml` file, or absolute). That directory is the module root: it must contain `mod.qp` or `<name>.qp` (same exclusivity rules as local file modules).

```qp
mod math;          // local first; else packages.toml dependency "math"
use math::*;
```

Local `math.qp` / `math/mod.qp` next to the importer still wins. Nested `mod` inside a package resolve relative to that package root. Versioned registries are not in v0 — only path dependencies.

`extern` declares a symbol the host (C++) provides:

```qp
extern "C" {
    fn puts(s: *u8) -> i32;
}

extern {
    fn log_info(msg: string);

    pub struct Test;
    impl Test {
        pub fn add<T>(self, a: T, b: T) -> T;
    }
    pub let mut test_object: Test;
}

fn demo() -> i32 {
    test_object.add(3, 5)
}
```

`extern "C"` — free functions with the C ABI only. `extern` with no ABI — Q+ C++ runtime (`qplus::...`). The body is written in C++, not in `.qp`.

`struct Test;` in `extern` is an opaque host type: Q+ does not know the fields and does not emit `struct`. Methods in `impl` are prototypes only; `obj.add(3, 5)` becomes `obj.add<std::int32_t>(3, 5)` in C++ (`T` is inferred from arguments, or write `add<i32>(...)`). `let` without `=` is an `extern` host global. The host puts the complete type in `qplus_host.h` on the include path, in `namespace qplus`.

Bindings may be grouped in a nested module and imported:

```qp
mod engine;
use engine::*;

fn tick() -> i32 { world.step() }
```

```qp
// engine.qp
extern {
    pub struct World;
    impl World {
        pub fn step(self) -> i32;
    }
    pub let mut world: World;
}
```

The generated declarations still use `qplus::World` / `qplus::world` (short names), matching the host header.

### 7.1 Closures

```qp
let add = |a: i32, b: i32| a + b;
let f: fn(i32) -> i32 = |x: i32| x + 1;
let k = || 1;
add(1, 2);
(|x: i32| x + 1)(3);

let mut s = 0;
p.each(ref |x: i32| { s = s + x; });
p.zip(4, |x: i32, y: i32| x * y);
```

Parameter types are required. Capture is by copy (`[=]` in C++). `ref |...|` captures by reference (`[&]`) so the closure can assign to outer `mut` bindings. Pass a `ref` closure into a method when the callback must mutate outer `mut` bindings. The type is `fn(T, U) -> R`, mapped to `qplus::Fn<R(T, U)>` (`std::function`).

---

## 8. Panic and safety

Scripts have no UB. Violations `panic`.

| Situation | Result |
|---|---|
| `x!` when `x == null` | panic |
| index out of bounds on `[T]` | panic |
| `i32` division by 0 | panic |
| `f32` division by 0 | IEEE |
| `i32` overflow in debug | panic; in release — wrapping (like LLVM `i32`) — lock this in the implementation |

In generated C++, `panic` is a runtime function, not an uncaught C++ exception across FFI (you may `throw` internally and catch at the module boundary). On WASM — `abort` or a JS-host trap, depending on the runtime.

JIT in debug: panic shows the Q+ stack through LLVM debug info.

---

## 9. Q+ → C++ mapping (summary)

| Q+ | C++ |
|---|---|
| `mod foo` / `mod foo;` | `namespace foo` |
| `struct S { x: i32 }` | `struct S { int32_t x; }` |
| `struct Pair<T>` | `template <typename T> struct Pair` |
| `impl S { fn f(self) }` | `S S::f() const` |
| `fn f(mut self)` | `S::f()` non-const / `S&` |
| `T?` | `T*` |
| `new T { }` | `qplus::alloc<T>(...)` |
| `T` where `T?` expected | `qplus::nullable_of(T)` |
| `string` | `qplus::String` |
| `[T]` | `qplus::List<T>` |
| `[T; N]` | `qplus::Array<T, N>` |
| `{K: V}` | `qplus::Dict<K, V>` |
| `fn(T) -> R` / `|x: T| ...` | `qplus::Fn<R(T)>` (`std::function`) |
| `enum E { A, B }` | `enum class E` |
| `variant E { A, B { x } }` | `std::variant` tagged union |
| `extern { fn f(); }` | declaration in `qplus::`, body in the host |
| `extern { struct T; impl T { fn f(self); } let x: T; }` | host type + methods + `extern T x;` |
| `extern "C" { fn f(); }` | `extern "C"` declaration, body in the host |
| `trait T` + `impl` | concept / template |
| `dyn Trait` | fat pointer `dyn_Trait { data*, vtable* }` |
| `fn foo<T: Add>` | `template<typename T> requires ...` |
| `match` | `switch` + union accessors |
| `null` | `nullptr` |
| `panic` | `qplus::panic(...)` |
| `sin` / `sqrt` / `fmod` / … | `std::sin` / `std::sqrt` / `std::fmod` / … (`<cmath>`) |
| `to_string(x)` | `qplus::to_string(x)` |

The generator does not use C++ exceptions in the public ABI of Q+ functions.

---

## 10. Example

```qp
mod geom;

pub struct Vec2 {
    mut x: f32,
    mut y: f32,
}

impl Vec2 {
    pub fn length(self) -> f32 {
        sqrt(self.x * self.x + self.y * self.y)
    }
}

pub enum Side {
    Left,
    Right,
}

pub fn offset(p: Vec2, side: Side, d: f32) -> Vec2 {
    match side {
        Left => Vec2 { x: p.x - d, y: p.y },
        Right => Vec2 { x: p.x + d, y: p.y },
    }
}

pub fn longest(a: Vec2?, b: Vec2?) -> Vec2? {
    if let va = a {
        if let vb = b {
            if va.length() >= vb.length() { a } else { b }
        } else {
            a
        }
    } else {
        b
    }
}
```

---

## 11. Keywords (v0)

```
as async break const continue dyn else enum extern false fn for
if impl in let loop match mod mut new null pub return struct
trait true type use variant while
```

`async` is reserved and not implemented in v0.

Contextual: `where`, `for` (in `impl Trait for Type`).

---

## 12. Implementation stages

Order matters more than covering every piece of syntax.

1. **Lexer + parser** for a subset: `fn`, `let`, `struct`, `impl`, `if`/`while`/`return`, calls, literals.
2. **HIR + typeck** for primitives and `struct`.
3. **CppBackend**: `i32`/`f32` functions and value `struct`s → `.cpp`, built with clang++/MSVC into an exe. First milestone: `qpc` compiles `fn add(a: i32, b: i32) -> i32` and links with `main.cpp`.
4. **Minimal runtime:** `panic`, `string`, `[T]`.
5. **`enum` / `variant` + `match`, `T?` + `new`, GC/ARC.**
6. **Modules, `use`, `extern`.**
7. **`trait` + generics** (monomorphization).
8. **LlvmJitBackend** on the same HIR: run a `fn` without a C++ compile step.
9. **Hot reload** of function bodies in the JIT (same signature).
10. **WASM** as a CI target: the same `gen/*.cpp` + emcc. Trim the runtime.

JIT does not block step 3. The C++ path is the source of truth for semantics; JIT must match on tests (same `.qp` → same result).

---

## 13. Open decisions

1. **GC vs ARC.** v0 AOT uses conservative mark-sweep (`qplus::alloc` / `qplus::gc_collect`) so `T?` can stay `T*`. JIT/WASM may still switch to ARC + no cycles later.
2. **Does `new` return `T?` or a non-null heap `T`.** Today `T?`. A non-null heap ref (`Box<T>` / a separate type) can be added without breaking `T?`.
3. **Integer overflow** in release: wrapping vs panic. Proposal: wrapping, like LLVM `add`.
4. **Artifact names:** language Q+, crate/compiler `qpc`, runtime `libqplus`, namespace `qplus`.

Invariants: Rust-like surface, `struct`/`impl` as the base, null through `T?`, AOT = C++, WASM from that C++, hot reload = LLVM JIT.
