# Variables

Variables hold values. You assign them with `=`:

```lil
x = 10
name = "alice"
```

Variable names must start with a letter and can contain letters, digits, and underscores.

Variables do not need to be declared before use. The first time you assign to a name, the variable is created. If you use a variable that has not been assigned yet, its value defaults to `0`:

```lil
print z
```

In Python, an undefined variable raises `NameError`. In lil, you get `0`. This is useful for counters and accumulators that start at zero implicitly.

Counting up works naturally:

```lil
count = 0
count = count + 1
count = count + 1
print count
```

You can reassign variables to values of any type:

```lil
x = 42
x = "now im a string"
x = 99
```

Assignment always goes `variable = expression`:

```lil
y = x + 50
```

Assign a number to a variable:

```lil
n = 10
print "number:", n
```

Store the result of a math expression:

```lil
area = 3.14 * 5 * 5
print "area is", area
```

## Forcing variable types

`force` locks a variable completely. Once forced, you cannot change it with `strify`, `intify`, `swify`, or regular assignment:

```lil
x = 42
force x
x = 30        # ERROR: cannot assign to a forced variable
strify x      # ERROR: cannot strify a forced variable
unforce x     # unlock it
x = 30        # works again
```

Combined assignment and force:

```lil
force x = 42
unforce x = 56
```

`unforce` unlocks the variable so it can be changed again.

`force int input` reads input, validates it as a number, and forces the variable:

```lil
force int input "Enter your age: " age
print "next year you will be", age + 1
```

`force str input` reads input as a string and forces it:

```lil
force str input "Enter your name: " name
print "hello", name
```
