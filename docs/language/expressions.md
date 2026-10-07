# Expressions

## 1. Overview

An expression is a syntactic construct that produces a value. Every
expression has a type, determined at compile time.

Most of the language is expressions. Statements are a thin layer on top
(see `statements.md`).

This document specifies:

- The operator set.
- Operator precedence and associativity.
- Typing rules for each operator.
- Place expressions and value expressions.

Operator overloading is **not implemented in v0.1**. Operators are
defined only on the types listed below. User-defined types have no
`+`, `-`, `==`, `<`, etc. Function overloading does exist (see
`functions.md` §7); it is unrelated to operator overloading.

## 2. Operator set

### 2.1 Unary operators

| Operator    | Name                  | Example      | Operand type        | Result type |
|-------------|-----------------------|--------------|---------------------|-------------|
| `+`         | Unary plus            | `+x`         | numeric             | same as `x` |
| `-`         | Negation              | `-x`         | numeric             | same as `x` |
| `~`         | Bitwise NOT           | `~x`         | integer             | same as `x` |
| `!`         | Logical NOT           | `!x`         | `bool`              | `bool`      |
| `&`         | Reference             | `&x`         | place of type `T`   | `&T`        |
| `&mut`      | Mutable reference     | `&mut x`     | mutable place `T`   | `&mut T`    |
| `&raw`      | Raw pointer           | `&raw x`     | place of type `T`   | `*T`        |
| `&raw mut`  | Mutable raw pointer   | `&raw mut x` | mutable place `T`   | `*mut T`    |
| `*`         | Pointer dereference   | `*p`         | `*T` or `*mut T`    | `T` (place) |

- `&`, `&mut`, `&raw` and `&raw mut` create references and raw pointers.
  `&` is the **reference** operator; it never produces a raw pointer.
  `&raw` is a separate operator, not an alternate spelling of `&`.
- `*` is only defined on raw pointers. It is **not** defined on
  references: references auto-dereference (see
  `references-and-pointers.md` §2.2). `*r` where `r: &T` is a type
  error.
- `+`, `-`, `~` and `!` are not overloadable.
- `~` is defined only for integer types. It is not defined for `bool`.
- `!` is defined only for `bool`.
- Unary `+` is a no-op for numeric types.
- See `references-and-pointers.md` for the semantics of the reference
  and pointer operators.

### 2.2 Binary operators

| Operator | Name             | Example    | Operand types        | Result type |
|----------|------------------|------------|----------------------|-------------|
| `+`      | Addition         | `a + b`    | numeric, numeric     | common type |
| `-`      | Subtraction      | `a - b`    | numeric, numeric     | common type |
| `*`      | Multiplication   | `a * b`    | numeric, numeric     | common type |
| `/`      | Division         | `a / b`    | numeric, numeric     | common type |
| `%`      | Remainder        | `a % b`    | integer, integer     | common type |
| `==`     | Equality         | `a == b`   | see §4.2             | `bool`      |
| `!=`     | Inequality       | `a != b`   | see §4.2             | `bool`      |
| `<`      | Less than        | `a < b`    | ordered, ordered     | `bool`      |
| `<=`     | Less or equal    | `a <= b`   | ordered, ordered     | `bool`      |
| `>`      | Greater than     | `a > b`    | ordered, ordered     | `bool`      |
| `>=`     | Greater or equal | `a >= b`   | ordered, ordered     | `bool`      |
| `&&`     | Logical AND      | `a && b`   | `bool`, `bool`       | `bool`      |
| `\|\|`   | Logical OR       | `a \|\| b` | `bool`, `bool`       | `bool`      |
| `&`      | Bitwise AND      | `a & b`    | integer, integer     | common type |
| `\|`     | Bitwise OR       | `a \| b`   | integer, integer     | common type |
| `^`      | Bitwise XOR      | `a ^ b`    | integer, integer     | common type |
| `<<`     | Left shift       | `a << b`   | integer, integer     | `a`'s type  |
| `>>`     | Right shift      | `a >> b`   | integer, integer     | `a`'s type  |

The **common type** for two operands of different numeric types is the
result of the implicit widening rules in `types.md` §1.1. If no common
type exists (e.g. `u32` and `i32`, `i32` and `f64`, or an integer and
a float), the operator is a compile error. The analyser does not apply
any numeric conversion beyond those rules.

### 2.3 Assignment operators

