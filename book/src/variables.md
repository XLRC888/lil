# Variables

Variables hold values. You assign them with `=`:

```lil
x = 10
name = "alice"
```

Variable names must start with a letter and can contain letters, digits, and underscores.

Variables do not need to be declared before use. The first time you assign to a name, the variable is created. Use `%name` to explicitly declare a variable with value 0:

```lil
%counter
write(counter)     : 0
counter = 10
write(counter)     : 10
```

If you use a variable that has not been assigned yet, its value defaults to `0`:

```lil
write(z)
```

In Python, an undefined variable raises `NameError`. In lil, you get `0`. This is useful for counters and accumulators that start at zero implicitly.

Counting up works naturally:

```lil
count = 0
count = count + 1
count = count + 1
write(count)
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
write("number:", n)
```

Store the result of a math expression:

```lil
area = 3.14 * 5 * 5
write("area is", area)
```

## Forcing variable types

`force` locks a variable completely. Once forced, you cannot change it with `stringify`, `intify`, `toggle`, or regular assignment:

```lil
x = 42
force x
x = 30        : ERROR: cannot assign to a forced variable
stringify x   : ERROR: cannot stringify a forced variable
unforce x     : unlock it
x = 30        : works again
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
write("next year you will be", age + 1)
```

`force str input` reads input as a string and forces it:

```lil
force str input "Enter your name: " name
write("hello", name)
```

### Input validation without force

Use `int input` or `str input` to validate the input type without forcing the variable:

```lil
int input "Enter a number: " x
x = "now im a string"     : allowed (not forced)
str input "Enter text: " y
y = 42                     : allowed (not forced)
```

`int input` errors if the input is not a valid number. `str input` accepts anything as a string.

## Changing the undefined default

By default, reading an undefined variable returns `0`. Use `?default = value` to change this:

```lil
?default = "Undefined"
write(someVar)     : prints "Undefined"

?default = 0
write(anotherVar)  : prints 0
```

The name after `?` is descriptive only and is ignored by the runtime. Only one value is stored globally. Once set, all undefined variables will return that value until changed.
