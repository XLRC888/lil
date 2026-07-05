# Live Bindings

A `live` binding links one variable to another so that reads of the bound variable always reflect the current value of its source.

## Syntax

```lil
y = 1
live x = y
y = 42
write(x)
```

This prints `42`. After `live x = y`, every read of `x` follows the link back to `y` and returns whatever `y` currently holds. You do not need to reassign `x` when `y` changes.

## What gets bound

`live` records the source variable by name. Reads dereference that name at lookup time, so later assignments to the source propagate automatically:

```lil
a = 10
live b = a
a = 20
write(b)
```

Prints `20`.

## Reassigning the bound variable

Assigning directly to `x` overwrites the binding with the new value and clears the live link. `live x = y` followed by `x = 5` leaves `x` holding `5` as an ordinary variable, no longer tracking `y`.

## When to use it

Live bindings are useful for aliases that should track a changing source: a display variable that mirrors a counter, a cached view of a value that must stay in sync. For a one-time copy, use plain assignment (`x = y`).
