# User-defined Functions

Define a function with `$name(params)` syntax:

```lil
$greet() {
  print "hello"
}

$greet()
```

Functions can take parameters and return the last expression's value:

```lil
$add(a, b) {
  a + b
}

result = $add(3, 4)
print result    # prints 7
```

Parameters are local to the function call — they don't leak to the caller:

```lil
x = 10

$set_x(val) {
  x = val
}

$set_x(5)
print x         # still prints 10 outside
```

Functions can modify non-parameter variables (which are global):

```lil
x = 0

$increment() {
  x = x + 1
}

$increment()
print x    # prints 1
```

Functions defined in included libraries can be called from other files using the `include` statement:

```lil
include mylib
```

This loads and runs the file `mylib.lil`, which can define functions and variables that the including file can use.

Functions support recursion and work in both interpreted and compiled (AOT) mode.
