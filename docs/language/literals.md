# Literals

## 1. Overview

A literal is a value written directly in source.

Xenon has scalar literals (integers, floats, runes, strings, booleans,
`nullptr`) and compound literals (arrays, structs, and scalar
construction forms).

This document specifies the syntax, typing rules, and range-checking
rules for each kind of literal.

### 1.1 The literal-type problem

Consider:

```xe
let a: u8  = 1;
let b: i64 = 1;
let c      = 1;
```

All three write `1`. All three are legal. They produce values of three
different types: `u8`, `i64`, and `i32`.

This is achieved with **literal type metavariables**. An unsuffixed
integer literal is not any particular integer type. It carries a
placeholder, written `{integer}`, that unifies with whatever integer
type the surrounding context requires. If no context constrains it, it
defaults to `i32`.

Similarly, an unsuffixed float literal has type `{float}` and defaults
to `f32`.

This is a deliberate exception to Xenon's "explicit over implicit"
principle. Forcing a suffix on every literal would be unbearably verbose
in numeric code. The rule is narrow: literal metavariables only appear
in literal expressions, and they are resolved by the end of type
inference. They never escape into user-visible types.

See `types.md` §4.3 for the type-level rules.

## 2. Integer literals

### 2.1 Syntax

```xe
1234          // decimal
0xFF, 0xff    // hex
0b1010        // binary
0o777         // octal
```

Digit separators: a single underscore may appear between digits.

```xe
1_000_000        // valid
0xFF_FF          // valid
_1               // invalid: leading underscore
1_               // invalid: trailing underscore
1__2             // invalid: consecutive underscores
```

### 2.2 Suffixes

A literal may carry a type suffix:

```xe
1u8   1u16   1u32   1u64   1usize
1i8   1i16   1i32   1i64
```

Suffixes may be separated from the digits by an underscore for
readability:

```xe
1u8      // valid
1_u8     // also valid
```

Underscore-separated suffixes are accepted for consistency with digit
separators, but the style guide recommends `1u8`.

Hex, binary, and octal literals accept the same suffixes:

```xe
0xFFu8
0b1010_u16
0o777i32
```

### 2.3 Typing rules

1. An unsuffixed integer literal has type `{integer}`.
2. `{integer}` unifies with any integer type (`u8`..`u64`, `i8`..`i64`,
   `usize`).
3. If, after full type inference, an `{integer}` is still unconstrained,
   it defaults to `i32`.
4. A suffixed literal has exactly the named type.

### 2.4 Range checking

A literal must be representable in its resolved type. This is checked at
compile time, not at runtime.

```xe
256u8                     // error: literal out of range for u8
128i8                     // error: literal out of range for i8
18446744073709551616u64   // error: literal out of range for u64
```

### 2.5 Negative literals

The sign of a numeric literal is not part of the literal itself. `-1`
is parsed as `-(1)`: the unary negation operator applied to the integer
literal `1`.

This matters for range checking. `-128` is not a literal; it is the
negation of `128`. The literal `128` does not fit in `i8`, but `-128`
does. Sema handles this by special-casing `-<intliteral>`: when it sees
a unary minus applied directly to an integer literal, it range-checks
the negated value against the target type.

```xe
128i8        // error: 128 does not fit in i8
-128i8       // ok; the negation of 128 fits in i8
-129i8       // error: -129 does not fit in i8
```

For unsigned types, the negation is always an error (see
`expressions.md` §5.2).

```xe
-1u8         // error: unary minus on unsigned
```

## 3. Float literals

### 3.1 Syntax

```xe
1.0
1.5e-3
3.14159
1e10
```

A float literal must contain either a `.` or an exponent. `1` is an
integer literal; `1.0` is a float literal. This is a hard rule: it
prevents `1` from being ambiguous.

Leading dot (`.5`) and trailing dot (`1.`) are **not** allowed in v0.1.
Write `0.5` and `1.0`.

Rationale: `.5` conflicts with the `.` postfix operator (field access),
and `1.` reads like a malformed member access.

### 3.2 Suffixes

