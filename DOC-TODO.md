# DOCS-TODO.md

Key points for every section of the Xenon documentation, condensed from the
scrap documents. Version naming follows the specs: **v0.1** is the current
language, **v0.2** is the next. (The old draft said "Xenon 1" / "Xenon 2".)

Items marked **[DECIDED]** resolve a conflict between scrap documents. They
are listed again in the Decision Log at the bottom. Items marked **[OPEN]**
still need your decision.

The "Compiler Toolchain (v1)" section near the end documents the v1 pipeline,
configuration files, environments, standard library and capabilities.

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
  begins at a function the selected environment requires (`main::main` for the
  `xenon` environment).
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
  - Type is always `str`, never `*u8`, `[N]u8` or `&[N]u8`.
  - Not addressable, so `&"hello"` is an error.
- [ ] Boolean and `nullptr` literals.
- [ ] Array literals
  - **[DECIDED]** List form only: `[1, 2, 3]`. There is no typed literal
    (`[3]f32{ … }`), because it would reintroduce `T{ x }`-style construction,
    which is deferred to v0.2 with casting. The element type comes from the
    annotation (`let a: [3]f32 = [1, 2, 3];`, using contextual literal
    inference) or from defaulting (`[1.0, 2.0]` is `[2]f32`).
  - No repeat form, no empty literal (`[0]T` is invalid).
  - With an annotation, the element count must equal `N`. Element
    metavariables unify across the array.
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
  - **[DECIDED]** Deferred to v0.2. `[N]T` therefore takes only a literal `N`
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
- [ ] Static arrays `[N]T`
  - **[DECIDED]** The length comes first, so the type reads left to right and
    matches indexing: `[2][3]f64` is 2 rows of 3, and `a[i][j]` has `i < 2` and
    `j < 3`. This replaces `[T; N]`.
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
- [ ] Required functions
  - **[DECIDED]** The language itself does not require `main`. The selected
    environment's `required_functions` list names the functions a program must
    define, with their signatures. An omitted return type means `void`. The
    hosted `xenon` environment requires `main::main(usize, *str) -> i32`
    (argument count, argument vector, and the process exit code). The
    illustrative `caesium` environment would require `main::setup()` and
    `main::loop()`.
  - **[DECIDED]** An entry is the widest signature the environment calls. A
    program may omit trailing parameters, so `main()`, `main(usize)` and
    `main(usize, *str)` all satisfy the `xenon` entry. Parameters may not be
    retyped, reordered or added, and the return type must match exactly.
  - See Compiler Toolchain > Required Functions.

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

- [ ] A source file that starts with `module name;` declares one named module
  (`module a::b;` is allowed), and imports follow at the top. A file declares at
  most one module, and there are no inline modules. A file with no `module`
  declaration is a global-scope file (see "Global scope" below).
- [ ] Global scope
  - **[DECIDED]** `module <name>;` is optional. A file with one declares a
    named module. A file without one contributes its declarations to the
    **global scope**.
  - A global-scope file listed in `source_files` behaves like `runtime.xe`: its
    `pub` declarations are available in every module without an import.
    `runtime.xe` is simply a global-scope file supplied by the environment, so it
    needs no special module declaration.
  - Unqualified names resolve in local scopes, then the current module, then
    the global scope. A leading `::` names the global scope explicitly, so
    `::say` always means the global `say`, even if the current module declares
    its own `say`.
  - Example. `libs/utils.xe` has no `module` line:
    ```xe
    pub func say(s: str) {
        println(s);
    }
    ```
    and a listed module uses it:
    ```xe
    module main;

    func main() -> i32 {
        say("Ping");     // normal lookup finds the global `say`
        ::say("Pong");   // explicit global scope
        return 0;
    }
    ```
  - **[OPEN]** Corner cases: privacy of non-`pub` items in a global-scope
    file, imports inside one, redeclaration rules and lookup precedence are
    recommended in Open Decisions (item 3).
