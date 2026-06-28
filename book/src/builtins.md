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
function sys sleep 1000                    # sleep for N milliseconds
```

## date library

```lil
function date format minimal      # "1.1.24" style date
function date format standart     # "01.01.2024"
function date format standartplus # "01.01.2024 Monday"
function date format full         # "01.01.2024 Monday 12:00:00"
function date format fullplus     # with milliseconds
function date set format eu       # switch to EU format (default)
function date set format us       # switch to US format
```
