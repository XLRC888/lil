# User-defined Functions

Define a function with `#name(params)` syntax followed by a block in braces:

```lil
#greet() {
  print "hello"
}

greet()
```

## Parameters and Return Values

Functions take parameters and return the last expression's value. There's no `return` keyword.

Python:

```python
def add(a, b):
    return a + b
```

lil:

```lil
#add(a, b) {
  a + b
}

result = add(3, 4)
print result
```

## Practical Examples

Convert Fahrenheit to Celsius:

```lil
#fahr_to_celsius(f) {
  (f - 32) * 5 / 9
}

temp = fahr_to_celsius(100)
print temp
```

Roll a die using a library function inside a user function:

```lil
#roll_dice() {
  randint@math 1 6
}

roll = roll_dice()
print "you rolled", roll
```

Check if a number is even:

```lil
#is_even(n) {
  n % 2 == 0
}

x = is_even(10)
print x
```

## Scope

Each function call gets its own scope. Parameters and local variables are isolated to that call and don't leak:

```lil
#a(x) {
  x = x + 10
  x
}

#b(x) {
  x = x * 2
  x
}

print a(5)    # 15
print b(5)    # 10
print a(100)  # 110
```

Two functions with the same parameter name don't interfere with each other:

```lil
#a(x) {
  print x
}

#b(x) {
  a(x + 1)
  print x
}

b(100)
```

Variables created inside a function are local to that call and cleaned up when the function returns:

```lil
#outer() {
  x = 99
  inner()
  print x
}

#inner() {
  x = 42
}

outer()
```

Variables inside a function create new bindings in the function's scope. They don't modify outer-scope variables of the same name:

```lil
x = 10

#set_x(val) {
  x = val
}

set_x(42)
print x
```

## Include

Functions defined in other files can be loaded with `include`:

```lil
include dice

roll = roll_dice()
print roll
```

This loads and runs `dice.lil`, making its functions available. For example, `dice.lil` might contain:

```lil
#roll_dice() {
  randint@math 1 6
}
```

Then your main script includes it and uses `roll_dice` like any local function.

**Selective import**: specify which functions to import by appending `/` and function names:

```lil
include dice.lil/roll_dice flip_coin
```

Only `roll_dice` and `flip_coin` are registered from `dice.lil`. Other functions are discarded. The file runs in an isolated variable scope.

## Recursion

Functions can call themselves:

```lil
#fib(n) {
  if n <= 2 {
    1
  } else {
    fib(n - 1) + fib(n - 2)
  }
}

print fib(10)
```

lil's functions work in both interpreted and compiled (AOT) mode.

## Anonymous Functions (Closures)

Functions without a name can be created inline with `#(params) { body }`:

```lil
double = #(x) { x * 2 }
print double(5)
```

The real power is passing them to higher-order library functions:

```lil
include list

nums = new@list
push@list nums 1
push@list nums 2
push@list nums 3

doubled = map@list nums #(x) { x * 2 }
print doubled
```

Anonymous functions see variables from the scope where they're defined:

```lil
prefix = "> "
#add_prefix(s) { prefix + s }
print add_prefix("hello")
```

The anonymous function creates an internal name, registers in the function table, and returns the name as a string. `map@list` and `filter@list` resolve it automatically.

