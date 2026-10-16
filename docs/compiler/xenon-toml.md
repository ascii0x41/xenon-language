# xenon.toml

Xenon project mode is configured with `xenon.toml` placed at the project root.
The compiler sexeches upward from the current working directory until it finds this file.

## Required sections

### `[project]`

Required keys:

* `name` — project name.
* `version` — project version.
* `entry` — path to the main Xenon source file relative to the project root.

Optional keys:

* `authors` — array of author names.

Example:

```toml
[project]
name = "my_project"
version = "0.1.0"
authors = ["Your Name"]

source_files = [
    "src/main.xe",
]

[build]
output = "build/"
target = "x86_64-linux"
optimisation = "debug"
```

## Optional build configuration

### `[build]`

* `target` — target triple, e.g. `x86_64-linux`.
* `output` — output directory for build artifacts. Defaults to `build/. (**Optional**)
* `optimisation` — `none`, `debug`, or `release`. (**Optional**)

Example:

```toml
[build]
output = "build/"
target = "x86_64-linux"
optimisation = "debug"
```

## Optional warnings configuration

### `[warnings]`

Use this section to set warning levels for named warnings.
Supported values are `ignore`, `warn`, and `error`.

Example:

```toml
[warnings]
dangling_ref = "error"
unused_var = "warn"
dead_code = "warn"
```

## Optional pedantic mode

### `[pedantic]`

This section currently supports one key:

* `enabled` — set to `true` or `false`.

Example:

```toml
[pedantic]
enabled = "false"
```

## Notes

* The TOML parser currently supports simple key-value pairs and string arrays.
* Comments are supported with `#`.
* The compiler only reads values from the sections shown above.
