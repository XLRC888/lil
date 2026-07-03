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

String indexing works the same way. `s[i]` returns a single-character string, and `s[i] = "x"` replaces a character:

```
s = "hello"
print s[0]          # "h"
s[0] = "j"
print s             # "jello"
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
- `&list|map list $fn` — apply function to each element, returns new list
- `&list|filter list $fn` — keep elements where function returns truthy
- `&list|reduce list $fn init` — reduce list to single value

## map/filter/reduce

These higher-order functions take a user-defined function and apply it to each element:

```lil
include list

$double(x) {
    x * 2
}

nums = &list|new
&list|push nums 1
&list|push nums 2
&list|push nums 3

doubled = &list|map nums $double
print doubled
```

`filter` keeps elements where the function returns truthy:

```lil
$isbig(x) {
    if x > 5 { 1 } else { 0 }
}

filtered = &list|filter nums $isbig
print filtered
```

`reduce` combines all elements using a two-parameter function and an initial value:

```lil
$add(a, b) {
    a + b
}

total = &list|reduce nums $add 0
print total
```
