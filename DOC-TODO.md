# DOCS-TODO.md

Key points for every section of the Xenon documentation, condensed from the
scrap documents. Version naming follows the specs: **v0.1** is the current
language, **v0.2** is the next. (The old draft said "Xenon 1" / "Xenon 2".)

Items marked **[DECIDED]** resolve a conflict between scrap documents. They
are listed again in the Decision Log at the bottom. Items marked **[OPEN]**
still need your decision.

# README.md

- [ ] What is Xenon
- [ ] Project status
- [ ] Installation using `install.py`
- [ ] Links
  - [ ] Link to language overview
  - [ ] Link to compiler overview
- [ ] Credits

# Getting Started

- [ ] Creating `Hello, world!`

  ```xe
  module main;

  func main() -> i32 {
      println("Hello, world!");
      return 0;
  }
  ```

# Language Overview

- [ ] What Xenon is: compiled, statically typed systems language for
  scientific, numerical, embedded and aerospace work.
- [ ] Design principles: explicit over implicit; predictable numerics; fail
  loudly at compile time.
- [ ] Non-goals: source compatibility with other languages, transpilation,
  a garbage-collected runtime.
- [ ] Mental model: program = modules, one `.xe` file per module; execution
  begins at `main` in module `main`.
- [ ] Table of contents (see the section list below).

## Lexical Structure

- [ ] Write `lexical.md` (linked from the overview but not among the scraps).
  - Identifiers, keyword list, comments (`//`, `///` doc comments), whitespace.
  - Reserved words: `let mut func type impl static pub module import if else
    while return break continue true false nullptr raw self`.
  - **[DECIDED]** `raw` is reserved so `&raw x` is unambiguous.
  - Tokenisation notes: no `**` or `&&`-as-reference tokens. `**pp` lexes as two
    `*` tokens; `&&` is always logical AND. There is no `++` / `--`.
  - Annotation syntax `#[...]`.

## Primitives

