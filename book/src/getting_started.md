# Getting Started

lil is a simple, fast programming language that can run scripts directly or compile them to standalone C binaries. If you know Python, you'll find the syntax familiar but much simpler.

## What Makes lil Different from Python

- No indentation rules. Use `{}` for blocks, just like C.
- No colons after `if`, `for`, `while`, or function definitions.
- No `self` parameter. No `__init__`. No classes at all.
- No `return` keyword. Functions return the value of their last expression.
- No `import` statement. Use `include` to load other script files.
- Variables are dynamically typed (like Python), but the compiler infers types for C codegen.

## What to Read Next

- [Installation](installation.md) - build and install lil
- [Hello, World](hello_world.md) - your first lil script
- [Basics](basics.md) - syntax overview
- [What is lil?](what_is_lil.md) - language history and design goals
