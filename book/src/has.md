# The Has Operator

The `has` operator checks if a variable exists or if a string contains a substring. It can only be used inside `if` conditions.

## Variable existence

```lil
if has x {
  print "x exists"
}
```

This checks whether the variable `x` has been assigned. If the variable was never assigned, this is false.

## Substring search

```lil
text = "hello world"
if text has "world" {
  print "found"
}
```

## Flags

| flag | meaning |
|------|---------|
| `nocase` | case insensitive search |
| `anywhere` | search anywhere in the string (default) |
| `word` | match whole words only |

```lil
if text has nocase "WORLD" {
  print "found case insensitive"
}

if text has word "world" {
  print "found as whole word"
}
```

You can combine flags:

```lil
if text has nocase anywhere "WORLD" {
  print "found"
}
```
