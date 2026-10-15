#!/usr/bin/env bash
# Golden-output runner.
#
#   run_golden.sh [--regen] [--tweets BIN] [--tweets-a BIN] [--snakes BIN]
#
# Runs every case in cases.txt whose program was given a binary and compares
# stdout byte-for-byte with expected/<name>.out. With --regen, (re)writes the
# expected files instead. Set GOLDEN_WRAPPER (e.g. "valgrind -q
# --error-exitcode=99 --leak-check=full") to run each case under a tool.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
declare -A bin=()
regen=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --regen)    regen=1; shift ;;
        --tweets)   bin[tweets]="$2"; shift 2 ;;
        --tweets-a) bin[tweets_a]="$2"; shift 2 ;;
        --snakes)   bin[snakes]="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

pass=0 fail=0 skip=0
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

while IFS='|' read -r name prog args; do
    name="$(echo "$name" | xargs)"; prog="$(echo "$prog" | xargs)"
    [[ -z "$name" || "$name" == \#* ]] && continue
    exe="${bin[$prog]:-}"
    if [[ -z "$exe" ]]; then skip=$((skip + 1)); continue; fi

    # shellcheck disable=SC2086  # args are intentionally word-split
    (cd "$root" && ${GOLDEN_WRAPPER:-} "$exe" $args) > "$tmp" || {
        echo "FAIL $name (exit status $?)"; fail=$((fail + 1)); continue; }

    expected="$here/expected/$name.out"
    if [[ $regen -eq 1 ]]; then
        cp "$tmp" "$expected"; echo "wrote $name"; pass=$((pass + 1))
    elif cmp -s "$tmp" "$expected"; then
        pass=$((pass + 1))
    else
        echo "FAIL $name (output differs)"; diff "$expected" "$tmp" | head -5
        fail=$((fail + 1))
    fi
done < "$here/cases.txt"

echo "golden: $pass passed, $fail failed, $skip skipped"
[[ $fail -eq 0 && $pass -gt 0 ]]
