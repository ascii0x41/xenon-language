# Types

## 1. Overview

Xenon is statically typed. Every expression has a type known at compile
time. Types are either **primitive** (built into the language) or
**user-defined** (declared with `type`).

This document specifies:

- Implicit numeric widening.
- Primitive types.
- Composite types.
- Type equality and conversion.
- Copy semantics.

### 1.1 Implicit numeric widening

Implicit conversion is allowed only when it is **widening**,
**same-kind**, and **same-signedness**:

- `uN -> uM` where `M > N` (e.g. `u8 -> u32`)
- `iN -> iM` where `M > N` (e.g. `i16 -> i64`)
- `f32 -> f64`

No other implicit numeric conversion is allowed:

- Narrowing (`u32 -> u8`) is not implicit.
- Signedness change (`u32 -> i32`, `i32 -> u32`) is not implicit.
- Integer to float (`i32 -> f64`) is not implicit.
- Float to integer is not implicit.

These rules are the *only* implicit numeric conversions. They are used
uniformly by arithmetic, comparison, assignment, argument passing, and
`return`.

There is no explicit conversion syntax in v0.1 (see §4.4).

> **Note:** Literal typing is a separate mechanism, not a conversion. An
> unsuffixed `1` does not *convert* to `u8`; it *is* `u8` in a context
> that requires `u8`. See `literals.md`.

> **Open question:** Whether `uN -> iM` is ever an implicit conversion
> (and under exactly what circumstances, e.g. `M > N` only) has not been
> decided. Until it is, it is **not** implicit.

## 2. Primitive types

### 2.1 Integer types

| Type    | Size     | Signed | Range                                         |
|---------|----------|--------|-----------------------------------------------|
| `u8`    | 8 bits   | No     | 0 ..= 255                                     |
| `u16`   | 16 bits  | No     | 0 ..= 65535                                   |
| `u32`   | 32 bits  | No     | 0 ..= 4294967295                              |
| `u64`   | 64 bits  | No     | 0 ..= 18446744073709551615                    |
| `i8`    | 8 bits   | Yes    | -128 ..= 127                                  |
| `i16`   | 16 bits  | Yes    | -32768 ..= 32767                              |
| `i32`   | 32 bits  | Yes    | -2147483648 ..= 2147483647                    |
| `i64`   | 64 bits  | Yes    | -9223372036854775808 ..= 9223372036854775807 |
| `usize` | platform | No     | 0 ..= platform maximum                        |

`u128`, `i128`, and `f128` are planned for future versions.

`usize` is the unsigned integer type that is the size of a platform
address. It follows the unsigned rules everywhere.

#### Overflow behavior

Overflow behavior depends on the build mode:

- **Debug builds:** arithmetic overflow is checked. When overflow
  occurs, the program panics.
- **Release builds:** overflow checks are not guaranteed to be present.
  The result may wrap according to the machine's two's-complement
  semantics. A release build must not be relied upon to panic.

"Panic on overflow" is therefore a property of debug builds, not a
universal guarantee of the language. The purpose of debug builds is to
catch these bugs early.

What a panic *is* is specified in `runtime.md`.

### 2.2 Floating-point types

| Type  | Size   | Standard |
|-------|--------|----------|
| `f32` | 32 bit | IEEE 754 |
| `f64` | 64 bit | IEEE 754 |

Xenon follows IEEE 754 for all floating-point semantics. `-0.0` is
distinct from `0.0`. NaN and ±Inf are representable values.

Literals for NaN and ±Inf are not available in v0.1. They can be produced
via arithmetic but not written directly.

#### Float literal default

Unsuffixed float literals default to `f32`. To request `f64`, use a
suffix or a type annotation:

```xe
let a = 3.14;        // f32
let b = 3.14f64;     // f64
let c: f64 = 3.14;   // f64 (context-fixed)
```

Rationale: Xenon targets systems and embedded contexts where `f32` is
often the native width.

