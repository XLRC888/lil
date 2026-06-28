# Error Handling

lil has a `try`/`catch` mechanism for handling runtime errors without crashing.

## Basic try/catch

Wrap code that might fail in a `try` block, and handle the error in a `catch` block:

```lil
try {
  x = "hello"
  intify x
  print "this line never runs"
} catch {
  print "caught an error"
}
print "execution continues here"
```

If no error occurs, the `catch` block is skipped:

```lil
try {
  x = "42"
  intify x
  print "this runs fine"
} catch {
  print "this never runs"
}
```

## What gets caught

Any error inside the `try` block triggers the `catch` block:

- Failed type conversion (`intify` on a non-numeric string)
- Division by zero
- Variable limit exceeded

The error message is still printed to stderr, but execution continues from the `catch` block instead of stopping the program.

## Nesting

Try/catch blocks can be nested:

```lil
try {
  try {
    intify "oops"
  } catch {
    print "inner catch"
  }
} catch {
  print "outer catch never runs"
}
```

Errors inside a `catch` block are not caught by the same `try`. They propagate to the enclosing `try` or stop the program if there is none.