- [ ] `import path;` imports a whole module. The full path is required, with no
  aliases, globs, item imports or `use`.
  - **[DECIDED]** After `import utils::helpers;` the module is referred to by
    its **full path**, `utils::helpers::f()`. No last-segment shorthand.
  - **[DECIDED]** There is no `crate`, `self::` or `super::` path root.
  - **[DECIDED]** File name ↔ module name mapping and module discovery. This
    matches what the driver already does:
    - `project.source_files` in `xenon.toml` lists each source file, relative to
      the project root (the directory containing `xenon.toml`).
    - The project's own modules are exactly the files in
      `project.source_files` (`xenon.toml`). Nothing in the project is
      auto-discovered.
    - **[DECIDED]** Standard-library modules are the one exception. They are
      discovered under `<installation>/stdlib/<name>/module.toml` and parsed
      only when a module imports them (see Compiler Toolchain > Standard
      Library). A user module with the same name as a standard-library module
      is **[OPEN]** (see Open Decisions).
    - A module's name is the one in its `module` declaration. File name and
      directory are irrelevant. The analyser never assumes a layout.
    - Two files declaring the same module name is an error.
    - `import p;` must name a module that some listed file declares.
      A namespace prefix alone is not a module, so `import verification;` is an
      error if only `verification::age_verification` exists.
    - `a` and `a::b` may both exist. Nesting is only naming, with no
      parent/child relationship.
    - Every function in the selected environment's `required_functions` list must
      exist and match its signature (`xenon` requires `main::main(usize, *str) -> i32`).
      `runtime.xe` is supplied by the environment and is not listed in
      `source_files`.
    - Import cycles (including self-import) are errors.
- [ ] Visibility
  - Everything is private by default. `pub` applies to functions, types, fields,
    `impl` members and statics.
  - Modules have no visibility of their own.
  - A declaration is accessible from another module if it is `pub` **and** the
    module is imported.
- [ ] Paths use `::` (`math::add`, `Point::origin`, `Counter::total`). A
  leading `::` (`::say`) refers to the global scope.
  - Unqualified names resolve as described under "Global scope". Unresolved
    names are errors.
- [ ] Constructors protecting invariants (the `Age::init` example).

## Runtime

- [ ] Environment dependence
  - Each environment target supplies its own `runtime.xe` and C runtime. The items
    below describe the hosted `xenon` environment (stderr, process exit codes).
    Other environments such as `caesium` have no stderr and no process to exit, and
    may implement `panic`, `exit` and `println` differently.
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
- [ ] `#[require_capability("name")]`: reserved syntax. It parses as an
  ordinary annotation on a function or module and has no effect in v1. Intended
  future meaning: the declaration may only be used when the selected environment and
  target provide capability `name`. See Compiler Toolchain > Capabilities.
- [ ] Doc comments `///` attach to the following declaration.

### Xenon Exit Code Standard

- [ ] Define the standard exit codes. These apply to hosted environments (`xenon`).
  Bare-metal environments define their own behaviour.
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

# Compiler Toolchain (v1)

How the compiler turns a project into an executable. The phases are logical,
not six separate subsystems. The dataclasses below are conceptual Python; the
compiler itself is C++. Anything not described here is out of scope for v1
(see "Out of Scope for v1" at the end of this section).

Only the hosted `xenon` environment ships and is implemented in v1. `caesium`
appears below as an illustrative future environment, to show why environments are
extensible. Nothing for it needs to be built now.

## Pipeline

- [ ] **Phase 1: Preprocessing**
  1. Parse CLI arguments.
  2. Locate and parse `xenon.toml`.
  3. Load the selected environment's `environment.toml`. The environment name is resolved
     first (CLI, then project config, then the default `xenon`), because it decides
     which file to load.
  4. Discover standard-library modules by reading their `module.toml` files.
  5. Resolve CLI, project config, environment, target and capabilities into a single
     `ResolvedConfigs`.
  - Output: `ResolvedConfigs` and discovered standard-library metadata.
- [ ] **Phase 2: Parsing**
  1. Lex and parse every project source file into an AST.
  2. Parse the selected target's `runtime.xe`. It is a global-scope file.
  3. Parse a standard-library module only when an already-parsed module imports
     it. Import *discovery* is therefore a worklist inside Phase 2, and Phase 3
     does the resolution and validation.
  4. Collect all resulting ASTs.
  - Output: parsed module ASTs.
