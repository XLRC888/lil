# C Extensions

lil provides two operators that give you direct access to memory. These only work when compiling with `lil -c`. Running them with the interpreter produces an error.

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

In the interpreter (`lil file.lil`), both operators produce an error.
