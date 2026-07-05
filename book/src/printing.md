# Write (Output)

The `write()` function outputs values to stdout.

```lil
write("hello")
write(42)
write(3.14)
```

Multiple arguments are printed with spaces between them, followed by a newline:

```lil
write("the answer is", 42)
```

Write with no arguments prints a blank line:

```lil
write("")
```

Write the result of a library function directly:

```lil
write(full@date)
```

Write multiple results from different functions in one line:

```lil
write(minimal@date, random@math)
```

Write values alongside variables:

```lil
name = "bob"
score = 95
write("player", name, "scored", score)
```

## Statement separator

Multiple statements can go on one line with `;`:

```lil
write("a") ; write("b")
```

## Comments

`:` starts a single-line comment. `::` starts a block comment that ends with `::`:

```lil
: this is a comment
write("hello")

:: this is a
block comment ::
write("world")
```

`//` is integer division, not a comment: `10 // 3 = 3`.
