# Continue

The `continue` keyword skips the rest of the current loop iteration and jumps to the next one.

```lil
i = 0
while i < 10 {
  i = i + 1
  if i == 5 { continue }
  print i
}
```

This prints 1, 2, 3, 4, 6, 7, 8, 9, 10. The number 5 is skipped because `continue` jumps back to the loop condition without printing.

`continue` works in `while`, `for`, and `loop` blocks.
