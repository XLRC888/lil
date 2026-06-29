# For Loops

The `for` loop iterates over a range of numbers. In Python you'd write `for i in range(1, 6):`. In lil it's `for i = 1 to 5 {`:

```lil
for i = 1 to 5 {
  print i
}
```

This prints 1 through 5. The loop variable (`i`) starts at the first value and increments by 1 each iteration until it exceeds the second value.

Python's `range(1, 6)` includes 1 but excludes 6. lil's `1 to 5` includes both ends.

## Using the loop variable in calculations

```lil
for i = 1 to 10 {
  print i * i
}
```

Prints 1, 4, 9, 16, 25, 36, 49, 64, 81, 100. The loop variable is just a regular variable you can use in any expression.

Store results in another variable:

```lil
squares = 0
for i = 1 to 10 {
  squares = squares + i * i
}
print squares
```

This sums the squares of 1 through 10.

## Expressions for range bounds

You can use expressions for the start and end values:

```lil
start = 2
end = 10
for i = start to end {
  print i
}
```

## No counting down

lil's `for` only counts up. To count down, use a while loop:

```lil
i = 10
while i >= 1 {
  print i
  i = i - 1
}
```

## Nested for loops: multiplication table

```lil
for row = 1 to 5 {
  line = ""
  for col = 1 to 5 {
    line = line + " " + row * col
  }
  print line
}
```

Prints a 5x5 multiplication table. Each row builds up a string as the inner loop runs.

## Break in for loops

```lil
for i = 1 to 100 {
  if i > 3 { break }
  print i
}
```

This prints 1, 2, 3.
