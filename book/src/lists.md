# Lists

Lists are ordered collections of values. They are created with square bracket syntax and support index access.

## Creating Lists

```
x = [1, 2, 3]
mixed = ["hello", 42, 99]
empty = []
```

## Index Access

Access elements by position (0-indexed):

```
x = [10, 20, 30]
print x[0]
print x[1]
x[2] = 99
```

Out-of-range access errors at runtime.

## List Library

The `list` library provides operations for dynamic list manipulation:

```
include list

x = &list|new
&list|push x 1
&list|push x 2
&list|push x 3
print &list|len x
print &list|at x 1
val = &list|pop x
```

### Functions
- `&list|new` — create empty list
- `&list|push list val` — append value to end
- `&list|pop list` — remove and return last element
- `&list|at list index` — retrieve element without modifying
- `&list|len list` — number of elements
