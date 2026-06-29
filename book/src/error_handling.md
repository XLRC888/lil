# Error Handling

lil uses `try`/`catch` for handling runtime errors, similar to Python's `try`/`except`.

## Basic try/catch

Python:

```python
try:
    x = int("hello")
except:
    print("caught an error")
```

lil:

```lil
try {
  x = "hello"
  intify x
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

## File Operation Example

A file might not exist. try/catch handles it gracefully:

```lil
try {
  data = &file|read "maybe.tmp"
  print data
} catch {
  print "could not read file"
}
```

## Input Validation with intify

Use `intify` inside try/catch to convert and validate in one step:

```lil
input "enter a number: " s
try {
  intify s
  print "valid number:", s
} catch {
  print "that was not a number"
}
```

Keep asking until the user enters something valid:

```lil
loop {
  input "enter a number: " s
  ok = 1
  try {
    intify s
  } catch {
    ok = 0
    print "try again"
  }
  if ok == 1 { stop }
}
print "you entered:", s
```

## What Gets Caught

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
