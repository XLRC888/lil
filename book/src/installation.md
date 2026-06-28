# Installation

## Requirements

- A C99 compiler (gcc, clang, or compatible)
- git
- make (optional)

## Building from source

```bash
git clone https://github.com/XLRC888/lil.git
cd lil
./install.sh
```

This compiles lil.c and copies the binary to `~/.local/bin/lil`. Make sure that directory is on your PATH.

If you prefer to build in place:

```bash
make
./lil
```

## Verifying

Run `lil --help` to confirm everything works:

```
$ lil --help
usage: lil [options] [file.lil]
  no args      start REPL
  file.lil     execute file
  -c file.lil  compile to standalone binary
  -o output    output filename (default: a.out)
```
