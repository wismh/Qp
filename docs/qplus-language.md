# Q+ Language Design

**Q+** (QPlus). Файли: `.qp`. Компілятор: `qpc`.

Q+ — універсальна скриптова мова з синтаксисом, близьким до Rust, без ownership і lifetime. Null дозволений через `T?`. Поліморфізм — через `trait` + `impl`, без ієрархії класів.

Це специфікація ядра мови і трьох шляхів компіляції. Без прив’язки до конкретного рантайму чи рушія.

---

## 1. Цілі

1. Писати код швидше і безпечніше, ніж на C++, без borrow checker.
2. Поверхня як у Rust: `fn`, `let`/`mut`, `struct`/`impl`, `match`, модулі.
3. Три практичні артефакти з одного фронтенда:
   - **C++ source** → далі звичайний C++ компілятор у native desktop;
   - **той самий C++** → WASM (Emscripten / clang);
   - **LLVM JIT** — editor і debug, hot reload.

### Чого немає в ядрі (v0)

Ownership, lifetimes, ручний `free`, спадкування класів, макроси рівня Rust, препроцесор, винятки як основний контроль потоку.

Все, що було б фреймворком (сервіси, контейнери, ECS, скрипти на сутностях) — не частина мови. Це можна пізніше надбудувати бібліотекою.

---

## 2. Три бекенди, один фронтенд

```
.qp ──► lexer ──► parser ──► HIR ──► typeck
                                      │
                    ┌─────────────────┼─────────────────┐
                    ▼                 ▼                 ▼
              CppBackend        LlvmJitBackend     (немає окремого
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

Фронтенд (лексер, парсер, HIR, перевірка типів) спільний. Розходяться лише кодогенератори.

### 2.1 AOT: Q+ → C++

Основний шлях для релізу.

```
qpc build src/ -o gen/          # згенерувати C++
clang++ gen/*.cpp runtime/*.cpp -o app
```

Чому C++, а не одразу LLVM object:

- native desktop збирається **тим самим** тулчейном, що й решта проєкту (MSVC / clang / gcc);
- WASM не потребує другого кодогенератора Q+;
- згенерований C++ можна читати, ставити breakpoint (з `#line` назад у `.qp`);
- ABI з існуючим C++ кодом — натуральний (типи, calling convention, лінковка).

Кодогенерація має бути детермінованою і відносно читабельною. Імена: `qplus::mod_name::TypeName`. Файли: один `.qp` модуль → пара `.h` / `.cpp` (або `.hpp` за політикою проєкту).

`#line 42 "src/math.qp"` на кожній згенерованій функції, щоб дебагер показував Q+.

### 2.2 WASM: C++ → wasm, не Q+ → wasm

Окремий бекенд Q+ → WASM **не робимо**, поки не доведено, що C++-шлях не тягне.

```
qpc build src/ -o gen/
emcc gen/*.cpp runtime/*.cpp -o app.wasm
# або
clang++ --target=wasm32-wasi ...
```

Обмеження WASM (лінійна пам’ять, немає типових винятків C++, інший лінкер) живуть у **рантаймі** (`runtime/wasm/`), не в синтаксисі мови. Мова одна.

Якщо колись знадобиться тонший wasm (менший runtime, GC proposal) — це третій бекенд від того ж HIR. Не зараз.

### 2.3 JIT: LLVM для editor / debug / hot reload

Релізний C++ не вміє підміняти функції на льоту. Для цього — окремий LLVM-бекенд:

```
HIR ──► LLVM IR ──► ORC JIT ──► адреса функції в процесі редактора
```

Редактор і debug-сесія **виконують Q+ через JIT**, а не через попередньо зібраний C++. Hot reload:

| Зміна | Поведінка |
|---|---|
| Тіло `fn` / методу | Перекомпілювати функцію, замінити в JIT. Стан живий. |
| Нова `fn` у модулі | Додати символ. |
| Нове поле `struct` / зміна layout | Неможливо безпечно на живих об’єктах → повний reload модуля або сесії. |
| Зміна сигнатури `fn` | Reload модуля; старі call site невалідні. |
| Зміна `trait` | Reload усіх impl. |

JIT і C++-бекенд зобов’язані дотримуватись **одного ABI** для runtime-типів (`qplus::String`, `Vec<T>`, object header GC), щоб хост на C++ міг викликати JIT-код і навпаки.

`qpc` у режимі editor тримає інкрементальний граф модулів: змінився файл → typeck модуля та імпортерів → codegen лише брудних `fn`.

### 2.4 Рантайм (C++)

Одна бібліотека, лінкується і до AOT, і до JIT-хоста:

- аллокатор / GC (або ARC + cycle check — рішення в §16);
- `String`, `Vec<T>`, `Map<K,V>`, `Set<T>`;
- panic (стек, повідомлення, `#line`);
- nullable ptr helpers;
- точка входу JIT: `qplus_jit_lookup("mod.fn")`.

Без цього згенерований C++ не самодостатній.

---

## 3. Принципи мови

**Явний null.** `T` не буває `null`. `T?` буває. Немає «всі посилання nullable».

**struct — дані, impl — поведінка.** Немає `class`, конструкторів-монстрів і спадкування.

**Значення копіюються, посилання — окремий тип.** Примітиви, `struct`, `enum` — value types. Купа — через `T?` / `new` (див. §5.2).

**Останній вираз блока — його значення.** `return` теж є.

---

## 4. Лексика

Коментарі: `//`, `/* */`. Ідентифікатори Unicode. Конвенція: `snake_case` для значень і функцій, `PascalCase` для типів. Ключові слова англійською.

Рядки: `"hello"`, інтерполяція `"hp = ${hp}"`, сирі `#"path\raw"#`.

