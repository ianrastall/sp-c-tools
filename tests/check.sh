#!/usr/bin/env bash
# SPCT regression test.
#
# Builds spct and smoke, runs the core smoke test, then runs every report
# case listed for each corpus under tests/golden/ and compares each output
# (stdout included) against the recorded SHA-256 hashes.
#
# Usage:  tests/check.sh [--update]
#   --update  re-record the golden hashes from the current build. Use it only
#             after an INTENDED output change, and review the diff of the
#             *.statistics.txt snapshots before committing.
#
# Layout, per corpus <name> (input is tests/data/<name>.pgn):
#   tests/golden/<name>/input.sha256   hash of the corpus the goldens came from
#   tests/golden/<name>/cases          one case per line: "<case> <spct args...>"
#                                      (the corpus file is appended as last arg)
#   tests/golden/<name>/<case>.sha256  hashes of that case's outputs
#   tests/golden/<name>/<case>.statistics.txt   readable report snapshot
#                                      (EAS or SGS statistics, if the case has one)
#
# A corpus whose PGN is missing (large corpora are gitignored) or differs
# from input.sha256 is skipped with a note, not failed.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOLDEN="$ROOT/tests/golden"
UPDATE=0
[ "${1:-}" = "--update" ] && UPDATE=1

OUTPUTS="stdout.txt statistics_EAS_ratinglist.txt interesting_wins.pgn \
         very_interesting_wins.pgn errorgames.pgn statistics.txt \
         sacgames_1_pawns.pgn sacgames_2_pawns.pgn sacgames_3_pawns.pgn \
         sacgames_4_pawns.pgn sacgames_5_pawns.pgn sacgames_queensacs.pgn \
         games_with_sacrifices.pgn"

# The human-readable report a case produced, if any.
report_of() {
    for r in statistics_EAS_ratinglist.txt statistics.txt; do
        [ -f "$1/$r" ] && { echo "$1/$r"; return; }
    done
}

echo "== build"
"$ROOT/build.sh" spct >/dev/null || { echo "FAIL: build spct"; exit 1; }
"$ROOT/build.sh" smoke >/dev/null || { echo "FAIL: build smoke"; exit 1; }
SPCT="$ROOT/build/spct.exe"
SMOKE="$ROOT/build/smoke.exe"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fail=0
ran=0
for gdir in "$GOLDEN"/*/; do
    name="$(basename "$gdir")"
    pgn="$ROOT/tests/data/$name.pgn"
    if [ ! -f "$pgn" ]; then
        echo "== $name: SKIP (tests/data/$name.pgn not present)"
        continue
    fi
    have="$(sha256sum "$pgn" | cut -d' ' -f1)"
    if [ $UPDATE -eq 1 ]; then
        echo "$have" > "$gdir/input.sha256"
    elif [ "$have" != "$(cat "$gdir/input.sha256")" ]; then
        echo "== $name: SKIP (tests/data/$name.pgn differs from the golden input)"
        continue
    fi
    echo "== $name"

    # Core: in-process passes and the streaming corpus agree with pgn-extract.
    if (cd "$ROOT" && "$SMOKE" "$pgn" >"$TMP/smoke.txt"); then
        echo "   smoke            ok"
    else
        echo "   smoke            FAIL"; sed 's/^/     /' "$TMP/smoke.txt"; fail=1
    fi

    while read -r case args; do
        [ -z "$case" ] && continue
        case "$case" in \#*) continue ;; esac
        run="$TMP/$name.$case"
        mkdir -p "$run"
        cp "$pgn" "$run/$name.pgn"
        # Run from a scratch dir: outputs land there, and the name in the
        # report header is just "<name>.pgn" wherever the repo lives.
        # shellcheck disable=SC2086
        (cd "$run" && "$SPCT" $args "$name.pgn" >stdout.txt 2>stderr.txt)
        rc=$?
        got="$run/hashes"
        : >"$got"
        for f in $OUTPUTS; do
            [ -f "$run/$f" ] && (cd "$run" && sha256sum "$f") >>"$got"
        done
        want="$gdir/$case.sha256"
        if [ $UPDATE -eq 1 ]; then
            cp "$got" "$want"
            rep="$(report_of "$run")"
            [ -n "$rep" ] && cp "$rep" "$gdir/$case.statistics.txt"
            printf "   %-16s recorded\n" "$case"
        elif [ $rc -eq 0 ] && diff -q "$want" "$got" >/dev/null; then
            printf "   %-16s ok\n" "$case"
        else
            printf "   %-16s FAIL (exit %d)\n" "$case" $rc
            diff "$want" "$got" | sed 's/^/     /'
            rep="$(report_of "$run")"
            if [ -f "$gdir/$case.statistics.txt" ] && [ -n "$rep" ]; then
                diff "$gdir/$case.statistics.txt" "$rep" | head -20 | sed 's/^/     /'
            fi
            sed 's/^/     stderr: /' "$run/stderr.txt" | head -5
            fail=1
        fi
        ran=$((ran + 1))
    done <"$gdir/cases"
done

if [ $ran -eq 0 ]; then echo "no cases ran"; exit 1; fi
if [ $fail -ne 0 ]; then echo "FAILED"; exit 1; fi
echo "all $ran cases passed"
