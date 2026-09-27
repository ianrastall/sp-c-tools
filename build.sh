#!/usr/bin/env bash
# SPCT build script (MSYS2 UCRT64 / mingw gcc).
#
# Usage:  ./build.sh [target]
#   target defaults to "spct" (the unified tool). "smoke" builds the core
#   self-test. Produces build/<target>.exe
#
# Notes:
#   - pgn-extract uses POSIX regex; mingw has no libc regex, so we MUST
#     link -lregex (and -lm). If missing, GNU ld fails SILENTLY with
#     "ld returned 5" and emits a garbage exe.
#   - The vendored pgn-extract is a lightly patched copy; see
#     vendor/pgn-extract/PATCHES.md.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${1:-spct}"
VENDOR="$ROOT/vendor/pgn-extract"
CORE="$ROOT/core"
OBJ="$ROOT/build/obj"
BIN="$ROOT/build"
CC="${CC:-gcc}"
CFLAGS="-O2 -std=c99 -w -I$VENDOR -I$CORE"
LIBS="-lm -lregex"

mkdir -p "$OBJ" "$BIN"

case "$TARGET" in
    spct)  TOOL_SRCS="tools/spct/spct.c tools/eas/eas.c tools/iws/iws.c" ;;
    smoke) TOOL_SRCS="tools/smoke/smoke.c" ;;
    *) echo "unknown target '$TARGET' (try: spct, smoke)" >&2; exit 1 ;;
esac

VENDOR_UNITS="grammar lex map decode moves lists apply output eco lines end \
              main hashing argsfile mymalloc fenmatcher taglines zobrist \
              csvreader playerhashtable"

objs=()
for u in $VENDOR_UNITS; do
    "$CC" $CFLAGS -c "$VENDOR/$u.c" -o "$OBJ/$u.o"
    objs+=("$OBJ/$u.o")
done
for c in pgnx pgnu corpus; do
    "$CC" $CFLAGS -c "$CORE/$c.c" -o "$OBJ/$c.o"
    objs+=("$OBJ/$c.o")
done
for src in $TOOL_SRCS; do
    base="$(basename "$src" .c)"
    "$CC" $CFLAGS -c "$ROOT/$src" -o "$OBJ/$base.o"
    objs+=("$OBJ/$base.o")
done

"$CC" "${objs[@]}" $LIBS -o "$BIN/$TARGET.exe"
echo "Built $BIN/$TARGET.exe"
