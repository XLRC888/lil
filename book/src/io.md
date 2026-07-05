# Input and Output

lil keeps I/O simpler than Python. No file objects, no context managers, no close() calls. Just read, write, and a few file library functions.

## Write

The `write()` function outputs text:

```lil
write("hello world")
```

Write multiple values separated by commas:

```lil
name = "alice"
write("hello", name)
```

```lil
x = 42
write("the answer is", x)
```

## Read

The `read()` function reads a line from stdin and returns it as a string:

```lil
name = read()
write("hello", name)
```

Show a prompt string:

```lil
name = read("enter your name: ")
write("hello", name)
```

In Python, you'd write `name = input("enter your name: ")`. In lil, `read()` is a function that returns a string.

```lil
s = read("type something: ")
len = len@string(s)
write("you typed", len, "characters")
```

## File I/O

lil uses the `file` library for file operations:

```lil
data = read@file("config.txt")
```

In Python that's `with open("config.txt") as f: data = f.read()`. lil does it in one call.

```lil
text = read@file("notes.txt")
if text == "" {
  write("file is empty or missing")
}
```

Writing a file:

```lil
write@file("output.txt", "hello world")
```

You can write the result of an expression directly:

```lil
today = standart@date
write@file("date.txt", today)
```

Appending to a file:

```lil
append@file("log.txt", "new entry")
```

Check if a file exists:

```lil
if exists@file("data.txt") == 1 {
  data = read@file("data.txt")
  write(data)
}
```

List directory contents:

```lil
files = list@file("/home/user")
write(files)
```

See the [Built-in Libraries](builtins.md) page for the full file library reference.
