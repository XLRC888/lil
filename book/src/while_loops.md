# While Loops

## Basic while

A while loop runs as long as its condition is truthy. In Python you'd write `while condition:`. In lil it's `while condition {`:

```lil
i = 0
while i < 5 {
  print i
  i = i + 1
}
```

This prints 0 through 4. Same pattern as Python, just braces instead of a colon.

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

This sums 1 + 2 + ... + 100 and prints 5050. Notice how `total` gets updated each iteration.

## Reading input until a sentinel

A common pattern: keep reading user input until they enter a special value:

```lil
lines = ""
while 1 {
  line = &sys|readln
  if line == "done" { break }
  lines = lines + line + "\n"
}
print "you entered:\n$lines"
```

The loop reads lines forever. When the user types "done", `break` exits the loop.

## Random loop condition

Using `&math|random` to create randomness in a loop condition:

```lil
count = 0
total = 0
while &math|random 100 > 10 {
  count = count + 1
  total = total + &math|random 100
}
print "loop ran $count times, total=$total"
```

Each iteration has a 90% chance of continuing (random 0..99 > 10). The loop ends when the random value is 10 or less.

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
