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

## Dynamic math with eval

The `&math|eval` function evaluates a math expression string at runtime:

```lil
result = &math|eval "(10 + 5) * 2"
print result
```

This is useful when the expression comes from user input or is built dynamically:

```lil
a = 3
b = 4
expr = "(" + a + " + " + b + ") * 2"
result = &math|eval $expr
print result
```
