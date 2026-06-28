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
