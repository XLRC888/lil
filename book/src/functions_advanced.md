# Functions with Parameters

Functions can take parameters and return values. This page covers practical patterns, library integration, and advanced behavior.

## Defining a Function

Use `$name(params)` syntax followed by the function body in braces:

```lil
$add(a, b) {
  a + b
}
```

## Calling a Function

```lil
result = $add(3, 4)
print result
```

## Practical Predicate Functions

Functions that return 0 or 1 as boolean results:

```lil
$is_even(n) {
  n % 2 == 0
}

$is_odd(n) {
  not $is_even(n)
}

print $is_even(42)
print $is_odd(42)
```

## Using Library Functions Inside User Functions

Built-in library functions work inside your user-defined functions:

```lil
$random_in_range(min, max) {
  &math|randint $min $max
}

r = $random_in_range(5, 10)
print r
```

The `$min` and `$max` prefix tells lil to use the parameter's value instead of the literal string "min".

```lil
$rand_name() {
  &math|choice "alice" "bob" "charlie"
}

$greet() {
  name = $rand_name()
  print "hello", name
}

$greet()
```

```lil
$slugify(s) {
  lower = &string|lower $s
  &string|replace $lower " " "-"
}

slug = $slugify("Hello World")
print slug
```

## Return Values

The last expression in the function body is the return value:

```lil
$double(x) {
  x * 2
}

print $double(21)
```

If a function ends with an `if` expression, the taken branch's last value is returned:

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

print $sign(-5)
```

If there's no value-producing expression, it returns 0:

```lil
$noop() {
  print "no return"
}

x = $noop()
print x
```

## Variable Save/Restore

When you call a function, lil saves all current variables before running the function body and restores them after. Parameters and any variables changed inside stay contained:

```lil
x = "hello"

$test() {
  x = "world"
  print x
}

$test()
print x
```

Parameters are always save/restored independently of global variables:

```lil
x = 100

$f(x) {
  x = x + 1
  print x
}

$f(5)
print x
```

## Recursion

```lil
$fib(n) {
  if n <= 2 {
    1
  } else {
    $fib(n - 1) + $fib(n - 2)
  }
}

print $fib(10)
```

## Notes

Function calls work in both interpreted and compiled (AOT) mode. In compiled mode, each function becomes a standalone C function with its own type inference pass.
