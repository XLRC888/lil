# Comments

`:` starts a single-line comment. Everything after `:` on that line is ignored:

```lil
: this is a comment
x = 42
```

`::` starts a block comment. Everything between `::` and the next `::` is ignored:

```lil
:: this is a
block comment ::
x = 10
```

Block comments can span multiple lines:

```lil
::
this is
a multi-line
comment
::
```

`#` is for function definitions, not comments: `#add(a, b) { a + b }`.

`//` is integer division, not a comment: `10 // 3 = 3`.
