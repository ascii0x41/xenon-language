# Methods and `impl` Blocks

## 1. Overview

An `impl` block attaches functions and static variables to a user-defined
type.

This document specifies:

* `impl` blocks.
* Methods and the `self` receiver.
* Static functions and static variables.
* Method calls.
* Visibility.

Xenon v0.1 has no traits. `impl` blocks are inherent only: `impl <Type>`.

## 2. `impl` blocks

```xe
impl Point {
    ...
}
```

An `impl` block contains:

* Methods (functions with a `self` receiver).
* Static functions (`static func`).
* Static variables (`static let`).

An `impl` block has no visibility of its own. Each member declares its
own visibility with `pub` (see [modules.md](modules.md) §4.3).

## 3. Methods

A **method** is a function in an `impl` block whose first parameter is a `self` receiver. There are two receiver forms:

| Receiver    | Meaning                                   |
| ----------- | ----------------------------------------- |
| `&self`     | Receiver is passed by immutable reference |
| `&mut self` | Receiver is passed by mutable reference   |

```xe
type Point {
    x: f64;
    y: f64;
}

impl Point {
    pub func transpose(&mut self, other: &Point) {
        self.x += other.x;
        self.y += other.y;
    }

    pub func get_x(&self) -> f64 {
        return self.x;
    }
}
```

`self` has a special meaning only in method declarations and method bodies. It cannot be used as the name of an ordinary function parameter.

The receiver's underlying type is the type being implemented: in `impl Point`, `&self` means `&Point`, and `&mut self` means `&mut Point`.

Inside a method body, `self` is a reference and follows the ordinary reference rules. Field access automatically dereferences it, so `self.x` accesses the field directly. See [references-and-pointers.md](references-and-pointers.md) §2.2.


## 4. Static functions and variables

A **static function** has no receiver. It belongs to the type and is called through the type's name:

```xe
impl Point {
    pub static func init(x: f64, y: f64) -> Point {
        return Point { x, y };
    }

    pub static func origin() -> Point {
        return Point { 0.0, 0.0 };
    }
}

let p = Point::origin();
```

A **static variable** is declared with `static let` or `static let mut` and is accessed through the type's name:

```xe
impl Counter {
    pub static let MAX: i32 = 100;
    pub static let mut total: i32 = 0;
}

Counter::total += 1;
```

Static variables follow these rules:

* **Mutability.** `static let` declares an immutable static variable. `static let mut` declares a mutable static variable. A mutable static may be modified from any function that can access it, subject to the ordinary visibility rules.
* **Initialization.** A static variable's initializer may be an arbitrary expression, including one that executes code. The initializer is evaluated once, when the static variable is first accessed.
* **Lifetime.** A static variable exists for the entire duration of the program.
* **Visibility.** Static variables follow the ordinary member visibility rules: they are private to the declaring module by default, and `pub` makes them accessible to other modules, subject to module visibility rules.
* **Types.** A static variable may have any type permitted for an ordinary variable declaration, subject to the ordinary type rules.


## 5. Method calls

A method call is written `<receiver>.<method>(<args>)`:

```xe
let p1 = Point::init(3.0, 4.0);
let mut p2 = Point::init(6.0, 8.0);

p2.transpose(&p1);
let x = p1.get_x();
```

The receiver written before the `.` is the implicit `self` argument. The call supplies the receiver in the form required by the method's receiver declaration:

* `&self`: the call passes `&receiver`.
* `&mut self`: the call passes `&mut receiver`. The receiver must be a mutable place ([variables.md](variables.md) §3.2).

This is the only place where a reference is created without the caller explicitly writing `&` or `&mut`. Conceptually, `p.get_x()` for a method declared with `&self` is equivalent to passing `&p` as its receiver argument.

Other arguments follow the normal rules ([functions.md](functions.md) §5.1). For example, in `p2.transpose(&p1)`, `other: &Point` must be passed as `&p1`.

Static functions are not called with `.` syntax. `p.origin()` is an error.


### 5.1 Choosing among receiver forms

Methods may be overloaded on the receiver form. `get(&self)` and
`get(&mut self)` are distinct signatures, because `&T` and `&mut T` are
distinct types ([functions.md](functions.md) §7). A call is resolved as follows:

1. **Reference receivers pass through.** If the receiver expression
   already has type `&T` or `&mut T`, the overload whose `self` has
   exactly that type is chosen, and the reference is passed as it is. If
   only the `&self` form exists, a `&mut T` receiver converts to `&T`
   ([references-and-pointers.md](references-and-pointers.md) §4).
2. **Place receivers use the weakest borrow.** If the receiver is a
   place of type `T`, the overloads are tried in the order `&self`, then
   `&mut self`. The first viable one is chosen. `&mut self` is viable
   only if the place is mutable.
3. Consequently, `v.get()` on a mutable `v` always selects the `&self`
   overload when one exists. Selecting the `&mut self` overload requires
   writing the reference at the call site: `(&mut v).get()`.

**Temporary receivers.** If a method call's receiver is a temporary value rather than a place, the temporary remains alive until the end of the enclosing statement. The temporary must remain valid for the entire method call and is destroyed at the end of the statement.

This rule applies to implicit receiver borrowing for both `&self` and `&mut self`. It does not, by itself, permit explicit references to temporary values.

How a method call is lowered (ABI, name mangling) is an implementation
detail and is not part of the language specification.

## 6. Overloading

Functions in `impl` blocks participate in function overloading with the
same signature rules as ordinary functions ([functions.md](functions.md) §7).

## 7. Visibility

Members of an `impl` block are private to the module that declares the
type's `impl` block by default. `pub` makes a member visible to other
modules. See [modules.md](modules.md) §4.3.