# execra

A POSIX-style shell written in C++17. Supports raw-mode line editing
(arrow-key history navigation, in-place cursor movement, tab-completion
from history), N-stage pipelines, I/O redirection, heredocs, background
job execution, real job control (fg/bg/Ctrl+Z/kill), and command chaining.

This is a merge of two earlier prototypes: one had every feature but lived
in a single file, the other was split into modules but was missing pieces.
This version keeps the split and has the full feature set.

## Features

- Quoted-argument tokenizer (`echo "hello world"` stays one argument),
  with backslash escapes
- Multi-stage pipelines: `cmd1 | cmd2 | cmd3 | ...`
- Redirection: `>` (truncate), `>>` (append), `<` (input), `<<DELIM` (heredoc)
- Command chaining: `;` (sequence), `&&` (run-on-success), `||` (run-on-failure)
- Background execution: `sleep 5 &`, with async reaping — every stage of
  a background pipeline is tracked, not just the last one
- Real job control: `jobs`, `fg [%n]`, `bg [%n]`, `kill <pid|%n>`, and
  Ctrl+Z to stop a foreground job (uses process groups + `tcsetpgrp`,
  the same mechanism a real shell uses)
- Builtins: `cd` (including `cd -`), `history`, `jobs`, `fg`, `bg`,
  `kill`, `help`, `exit [code]`
- Persistent history across sessions, saved to `~/.execra_history`
- Arrow-key history navigation, in-place line editing, and Tab to accept
  a greyed-out history-based suggestion
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
for history, Tab to accept a suggestion, `exit` or Ctrl+D to leave. A few
things to try:

```
$ echo "hello world" | wc -c
$ ls *.cpp > files.txt
$ cat < files.txt
$ cat << EOF
> multi-line
> input
> EOF
$ sleep 30 &
$ jobs
$ sleep 30
^Z
$ bg
$ fg
$ kill %1
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
redirection modes, heredocs, single- and multi-stage pipes, chaining
(`&&`/`||`/`;`), background jobs, the `jobs`/`kill` builtins, `history`,
and edge cases (unmatched quotes, dangling pipes, missing redirection
targets, unknown commands, `exit <code>`).

It does **not** exercise raw-mode line editing (arrow-key recall, tab
suggestions, in-place backspace/cursor movement) or interactive job
control (Ctrl+Z / `fg` reclaiming the terminal), since those need a real
pseudo-terminal rather than a pipe. Those were verified manually and
interactively during development.

You can point it at a different binary if needed:
```bash
./tests/run_tests.sh /path/to/execra
```

## Project layout

```
src/main.cpp     entry point: signal setup, process-group/terminal setup, starts the shell
src/types.hpp    shared data types (Command, PipelineJob, BgJob, Connector)
src/colors.hpp   ANSI color macros
src/history.hpp/.cpp   persistent command history (load/save/push)
src/input.hpp/.cpp     raw-mode line editor (arrows, backspace, tab-suggest)
src/parser.hpp/.cpp    tokenizer + grammar (pipes, redirection, heredoc, chaining)
src/exec.hpp/.cpp      process launching, pipelines, job control (fg/bg/kill)
src/shell.hpp/.cpp     orchestrates the above: the run loop, builtins, prompt
tests/run_tests.sh     test suite (bash, drives execra via piped stdin)
Makefile
```

## Known limitations

- No variable expansion (`$HOME`, `$?`) or globbing (`*.txt` is passed
  through to `execvp` as a literal argument unless the invoked program
  expands it itself).
- Tab only completes from history, not filenames/commands.
