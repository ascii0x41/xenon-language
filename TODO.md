# Xenon TODO

## v0.1 — Core Language

### Variables and Functions

* [x] Immutable and mutable bindings (`let`, `let mut`)
* [x] Uninitialised local bindings

  * [x] Track definite initialisation
  * [x] Warn when an uninitialised value is read
* [x] Functions

  * [x] `pub? static? func <name>(<params>)[-> <return type>]?`
  * [x] External functions via `#[extern(...)]`
  * [x] `#[noreturn]`
* [x] Function overloading

  * [x] Exact-match overload resolution
  * [x] Widening/conversion fallback
  * [x] Integer literals default to `i32` for overload resolution
  * [x] `mut` does not participate in ordinary function signatures

### Control Flow

* [x] `if`
* [x] `else`
* [x] `else if`
* [x] `while`
* [x] `return`
* [x] `break`
* [x] `continue`
* [x] Ternary operator
* [x] Non-void functions require a valid return on every reachable path
* [x] `if` is not an expression
* [x] Blocks are not value-producing expressions
* [x] No `for` loops in v0.1

### Types

* [x] Primitive integer types
* [x] Primitive floating-point types
* [x] `bool`
* [x] `rune`
* [x] `str`
* [x] `nulltype`
* [x] `nullptr`
* [x] Fixed-size arrays `[T; N]`
* [x] Struct types via `type`

```xenon
type Point {
    x: f64;
    y: f64;
}
```

* [x] Struct field visibility
* [x] Private fields cannot be initialised through struct literals outside their defining module
* [x] Named struct literals
* [x] Ordered struct literals

```xenon
Point { x: 1.0, y: 2.0 }
Point { 1.0, 2.0 }
```

### References and Pointers

* [x] Immutable references

  * [x] `&x`
  * [x] `&T`
* [x] Mutable references

  * [x] `&mut x`
  * [x] `&mut T`
* [x] Immutable raw pointers

  * [x] `&raw x`
  * [x] `*T`
* [x] Mutable raw pointers

  * [x] `&raw mut x`
  * [x] `*mut T`
* [x] No unary dereference operator for references
* [x] No references to rvalues
* [x] `&"literal"` is invalid
* [x] Reference mutability rules
* [x] Pointer mutability rules
* [x] Pointer equality
* [x] `nulltype` implicitly converts to pointer types
* [x] `*T == *mut T` comparisons
* [ ] Complete pointer/reference conversion rules

### Structs and Methods

* [x] Structs via `type`
* [x] `impl` blocks
* [x] Methods
* [x] Static methods
* [x] Fields
* [x] Static fields
* [x] Method receiver syntax:

  * [x] `self`
  * [x] `&self`
  * [x] `&mut self`
* [x] Implicit receiver passing in method calls
* [x] Mutable receiver validation
* [x] Method visibility

### Operators

* [x] Arithmetic operators
* [x] Comparison operators
* [x] Logical operators
* [x] Bitwise operators
* [x] Shift operators
* [x] Assignment
* [x] In-place assignment
* [x] Unary operators
* [x] Ternary operator
* [x] Operator precedence
* [x] Integer widening rules
* [x] No operator overloading in v0.1

### Arrays

* [x] Fixed-size arrays `[T; N]`
* [x] Array literals
* [x] Bounds-checked indexing
* [x] Array indices range from `0` through `N - 1`
* [x] `N` must be greater than zero
* [x] Out-of-bounds access is a runtime failure
* [x] v0.1 array sizes are non-zero unsigned integer literals
* [ ] Full indexing type rules

### Modules

* [x] `module <name>;`
* [x] `import <module>;`
* [x] Nested module paths
* [x] One module per `.xe` source file
* [x] `runtime.xe` special case
* [x] Module/import declarations at the top of the file
* [ ] Final project/file-system module mapping

### Annotations and Documentation

* [x] `#[extern(...)]`
* [x] `#[noreturn]`
* [x] `///` documentation comments

### Numeric Semantics

* [x] Integer widening
* [x] Integer literal default type: `i32`
* [x] Floating literal default type: `f32`
* [x] Floating-point suffixes

  * [x] `f32`
  * [x] `f64`
  * [x] `f`
  * [x] `d`
