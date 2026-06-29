# User-defined Functions

Define a function with `$name(params)` syntax followed by a block in braces:

```lil
$greet() {
  print "hello"
}

$greet()
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
$add(a, b) {
  a + b
}

result = $add(3, 4)
print result
```

## Practical Examples

Convert Fahrenheit to Celsius:

```lil
$fahr_to_celsius(f) {
  (f - 32) * 5 / 9
}

temp = $fahr_to_celsius(100)
print temp
```

Roll a die using a library function inside a user function:

```lil
$roll_dice() {
  &math|randint 1 6
}

roll = $roll_dice()
print "you rolled", roll
```

Check if a number is even:

```lil
$is_even(n) {
  n % 2 == 0
}

x = $is_even(10)
print x
```

## Scope

Parameters are local to the function call and don't leak to the caller:

```lil
x = 10

$set_x(val) {
  x = val
}

$set_x(5)
print x
```

Non-parameter variables inside a function refer to global variables:

```lil
x = 0

$increment() {
  x = x + 1
}

$increment()
print x
```

## Include

Functions defined in other files can be loaded with `include`:

```lil
include dice

roll = $roll_dice()
print roll
```

This loads and runs `dice.lil`, making its functions available. For example, `dice.lil` might contain:

```lil
$roll_dice() {
  &math|randint 1 6
}
```

Then your main script includes it and uses `$roll_dice` like any local function.

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

print $fib(10)
```

lil's functions work in both interpreted and compiled (AOT) mode.
