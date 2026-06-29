# Print

The `print` statement outputs values to stdout.

```lil
print "hello"
print 42
print 3.14
```

Multiple values separated by commas are printed with spaces between them:

```lil
print "the answer is", 42
```

Print with no arguments is a syntax error. To print a blank line, print an empty string:

```lil
print ""
```

Print the result of a library function directly:

```lil
print &date|full
```

Print multiple results from different functions in one line:

```lil
print &date|minimal, &math|random
```

Print values alongside variables:

```lil
name = "bob"
score = 95
print "player", name, "scored", score
```
