# Undefined Default

By default, reading an undefined variable returns `0`. Use `?default = value` to change this:

```lil
?default = "hello"
write(x)       : prints "hello"

?default = 99
write(y)       : prints 99
```

The name after `?` is descriptive only and is ignored by the runtime. Only one global default exists. Once set, every undefined variable will return that value until you change it again.