- [ ] **Phase 3: Pre-analysis**
  1. No two source files may declare the same module name.
  2. Files without a module declaration contribute their declarations to the
     global scope (see Modules > Global scope). They are not nodes in the
     dependency graph.
  3. Resolve module imports.
  4. Build the module dependency graph.
  5. Kahn's algorithm produces a topological compilation order.
  6. Report circular dependencies if the graph cannot be fully ordered.
  - Output: dependency graph and compilation order.
- [ ] **Phase 4: Semantic analysis**
  1. Process modules in dependency order.
  2. Resolve names and symbols.
  3. Resolve types.
  4. Resolve function overloads.
  5. Validate the environment's `required_functions`: each must exist and match
     its signature (see Required Functions).
  6. Check calls, expressions and the remaining semantic rules.
  - Capabilities are not checked in v1 (see Capabilities).
  - Output: typed AST.
- [ ] **Phase 5: Code generation**
  1. Generate LLVM IR.
  2. Verify the LLVM module.
  3. Generate target object files.
  4. Compile the environment target's `c_source` for the selected target. The
     `c_source` of every imported standard-library module is compiled the same
     way.
  - Output: object files.
  - Carried over from earlier design discussion (drop it if unwanted): verify
    that `Target.pointer_width` equals the pointer size in LLVM's `DataLayout`,
    and report a mismatch as an error.
- [ ] **Phase 6: Linking**
  1. Link the generated object files.
  2. Link the environment's runtime and C support, plus the system libraries named
     by imported standard-library modules (`link_libs`, for example `m`).
  3. Apply the target's `linker_script` if provided.
  4. Produce the final ELF executable.
  - Output: executable.

## Configuration

### CLI

- [ ] `CLIArgs` holds `command`, `project_dir` (default `.`) and the optional
  overrides `target`, `environment`, `optimisation` and `output`.
- [ ] Relative paths given on the CLI are relative to the current working
  directory. Paths in `xenon.toml` are relative to the project directory.
- [ ] Capabilities cannot be set from the CLI.
- [ ] The CLI front end is `xec`. v1 needs at least `xec build <project>`.
  `xenon.toml` is read from `<project_dir>/xenon.toml`. There is no search of
  parent directories. **[OPEN]** The full command set (is there `init`?) is
  unspecified.

### Project config (`xenon.toml`)

- [ ] Contains project information, build overrides and capability overrides.
  ```toml
  [project]
  name = "My Project"
  source_files = ["src/main.xe", "libs/utils.xe"]

  [build]
  environment = "xenon"
  target = "x86_64-linux"
  optimisation = "release"

  [capabilities]
  fpu = true
  ```
- [ ] The project config preserves omitted values. Defaults are applied only during
  resolution.
- [ ] Unknown keys are errors, so a misspelled `optimisation` cannot silently
  fall back to a default. `project.version` and `project.authors` are accepted
  and ignored.
- [ ] `optimisation` is `"debug"` or `"release"`. It decides whether overflow
  checks are on (`debug` panics on overflow, `release` wraps).
- [ ] Defaults
  - **[DECIDED]** `environment`: `xenon`.
  - **[DECIDED]** `target`: explicitly required. Missing is an error.
  - **[DECIDED]** `optimisation`: `debug`.
  - **[DECIDED]** `output`: `build/`, a directory for all build products,
    relative to the project directory.

### Environment configuration (`environment.toml`)

- [ ] Each installed environment has its own directory with an `environment.toml`.
  ```text
  <installation>/
  ├── xenon/                          (ships in v1)
  │   ├── environment.toml
  │   ├── runtime.xe
  │   ├── libxenon.c
  │   └── libxenon.h
  └── caesium/                        (illustrative, not shipped)
      ├── environment.toml
      └── rp2040/
          ├── runtime.xe
          ├── libcaesium_rp2040.c
          ├── libcaesium_rp2040.h
          └── link.ld
  ```
- [ ] An environment describes its targets, default capabilities, target-specific
  capabilities, runtime and C source paths, and required functions.
- [ ] Paths in `environment.toml` are relative to the environment directory, so an
  installation can be moved without breaking it.