| Operator | Example   | Meaning           |
|----------|-----------|-------------------|
| `=`      | `a = b`   | Assign `b` to `a` |
| `+=`     | `a += b`  | `a = a + b`       |
| `-=`     | `a -= b`  | `a = a - b`       |
| `*=`     | `a *= b`  | `a = a * b`       |
| `/=`     | `a /= b`  | `a = a / b`       |
| `%=`     | `a %= b`  | `a = a % b`       |
| `&=`     | `a &= b`  | `a = a & b`       |
| `\|=`    | `a \|= b` | `a = a \| b`      |
| `^=`     | `a ^= b`  | `a = a ^ b`       |
| `<<=`    | `a <<= b` | `a = a << b`      |
| `>>=`    | `a >>= b` | `a = a >> b`      |

The left-hand side of any assignment must be a mutable place (see §6).

Assignment operators are right-associative in the grammar, but all
assignment operators have result type `void`. This means assignment
cannot appear in value position.

```xe
a = b;               // ok: statement
let x = (a = b);     // error: cannot use void in value position
if a = b { ... }     // error: if condition must be bool, got void
a = b = c;           // error: b = c has type void, cannot assign to a
```

To assign `c` to `b` and then `b` to `a`, write two statements:

```xe
b = c;
a = b;
```

This avoids the C-family "assignment in condition" footgun.

Compound assignments `a op= b` are equivalent to `a = a op b`, except
that `a` is evaluated only once.

### 2.4 Postfix operators

| Operator | Name         | Example | Meaning                    |
|----------|--------------|---------|----------------------------|
| `[]`     | Index        | `a[i]`  | Array indexing             |
| `()`     | Call         | `f(x)`  | Function call              |
| `.`      | Field/method | `a.b`   | Field access, method call  |

Postfix operators bind tighter than any unary operator.

**Indexing.** `a[i]` requires `a` to be a static array `[T; N]`. The
result is a place of type `T`. Indexing is bounds checked: if `i` is not
in `0 ..= N-1`, the program panics. Out-of-bounds access is a runtime
failure, never undefined behavior.

> **Open question:** The type(s) allowed for the index `i` have not been
> decided (e.g. only `usize`, or any unsigned integer that widens to
> `usize`). Whether a `{integer}` literal index infers as `usize`
> follows from that decision.

**Method calls.** `a.f(args)` supplies `a` as the receiver (the `self`
argument) of method `f`, according to how `f` declares `self`. See
`methods.md`.

### 2.5 Ternary operator

```xe
<cond> ? <expr> : <expr>
```

- `<cond>` must be `bool`.
- Both branch expressions must have a common type (under the widening
  rules in `types.md` §1.1). The result has that type.
- Neither branch may be `void`.
- The ternary is the expression form of a conditional. `if` is a
  statement and cannot be used as a value (see `statements.md` §5).

```xe
let w = true ? 1 : 2;
```

> **Open question:** Only the selected branch should be evaluated. This
> is not yet stated formally.

## 3. Precedence and associativity

From lowest precedence (binds loosest) to highest (binds tightest):

| Level | Operators                                                  | Associativity |
|-------|------------------------------------------------------------|---------------|
| 1     | Assignment `= += -= *= /= %= &= \|= ^= <<= >>=`, ternary `?:` | right      |
| 2     | Logical OR `\|\|`                                          | left          |
| 3     | Logical AND `&&`                                           | left          |
| 4     | Bitwise OR `\|`                                            | left          |
| 5     | Bitwise XOR `^`                                            | left          |
| 6     | Bitwise AND `&`                                            | left          |
| 7     | Equality `== !=`                                           | left          |
| 8     | Comparison `< <= > >=`                                     | non-assoc     |
| 9     | Shift `<< >>`                                              | left          |
| 10    | Additive `+ -`                                             | left          |
| 11    | Multiplicative `* / %`                                     | left          |
| 12    | Unary `+ - ~ ! * & &mut &raw &raw mut`                     | prefix        |
| 13    | Postfix `[] () .`                                          | left          |

Notes:

- **Non-associative comparison.** `a < b < c` is a parse error.
  Chained comparisons must be written explicitly:

  ```xe
  a < b && b < c
  ```

- **`&` and `*` are both unary and binary.** The parser disambiguates
  by position. Prefix `&x` is the reference operator; infix `a & b` is
  bitwise AND. Prefix `*p` is dereference; infix `a * b` is multiply.

