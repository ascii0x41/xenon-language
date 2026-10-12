# Variables

## 1. Overview

A variable is a named binding to a value. Variables are introduced with
`let`.

This document specifies:

* The syntax of `let` bindings.
* Mutability.
* Type annotations and inference.
* Scoping.
* Uninitialised variables.

## 2. Let bindings

The syntax is:

```xe
let [mut] <name> [: <type>] [= <expr>] ;
```

* The name must be a valid identifier.
* The type annotation is optional if the initializer determines the
  type.
* The initializer is optional if a type annotation is present.
* A binding with neither a type annotation nor an initializer is a
  compile error.

```xe
let x = 1;                 // inferred: i32
let y: i64 = 1;            // annotated: i64
let z: u8;                 // ok: uninitialised, type given
let w: u8 = 0;             // ok
let v;                     // error: type cannot be inferred
```

## 3. Mutability

Bindings are immutable by default. A binding can be made mutable with the `mut` keyword:

```xe
let x = 1;                 // immutable
let mut y = 1;             // mutable
```

An immutable binding may not be reassigned:

```xe
let x = 1;
x = 2;                     // error: x is not mutable
```

A mutable binding may be reassigned:

```xe
let mut y = 1;
y = 2;                     // ok
y = y + 1;                 // ok
```

Mutability is a property of the *binding*, not the value. Two bindings of the same type can differ in mutability:

```xe
let a = 1;                 // immutable
let mut b = 1;             // mutable
```

Assigning a value does not require the value to be mutable; it requires the binding to be mutable.

A binding declared without an initializer must be mutable:

```xe
let mut x: i32;
x = 42;                    // ok: initialises x
x = 43;                    // ok: reassigns x

let y: i32;                // error: immutable binding requires an initializer
```

An uninitialized mutable binding may be assigned later to initialize it. The assignment must obey the ordinary type rules. Whether the binding has been initialized is tracked according to §6.


### 3.1 Mutability and references

A binding's mutability determines whether `&mut` and `&raw mut` may be
applied to it:

```xe
let a = 1;
let r: &mut i32 = &mut a;       // error: a is not mutable
let p: *mut i32 = &raw mut a;   // error: a is not mutable

let mut b = 1;
let q: &mut i32 = &mut b;       // ok
let s: *mut i32 = &raw mut b;   // ok
```

See [references-and-pointers.md](references-and-pointers.md).

### 3.2 Mutability and method calls

A method whose receiver is `&mut self` requires the receiver to be a
mutable place:

```xe
let p = Point::init(1, 2);
p.translate(1, 1);              // error: p is not mutable

let mut q = Point::init(1, 2);
q.translate(1, 1);              // ok
```

Methods are specified in [methods.md](methods.md).

## 4. Type annotations and inference

A binding's type comes from either:

1. An explicit type annotation: `let x: i32 = 1;`
2. Inference from the initializer: `let x = 1;`

If both are present, the annotation and the initializer's type must be
compatible. An implicit widening conversion is applied if needed:

```xe
let x: u32 = 1u8;          // ok; 1u8 widens to u32

let y: u8 = 1u32;          // error: u32 does not widen to u8
```

If only the annotation is present and there is no initializer, the
binding has the annotated type but is uninitialised. See §6.

If neither is present, it is a compile error.

The default types for unconstrained literals apply (see [types.md](types.md)
§4.3 and [literals.md](literals.md)):

```xe
let x = 1;                 // x: i32
let y = 1.0;               // y: f32
let z = "hello";           // z: str
let w = true;              // w: bool
```
## 5. Scope

A binding is visible from its declaration to the end of the enclosing block.

```xe
{
    let x = 1;
    // x is visible here
}
// x is not visible here
```

Function parameters are bindings scoped to the function body:

```xe
func f(a: i32, b: i32) -> i32 {
    // a and b are visible here
    return a + b;
}
// a and b are not visible here
```

**Shadowing.** A binding may shadow a binding from an enclosing scope. The inner binding hides the outer binding within its own scope; the outer binding becomes visible again when the inner scope ends.

```xe
let x = 1;

{
    let x = 2;             // ok: shadows the outer x
    println(x);            // prints 2
}

println(x);                // prints 1
```

Declaring the same name more than once in the same scope is a compile-time error.

```xe
let x = 1;
let x = 2;                 // error: x is already declared in this scope
```

Shadowing creates a new binding; it does not reassign or change the mutability of the existing binding.


## 6. Uninitialised bindings

A mutable `let` binding with a type annotation and no initializer is legal:

```xe
let mut z: u8;
```

The variable is **uninitialised**: its storage does not yet contain a value that the program may safely read.

Reading an uninitialised variable produces a compile-time warning, not an error.

```xe
let mut x: i32;
println(x);                // warning: x is uninitialised
x = 42;
println(x);                // ok
```

### 6.1 What counts as a read

The analyser tracks whether each binding has been initialized. A binding declared without an initializer starts uninitialised. An assignment to the binding initializes it.

* **Assignment is not a read:** `x = 10;` does not read the previous value of `x`.
* **Compound assignment is a read:** `x += 1;` reads the previous value of `x` and therefore warns if `x` is uninitialised.
* **Using a variable as a value is a read:** passing it to a function, using it as an operator operand, or using it in an initializer all count as reads.

```xe
let mut x: i32;
x = 10;                    // ok: assignment initialises x
let y = x + 1;             // ok: x is initialised
```

```xe
let mut a: i32;
let b = a + 1;             // warning: a is uninitialised
```

```xe
let mut count: i32;
count += 1;                // warning: reads uninitialised count
```

For v0.1, initialization tracking is conservative and not fully path-sensitive. The analyser may not detect every uninitialized read that depends on control flow. Path-sensitive analysis may be introduced in a later version.

**References.** A reference binding must be initialized at its declaration. Declaring an uninitialized reference is a compile-time error because a reference must always refer to a valid value.

```xe
let r: &i32;                // error: reference requires an initializer
let mut value = 42;
let r: &i32 = &value;       // ok
```

A compiler may provide an option to promote uninitialized-read warnings to errors. Whether such an option is supported is implementation-defined.


## 7. References in `let` bindings

A reference is created with `&` or `&mut`; it is never created
implicitly:

```xe
let a: i32 = 1;
let r: &i32 = &a;          // ok: r refers to a
let s: &i32 = a;           // error: expected &i32, got i32
```

See [references-and-pointers.md](references-and-pointers.md) §2.2 for the open questions about
copying a reference-typed binding.