- [ ] The `.h` files are only `#include`d by the C sources. The compiler never
  reads them.
- [ ] The environment name is the directory name. A `name` key that disagrees with
  it is an error.
- [ ] `xenon` example (the environment that ships):
  ```toml
  name = "xenon"

  required_functions = [
      "main::main(usize, *str) -> i32",
  ]

  [capabilities]
  fpu = true
  heap_alloc = true

  [targets.x86_64-linux]
  llvm_triple   = "x86_64-unknown-linux-gnu"
  pointer_width = 64                # bits: 16, 32 or 64
  runtime       = "runtime.xe"
  c_source      = "libxenon.c"
  ```
- [ ] `caesium` example (illustrative only):
  ```toml
  name = "caesium"

  required_functions = [
      "main::setup()",
      "main::loop()",
  ]

  [capabilities]                      # environment defaults
  fpu = false
  heap_alloc = false

  [targets.rp2040]
  llvm_triple   = "thumbv6m-unknown-none-eabi"
  cpu           = "cortex-m0plus"     # optional
  pointer_width = 32
  runtime       = "rp2040/runtime.xe"
  c_source      = "rp2040/libcaesium_rp2040.c"
  linker_script = "rp2040/link.ld"    # optional
  configurable  = ["heap_alloc"]

  [targets.rp2040.capabilities]       # target overrides
  fpu = false
  ```
- [ ] There is no `default_target`: the target is always chosen explicitly.
- [ ] v1 implements hosted targets only. Handling freestanding targets (no
  system C library or startup files) arrives with the first non-hosted
  environment, so `Target` has no hosted flag yet.

### Standard library (`stdlib/<name>/module.toml`)

- [ ] v1 has one standard-library module: `math`. The layout supports
  discovering more later.
  ```text
  <installation>/
  └── stdlib/
      └── math/
          ├── module.toml
          ├── math.xe
          └── math.c
  ```
  ```toml
  name = "math"
  source = "math.xe"
  c_source = "math.c"
  link_libs = ["m"]       # optional: system libraries passed to the linker
  ```
- [ ] Preprocessing reads each subdirectory's `module.toml` to learn the module
  names. It does not parse the `.xe` files and does not check that they exist.
- [ ] A module's `.xe` is parsed only if the project imports it (Phase 2).
- [ ] Paths in `module.toml` are relative to that module's directory.
- [ ] This `module.toml` is discovery metadata only. There are no dependency
  fields and no dependency resolution in v1.
- [ ] The `name` must equal the directory name. Two modules with the same name
  are an error.

### Resolution rules

- [ ] The resolver produces one `ResolvedConfigs`.
- [ ] `environment`: CLI, then project config, then `xenon`.
- [ ] `target`: CLI, then project config. Neither is an error. The target must exist
  in the selected environment.
- [ ] `optimisation`: CLI, then project config, then `debug`.
- [ ] `output`: CLI, then project config, then `build/`.
- [ ] Capabilities: environment defaults, then target overrides, then project
  overrides, merged into one map. v1 does no further checking of them.
- [ ] Source files are made absolute against the project directory. A missing
  file, a file listed twice and an empty `source_files` are errors.

### Data structures

- [ ] Conceptual Python representation. Translate to C++ structures.
  ```python
  @dataclass
  class CLIArgs:
      command: str
      project_dir: Path = Path(".")
      target: Optional[str] = None
      environment: Optional[str] = None
      optimisation: Optional[str] = None
      output: Optional[Path] = None

  @dataclass
  class BuildConfig:
      environment: Optional[str] = None
      target: Optional[str] = None
      optimisation: Optional[str] = None
      output: Optional[Path] = None

  @dataclass
  class ProjectConfig:
      name: str
      source_files: list[Path] = field(default_factory=list)
      build: BuildConfig = field(default_factory=BuildConfig)
      # Added: the example xenon.toml has a [capabilities] table.
      capabilities: dict[str, bool] = field(default_factory=dict)

  @dataclass
  class Target:
      name: str
      llvm_triple: str
      pointer_width: int            # bits
      runtime: Path
      c_source: Path
      cpu: Optional[str] = None
      linker_script: Optional[Path] = None
      capabilities: dict[str, bool] = field(default_factory=dict)
      configurable: set[str] = field(default_factory=set)

  @dataclass
  class Environment:
      name: str
      targets: dict[str, Target]
      capabilities: dict[str, bool] = field(default_factory=dict)
      required_functions: list[str] = field(default_factory=list)

  @dataclass
  class ModuleManifest:
      name: str
      directory: Path
      source: Path
      c_source: Optional[Path] = None
      # Added: math needs libm on hosted targets.
      link_libs: list[str] = field(default_factory=list)

  @dataclass
  class ResolvedConfigs:
      project_name: str
      project_directory: Path
      source_files: list[Path]
      environment: Environment
      target: Target
      optimisation: str
      output: Path
      capabilities: dict[str, bool]
      stdlib_directory: Path
      stdlibs: dict[str, ModuleManifest]
  ```
