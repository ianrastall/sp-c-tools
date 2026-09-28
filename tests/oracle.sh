#!/usr/bin/env bash
# Run one of Stefan Pohl's original batch tools as an oracle (Windows only).
#
# Usage:
#   tests/oracle.sh [--pgn-extract EXE] TOOL_DIR BATCH PGN OUT_DIR ANSWER...
#
#   TOOL_DIR  a folder of Stefan's release, e.g.
#             archive/Sacrifice_Games_Search_Tool (not in the repo: use your
#             own copy of his tools, see NOTICE.md)
#   BATCH     the .bat inside it, e.g. Sacrifice_Games_Search_Tool_V3.2.bat
#   PGN       the games file; copied in as-is, so answer with its base name
#   OUT_DIR   receives every file the batch wrote next to itself, plus
#             console.txt
#   ANSWER... one per `set /p` prompt, in order ("" for just Return)
#
#   --pgn-extract EXE  replace the release's bin/pgn-extract.exe, e.g. with a
#             stock build of the version SPCT vendors (v26-04), so that a
#             comparison isolates the port from pgn-extract version changes.
#
# The batch runs in a scratch copy of TOOL_DIR with its sound helper
# silenced. NoDefaultCurrentDirectoryInExePath is cleared for the run
# because the batches call pgn-extract from their current directory.
set -euo pipefail

PE=""
if [ "${1:-}" = "--pgn-extract" ]; then PE="$2"; shift 2; fi
if [ $# -lt 4 ]; then
    sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
TOOL_DIR="$1"; BATCH="$2"; PGN="$3"; OUT="$4"; shift 4

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp -r "$TOOL_DIR"/. "$WORK"/
cp "$PGN" "$WORK"/
[ -n "$PE" ] && cp "$PE" "$WORK/bin/pgn-extract.exe"
printf '@exit /b 0\r\n' > "$WORK/bin/playwav.cmd"
ls -1 "$WORK" > "$WORK/.before"

# Answers, then one more line for the closing `pause`.
: > "$WORK/input.txt"
for a in "$@" ""; do printf '%s\r\n' "$a" >> "$WORK/input.txt"; done

# A wrapper .cmd avoids quoting a compound command through MSYS. System32
# goes first on PATH: under Git Bash, `find` would otherwise be the Unix
# find, and the batches' `find /C "[White " file` would walk all of C:\.
printf '@echo off\r\nset NoDefaultCurrentDirectoryInExePath=\r\nset "PATH=%%SystemRoot%%\\System32;%%PATH%%"\r\ncd /d "%%~dp0"\r\ncall "%%~dp0%s" < "%%~dp0input.txt"\r\n' \
    "$BATCH" > "$WORK/run.cmd"
echo run.cmd >> "$WORK/.before"
cmd.exe //c "$(cygpath -w "$WORK/run.cmd")" > "$WORK/console.out" 2>&1 || true

mkdir -p "$OUT"
tr -d '\r' < "$WORK/console.out" > "$OUT/console.txt"
for f in "$WORK"/*; do
    b="$(basename "$f")"
    [ -f "$f" ] || continue
    grep -qxF "$b" "$WORK/.before" && continue
    case "$b" in input.txt|console.out) continue ;; esac
    cp "$f" "$OUT/"
done
