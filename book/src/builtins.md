# Built-in Libraries

Libraries are called with `&` syntax:

```lil
&<library>|<function> <arguments...>
```

Arguments are separated by spaces. Prefix a variable name with `$` to dereference it. Without `$`, the argument is treated as a literal string.

## string library

```lil
&string|lower $s
&string|upper $s
&string|reverse $s
&string|len $s
&string|repeat $s 3
&string|substr $s 1 3
&string|replace $s "old" "new"
```

The `$` prefix tells lil to use the variable's value:

```lil
name = "alice"
&string|upper $name
print name
```

Assign the result to a variable:

```lil
name = "alice"
len = &string|len $name
print len
```

```lil
msg = "hello world"
upper = &string|upper $msg
print upper
```

```lil
greeting = "ha"
repeated = &string|repeat $greeting 3
print repeated
```

## math library

```lil
&math|eval "2 + 3 * 4"
&math|random
&math|randint 1 10
&math|factors 12
&math|fib 10
&math|isprime 7
&math|sleep 0.5
&math|hasops "2+3"
&math|choice "a" "b" "c"
```

Assign results to variables:

```lil
roll = &math|randint 1 6
print "you rolled", roll
```

```lil
n = &math|random
print n
```

```lil
prime = &math|isprime 17
print prime
```

```lil
dice = &math|choice "fire" "ice" "lightning"
print dice
```

## file library

```lil
content = &file|read "path"
&file|write "path" "content"
&file|append "path" "content"
&file|delete "path"
exists = &file|exists "path"
listing = &file|list "path"
```

Most file functions return values you'll want to capture:

```lil
data = &file|read "config.txt"
print data
```

```lil
ok = &file|delete "temp.tmp"
if ok == 1 {
  print "deleted"
}
```

```lil
files = &file|list "/home/user"
print files
```

```lil
cfg = &file|read "settings.cfg"
if cfg == "" {
  print "no config found"
}
```

## sys library

```lil
output = &sys|cmd "command"
value = &sys|env "VARNAME"
```

Assign results to check environment variables or capture command output:

```lil
home = &sys|env "HOME"
print "home directory:", home
```

```lil
result = &sys|cmd "ls -la"
print result
```

```lil
shell = &sys|env "SHELL"
print "your shell is", shell
```

## date library

```lil
print &date|minimal
print &date|standart
print &date|standartplus
print &date|full
print &date|full noseconds
print &date|full noday
print &date|fullplus
&date|format eu
&date|format us
print &date|format
```

Dates are commonly assigned to variables for later use:

```lil
today = &date|standart
print "date:", today
```

```lil
now = &date|full
print now
```

```lil
short = &date|minimal
print short
```

Most `date` functions return the date string. `&date|format` returns the current mode ("EU"/"US"), and setting the format returns empty.