* [x] Reject unsupported mixed signed/unsigned/floating operations
* [ ] Final implicit numeric-conversion table
* [ ] Final signed-shift semantics
* [ ] Final overflow semantics

### Runtime / Safety

* [x] Debug overflow checks
* [x] Release-mode overflow behaviour
* [x] Bounds checking
* [x] `panic`
* [ ] Fully document panic semantics
* [ ] Dangling-reference detection deferred to v0.2

---

## v0.2 — Type System Expansion, Generics, Traits, Consts, and Operator Overloading

## Type System

* [ ] Type aliases

```xenon
type Age = u8;
```

* [ ] `Option<T>`

```xenon
type Option<T> {
    has: bool;
    value: T;
}

impl<T> Option<T> {
    pub static func some(value: T) -> Option<T> {
        return Option::<T> {
            has: true,
            value: value
        };
    }

    pub static func none() -> Option<T> {
        return Option::<T> {
            has: false
        };
    }
}
```

* [ ] `Result<T, E>`

```xenon
type Result<T, E> {
    success: bool;

    union {
        value: T;
        error: E;
    };
}

impl<T, E> Result<T, E> {
    pub static func ok(value: T) -> Result<T, E> {
        return Result::<T, E> {
            success: true,
            value: value
        };
    }

    pub static func err(error: E) -> Result<T, E> {
        return Result::<T, E> {
            success: false,
            error: error
        };
    }
}
```

Generic type arguments may be inferred from the expected return type, allowing:

```xenon
func divide(a: i32, b: i32) -> Result<i32, str> {
    if b == 0 {
        return Result::err("division by zero");
    }

    return Result::ok(a / b);
}
```

* [ ] Union types / union storage
* [ ] Anonymous unions inside `type` declarations
* [ ] Union members occupy overlapping storage
* [ ] Union storage size is based on the largest member, subject to alignment/padding
* [ ] A union may have at most one active member
* [ ] Reading an inactive union member is invalid
* [ ] Struct literals may initialise at most one member of a given union
* [ ] Define rules for uninitialised union storage
* [ ] Define union member lifetime/destruction semantics
* [ ] Define union behaviour when assigning a new active member

Example:

```xenon
type Result<T, E> {
    success: bool;

    union {
        value: T;
        error: E;
    };
}
```

Valid:

```xenon
Result::<i32, str> {
    success: true,
    value: 42
}
```

Valid:

```xenon
Result::<i32, str> {
    success: false,
    error: "division by zero"
}
```

Invalid:

```xenon
Result::<i32, str> {
    success: false,
    value: 42,
    error: "division by zero"
}
```

* [ ] Formalise Copy semantics

  * [ ] Which primitive types are Copy?
  * [ ] Which structs are Copy?
  * [ ] Copy rules for arrays
  * [ ] Reference copy semantics
* [ ] Formalise move semantics
* [ ] Generic type checking
* [ ] Type inference improvements
* [ ] Generic associated/static function inference
* [ ] Type conversions/coercions fully specified


## Const Evaluation

* [ ] `const` declarations

```xenon
const PI: f64 = 3.14159265358979323846;
```

* [ ] Compile-time constant expressions
* [ ] Constants usable as array sizes
* [ ] Constant-expression validation

## Generics

* [ ] Generic types

```xenon
type Box<T> {
    data: *mut T;
}
```

* [ ] Generic `impl` blocks

```xenon
impl<T> Box<T> {
    ...
}
```

* [ ] Generic functions

```xenon
func identity<T>(x: T) -> T {
    return x;
}
```

* [ ] Generic type checking
* [ ] Infer generic arguments from expected return types
* [ ] Monomorphization strategy
* [ ] Trait bounds
* [ ] Turbofish syntax

```xenon
foo::<i32>(42);
```

## Traits

* [ ] `trait` declarations
* [ ] Trait implementations
* [ ] Trait bounds
* [ ] Trait method resolution
* [ ] Associated functions
* [ ] Associated types

Example:

```xenon
trait Serialise {
    func serialise(&self) -> str;
}

impl Serialise for Point {
    func serialise(&self) -> str {
        ...
    }
}
```

