# Continue

`continue` skips to the next iteration of a loop. It works in `while`, `for`, and `loop` blocks.

```lil
i = 0
while i < 10 {
  i = i + 1
  if i % 2 == 0 { continue }
  write(i)
}
```

This prints odd numbers 1, 3, 5, 7, 9. When `i` is even, `continue` jumps back to the condition check, skipping the print.

`continue` only skips the rest of the current iteration, it does not exit the loop. For that, use `break`.
