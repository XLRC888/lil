# Operators

## Arithmetic

```lil
a = 10 + 3    # addition, result 13
b = 10 - 3    # subtraction, result 7
c = 10 * 3    # multiplication, result 30
d = 10 / 3    # division, result 3
e = 10 % 3    # modulo, result 1
```

## Unary minus

```lil
x = -5
y = -(x + 3)
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
