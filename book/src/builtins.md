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

Libraries can be disabled with `drop`:

```lil
include math
randint@math(1, 10)
drop math
randint@math(1, 10)    : error: library not imported
```

If the library wasn't included, `drop` prints a warning and continues:

```lil
drop math          : warning: library 'math' was not included
```

Libraries are called with `func(args)@lib` syntax:

```lil
<function>@<library>(<arguments...>)
```

Arguments are comma-separated in parentheses. Bare variable names are auto-dereferenced. Use double quotes for literal strings.

## string library

```lil
lower@string(s)
upper@string(s)
reverse@string(s)
len@string(s)
repeat@string(s, 3)
substr@string(s, 1, 3)
replace@string(s, "old", "new")
split@string(s, ",")
join@string(list, ",")
contains@string(s, "sub")
find@string(s, "sub")
ord@string(s)
chr@string(65)
isdigit@string(s)
isalpha@string(s)
isalnum@string(s)
isspace@string(s)
```

`split` returns a list of substrings:

```lil
include string
parts = split@string("a,b,c", ",")
write(parts)
```

`join` concatenates list elements with a delimiter:

```lil
include string
result = join@string(parts, "-")
write(result)
```

`contains` returns 1 if the string contains the substring, 0 otherwise:

```lil
include string
x = contains@string("hello world", "world")
write(x)
```

`find` returns the index of the first occurrence, or -1 if not found:

```lil
include string
idx = find@string("hello", "ell")
write(idx)
```

`ord` returns the ASCII code of the first character:

```lil
include string
code = ord@string("A")
write(code)
```

`chr` returns a single-character string from an ASCII code:

```lil
include string
c = chr@string(65)
write(c)
```

Character classifiers check if a string consists entirely of the given character type:

```lil
include string
write(isdigit@string("123"))
write(isalpha@string("abc"))
write(isalnum@string("a1"))
write(isspace@string("  "))
write(isdigit@string("12a"))
```

Bare variable names are auto-referenced, no `$` needed:

```lil
name = "alice"
upper@string(name)
write(name)
```

Assign the result to a variable:

```lil
name = "alice"
len = len@string(name)
write(len)
```

Method chaining works on values:

```lil
name = "HELLO"
lower_name = name.lower()@string
write(lower_name)

clean = "  HELLO  ".replace(" ", "")@string.lower()@string
write(clean)
```

## math library

```lil
calc@math("2 + 3 * 4")
random@math()
randint@math(1, 10)
factors@math(12)
fib@math(10)
isprime@math(7)
sleep@math(0.5)
hasops@math("2+3")
choice@math("a", "b", "c")
```

Assign results to variables:

```lil
roll = randint@math(1, 6)
write("you rolled", roll)
```

```lil
n = random@math()
write(n)
```

```lil
prime = isprime@math(17)
write(prime)
```

```lil
dice = choice@math("fire", "ice", "lightning")
write(dice)
```

## file library

```lil
content = read@file("path")
write@file("path", "content")
append@file("path", "content")
delete@file("path")
exists = exists@file("path")
listing = list@file("path")
```

Most file functions return values you'll want to capture:

```lil
data = read@file("config.txt")
write(data)
```

```lil
ok = delete@file("temp.tmp")
if ok == 1 {
  write("deleted")
}
```

```lil
files = list@file("/home/user")
write(files)
```

```lil
cfg = read@file("settings.cfg")
if cfg == "" {
  write("no config found")
}
```

## sys library

```lil
output = cmd@sys("command")
value = env@sys("VARNAME")
```

Assign results to check environment variables or capture command output:

```lil
home = env@sys("HOME")
write("home directory:", home)
```

```lil
result = cmd@sys("ls -la")
write(result)
```

```lil
shell = env@sys("SHELL")
write("your shell is", shell)
```

### Debugging

```
include sys
log@sys("debug message")
```

`log@sys` prints to stderr. `last_error@sys` returns the last error from a failed FFI call. `clear_error@sys` resets it.

## list library

```lil
x = new@list
push@list(x, 1)
push@list(x, 2)
push@list(x, 3)
val = pop@list(x)
write(x[0])
write(len@list(x))
```

Create a list, add elements, remove and access them. Read elements with bracket syntax `x[i]`:

```lil
include list
items = new@list
push@list(items, "apple")
push@list(items, "banana")
push@list(items, "cherry")
write(items[1])
```

Push returns 0, pop returns the removed element. `len@list` returns the count.

### map/filter/reduce

Higher-order functions that take a user-defined function by name:

```lil
include list

#double(x) { x * 2 }

nums = new@list
push@list(nums, 1)
push@list(nums, 2)
push@list(nums, 3)

doubled = map@list(nums, double)
write(doubled)
```

```lil
#isbig(x) { if x > 5 { 1 } else { 0 } }

filtered = filter@list(nums, isbig)
write(filtered)
```

```lil
#add(a, b) { a + b }

total = reduce@list(nums, add, 0)
write(total)
```

`map@list` returns a new list with the function applied to each element. `filter@list` keeps elements where the function returns truthy. `reduce@list` combines all elements with a two-parameter function and an initial value.

## dict library

```lil
d = new@dict
set@dict(d, "name", "alice")
set@dict(d, "age", 30)
write(get@dict(d, "name"))
write(contains@dict(d, "age"))
write(len@dict(d))
write(keys@dict(d))
```

Dicts store key-value pairs. `get@dict` returns the value or 0 if missing.

```lil
remove@dict(d, "age")
clear@dict(d)
```

## date library

```lil
write(minimal@date)
write(standart@date)
write(standartplus@date)
write(full@date)
write(full@date, noseconds)
write(full@date, noday)
write(fullplus@date)
format@date(eu)
format@date(us)
write(format@date)
```

Dates are commonly assigned to variables for later use:

```lil
today = standart@date
write("date:", today)
```

```lil
now = full@date
write(now)
```

```lil
short = minimal@date
write(short)
```

Most `date` functions return the date string. `format@date` returns the current mode ("EU"/"US"), and setting the format returns empty.

## Variable extraction (get)

Extract variable values from other `.lil` files at runtime:

```lil
get "TOKEN" from /path/to/file.lil
get "TOKEN" from file.lil              : relative path
get "TOKEN" from /path/to/file         : .lil added automatically
```

**Selective extraction** with assignment index:

```lil
get "x"(2) from /tmp/data.lil          : 2nd-to-last assignment of x
```

**Renaming** on import:

```lil
get "TOKEN"="api_key" from auth.lil     : saves as api_key instead of TOKEN
```

**Multiple variables** in one call:

```lil
get "x", "y", "name" from data.lil
```

The target file is run in a sandboxed interpreter. Its variable state is isolated from the current script.
