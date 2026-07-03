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

x = new@list
push@list x 1
push@list x 2
push@list x 3
print len@list x
print at@list x 1
val = pop@list x
```

### Functions
- `new@list`  -  create empty list
- `push@list list val`  -  append value to end
- `pop@list list`  -  remove and return last element
- `at@list list index`  -  retrieve element without modifying
- `len@list list`  -  number of elements
- `map@list list fn`  -  apply function to each element, returns new list
- `filter@list list fn`  -  keep elements where function returns truthy
- `reduce@list list fn init`  -  reduce list to single value

## map/filter/reduce

These higher-order functions take a user-defined function and apply it to each element:

```lil
include list

#double(x) {
    x * 2
}

nums = new@list
push@list nums 1
push@list nums 2
push@list nums 3

doubled = map@list nums double
print doubled
```

`filter` keeps elements where the function returns truthy:

```lil
#isbig(x) {
    if x > 5 { 1 } else { 0 }
}

filtered = filter@list nums isbig
print filtered
```

`reduce` combines all elements using a two-parameter function and an initial value:

```lil
#add(a, b) {
    a + b
}

total = reduce@list nums add 0
print total
```
