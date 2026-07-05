# Read (Input)

The `read()` function reads a line from stdin and returns it as a string.

```lil
name = read()
write("hello", name)
```

You can provide a prompt string:

```lil
name = read("what is your name? ")
write("nice to meet you,", name)
```

If the user does not enter anything (EOF), the result is an empty string:

```lil
x = read("press enter to continue")
write("ok, continuing")
```

Force input as a number and do a calculation:

```lil
age = read("age: ")
intify age
write("next year youll be", age + 1)
```

Read a number input and use it in an expression directly after:

```lil
n = read("enter a number: ")
intify n
result = n * 2
write("doubled:", result)
```

## Legacy syntax

The old `input` statement still works but `read()` is preferred:

```lil
input name
input "what is your name? " name
```
