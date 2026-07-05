# Loop and Break

## Loop

`loop` creates an infinite loop with no condition. Use `break` to exit:

```lil
loop {
  if condition { break }
}
```

This is equivalent to `while 1` but reads more naturally for "do until done" patterns.

## Break

`break` exits a loop. It works in `loop`, `while`, and `for` blocks.

```lil
i = 0
loop {
  write(i)
  i = i + 1
  if i > 5 { break }
}
```

`stop` is kept as an alias for backward compatibility, but `break` is the preferred keyword.

## Practical example

A common pattern: run an event loop that breaks when a flag is set:

```lil
running = 1
loop {
  cmd = read()
  if cmd == "quit" { running = 0 }
  if cmd == "help" { write("commands: quit, help") }
  if not running { break }
}
```

The flag `running` starts as 1 (true). When the user types "quit", the flag goes to 0 and the loop exits on the next iteration.

## Repeating timer with sleep

Using `sleep@math` to run something every 2 seconds:

```lil
count = 0
loop {
  write("tick", count)
  count = count + 1
  sleep@math(2)
  if count >= 5 { break }
}
```

Sleep takes seconds. The loop ticks every 2 seconds for 5 iterations.
