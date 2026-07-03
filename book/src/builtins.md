# Built-in Libraries

Libraries must be imported with `include` before use:

```lil
include math
include date
include string
include file
include sys
include gtk
include list
```

Libraries can be disabled with `uninclude`:

```lil
include math
randint@math 1 10
uninclude math
randint@math 1 10    # error: library not imported
```

If the library wasn't included, `uninclude` prints a warning and continues:

```lil
uninclude math          # warning: library 'math' was not included
```

Libraries are called with `@` syntax:

```lil
<function>@<library> <arguments...>
```

Arguments are separated by spaces. Bare variable names are auto-dereferenced. Use double quotes for literal strings.

## string library

```lil
lower@string s
upper@string s
reverse@string s
len@string s
repeat@string s 3
substr@string s 1 3
replace@string s "old" "new"
split@string s ","
join@string list ","
contains@string s "sub"
find@string s "sub"
ord@string s
chr@string 65
isdigit@string s
isalpha@string s
isalnum@string s
isspace@string s
```

`split` returns a list of substrings:

```lil
include string
parts = split@string "a,b,c" ","
print parts
```

`join` concatenates list elements with a delimiter:

```lil
include string
result = join@string parts "-"
print result
```

`contains` returns 1 if the string contains the substring, 0 otherwise:

```lil
include string
x = contains@string "hello world" "world"
print x
```

`find` returns the index of the first occurrence, or -1 if not found:

```lil
include string
idx = find@string "hello" "ell"
print idx
```

`ord` returns the ASCII code of the first character:

```lil
include string
code = ord@string "A"
print code
```

`chr` returns a single-character string from an ASCII code:

```lil
include string
c = chr@string 65
print c
```

Character classifiers check if a string consists entirely of the given character type:

```lil
include string
print isdigit@string "123"
print isalpha@string "abc"
print isalnum@string "a1"
print isspace@string "  "
print isdigit@string "12a"
```

Bare variable names are auto-dereferenced, no `$` needed:

```lil
name = "alice"
upper@string name
print name
```

Assign the result to a variable:

```lil
name = "alice"
len = len@string name
print len
```

## math library

```lil
eval@math "2 + 3 * 4"
random@math
randint@math 1 10
factors@math 12
fib@math 10
isprime@math 7
sleep@math 0.5
hasops@math "2+3"
choice@math "a" "b" "c"
```

Assign results to variables:

```lil
roll = randint@math 1 6
print "you rolled", roll
```

```lil
n = random@math
print n
```

```lil
prime = isprime@math 17
print prime
```

```lil
dice = choice@math "fire" "ice" "lightning"
print dice
```

## file library

```lil
content = read@file "path"
write@file "path" "content"
append@file "path" "content"
delete@file "path"
exists = exists@file "path"
listing = list@file "path"
```

Most file functions return values you'll want to capture:

```lil
data = read@file "config.txt"
print data
```

```lil
ok = delete@file "temp.tmp"
if ok == 1 {
  print "deleted"
}
```

```lil
files = list@file "/home/user"
print files
```

```lil
cfg = read@file "settings.cfg"
if cfg == "" {
  print "no config found"
}
```

## sys library

```lil
output = cmd@sys "command"
value = env@sys "VARNAME"
```

Assign results to check environment variables or capture command output:

```lil
home = env@sys "HOME"
print "home directory:", home
```

```lil
result = cmd@sys "ls -la"
print result
```

```lil
shell = env@sys "SHELL"
print "your shell is", shell
```

### Debugging

```
include sys
log@sys "debug message"
```

`log@sys` prints to stderr. `last_error@sys` returns the last error from a failed FFI call. `clear_error@sys` resets it.

## date library

```lil
print minimal@date
print standart@date
print standartplus@date
print full@date
print full@date noseconds
print full@date noday
print fullplus@date
format@date eu
format@date us
print format@date
```

Dates are commonly assigned to variables for later use:

```lil
today = standart@date
print "date:", today
```

```lil
now = full@date
print now
```

```lil
short = minimal@date
print short
```

Most `date` functions return the date string. `format@date` returns the current mode ("EU"/"US"), and setting the format returns empty.

## Variable extraction (get)

Extract variable values from other `.lil` files at runtime:

```lil
get "TOKEN" from /path/to/file.lil
get "TOKEN" from file.lil              # relative path
get "TOKEN" from /path/to/file         # .lil added automatically
```

**Selective extraction** with assignment index:

```lil
get "x"(2) from /tmp/data.lil          # 2nd-to-last assignment of x
```

**Renaming** on import:

```lil
get "TOKEN"="api_key" from auth.lil     # saves as api_key instead of TOKEN
```

**Multiple variables** in one call:

```lil
get "x", "y", "name" from data.lil
```

The target file is run in a sandboxed interpreter. Its variable state is isolated from the current script.
