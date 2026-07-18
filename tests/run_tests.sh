#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="${1:-$SCRIPT_DIR/../build/execra}"
WORKDIR="$(mktemp -d)"
export HOME="$WORKDIR/home"
mkdir -p "$HOME"

PASS=0
FAIL=0

run_test() {
    local name="$1" input="$2" expect="$3" forbid="${4:-}"
    local out
    out="$(cd "$WORKDIR" && printf '%b' "$input" | "$BIN" 2>&1)"

    local clean
    clean="$(printf '%s' "$out" | sed -e 's/\x1b\[[0-9;]*[a-zA-Z]//g' -e 's/\r//g')"

    local ok=1
    if [[ -n "$expect" && "$out" != *"$expect"* ]]; then ok=0; fi
    if [[ -n "$forbid" ]] && printf '%s\n' "$clean" | grep -qxF "$forbid"; then ok=0; fi

    if [[ "$ok" -eq 1 ]]; then
        PASS=$((PASS+1))
        echo "  PASS  $name"
    else
        FAIL=$((FAIL+1))
        echo "  FAIL  $name"
        echo "        expected to contain: '$expect'"
        [[ -n "$forbid" ]] && echo "        expected NOT to contain: '$forbid'"
        echo "        --- actual output ---"
        echo "$out" | sed 's/^/        /'
        echo "        ---------------------"
    fi
}

if [[ ! -x "$BIN" ]]; then
    echo "execra binary not found or not executable at: $BIN"
    echo "Build it first with: make"
    exit 1
fi

echo "Testing binary: $BIN"
echo "Sandbox dir:    $WORKDIR"
echo

echo "== Basic commands =="
run_test "pwd runs"                    'pwd\nexit\n'                              "$WORKDIR"
run_test "echo plain args"             'echo one two three\nexit\n'               "one two three"
run_test "echo quoted string"          'echo "hello world"\nexit\n'               "hello world"
run_test "echo single-quoted string"   "echo 'hi there'\nexit\n"                  "hi there"

echo
echo "== cd builtin =="
run_test "cd into subdir + pwd"        'mkdir sub\ncd sub\npwd\nexit\n'            "$WORKDIR/sub"
run_test "cd - returns to previous"    'mkdir sub\ncd sub\ncd -\npwd\nexit\n'      "$WORKDIR"$'\n'
run_test "cd nonexistent dir errors"   'cd /no/such/dir\nexit\n'                  "No such file or directory"

echo
echo "== Redirection =="
run_test "output redirection >"        'echo hi > out.txt\ncat out.txt\nexit\n'   "hi"
run_test "append redirection >>"       'echo a > f.txt\necho b >> f.txt\ncat f.txt\nexit\n' "a"$'\n'"b"
run_test "input redirection <"         'printf "x\ny\nz\n" > f.txt\nwc -l < f.txt\nexit\n' "3"
run_test "missing redirect target"     'echo hi >\nexit\n'                        "expected filename"

echo
echo "== Pipes (single and multi-stage) =="
run_test "single pipe"                 'printf "a\nb\nc\n" | grep b\nexit\n'      "b"
run_test "three-stage pipe"            'printf "a\nb\nb\n" | grep b | wc -l\nexit\n' "2"
run_test "leading pipe is a syntax error"  '| ls\nexit\n'                         "syntax error"
run_test "trailing pipe is a syntax error" 'ls |\nexit\n'                         "syntax error"

echo
echo "== Command chaining =="
run_test "&& runs on success"          'true && echo yes\nexit\n'                 "yes"
run_test "&& skips on failure"         'false && echo no\nexit\n'                 "" "no"
run_test "|| runs on failure"          'false || echo fallback\nexit\n'           "fallback"
run_test "|| skips on success"         'true || echo skipped\nexit\n'             "" "skipped"
run_test "; runs both regardless"      'echo a; echo b\nexit\n'                   "a"$'\n'"b"

echo
echo "== Background jobs =="
run_test "background job reports pid"  'sleep 1 &\nexit\n'                       "["
run_test "shell stays responsive"      'sleep 1 &\necho still_here\nexit\n'       "still_here"

echo
echo "== History =="
run_test "history lists prior commands" 'echo marker_cmd\nhistory\nexit\n'       "marker_cmd"

echo
echo "== Error handling / edge cases =="
run_test "unknown command"             'thiscommanddoesnotexist\nexit\n'          "command not found"
run_test "unmatched quote"             'echo "unterminated\nexit\n'               "unmatched quote"
run_test "empty input line is ignored" '\n\necho ok\nexit\n'                      "ok"

(cd "$WORKDIR" && printf '%b' 'exit 3\n' | "$BIN" >/dev/null 2>&1)
code=$?
if [[ "$code" -eq 3 ]]; then
    PASS=$((PASS+1))
    echo "  PASS  exit with explicit code propagates \$?"
else
    FAIL=$((FAIL+1))
    echo "  FAIL  exit with explicit code propagates \$? (got $code, expected 3)"
fi

echo
echo "================================"
echo " Results: $PASS passed, $FAIL failed"
echo "================================"

rm -rf "$WORKDIR"

if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
exit 0
