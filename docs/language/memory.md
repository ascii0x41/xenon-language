# Memory Model

## Pointers

Both `ptr T` and `box T` are zero-cost abstractions. They differ only in ownership semantics.

Pointers are nullable and immutable by default.

### Raw Pointer
Use `ptr` to denote a raw pointer.

```
var a = 42;
let pA: ptr i32 = &a; 

// Reassign a to 41;
*pA = 41;
```

- Manual memory management. You call `delete`:

```
delete pA;
```

- Can alias (multiple pointers to same memory).
- No runtime overhead.

### Box Pointer
Use `box` to denote a box pointer

```
... {
    let box_ptr: box i32 = new i32(42);
    *box_ptr = 10;
} // Automatically freed HERE
```

- Box pointers are freed via **D**estruction **A**t **E**nd **O**f **S**cope (**DAEOS**).
- Box pointers can only have one owner and thus, must be moved between owners:
```
let a: box i32 = new i32(42);
let b = move(a);      // a becomes null
let c = *a;           // Runtime error: null dereference
if a != null {        // You can check!
    // safe to use
}
```

## References

References are non-nullable, immutable by default.

```

```

## Mutability

Add `mut` before `ptr`, `box`, or `ref` to turn the pointer or reference mutable.

```
var x: i32 = 42;

// Raw Pointer
let rawX: ptr i32 = &x;
let m_rawX: mut ptr i32 = &x;

// *rawX = 11; // ERROR: rawX is immutable
*m_rawX = 11; // OK

// Box Pointer
let boxX: box i32 = &x;
let m_boxX: mut box i32 = &x;

// *boxX = 11; // ERROR: boxX is immutable
*m_boxX = 11; // OK

// Reference
let refX: ref i32 = &x;
let m_refX: mut ref i32 = &x;

// *refX = 11; // ERROR: refX is immutable
*m_refX = 11; // OK
```

