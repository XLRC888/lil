# Control Flow

This chapter covers all the ways to control what your program does next: if/elif/else conditions, while loops, for loops, and the loop/stop/break/continue keywords.

## If/elif/else

lil's if/elif/else works like Python but without colons or parentheses. In Python you write:

```python
if x > 5:
    print("big")
elif x > 0:
    print("small")
else:
    print("zero or negative")
```

In lil the same logic looks like:

```lil
if x > 5 {
  print "big"
} elif x > 0 {
  print "small"
} else {
  print "zero or negative"
}
```

Conditions don't need parentheses around them. Just write the expression directly after `if` or `elif`. The braces are required even for single-line bodies.

You can store condition results in variables:

```lil
is_positive = x > 0
if is_positive {
  print "x is positive"
}
```

Since conditions return 1 (true) or 0 (false), you can use them in arithmetic:

```lil
count = (x > 10) + (y > 10) + (z > 10)
print "variables over 10: $count"
```

## Loop types

lil has three kinds of loops covered on their own pages:

- [While loops](while_loops.md) - loop while a condition holds
- [For loops](for_loops.md) - iterate over a numeric range
- [Loop blocks](loop_stop_break.md) - infinite loop with explicit exit

All three support `break`, `stop`, and `continue`.
