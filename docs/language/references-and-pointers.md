# References and Pointers

## 1. Overview

This document specifies Xenon's reference and pointer model.

Xenon has two families of types for referring to memory:

- **References** (`&T`, `&mut T`) are non-null, auto-dereferenced
  aliases for a value.
- **Pointers** (`*T`, `*mut T`) are raw addresses. They may be null.
  They do not auto-dereference.

These are four distinct types with different capabilities.

Creation operators:

| Expression   | Type     |
|--------------|----------|
| `&x`         | `&T`     |
| `&mut x`     | `&mut T` |
| `&raw x`     | `*T`     |
| `&raw mut x` | `*mut T` |

`&` is the **reference** operator. `&raw` is the explicit way to request
a raw pointer. There is no implicit conversion between references and
raw pointers.

References are the default for passing values by reference. Pointers
are for the low-level case where null is possible or an explicit address
is needed.

In v0.1, memory safety is not enforced by the compiler: references are
not lifetime-checked, and dangling references and pointers are
possible. See §8.

## 2. References

A reference is a non-null, immutable or mutable alias for a value.

```xe
&T          // immutable reference to T
&mut T      // mutable reference to T
```

- `&T` refers to a value you may read but not write.
- `&mut T` refers to a value you may read and write.
- References are never null.
- References are the size of an ordinary address (one word). They are
  not fat pointers.
- References **auto-dereference**. There is no dereference operator for
  references (see §2.2).
- A reference is created with `&` or `&mut`. The operand must be a
  place. `&mut` requires a mutable place.

### 2.1 Creating references

```xe
let a: i32 = 1;
let r: &i32 = &a;           // r refers to a

let mut b: i32 = 1;
let m: &mut i32 = &mut b;   // ok; b is mutable

let c: i32 = 1;
let bad: &mut i32 = &mut c; // error: c is not mutable
```

The operand of `&` or `&mut` must be a place. References cannot be
created from values or literals:

```xe
let r1: &i32 = &1;           // error: 1 is not a place
let r2: &i32 = &(a + b);     // error: expression is not a place
let r3 = &"hello";           // error: a string literal is a value, not a place
```

This keeps one rule: references are created from addressable storage,
never from temporaries. Pass `str` by value, since it is already a view.

### 2.2 Auto-dereference

A reference behaves as if it were the referent. Reading it reads the
referent; assigning to it (including with compound assignment) writes
the referent.

```xe
func increment(x: &mut i32) {
    x += 1;                  // modifies the caller's value
}

let mut b: i32 = 1;
increment(&mut b);           // b is now 2
```

```xe
let mut x = 10;
let rmx = &mut x;

rmx += 5;                    // modifies x
*rmx += 5;                   // error: * is not defined on references
```

Field access auto-dereferences:

```xe
type Pair { x: i32; y: i32; }

func sum(p: &Pair) -> i32 {
    return p.x + p.y;
}
```

Because `*` is not defined on references, `&&T` has no dereference
syntax either; see the open question below.

> **Open question:** Several consequences of "no dereference operator"
> need an explicit rule:
>
> - `let r2 = r;` where `r: &i32`: does `r2` become an `i32` copy, or a
>   second reference to the same location?
> - Passing a reference variable to a reference parameter (`g(r)` where
>   `r: &i32` and `g(x: &i32)`).
> - Whether `&T` of an expression that is already a reference (`&&T`,
>   or `&r`) is a type error or a reborrow.
> - Whether a reference can be rebound after creation.

### 2.3 Non-null

There is no null reference. A reference always refers to a valid
location. In v0.1, this is a convention that the compiler does not
enforce (see §8).

`nullptr` cannot be used as a reference:

```xe
let r: &i32 = nullptr;       // error: references are non-null
```

A raw pointer does not implicitly become a reference:

```xe
let r: &i32 = &raw a;        // error: *i32 is not a reference
```

A reference variable must be initialised. `let r: &i32;` is an error.

## 3. Pointers

A pointer is a raw memory address.

```xe
*T          // pointer to T (read-only through the pointer)
*mut T      // pointer to T (readable and writable)
```

- `*T` points at a `T`; you may read through it but not write.
- `*mut T` points at a `T`; you may read and write through it.
- A pointer may be null.
- A pointer may dangle.
- A pointer does not auto-dereference. Use `*p` to access the
  pointed-to value.
- A pointer is created with `&raw` or `&raw mut`.

### 3.1 The raw-pointer operators

```xe
&raw x          // raw pointer to x, read-only: type *T
&raw mut x      // raw pointer to x, mutable: type *mut T
```

- `x` must be a place.
- `&raw x` may be applied to any place. The result has type `*T`, where
  `T` is the type of `x`.
- `&raw mut x` requires `x` to be a mutable place. The result has type
  `*mut T`.

```xe
let a: i32 = 1;
let p: *i32 = &raw a;             // ok

let mut b: i32 = 1;
let q: *mut i32 = &raw mut b;     // ok

let c: i32 = 1;
let bad: *mut i32 = &raw mut c;   // error: c is not mutable
```

`&raw` and `&raw mut` produce **raw pointers**, not references. `&` and
`&mut` produce **references**, not raw pointers.

> **Note:** `&x` and `&raw x` can be applied to the same place but
> produce different types. `&raw` is not syntactic sugar for `&`.

> **Open question:** `&raw x` can also be read as `&` applied to a call
> `raw(x)` or a variable named `raw`. The grammar must either reserve
> `raw` as a keyword or define the disambiguation.

### 3.2 Dereference

The unary `*` operator dereferences a **pointer**:

```xe
*p          // dereference
```

