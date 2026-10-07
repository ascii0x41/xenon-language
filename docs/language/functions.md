# Functions

## 1. Overview

A function is a named block of code that takes parameters, performs a
computation, and optionally returns a value.

This document specifies:

- Function declaration syntax.
- Parameters.
- Return values.
- Calling conventions.
- Recursion.
- Overloading.
- Annotations and external functions.
- Visibility.
- `main`.

Functions declared inside an `impl` block (methods and static functions)
are specified in `methods.md`.

## 2. Declaration syntax

```xe
func <name> ( <param> , ... ) [-> <return_type>] <body>
```

- `<name>` is an identifier.
- `<param>` is `[mut] <name> : <type>`.
- `<return_type>` is optional. If omitted, the function returns `void`.
  `-> void` may be written explicitly and means the same thing.
- `<body>` is a block `{ ... }`, except for external functions (see §8).

Examples:

```xe
func add(a: i32, b: i32) -> i32 {
    return a + b;
}

func print_hello() {
    print("hello");
}

func explicit_void() -> void {
    // same as omitting the return type
}

func main() -> i32 {
    return 0;
}
```

## 3. Parameters

Parameters are bindings scoped to the function body. They are
immutable by default:

```xe
func f(x: i32) {
    x = x + 1;             // error: x is not mutable
}
```

To allow mutation of a parameter, declare it mutable. `mut` goes
before the parameter name:

```xe
func f(mut x: i32) {
    x = x + 1;             // ok; mutates the local copy
}
```

A mutable parameter is a local copy, not a reference to the caller's
value. Mutation does not propagate to the caller.

To mutate the caller's value, use a mutable reference or a mutable
pointer:

```xe
func increment(x: &mut i32) {
    x += 1;                // writes through the reference
}
```

See `references-and-pointers.md`.

### 3.1 Parameter passing

Parameters are passed by value. The value is a copy of the argument.
References and pointers are themselves values (one word each), so
copying one duplicates the reference or pointer and shares the referent.

For primitive types, "copy" means the value is duplicated:

```xe
func f(x: i32) {
    // x is a copy of the caller's value
}
```

For user-defined types, "copy" means the struct's fields are copied:

```xe
type Point { x: i32; y: i32; }

func f(p: Point) {
    // p is a copy of the caller's Point
}
```

For references, the reference is duplicated and the referent is shared:

```xe
func f(r: &i32) {
    // r refers to the caller's value
}
```

For pointers, the pointer is duplicated and the pointed-to value is
shared:

```xe
func f(p: *i32) {
    // p points at the caller's value
}
```

### 3.2 Parameter mutability and the caller

Changing a parameter's value never affects the caller, because
parameters are always copies:

```xe
func f(mut x: i32) {
    x = 100;
}

let a = 1;
f(a);
// a is still 1
```

To affect the caller, use `&mut T` or `*mut T`.

## 4. Return values

A function's return type is specified after `->`:

```xe
func add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

If `->` is omitted, the function returns `void`:

```xe
func print_hello() {
    print("hello");
}
```

A `void` function does not need a `return` statement. It may still
have one:

```xe
func f() {
    return;                // ok
}
```

### 4.1 Return-path checking

A function with a non-`void` return type must return a value on every
path. It must not "fall off the end" of its body. `return` statements
may appear anywhere in the body.

```xe
func foo(x: i32) -> i32 {
    if x > 0 {
        return x;
    }
}                          // error: not every path returns a value
```

The exact rules for deciding which statements are guaranteed to return
are part of the control-flow analysis. See `statements.md` §7.1.

### 4.2 `return` semantics

`return <expr>;` evaluates `<expr>`, converts it to the function's
return type (via implicit widening if needed), and returns from the
function.

```xe
func f() -> i64 {
    return 1;              // 1 is i64 from context
}

func g(x: i32) -> i64 {
    return x;              // i32 widens to i64
}
```

`return;` is only allowed in a `void` function.

A `return` statement exits the function immediately, skipping any
remaining statements in the body.

## 5. Calling conventions

Function calls use `()`:

```xe
let x = add(1, 2);
```

Argument expressions are evaluated in the order they appear, left to
right. This is specified because it matters for functions with side
effects:

```xe
func f() -> i32 { print("f"); return 1; }
func g() -> i32 { print("g"); return 2; }
add(f(), g());             // prints "f" then "g"
```

A function call is an expression with the function's return type.

### 5.1 Arguments

Arguments must match the parameters' types, either exactly or via an
implicit widening conversion:

```xe
func f(x: u32) { }

f(1u8);                    // ok; 1u8 widens to u32
f(1u32);                   // ok
f(1i32);                   // error: i32 does not convert to u32
```

**Reference and pointer arguments are never implicit.** The caller
writes the operator that creates the reference or pointer:

```xe
func by_value(x: i32) { }
func by_ref(x: &i32) { }
func by_mut_ref(x: &mut i32) { }
func by_ptr(p: *i32) { }