Числа: `10`, `10_000`, `0xFF`, `3.14` (default **`f32`**), `3.14f64`, суфікси `i32`/`u64`.

```qp
fn clamp(x: f32, a: f32, b: f32) -> f32 {
    if x < a { a } else if x > b { b } else { x }
}
```

---

## 5. Типи

### 5.1 Примітиви

| Тип | Опис |
|---|---|
| `bool` | `true` / `false` |
| `i8` `i16` `i32` `i64` | знакові |
| `u8` `u16` `u32` `u64` | беззнакові |
| `f32` `f64` | float; літерал `3.0` → `f32` |
| `char` | Unicode scalar |
| `string` | UTF-8, immutable |
| `()` | unit |
| `!` | never |

Інференс локальний. Сигнатури `fn`, публічних полів і `pub`-елементів — обов’язкові.

```qp
let hp = 100;            // i32
let speed = 6.0;         // f32
let speed: f64 = 6.0;
let name = "Ada";        // string
```

### 5.2 Null і купа

`null` лише для `T?`.

```qp
let p: Point? = null;
let p = new Point { x: 1.0, y: 2.0 };   // Point?
let stack = Point { x: 1.0, y: 2.0 };   // Point (value)

let a: Point = p;        // помилка компіляції
let a = p!;              // panic, якщо null
let a = p ?? stack;      // fallback, тип Point
let x = p?.x;            // f32?
```

| Синтаксис | Значення |
|---|---|
| `T?` | nullable посилання на heap-`T` |
| `new T { ... }` | алокація, результат `T?` (або `T`, якщо колись додамо non-null new — не в v0) |
| `x?.field` / `x?.method()` | null-safe; результат `U?` |
| `x ?? y` | якщо `x` null — `y` |
| `x!` | assert non-null |
| `x?` | propagate null з функції, що повертає `U?` |
| `if let v = x { }` | у гілці `v: T` |

Неявне `T` → `T?` немає: value і reference — різні. Щоб покласти value на купу — `new`.

`T?` у C++: `T*` + контракт; у рантаймі об’єкт має header GC. Value `T` у C++: `struct T` by value.

### 5.3 `struct`

Іменований продукт. Value type: присвоєння копіює поля.

