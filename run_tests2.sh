#!/bin/bash
FAILED=0
PASSED=0
CRASHED=0

run_test() {
    local name="$1"
    local input="$2"
    local expected="$3"

    safename=$(echo "$name" | tr ' /' '__')
    printf '%s\n' "$input" > "test_cases/${safename}.lil"
    output=$(./lil "test_cases/${safename}.lil" 2>&1)
    exitcode=$?

    if [ $exitcode -gt 128 ] || [ $exitcode -eq 139 ] || [ $exitcode -eq 134 ] || [ $exitcode -eq 6 ]; then
        echo "CRASHED [$exitcode] $name"
        echo "  input: $input"
        echo "  output: $output"
        CRASHED=$((CRASHED+1))
        FAILED=$((FAILED+1))
        return
    fi

    if [ -n "$expected" ]; then
        if echo "$output" | grep -qF "$expected"; then
            echo "PASS $name"
            PASSED=$((PASSED+1))
        else
            echo "FAIL $name"
            echo "  input: $input"
            echo "  expected: $expected"
            echo "  output: $output"
            FAILED=$((FAILED+1))
        fi
    else
        echo "INFO $name"
        echo "  output: $output"
        PASSED=$((PASSED+1))
    fi
}

rm -f test_cases/*.lil 2>/dev/null

echo "=== arithmetic edge cases ==="
run_test "zero_div_five" 'x = 0 / 5
print x' "0"
run_test "five_div_zero" 'x = 5 / 0
print x' "division by zero"
run_test "mod_10_3" 'x = 10 % 3
print x' "1"
run_test "mod_10_0" 'x = 10 % 0
print x' "modulo by zero"
run_test "neg_five" 'x = -5
print x' "-5"
run_test "five_plus_neg_three" 'x = 5 + -3
print x' "2"
run_test "nested_parens" 'x = (1 + 2) * (3 + 4)
print x' "21"

echo "=== comparison edge cases ==="
run_test "str_eq_same" 'x = "abc" == "abc"
print x' "1"
run_test "str_eq_diff" 'x = "abc" == "def"
print x' "0"
run_test "num_eq_num" 'x = 5 == 5.0
print x' "1"
run_test "num_eq_str" 'x = 5 == "5"
print x' "0"

echo "=== comparison print directly ==="
run_test "str_eq_same_direct" 'print "abc" == "abc"' "1"
run_test "str_eq_diff_direct" 'print "abc" == "def"' "0"
run_test "num_eq_num_direct" 'print 5 == 5.0' "1"
run_test "num_eq_str_direct" 'print 5 == "5"' "0"

echo "=== has edge cases ==="
run_test "has_simple" 'if "hello" has "ell" { print "yes" }' "yes"
run_test "has_nomatch" 'if "hello" has "xyz" { print "yes" } else { print "no" }' "no"
run_test "has_nocase" 'if "HELLO" has "ell" nocase { print "yes" }' "yes"
run_test "has_list_all" 'if "hello" has ["h","e","l"] { print "all" } else { print "not all" }' "all"
run_test "has_list_nomatch" 'if "hello" has ["x","y"] { print "all" } else { print "not all" }' "not all"

echo "=== loop/stop edge cases ==="
run_test "stop_outside" 'stop' "stop outside"
run_test "loop_stop" 'loop { stop }' ""
run_test "loop_stop_print" 'loop { print "once"; stop }' "once"
run_test "loop_exit" 'loop { exit }' ""

echo "=== function edge cases ==="
run_test "func_math_eval" 'print function math eval "2 + 3"' "5"
run_test "func_string_upper" 'print function string upper "hello"' "HELLO"
run_test "func_math_isprime" 'print function math isprime 7' "1"
run_test "func_math_factors" 'print function math factors 12' "1 2 3 4 6 12"
run_test "func_math_fib" 'print function math fib 5' "0 1 1 2 3"
run_test "func_math_random" 'print function math random' "0"
run_test "func_math_randint" 'print function math randint 1 6' ""

echo "=== template edge cases ==="
run_test "template_basic" 'x = 42
print `value: {x}`' "value: 42"
run_test "template_notpl" 'print `just text`' "just text"
run_test "template_empty_brace" 'print `{}`'
run_test "template_var_start" 'x = "hello"
print `{x} world`' "hello world"
run_test "template_undefined" 'print `{undefined}`' "0"

echo "=== error handling ==="
run_test "err_unterm_string" '"hello' "unterminated string"
run_test "err_unexpected_char" '@' "unexpected character"
run_test "err_missing_block_after_if" 'if 1' "expected"
run_test "err_missing_brace_if" 'if 1 {' "expected"
run_test "err_invalid_has_syntax" 'if has' "unexpected"

echo ""
echo "=== SUMMARY ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Crashed: $CRASHED"
