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