> **Note:** `let a = 0.1; let b: f64 = a;` is legal. `a` is an `f32`,
> and it is explicitly widened when assigned to `b`. The value `a`
> holds has already been rounded to `f32`.

### 2.3 `rune`

A `rune` is a 32-bit Unicode scalar value: an integer in `0..=0x10FFFF`
excluding the surrogate range `0xD800..=0xDFFF`. It represents a single
text character, not a byte.

A `rune` is not a `u32` and does not implicitly convert to or from any
integer type. A `rune` is not a byte; a `str` is UTF-8 bytes, and
`s[i]` yields a `u8`, not a `rune`.

See `literals.md` for rune literal rules.

### 2.4 `bool`

Two values: `true`, `false`. One byte.

### 2.5 `void`

The type of an expression that produces no value. A function with no
return value has return type `void`.

`void` cannot be used as a variable type, a field type, an array element
type, or a parameter type. A `void` expression cannot appear in value
position (see `expressions.md`).

### 2.6 `nulltype`

`nulltype` is the type of the literal `nullptr`. It has exactly one value:
`nullptr`.

- `nulltype` implicitly converts to any pointer type (`*T` or
  `*mut T`).
- It does not convert to any reference type.
- A pointer type never implicitly converts to `nulltype`.
- `nullptr` in a context that does not constrain it to a pointer type
  is an error ("cannot infer pointer type for nullptr").

`nulltype` is a user-visible type. A `nulltype` value cannot be
dereferenced.

```xe
let n: nulltype = nullptr;
```

Pointer comparisons involving `nullptr` are specified in
`expressions.md` §4.2.

### 2.7 References and pointers

There are two families of types for referring to memory:

- `&T` and `&mut T` are references.
- `*T` and `*mut T` are raw pointers.

These are four distinct types with different capabilities. The
conversions between them, how they are created, and how they are used
are specified in `references-and-pointers.md`.

### 2.8 `str`

A `str` is an immutable UTF-8 byte sequence. Conceptually:

```xe
type str {
    bytes:  *u8;
    length: usize;
}
```

> **Note:** This describes the *shape* of `str` for the purposes of
> reasoning about it. The representation is owned by the compiler and
> runtime. Programs cannot construct a `str` by writing a struct
> literal; the only source of a `str` is a string literal (or a `str`
> value passed along from one).

Properties:

- Immutable. You cannot write through a `str`.
- Bytes are always valid UTF-8.
- `bytes` is never null. The empty `str` has `length = 0` and a
  non-null `bytes` pointer.
- Two words. Copying a `str` is a two-word copy.
- `str` does not own its bytes.

In v0.1, every `str` value is backed by a string literal in the
source. Literals live in read-only static data for the entire duration
of the program. Consequently:

- `str` values never dangle.
- There is no allocation for strings.
- There is no concatenation. `a + b` is not defined for `str`.

Building strings is not part of the language in v0.1. An owned string
type (`String`) and concatenation are planned as a library type in a
future version, pending an allocator design.

`str` is not NUL-terminated. Bytes with value `0` are ordinary bytes.

`s[i]` (if defined) yields a `u8`, not a `rune`.

#### Naming convention

Lowercase `str` is the borrowed view type. Uppercase `String` (future,
stdlib) will be the owned type. The case distinction is deliberate.

## 3. Composite types

### 3.1 Static arrays

```xe
[T; N]
```

Where `T` is the element type and `N` is a **non-zero unsigned integer
literal**.

- `N` must be greater than zero. `[T; 0]` is invalid.
- In v0.1, `N` can only be an integer literal. There are no `const`
  declarations and no compile-time evaluation of expressions (`const`
  is planned for v0.2).
- Arrays carry their length in the type. `[u8; 4]` and `[u8; 5]` are
  different types.
- Valid indices are `0 ..= N-1`.
- Array indexing is bounds checked. An out-of-bounds index is a runtime
  failure (a panic), not undefined behavior. See `expressions.md` §2.4.

