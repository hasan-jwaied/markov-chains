#!/usr/bin/env bash
# Behavioural tests for the CLIs: argument validation, error paths and the
# regressions fixed from the original code (hangs, split words).
#
#   run_cli_tests.sh <tweets_generator> <snakes_and_ladders>
set -uo pipefail

tweets="$1"
snakes="$2"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
corpus="$root/data/corpora/justdoit_tweets.txt"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
pass=0 fail=0

# expect <name> <expected-exit> <stderr/stdout regex or ""> -- command...
expect() {
    local name="$1" want="$2" pattern="$3"; shift 4
    local out status
    out="$(timeout 20 "$@" 2>&1)"; status=$?
    if [[ $status -ne $want ]]; then
        echo "FAIL $name: exit $status, expected $want"; echo "$out" | head -3
        fail=$((fail + 1)); return
    fi
    if [[ -n "$pattern" ]] && ! grep -Eq -- "$pattern" <<< "$out"; then
        echo "FAIL $name: output does not match /$pattern/"; echo "$out" | head -3
        fail=$((fail + 1)); return
    fi
    pass=$((pass + 1))
}

printf 'a. b. c.\n' > "$tmp/all_terminal.txt"
: > "$tmp/empty.txt"
{ head -c 3000 /dev/zero | tr '\0' 'x'; echo " tail."; } > "$tmp/long_word.txt"

expect no_args          1 "Usage"                 -- "$tweets"
expect bad_seed         1 "invalid value for seed" -- "$tweets" abc 1 "$corpus"
expect negative_count   1 "num_tweets"            -- "$tweets" 1 -5 "$corpus"
expect huge_count       1 "num_tweets"            -- "$tweets" 1 99999999999999999999 "$corpus"
expect empty_number     1 "seed"                  -- "$tweets" "" 1 "$corpus"
expect missing_file     1 "incorrect file path"   -- "$tweets" 1 1 "$tmp/nope.txt"
expect bad_order        1 ""                      -- "$tweets" --order 9 1 1 "$corpus"
expect unknown_flag     1 "Usage"                 -- "$tweets" --frobnicate 1 1 "$corpus"
# The original looped forever when every word ended with '.'
expect all_terminal     1 "no state can start"    -- "$tweets" 1 1 "$tmp/all_terminal.txt"
expect empty_corpus     1 "no states"             -- "$tweets" 1 1 "$tmp/empty.txt"
# The original split words longer than its 1000-byte line buffer
expect long_word        0 "^Tweet 1: x{3000} tail\.$" -- "$tweets" 1 1 "$tmp/long_word.txt"
expect zero_tweets      0 ""                      -- "$tweets" 1 0 "$corpus"
expect modern_order3    0 "^Tweet 3: "            -- "$tweets" --modern --order 3 --sentence 4 3 "$corpus"

a="$("$tweets" --modern 9 5 "$corpus")"; b="$("$tweets" --modern 9 5 "$corpus")"
if [[ "$a" == "$b" && -n "$a" ]]; then pass=$((pass + 1)); else echo "FAIL modern_deterministic"; fail=$((fail + 1)); fi

expect snakes_args      1 "Usage"                 -- "$snakes" 1
expect snakes_bad_seed  1 "seed"                  -- "$snakes" abc 1
expect snakes_negative  1 "num_paths"             -- "$snakes" 1 -3
expect snakes_analyze   0 "E\[T\]  = 47\.5712"    -- "$snakes" --analyze
expect snakes_modern    0 "^Random Walk 2: \[1\]" -- "$snakes" --modern 4 2

echo "cli: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
