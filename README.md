# Xenon 0.1 (pre-release)

Xenon is a compiled, statically-typed, scientific-computation-first systems language focused on performance, correctness, and expressive low-level programming.

## Project Goals

Xenon aims to explore:
- safe systems programming
- scientific and numerical computing
- modern compiler architecture
- accurate, precise, and predicable yet expressive language design


## Note: WIP
> The lexer and parser are functional.
> The semantic analyzer is under development.
> Code generation is not implemented yet.

## Note: Memory Safety
Xenon does not currently enforce memory safety at the compiler level.
References are not lifetime-checked. This is planned for a future version.

## Project Status

| Component            | Status         |
|---------------------|----------------|
| Lexer               | Complete       |
| Parser              | Complete       |
| Semantic analyser   | In progress    |
| Code generator      | Not started    |

## Quick Start

### Requirements

* **python**
* **cmake**
* **clang**
* **llvm**

> Note: **python** and **cmake** are only required to build the

### Build the compiler

```bash
python installer.py
```

This produces the compiler executable `xec` that is now usable in the terminal

### Run the compiler help

```bash
xec --help
``` 

## Usage

Xenon currently supports:
- `xec build` – validate a project using `xenon.toml`
- `xec check` – parse and validate a project using `xenon.toml`

For detailed commands and flags, see [CLI Usage](docs/compiler/cli.md).

## Getting Started

For a step-by-step introduction, see [Getting Started](docs/getting-started.md).

## Documentation Roadmap

The documentation is organized to guide compiler development, especially the semantic analyser:

- [Language overview](docs/language/overview.md) — goals, philosophy, and the mental model of the language.
- [Language reference](docs/language/) — lexical structure, types, variables, expressions, statements, functions, methods, operators, modules, and diagnostics.
- [Compiler documentation](docs/compiler/) — architecture, parsing, semantic analysis, type system, and code generation.

These pages are intentionally written as a starting point that can evolve into the definitive design and implementation reference for the compiler.


## Authors

*Gabriel Aryee* - Lead developer
