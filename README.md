# execra

A small POSIX-style shell written in C++17. Supports raw-mode line editing
(arrow-key history navigation, in-place cursor movement), N-stage pipelines,
I/O redirection, background job execution, and command chaining.

## Features

- Quoted-argument tokenizer (`echo "hello world"` stays one argument)
- Multi-stage pipelines: `cmd1 | cmd2 | cmd3 | ...`
- Redirection: `>` (truncate), `>>` (append), `<` (input)
- Background execution: `sleep 5 &`, with a `jobs` builtin and
  automatic zombie reaping (every stage of a background pipeline is reaped,
  not just the last one)
- Command chaining: `;` (sequence), `&&` (run-on-success), `||` (run-on-failure)
- Builtins: `cd` (including `cd -`), `history`, `jobs`, `exit [code]`
- Persistent history across sessions, saved to `~/.execra_history`
- Arrow-key history navigation and in-place line editing in the prompt
- Descriptive syntax-error messages for malformed input (unmatched quotes,
  dangling pipes, missing redirection targets) instead of crashing

## Build

Requires g++ with C++17 support (tested on g++ 13, Ubuntu 24.04).

```bash
make
```

This produces `build/execra`. Clean up with `make clean`.

## Run

```bash
./build/execra
```

It's a normal interactive shell from here — type commands, use arrow keys
for history, `exit` or Ctrl+D to leave. A few things to try:

```
$ echo "hello world" | wc -c
$ ls *.cpp > files.txt
$ cat < files.txt
$ sleep 5 &
$ jobs
$ false && echo unreachable
$ true || echo unreachable
$ echo one; echo two; echo three
$ history
```

## Run the test suite

```bash
./tests/run_tests.sh
```

This feeds command scripts to `execra` over a plain (non-interactive) pipe —
the same way an automated grader would run it — and checks the output
against expected results. It covers basic commands, `cd`, all three
redirection modes, single- and multi-stage pipes, chaining (`&&`/`||`/`;`),
background jobs, `history`, and edge cases (unmatched quotes, dangling
pipes, missing redirection targets, unknown commands, `exit <code>`).

It does **not** exercise the raw-mode line editing itself (arrow-key
recall, in-place backspace/cursor movement) since that needs a real
pseudo-terminal rather than a pipe. That was verified manually and
interactively during development.

Sample output:

```
== Basic commands ==
  PASS  pwd runs
  PASS  echo plain args
  PASS  echo quoted string
  ...

================================
 Results: 27 passed, 0 failed
================================
```

You can point it at a different binary if needed:
```bash
./tests/run_tests.sh /path/to/execra
```

## Project layout

```
src/main.cpp     entry point, installs SIGINT policy, starts the shell
src/shell.hpp    Shell class + Command/Job/Connector types
src/shell.cpp    tokenizer, parser, pipeline execution, builtins, line editor
tests/run_tests.sh   test suite (bash, drives execra via piped stdin)
Makefile
```

## Known limitations

- No variable expansion (`$HOME`, `$?`) or globbing (`*.txt` is passed
  through to `execvp` as a literal argument unless the invoked program
  expands it itself).
- No tab completion.
- Job control is reap-only — no `fg`/`bg`/`kill %1` to move jobs between
  foreground and background after the fact.
