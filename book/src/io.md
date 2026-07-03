# Input and Output

lil keeps I/O simpler than Python. No file objects, no context managers, no close() calls. Just print, input, and a few file library functions.

## Print

The `print` statement outputs text:

```lil
print "hello world"
```

Print multiple values separated by commas:

```lil
name = "alice"
print "hello", name
```

```lil
x = 42
print "the answer is", x
```

## Input

The `input` statement reads a line from stdin and stores it in a variable:

```lil
input name
print "hello", name
```

Show a prompt string:

```lil
input "enter your name: " name
print "hello", name
```

In Python, you'd write `name = input("enter your name: ")`. In lil, `input` is a statement, not a function call. The variable comes after the optional prompt.

```lil
input "type something: " s
len = len@string s
print "you typed", len, "characters"
```

## File I/O

lil uses the `file` library for file operations:

```lil
data = read@file "config.txt"
```

In Python that's `with open("config.txt") as f: data = f.read()`. lil does it in one call.

```lil
text = read@file "notes.txt"
if text == "" {
  print "file is empty or missing"
}
```

Writing a file:

```lil
write@file "output.txt" "hello world"
```

You can write the result of an expression directly:

```lil
today = standart@date
write@file "date.txt" today
```

Appending to a file:

```lil
append@file "log.txt" "new entry"
```

Check if a file exists:

```lil
if exists@file "data.txt" == 1 {
  data = read@file "data.txt"
  print data
}
```

List directory contents:

```lil
files = list@file "/home/user"
print files
```

See the [Built-in Libraries](builtins.md) page for the full file library reference.