`*p` is a place expression referring to the pointed-to location.

- `*p` where `p: *T` is an immutable place of type `T`.
- `*p` where `p: *mut T` is a mutable place of type `T`.

Dereferencing does not check validity. Dereferencing a dangling or null
pointer is undefined behavior.

```xe
let a: i32 = 1;
let p: *i32 = &raw a;
let b: i32 = *p;              // read through pointer

let mut c: i32 = 1;
let q: *mut i32 = &raw mut c;
*q = 5;                       // write through mutable pointer
```

## 4. Conversions

References and pointers convert implicitly only by giving up a
capability:

| From          | To         | Implicit? | Reason                                     |
|---------------|------------|-----------|--------------------------------------------|
| `&mut T`      | `&T`       | yes       | giving up mutability                       |
| `*mut T`      | `*T`       | yes       | giving up mutability                       |
| `nulltype`    | `*T`       | yes       | `nullptr` is a pointer of any type         |
| `nulltype`    | `*mut T`   | yes       | `nullptr` is a pointer of any type         |
| `&T`          | `*T`       | no        | use `&raw`                                 |
| `&mut T`      | `*mut T`   | no        | use `&raw mut`                             |
| `&mut T`      | `*T`       | no        | use `&raw`                                 |
| `*T`          | `&T`       | no        | gaining non-null guarantee                 |
| `*mut T`      | `&mut T`   | no        | gaining non-null guarantee                 |
| `&T`          | `&mut T`   | no        | gaining mutability                         |
| `*T`          | `*mut T`   | no        | gaining mutability                         |
| `nulltype`    | `&T`       | no        | references are non-null                    |

The general rule: **you may give up mutability, never gain it. There
is no implicit conversion between references and raw pointers in either
direction.**

`*T -> &T` and `*mut T -> &mut T` are not available in v0.1. They are
potentially unsafe (a null or dangling pointer becomes a "valid"
reference), and will be revisited in a future `unsafe` model.

## 5. Null

`nullptr` has type `nulltype`. `nulltype` implicitly converts to any
pointer type, but not to any reference type.

```xe
let p: *i32 = nullptr;           // ok
let q: *mut i32 = nullptr;       // ok
let r: &i32 = nullptr;           // error: references are non-null
```

Pointer comparisons with `nullptr` are specified in `expressions.md`
§4.2.

## 6. Pointer arithmetic

Pointer arithmetic is **not available in v0.1**. There is no `p + 1`,
no `p[i]` for raw pointers, and no way to compute an offset from a
pointer.

Pointers in v0.1 can be:

- Created with `&raw x` or `&raw mut x`.
- Null.
- Dereferenced with `*`.
- Compared with `==` and `!=`.
- Passed to functions and returned from functions.

They cannot be offset, indexed, or incremented.

## 7. Pointers to pointers

`**T` is legal. Each pointer layer needs an explicit `*`:

```xe
let a: i32 = 1;

let p: *i32 = &raw a;
let pp: **i32 = &raw p;       // pointer to pointer

let b: i32 = **pp;            // two explicit derefs
```

## 8. References and pointers in signatures and fields

### 8.1 Function parameters

A function that reads through a reference takes `&T`:

```xe
func print_value(x: &i32) {
    // x is auto-dereferenced
}
```

A function that writes through a reference takes `&mut T`:

```xe
func increment(x: &mut i32) {
    x += 1;
}
```

A function that takes a possibly-null or raw address takes `*T` or
`*mut T`:

```xe
func read_maybe(p: *i32) -> bool {
    return p != nullptr;
}
```

**The caller always writes the operator.** There is no implicit
conversion that creates a reference or pointer from a value (the one
exception is a method receiver; see `methods.md`):

```xe
let a: i32 = 1;
print_value(&a);              // & creates &i32
read_maybe(&raw a);           // &raw creates *i32

let mut b: i32 = 1;
increment(&mut b);            // &mut creates &mut i32
read_maybe(&raw b);           // &raw creates *i32

print_value(a);               // error: expected &i32, got i32
increment(&raw mut b);        // error: expected &mut i32, got *mut i32
```

The last line is a type error because `&raw mut b` produces a raw
pointer, not a reference.

### 8.2 Struct fields

A struct may contain references and pointers:

```xe
type Pair {
    first:  &i32;
    second: *mut i32;
}
```

The pointed-to or referred-to values must outlive the struct. v0.1 does
not check this. Storing a reference or pointer to a local in a struct
that outlives the local produces a dangling reference or pointer.

### 8.3 Return types

A function may return a reference:

```xe
func first(p: &Pair) -> &i32 {
    return p.first;
}
```

Or a pointer:

```xe
func maybe_first(p: *Pair) -> *i32 {
    if p == nullptr {
        return nullptr;
    }
    return &raw (*p).first;
}
```

Returning a reference that outlives its referent is undefined behavior.
v0.1 does not check this.

> **Note:** In `first` above, `p.first` has type `&i32`, so what is
> returned is a reference, not a copied `i32`. This depends on the open
> question in §2.2 about using a reference-typed value where a reference
> is expected.

## 9. Memory safety

Xenon v0.1 does **not** enforce memory safety at the compiler level:

- References are not lifetime-checked. A `&T` may outlive the value it
  refers to.
- Dangling references and pointers are possible.
- Null dereference is possible through pointers.
- Dereferencing a dangling or null pointer is undefined behavior.

Dangling-reference detection is deferred to a later semantic-analysis
iteration (v0.2).

The one check that is always performed is **array bounds checking** (see
`types.md` §3.1 and `expressions.md` §2.4).