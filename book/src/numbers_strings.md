# Numbers and Strings

## Numbers

All numbers are floating point internally (C `double`). You can use them like integers:

```lil
x = 42
y = 3.14
z = -7
```

## Strings

String literals use double quotes:

```lil
greeting = "hello"
```

You can concatenate strings with `+`:

```lil
a = "hello "
b = "world"
c = a + b
print c   # prints "hello world"
```

If you add a string to a number, the number is converted to a string first:

```lil
x = "score: " + 42
print x   # prints "score: 42"
```

## Type conversion

Use `val_tonum()` semantics implicitly through arithmetic. If you need to convert a string to a number, arithmetic with `0` works:

```lil
x = "42"
y = x + 0    # y is the number 42
```
