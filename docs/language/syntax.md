# Syntax

## Variables & Declarations

### Constants
Use `let` to declare immutable constants:
```
let x: i32 = 42;
let name: string = "Xenon";
```

### Variables
Use `var` to declare mutable variables:
```
var count: i32 = 0;
var data: Array<i32> = [1, 2, 3];
```

## Built-in Types

### Numeric Types

#### Integer Types
- `i8`, `i16`, `i32`, `i64` — signed integers (`i64` aliased as `int`)
- `u8`, `u16`, `u32`, `u64` — unsigned integers (`u64` aliased as `uint`)

#### Floating-Point Types
- `f32` — 32-bit floating-point
- `f64` — 64-bit floating-point (aliased as `float`)

#### Complex Numbers
- `complex` — complex numbers (64-bit real + 64-bit imaginary)

#### Integer Literals

| Format | Example | Notes |
|--------|---------|-------|
| Decimal | `42`, `1_000_000` | Underscores for readability |
| Hexadecimal | `0xFF`, `0xdead_beef` | Prefix with `0x` |
| Binary | `0b1010`, `0b1111_0000` | Prefix with `0b` |
| Octal | `0o755`, `0o777` | Prefix with `0o` |

#### Floating-Point Literals

| Format | Example | Notes |
|--------|---------|-------|
| Decimal | `3.14159`, `0.5` | Standard decimal point |
| Scientific | `1e6`, `2.5e-3`, `1.0e9` | `e` for exponent (base 10) |

#### Numeric Suffixes

Specify type by appending the type name to the literal:

```xenon
let a = 42i8;      // i8
let b = 100u16;    // u16  
let c = 3.14f32;   // f32
let d = 255u8;     // u8
let e = 1000i64;   // i64
```

When no suffix is given, integers default to int (i64) and floats default to float (f64).

```
let z1 = 3 + 4i;      // 3 + 4i
let z2 = -1.5i;       // 0 - 1.5i
let z3 = 42;          // 42 + 0i (promotes to complex)
```

Note: The i suffix creates an imaginary number. Combine with real part using + or -.

#### Examples

```
let decimal = 42;              // int
let hex = 0xFF;                // int (255)
let binary = 0b1010;           // int (10)
let octal = 0o755;             // int (493)
let with_underscores = 1_000_000;  // int (readability)

let scientific = 1.5e-4;       // float (0.00015)
let sized = 255u8;              // u8
let complex_num = 3 + 4i;       // complex

let BYTE_MAX = 255u8;
let ZERO = 0;                   // int, default 0
```

### Default Values

All numeric types default to 0 (or 0.0 for floats, 0 + 0i for complex).

---

### Boolean
- `bool` — true or false (default)

### Pointers & References
- `ptr T` — nullable immutable raw pointer to type T (cannot mutate pointee)
- `mut ptr` - nullable mutable raw pointer to type T

- `ref T` — immutable reference to T (cannot mutate pointee)
- `mut ref T` — mutable reference to T

Note: `ptr` and `mut ptr` are `nullptr` by default

### Collections
- `[T; N]` — static array of N elements of type T
- `array<T>` — dynamic array of type T
- `{K: V}` — map with key type K and value type V
- `(T1, T2, T3)` — tuple with heterogeneous types

## Truthy and Falsey

|   Truthy                        | Falsey        
|---------------------            |----------------
| Non-0 numbers                   | `0`, `0x0`, `0b0`, `0o0`
| Non-empty strings and containers| Empty strings and containers
| `true` literal                  | `false` literal
| Non-null pointers               | Nulled pointers and `nullptr` literls

## Comments

Line comments use `//`:
```
// This is a comment
```

Block comments use `/* ... */`:
```
/* This is a
   multi-line comment */
```

## Code Blocks

Code blocks are denoted with curly braces `{}`:
```
func main() -> i32 {
    let x = 1;
    let y = 2;
}
```

## Semicolons

Semicolons are **mandatory** at the end of statements:
```
let x = 42;
var y = 0;
x = y + 1;
```

## Functions

Function syntax:
```
func name<T, U>(p1: T, p2: T) -> U { ... }
```

### Function Components
- `func` — function keyword
- `name` — function name
- `<T, U>` — optional generic parameters
- `(p1: T, p2: T)` — parameters with type annotations
- `-> U` — return type
- `{ ... }` — function body

### Generic Constraints
Functions can have constrained generic parameters:
```
func process<T: Printable + Comparable<U>, U>(params) { ... }
```

## Classes

Class syntax:
```
class name<T, U> impl Trait1, Trait2 { ... }
```

### Class Components
- `class` — class keyword
- `name` — class name
- `<T, U>` — optional generic parameters
- `impl Trait1, Trait2` — optional trait implementations

## Traits

Trait syntax:
```
trait Name<generics> {
    func name(params) -> ret;
    func name<generics>(params) -> ret;
    operator op(params) -> ret;
}
```

### Trait Rules
- Traits define public function or operator signatures
- Traits can have generic parameters
- Methods in traits can also have generic parameters
- Parameter names are ignored
- Methods in traits 

## Method Modifiers

Methods can have the following modifiers:
- `static` — static method (no instance)
- `mut` — method that mutates the instance
- `public` — public visibility
- `private` — private visibility (default)

Example:
```
class MyClass {

    var value: i32;

    public static func new() -> MyClass { ... }
    
    public func getValue() -> i32 { ... }
    
    public mut func setValue(value: i32) { ... }
    
    func internal() { ... }
}
```

## Operators

### Overloadable operators include:

- Arithmetic Operators: `+`,`-`,`*`,`/`,`%`,`+=`,`-=`,`*=`,`/=`,`%=`
- Bitwise Operators:  `&`,`|`,`~`,`^`,`<<`,`>>`
- Comparison Operators: `==`,`!=`,`<`,`<=`,`>`,`>=`
- Index Operator: `[]`
- Call Operator: `()`
- Derference Operator: `*`
- Address Operator: `&`

Example:
```
class Point3D {
    var x: int;
    var y: int;
    var z: int;

    static func new(x: int, y: int, z: int) -> Point3D {
        Point3D result;
        result.x = x;
        result.y = y;
        result.z = z;

        return result;
    }

    // Unary minus
    operator-() -> Point3D {
        return Point3D::new(-this.x, -this.y, -this.z);
    }

    operator+(other: ref Point3D) -> Point3D {
        return Point3D::new(
            this.x + other.x,
            this.y + other.y,
            this.z + other.z);
    }

    operator+=(other: ref Point3D) -> mut ref Point3D {
        this.x += other.x;
        this.y += other.y;
        this.z += other.z;
        return *this;
    }
}
```

## Control Flow

### Selection

Example
```
let x = 7

if x > 10 {
    writeln("x is greater than 10")
} elif x > 5 {
    writeln("x is between 5 and 10")
} else {
    writeln("x is less than 5")
}
```

### Iteration

#### `foreach` loop

```
func fibonacci(n: uint) -> uint {
    var x: uint = 0;
    var y: uint = 1;

    foreach _ in range(n-1) {
        x, y = y, x+y;
    }
    return x;
}
```

#### `while` loop

```
func lcm(n: uint, m: uint) {
    while x != y {
        if x > y {
            x -= y;
        } else {
            y -= x;
        }
    }

    return x;
}
```

#### `do while` lopp

```
func main() -> i32 {
    var number: int;

    do {
        number = strtoi(readln("Enter a number"));
        writeln($"You entered {number}");
    } while number != 0;

    return 0;
}
```
