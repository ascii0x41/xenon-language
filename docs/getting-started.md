# Getting Started with Xenon

## Requirements

- CMake 3.20+
- C++20 compiler (GCC or Clang)
- LLVM libraries (for future backend work)

## Build the compiler

```bash
cmake -B build
cmake --build build
```

This produces the compiler executable in `build/xec`.

## Check the compiler is working

```bash
xec --help
```

## Direct File Mode

You can compile or validate a single file without an `xenon.toml` project.

Create `hello.xe`:

```xenon
func main() -> i32 {
    writeln("Hello, world!");
    return 0;
}
```

Compile or validate directly:

```bash
xec hello.xe
xec check hello.xe
```

Run a single file with `xec run`:

```bash
xec run hello.xe
```

> Note: the compiler currently parses and validates input. Full code generation and executable output are still under development.

## Project Mode with `xenon.toml`

Project mode is useful when you want a reusable project configuration.

Create `xenon.toml` in your project root:

```
xec init hello_world
```

For info on `xenon.toml`, see [Xenon Build Config](compiler/xenon-toml.md)

Create `src/main.xe`:

```xenon
func main() -> i32 {
    writeln("Hello, world!");
    return 0;
}
```

Build the project:

```bash
xec build
```

Validate the project without building:

```bash
xec check
```

For an in-depth CLI usage guide, see [CLI Usage](compiler/cli.md)

