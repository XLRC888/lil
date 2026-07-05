# Templates

Template strings let you embed variable values inside a string using `{varname}` with backtick strings. In Python this would be an f-string like `f"hello {name}"`. In lil it's `` `hello {name}` ``:

```lil
name = "alice"
age = 30
write(`my name is {name} and i am {age} years old`)
```

This prints `my name is alice and i am 30 years old`.

## Template vs Concatenation

Without templates:

```lil
greeting = "hello " + name + " you are " + age + " years old"
```

With templates:

```lil
greeting = `hello {name} you are {age} years old`
```

Templates are cleaner for anything more than a single concatenation.

## Practical example

```lil
username = "bob"
score = 95
msg = `player {username} scored {score} points`
write(msg)
```

## Mixing with library calls

```lil
now = minimal@date
write(`the time is {now}`)
```

```lil
rand = random@math()
write(`your lucky number is {rand}`)
```

## Templates in conditions

```lil
name = read@file("name.txt")
if len@string(name) > 0 {
  write(`welcome, {name}`)
}
```

## What happens with undefined variables

If you use a variable name that doesn't exist in a template, it evaluates to 0 (for numbers) or "" (for strings):

```lil
write(`value is {undefined_var}`)   : prints "value is 0"
```
