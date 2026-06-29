# Templates

Template strings let you embed variable values inside a string using `$varname`. In Python this would be an f-string like `f"hello {name}"`. In lil it's `"hello $name"`:

```lil
name = "alice"
age = 30
print "my name is $name and i am $age years old"
```

This prints: `my name is alice and i am 30 years old`

## Storing template results

Templates work anywhere a string is expected, including variable assignment:

```lil
greeting = "hello $name"
print greeting
```

```lil
username = "bob"
score = 95
msg = "player $username scored $score points"
print msg
```

## Templates with function results

Variables set from library functions work in templates too:

```lil
now = &date|minimal
print "the time is $now"
```

```lil
rand = &math|random 100
print "your lucky number is $rand"
```

## Templates in conditions

```lil
name = &sys|readln
if &string|len name > 0 {
  print "welcome, $name"
}
```

## If a variable doesn't exist

If the variable does not exist, its value defaults to `0`.

```lil
print "value is $undefined_var"   # prints "value is 0"
```
