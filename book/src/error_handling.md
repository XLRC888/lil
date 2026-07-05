# Error Handling

## Try/Catch

Errors are caught with `try/catch`:

```lil
try {
  risky_stuff()
} catch {
  write("something went wrong")
}
```

If the code inside `try` triggers an error, execution jumps to `catch` immediately.

## Practical Example

Reading a file that might not exist:

```lil
data = ""
try {
  data = read@file("maybe.tmp")
} catch {
  write("could not read file")
}
write(data)
```

## What gets caught

Runtime errors, type errors, out-of-bounds access, division by zero, and undefined function calls are all caught. Parse errors happen before the code runs and can't be caught.

The script continues after `catch` finishes.
