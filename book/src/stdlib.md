# Standard Library

The `lib/` directory contains reusable lil modules loaded with `include`.

## Loading

```
include lib/extra.lil/sum filter
```

## Modules

### extra.lil

- `$sum(list)` — add all elements
- `$map(list fn)` — apply function to each element
- `$filter(list fn)` — keep elements where fn returns truthy
- `$range_list(start end)` — build list of numbers
- `$range(start end)` — print numbers

## Built-in FFI Errors

Functions that fail silently set a last error message:

```
include file
include sys
x = &file|read "/nonexistent"
print &sys|last_error
&sys|clear_error
print &sys|last_error
```

- `&sys|last_error` — returns last error string (empty if none)
- `&sys|clear_error` — resets error state
- `&sys|log "msg"` — prints to stderr for debugging
