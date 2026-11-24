# Xenon TODO

## v0.1 — Core Language

### Variables and Functions

* [x] Immutable and mutable bindings (`let`, `let mut`)
* [x] Uninitialised local bindings

  * [ ] A binding without an initializer must be `mut` (assign-once immutable bindings deferred to v0.2)
  * [ ] Track initialisation by textual order (not path-sensitive)
  * [ ] Reading an uninitialised binding is a compile-time error (was: warning)
  * [ ] Compound assignment counts as a read
* [x] Functions

  * [x] `pub? static? func <name>(<params>)[-> <return type>]?` (`static` only inside `impl`)
  * [x] External functions via `#[extern(...)]`
  * [x] `#[noreturn]`
  * [ ] `#[link_name("symbol")]`
  * [ ] Return-path analysis: `#[noreturn]` calls terminate, `while true` without `break` terminates, warn on unreachable code
* [ ] Function overloading (dominance-based)

  * [x] `mut` does not participate in ordinary function signatures
  * [ ] Contextual literal inference (a literal adopts a candidate's parameter type if the value fits)
  * [ ] Per-argument conversion ranks: exact, then widening by distance; conversions to `usize` are incomparable with other widening
  * [ ] Dominance selection; ambiguity error on ties (never declaration order)
  * [ ] Defaulting last: `i32` / `f32` only as a tie-break after resolution

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
* [x] Fixed-size arrays `[N]T` (length first: `[2][3]f64` indexes as `a[i][j]`)
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
* [ ] Every field must be initialised in a struct literal (a missing field is a compile error)

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
* [ ] Complete pointer/reference conversion rules (decided in DOCS-TODO.md; write into the specs)
* [ ] Reference copy rule: a `&T` used where `&T` is expected copies the reference (`let r2 = r;`)
* [ ] References cannot be rebound; `&&T` and `&r` are errors
* [ ] Reserve `raw` as a keyword

### Structs and Methods

* [x] Structs via `type`
* [x] `impl` blocks
* [x] Methods
* [x] Static methods (`static func`; a function with neither a receiver nor `static` is an error)
* [x] Fields
* [x] Static fields (`static let [mut]`, literal initialisers only)
* [x] Method receiver syntax:

  * [x] `self`
  * [ ] `mut self`
  * [x] `&self`
  * [x] `&mut self`
* [x] Implicit receiver passing in method calls
* [ ] Receiver overload choice: a mutable place prefers `&mut self` and falls back to `&self`; an immutable place uses `&self`
* [ ] Mixing by-value and reference receivers under one method name is an error
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
* [ ] `usize` widening: only `u8` and `u16` convert implicitly
* [ ] No `++` / `--`
* [ ] Assignment has type `void` (no chained assignment, no assignment in conditions)
* [ ] A literal shift count at least the bit width is a compile error
* [x] No operator overloading in v0.1

### Arrays

* [x] Fixed-size arrays `[N]T` (length first: `[2][3]f64` indexes as `a[i][j]`)
* [x] Array literals (list form only; the element type comes from the annotation or defaulting; no typed literal)
* [x] Bounds-checked indexing
* [x] Array indices range from `0` through `N - 1`
* [x] `N` must be greater than zero
* [x] Out-of-bounds access is a runtime failure
* [x] v0.1 array sizes are non-zero unsigned integer literals
* [ ] Full indexing type rules (decided: index is `usize`, integer literals infer it; write into the specs)

### Modules

* [x] `module <name>;`
* [x] `import <module>;`
* [x] Nested module paths (names only; no inline or nested modules)
* [x] One module per `.xe` source file
* [x] `runtime.xe` special case
* [x] Module/import declarations at the top of the file
* [ ] Final project/file-system module mapping (decided: `source_files` list in `xenon.toml`, name from the `module` declaration; write into `modules.md`)
* [ ] Duplicate module names are an error
* [ ] Importing a namespace prefix that no file declares is an error
* [ ] Every function in the environment's `required_functions` exists and matches its signature (replaces "exactly one `main` module")
* [ ] Imports are referred to by full path (`utils::helpers::f()`)
* [ ] Standard-library modules are discovered from `<installation>/stdlib/*/module.toml` and parsed only when imported
* [ ] A user module named like a standard-library module is an error (recommended; see open decisions)
* [ ] `module <name>;` is optional: a file without one contributes its declarations to the global scope (like `runtime.xe`)
* [ ] Leading `::` paths (`::say`) refer to the global scope; lookup order is local scopes, current module, then global scope (parser: `Path` gets a global flag)
* [ ] Global-scope corner cases: privacy of non-`pub` items, imports in global files, redeclaration rules (recommendations in `DOCS-TODO.md`)

### Annotations and Documentation

* [x] `#[extern(...)]`
* [x] `#[noreturn]`
* [ ] `#[require_capability("name")]` on functions and modules (checked after overload resolution; unknown names are errors)
* [x] `///` documentation comments

### Numeric Semantics

* [x] Integer widening
* [x] Integer literal default type: `i32`
* [x] Floating literal default type: `f32`
* [x] Floating-point suffixes

  * [x] `f32`
  * [x] `f64`
  * [ ] Confirm the `f` and `d` suffixes: they are not in `literals.md`. Either specify them or remove them
* [x] Reject unsupported mixed signed/unsigned/floating operations
* [ ] Negative literal range check (`-128i8` is valid, `-1u8` is an error)
* [ ] Float literal overflow is a compile error
* [ ] Final implicit numeric-conversion table (decided; write into `types.md`)
* [ ] Final signed-shift semantics (decided: signed `<<` is bitwise, signed `>>` is a compile error; write into `expressions.md`)
* [ ] Final overflow semantics (decided: debug panics, release wraps; write into `types.md`)

### Runtime / Safety

* [x] Debug overflow checks
* [x] Release-mode overflow behaviour
* [x] Bounds checking
* [x] `panic`
* [ ] Fully document panic semantics (decided: message to stderr, exit code 101, never returns; write into `runtime.md`)
* [ ] `assert(condition, message)` and `exit(code)`
* [ ] `println` takes only `str`; runtime items are in scope in every module (each environment target supplies its own `runtime.xe`)
* [ ] Xenon exit code standard (hosted environments only)
* [ ] Dangling-reference detection deferred to v0.2

### Toolchain (v0.1)

* [ ] Six logical phases: preprocessing, parsing, pre-analysis, semantic analysis, code generation, linking
* [ ] CLI `xec build <project>`: `CLIArgs` (`command`, `project_dir`, `target`, `environment`, `optimisation`, `output`)
* [ ] `xenon.toml` loader: `ProjectConfig` with `[project]`, `[build]` and `[capabilities]`; omitted values preserved until resolution
* [ ] `environment.toml` loader: `Environment`, `Target`, `required_functions`, capabilities, `configurable`. No `default_target`
* [ ] Standard-library discovery from `<installation>/stdlib/*/module.toml`; parse a module's `.xe` only when it is imported
* [ ] `ResolvedConfigs` resolver. Defaults: environment `xenon`, target required, optimisation `debug`, output `build/`. Capabilities are merged environment, then target, then project
* [ ] `optimisation = "debug" | "release"` resolved into concrete settings (overflow checks, optimisation level)
* [ ] Capabilities `fpu` and `heap_alloc` are metadata only in v1: merged into `ResolvedConfigs`, never checked
* [ ] `#[require_capability]` parses as an ordinary annotation (including on modules) and is ignored in v1. Enforcement is future work
* [ ] `required_functions`: each entry is the widest signature; the program's function may omit trailing parameters, nothing else may differ (omitted return type means `void`). Hosted `xenon`: `main::main(usize, *str) -> i32`
* [ ] Emit required functions with C linkage as `<module>__<name>` (`main__main`); the C runtime always calls `main__main(argc, argv)`, so the compiler emits the full signature and ignores unused parameters
* [ ] Hosted C entry builds the `argv` `str` array on its own stack (no heap, not UTF-8 validated)
* [ ] Compile the environment target's C source and imported stdlib C sources for the selected target (requires Clang)
* [ ] Link to ELF and apply the target's `linker_script`
* [ ] `-lm`: stdlib `module.toml` `link_libs` (`math` names `m`), so `math` works on hosted `xenon` in the first release
* [ ] Only the `xenon` environment is implemented in v1 (`caesium` is illustrative)
* [ ] `math` as the only v1 standard-library module
* [ ] Resolve the remaining open decisions in `DOCS-TODO.md`: global-scope corner cases, stdlib naming, C build defines and ABI, output layout

### Compiler Internals (v0.1)

* [ ] Rewrite the module system: a flat `ModuleGraph` keyed by declared name, no namespace tree
* [ ] Phase 3: duplicate module names, import resolution (report unresolved imports at the import's location, plus self-imports), dependency graph, Kahn's algorithm, cycle reporting
* [ ] Traverse modules in dependency order for semantic analysis
* [ ] Restructure the driver into the six phases
* [ ] Remove the stale duplicate `compiler.cpp`
* [ ] Lexer: reserve `raw`, and produce no `**` or `++` tokens
* [ ] Write `lexical.md`
* [ ] Write the new docs: pipeline, configuration, environments, standard library, capabilities

---

## v0.2 — Type System Expansion, Generics, Traits, Consts, and Operator Overloading

## Type System

* [ ] Type aliases

```xenon
type Age = u8;
```

* [ ] `Option<T>`

> Note: `none()` below omits `value`, which v0.1's rule that every field must be initialised forbids. Make `Option` a union like `Result`, or define an explicit uninitialised-field form.

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
* [ ] Explicit casts (`cast<T, U>` or `T{ x }` syntax). Define out-of-range behaviour; checked and panicking is recommended
* [ ] Path-sensitive initialisation analysis; allow assign-once immutable bindings
* [ ] Overload resolution: consider a "closest widening" rule (ties are ambiguity errors in v0.1)


## Const Evaluation

* [ ] `const` declarations

```xenon
const PI: f64 = 3.14159265358979323846;
```

* [ ] Compile-time constant expressions
* [ ] Constants usable as array sizes
* [ ] Constant-expression validation
* [ ] `static let` initialisers may use constant expressions (literal-only in v0.1)

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

    pub operator-(&self, other: Point) -> Point {
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

* [ ] `module.toml` (full library manifest; v1 uses the minimal `stdlib/<name>/module.toml`)
* [ ] Cross-platform IO abstraction
* [ ] POSIX support
* [ ] Windows support
* [ ] `Inf`
* [ ] `NaN`
* [ ] General C library linking beyond stdlib `link_libs` (v1 covers `math`'s `-lm`)

---

## v0.3 — Error Handling, Enums, Pattern Matching, and Iteration Syntax

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
type Pair<T> = [2]T;
```

* [ ] Associated types
* [ ] Advanced trait constraints

---

## v0.4+ — Future / Uncertain

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
* [ ] Build system (beyond `xec build`, which is v0.1)
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
* Scalar construction `T{ x }` in v0.1 — redundant with suffixes. Kept as a candidate cast syntax for v0.2.
* `++` / `--` — use `+= 1`.
* Assign-once initialisation of immutable bindings in v0.1 — needs path-sensitive analysis.
* Lazy first-access initialisation of statics — static initialisers are literals in v0.1.
* Mixing by-value and reference receivers under one method name.
* Library manifests beyond the minimal stdlib `module.toml`, and dependency resolution, in v1.
* A `LinkPlan` abstraction, a library registry class, and prebuilt object-file matrices in v1 — environment C sources are compiled at build time.
* A separate target-triple parser — the target table holds the `llvm_triple` string.
* Capability checking, negotiation and `#[require_capability]` enforcement in v1 — capabilities are metadata only.
* A separate runtime-component system in v1.
* Implementing environments other than `xenon` in v1 — `caesium` is illustrative.
* UF2, hex and raw binary output in v1 — ELF only; flashing and conversion are external.