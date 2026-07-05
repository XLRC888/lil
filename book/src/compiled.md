# Compiled Mode

lil can compile scripts into standalone binaries. The resulting executable does not need lil to run. Think of it like Cython for Python: same source, faster execution, no interpreter required.

## How it works

When you compile, lil:

1. Reads and parses the script
2. Infers variable types (numeric variables become C doubles)
3. Generates a C file containing the program logic and a minimal runtime
4. Compiles the C file with gcc into a standalone binary

## Usage

Write a script:

```lil
#fib(n) {
  if n <= 2 {
    1
  } else {
    fib(n - 1) + fib(n - 2)
  }
}

result = fib(35)
write("fib(35) = ", result)
```

Compile and run:

```bash
lil -c fib.lil -o fib
./fib
```

If you omit `-o`, the output name is derived from the input filename (the `.lil` extension is removed). If that fails, it defaults to `a.out`.

## Standalone Binary

The compiled binary is self-contained. Copy it to another machine (same architecture) and it runs without lil installed:

```bash
scp fib user@other-machine:~
ssh user@other-machine ./fib    : works without lil
```

## Performance

Compiled mode is significantly faster than interpreted mode, especially for numeric code. The type inference pass eliminates the overhead of dynamic type checks for variables that are proven to always hold numbers.

| benchmark | interpreted | compiled | C |
|-----------|-------------|----------|----|
| loop 50M  | 0.56s       | 0.05s    | 0.05s |
| primes 5k | 0.05s       | 0.005s   | 0.004s |
| fib(35)   | 4.2s        | 0.4s     | 0.3s |

Compare for yourself:

```bash
time lil fib.lil
time ./fib
```

Compiled code matches C performance on simple numeric loops. The `fib(35)` recursive function runs roughly 10x faster when compiled.

## Functions

User-defined functions (`#name()`) work in compiled mode. Each function becomes a standalone C function with its own local variable scope and type inference. Function calls work inside expressions, control flow, and can be recursive.

## What Compiles (and What Doesn't)

Simple operations, arithmetic, control flow, user-defined functions, and variable assignments all compile fine:

```lil
x = 42
y = x + 1
write(y)
```

This WILL compile and run correctly.

Library calls like `full@date` will NOT compile. The compiler generates a stub that returns a default value (0 for numbers, empty string for strings):

```lil
write(full@date)          : compiles but prints nothing useful
write(cmd@sys("ls"))      : returns 0 in compiled mode
write("hello")             : this compiles and works fine
```

## Limitations

The following features are not available in compiled mode:

- The `has` operator in conditions
- Templates (template strings)
- Library function calls (`func(args)@lib`)

If your script uses any of these features, compilation will fail and lil will tell you which construct could not be compiled.

## Short-circuit && and ; in compiled mode

`&&` and `;` work in compiled mode but fall back to the interpreter for execution. They work correctly but don't get the full AOT optimization.

## Modifiers in AOT

`force` and `unforce` still parse and run in compiled mode, but the locking concept does not apply. Variables in the generated C are statically typed, so `force x = 42` simply becomes `double x = 42;` (or the inferred type) with no lock attached. A later `x = 30` in the same compiled script will not be rejected the way it would in the interpreter. `unforce x` compiles to a no-op.
