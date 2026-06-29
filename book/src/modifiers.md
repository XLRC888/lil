# Modifiers

Modifiers are keywords that change how variables or operations behave.

## Force

`force` locks a variable so it cannot be changed:

```lil
x = 42
force x
x = 30        # ERROR: cannot assign to a forced variable
strify x      # ERROR: cannot strify a forced variable
unforce x
x = 30        # works again
```

Combined assignment and force:

```lil
force x = 42
unforce x = 56
```

## Undefined Default

By default, reading an undefined variable returns `0`. Use `?name = value` to change this:

```lil
?undef = "hello"
print x       # prints "hello"

?undef = 99
print y       # prints 99
```

The name after `?` is descriptive and ignored. Only one global default exists.
