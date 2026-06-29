# Installation

## Requirements

- A C99 compiler (gcc, clang, or compatible)
- git
- make (optional)

## Building from source

Python devs are used to `pip install`. lil works differently: you clone the repo and compile it yourself:

```bash
git clone https://github.com/XLRC888/lil.git
cd lil
./install.sh
```

This compiles lil.c and copies the binary to `~/.local/bin/lil`. Make sure that directory is on your PATH.

Check your PATH to confirm `~/.local/bin` is listed:

```bash
echo $PATH    # look for /home/yourname/.local/bin
```

If you prefer to build in place:

```bash
make
./lil
```

After rebuilding with `make`, always run `make install` to keep the system binary up to date:

```bash
make
make install
```

For GTK support:

```bash
make gtk
make install-gtk
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
