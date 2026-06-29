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
print c
```

If you add a string to a number, the number is converted to a string first:

```lil
x = "score: " + 42
print x
```

## String length

Strings have a length you can store as a number:

```lil
name = "alice"
len = 5
print name, "has", len, "characters"
print "length: 5"
```

## Math on numbers

You can write math expressions directly:

```lil
result = 3.14 * 2
print result

r = 0.5
print r
```

## Type conversion

Use `strify` and `intify` to convert between types explicitly.

`strify` converts a variable to its string representation:

```lil
x = 42
strify x
```

`intify` converts a variable to a number. If the string is not numeric, it produces an error:

```lil
x = "42"
intify x
x = x + 5
print x

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

`swify` toggles a variable between number and string. If its a number, it becomes a string. If its a string, it becomes a number:

```lil
x = 42
strify x       # now "42"
swify x        # back to 42
swify x        # back to "42"
swify x        # back to 42
```