let mut a = 1;

by_value(a);               // ok: value
by_ref(&a);                // ok: reference
by_mut_ref(&mut a);        // ok: mutable reference
by_ptr(&raw a);            // ok: raw pointer

by_ref(a);                 // error: expected &i32, got i32
by_ptr(&a);                // error: expected *i32, got &i32
by_ref(&1);                // error: 1 is not a place
```

The one exception is a method receiver, where the call syntax
`value.method()` supplies the receiver according to the method's `self`
declaration. See `methods.md`.

### 5.2 Arity

The number of arguments must match the number of parameters. There
are no default parameters and no variadic functions in v0.1.

```xe
func add(a: i32, b: i32) -> i32 { ... }

add(1);                    // error: expected 2 arguments, got 1
add(1, 2, 3);              // error: expected 2 arguments, got 3
```

## 6. Recursion

Functions may call themselves:

```xe
func factorial(n: u64) -> u64 {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}
```

Mutual recursion between functions is also allowed:

```xe
func is_even(n: u64) -> bool {
    if n == 0 { return true; }
    return is_odd(n - 1);
}

func is_odd(n: u64) -> bool {
    if n == 0 { return false; }
    return is_even(n - 1);
}
```

v0.1 does not perform tail-call optimization or detect unbounded
recursion. A recursive call that grows the stack without bound will
cause a stack overflow at runtime.

## 7. Overloading

Xenon supports **function overloading**. Several functions may share a
name if their signatures differ.

A function's **signature** consists of:

- the function name, and
- the parameter types, in order.

The return type is not part of the signature. A parameter's `mut`
qualifier is not part of the signature either.

```xe
func add(a: i32, b: i32) -> i32 { ... }
func add(a: f64, b: f64) -> f64 { ... }    // ok: different parameter types

func f(x: i32) { ... }
func f(mut x: i32) { ... }                 // error: redeclaration of f(i32)
```

Two functions with the same signature in the same scope are a
redeclaration error.

Mutability **does** matter for reference and pointer types, because
`&T`, `&mut T`, `*T` and `*mut T` are distinct types:

```xe
func g(x: &i32) { ... }
func g(x: &mut i32) { ... }                // ok: distinct signatures
```

This is function overloading only. There is no operator overloading in
v0.1 (see `expressions.md` §1).

### 7.1 Overload resolution

A call is resolved in this order:

1. **Exact match.** A function whose parameter types exactly match the
   argument types.
2. **Widening match.** Otherwise, a function the arguments can be
   passed to using only the implicit widening rules of `types.md`
   §1.1.
3. Otherwise resolution fails and the call is an error.

No conversion is invented to make an overload viable.

Unsuffixed integer literals default to `i32`, so `add(1, 2)` is first
treated as the call `add(i32, i32)` and looks for an exact
`add(i32, i32)`.

> **Open question:** Several cases are not decided:
>
> - If no exact `add(i32, i32)` exists but there is exactly one
>   function `add(u8, u8)`, can the literals `1` and `2` still unify
>   with `u8` (as they would in `let a: u8 = 1;`), or does the call fail?
> - If more than one widening match exists, is the call ambiguous (an
>   error), or is there a "closest" rule?

## 8. Annotations and external functions

An **annotation** is written `#[...]` immediately before a declaration.

```xe
#[extern("C")]
func test_extern() -> i32;

#[noreturn]
func quit() {
    exit(0);
}
```

- `#[extern("C")]` marks a function as external, with the named calling
  convention. An external function has no body and its declaration ends
  with `;`. It is the annotation that makes the body optional: a
  function without `#[extern(...)]` must have a body.
- `#[noreturn]` marks a function that never returns to its caller.

A bodiless declaration without `#[extern(...)]` is an error.

> **Open question:** The set of annotations beyond `extern` and
> `noreturn`, the rules for the string argument of `extern`, and how a
> `#[noreturn]` call interacts with return-path analysis are not
> specified.

### 8.1 Documentation comments

A comment beginning with `///` is a documentation comment. It attaches
to the declaration that follows it.

```xe
/// Adds two numbers.
func add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

## 9. Visibility

Functions are private to the module that declares them by default. The
`pub` keyword makes a function visible to other modules:

```xe
pub func add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

Module visibility rules are specified in `modules.md`.

## 10. `main`

A Xenon program must have a `main` function. This is the entry point.

```xe
module main;

func main() {
    // program starts here
}
```

`main` must be in the module named `main` (see `modules.md`).

`main` may return `void` or `i32`:

```xe
func main() -> i32 {
    return 0;
}
```

If `main` returns `i32`, the returned value is the process exit code.
`0` indicates success; any other value is an error code.

`main` does not take arguments in v0.1.