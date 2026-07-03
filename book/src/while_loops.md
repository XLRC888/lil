# While Loops

## Basic while

A while loop runs as long as its condition is truthy:

```lil
i = 0
while i < 5 {
  print i
  i = i + 1
}
```

This prints 0 through 4.

## Sum of numbers 1 to 100

Accumulate a result across loop iterations:

```lil
total = 0
i = 1
while i <= 100 {
  total = total + i
  i = i + 1
}
print total
```

This sums 1 + 2 + ... + 100 and prints 5050.

## Random loop condition

Using `random@math` to create randomness in a loop condition:

```lil
count = 0
total = 0
while random@math * 100 > 10 {
  count = count + 1
  total = total + random@math * 100
}
print "loop ran", count, "times, total=", total
```

Each iteration has a 90% chance of continuing.

## Infinite loop with break

```lil
i = 0
while 1 {
  if i > 5 { break }
  print i
  i = i + 1
}
```

The `break` keyword exits the loop immediately. It works in `while`, `for`, and `loop` blocks.

## Nested loops

```lil
i = 1
while i <= 3 {
  j = 1
  while j <= 3 {
    print i, j
    j = j + 1
  }
  i = i + 1
}
```
