# Runtime

## 1. Overview

The runtime is the support code that the compiler assumes every Xenon
program has. It lives in the special module `runtime.xe`, the one
module that does not need a `module` declaration (see `modules.md`
§2.2).

This document will specify:

- What `panic` is and what it does.
- What `error` is.
- Anything else the language specification refers to as "the runtime".

## 2. Panic

The other documents say that certain operations "panic":

- Array index out of bounds (`expressions.md` §2.4).
- Integer division by zero (`expressions.md` §4.1).
- Shift count not less than the bit width (`expressions.md` §4.4).
- Arithmetic overflow, in debug builds (`types.md` §2.1).
- Explicit calls to `panic(...)`.

A panic is a runtime failure that is **not undefined behavior**.

> **Open question:** What a panic *does* is not yet defined: whether it
> terminates the process, what exit code it produces, what it prints, and
> whether it can be intercepted. This section needs to be written before
> "panic" can be treated as more than a name.

## 3. `error`

> **Open question:** `error` is part of `runtime.xe` but has not been
> described.

## 4. Scope of runtime items

> **Open question:** Whether runtime items (`panic`, `error`, and
> `println` as used in examples) are in scope in every module without an
> `import` is not specified. See `modules.md` §3.