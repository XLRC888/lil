# Functions with Parameters

Functions can take parameters and return values.

## Defining a function

Use `$name(params)` syntax followed by the function body in braces:

```lil
$add(a, b) {
  a + b
}
```

## Calling a function

Call a function with `$name(args)`:

```lil
result = $add(3, 4)
print result    # prints 7
```

Parameters are passed by value. Inside the function, parameters act as local variables that don't leak to the caller's scope.

## Return values

The last expression in the function body is the return value. There is no `return` keyword:

```lil
$double(x) {
  x * 2
}

print $double(21)   # prints 42
```

If the function ends with an `if` expression, the taken branch's last expression is returned:

```lil
$sign(n) {
  if n > 0 {
    1
  } elif n < 0 {
    -1
  } else {
    0
  }
}

print $sign(-5)   # prints -1
```

If a function doesn't end with a value-producing expression, it returns 0:

```lil
$noop() {
  print "no return"
}

x = $noop()
print x             # prints 0
```

## Recursion

Functions can call themselves:

```lil
$fib(n) {
  if n <= 2 {
    1
  } else {
    $fib(n - 1) + $fib(n - 2)
  }
}

print $fib(10)      # prints 55
```

## Notes

Function calls work in both interpreted and compiled (AOT) mode. In compiled mode, each function becomes a standalone C function with its own type inference pass.
