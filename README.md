# Xenon 0.1 (alpha)

Xenon is a compiled, statically-typed, scientific-computation-first systems language focused on performance, correctness, and expressive low-level programming.

> **⚠️ Work in progress!**
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

### Build the compiler

```bash
cmake -B build
cmake --build build
```

This produces the compiler executable `xec` in the build output directory.

### Run the compiler help

```bash
xec --help
``` 

## Usage

Xenon currently supports:
- `xec build` – validate a project using `xenon.toml`
- `xec check` – parse and validate a project or file
- `xec run <file.ar>` – build and run a single file (runtime execution is experimental)
- `xec <file.ar>` – compile and validate a single file directly

For detailed commands and flags, see [CLI Usage](docs/compiler/cli.md).

## Getting Started

For a step-by-step introduction, see [Getting Started](docs/getting-started.md).

## Project Goals

Xenon aims to explore:
- safe systems programming
- scientific and numerical computing
- modern compiler xechitecture
- expressive but predictable language design

## Authors

*Aryee G.* - Lead developer