```xe
1.0f32
1.0f64
```

Underscore-separated forms (`1.0_f32`) are also accepted.

> **Open question:** An earlier note used `0.1d` as a spelling of
> `0.1f64`. `d` is not a defined suffix. Is it intended?

### 3.3 Typing rules

1. An unsuffixed float literal has type `{float}`.
2. `{float}` unifies with `f32` or `f64`.
3. If unconstrained, it defaults to `f32`.

Rationale for the `f32` default: Xenon targets systems and embedded
contexts where `f32` is often the native width. To get `f64`, write
`1.0f64` or give the binding a type annotation.

### 3.4 Range checking

A float literal must be representable in its resolved type. A literal
that overflows the type is a compile error, not a silent conversion to
infinity.

```xe
1.0e40f32    // error: literal overflows f32
```

This differs from IEEE 754 runtime semantics, where arithmetic overflow
produces `+Inf`. The rule is: **the compiler rejects literal overflow;
runtime arithmetic follows IEEE 754.** This makes mistakes visible at
compile time.

### 3.5 Special values

NaN, +Inf, and -Inf cannot be written as literals in v0.1.

## 4. Rune literals

A `rune` represents a single Unicode scalar value.

### 4.1 Syntax

```xe
'a'
'é'
'🦀'
'\n'      '\t'     '\r'     '\0'     '\\'     '\''     '\"'
'\u{1F980}'    // crab, by hex codepoint
'\u{41}'       // 'A'
```

### 4.2 Rules

- A rune literal contains exactly one Unicode scalar value.
- `'ab'` is an error: two scalars.
- `''` is an error: zero scalars.
- `'\u{D800}'` is an error: surrogate.
- `'\u{110000}'` is an error: above U+10FFFF.
- A bare newline inside `'...'` is an error; use `'\n'`.

### 4.3 Typing rules

Rune literals have type `rune`. There is no metavariable, no suffix, no
inference. `rune` is not interchangeable with any integer type.

### 4.4 Escapes

| Escape    | Meaning                        |
|-----------|--------------------------------|
| `\n`      | newline (U+000A)               |
| `\t`      | tab (U+0009)                   |
| `\r`      | carriage return (U+000D)       |
| `\0`      | null (U+0000)                  |
| `\\`      | backslash                      |
| `\'`      | single quote                   |
| `\"`      | double quote                   |
| `\u{...}` | Unicode scalar, hex codepoint  |

## 5. String literals

### 5.1 Syntax

```xe
"hello"
"line\nbreak"
"tab\there"
"quote: \""
"unicode: \u{1F980}"
""              // empty string, valid
```

Raw strings:

```xe
r"no \escapes \here"
r#"can contain "quotes""#
```

Raw strings disable escape processing. `r"\n"` is two characters:
backslash and `n`. The `r#...#` form allows embedded `"` and is used
when the string needs to contain quotes.

### 5.2 Encoding

String literals are UTF-8 byte sequences. The source file is UTF-8. The
literal's bytes are exactly the UTF-8 encoding of the written characters,
with escapes resolved.

`"\u{1F980}"` and `"🦀"` produce the same four bytes.

### 5.3 Typing rules

Every string literal has type `str`. It points into read-only static
data (`.rodata`). No inference is required.

```xe
let s: str = "hello";
```

A string literal has **only** the type `str`. It cannot be used as any of:

```xe
let a: *u8 = "Linus";        // error: expected *u8, got str
let b: &[u8; 5] = "Linus";   // error: expected &[u8; 5], got str
let c: [u8; 5] = "Linus";    // error: expected [u8; 5], got str
```

Programs cannot write the `str` representation themselves. There is no
way to construct a `str` other than from a literal or by passing one
along. See `types.md` §2.8.

### 5.4 What strings are not

- Not owned. `str` points at bytes it does not own.
- Not mutable. You cannot write through a `str`.
- Not NUL-terminated. Bytes with value `0` are ordinary bytes.
- Not concatenable. `a + b` is not defined for `str` in v0.1.
- Not indexable by `rune`. `s[i]`, if it exists, is a `u8`.
- Not addressable. A string literal is a value, not a storage location,
  so `&"hello"` is an error (see `references-and-pointers.md` §2.1).
  Pass `str` by value; it is already a view.