- [ ] C++ notes: `optimisation` is an enum (`debug`, `release`) in the resolved
  config. Paths in `Environment`, `Target` and `ModuleManifest` are resolved against
  their own directories when loaded. Fatal configuration errors are reported as
  exceptions.

## Environments

- [ ] An environment is a preset for the compiler: the platform family. It defines supported
  targets, default capabilities, runtime files and required functions.
- [ ] Only `xenon` is implemented in v1.
- [ ] An environment is a directory containing `environment.toml`.
- [ ] The environment's `runtime.xe` is a global-scope file. It has no module
  declaration, nothing imports it, and its names are in scope everywhere. It is
  loaded unconditionally by the compiler before user code.
- [ ] Each runtime function is a thin binding to a symbol in the environment's C
  source (`#[extern("C")]`, `#[link_name]`).
- [ ] An environment does not provide a separate library registry, prebuilt
  objects or runtime components in v1.

## Standard Library

- [ ] See the `module.toml` layout above. Only `math` ships in v1, and it must
  be usable from the first release on the hosted `xenon` environment. That is why
  `-lm` linking is a v1 feature.
- [ ] `math` example:
  ```xe
  module math;

  #[require_capability("fpu"), extern("C")]
  pub func sin(n: f64) -> f64;
  ```
  The `require_capability` attribute is accepted but has no effect in v1.

## Capabilities

- [ ] Capabilities are configuration metadata in v1. They are read from
  `environment.toml` and `xenon.toml`, merged, and stored in `ResolvedConfigs`.
  No phase consumes them, and nothing is checked or enforced.
- [ ] Known capability names are `fpu` and `heap_alloc`.
- [ ] `fpu = true` means the selected compilation environment provides the
  capability. It does not mean the hardware has a physical FPU. A software
  implementation may provide it.
- [ ] `#[require_capability("name")]` is reserved syntax. It parses as an
  ordinary annotation on a function or module and is otherwise ignored in v1.
- [ ] Intended future behaviour (not in v1, and no machinery should be built
  for it yet):
  - Environment: defines default capabilities.
  - Target: overrides them and lists the `configurable` capabilities.
  - Project configuration: may override only capabilities the target lists as
    `configurable`.
  - Unknown capability names are errors.
  - The semantic analyser checks `#[require_capability]` after overload
    resolution, so capabilities never change which overload is chosen. Using
    a function whose capability is not provided is an error at the use, not
    at the declaration.

## Required Functions

- [ ] The environment's `required_functions` list names the functions a program
  must define. The language itself has no required `main`.
  ```toml
  required_functions = [
      "main::main(usize, *str) -> i32",     # xenon
  ]
  ```
  ```toml
  required_functions = [
      "main::setup()",           # caesium (illustrative)
      "main::loop()",
  ]
  ```
- [ ] Signature format: `module::path::name(type, ...) [-> type]`. The return
  type is optional. If omitted it means `void`.
- [ ] The signature uses the overload-signature rule (name plus parameter
  types), so it identifies one overload.
- [ ] The compiler validates that each required function exists and matches
  its signature, including the return type (Phase 4). A missing or mismatched
  function is an error.
- [ ] The environment loader checks that each entry is well formed. Matching types
  is done by semantic analysis, which parses the type strings with the normal
  Xenon type parser.
