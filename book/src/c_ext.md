# C Extensions

lil provides two operators that give you direct access to memory. These only work in compiled mode.

## Address-of (@)

The `@` operator returns the memory address of a variable as a number:

```lil
x = 42
addr = @x
print addr    # prints some large number (the address)
```

## Dereference (^)

The `^` operator reads a value from a memory address:

```lil
x = 42
addr = @x
value = ^addr
print value    # prints 42
```

These are useful for low-level programming and interfacing with hardware at specific memory addresses (MMIO).

In interpreted mode, both operators produce a compile error.
