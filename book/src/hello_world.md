# Hello, World

Create a file called `hello.lil`:

```lil
print "hello, world"
```

Run it:

```bash
lil hello.lil
```

In Python you write `print("hello")` with parentheses. In lil, just `print "hello"`. No parens, no commas between the keyword and its arguments.

The `print` statement outputs text to stdout and appends a newline:

```lil
print 42
print "hello"
```

You can print multiple values by separating them with commas:

```lil
print "the answer is", 42
```

This prints: `the answer is 42`

Print with a variable:

```lil
name = "alice"
print "hello", name
```

Print the result of a library function by assigning it first:

```lil
d = &date|minimal
print "today is", d
```

Or call the function directly inside print:

```lil
print "today is", &date|full
```

What you learned: `print` works without parens, commas separate multiple values, and you can store function results in variables or use them directly in print.
