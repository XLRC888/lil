# Templates

Template strings let you embed variable values inside a string using `$varname`.

```lil
name = "alice"
age = 30
print "my name is $name and i am $age years old"
```

This prints: `my name is alice and i am 30 years old`

Templates are parsed at runtime. The `$name` and `$age` are replaced with the current values of those variables.

You can use templates anywhere a string is expected:

```lil
greeting = "hello $name"
print greeting
```

If the variable does not exist, its value defaults to `0`.