- **Assignment is right-associative** in the grammar, but the `void`
  result type makes chained assignment a type error.

- **Ternary** shares level 1 with assignment and is right-associative:
  `a ? b : c ? d : e` parses as `a ? b : (c ? d : e)`.

> **Open question:** Bitwise operators bind looser than `==`, so
> `a & b == c` parses as `a & (b == c)`. This is the C precedence
> behavior. It is kept as is for now; it should be decided deliberately
> rather than inherited.

## 4. Typing rules

### 4.1 Arithmetic operators

`+ - * /` accept two numeric operands. They must have a common type.

If the operands have different types:

- If both are integers of the same signedness, the smaller widens to
  the larger.
- If both are floats, `f32` widens to `f64`.
- Otherwise, it is a compile error.

```xe
let a: u8 = 1;
let b: u32 = 2;
a + b;            // ok; a widens to u32; result is u32

let c: i32 = 1;
let d: f64 = 2.0;
c + d;            // error: i32 and f64 are different kinds

let e: u32 = 1;
let f: i32 = 2;
e + f;            // error: u32 and i32 have different signedness
```

`%` is defined only for integer types.

Integer division `/` truncates toward zero.

**Integer division by zero** panics (debug and release).

**Overflow:** follows the rules in `types.md` §2.1: checked and
panicking in debug builds, unchecked and possibly wrapping in release
builds.

### 4.2 Comparison operators

**Equality (`==`, `!=`).** The operands must have a common type, using
the same rules as arithmetic. In particular:

- Numeric operands of the same kind and signedness are widened to a
  common type, so `u8 == u32` is valid and compares as `u32 == u32`.
- `i32 == f64`, `u32 == i32`, and `i32 == f32` are compile errors.
- `rune`, `bool`, and `str` compare with their own type. `str`
  compares byte content.
- User-defined types do not support `==` (no operator overloading).

**Pointer equality.** `==` and `!=` are defined on pointer types:

- `p == q` where `p` and `q` are `*T` and/or `*mut T` is valid. `*T ==
  *mut T` is valid.
- `p == nullptr` and `p != nullptr` are valid for any pointer `p`,
  because `nulltype` implicitly converts to any pointer type.
- Pointer comparison compares the pointer values (addresses). It does
  not dereference and does not compare the pointed-to values.
- `nullptr == nullptr` is `true`.
- Comparing a reference with `nullptr` is an error, because references
  are never null.

**Ordering (`<`, `<=`, `>`, `>=`).** Operands must be of an *ordered*
type and have a common type. Ordered types are:

- All integer types.
- All float types.
- `rune`.
- `str`.

`bool` is not ordered. `a < b` where `a, b: bool` is a type error.
Pointers are not ordered.

Comparison operators are non-associative. `a < b < c` is a parse error.

Float comparison follows IEEE 754: any comparison involving NaN is
`false`, except `!=`, which is `true`.

> **Open question:** The ordering of `str` (byte-wise lexicographic?) is
> not yet defined.

### 4.3 Logical operators

`&&` and `||` require both operands to be `bool`. They short-circuit:

- `a && b`: evaluates `a`. If `a` is `false`, the result is `false` and
  `b` is not evaluated. Otherwise `b` is evaluated and its value is
  the result.
- `a || b`: evaluates `a`. If `a` is `true`, the result is `true` and
  `b` is not evaluated. Otherwise `b` is evaluated and its value is
  the result.

There is no truthiness.

```xe
let a: i32 = 1;
a && true;         // error: && requires bool operands, got i32
```

`&&` and `||` are not overloadable. They are lowered as control flow,
not as function calls.

### 4.4 Bitwise operators

`& | ^` require two integer operands. They must have a common type under
the widening rules. Mixed-signedness operands are a compile error.

```xe
let a: u8 = 1;
let b: u32 = 2;
a & b;             // ok; a widens to u32; result is u32

let c: u32 = 1;
let d: i32 = 2;
c & d;             // error: u32 and i32 have different signedness
```

`~` requires an integer operand.

`<<` and `>>` require integer operands. The result type is the type of
the left operand. The right operand may be any integer type.

```xe
let a: u8 = 1;
let b: u32 = 2;
a << b;            // ok; result is u8
```

**Right shift.** `>>` is only defined for **unsigned** integer types,
where it is a logical shift (zero-filling). Right-shifting a signed
integer is a compile error in v0.1. Signed right shift will be specified
in a later version.

