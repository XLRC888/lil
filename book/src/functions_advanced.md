# Functions with Parameters

Functions can take parameters and return values.

## Defining a function

Use `$name(params)` syntax followed by the function body in braces:

```lil
$add(a, b) {
  return a + b
}
```

## Calling a function

Call a function with `$name(args)`:

```lil
result = $add(3, 4)
print result    # prints 7
```

Parameters are passed by value. Inside the function, parameters act as local variables that shadow any global variables with the same name.

## Return values

Use `return` to send a value back to the caller:

```lil
$double(x) {
  return x * 2
}

print $double(21)   # prints 42
```

If a function does not have a `return` statement, it returns 0.

```lil
$noop() {
  print "no return"
}

x = $noop()
print x             # prints 0
```

## Notes

Parameters and return values currently only work in interpreted mode. Compiled mode does not yet support function calls with return values.
