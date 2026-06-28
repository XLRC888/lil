# Loop, Stop, and Break

## The loop block

`loop` creates an infinite block that must be exited with `stop` or `break`:

```lil
i = 0
loop {
  i = i + 1
  if i > 3 { stop }
  print i
}
print "done"
```

This prints 1, 2, 3 and then "done".

## Stop vs Break

`stop` and `break` do the same thing: they exit the innermost enclosing loop immediately. You can use them inside `loop`, `while`, and `for` blocks.

```lil
while 1 {
  break    # exits the while loop
}

loop {
  stop     # exits the loop block
}

for i = 1 to 10 {
  break    # exits the for loop early
}
```

Using `stop` or `break` outside a loop is an error.