## 6. Boolean literals

```xe
true
false
```

Type `bool`. No inference, no suffixes.

## 7. Null literal

```xe
nullptr
```

`nullptr` has type `nulltype`. `nulltype` has exactly one value:
`nullptr`.

`nulltype` implicitly converts to any pointer type (`*T` or `*mut T`).
The reverse is not allowed. `nulltype` does not convert to a reference
type.

`nullptr` in a context that does not constrain it to a pointer type is
an error.

## 8. Compound literals

### 8.1 Array literals

Array literals have two forms.

**List form.** The element list is written directly:

```xe
[1, 2, 3]
```

The type is `[T; N]` where `N` is the number of elements. All elements
must have a common type.

**Typed form.** The array type is written first and the elements follow
in braces:

```xe
[f32; 3] { 0.0, -9.8, 0.0 }
[u64; 6] { 2, 3, 5, 7, 11, 13 }
```

The number of elements must equal `N`.

There is no repeat form (`[expr; N]`). There is no empty array literal,
because `[T; 0]` is not a valid type (see `types.md` §3.1).

Element types may be literal metavariables, which unify across the
array:

```xe
let a: [u8; 3] = [1, 2, 3];      // 1, 2, 3 all become u8
let b = [1, 2, 3];               // all become i32 (default)
let c = [1u8, 2, 3];             // all become u8
```

### 8.2 Struct literals

Struct literals have two forms.

**Named fields:**

```xe
Point { x: 1, y: 2 }
```

Fields may be written in any order.

**Positional fields:**

```xe
Point { 1, 2 }
```

Values are assigned to fields in declaration order. There is no field
shorthand, so `Point { x, y }` is a *positional* literal whose values
are the variables `x` and `y`.

A literal may initialise only fields that are accessible from the
current module. Fields that are private may only be initialised by
code in the module that declares the type. See `modules.md` §4.2.

Any field that a literal does not initialise holds whatever is stored in
that memory. Reading such a field is **UB**.

> **Open question:** Fields that are not initialised are currently
> UB, which sits uneasily with "fail loudly at compile time". Should a
> literal be required to initialise every field it is permitted to
> construct?

### 8.3 Scalar construction

A scalar type can be written in front of braces, in the same shape as a
struct literal:

```xe
usize { 42 }
bool { false }
```

For `usize`, the value inside the braces must be an unsigned integer.

> **Open question:** The full rules for this form are not specified:
> which source types are allowed for each scalar target, and whether a
> narrowing use such as `usize { some_u64 }` on a 32-bit target is
> permitted. Until this is written, treat it as a typed literal only.

## 9. Summary table

| Literal kind | Metavariable? | Default when unconstrained | Suffix form          |
|--------------|---------------|----------------------------|----------------------|
| Integer      | `{integer}`   | `i32`                      | `1u8`, `1_i64`       |
| Float        | `{float}`     | `f32`                      | `1.0f32`, `1.0_f64`  |
| Rune         | no            | `rune`                     | none                 |
| String       | no            | `str`                      | none                 |
| Bool         | no            | `bool`                     | none                 |
| `nullptr`    | no            | error if unconstrained     | none                 |

## 10. What sema enforces

- [ ] Integer literal fits its resolved type.
- [ ] Float literal fits its resolved type (no implicit overflow to Inf).
- [ ] Rune literal is exactly one Unicode scalar, not a surrogate.
- [ ] All escapes in string and rune literals are valid.
- [ ] `{integer}` and `{float}` metavariables are resolved by the end of
      inference.
- [ ] Suffix names a known primitive type.
- [ ] Digit separators appear only between digits.
- [ ] Unary minus on an integer literal range-checks the negated value.
- [ ] A string literal is never accepted as anything other than `str`.
- [ ] Array literal element count matches `N` in the typed form.
- [ ] A struct literal initialises only fields accessible from the
      current module.