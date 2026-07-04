# Operators

## Arithmetic

```lil
a = 10 + 3
b = 10 - 3
c = 10 * 3
d = 10 / 3
e = 10 % 3
```

Modulo with negative numbers follows the sign of the dividend:

```lil
print -10 % 3
print 10 % -3
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
    print "both positive"
}
if a > 0 || b > 0 {
    print "at least one is positive"
}
```

## Special syntax

`#` defines a function. `#add(a, b) { a + b }` creates a function named `add`. `#(x) { x * 2 }` creates an anonymous function (closure) without a name.

`@` makes a library call. `randint@math 1 10` calls `randint` from the `math` library.

`%` declares a variable with value 0. `%x` is the same as `x = 0`.

## Operator precedence

From lowest to highest:

1. `or`
2. `and`
3. `not`
4. `==`, `!=`, `<`, `>`, `<=`, `>=`
5. `+`, `-`
6. `*`, `/`, `%`
7. unary `-`, unary `not`
8. parentheses `()`

Use parentheses for clarity:

```lil
result = (1 + 2) * (3 + 4)
```

Precedence matters in real calculations:

```lil
x = 10 + 5 * 2
print x
y = (10 + 5) * 2
print y
```

## Direct arithmetic

lil evaluates math expressions directly:

```lil
result = (10 + 5) * 2
print result
```
