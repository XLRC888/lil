# Modifiers

Modifiers use the `?` prefix to change global behavior. Other features like `get` and `include` with function selection are covered here.

## Modifiers

- [Undefined Default](undefined_default.md)  -  change what undefined variables return

## Get Statement

The `get` statement extracts variable values from an external file without leaking its variables into your scope:

```lil
get "x", "y" from data.lil
```

This runs `data.lil` in a sandbox, snapshots the values of `x` and `y`, then copies them into your script. The file's own variables are discarded.

**Indexed access**: retrieve historical (previous) assignment values:

```lil
get "x"(1) from data.lil    # 1st previous value of x
get "x"(2) from data.lil    # 2nd previous value
```

Index 0 (or no index) returns the current value. Uses the assignment history tracking in the VM.

**Rename on import**: map the file's variable to a different name:

```lil
get "x" = "myX", "y" = "myY" from data.lil
```

The file's `x` goes into your script's `myX`, and `y` goes into `myY`.

## Include with Function Selection

The `include` statement can import specific functions from a file instead of registering everything:

```lil
include dice.lil/roll_dice flip_coin
```

Only `roll_dice` and `flip_coin` from `dice.lil` are registered. Other functions defined in the file are discarded. This is useful for utility files that define many helpers but you only need a few.
