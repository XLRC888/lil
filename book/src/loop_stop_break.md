# Loop, Stop, and Break

## Loop

`loop` creates an infinite loop with no condition. Use `stop` or `break` to exit:

```lil
loop {
  if condition { stop }
}
```

This is equivalent to `while 1` but reads more naturally for "do until done" patterns.

## Stop vs Break

`stop` and `break` both exit a loop. They work in `loop`, `while`, and `for` blocks. There is no difference between them.

```lil
i = 0
loop {
  print i
  i = i + 1
  if i > 5 { stop }
}
```

## Practical example

A common pattern: run an event loop that stops when a flag is set:

```lil
running = 1
loop {
  input cmd
  if cmd == "quit" { running = 0 }
  if cmd == "help" { print "commands: quit, help" }
  if not running { stop }
}
```

The flag `running` starts as 1 (true). When the user types "quit", the flag goes to 0 and the loop exits on the next iteration.

## Repeating timer with sleep

Using `sleep@math` to run something every 2 seconds:

```lil
count = 0
loop {
  print "tick", count
  count = count + 1
  sleep@math 2
  if count >= 5 { break }
}
```

Sleep takes seconds. The loop ticks every 2 seconds for 5 iterations.
