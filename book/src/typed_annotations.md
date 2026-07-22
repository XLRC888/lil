# Type Annotations

lil supports gradual type annotations using the `typed` keyword. Variables can be annotated with a type, and the runtime will automatically coerce values or error on incompatible types.

## Variable Annotations

```lil
typed int x = 42
typed str name = "hello"
typed float pi = 3.14
typed bool flag = 1
```

## Automatic Coercion

The runtime converts values when possible:

```lil
typed int x = "100"    : x is 100 (number)
typed str y = 99       : y is "99" (string)
typed float z = "3.14" : z is 3.14
```

## Type Errors

Incompatible conversions fail at runtime:

```lil
try {
    typed int x = "hello"
} catch e {
    write("error: ", e.msg)
}
```

Output: `error: cannot convert 'hello' to number`

## Function Annotations

Functions can annotate parameter types and return type:

```lil
#add(a typed int, b typed int) typed int {
    a + b
}
```

The runtime checks parameter types at call time and coerces the return value:

```lil
typed str s = "42"
typed int n = add(s, 1)  : n is 43
```

## Why `typed` instead of `x: int`?

lil uses `:` for single-line comments (`: this is a comment`). Using `x: int` syntax would conflict. The `typed` keyword follows the same pattern as `force`/`unforce`:

- `force x` - mark variable as immutable
- `typed int x` - mark variable as typed

## Standalone Conversion

The `intify`, `stringify`, and `toggle` forms still work for in-place conversion without annotations:

```lil
x = "hello"
intify x
: x is now 0 (conversion failed, defaults to 0)

y = 99
stringify y
: y is now "99"
```
