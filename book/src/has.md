# The Has Operator

The `has` operator checks if a variable exists or if a string contains a substring. It can only be used inside `if` conditions.

## Variable existence

```lil
if has x {
  print "x exists"
}
```

This checks whether the variable `x` has been assigned. If the variable was never assigned, this is false.

A practical use: check if data was read successfully:

```lil
data = read@file "data.txt"
if has data {
  print "read succeeded"
}
```

`read@file` returns empty string if the file doesn't exist. `has data` tells you if data is non-empty.

## Substring search

```lil
text = "hello world"
if text has "world" {
  print "found"
}
```

Check if a URL contains a protocol:

```lil
url = "https://example.com"
if url has "https" {
  print "secure connection"
}
```

## Multiple has conditions combined

You can chain `not` with `has`:

```lil
if has result and result has "error" {
  print "got an error response"
}
```

If `result` was never set, the first `has` is false and the substring check is never evaluated. This is a safe way to check both existence and content.

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

## Has vs regular comparison

`has` checks existence, not value. Use `==` to check a value:

```lil
x = 0
if has x { print "x exists" }   # prints (x was assigned)
if x == 0 { print "x is zero" } # also prints

y = ""
if has y { print "y exists" }   # prints (y was assigned, even empty)
if y == "" { print "y is empty" } # also prints
```

A variable can exist but hold a falsy value. Use `has` when you care whether it was assigned at all, not what it holds.
