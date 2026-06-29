# Undefined Default

By default, reading an undefined variable returns `0`. Use `?undef = value` to change this:

```lil
?undef = "hello"
print x       # prints "hello"

?undef = 99
print y       # prints 99
```

The name after `?` is descriptive and ignored. Only one global default exists. Once set, every undefined variable will return that value until you change it again.
