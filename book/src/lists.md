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
write(x[0])
write(x[1])
x[2] = 99
```

String indexing works the same way. `s[i]` returns a single-character string, and `s[i] = "x"` replaces a character:

```
s = "hello"
write(s[0])          : "h"
s[0] = "j"
write(s)             : "jello"
```

Out-of-range access errors at runtime.

## Destructuring

Pull multiple fields from a dict or struct in one line with `{field1, field2} = expr`:

```lil
node = {"type": "num", "value": 42}
{type, value} = node
write(type)
write(value)
```

Works with structs too (structs are dicts underneath):

```lil
struct Point { x, y }
p = Point(10, 20)
{x, y} = p
write(x, y)
```

This is shorthand for `type = node["type"]; value = node["value"]`. Field names become variable names.


## List Library

The `list` library provides operations for dynamic list manipulation:

```
include list

x = new@list
push@list(x, 1)
push@list(x, 2)
push@list(x, 3)
write(len@list(x))
write(x[1])
val = pop@list(x)
```

### Functions
- `new@list`  -  create empty list
- `push@list(list, val)`  -  append value to end
- `pop@list(list)`  -  remove and return last element
- `len@list(list)`  -  number of elements
- `map@list(list, fn)`  -  apply function to each element, returns new list
- `filter@list(list, fn)`  -  keep elements where function returns truthy
- `reduce@list(list, fn, init)`  -  reduce list to single value

Read individual elements with bracket syntax: `x[i]` returns the value at position `i` (0-indexed).

## map/filter/reduce

These higher-order functions take a user-defined function and apply it to each element:

```lil
include list

#double(x) {
    x * 2
}

nums = new@list
push@list(nums, 1)
push@list(nums, 2)
push@list(nums, 3)

doubled = map@list(nums, double)
write(doubled)
```

`filter` keeps elements where the function returns truthy:

```lil
#isbig(x) {
    if x > 5 { 1 } else { 0 }
}

filtered = filter@list(nums, isbig)
write(filtered)
```

`reduce` combines all elements using a two-parameter function and an initial value:

```lil
#add(a, b) {
    a + b
}

total = reduce@list(nums, add, 0)
write(total)
```
