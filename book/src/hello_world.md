# Hello, World

Create a file called `hello.lil`:

```lil
write("hello, world")
```

Run it:

```bash
lil hello.lil
```

In Python you write `print("hello")` with parentheses. In lil, `write("hello")` is a function call with parentheses.

The `write()` function outputs text to stdout and appends a newline:

```lil
write(42)
write("hello")
```

You can write multiple values by separating them with commas:

```lil
write("the answer is", 42)
```

This prints: `the answer is 42`

Write with a variable:

```lil
name = "alice"
write("hello", name)
```
