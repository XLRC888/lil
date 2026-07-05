# C Extensions

lil provides two operators that give you direct access to memory. These only work when compiling with `lil -c`. Running them with the interpreter produces an error.

If you've used Python's `ctypes` module to get the address of a C object or read memory at a raw pointer, the concept is the same: `@` is like `ctypes.addressof()` and `^` is like `ctypes.string_at()`.

## Address-of (@)

The `@` operator returns the memory address of a variable as a number:

```lil
x = 42
addr = @x
write(addr)
```

The address will be different every time you run the program (ASLR). One run might print `140732789041152`, the next `140732789041184`.

You can also get addresses of string variables:

```lil
msg = "hello"
msg_addr = @msg
write(msg_addr)
```

## Dereference (^)

The `^` operator reads a value from a memory address:

```lil
x = 42
addr = @x
value = ^addr
write(value)    : prints 42
```

This round-trips through a pointer: write a value, take its address, then dereference it back.

## Practical Example

```lil
x = 42
x_addr = @x
x_val = ^x_addr
write("address: ", x_addr)
write("value: ", x_val)
```

Each run will show a different address but always the same value.

## Safety Warning

Dereferencing an invalid address will crash your program with a segfault, just like Python's `ctypes.string_at(bad_address)`:

```lil
value = ^9999999    : almost certainly a segfault
```

These operators are useful for low-level programming and interfacing with hardware at specific memory addresses (MMIO). Use them carefully.

In the interpreter (`lil file.lil`), both operators produce an error.
