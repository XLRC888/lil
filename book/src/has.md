# The Has Operator

The `has` operator checks if a variable exists or if a string contains a substring. It can only be used inside `if` conditions.

## Variable existence

```lil
if has x {
  write("x exists")
}
```

This checks whether the variable `x` has been assigned. If the variable was never assigned, this is false.

A practical use: check if data was read successfully:

```lil
data = read@file("data.txt")
if has data {
  write("read succeeded")
}
```

`read@file` returns empty string if the file doesn't exist. `has data` tells you if data is non-empty.

## Substring search

```lil
text = "hello world"
if text has "world" {
  write("found")
}
```

Check if a URL contains a protocol:

```lil
url = "https://example.com"
if url has "https" {
  write("secure connection")
}
```

## Multiple has conditions combined

You can chain `not` with `has`:

```lil
if has result and result has "error" {
  write("got an error response")
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
  write("found case insensitive")
}

if text has word "world" {
  write("found as whole word")
}
```

You can combine flags:

```lil
if text has nocase anywhere "WORLD" {
  write("found")
}
```

## Has vs regular comparison

`has` checks existence, not value. Use `==` to check a value:

```lil
x = 0
if has x { write("x exists") }   : prints (x was assigned)
if x == 0 { write("x is zero") } : also prints

y = ""
if has y { write("y exists") }   : prints (y was assigned, even empty)
if y == "" { write("y is empty") } : also prints
```

A variable can exist but hold a falsy value. Use `has` when you care whether it was assigned at all, not what it holds.