```xe
let a: [u8; 10] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
let b: [u8; 0];     // error: array length must be greater than zero
let n = 4;
let c: [u8; n];     // error: array length must be an integer literal
```

See `literals.md` §8.1 for array literals.

### 3.2 User-defined types

```xe
type <name> {
    ( <field name>: <type>; )*
}
```

Rules:

- Fields are ordered. Field order is declaration order. No reordering
  is performed by the compiler. Padding and layout are unspecified in
  v0.1.
- Types are **nominal**. Two types with the same shape are distinct
  types. `type A { x: i32; }` and `type B { x: i32; }` are not
  interchangeable.
- A type cannot contain itself directly. Indirect recursion through a
  pointer is allowed.
- A type may be empty: `type Empty {}`.
- Methods and static members are declared in `impl <name>` blocks. See
  `methods.md`.
- Fields are private by default. A field is made visible to other
  modules with `pub`: `pub x: i32;`.
- A struct literal may only initialise fields that are accessible from
  the current module. See `modules.md` §4.2.

## 4. Type equality and conversion

### 4.1 Type equality

Two types are equal if:

- They are the same primitive type (`i32 == i32`).
- They are the same nominal user type (identity by declaration).

Two types are **not** equal even if they have the same layout. Nominal
typing applies throughout.

### 4.2 Value equality

The `==` and `!=` operators are specified in `expressions.md` §4.2. In
summary:

- **Numeric types:** operands are brought to a common type by the
  widening rules in §1.1, then compared by value. Operands with no
  common type are a compile error.
- **`rune`:** compares scalar values.
- **`bool`:** normal boolean equality.
- **`str`:** compares byte content.
- **Pointers and `nullptr`:** compare addresses (see
  `expressions.md` §4.2).
- **User-defined types:** `==` is **not available**. There is no
  operator overloading in v0.1.

Cross-kind comparison (`i32 == f64`, `u32 == i32`, `i32 == f32`) is a
compile error. In v0.1 there is no conversion syntax to make such a
comparison possible, so cross-kind numeric comparison is not
expressible.

Rationale: Xenon's philosophy is explicit over implicit. Cross-kind
comparison hides a conversion that the user should write explicitly.

### 4.3 Literal typing

An unsuffixed integer literal has type `{integer}`. An unsuffixed
float literal has type `{float}`. These metavariables unify with any
integer or float type in context:

```xe
let a: u8 = 1;       // 1 becomes u8
let b: i64 = 1;      // 1 becomes i64
let c = 1;           // 1 defaults to i32 (unconstrained)

let d: f64 = 1.0;    // 1.0 becomes f64
let e = 1.0;         // 1.0 defaults to f32 (unconstrained)
```

If an `{integer}` or `{float}` is still unconstrained after type
inference, it defaults to `i32` or `f32` respectively.

See `literals.md` for suffixes, digit separators, and range checks.

### 4.4 Explicit conversions

Xenon v0.1 has no syntax for explicit numeric conversion. There is no
`as`, no `cast<T, U>`, no narrowing syntax.

Consequence: narrowing and cross-kind conversion are not expressible
in v0.1. A `u32` cannot be converted to a `u8`; an `i32` cannot be
converted to an `f64`.

The scalar construction form `T { x }` (see `literals.md` §8.3) is the
one form that resembles a conversion. Its rules are not yet fully
specified.

Rationale: explicit casts are planned as `cast<T, U>(value: &U)`,
which requires generics (a v0.2 feature).

This is a known limitation and will be lifted in v0.2.

## 5. Copy semantics

In Xenon v0.1, every type is copyable. Assigning, passing, or returning a
value copies it, and the source remains valid.

The language has no resource-owning types yet: no heap allocation, no
file handles, no owned buffers. `str` is a view into static data,
primitives are values, and structs contain only copyable fields.

Move-only types, `drop`, and use-after-move checking are deferred to a
future version and are not part of v0.1.