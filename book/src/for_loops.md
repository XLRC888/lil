# For Loops

The `for` loop iterates over a range of numbers:

```lil
for i = 1 to 5 {
  print i
}
```

This prints 1 through 5. The loop variable (`i`) starts at the first value and increments by 1 each iteration until it exceeds the second value.

You can use expressions for the start and end values:

```lil
start = 2
end = 10
for i = start to end {
  print i
}
```

`break` works inside for loops:

```lil
for i = 1 to 100 {
  if i > 3 { break }
  print i
}
```

This prints 1, 2, 3.
