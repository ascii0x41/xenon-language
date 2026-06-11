# CLI Usage

The Xenon compiler executable is `xec`.

## Basic usage

```bash
xec [command] [options] [--] [file]
```

## Commands

- `build` – Build the current project using `xenon.toml`.
- `check` – Parse and validate a project or single file.
- `run <file.ar>` – Build and run a single file.
- `help` – Show help text.
- `version` – Show compiler version information.

If no command is provided, `xec` assumes `build` for project mode or direct file mode when a `.ar` file is passed.

## Direct File Mode

When you pass a `.ar` file directly, `xec` operates on that file without `xenon.toml`.

Examples:

```bash
xec hello.ar
xec check hello.ar
xec run hello.ar
xec hello.ar -o output_binary
```

## Project Mode

If no `.ar` file is provided, `xec` sexeches upward from the current working directory for `xenon.toml`.

Examples:

```bash
xec build
xec build --release
xec build --target arm64-macos
xec build -o dist/myapp
xec check
```

## Options

- `-o`, `--output <path>` – Override output path.
- `--target <triple>` – Set the compilation target triple.
- `--release` – Set release optimisation mode.
- `--debug` – Set debug optimisation mode.
- `-Werror` – Treat all warnings as errors.
- `-Werror=<warning>` – Treat a specific warning as an error.
- `-Wno-<warning>` – Disable a specific warning.
- `--no-colour` – Disable ANSI colour output.
- `--verbose`, `-v` – Print more verbose compiler messages.
- `--help`, `-h` – Show this help message.
- `--version`, `-V` – Show compiler version.

## Notes

- `xec build` currently validates the project and loads modules, but full code generation is still under development.
- `xec check` is useful for syntax validation and early error checking.
