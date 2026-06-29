# Continue

The `continue` keyword skips the rest of the current loop iteration and jumps to the next one. It works in `while`, `for`, and `loop` blocks.

## Skip even numbers, print odds

```lil
for i = 1 to 10 {
  if i % 2 == 0 { continue }
  print i
}
```

Prints 1, 3, 5, 7, 9. When `i` is even, `continue` skips the `print` and goes to the next iteration.

Accumulate odds into a variable:

```lil
sum_odds = 0
for i = 1 to 10 {
  if i % 2 == 0 { continue }
  sum_odds = sum_odds + i
}
print sum_odds
```

## Input validation loop

Skip bad input and keep asking until valid:

```lil
while 1 {
  age = &sys|readln
  if age == "" { continue }
  age = &sys|tonum age
  if age < 1 { continue }
  print "your age is $age"
  break
}
```

Both empty input and non-positive numbers hit `continue`, jumping back to the top of the while loop. Only a valid age escapes the loop.

## Continue in a while loop

```lil
i = 0
count = 0
while i < 100 {
  i = i + 1
  if i % 3 == 0 { continue }
  count = count + 1
}
print count
```

Counts how many numbers from 1 to 100 are not divisible by 3.