**Shift count.** The shift count must be less than the bit width of the
left operand.

For example, shifting a `u8` or `i8` by `0` through `7` is valid. A
shift by `8` or greater is invalid and produces a runtime error
(panic).

Shift count violations have the same behavior in debug and release
builds. They are not undefined behavior and are not silently masked.

> **Open question:** Left-shifting a signed integer (and the behavior
> when bits are shifted out) is not yet specified.

### 4.5 Assignment operators

The left operand must be a mutable place. The right operand's type must
be assignable to the left operand's type, either exactly or via an
implicit widening conversion (see `types.md` §1.1).

```xe
let mut a: u32 = 1;
let b: u8 = 2;
a = b;             // ok; b widens to u32

let mut c: u8 = 1;
let d: u32 = 2;
c = d;             // error: u32 does not widen to u8
```

All assignment operators have result type `void`.

Compound assignments `a op= b` are equivalent to `a = a op b`, except
that `a` is evaluated only once.

Assigning to a reference-typed place writes through the reference (see
`references-and-pointers.md` §2.2).

## 5. Unary operator typing

### 5.1 Unary `+`

`+x` requires `x` to be a numeric type. The result is `x` unchanged.
For integer types, no promotion is performed (unlike C).

### 5.2 Unary `-`

`-x` requires `x` to be a numeric type.

- For signed integers: negation. Negating the minimum value of the type (e.g. `-128i8`
  as a computed value rather than a literal) overflows; behavior follows
  the overflow rules in `types.md` §2.1.
- For unsigned integers: `-x` is a compile error.
- For floats: IEEE 754 negation. `-0.0` is distinct from `0.0`.

### 5.3 `~`

`~x` requires `x` to be an integer type. The result is the bitwise
complement of `x` in `x`'s type.

### 5.4 `!`

`!x` requires `x` to be `bool`. The result is the logical negation.

### 5.5 Reference and pointer operators

See `references-and-pointers.md` for full semantics. In brief:

- `&x` requires `x` to be a place of type `T`. The result is `&T`.
- `&mut x` requires `x` to be a *mutable* place of type `T`. The result
  is `&mut T`.
- `&raw x` requires a place of type `T`. The result is `*T`.
- `&raw mut x` requires a *mutable* place of type `T`. The result is
  `*mut T`.
- `*p` requires `p` to be a raw pointer type (`*T` or `*mut T`). It
  produces a place referring to the pointed-to location. Dereferencing
  a `*mut T` produces a mutable place; dereferencing a `*T` produces an
  immutable place.
- `*` cannot be applied to a reference.

The operand of each of these must be a place. `&1`, `&(a + b)`, and
`&"hello"` are errors.

## 6. Places and values

Every expression is either a **place expression** or a **value
expression**.

A **place expression** designates storage: a location that can be read,
and (if mutable) written, and whose address can be taken. A **value
expression** produces a value with no storage of its own.

### 6.1 Place expressions

- Variable names: `x`
- Field access of a place: `a.b`
- Indexing of a place: `a[i]`
- Dereference of a raw pointer: `*p`
- Any expression of reference type (see
  `references-and-pointers.md` §2.2): it designates the storage of its
  referent.

### 6.2 Value expressions

- Literals: `1`, `"hello"`, `true`, `nullptr`
- Arithmetic, comparison, logical and bitwise expressions: `a + b`
- Reference and raw-pointer creation: `&x`, `&mut x`, `&raw x`,
  `&raw mut x`
- Ternary expressions
- Calls that return a non-reference type

### 6.3 Mutability

A place is **mutable** if it is one of:

- A variable declared with `let mut`, or a `mut` parameter.
- A field of a mutable place.
- An element of a mutable array place.
- The dereference of a `*mut T`.
- The referent of an `&mut T`.

All other places are immutable.

### 6.4 Where places are required

- The left-hand side of assignment must be a mutable place.
- The operand of `&` and `&raw` must be a place.
- The operand of `&mut` and `&raw mut` must be a mutable place.

Function calls are value expressions unless the function returns a
reference type, in which case the call designates the storage of the
referent:

```xe
func get_state() -> i32 { ... }       // call is a value expression
func get_state_ref() -> &i32 { ... }  // call designates the referent's storage
```

> **Note:** The exact lvalue/rvalue terminology may change. The rules
> above (what designates storage, what can be assigned to or borrowed,
> and mutability) are what the analyser relies on.