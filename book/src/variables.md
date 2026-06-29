# Variables

Variables hold values. You assign them with `=`:

```lil
x = 10
name = "alice"
```

Variable names must start with a letter and can contain letters, digits, and underscores.

Variables do not need to be declared before use. The first time you assign to a name, the variable is created. If you use a variable that has not been assigned yet, its value defaults to `0`:

```lil
print z   # prints 0 (z was never assigned)
```

You can reassign variables to values of any type:

```lil
x = 42
x = "now im a string"
```

This is fine. lil does not enforce type consistency.

Assignment always goes `variable = expression`:

```lil
y = x + 50
```

## Forcing variable types

You can lock a variable to its current type with `force`. This prevents `strify` and `intify` from converting it:

```lil
x = 42
force x           # x is now locked to numeric type
strify x          # ERROR: cannot strify a forced variable
unforce x         # unlock
strify x          # works now
print x           # prints "42"
```

Combined assignment and force:

```lil
force x = 42      # assign and lock in one step
unforce x = 56    # unlock and assign in one step
```

`force int input` reads input, validates it as a number, and forces the variable:

```lil
force int input "Enter your age: " age
```

`force str input` reads input as a string and forces it:

```lil
force str input "Enter your name: " name
```
