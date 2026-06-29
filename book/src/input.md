# Input

The `input` statement reads a line from stdin and stores it in a variable as a string.

```lil
input name
print "hello", name
```

You can provide a prompt string:

```lil
input "what is your name? " name
print "nice to meet you,", name
```

If the user does not enter anything (EOF), the variable is set to an empty string:

```lil
input "press enter to continue" x
print "ok, continuing"
```

Force input as a number and do a calculation:

```lil
force int input "age: " age
print "next year youll be", age + 1
```

Read a number input and use it in an expression directly after:

```lil
force int input "enter a number: " n
result = n * 2
print "doubled:", result
```
