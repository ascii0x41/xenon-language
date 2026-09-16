
# Xenon TODO

## Guiding Principles
- Explicit over implicit (self, mutability, visibility)
- Structural literals, not constructors
- Copy-by-default, opt-in reference semantics
- C-family syntax, Rust-family rigor



## v0.1 — DONE
- [x] Modules (`module main;`, `import`)
- [x] Functions (`func`, `-> T` return types)
- [x] Variables (`let`), constants (`let` in impl)
- [x] `if` / `while`
- [x] Structs via `class` (soon `type`)
- [x] Methods, static methods
- [x] Fields, static fields
- [x] Operators, ternary, in-place assignment
- [x] Structural literals (`Point { x, y }`)
- [x] `impl` blocks separating structure from behavior
- [x] Explicit `self: &T` / `self: &mut T`



## v0.2 — Generics, Iteration, Type System Expansion

### Type System
- [ ] Replace `class` with `type`
  - [ ] `type Age = u8;` (alias)
  - [ ] `type Point { ... }` (struct)
  - [ ] Parser disambiguation after name
- [ ] Optional types: `type OptionalString = string?;`
- [ ] Result types: `type ValueOrError = i32!str;`
- [ ] Copy-by-default semantics formalized
  - [ ] Which types are Copy? (primitives yes, structs if all fields Copy)
  - [ ] Reference types (`&T`, `&mut T`) always Copy

### Generics
- [ ] Generic types: `type Box<T> { data: *mut T; }`
- [ ] Generic impls: `impl<T> Box<T> { ... }`
- [ ] Generic functions: `func identity<T>(x: T) -> T`
- [ ] Monomorphization strategy
- [ ] Trait bounds: `func f<T: Display>(x: T)`

### Traits
- [ ] `trait` declaration
- [ ] `impl Trait for Type`
- [ ] Default methods
- [ ] Iterator trait
- [ ] `Self` keyword
  ```xenon
  trait Iterator {
      type Item;
      func next(&mut Self) -> Item?;
  }
  ```
- [ ] `IntoIterator` for `for` loop desugaring
- [ ] `Cast` TBD
- [ ] `Drop` trait for RAII or DES (Drop at End of Scope)
    ```xenon
    pub trait Drop {
        func drop(self: &mut Self);
    }

    impl Drop for MyType {
        func drop(self: &mut MyType) {
            // cleanup
        }
    }
    ```

### Turbofish
- [ ] Parse `::<...>` in expression position

### Control Flow
- [ ] `for` loops
  ```xenon
  for x in collection { ... }
  ```
- [ ] Desugars to `IntoIterator::into_iter` + `Iterator::next`
- [ ] `break` / `continue` (with optional labels?)

### Memory
- [ ] `Box<T>` — unique ownership, just `*mut T`
- [ ] Decide: is `Box` with refcount actually `Rc<T>`?
- [ ] `Box<T>::init(value: T) -> Box<T>` via compiler intrinsic
- [ ] Deref semantics for `Box<T>` (auto-deref on field access?)
- [ ] Drop / finalizer story (needed once `Box` exists)



## v0.3 — Error Handling, Iterators via Traits

### Error Handling
- [ ] `?` propagation operator for result types
- [ ] Early exit semantics
- [ ] Integration with `i32!str` result types

### Iterators (full)
- [ ] `map`, `filter`, `fold`, `collect` via traits
- [ ] Lazy evaluation guarantees
- [ ] `for` loop over ranges: `for i in 0..10 { ... }`

### Type Aliases (deeper)
- [ ] Generic aliases: `type Pair<T> = (T, T);` (needs tuples?)
- [ ] Associated types in traits



## v0.4+ — Open Questions / Future

### Language
- [ ] Closures / lambdas — syntax? capture rules?
- [ ] Pattern matching (`match`)?
- [ ] Tuples? Named tuples?
- [ ] Enums / tagged unions? (needed for real `Optional` / `Result`?)
- [ ] Const generics?
- [ ] Variadics / varargs?
- [ ] Async / await?

### Memory
- [ ] `Rc<T>`, `Arc<T>` (if not folded into `Box`)
- [ ] `Weak<T>`
- [ ] Slices / views
- [ ] Arenas / allocators as first-class?

### Tooling
- [ ] Package manager
- [ ] Build system
- [ ] LSP
- [ ] Formatter (`xenonfmt`)
- [ ] Docs generator

---

## Rejected Ideas (for now)
- Constructors — structural literals + `init` functions cover this
- Inheritance — traits + composition only
- Implicit `this` — explicit `self: &T` is better
- Move semantics for v1 — Copy-by-default keeps things simple
