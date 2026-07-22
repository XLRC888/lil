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

## Catch Variable

You can optionally name the error to get a stack trace:

```lil
try {
  risky_stuff()
} catch e {
  write("error on line ", e.line)
  write("message: ", e.msg)
}
```

The error object has two fields:
- `e.line` - the line number where the error occurred
- `e.msg` - the error message string

## Practical Example

Reading a file that might not exist:

```lil
data = ""
try {
  data = read@file("maybe.tmp")
} catch e {
  write("could not read file: ", e.msg)
}
write(data)
```

## Division by Zero

```lil
try {
  x = 1 / 0
} catch e {
  write("math error: ", e.msg)
}
```

## Type Conversion Errors

```lil
try {
  x = "hello"
  intify x
} catch e {
  write("conversion failed: ", e.msg)
}
```

## What gets caught

Runtime errors, type errors, out-of-bounds access, division by zero, undefined variable access, and undefined function calls are all caught. Parse errors happen before the code runs and can't be caught.

The script continues after `catch` finishes.
