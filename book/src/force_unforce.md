# Force and Unforce

The `force` and `unforce` commands lock a variable to its current type. This prevents accidental type conversion with `strify` or `intify`.

## Basic usage

```lil
x = 42
force x           # x is locked to numeric type
strify x          # ERROR: cannot strify a forced variable
```

`unforce` removes the lock:

```lil
unforce x         # unlock
strify x          # works now
print x           # prints "42"
```

## Combined assignment

Both `force` and `unforce` can assign a value at the same time:

```lil
force x = 100     # assign 100 to x AND lock it
unforce x = 200   # unlock x AND assign 200
```

## intify is also blocked

`intify` fails on forced variables too:

```lil
x = "hello"
force x
intify x          # ERROR: cannot intify a forced variable
```

## Regular assignment still works

Forcing only blocks `strify` and `intify`. Normal assignment works fine:

```lil
x = 42
force x
x = 99            # OK, still a number
print x           # prints 99
```

## Force with input

You can force a variable's type at input time:

```lil
force int input "Enter your age: " age   # reads a number, stores as number, locks it
force str input "Enter your name: " name # reads a string, stores as string, locks it
```

`force int input` validates the input is a valid number at read time and gives a clear error if it isn't. `force str input` stores the input as a string and prevents accidental conversion to number.

These are equivalent to:
```lil
input "Enter your age: " age
force age
# but with validation at input time instead of at use time
```
