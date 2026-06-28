# Comparison and Logic

## Comparisons

Comparisons return `1` (true) or `0` (false):

```lil
x = 5 == 5    # 1 (true)
y = 5 != 5    # 0 (false)
z = 5 < 10    # 1
w = 5 >= 10   # 0
```

Full list: `==`, `!=`, `<`, `>`, `<=`, `>=`

You can compare strings lexicographically:

```lil
print "apple" < "banana"   # 1
print "abc" == "abc"        # 1
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