- [ ] **[DECIDED]** Prefix rule: an entry is the widest signature the
  environment calls. The program's function may omit trailing parameters and
  nothing else may differ. There are no alternative signatures per entry, so
  `required_functions` stays a list of strings.
- [ ] **[DECIDED]** Calling convention: required functions are emitted with C
  linkage under their path joined with `__` (`main__main`, `main__setup`). The
  C runtime always calls the widest form, `main__main(argc, argv)`. The compiler
  emits the symbol with the full signature and ignores parameters the user
  left out.
- [ ] **[DECIDED]** `argv` on the hosted environment: the C entry function
  builds an array of `str` views (`{bytes, length}`) on its own stack, one per
  argument, pointing at the strings the OS already placed in the process. There
  is no heap allocation and no copying, and each length is a `strlen`. The array
  lives until `main` returns. Arguments are not validated as UTF-8 in v1, so a
  `str` from `argv` may hold invalid UTF-8.

## C Compilation and Linking

- [ ] The environment target's `c_source` (and imported stdlib `c_source` files) is
  compiled at build time for the selected target. This requires a C compiler
  such as Clang at build time.
- [ ] The target's `llvm_triple` (and `cpu` if set) select the target.
- [ ] Linking produces an ELF executable only. UF2, hex and raw binary output
  are not part of v1. Flashing and conversion are external.
- [ ] Imported stdlib modules can name system libraries in `link_libs`. `math`
  names `m`, so `-lm` is passed to the linker when `math` is imported.
- [ ] If the target has a `linker_script`, it is applied at link time.
- [ ] **[OPEN]** C defines, the `extern("C")` ABI, and how the C compiler,
  linker and `<installation>` directory are found. See Open Decisions.

## Out of Scope for v1

- Implementing any environment other than `xenon`.
- Capability checking, negotiation and `#[require_capability]` enforcement.
- Library manifests beyond the minimal `module.toml` (`ModuleManifest`), and dependency
  resolution.
