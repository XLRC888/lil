# While Loops

## Basic while

```lil
i = 0
while i < 5 {
  print i
  i = i + 1
}
```

This prints 0 through 4. The loop continues as long as the condition is truthy.

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