- [ ] Numerical: `u8 u16 u32 u64`, `i8 i16 i32 i64`, `f32 f64`, `usize`
  - `u128`, `i128`, `f128` are planned, not in v0.1.
  - Implicit widening only when widening, same kind and same signedness:
    `uN -> uM`, `iN -> iM` (M > N), `f32 -> f64`.
  - Used uniformly by arithmetic, comparison, assignment, argument passing
    and `return`.
  - No implicit narrowing, signedness change, int-to-float or float-to-int.
  - **No explicit conversion syntax in v0.1.** `cast<T, U>` needs generics
    (v0.2).
  - IEEE 754 for all float semantics. `-0.0` is distinct from `0.0`.
  - Unconstrained literals default to `i32` / `f32`.
  - Overflow: panic in debug builds. **[DECIDED]** In release builds the result
    wraps (two's complement). It is defined behaviour, but code must not rely
    on it. The old specs said "may wrap".
  - Integer division and remainder by zero panic in every build mode.
  - **[DECIDED]** `usize` widening: only `u8` and `u16` widen implicitly to
    `usize` (valid on every target). `usize` widens to nothing. `u32`/`u64`
    to `usize` needs `cast` (v0.2).
  - `let x: usize = -1;` is a compile error (unary minus on unsigned), so
    negative-to-`usize` cannot arise in v0.1.
  - v0.2 note: define `cast` out-of-range behaviour. Recommendation: `cast` is
    checked and panics in every build mode when the value is not representable,
    with a separately named wrapping/truncating form for when wrap is intended.

- [ ] `str`
  - Immutable UTF-8 byte sequence, a view that does not own its bytes.
  - Non-NUL-terminated. Never null, and the empty string has a non-null pointer.
  - Two words (`bytes: *u8`, `length: usize`). **[DECIDED]** the field is named
    `length`, as the draft used `len`. The shape is conceptual and not
    accessible to programs.
  - Only source is a string literal (read-only static data), so it never
    dangles.
  - No concatenation, no allocation. `String` is planned as a stdlib type.
  - `==` / `!=` compare bytes. No ordering.
  - **[DECIDED]** `s[i]` is not defined in v0.1. The specs said "if defined",
    and without casts nothing useful can be done with the result.

- [ ] `rune`
  - 32-bit Unicode scalar value (no surrogates, at most U+10FFFF).
  - Not an integer and no implicit conversion. Orderable.

- [ ] `bool`
  - `true`, `false`, one byte. Not ordered. No truthiness anywhere.

- [ ] `nulltype`
  - Type of `nullptr`. Converts implicitly to any pointer type, never to a
    reference. A pointer never converts to `nulltype`.
  - `nullptr` with no constraining pointer context is an error.
  - Cannot be dereferenced.

- [ ] `void`
  - Return type of a function that returns without a value.
  - Not allowed as a variable, field, array element or parameter type, nor in
    value position.
  - A function that never returns is marked `#[noreturn]`. Its call is
    treated as terminating in return-path analysis (see Statements).

## Literals

- [ ] Literal typing uses metavariables `{integer}` and `{float}`
  - Unify with any integer or float type from context, otherwise default to
    `i32` / `f32`.
  - Deliberate exception to "explicit over implicit". They never escape
    inference.
- [ ] Integer literals
  - Decimal, `0x`, `0b`, `0o`.
  - Digit separators `_` only between digits.
  - Suffixes `u8 … u64 usize i8 … i64`, optionally `_`-separated (`1_u8`). The
    style guide prefers `1u8`.
  - Range-checked at compile time.
  - Negative literals: `-N` is unary minus applied to `N`. Sema range-checks the
    negated value, so `-128i8` is OK and `128i8` is an error. `-1u8` is an
    error.
- [ ] Float literals
  - Must contain `.` or an exponent. No leading or trailing dot (`.5`, `1.`).
  - Suffixes `f32`, `f64`.
  - **[DECIDED]** `0.1d` is not a suffix and is removed from the spec.
  - Literal overflow (`1.0e40f32`) is a compile error. Runtime overflow follows
    IEEE 754.
  - No NaN/Inf literals.
- [ ] Rune literals
  - Exactly one scalar value. Escapes `\n \t \r \0 \\ \' \" \u{…}`.
  - Errors: `''`, `'ab'`, surrogates, > U+10FFFF, bare newline.
- [ ] String literals
  - UTF-8, same escapes as runes, plus raw strings `r"…"` and `r#"…"#`.
  - Type is always `str`, never `*u8`, `[u8; N]` or `&[u8; N]`.
  - Not addressable, so `&"hello"` is an error.
- [ ] Boolean and `nullptr` literals.
- [ ] Array literals
  - List form `[1, 2, 3]` and typed form `[f32; 3] { … }`.
  - No repeat form, no empty literal (`[T; 0]` is invalid).
  - Element count must equal `N`. Element metavariables unify across the array.
- [ ] Struct literals
  - Named `Point { x: 1, y: 2 }` (any order) or positional `Point { 1, 2 }`
    (declaration order). No shorthand, so `Point { x, y }` is positional.
  - **[DECIDED]** A literal must initialise every field. Missing fields are a
    compile error, not UB. The old text left them UB, and the draft TODO said
    they are not permitted.
  - **[DECIDED]** A literal may only initialise fields accessible from the
    current module. Combined with the rule above, a type with any private
    field can only be constructed inside its defining module. Outside it,
    use a `pub` constructor (`Age::init(42)`).
    The draft said "within the type's defining implementation", but the module
    rule from `modules.md` is the one the analyser can check.
- [ ] Scalar construction `usize { 42 }`
  - **[DECIDED]** Removed from v0.1. Use suffixes (`42usize`) or a typed
    binding. `T { … }` means struct literals only.
  - v0.2: `T{ x }` is a candidate spelling for explicit casts. Casting needs a
    proper design pass (syntax, out-of-range behaviour) and is deferred.
- [ ] Sema checklist for literals (keep the list from `literals.md` §10).

## Expressions

- [ ] Expressions have a compile-time type. Every expression is a **place** or
  a **value**.
- [ ] Unary: `+ - ~ ! * & &mut &raw &raw mut`
  - `+ -` numeric, `~` integer, `!` bool.
  - Unary `-` on unsigned is an error.
  - `*` is for raw pointers only. `*r` on a reference is a type error.
- [ ] Binary
  - Arithmetic `+ - * /` on numerics with a common type. `%` and bitwise
    `& | ^` on integers only. Float `%` is not defined.
  - Common type comes from the widening rules. If none exists it is a compile
    error. Mixed signedness, and int/float mixes, are errors.
  - Integer `/` truncates toward zero.
  - Comparison `== !=` on numerics (common type), `rune`, `bool`, `str` (byte
    content) and pointers (address; `*T` vs `*mut T` OK; `nullptr` OK).
    Comparing a reference to `nullptr` is an error. User types have no `==`.
  - Ordering `< <= > >=` on integers, floats and `rune` only. Not on `bool` or
    pointers. NaN comparisons follow IEEE.
  - `&& ||` require `bool` and short-circuit.
  - Shifts
    - `<<`: any integer left operand, result has the left type, discarded bits
      are dropped, no overflow check.
    - `>>`: the left operand must be unsigned (signed `>>` is a compile error),
      and the shift is logical.
    - The count may be any integer type. A count that is negative or at least the
      bit width panics in all build modes.
    - **[DECIDED]** A literal count at least the bit width is a compile error.
      This is added for "fail loudly at compile time".
- [ ] Assignment `= += -= *= /= %= &= |= ^= <<= >>=`
  - LHS must be a mutable place. RHS must be assignable (exact or widening).
  - Result type is `void`, so `a = b = c` and `if a = b` are errors.
  - `a op= b` is `a = a op b` with `a` evaluated once.
  - Compound assignment reads `a`.
  - There is no `++` / `--`. **[DECIDED]** The draft used `Counter::gCount++`,
    which is invalid. Use `+= 1`.
- [ ] Postfix
  - Indexing: `a[i]` on arrays only, `i: usize`, bounds-checked (panic).
    Integer literals infer as `usize`.
  - Call `f(x)`: arguments evaluated left to right.
  - `.` field access and method call.
- [ ] Ternary `c ? a : b`: `c` is `bool`, branches share a common type, neither
  is `void`, only the selected branch is evaluated, right-associative.
- [ ] Precedence (loosest to tightest): assignment, ternary, `||`, `&&`, `|`,
  `^`, `&`, equality, comparison, shift, additive, multiplicative, unary,
  postfix.
  - Equality and comparison are non-associative, so `a < b < c` is a parse error.
  - Bitwise operators bind tighter than equality, unlike C, so
    `f & M == E` means `(f & M) == E`.
- [ ] Places and values
  - Places: variables, field of a place, index of a place, `*p`, any
    reference-typed expression.
  - Values: literals, operator results, `&`/`&raw` expressions, ternary,
    non-reference calls.
  - Mutable places: `let mut` or `mut` params, fields/elements of mutable places,
    `*p` for `*mut T`, referent of `&mut T`.
  - Calls returning a reference designate the referent's storage.
- [ ] No operator overloading in v0.1 (function overloading does exist).

## Statements

- [ ] Statement kinds: `let`, expression statement, assignment, block, `if`,
  `while`, `return`, `break`, `continue`.
  - **[DECIDED]** `break` and `continue` are part of v0.1 (confirmed).
    Remove the open question from `statements.md`.
- [ ] No `for` in v0.1 (needs iterators, v0.2).
- [ ] Statement vs expression: blocks and `if` are statements, never values.
  Use `?:` for conditional values. `let x = if …` is an error.
- [ ] Blocks
  - `{ stmt* }` introduces a scope. A block has no value.
  - A binding is visible from its declaration to the end of its block.
  - Shadowing an outer scope's binding is allowed. Redeclaring a name in the
    *same* scope is an error. Shadowing does not change the old binding.
  - **[DECIDED]** Function parameters live in the function body's outermost scope,
    so `let a` in the body with a parameter `a` is a redeclaration error.
- [ ] Expression statements
  - `expr;` discards the value. A `void` expression is fine here and an error
    only in value position.
- [ ] `if` / `else if` / `else`
  - Condition must be `bool`. Braces always required.
- [ ] `while`
  - Condition must be `bool`, evaluated before every iteration. Braces required.
    No `else`.
- [ ] `break` / `continue`
  - Apply to the innermost loop. No labels in v0.1. Using either outside a loop
    is an error.
- [ ] `return`
  - `return expr;` converts to the return type (implicit widening allowed).
  - `return;` only in `void` functions.
- [ ] Return-path analysis (non-`void` functions must return on every path)
  - **[DECIDED]** These rules are proposed for the semantic-analysis spec.
  - A statement *terminates* if it is `return`, or a call to a `#[noreturn]`
    function, or a block containing a terminating statement, or an
    `if`/`else if`/`else` chain with an `else` where every branch terminates.
    A `while` terminates only if its condition is literal `true` and its body
    contains no `break`.
  - Statements after a terminating statement produce an "unreachable code"
    warning.

## Bindings

- [ ] `let` bindings
  - `let [mut] name [: type] [= expr];`. Declared only inside functions.
  - Needs a type annotation or an initializer, otherwise it is an error.
  - Annotation and initializer must be compatible (widening allowed).
  - Default literal types apply (`i32`, `f32`, `str`, `bool`).
  - Initializers are optional only if an annotation is present.
- [ ] Mutability
  - Immutable by default. `mut` is a property of the **binding**, not the value.
  - Mutability controls reassignment, `&mut` and `&raw mut` creation, and
    `&mut self` method calls.
  - Mutability of fields and array elements follows the containing place.
  - Mutable references do not imply exclusive access (no borrow checker).
- [ ] Initialisation
  - **[DECIDED]** A binding without an initializer must be `mut`.
    `let mut x: i32; x = 42;` is OK. `let y: i32;` is an error. The draft allowed
    one deferred assignment to an immutable binding, but that needs path-sensitive
    analysis, so it is deferred to a later version.
  - **[DECIDED]** Reading an uninitialised binding is a **compile-time error**.
    `variables.md` said warning, but the draft and "fail loudly" say error.
    The v0.1 analyser uses textual-order tracking, so a read is an error if no
    assignment to the binding precedes it in source order within the function.
    This produces no false positives and is not path-sensitive.
    Path-sensitive analysis is deferred.
  - Reads: any use as a value, including compound assignment. Plain assignment
    is not a read.
  - References and pointers must be initialised at declaration (`let r: &i32;`
    is an error).
- [ ] `static` bindings
  - **[DECIDED]** Syntax is `static let` / `static let mut`, inside `impl`
    blocks only. This follows `methods.md` and `modules.md`. The draft's
    `static mut` is dropped.
  - Static storage duration, accessed via `Type::name`, `pub` for visibility.
  - **[DECIDED]** Initialisation: a static's initializer must be a literal
    (`-literal` counts, since the sign is unary minus). No function calls, no
    lazy first-access evaluation. This replaces `methods.md`'s "any
    expression, evaluated on first access". Wider initializers arrive with
    `const` in v0.2.
- [ ] `const` bindings
  - **[DECIDED]** Deferred to v0.2. `[T; N]` therefore takes only a literal `N`
    in v0.1. The draft said literals were supported "as of 1.0", which
    contradicts `types.md`.
  - Planned: immutable, compile-time expression, `const mut` is an error.

## Types

- [ ] `type` declarations: `type Name { field: T; … }`
  - Nominal, so identical shapes are distinct types.
  - Declaration order is field order. Layout and padding are unspecified.
  - No direct self-containment, but recursion through a pointer is allowed.
  - Empty types are allowed (`type Empty {}`). **[DECIDED]** The draft's
    `type Counter;` form is dropped.
  - Fields are private by default, and `pub` exposes them.
  - Struct literals follow the rules under Literals.
- [ ] Static arrays `[T; N]`
  - `N` is a non-zero integer literal. The length is part of the type.
  - Bounds-checked indexing.
- [ ] Type equality: same primitive, or same nominal declaration.
- [ ] Copy semantics
  - Every type is copyable in v0.1. Assign, pass and return copy.
  - Move-only types and `drop` are deferred.
- [ ] `impl Type { … }`
  - Inherent only, no traits. Each member declares its own visibility.
  - Contains methods, static functions and static bindings.
- [ ] Methods
  - Receiver forms: `self`, `mut self`, `&self`, `&mut self`. `*self` is not
    supported.
  - A method call supplies the receiver itself. This is the **only** place a
    reference is created without writing `&`.
  - **[DECIDED]** Receiver form and overloading:
    - `&self` and `&mut self` are distinct signatures and may be overloaded.
    - **[DECIDED]** Mixing by-value (`self` / `mut self`) and reference
      (`&self` / `&mut self`) receivers under one method name on a type is an
      error, whatever the other parameters are. So `get(self, …)` and
      `get(&self, …)` cannot coexist. Several by-value overloads may coexist if
      their remaining parameters differ.
    - `mut` on `self` is not part of the signature, as for ordinary parameters.
  - **[DECIDED]** Receiver overload choice for a place receiver:
    - A mutable place prefers the `&mut self` overload. If no `&mut self`
      candidate is viable for the arguments, it falls back to `&self`.
    - An immutable place can only use `&self`.
    - The first tier with a viable candidate wins. Within it, the dominance
      rules apply to the remaining arguments. Receiver form is not traded off
      against argument conversions.
    - Reference receivers (`&T`, `&mut T`) pass through unchanged, so
      `(&v).get()` forces `&self` on a mutable `v`.
    - Consequence: adding or removing `mut` on a binding can change which
      overload runs.
    - "Mutable place" means anything in the mutable-places list under
      Expressions (not only a `let mut` binding): fields and elements of mutable
      places, `*p` for `*mut T`, and so on.
    - Because mixing is forbidden, by-value receivers need no tier in the
      receiver rule. A by-value-only overload set just copies the receiver.
  - Temporary receivers live until the end of the enclosing statement.
- [ ] Static functions
  - **[DECIDED]** Static functions are declared with the `static` keyword:
    `static func name(...)` (`pub static func` for visibility). Called as
    `Type::name()`. `p.origin()` is an error.
  - A function in an `impl` with neither a receiver nor `static` is an error,
    and so is `static` combined with a receiver. This mirrors `static let`.
  - `methods.md` needs updating to match (see Doc Fixes). The draft and
    `modules.md` examples already use `pub static func`.

## Functions

- [ ] `func name(params) [-> type] { body }`
  - Params are `[mut] name: type`. They are immutable by default. `mut` makes a
    local copy and never affects the caller.
  - No return type means `void`. `-> void` is equivalent.
- [ ] Parameter passing
  - By value (a copy). References and pointers are copied as values and share
    the referent.
  - Callers always write `&`, `&mut` or `&raw`. A method receiver is the only
    exception.
  - Arity must match. No defaults and no variadics.
- [ ] Return-path checking (see Statements).
- [ ] Recursion and mutual recursion are allowed. There is no tail-call
  optimisation and no stack-overflow detection.
- [ ] Overloading
  - Signature is name + parameter types (+ owner for `impl` members + receiver
    form). Return type and param `mut` are not part of it.
  - `&T`, `&mut T`, `*T`, `*mut T` are distinct types for overloading.
  - Resolution is dominance-based (see below). No candidate means an error,
    and no unique best candidate means an ambiguity error.
  - **[DECIDED]** Overload resolution rules:
    1. **Viable candidates.** Same name/owner and arity, where every argument
       converts to its parameter using only implicit conversions (exact,
       widening, `&mut T -> &T`, `*mut T -> *T`, `nullptr -> pointer`).
    2. **Contextual literal inference.** An unsuffixed integer or float literal
       (including `-literal`) may adopt a candidate parameter's type of its
       kind if its value fits (range-checked). Adopting counts as an exact match
       for that argument. A candidate whose parameter the value does not fit is
       not viable.
    3. **Conversion ranks (per argument).** Exact is best. Widening is ranked by
       distance in width doublings (`u8 -> u16` = 1, `u8 -> u32` = 2,
       `f32 -> f64` = 1), and nearer is better. Other implicit conversions rank
       below exact.
    4. **Dominance, not scoring.** A beats B if A's conversion is no worse than
       B's for every argument and strictly better for at least one. The chosen
       candidate must dominate every other viable candidate. Rank values are
       never summed.
    5. **Ambiguity on ties.** If no candidate dominates all others, report an
       ambiguity error. Declaration order is never used.
    6. **Defaulting last.** Only when step 5 is ambiguous, and the tied
       candidates differ in the type given to unsuffixed literals, retry with
       the tied set restricted to candidates where every unsuffixed literal
       argument takes its default type (`i32` / `f32`). If exactly one remains,
       pick it. Otherwise report ambiguity. Literals never default earlier.
    - Examples:
      - `add(1, 2)` with only `add(u8, u8)`: OK, the literals become `u8`.
      - `add(1, 2)` with `add(i32, i32)` and `add(u8, u8)`: tie, defaulting
        picks `i32`.
      - `add(1, 2)` with `add(u8, u8)` and `add(u16, u16)`: ambiguity. Write
        `1u8`.
      - `add(300, 1)` with `add(u8, u8)` and `add(u16, u16)`: only `u16`
        fits, so it is chosen.
      - `f(x)` with `x: u8` and overloads `f(u16)`, `f(u32)`: picks `f(u16)`.
      - `f(a, b)` with `(u8, u8)` and overloads `f(u8, u32)`, `f(u32, u8)`:
        neither dominates, so it is ambiguous.
  - **[DECIDED]** `usize` rank. Its width is platform-dependent, so a
    widening conversion to `usize` is *incomparable* with widening conversions
    to other types (exact still beats it). A tie that depends on comparing them
    is ambiguous on every target.
  - Method receivers follow the receiver rule under Methods, then the rules
    above apply to the remaining arguments.
- [ ] `pub func` for cross-module visibility.
- [ ] `main`
  - Required, in module `main`, no arguments, returns `void` or `i32`
    (the process exit code).

## Memory

### References and Pointers

- [ ] Four distinct types: `&T`, `&mut T` (references) and `*T`, `*mut T`
  (raw pointers).
- [ ] Creation operators: `&x`, `&mut x`, `&raw x`, `&raw mut x`.
  - Operand must be a **place**. `&mut` / `&raw mut` need a mutable place.
  - Temporaries and literals are not places, so `&1`, `&(a + b)` and `&"hi"`
    are errors. **[DECIDED]** This closes the draft's "rules for temporaries"
    item. The only temporary allowed is a method receiver.
  - `&raw` is a separate operator, not sugar for `&`.
- [ ] References
  - Non-null, one word, auto-dereferenced (no `*` on references).
  - Must be initialised at declaration. Cannot be `nullptr`.
  - Reading a reference reads the referent. Assigning to it writes the
    referent, including with compound assignment.
  - **[DECIDED]** Reference semantics. These close the open questions in
    `references-and-pointers.md` §2.2:
    - **Copying.** An expression of type `&T` used where `&T` is expected copies
      the reference. This covers `let r2 = r;`, which infers `r2: &i32`,
      `g(r)` for `g(x: &i32)` and `return p.first`. The result aliases the same
      location. Used where `T` is expected (operands, annotated `let x: i32 = r`),
      it reads the referent.
    - **Rebinding.** A reference cannot be rebound after creation. `r = x`
      writes through the reference, and `r = &y` is a type error.
    - **Nested references.** `&&T` and `&r` where `r` is already a reference are
      errors. Reference-to-reference types are not valid in v0.1.
  - Field access auto-dereferences. References are not lifetime-checked.
- [ ] Raw pointers
  - May be null or dangling. No auto-dereference, so use `*p`.
  - `*p` is a place (mutable only for `*mut T`).
  - Dereferencing a null or invalid pointer is **undefined behaviour**.
  - `**T` is legal. Each layer needs its own `*`.
  - Operations: create, null, dereference, `== !=`, pass and return.
  - No pointer arithmetic, no indexing, no ordering.
  - There is no `unsafe` block in v0.1.
- [ ] Conversions
  - Implicit: `&mut T -> &T`, `*mut T -> *T`, `nulltype -> *T / *mut T`.
  - Everything else is rejected: reference to pointer and back, gaining
    mutability, `nulltype -> &T`.
  - Rule: you may give up capability, never gain it.
- [ ] Null semantics: `nullptr` is `nulltype`. `p == nullptr` is valid for any
  pointer. `nullptr == nullptr` is `true`.
- [ ] Struct fields and returns may be references or pointers. Referents must
  outlive the holder, but v0.1 does not check it.
- [ ] Memory safety
  - Not enforced in v0.1 (no lifetime checks). Dangling and null dereference
    are UB.
  - The one always-on check is array bounds checking.
  - Lifetime and dangling detection is planned for v0.2.
- [ ] Operators and references
  - Do not assume `x + &x` equals `x + x`.
  - Reference arguments and receiver passing are defined independently of
    operator overloading.

## Modules

- [ ] One `.xe` file is one module. It starts with `module name;`
  (`module a::b;` is allowed), and imports follow at the top. There are no
  inline modules.
- [ ] `runtime.xe` is the one file without a module declaration.
  - **[OPEN]** Making `module <name>;` optional in user files. See Open
    Decisions.
- [ ] `import path;` imports a whole module. The full path is required, with no
  aliases, globs, item imports or `use`.
  - **[DECIDED]** After `import utils::helpers;` the module is referred to by
    its **full path**, `utils::helpers::f()`. No last-segment shorthand.
  - **[DECIDED]** There is no `crate`, `self::` or `super::` path root.
  - **[DECIDED]** File name ↔ module name mapping and module discovery. This
    matches what the driver already does:
    - `project.source_files` in `xenon.toml` lists each source file, relative to
      the project root (the directory containing `xenon.toml`).
    - The project's modules are exactly the files in `project.source_files`
      (`xenon.toml`). Nothing is auto-discovered.
    - A module's name is the one in its `module` declaration. File name and
      directory are irrelevant. The analyser never assumes a layout.
    - Two files declaring the same module name is an error.
    - `import p;` must name a module that some listed file declares.
      A namespace prefix alone is not a module, so `import verification;` is an
      error if only `verification::age_verification` exists.
    - `a` and `a::b` may both exist. Nesting is only naming, with no
      parent/child relationship.
    - Exactly one module named `main` must exist and contain `main`. `runtime.xe`
      is supplied by the compiler and is not listed.
    - Import cycles (including self-import) are errors.
- [ ] Visibility
  - Everything is private by default. `pub` applies to functions, types, fields,
    `impl` members and statics.
  - Modules have no visibility of their own.
  - A declaration is accessible from another module if it is `pub` **and** the
    module is imported.
- [ ] Paths use `::` (`math::add`, `Point::origin`, `Counter::total`).
  - Unqualified names resolve in the current module. Unresolved names are errors.
- [ ] Constructors protecting invariants (the `Age::init` example).

## Runtime

- [ ] Panic
  - **[DECIDED]** `panic(message: str)` prints to stderr, terminates the program
    with exit code 101 and never returns (`#[noreturn]`). It is not UB, and it
    cannot be intercepted in v0.1.
  - Panic sources: array out-of-bounds, integer division/remainder by zero, bad
    shift count, overflow (debug only), explicit `panic(...)`.
- [ ] Free functions
  - `func panic(message: str)` (`#[noreturn]`)
  - `func assert(condition: bool, message: str)`: calls `panic(message)` when
    false.
  - `func exit(code: i32)` (`#[noreturn]`), forwards `code` to the OS.
  - `func println(s: str)`
  - **[DECIDED]** `println` takes only `str` in v0.1. There is no `print`.
    Spec examples that call `println(x)` with non-`str` arguments must be
    rewritten (see Doc Fixes).
- [ ] Scope of runtime items
  - **[DECIDED]** `runtime.xe` items are in scope in every module without an
    `import`. The specs left this open, and the `Age::init` example already
    uses `panic` with no import.
- [ ] `error`
  - **[DECIDED]** Not part of `runtime.xe`. Remove it from `runtime.md`.

### Annotations

- [ ] Syntax `#[name]` or `#[name("arg")]`, placed before a declaration.
- [ ] `#[extern("C")]`: external function with the named calling convention.
  The declaration has no body and ends with `;`.
  - A bodiless function without `extern` is an error.
- [ ] `#[link_name("symbol")]`: linker symbol name.
  - **[DECIDED]** Reconciles the draft's `extern(extern_name)` with
    `functions.md`. `extern`'s argument is the calling convention and
    `link_name` carries the symbol.
- [ ] `#[noreturn]`: never returns to its caller. It interacts with return-path
  analysis as described under Statements.
- [ ] Doc comments `///` attach to the following declaration.

### Xenon Exit Code Standard

- [ ] Define the standard exit codes.
  - `0`: successful termination.
  - `1`: general failure.
  - `101`: runtime panic.
  - Other nonzero exit codes may be used by applications.
  - `exit(code)` forwards the requested exit code to the operating system. A
    `main` returning `i32` does the same.
  - Document portability limits imposed by operating systems and shells (for
    example, Unix keeps only the low 8 bits).
  - Follow the Unix convention that zero is success and nonzero is failure.

---

# Decision Log (conflicts between scrap documents)

| # | Topic | Conflict | Resolution |
|---|-------|----------|------------|
| 1 | Uninitialised read | `variables.md`: warning. Draft: error in all builds | Error, using textual-order analysis |
| 2 | Deferred init of immutable binding | Draft: allowed once. `variables.md`: error | Error in v0.1, since it needs path-sensitive analysis |
| 3 | Static function syntax | Draft and `modules.md`: `static func`. `methods.md`: no keyword | `static func` is required |
| 4 | Static variable syntax | Draft: `static mut`. Specs: `static let mut` | `static let [mut]` |
| 5 | `++` on statics | Draft used `Counter::gCount++` | No `++`/`--`, use `+= 1` |
| 6 | Struct literal rules | Draft: only in the `impl`. UB or "not permitted" for missing fields | Module-level field accessibility, all fields required |
| 7 | `const` | Draft: supported in 1.0. `types.md`: v0.2 | v0.2 |
| 8 | Release overflow | Draft: wraps. `types.md`: may wrap | Wraps, but code must not rely on it |
| 9 | Annotation syntax | Draft: `extern(name)`. `functions.md`: `#[extern("C")]` | `#[...]` form, `link_name` added |
| 10 | `*r` on a reference | Draft: unspecified. Specs: error | Type error |
| 11 | Address of temporaries | Draft: unspecified. Specs: error | Error, except method receivers |
| 12 | `break` / `continue` | Open question in `statements.md` | Part of v0.1 (confirmed) |
| 13 | Hello world | Draft used `int main()` | `func main() -> i32` |
| 14 | `str` length field | Draft: `len`. `types.md`: `length` | `length` |
| 15 | `type Counter;` | Draft only | `type Counter {}` |
| 16 | Runtime scope | Open in `runtime.md` and `modules.md` | In scope everywhere |
| 17 | Panic behaviour | Open in `runtime.md`. Draft defines it | Draft's definition (stderr, exit 101) |

# Open Decisions (need your call)

1. Optional `module <name>;`. Proposal: an omitted declaration puts the file's
   `pub` declarations in a "root". Recommendation: keep the declaration
   required (explicit over implicit). If you want shorter single-file programs,
   make an omitted declaration mean the module is named `main`, not a root.
2. v0.2 casting design (parked): syntax (`T{ x }` is a candidate) and
   out-of-range behaviour (checked and panicking is recommended).

# Doc Fixes Needed in the Scrap Documents

- `methods.md` §3 and §4: static functions are declared `static func`. Remove
  the text saying the keyword is omitted. A function in an `impl` with neither
  `self` nor `static` is an error. `modules.md` already uses `pub static func`.
- `methods.md` example: `translate(self, …)` mutates `self.x`, which needs
  `mut self` (and then only changes a copy).
- `statements.md` §2: delete the `break`/`continue` open question.
- `variables.md` §6: warning becomes error, and the textual-order rule is added.
- `variables.md`, `statements.md`: `println(x)` / `print(x)` with non-`str`
  arguments must be rewritten. `println` takes only `str`, and `print` does not
  exist. Use comments (`// prints 2`) or string literals instead.
- `literals.md` §8.3 and `types.md` §4.4: remove scalar construction. Note
  `T{ x }` as a candidate cast syntax for v0.2.
- `functions.md` §7.1: replace the resolution steps and open question with the
  dominance rules above.
- `methods.md` §5.1: replace the weakest-borrow rule with the receiver rule
  above. `v.get()` on a mutable `v` now selects `&mut self`, and
  `(&mut v).get()` is no longer needed to get it. Also state the
  redeclaration rule for `self` vs `&self` / `&mut self` (§3 and §6).
- `runtime.md` §3: delete the `error` section.
- `modules.md` §2.3: replace the open question with the module discovery rules.
- `types.md` §1.1: add `u8`/`u16 -> usize` to the widening rules.
- `methods.md` §4: static initialisers must be literals.
- `literals.md` §8.2: UB for missing fields becomes a compile error, and the open
  question there is closed.
- `references-and-pointers.md` §2.2: replace the open question with the decided
  rules, and reserve `raw` in §3.1.
- `runtime.md`: write the panic section from the Runtime items above.
- `overview.md` links `lexical.md`, which still has to be written.

# Compiler Driver Follow-ups (from reviewing `compiler_driver.*` and `compiler.cpp`)

- `ModuleNamespaceTree::add_module` silently overwrites a module declared
  twice. It should report a duplicate-module error.
- Namespace-only intermediate nodes count as modules in `dfs_validate_module`,
  so `import verification;` resolves if only `verification::age_verification`
  exists. Require `dep_module->ast.has_value()`.
- `split_name` silently drops a lone `:`. Reject it instead.
- No check that exactly one `main` module exists (driver or sema).
- `compiler.cpp` is a second, divergent `ModuleNamespaceTree` and `Compiler`
  (namespace `xenon::compiler`):
  - It uses `.` separators and file stems for names.
  - Its DFS leaves `path` and `state` stale on early return, which is the bug
    the driver's comment describes.
  - Its `get_module` compares `part == parts.back()`, which breaks on `a::a`.
  - Its `init_project` writes `fn main()` instead of `func main()`.
  - Delete it, or port its optional-based CLI-override design (`TomlSettings` /
    `ResolvedConfig`) into the driver in place of the `_explicit` flags.
- `parse_string_array` exists in both files. The `compiler.cpp` version splits
  on commas inside quoted strings.