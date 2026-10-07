# Modules

## 1. Overview

A Xenon program is a collection of modules. A module is a unit of
organization: it declares types and functions, and it controls what
other modules can see.

This document specifies:

- Source files and module declarations.
- Imports.
- Visibility (`pub`).
- Paths.
- The `main` module.

## 2. Source files and module declarations

A Xenon project consists of `.xe` source files. **Each `.xe` source file
corresponds to exactly one module.**

Every `.xe` file starts with a module declaration:

```xe
module main;

func main() {
    println("Hello, world!");
}
```

- `module <name>;` is required in every `.xe` file, with one exception:
  `runtime.xe` (see §2.2).
- The name may be a path: `module verification::age_verification;`.
- There are no inline modules. Declarations of the form
  `module math { ... }` and `pub module sub { ... }` do not exist.

The module declaration and any imports must appear at the top of the
file, before ordinary declarations. By convention the module
declaration comes first, then the imports:

```xe
module main;

import verification::age_verification;
import graphics;

func main() {
    ...
}
```

The relative order of the module declaration and the imports is not
semantically significant.

### 2.1 Nested names

A module path such as `verification::age_verification` names a module
that belongs to the `verification` namespace. It is a name; there are
no inline nested modules.

### 2.2 `runtime.xe`

`runtime.xe` is a special module that is part of the compiler/runtime.
It does not need a `module` declaration. It contains the language's
runtime support, such as `panic` and `error`. See `runtime.md`.

### 2.3 Files and project layout

> **Open question:** The exact relationship between file names, module
> names, and project/module discovery has not been specified. There is
> no `crate` concept. The semantic analyser works from module
> declarations and from the module-loading information supplied by the
> compiler; it must not assume any directory layout.

## 3. Imports

Another module is brought into scope with `import`:

```xe
import math;
import utils::helpers;
```

- The operand of `import` is a **module path**. The imported entity is
  the module itself.
- The full module path is required.
- There are no aliases, no glob imports, and no importing individual
  items. (They may be added in a later version.)
- `use` is not part of Xenon.

```xe
module main;

import math;

func main() {
    let x = math::add(1, 2);
}
```

> **Open question:** After `import utils::helpers;`, how is the module
> referred to in code: as `helpers::f()` (last segment) or as
> `utils::helpers::f()` (full path)? Examples in this document use
> single-segment modules so that the question does not arise.

> **Open question:** Whether items of `runtime.xe` (`panic`,
> `error`, and `println` used in examples) are in scope in every module
> without an `import`.

## 4. Visibility

Every declaration is private to its module by default. The `pub`
keyword makes a declaration visible to other modules.

```xe
module math;

pub func add(a: i32, b: i32) -> i32 { ... }    // visible externally
func internal_helper() { ... }                 // private
```

### 4.1 What can be `pub`

- Functions: `pub func f() { ... }`
- Types: `pub type Point { ... }`
- Fields: `pub x: i32;` (within a type declaration)
- Methods and static functions: `pub func ...` and `pub static func ...`
  (within an `impl` block)
- Static variables: `pub static let ...` (within an `impl` block)

Modules themselves have no visibility. A module is accessible from
another module if it has been imported.

### 4.2 Field visibility and struct literals

Fields are private by default. A field is made visible with `pub`:

```xe
module geometry;

pub type Point {
    pub x: i32;
    y: i32;                // private
}
```

Outside the declaring module, `p.x` is accessible; `p.y` is not.

**A struct literal may only initialise fields that are accessible from
the current module.** Private fields cannot be initialised from outside
their defining module. There is no exception because the type itself is
`pub`.

This is what lets a module protect an invariant:

```xe
module verification;

pub type Age {
    value: u8;
}

impl Age {
    pub static func init(value: u8) -> Age {
        if value <= 150 {
            return Age { value };
        } else {
            panic("Invalid age!");
        }
    }
}
```

```xe
module main;

import verification;

func main() {
    let a = verification::Age { 255 };   // error: field `value` is private
    let b = verification::Age::init(40); // ok: goes through the constructor
}
```

Because `value` is private, a literal for `Age` is only legal inside
module `verification`. Code elsewhere has to go through `Age::init`,
which enforces `value <= 150`.

Consequences:

- A literal for a type that has any private field can only appear in the
  defining module.
- A literal outside the defining module is legal only when every field
  of the type is `pub`.
- The same rule applies to named literals (`Age { value: 255 }`) and
  positional literals (`Age { 255 }`).
- A public type with private fields exposes constructors (usually
  `static` functions) to build values.

### 4.3 Method and static visibility

Functions declared in an `impl` block are private by default. They are
made visible with `pub`:

```xe
impl Point {
    pub func get_x(&self) -> i32 { ... }
    func internal(&self) { ... }           // private
}
```

An `impl` block has no visibility of its own; each member declares its
own. See `methods.md`.

## 5. Paths

A path names an item. Paths use `::` as a separator:

```xe
math::add                   // function `add` in module `math`
Point::origin               // static function `origin` of type `Point`
Counter::total              // static variable `total` of type `Counter`
Point                       // a type in the current module
```

- An **unqualified** name looks up the name in the current module
  (declarations in the current file).
- A **qualified** path names an item through an imported module or a
  type.

There is no `crate`, `self::` or `super::` path root.

> **Open question:** Whether `self::` and `super::` exist is not
> specified. `self` is now the method-receiver keyword (see
> `methods.md`).

### 5.1 Resolution

The compiler must resolve every name to exactly one declaration:

- If a name is not found, it is an error.
- A path through a module is accessible only if each item named is
  `pub` (or the code is in the same module).
- Functions with the same name may coexist if their signatures differ
  (function overloading, `functions.md` §7). Overload resolution is
  applied to the functions found by the path.

## 6. The `main` module

The program entry point is the function `main` in the module named
`main`:

```xe
module main;

func main() {
    // program starts here
}
```

`main` does not take arguments in v0.1.

## 7. Accessibility

A declaration is accessible from another module if:

1. The declaration is `pub`, and
2. The module that contains it has been imported by the referring
   module.

## 8. Summary

- Each `.xe` file is one module and starts with `module <name>;`
  (except `runtime.xe`).
- There are no inline modules and no `crate`.
- Imports use `import <module path>;` and import a whole module. There
  is no `use`.
- Declarations are private by default; `pub` makes them visible.
- Fields are private by default, and a struct literal may only
  initialise accessible fields.
- `main` is in the module named `main`.