```qp
pub struct Vec2 {
    mut x: f32,
    mut y: f32,
}

impl Vec2 {
    pub const ZERO: Vec2 = Vec2 { x: 0.0, y: 0.0 };

    pub fn length(self) -> f32 {
        (self.x * self.x + self.y * self.y).sqrt()
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

- Поля за замовчуванням іммутабельні; `mut` — можна писати.
- `self` — копія отримувача.
- `mut self` — ексклюзивний view, зміни видимі викликачеві (для value — як `&mut self` у Rust, без lifetime: передається вказівник у згенерованому C++).
- Немає спадкування `struct`.
- Update: `Vec2 { x: 1.0, ..v }`.

У C++:

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

Звичайний C-подібний перелік іменованих констант. Без полів.

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

У C++: `enum class Color : std::int32_t { Red = 0, Green = 2, Blue = 3 };`. Значення: `Color::Red`.

Якщо потрібні поля — `variant`.

### 5.4.1 `variant`

ADT, як `enum` у Rust.

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

У C++: `std::variant` внутрішніх структур.

### 5.5 Колекції

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
| `(A, B)` | `std::tuple` або struct |

### 5.6 Generics і `trait`

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
```

Мономорфізація в C++ (шаблони) і в LLVM JIT (копія функції на набір типів). Dynamic dispatch: `dyn Trait` — fat pointer `(data*, vtable*)`, якщо знадобиться; у v0 можна лише static `T: Trait`.

Associated types:

```qp
pub trait Pool {
    type Item;
    fn take(mut self) -> Self.Item?;
}
```

### 5.7 `type` і `Result`

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

`?` на `Result` прокидає `Err`. `?` на `T?` прокидає `null`. В одній `fn` не змішувати без явного типу повернення, який це дозволяє (`Result<T?, E>`).

---

## 6. Змінні і контроль потоку

```qp
let x = 1;
let mut y = 2;
y = 3;

const MAX: i32 = 4;

if y > 2 { ... } else { ... }

while y > 0 { y -= 1; }

for i in 0..10 { ... }
for item in xs { ... }
for (i, item) in xs.enumerate() { ... }

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

Параметр `mut x: T` — локально змінний. `mut self` — див. §5.3.

---

## 7. Функції, видимість, модулі

```qp
pub fn min(a: i32, b: i32) -> i32 {
    if a < b { a } else { b }
}
```

Видимість: за замовчуванням модуль; `pub` — зовні.

```qp
mod math;
mod util { pub fn id<T>(x: T) -> T { x } }

use math::Vec2;
use util::*;
```

Модуль → C++ namespace. `use` не впливає на ABI, лише на імена в Q+.

`extern` — оголошення символу, який дає хост (C++):

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

`extern "C"` — лише вільні функції з C ABI. `extern` без ABI — C++ runtime Q+ (`qplus::...`). Тіло пишеться на C++, не в `.qp`.

`struct Test;` у `extern` — непрозорий тип хоста: Q+ не знає полів і не генерує `struct`. Методи з `impl` теж лише прототипи; виклик `obj.add(3, 5)` іде в C++ як `obj.add<std::int32_t>(3, 5)` (аргументи виводять `T`, або пишеться `add<i32>(...)`). `let` без `=` — `extern` глобаль хоста. Повний тип хост кладе в `qplus_host.h` на include path, у `namespace qplus`.

---

## 8. Паніка і безпека

У скриптах немає UB. Порушення — `panic`.

| Ситуація | Результат |
|---|---|
| `x!` коли `x == null` | panic |
| індекс за межами `[T]` | panic |
| ділення `i32` на 0 | panic |
| ділення `f32` на 0 | IEEE |
| overflow `i32` у debug | panic; у release — wrapping (як `i32` у LLVM) — зафіксувати в реалізації |

У згенерованому C++ `panic` — функція рантайму, не необроблений C++ exception крізь FFI (можна всередині `throw` і ловити на межі модуля). У WASM — `abort` або JS-host trap, залежно від рантайму.

JIT у debug: panic показує Q+ стек через debug info LLVM.

---

## 9. Відповідність Q+ → C++ (конспект)

| Q+ | C++ |
|---|---|
| `mod foo` | `namespace foo` |
| `struct S { x: i32 }` | `struct S { int32_t x; }` |
| `impl S { fn f(self) }` | `S S::f() const` |
| `fn f(mut self)` | `S::f()` non-const / `S&` |
| `T?` | `T*` |
| `new T { }` | `qplus::alloc<T>(...)` |
| `string` | `qplus::String` |
| `[T]` | `qplus::List<T>` |
| `[T; N]` | `qplus::Array<T, N>` |
| `{K: V}` | `qplus::Dict<K, V>` |
| `enum E { A, B }` | `enum class E` |
| `variant E { A, B { x } }` | `std::variant` tagged union |
| `extern { fn f(); }` | declaration in `qplus::`, body in the host |
| `extern { struct T; impl T { fn f(self); } let x: T; }` | host type + methods + `extern T x;` |
| `extern "C" { fn f(); }` | `extern "C"` declaration, body in the host |
| `trait T` + `impl` | концепт / шаблон, або vtable для `dyn` |
| `fn foo<T: Add>` | `template<typename T> requires ...` |
| `match` | `switch` + accessors union |
| `null` | `nullptr` |
| `panic` | `qplus::panic(...)` |

Генератор не використовує винятки C++ у публічному ABI Q+-функцій.

---

## 10. Приклад

```qp
mod geom;

