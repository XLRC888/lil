# User-defined Functions

Define a function with `$name()` syntax:

```lil
$greet() {
  print "hello"
}

$greet()
```

The function body runs when you call the name with `()`.

Functions can modify global variables:

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

Functions currently do not have local scope or parameters. They run in the global scope and can access and modify all variables.
