# Operators

## Arithmetic

```lil
a = 10 + 3
b = 10 - 3
c = 10 * 3
d = 10 / 3
e = 10 % 3
```

Integer division:

```lil
x = 10 // 3    : 3
```

Modulo with negative numbers follows the sign of the dividend:

```lil
write(-10 % 3)
write(10 % -3)
```

## Unary minus

```lil
x = -5
y = -(x + 3)
```

## Logical operators

lil uses `and`, `or`, `not` like Python, but also supports `&&`, `||`, `!` as aliases:

```lil
if a > 0 and b > 0 {
    write("both positive")
}
if a > 0 || b > 0 {
    write("at least one is positive")
}
```

## Short-circuit &&

`&&` short-circuits: the right side is only evaluated if the left side is truthy. It returns the actual value, not 0/1:

```lil
x = 0
result = x && 5    : returns 0 (x is falsy, 5 never evaluated)
y = "hello"
result2 = y && 42   : returns 42 (y is truthy)
```

Useful in while conditions:

```lil
while i < 10 && arr[i] != "" {
    i = i + 1
}
```

## Statement separator ;

`;` separates multiple statements on one line. It has the lowest precedence:

```lil
a = 5 ; b = 10 ; write(a + b)
```

## Special syntax

`#` defines a function. `#add(a, b) { a + b }` creates a function named `add`. `#(x) { x * 2 }` creates an anonymous function (closure) without a name.

`@` makes a library call. `randint@math(1, 10)` calls `randint` from the `math` library.

`%` declares a variable with value 0. `%x` is the same as `x = 0`.

## Operator precedence

From lowest to highest:

1. `;`
2. `or`
3. `and`
4. `not`
5. `==`, `!=`, `<`, `>`, `<=`, `>=`
6. `+`, `-`
7. `*`, `/`, `%`, `//`
8. unary `-`, unary `not`
9. parentheses `()`

Use parentheses for clarity:

```lil
result = (1 + 2) * (3 + 4)
```

Precedence matters in real calculations:

```lil
x = 10 + 5 * 2
write(x)
y = (10 + 5) * 2
write(y)
```

## Direct arithmetic

lil evaluates math expressions directly:

```lil
result = (10 + 5) * 2
write(result)
```