pub struct Vec2 {
    mut x: f32,
    mut y: f32,
}

impl Vec2 {
    pub fn length(self) -> f32 {
        (self.x * self.x + self.y * self.y).sqrt()
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

## 11. Ключові слова (v0)

```
as async break const continue else enum extern false fn for
if impl in let loop match mod mut new null pub return struct
trait true type use variant while
```

`async` зарезервовано, у v0 не реалізується.

Контекстні: `where`, `for` (у `impl Trait for Type`), `dyn`.

---

## 12. Етапи реалізації

Порядок важливіший за полноту синтаксису.

1. **Лексер + парсер** підмножини: `fn`, `let`, `struct`, `impl`, `if`/`while`/`return`, виклики, літерали.
2. **HIR + typeck** для примітивів і `struct`.
3. **CppBackend**: функції на `i32`/`f32` і value-`struct` → `.cpp`, збірка clang++/MSVC у exe. Перший milestone: `qpc` компілює `fn add(a: i32, b: i32) -> i32` і лінкується з `main.cpp`.
4. **Рантайм мінімум:** `panic`, `string`, `[T]`.
5. **`enum` / `variant` + `match`, `T?` + `new`, GC/ARC.**
6. **Модулі, `use`, `extern`.**
7. **`trait` + generics** (мономорфізація).
8. **LlvmJitBackend** на тому ж HIR: виконати `fn` без C++ compile step.
9. **Hot reload** тіл функцій у JIT (однакова сигнатура).
10. **WASM** як CI-ціль: той самий `gen/*.cpp` + emcc. Підрізати runtime.

JIT не блокує крок 3. C++-шлях — джерело правди для семантики; JIT має збігатися по тестах (однакові `.qp` → однаковий результат).

---

## 13. Відкриті рішення

1. **GC vs ARC.** Для JIT і WASM простіше стартувати з ARC + заборона циклів (або cycle detector пізніше). AOT C++ тоді генерує `qplus::Rc<T>`. Зафіксувати до кроку 5.
2. **`new` повертає `T?` чи non-null `T` на купі.** Зараз `T?`. Non-null heap-ref (`Box<T>` / окремий тип) можна додати не ламаючи `T?`.
3. **Overflow цілих** у release: wrapping vs panic. Пропозиція: wrapping, як LLVM `add`.
4. **`dyn Trait` у v0 чи лише мономорфізація.** Пропозиція: лише мономорфізація, поки не знадобиться гетерогенний список.
5. **Імена артефактів:** мова Q+, крейт/компілятор `qpc`, runtime `libqplus`, namespace `qplus`.

Інваріанти: Rust-подібна поверхня, `struct`/`impl` як основа, null через `T?`, AOT = C++, WASM з того ж C++, hot reload = LLVM JIT.
