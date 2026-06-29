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

In Python, only `break` exists. In lil, `stop` and `break` are interchangeable, they both exit the innermost enclosing loop immediately. You can use them inside `loop`, `while`, and `for` blocks.

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

## Event loop with a flag

A common pattern: run an event loop that stops when a flag is set:

```lil
running = 1
loop {
  cmd = &sys|readln
  if cmd == "quit" { running = 0 }
  if cmd == "help" { print "commands: quit, help" }
  if not running { stop }
}
```

The flag `running` starts as 1 (true). When the user types "quit", the flag goes to 0 and the loop exits on the next iteration.

## Repeating timer with sleep

Using `&math|sleep` to run something every 2 seconds:

```lil
count = 0
loop {
  print "tick $count"
  count = count + 1
  &math|sleep 2000
  if count >= 5 { break }
}
```

Sleep takes milliseconds. The loop ticks every 2 seconds for 5 iterations. Store the count and display it through a template.
