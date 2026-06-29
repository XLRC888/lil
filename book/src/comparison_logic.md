# Comparison and Logic

## Comparisons

Comparisons return `1` (true) or `0` (false). The operators are the same as Python:

```lil
x = 5 == 5    # 1 (true)
y = 5 != 5    # 0 (false)
z = 5 < 10    # 1
w = 5 >= 10   # 0
```

Full list: `==`, `!=`, `<`, `>`, `<=`, `>=`

You can compare strings lexicographically, same as Python:

```lil
print "apple" < "banana"   # 1
print "abc" == "abc"        # 1
```

## Complex conditions

Combine comparisons with `and`, `or`, `not`:

```lil
x = 7
if x > 0 and x < 10 {
  print "in range"
}
```

Store the result of a complex condition:

```lil
in_range = x > 0 and x < 10
if in_range {
  print "x is between 1 and 9"
}
```

## Using not for negation

```lil
logged_in = 0
if not logged_in {
  print "please log in"
}
```

## Comparison with variables

Compare a variable to a value:

```lil
len = 10
if len > 5 {
  print "long name"
}
```

Store and use multiple comparisons:

```lil
len = 10
if len > 2 and len < 10 {
  print "name length is reasonable"
}
```

## Logical operators

```lil
a = 1 and 0    # 0 (false)
b = 1 or 0     # 1 (true)
c = not 1      # 0 (false)
```

`and` and `or` short-circuit. If the left operand determines the result, the right operand is never evaluated.

## Truthiness

For conditions, a value is considered true if it is a non-zero number or a non-empty string:

```lil
if "hello" { print "yes" }   # prints "yes" (non-empty string is truthy)
if 0 { print "no" }          # prints nothing (zero is falsy)
if "" { print "no" }         # prints nothing (empty string is falsy)
```
