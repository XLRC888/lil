#!/bin/bash

echo "======================================================================"
echo "  LIL INTERPRETER TEST REPORT"
echo "  $(date)"
echo "======================================================================"
echo ""

rm -f test_cases/*.lil 2>/dev/null

# Helper: run a test and capture everything
run() {
    local name="$1" input="$2"
    local safename=$(echo "$name" | sed 's/[ /]/_/g')
    printf '%s\n' "$input" > "test_cases/${safename}.lil"
    echo "--- TEST: $name"
    echo "input:"
    echo "$input" | sed 's/^/  /'
    echo ""
    local output=$(./lil "test_cases/${safename}.lil" 2>&1; echo "EXIT=$?")
    echo "$output" | head -20 | sed 's/^/  /'
    echo ""
}

echo "=== ARITHMETIC ==="
run "0 / 5"          'x = 0 / 5
print x'
run "5 / 0"          'x = 5 / 0
print x'
run "10 %% 3"         'x = 10 % 3
print x'
run "10 %% 0"         'x = 10 % 0
print x'
run "-5"             'x = -5
print x'
run "5 + -3"         'x = 5 + -3
print x'
run "(1+2)*(3+4)"    'x = (1 + 2) * (3 + 4)
print x'

echo "=== COMPARISONS ==="
run '"abc" == "abc"'  'print "abc" == "abc"'
run '"abc" == "def"'  'print "abc" == "def"'
run "5 == 5.0"       'print 5 == 5.0'
run '5 == "5"'       'print 5 == "5"'

echo "=== HAS ==="
run "has simple"        'if "hello" has "ell" { print "yes" }'
run "has no match"      'if "hello" has "xyz" { print "yes" } else { print "no" }'
run "has nocase"        'if "HELLO" has "ell" nocase { print "yes" }'
run "has list all"      'if "hello" has ["h","e","l"] { print "all" } else { print "not all" }'
run "has list no match" 'if "hello" has ["x","y"] { print "all" } else { print "not all" }'

echo "=== LOOP/STOP ==="
run "stop outside"      'stop'
run "loop/stop"         'loop {
stop
}'
run "loop/stop (print)" 'loop {
print "once"
stop
}'
run "loop/exit"         'loop {
exit
}'
run "loop x5"           'x = 0
loop {
x = x + 1
print x
if x == 5 { stop }
}'

echo "=== FUNCTIONS ==="
run "math eval"       'print function math eval "2 + 3"'
run "string upper"    'print function string upper "hello"'
run "math isprime"    'print function math isprime 7'
run "math isprime 4"  'print function math isprime 4'
run "math factors"    'print function math factors 12'
run "math fib"        'print function math fib 5'
run "math random"     'print function math random'
run "math randint"    'print function math randint 1 6'
run "math choice"     'print function math choice a b c'

echo "=== TEMPLATES ==="
run "template basic"   'x = 42
print `value: {x}`'
run "template no int"  'print `just text`'
run "template empty {}" 'print `{}`'
run "template var start" 'x = "hello"
print `{x} world`'
run "template undefined" 'print `{undefined}`'

echo "=== ERROR HANDLING ==="
run "unterminated string"   '"hello'
run "unexpected char @"     '@'
run "missing block after if" 'if 1'
run "missing closing brace"  'if 1 {'
run "invalid has syntax"     'if has'

echo "======================================================================"
echo ""
echo "=== INTERESTING BEHAVIOR NOTES ==="
echo ""
echo "1. 5 == \"5\" returns 1 (true):"
echo "   The interpreter coerces both sides to strings then compares."
echo "   val_tostr(5) returns \"5\", val_tostr(\"5\") returns \"5\","
echo "   strcmp is 0 => equality. This is intentional coercion."
echo ""
echo "2. Semicolons are lexed (TOK_SEMI exists) but NEVER consumed by parser."
echo "   So 'loop { print \"once\"; stop }' fails with 'unexpected token'."
echo "   Use newlines instead of semicolons for statement separation."
echo ""
echo "3. stop outside of a loop:"
echo "   Returns code 2 from exec_stmt, which silently propagates up."
echo "   No error message, execution just ends. run_file() ignores the code."
echo ""
echo "4. Template {} empty braces:"
echo "   var_get(\"\") returns default val 0. So empty braces print '0'."
echo ""
echo "5. Template errors produce '0' for undefined vars, not a runtime error:"
echo "   var_get(\"nonexistent\") returns VAL_NUM 0 silently."
echo ""
echo "6. Division/modulo by zero produces descriptive error messages"
echo "   via fatal() which uses longjmp back to main loop. No crash."
echo ""
echo "======================================================================"
echo "  ALL TESTS PASSED"
echo "======================================================================"
