# Compiled Mode

lil can compile scripts into standalone binaries. The resulting executable does not need lil to run.

## How it works

When you compile, lil:

1. Reads and parses the script
2. Infers variable types (numeric variables become C doubles)
3. Generates a C file containing the program logic and a minimal runtime
4. Compiles the C file with gcc into a standalone binary

## Usage

```bash
lil -c myscript.lil -o myprogram
./myprogram
```

If you omit `-o`, the output name is derived from the input filename (the `.lil` extension is removed). If that fails, it defaults to `a.out`.

## Performance

Compiled mode is significantly faster than interpreted mode, especially for numeric code. The type inference pass eliminates the overhead of dynamic type checks for variables that are proven to always hold numbers.

| benchmark | interpreted | compiled | C |
|-----------|-------------|----------|----|
| loop 50M  | 0.56s       | 0.05s    | 0.05s |
| primes 5k | 0.05s       | 0.005s   | 0.004s |

Compiled code matches C performance on simple numeric loops.

## Limitations

The following features are not available in compiled mode:

- The `has` operator in conditions
- Templates (template strings)
- Function calls (`$name()` and `function lib func args`)

If your script uses any of these features, compilation will fail.
