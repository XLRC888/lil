# Built-in Libraries

Libraries are called with the `function` keyword:

```lil
function <library> <function> <arguments...>
```

Arguments are separated by spaces. If an argument is a variable name, prefix it with `$` to dereference it. Without `$`, the argument is treated as a literal string.

## string library

```lil
function string lower $s       # convert to lowercase (modifies $s)
function string upper $s       # convert to uppercase (modifies $s)
function string reverse $s     # reverse the string (modifies $s)
function string len $s         # returns the length
function string repeat $s 3    # repeat the string 3 times
function string substr $s 1 3  # extract substring (start index, length)
function string replace $s "old" "new"  # replace all occurrences
```

The `$` prefix tells lil to use the variable's value:

```lil
name = "alice"
function string upper $name
print name    # prints "ALICE"
```

## math library

```lil
function math eval "2 + 3 * 4"  # evaluate math expression, returns number
function math random            # random number between 0 and 1
function math randint 1 10      # random integer between 1 and 10
function math factors 12        # returns string of factors: "1 2 3 4 6 12"
function math fib 10            # returns the 10th fibonacci number
function math isprime 7         # returns 1 if prime, 0 otherwise
function math sleep 0.5         # sleep for N seconds (can be fractional)
function math hasops "2+3"      # returns 1 if string contains math operators
function math choice "a" "b" "c"  # returns a random argument
```

## file library

```lil
content = function file read "path"       # read entire file, returns string
function file write "path" "content"       # write to file (overwrites)
function file append "path" "content"      # append to file
function file delete "path"                # delete file, returns 1 on success
exists = function file exists "path"       # returns 1 if file exists, 0 otherwise
listing = function file list "path"        # list directory contents, newline separated
```

## sys library

```lil
output = function sys cmd "command"        # run shell command, returns stdout
value = function sys env "VARNAME"         # get environment variable, returns "" if not set
```

## date library

```lil
print function date minimal          # "1.6.26" (or "6/1/26" in US mode)
print function date standart         # "01.06.2026"
print function date standartplus     # "01.06.2026 Monday"
print function date full             # "01.06.2026 Monday 12:00:00"
print function date full noseconds   # without seconds
print function date full noday       # without day name
print function date fullplus         # with milliseconds
function date set format eu          # day.month.year (default)
function date set format us          # month/day/year
```

All `date` functions return the date string, so you `print` the return value.

## force / unforce

Lock a variable to its current type to prevent `strify`/`intify` conversions:

```lil
x = 42
force x           # x is now locked to numeric type
strify x          # ERROR: cannot strify a forced variable
unforce x         # unlock
strify x          # works now
print x           # prints "42"
```

Combined assignment and force:

```lil
force x = 42      # assign and lock in one step
unforce x = 56    # unlock and assign in one step
```

`force`/`unforce` work in both interpreted and compiled mode.

### Force with input

```lil
force int input "Enter age: " age   # reads number, forces to numeric type
force str input "Enter name: " name # reads string, forces to string type
```

`force int input` validates the input at read time and gives a clear error if it's not a valid number.
