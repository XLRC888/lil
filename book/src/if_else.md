# If and Else

## Basic if

```lil
x = 10
if x > 5 {
  print "big"
}
```

## If/else

```lil
x = 2
if x > 5 {
  print "big"
} else {
  print "small"
}
```

## If/elif/else

```lil
x = 5
if x > 10 {
  print "big"
} elif x > 3 {
  print "medium"
} else {
  print "small"
}
```

You can chain as many `elif` blocks as you need.

## Inline conditions

You can put small blocks on one line:

```lil
if x > 5 { print "big" } else { print "small" }
```

## Has operator in conditions

The `if` statement can also check whether a variable exists or whether a string contains a substring:

```lil
name = "alice"
if has name {
  print "name exists"
}

text = "hello world"
if text has "world" {
  print "found world"
}

# case-insensitive search
if text has nocase "WORLD" {
  print "found world (case insensitive)"
}

# search anywhere in the string (default)
if text has anywhere "ello" {
  print "found"
}

# match whole words only
if text has word "world" {
  print "found as whole word"
}
```

Multiple flags can be combined:

```lil
if text has nocase anywhere "WORLD" {
  print "found"
}
```