- A `LinkPlan` abstraction.
- A library registry class.
- Prebuilt object-file matrices.
- A separate target-triple parser.
- A separate runtime-component system.
- Freestanding (non-hosted) builds.
- UF2, hex and raw binary output.

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
| 18 | Entry point | Language spec: `main` required in module `main` | Environment-defined `required_functions` list (`xenon`: `main::main(usize, *str) -> i32`, trailing parameters may be omitted) |
| 19 | Module set | `source_files` only | `source_files` plus stdlib modules discovered under `<installation>/stdlib/`, parsed on import |
| 20 | `runtime.xe` | One shared file | One per environment target (environment root for `xenon`) |
| 21 | Output format | UF2, hex and binary considered | ELF only in v1 |
| 22 | Debug/release key | `mode` considered | `optimisation = "debug" \| "release"` |
| 23 | Prebuilt runtime objects | Per-target `.o` matrix considered | Environment C source compiled at build time |
| 24 | Module declaration | Module rules required `module <name>;` in every file | Optional. A file without one contributes to the global scope, reachable as `name` or `::name` |
| 25 | Build defaults | Unspecified | Environment `xenon`, target required, optimisation `debug`, output `build/` |
| 26 | Capabilities | Enforced during semantic analysis | Metadata only in v1. `#[require_capability]` is reserved and unchecked |
| 27 | Required functions | `[entry]` table mapping C symbols | `required_functions` list. Return type optional (`void` if omitted) |
| 28 | `-lm` | v0.2 | v1, through `link_libs` in the stdlib `module.toml` |
| 29 | Environment scope | `xenon` and `caesium` | Only `xenon` in v1. `caesium` is illustrative |
| 30 | Naming | `profile`, `profile.toml`, `xenon.toml` (a "manifest"), stdlib `config.toml` | `environment` (concept, CLI flag, `[build]` key, struct, directories), `environment.toml` (manifest of the environment), `xenon.toml` (`ProjectConfig`, the project's build configuration), stdlib `module.toml` (`ModuleManifest`) |
| 31 | Array syntax | `[T; N]` | `[N]T`, so `[2][3]f64` reads like its indexing |
| 32 | Typed array literals | `[f32; 3] { … }` | Dropped. Annotation or defaulting gives the element type |
| 33 | Hosted `main` | No arguments (language spec) vs `argc`/`argv` (example) | `main::main(usize, *str) -> i32` entry with a prefix rule, so `main()` is still valid |
| 34 | `argv` representation | Unspecified | Stack array of `str` views built by the C entry function, no heap, not UTF-8 validated |
| 35 | Required-function call | Unspecified | C runtime calls `main__main(argc, argv)`, the compiler emits the symbol with the full signature |
| 36 | Import keyword | Sample program used `use math;` | `import` (no `use`), as already specified |

# Open Decisions (need your call)

Resolved since the last round: optional `module` declarations (global scope),
build defaults, capabilities as v1 metadata, the `required_functions` list,
`-lm` in v1, `xenon`-only scope, the hosted `main` signature (argc/argv with a
prefix rule), the C symbol naming for required functions, `[N]T` arrays, and
the file and concept renames. What remains:

1. **Global-scope files.** The core rule is decided. These corners are not:
   - Privacy: a non-`pub` declaration in a global-scope file. Recommendation:
     visible only within that file, matching "private by default".
   - Imports: may a global-scope file contain `import`? Recommendation: no in
     v1. It would make the global scope a node in the dependency graph, and
     modules that use global names would form cycles with it.
   - Collisions: the same name and signature declared twice in the global scope
     (across files, or against `runtime.xe`) is a redeclaration error, as in a
     module. Different signatures overload as usual.
   - Lookup order: local scopes, then the current module, then the global
     scope. A module's own declaration therefore hides a global one, and
     `::name` reaches the global one.
   Recommendation: accept all four as written.
2. **Stdlib naming.** The loader enforces that `name` equals the directory name
   and that names are unique. Still open: whether the module's own `module`
   declaration must match (recommended: yes, checked in Phase 2), and whether
   a user module may share a name with a stdlib module (recommended: error).
3. **`link_libs`.** `ModuleManifest` gained an optional `link_libs` list so
   `math` can request `-lm`. The alternative is always passing `-lm` on the
   hosted environment. Recommendation: keep `link_libs`. It is one optional key.
4. **C build details.**
   - Pass `-DXENON_POINTER_BITS=<n>` to the C compiler so the C side can assert
     its own sizes. Recommended. Capability defines are not needed while
     capabilities are metadata.
   - Define the `extern("C")` ABI: `str` crosses as a `(ptr, len)` pair and is
     never returned as a struct, and `bool` is `i8` with zero extension.
     Recommended.
   - Locating tools and the installation. Recommendation: `clang` on `PATH`
     with an `XENON_CC` override and a clear error naming a missing tool, and
     `<installation>` found relative to the compiler executable with a
     `XENON_HOME` override.
5. **Output layout.** `output` is a directory. Recommendation: the final
   executable is `<output>/<project name>` and object files go in
   `<output>/obj/`. Project names containing spaces (as in the example
   `"My Project"`) need a rule. Recommendation: restrict `name` to
   identifier-like characters, or sanitise it for the file name.
6. **Parked.** v0.2 casting design: syntax (`T{ x }` is a candidate) and
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
- `types.md`, `literals.md`: arrays are `[N]T`, not `[T; N]`, and the typed
  array literal form is removed. List literals take their element type from the
  annotation or from defaulting.
- `functions.md` (`main`) and `runtime.md`: the hosted entry is
  `main::main(usize, *str) -> i32` with the prefix rule. Describe the `argv`
  `str` array, which is not validated as UTF-8.
- `overview.md` links `lexical.md`, which still has to be written. Its "execution
  begins at `main`" line becomes "execution begins at a function the environment requires".
- `functions.md` (`main`): required functions are environment-defined. Remove the
  language-level requirement for `main`.
- `runtime.md`: describe runtime behaviour per environment. Panic output, `exit` and
  exit codes apply to the hosted `xenon` environment. State that `runtime.xe` is a
  global-scope file.
- `modules.md`: add standard-library modules (discovered from `stdlib/`, parsed
  on import) to the module-set rules. Make `module <name>;` optional, and
  document the global scope, the leading `::` path and the lookup order.
- `functions.md` / annotations: add `#[require_capability("name")]` as reserved
  syntax with no effect in v1.
- New docs to write: compiler pipeline, configuration (`xenon.toml`,
  `environment.toml`, stdlib `module.toml`), environments, standard library, capabilities.

# Future Syntax

```
import math;
import physics;

const $GRAVITY: f64 = 9.80665;
const $PARTICLE_COUNT: usize = 3;
const $TIME_STEP: f64 = 0.016;

// A generic two-dimensional vector.
type Vector2<T: Numeric>
    T :: x;
    T :: y;
end type Vector2

impl<T: Numeric> Vector2<T>

    func zero() -> Vector2<T>
        return Vector2<T> {
            x: T::zero(),
            y: T::zero()
        };
    end func zero

    func magnitude(&self) -> T
        return math::sqrt(self.x ** 2 + self.y ** 2);
    end func magnitude

    func scale(&mut self, factor: T)
        self.x = self.x * factor;
        self.y = self.y * factor;
    end func scale

    func add(&mut self, other: &Vector2<T>)
        self.x = self.x + other.x;
        self.y = self.y + other.y;
    end func add

end impl Vector2

// Particle representation.
type Particle
    f64 :: mass;
    Vector2<f64> :: position;
    Vector2<f64> :: velocity;
end type Particle

impl Particle

    func kinetic_energy(&self) -> f64
        let speed: f64 = self.velocity%magnitude();
        return 0.5 * self.mass * speed ** 2;
    end func kinetic_energy

    func accelerate(&mut self, acceleration: &Vector2<f64>, dt: f64)
        self.velocity.x = self.velocity.x + acceleration.x * dt;
        self.velocity.y = self.velocity.y + acceleration.y * dt;
    end func accelerate

    func update(&mut self, dt: f64)
        self.position.x = self.position.x + self.velocity.x * dt;
        self.position.y = self.position.y + self.velocity.y * dt;
    end func update

end impl Particle

// Calculate the dot product of two vectors.
func dot<T: Numeric>(a: &Vector2<T>, b: &Vector2<T>) -> T
    return a.x * b.x + a.y * b.y;
end func dot

// Compile-time architecture specialization.
$if TARGET_ARCH == "AVX2"

    const $VECTOR_BACKEND: str = "AVX2";

$else

    const $VECTOR_BACKEND: str = "SCALAR";

$end

// Program entry point.
func main() -> i32

    let mut particles: [$PARTICLE_COUNT]Particle = [
        Particle {
            mass: 2.0,
            position: Vector2<f64> { x: 0.0, y: 10.0 },
            velocity: Vector2<f64> { x: 3.0, y: 0.0 }
        },

        Particle {
            mass: 1.5,
            position: Vector2<f64> { x: 5.0, y: 20.0 },
            velocity: Vector2<f64> { x: -1.0, y: 2.0 }
        },

        Particle {
            mass: 3.0,
            position: Vector2<f64> { x: 10.0, y: 15.0 },
            velocity: Vector2<f64> { x: 0.0, y: -1.0 }
        }
    ];

    let gravity: Vector2<f64> = Vector2 {
        x: 0.0,
        y: -$GRAVITY
    };

    println("Xenon Particle Simulation");
    println("Backend: {}", $VECTOR_BACKEND);
    println("Particles: {}", $PARTICLE_COUNT);

    // Simulation loop.
    do step in 0..100

        do i in 0..$PARTICLE_COUNT

            particles[i].accelerate(&gravity, $TIME_STEP);
            particles[i].update($TIME_STEP);

        end do i

    end do step

    // Display simulation results.
    println("");
    println("Simulation results:");

    do i in 0..$PARTICLE_COUNT

        let particle: &Particle = &particles[i]

        println("Particle {}", i);
        println("  Position: ({}, {})",
            particle.position.x,
            particle.position.y
        );

        println("  Velocity: ({}, {})",
            particle.velocity.x,
            particle.velocity.y
        );

        println("  Kinetic energy: {} J",
            particle.kinetic_energy()
        );

    end do i

    return 0;

end func main
```