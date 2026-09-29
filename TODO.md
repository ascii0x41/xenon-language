
# Xenon TODO

## v0.1
- [x] Immutable and mutable bindings (`let`, `let mut`)
- [x] Functions (`pub? static? func <name>(<params>)[-> <return type>]?`)
- [x] Selection and iteration (`if`, `while`)
- [x] Structs via `type`
```
type Point {
    x: f64;
    y: f64;
}
```
- [x] Methods, static methods
- [x] Fields, static fields
- [x] Operators, ternary, in-place assignment
- [x] Structural literals (`Point { x, y }`)
- [x] `impl` blocks separating structure from behavior
- [x] Explicit `self: &T` / `self: &mut T`
- [x] Modules (`module main;`, `import`)


## v0.2 — Type System Expansion, Generics, Enums, Stdlib

### Type System
- [ ] Alias type:
```
type Age = u8;
```
- [ ] Option type:
```
type Option<T> {
    has: bool;
    data: T;
}

impl<T> Option<T> {
    pub static func Some(data: T) -> Option<T> {
        return Option<T> { has: true, data: data };
    }

    pub static func None() -> Option<T> {
        return Option<T> { has: false, data: T {} };
    }
}
```
- [ ] Result types:
> TBD
- [ ] Copy-by-default semantics formalized
  - [ ] Which types are Copy? (primitives yes, structs if all fields Copy)
  - [ ] Reference types (`&T`, `&mut T`) always Copy


### Generics
- [ ] Generic types: `type Box<T> { data: *mut T; }`
- [ ] Generic impls: `impl<T> Box<T> { ... }`
- [ ] Generic functions: `func identity<T>(x: T) -> T`
- [ ] Monomorphization strategy
- [ ] Trait bounds: `func f<T: Display>(x: T)`
- [ ] Turbofish: Parse `::<...>` in expression position

### Traits
- [ ] `trait` declaration
```
trait Seralise {
    func serialise(&Self) -> string;
}

impl Serialise for Point {
    func serialise(self: &Point) -> string {
        return $"{{x: {self.x}, y: {self.y}}}";
    }
}
```

### Enums
- [ ] Enums:
```
enum Colour {
    RED,
    BLUE,
    GREEN
}
```

### Stdlib

- [ ] Handling for `module.toml`

#### Memory
- [ ] `Box<T>` — unique ownership, just `*mut T`
- [ ] `Box<T>::init(value: T) -> Box<T>` via compiler intrinsic
- [ ] Deref semantics for `Box<T>` (auto-deref on field access?)

#### IO
- [ ] Works both on POSIX and Windows

#### Math
- [ ] Link with -lm for C interop

### Misc
- [ ] `xec build <path to project>` 


## v0.3 — Error Handling, Iterators via Traits

### Error Handling
- [ ] `?` propagation operator for result types
- [ ] Early exit semantics

### Iterators
- [ ] `map`, `filter`, `fold`, `collect` via traits
- [ ] Lazy evaluation guarantees
- [ ] `for` loop over ranges: `for i in 0..10 { ... }`
- [ ] `for` loops using traits `Iterate`, `Iterator`
  ```xenon
  for x in collection { ... }
  ```

### Type Aliases (deeper)
- [ ] Generic aliases: `type Pair<T> = [T; 2];`
- [ ] Associated types in traits


## v0.4+ — Open Questions / Future (Uncertain)

### Language
- [ ] Lambda syntax
- [ ] Pattern matching (`match`)
- [ ] Enums
- [ ] Function args

### Tooling
- [ ] Package manager
- [ ] Build system
- [ ] LSP
- [ ] Formatter (`xenonfmt`)
- [ ] Docs generator

---

## Rejected Ideas
- Constructors — structural literals + `init` functions cover this
- Inheritance — traits + composition only
- Implicit `this` — explicit `self: &T` is better
- Move semantics for v1 — Copy-by-default keeps things simple
