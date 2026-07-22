# Channel Concurrency

lil supports Go-style channel concurrency with `channel`, `send`, `recv`, `spawn`, and `wait`.

## Creating a Channel

```lil
ch = channel 3
```

Creates a buffered channel with capacity 3. Unbuffered channels use `channel 0`.

## Sending and Receiving

```lil
ch = channel 3
send ch "hello"
msg = recv ch
write(msg)  : hello
```

## Spawning Threads

Use `spawn` to run code in a new thread:

```lil
ch = channel 1
spawn {
    send ch "from another thread"
}
msg = recv ch
write(msg)  : from another thread
```

## Waiting for Completion

Use `wait` to block until all spawned threads finish:

```lil
spawn {
    write("working...")
}
wait
write("done")
```

## Producer-Consumer Pattern

```lil
ch = channel 5
spawn {
    i = 0
    while i < 10 {
        send ch i
        i = i + 1
    }
}
loop {
    msg = recv ch
    write("got: ", msg)
    if msg == 9 { break }
}
wait
```

## Channel Capacity

Buffered channels block when full. Unbuffered channels (`channel 0`) block until the receiver is ready:

```lil
ch = channel 1
spawn {
    send ch "a"  : succeeds immediately
    send ch "b"  : blocks until received
}
write(recv ch)  : a
write(recv ch)  : b
wait
```

## Notes

- Channels are heap-allocated and shared between threads
- `recv` returns 0 if the channel is closed (no senders)
- `wait` blocks until all spawned threads complete
- Channels work with all value types (numbers, strings, booleans, lists, dicts)
