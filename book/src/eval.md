# Dynamic Eval

The `sys` library includes an `eval` function that executes lil code from a string at runtime.

```
include sys
&sys|eval "x = 42"
print x
```

The return value of eval is the value of the last expression:

```
result = &sys|eval "1 + 2"
print result
```

Variables set inside eval are visible to the caller. Errors are caught and return zero.
