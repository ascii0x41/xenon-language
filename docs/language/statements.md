# Statements

## 1. Overview

A statement is a unit of execution that does not produce a value. A
block is a sequence of statements.

Most of the language is expressions; statements are a thin layer on
top. This document specifies:

- Statement kinds.
- Blocks and scoping.
- Control flow: `if`, `while`, `return`, `break`, `continue`.
- The expression/statement distinction.

## 2. Statement kinds

Xenon has the following statements:

- **Let binding:** `let x = expr;`
- **Expression statement:** `expr;`
- **Assignment:** `lvalue = expr;`
- **Block:** `{ stmt* }`
- **If:** `if cond { ... } else if cond { ... } else { ... }`
- **While:** `while cond { ... }`
- **Return:** `return expr?;`
- **Break:** `break;`
- **Continue:** `continue;`

> **Open question:** `break` and `continue` are not exercised in
> `syntax.txt`. Confirm they are part of the v0.1 grammar.

There is no `for` statement in v0.1. A `for ... in` loop depends on an
iteration protocol that will be added together with iterators in v0.2.

## 3. Blocks

A block is a sequence of statements wrapped in braces:

```xe
{
    let x = 1;
    let y = 2;
    print(x + y);
}
```

Blocks introduce a new scope. Bindings declared in a block are visible
until the end of the block:

```xe
{
    let x = 1;
    // x is visible here
}
// x is not visible here
```

Blocks can appear as:

- The body of a function.
- The body of a control-flow statement.
- A standalone statement.

A block is a **statement**, not an expression. It does not produce a
value. The last expression in a block is not the block's value.

## 4. Expression statements

An expression followed by `;` is a statement:

```xe
x + 1;                     // legal but useless; result is discarded
f();                       // calls f, discards the result (if any)
x = 5;                     // assignment statement
```

The semicolon terminates the statement. The expression's value, if it
has one, is discarded, except for its side effects.

An expression of type `void` (a call to a function returning `void`, or
an assignment) is a valid expression statement. It is only an error in
*value* position (see `expressions.md` §2.3).

## 5. `if` statements

The `if` statement has these forms:

```xe
if cond {
    ...
}

if cond {
    ...
} else {
    ...
}

if cond {
    ...
} else if cond2 {
    ...
} else {
    ...
}
```

The condition must be `bool`. If `cond` is `true`, the then-branch
runs; otherwise, the next `else if` condition is tested, and finally
the `else` branch (if any) runs.

Braces are required. There is no dangling-else ambiguity because there
is no single-statement `if`:

```xe
if x > 0 {
    print("positive");
}
```

`if` is a **statement**, not an expression. It does not produce a value
and cannot appear on the right of `let`:

```xe
let x = if cond { 1 } else { 2 };   // error: if is not an expression
```

To select a value, use the ternary operator (`expressions.md` §2.5):

```xe
let x = cond ? 1 : 2;
```

## 6. `while` statements

The `while` statement repeats a block as long as a condition is true:

```xe
while cond {
    ...
}
```

The condition must be `bool`. It is evaluated before each iteration.
If it is `false`, the loop exits.

```xe
let mut i = 0;
while i < 10 {
    print(i);
    i += 1;
}
```

Braces are required.

`while` does not have an `else` clause.

## 7. `return` statements

`return` exits the enclosing function:

```xe
func f() -> i32 {
    return 1;
}

func g() {
    return;
}
```

`return <expr>;` is required in a function with a non-`void` return
type:

```xe
func f() -> i32 {
    return 1;              // ok
}

func f() -> i32 {
    return;                // error: expected i32, got void
}
```

`return;` (with no expression) is allowed in a `void` function, or as
the last statement of a function with an implicit `void` return.

### 7.1 Return-path analysis

A function with a non-`void` return type must return a value on every
path. Execution must not be able to reach the end of the body without
returning:

```xe
func foo(x: i32) -> i32 {
    if x > 0 {
        return x;
    }
}                          // error: not every path returns a value
```

The exact rules for deciding that a statement (an `if`/`else if`/`else`
chain, a block, a `while` loop, a call to a function marked
`#[noreturn]`) is guaranteed to return belong to the control-flow
analysis in the semantic-analysis specification.

## 8. `break` and `continue`

`break` exits the innermost enclosing loop:

```xe
while true {
    if x > 10 {
        break;
    }
    x += 1;
}
```

`continue` skips to the next iteration of the innermost enclosing
loop:

```xe
while i < 10 {
    i += 1;
    if i == 5 {
        continue;
    }
    print(i);
}
```

`break` and `continue` outside a loop are compile errors.

`break` and `continue` do not take labels in v0.1. Nested loops can
only break or continue the innermost loop.

## 9. Control flow and blocks

Every control-flow statement uses braces. There is no single-statement
form:

```xe
if x > 0 {
    print("positive");
}

// NOT:
// if x > 0 print("positive");
```

This eliminates a class of bugs (the dangling-else problem, missing
braces) and makes the grammar simpler. Braces are required everywhere a
block appears. There are no significant-whitespace rules.

## 10. Expression vs statement

Xenon's grammar distinguishes expressions from statements. A statement
does not produce a value; an expression does.

Statements that contain expressions:

- `let x = expr;`
- `expr;`
- `lvalue = expr;`
- `return expr;`
- `if expr { ... }`
- `while expr { ... }`

In each case, the expression is evaluated, and its value is used by
the statement.

`if` and blocks are never expressions. The expression form of a
conditional is the ternary operator.

## 11. Summary

- Statements are: `let`, expression statements, assignments, blocks,
  `if` (with `else if` / `else`), `while`, `return`, `break`,
  `continue`.
- Braces are required around all control-flow bodies.
- No single-statement `if` or `while`.
- `if` and blocks are statements, not expressions.
- `while` has no `else`.
- There is no `for` in v0.1.
- `break` and `continue` are unlabeled.
- A non-`void` function must return on every path.