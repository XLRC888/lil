# Standard Library

The `lib/` directory contains reusable lil modules that can be loaded with `include`.

## Loading Modules

```
include lib/extra.lil/sum range_list
print $sum([1, 2, 3])
$range_list(1 5)
```

## Available Modules

### extra.lil

- `$range(start end)` — print numbers from start to end-1
- `$sum(list)` — sum all elements in a list
- `$map(list fn)` — apply function to each element, return new list
- `$filter(list fn)` — return new list with elements where fn returns truthy
- `$range_list(start end)` — return list of numbers from start to end-1
