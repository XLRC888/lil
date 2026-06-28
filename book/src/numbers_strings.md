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

Use `strify` and `intify` to convert between types explicitly.

`strify` converts a variable to its string representation:

```lil
x = 42
strify x     # x is now "42"
```

`intify` converts a variable to a number:

```lil
x = "42"
intify x     # x is now 42
x = x + 5
print x      # prints 47
```

If the string does not contain a valid number, `intify` produces an error:

```lil
x = "hello"
intify x     # error: cannot convert to number
```
