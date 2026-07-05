# Functions with Parameters

Functions can take parameters and return values. This page covers practical patterns, library integration, and advanced behavior.

## Defining a Function

Use `#name(params)` syntax followed by the function body in braces:

```lil
#add(a, b) {
  a + b
}
```

## Calling a Function

```lil
result = add(3, 4)
write(result)
```

## Practical Predicate Functions

Functions that return 0 or 1 as boolean results:

```lil
#is_even(n) {
  n % 2 == 0
}

#is_odd(n) {
  not is_even(n)
}

write(is_even(42))
write(is_odd(42))
```

## Using Library Functions Inside User Functions

Built-in library functions work inside your user-defined functions:

```lil
#random_in_range(min, max) {
  randint@math(min, max)
}

r = random_in_range(5, 10)
write(r)
```

Bare variable names are auto-dereferenced in library calls:

```lil
#rand_name() {
  choice@math("alice", "bob", "charlie")
}

#greet() {
  name = rand_name()
  write("hello", name)
}

greet()
```

```lil
#slugify(s) {
  lower = lower@string(s)
  replace@string(lower, " ", "-")
}

slug = slugify("Hello World")
write(slug)
```

## Return Values

The last expression in the function body is the return value:

```lil
#double(x) {
  x * 2
}

write(double(21))
```

If a function ends with an `if` expression, the taken branch's last value is returned:

```lil
#sign(n) {
  if n > 0 {
    1
  } orif n < 0 {
    -1
  } else {
    0
  }
}

write(sign(-5))
```

If there's no value-producing expression, it returns 0:

```lil
#noop() {
  write("no return")
}

x = noop()
write(x)
```

## Variable Save/Restore

When you call a function, lil saves all current variables before running the function body and restores them after. Parameters and any variables changed inside stay contained:

```lil
x = "hello"

#test() {
  x = "world"
  write(x)
}

test()
write(x)
```

Parameters are always save/restored independently of global variables:

```lil
x = 100

#f(x) {
  x = x + 1
  write(x)
}

f(5)
write(x)
```

## Recursion

```lil
#fib(n) {
  if n <= 2 {
    1
  } else {
    fib(n - 1) + fib(n - 2)
  }
}

write(fib(10))
```

## Notes

Function calls work in both interpreted and compiled (AOT) mode. In compiled mode, each function becomes a standalone C function with its own type inference pass.
