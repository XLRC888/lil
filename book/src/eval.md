# Dynamic Eval

`eval@sys` lets you execute lil code from a string at runtime:

```lil
include sys

eval@sys "x = 42"
print x
```

You can capture the return value:

```lil
include sys
result = eval@sys "1 + 2"
print result
```

Eval uses the same parser and interpreter as normal execution. Variables set inside eval are visible to the caller afterwards.

Errors during eval are caught silently and return 0.
