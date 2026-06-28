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

If the user does not enter anything (EOF), the variable is set to an empty string.
