# If and Else

## Basic if

```lil
x = 10
if x > 5 {
  write("big")
}
```

## If/else

```lil
x = 2
if x > 5 {
  write("big")
} else {
  write("small")
}
```

## If/orif/else

```lil
x = 5
if x > 10 {
  write("big")
} orif x > 3 {
  write("medium")
} else {
  write("small")
}
```

You can chain as many `orif` blocks as you need. `elif` still works as an alias.

## Inline conditions

You can put small blocks on one line:

```lil
if x > 5 { write("big") } else { write("small") }
```
