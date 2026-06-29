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

## Using library functions with strings

The `&string|len` function returns the length of a string. In Python you call `len(x)`, in lil you call `&string|len $x` (the `$` prefix tells lil to use the variable's value):

```lil
name = "alice"
len = &string|len $name
print name, "has", len, "characters"
```

You can pass a literal string too:

```lil
print &string|len "hello"
```

Other string functions work similarly:

```lil
msg = "hello world"
print &string|upper $msg
print &string|lower $msg
print &string|reverse $msg
```

## Using library functions with numbers

The `&math|eval` function evaluates a math expression string and returns a number. Store the result in a variable:

```lil
result = &math|eval "3.14 * 2"
print result
```

`&math|random` returns a random number between 0 and 1:

```lil
r = &math|random
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
