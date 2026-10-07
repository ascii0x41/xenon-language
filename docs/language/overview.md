# Language Overview

## 1. What is Xenon?

Xenon is a compiled, statically typed systems language designed with
scientific computing and low-level programming in mind, with a focus on
performance, correctness, predictable behavior, and expressive systems
programming.

Xenon's philosophy emphasizes numerical correctness, explicitness,
predictability, and simplicity.

Xenon is intended for applications where these characteristics are
important, including:

- Flight computer software
- Scientific and numerical computing
- Data analysis
- Precision machine software
- Aerospace and embedded applications

## 2. Design principles

- **Explicit over implicit.** Avoid silent narrowing and hidden
  allocations. Important operations should be visible in the source code.

- **Predictable numerics.** Numerical behavior should be explicitly
  specified rather than left to implementation-defined behavior.

- **Local reasoning.** A function's behavior should be understandable
  from its signature and body without requiring knowledge of unrelated
  global state.

- **Fail loudly at compile time.** When an error can be detected during
  compilation, prefer a clear compile-time diagnostic over an unexpected
  runtime failure.

## 3. Non-goals

- **Source compatibility with existing languages.** Xenon is not a
  drop-in replacement for C, C++, Rust, or Python. Syntax may look
  familiar, but semantics are defined by Xenon, not inherited.
- **Automatic translation from other languages.** No transpiler is
  planned.
- **A garbage-collected runtime.**

## 4. Mental model

A Xenon program is a collection of modules. Each `.xe` source file is
one module. Modules contain top-level declarations such as functions and
types.

Compilation consists of several stages, including parsing, semantic
analysis, and code generation.

Execution begins at `main`, in the module named `main`.

## 5. Language specification

The documents under `docs/language/` define the intended semantics of
the Xenon language.

The compiler implementation is expected to conform to these documents.
Implementation details are documented separately under `docs/compiler/`.

`syntax.txt` is the source of truth for syntax. It is a parse-only test:
it checks that the grammar is accepted, not that every example passes
semantic analysis.

Open questions that have not yet been decided are marked in the
documents as **Open question**. The semantic analyser must not invent
answers to them.

## 6. Where to go next

### Language

- [Lexical structure](lexical.md)
- [Types](types.md)
- [Literals](literals.md)
- [Expressions](expressions.md)
- [Variables](variables.md)
- [Statements](statements.md)
- [Functions](functions.md)
- [Methods and `impl` blocks](methods.md)
- [References and pointers](references-and-pointers.md)
- [Modules](modules.md)
- [Runtime](runtime.md)

### Compiler

- [Compiler architecture](../compiler/architecture.md)