# Functions

lil has two kinds of functions: built-in library functions from the standard libraries, and user-defined functions you write yourself.

## Built-in Library Functions

These are functions that come with lil, organized by library like `math`, `string`, `file`, `sys`, and `date`. You call them with `func(args)@lib` syntax:

```lil
randint@math(1, 6)
```

The function name comes first, then `@`, then the library name, then arguments in parentheses.

```lil
name = "alice"
upper@string(name)
write(name)
```

See the [Built-in Libraries](builtins.md) page for the full list of available functions.

## User-defined Functions

When you need custom logic, define your own functions with `#` syntax:

```lil
#add(a, b) {
  a + b
}
```

There's no `def`, no colon, and no `return` keyword. The function body is a block in braces, and the last expression's value is automatically returned.

```lil
result = add(3, 4)
write(result)
```

Functions can also be created without a name (anonymous functions/closures) for inline use:

```lil
double = #(x) { x * 2 }
write(double(5))
```

See [User-defined Functions](user_functions.md) and [Advanced Functions](functions_advanced.md) for details.

## Choosing Which to Use

Library functions handle built-in operations: math, strings, files, dates, system commands. User-defined functions let you organize your own code, avoid repetition, and build reusable abstractions. You'll typically use both together.
