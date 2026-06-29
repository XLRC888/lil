# Basics

lil is dynamically typed, just like Python. A variable can hold a number, a string, or anything else, and you can change its type whenever you want.

There is no `def`, no `class`, and no `return` keyword. Functions are defined with `$name` syntax and the last expression in the body is automatically returned.

All variables are global by default. There is no local scope (except function parameters, which are saved and restored automatically around calls).

Newlines separate statements. Semicolons are optional but you don't need them. Just write one thing per line.

Here is "hello world" in Python compared to lil:

```python
# Python
print("hello, world")
```

```lil
print "hello, world"
```

Here is a variable, a function call, and a print in lil:

```lil
name = "alice"
greeting = "hello, " + name
print greeting
```

Variables hold values. You assign with `=` and read by just writing the name:

```lil
x = 10
y = x + 5
print y
```

If a variable is used before being assigned, it defaults to `0` instead of raising an error. This is different from Python, where `NameError` would fire.
