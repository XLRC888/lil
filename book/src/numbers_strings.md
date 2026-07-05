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
write(c)
```

If you add a string to a number, the number is converted to a string first:

```lil
x = "score: " + 42
write(x)
```

## String length

Get the length of a string with `len@string`:

```lil
name = "alice"
len = len@string("hello")
write(name, "has", len, "characters")
```

## Math on numbers

You can write math expressions directly:

```lil
result = 3.14 * 2
write(result)

r = 0.5
write(r)
```

## Type conversion

Use `stringify` and `intify` to convert between types explicitly.

`stringify` converts a variable to its string representation:

```lil
x = 42
stringify x
```

`intify` converts a variable to a number. If the string is not numeric, it produces an error:

```lil
x = "42"
intify x
x = x + 5
write(x)

x = "hello"
intify x
```

`intify` can also format a string as binary, hex, or octal byte representation:

```lil
x = "hello"
intify x binary
intify x hex
intify x octal
```

`toggle` toggles a variable between number and string. If its a number, it becomes a string. If its a string, it becomes a number:

```lil
x = 42
stringify x     : now "42"
toggle x        : back to 42
toggle x        : back to "42"
toggle x        : back to 42
```
