# SPCT — Stefan Pohl Chess Tools (C port)

Native C reimplementation of [Stefan Pohl's](https://www.sp-cc.de/) computer-chess
analysis tools. The originals are separate Windows batch scripts that shell out to
[pgn-extract](https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/) (and
Norman Pollock's pgn-tools) dozens of times per run, passing PGN subsets through
temp files. That separation was a scripting artifact: underneath, the tools are
one pipeline — **ingest a PGN, label each game with facts (sacrifice depth,
length, endgame reached, material imbalance, …), then aggregate or filter those
labels.** So this is one binary, `spct`, with a subcommand per report, over a
shared classification core with pgn-extract folded in and driven **in-process**.

Status: **EAS and IWS are ported and validated** (as `spct eas` / `spct iws`).
Others (SGS, SGA, GamePairs) are planned; DecisionTimeStats already exists
separately as a C# tool.

## Architecture

```
vendor/pgn-extract/   pgn-extract 26-04, GPLv3, lightly patched (see PATCHES.md)
                      to be driven in-process, once per "pass", many times.
core/pgnx.[ch]        Runs one pgn-extract "pass" (== one command line) in
                      process with full state reset; pgnx_games_matched() counts.
core/pgnu.[ch]        Shared machinery over pgnx: work paths, the pass wrapper,
                      length-sort, annotate, and the sacrifice classifier that
                      every report uses.
core/corpus.[ch]      Streaming labels: one pass over the PGN (via the
                      spct_game_hook) records each game's result, length and
                      players in memory, so count/length/player queries need no
                      filter passes. (First step off multi-pass orchestration;
                      material labels still come from pgn-extract -y/-z.)
tools/spct/spct.c     Front-end: dispatches <command> to a report.
tools/eas/eas.c       EAS report: engine-aggressiveness scoring, rating lists.
tools/iws/iws.c       IWS report: filter spectacular wins into two tiers.
data/patterns/        -y/-z material-pattern files (sacrifice / endgame /
                      imbalance detection).
data/anno/            EAS annotator-tag and termination-filter files.
data/anno_iws/        IWS annotator-tag files.
```

pgn-extract already contains the hard part (a correct move parser with board and
material tracking, and the `-y`/`-z` material matcher). The reports reuse that
engine and the shared classifiers, adding only their aggregation/output. No
subprocess spawning, no temp-file shuffling between processes, no external `.exe`s.

## Build (Windows, MSYS2 UCRT64)

Requires `gcc` and the `regex` library (pgn-extract uses POSIX regex, which
mingw's libc lacks — the build links `-lregex`).

```bash
./build.sh            # -> build/spct.exe
```

There is also a `CMakeLists.txt` for IDE/CMake builds.

## Run EAS

```bash
./build/spct.exe eas yourgames.pgn
# or, interactively:
./build/spct.exe eas
```

Writes:
- `statistics_EAS_ratinglist.txt` — three rating lists + a single-stats medal table
- `interesting_wins.pgn` — the spectacular games (sacrifices, very short wins,
  wins before the endgame, material imbalances), length-sorted within each
  category and tagged with an `[Annotator "EAS-Tool: …"]` line
- `errorgames.pgn` — games with non-regular terminations

Options:
- `--gauntlet` — evaluate only the engine that played the most games (for
  engine devs testing one version against many opponents)
- `--hardavg N` — hardcode the average won-game length (batch's `hard_moveaverage`)
- `--work=DIR` — directory for intermediate files

Note: the batch computed the before-endgame / imbalance categories of
`interesting_wins.pgn` from only the last engine processed; this port computes
them across all engines (its evident intent). The EAS-Scores are unaffected.

## Run IWS

```bash
./build/spct.exe iws yourgames.pgn [--moves N] [--player NAME]
```

Filters the spectacular wins (no statistics) into two tiers:
- `interesting_wins.pgn` — queen/5/4/3/2/1 pawn sacs, wins before the endgame,
  material imbalances; sorted shortest-first within each category and annotated
- `very_interesting_wins.pgn` — the same minus 1-pawn sacs and imbalances

`--moves N` caps the game length considered (default 100; clamped to 30..250).
`--player NAME` restricts to that engine/player's wins. Runs in one pass over
the whole file — no per-engine loop.

## Validation

EAS was checked against a stock pgn-extract "oracle" that replays the batch's
exact pass sequence: win counts, short-win buckets, and the sacrifice-detection
+ dedup chain reproduce the port's numbers exactly on real GM tournament data.
See `NOTICE.md` for the v24-11 vs v26-04 material-match caveat.

## License

GPLv3 (see `LICENSE` and `NOTICE.md`) — the combined work links pgn-extract's
GPLv3 source.