## Operator Overloading

* [ ] Operator overloading
```
type Point {
    x: f64;
    y: f64;
}

impl Point {
    pub operator+(&self, other: Point) -> Point {
        return Point {
            x: self.x + other.x,
            y: self.y + other.y
        };
    }

    pub operator-(&self other: Point) -> Point {
        return Point {
            x: self.x - other.x,
            y: self.y - other.y
        };
    }
}
```
* [ ] Define which operators can be overloaded (Arithmetic, ordering, equality, bitwise. No logic. No reference. No dereference)
* [ ] Operator-to-function mapping
* [ ] Operator overload resolution
* [ ] Interaction with normal function overloading
* [ ] Restrictions on operator overload signatures

## Iterators

* [ ] Iterator traits
* [ ] `map`
* [ ] `filter`
* [ ] `fold`
* [ ] `collect`
* [ ] Lazy evaluation guarantees
* [ ] Iterator adapters

## Memory

* [ ] Dangling-reference analysis
* [ ] Lifetime/reference safety analysis
* [ ] `Box<T>`
* [ ] `Box<T>` ownership semantics
* [ ] `Box<T>::init(value: T) -> Box<T>`
* [ ] Box compiler intrinsic
* [ ] Dereference semantics for `Box<T>`

## Standard Library

* [ ] `module.toml`
* [ ] Cross-platform IO abstraction
* [ ] POSIX support
* [ ] Windows support
* [ ] `Inf`
* [ ] `NaN`
* [ ] `-lm` C interoperability where required

---

# v0.3 — Error Handling, Enums, Pattern Matching, and Iteration Syntax

## Error Handling

* [ ] `Result<T, E>` error-handling conventions
* [ ] `?` propagation operator
* [ ] Early-exit semantics
* [ ] Error-handling standard library

## Enums

* [ ] Enums
* [ ] Unit variants
* [ ] Data-carrying variants
* [ ] Enum construction
* [ ] Enum comparison
* [ ] Enum methods

Example:

```xenon
enum Colour {
    RED,
    BLUE,
    GREEN
}
```

## Pattern Matching

* [ ] `match`
* [ ] Enum pattern matching
* [ ] Destructuring
* [ ] Exhaustiveness checking
* [ ] Guards

## Iteration Syntax

* [ ] `for` loops

```xenon
for i in 0..10 {
    ...
}
```

* [ ] `for` over iterators
* [ ] `for` over ranges
* [ ] `for` integration with iterator traits

## Type System

* [ ] Generic type aliases

```xenon
type Pair<T> = [T; 2];
```

* [ ] Associated types
* [ ] Advanced trait constraints

---

# v0.4+ — Future / Uncertain

## Language

* [ ] Lambda/closure syntax
* [ ] Advanced pattern matching
* [ ] Destructuring assignment
* [ ] Function argument enhancements
* [ ] Tuples
* [ ] Slices
* [ ] Dynamic arrays
* [ ] Additional collection types

## Memory / Safety

* [ ] Advanced lifetime analysis
* [ ] More sophisticated ownership analysis
* [ ] Smart-pointer abstractions
* [ ] Additional allocation strategies

## Tooling

* [ ] Package manager
* [ ] Build system
* [ ] `xec build <path-to-project>`
* [ ] LSP
* [ ] Formatter (`xenonfmt`)
* [ ] Documentation generator
* [ ] Debugger integration

## Standard Library

* [ ] Comprehensive IO library
* [ ] Filesystem API
* [ ] Networking
* [ ] Collections
* [ ] Concurrency/threading
* [ ] Time/date APIs

---

# Rejected Ideas

* Constructors — structural literals + explicit `init` functions cover this.
* Operator overloading in v0.1 — deferred to v0.2.
* `for ... in` loops in v0.1 — deferred until iterator support exists.
* Inline modules — one `.xe` file corresponds to one module.
* `use` declarations — `import` is the module import mechanism.
* `crate` — no crate concept in Xenon.
* Implicit references in ordinary function calls — use `&x` / `&mut x` explicitly.
* Dereferencing references with unary `*` — references are accessed directly.
