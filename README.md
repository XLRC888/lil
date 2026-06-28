lil is a small programming language you can either interpret or compile to a standalone binary.

## setup

```bash
git clone https://github.com/XLRC888/lil.git
cd lil
./install.sh
```

this builds lil.c with gcc and installs to ~/.local/bin/lil. make sure that directory is in your PATH. or just run `make` and use ./lil directly.

no external dependencies. just need gcc or any c99 compiler.

## usage

```bash
lil file.lil          # run a script
lil                   # interactive repl
lil -c file.lil       # compile to standalone binary
lil -c file.lil -o myprog  # compile with custom output name
```

## performance

| benchmark  | python3 | lil (vm) | lil (aot) | C |
|------------|---------|----------|-----------|----|
| loop 50M   | 3.38s   | 0.56s    | 0.05s     | 0.05s |
| primes 5k  | 0.13s   | 0.05s    | 0.005s    | 0.004s |
| strcat 1M  | 0.14s   | 0.14s    | 0.13s     | 0.01s  |

## documentation

the lil book is available at https://xlrc888.github.io/lil/
source is in the book/ directory.

lil vm beats python on compute. compiled output matches C on simple loops.
