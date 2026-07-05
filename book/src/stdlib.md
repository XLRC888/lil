# Standard Library

The `lib/` directory contains reusable lil modules loaded with `include`.

## Loading

```
include lib/extra.lil/sum filter
```

## Modules

### extra.lil

- `#sum(list)`  -  add all elements
- `#range_list(start end)`  -  build list of numbers
- `#range(start end)`  -  print numbers

## Built-in FFI Errors

Functions that fail silently set a last error message:

```
include file
include sys
x = read@file("/nonexistent")
write(last_error@sys)
clear_error@sys
write(last_error@sys)
```

- `last_error@sys`  -  returns last error string (empty if none)
- `clear_error@sys`  -  resets error state
- `log@sys("msg")`  -  prints to stderr for debugging
