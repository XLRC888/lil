# Advanced Features

This chapter covers the advanced features of lil: templates, the has operator, compiled mode, Minecraft Forge mod generation, and C extensions. If you know Python, many of these concepts will feel familiar under a different syntax.

## What You'll Find Here

| Topic | Python Analogy | What It Does |
|-------|---------------|--------------|
| Templates | f-strings like `f"hello {name}"` | Inline variable substitution in strings |
| Has operator | `in` keyword like `"sub" in text` | Check variable existence or substring match |
| Compiled mode | Cython / Nuitka | Compile lil scripts to standalone C binaries |
| Minecraft Forge | N/A | Generate complete Minecraft Forge mod Java source |
| C extensions | `ctypes` / `cffi` | Direct memory access with address-of and dereference |

Each topic is self-contained. Jump to whatever you need:

- [Templates](templates.md) - string interpolation with `{varname}`
- [Has operator](has.md) - substring matching and variable existence checks
- [Compiled mode](compiled.md) - AOT compilation for 10x speedup
- [Minecraft Forge](minecraft_forge.md) - Java mod source generation
- [C extensions](c_ext.md) - low-level memory operations with `@` and `^`

These features are considered advanced because they either:
- Only work in specific modes (compiled mode for C extensions, interpreted mode for `has`)
- Change how you structure your code (templates vs string concatenation)
- Expose low-level behavior (memory addresses, direct compilation)

Start with [templates](templates.md) if you want easier string building, or jump to [compiled mode](compiled.md) if you need raw speed.
