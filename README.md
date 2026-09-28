# SPCT — Stefan Pohl Chess Tools (C port)

*Idea, design, algorithms and scoring (C) 2024-2025, Stefan Pohl (SPCC),
[www.sp-cc.de](https://www.sp-cc.de/). pgn-extract (C) David J. Barnes.*

Native C reimplementation of [Stefan Pohl's](https://www.sp-cc.de/) computer-chess
analysis tools. The originals are separate Windows batch scripts that shell out to
[pgn-extract](https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/) (and
Norman Pollock's pgn-tools) dozens of times per run, passing PGN subsets through
temp files. That separation was a scripting artifact: underneath, the tools are
one pipeline — **ingest a PGN, label each game with facts (sacrifice depth,
length, endgame reached, material imbalance, …), then aggregate or filter those
labels.** So this is one binary, `spct`, with a subcommand per report, over a
shared classification core with pgn-extract folded in and driven **in-process**.

Status: **EAS, IWS, SGS (including the SGS comfort tool), SGA and GamePairs
are ported** (as `spct eas`, `spct iws`, `spct sgs [--comfort]`,
`spct sga [--no-endgame]`, `spct gamepairs`). DecisionTimeStats already exists
separately as a C# tool.

## Architecture

```
vendor/pgn-extract/   pgn-extract 26-04, GPLv3, lightly patched (see PATCHES.md)
                      to be driven in-process, once per "pass", many times.
core/pgnx.[ch]        Runs one pgn-extract "pass" (== one command line) in
                      process with full state reset; pgnx_games_matched() counts.
core/corpus.[ch]      Streaming labels: one pass over the PGN (via the
                      spct_game_hook) records each game's result, length,
                      players, duplicate key and material labels (sacrifice
                      depth and first-sac ply, endgame reached, imbalance -
                      from pgn-extract's own -y/-z matcher, run in memory).
                      Also the batch's -c/-D set operations and the shared
                      sacrifice classifier, over index lists, and copying
                      selected games straight from the source text.
core/pgnu.[ch]        Shared plumbing: data-dir lookup, work paths, the pass
                      wrapper for the few remaining whole-file passes
                      (clean-up, length sort, final dedup), annotation.
tools/spct/spct.c     Front-end: dispatches <command> to a report.
tools/eas/eas.c       EAS report: engine-aggressiveness scoring, rating lists.
tools/iws/iws.c       IWS report: filter spectacular wins into two tiers.
tools/sgs/sgs.c       SGS report: sacrifice games by depth, plus statistics.
tools/sga/sga.c       SGA report: short games and short sacs per engine.
tools/gp/gp.c         GamePairs: score engine matches by opening pairs.
data/patterns/        -y/-z material-pattern files (sacrifice / endgame /
                      imbalance detection).
data/anno/            EAS annotator-tag and termination-filter files.
data/anno_iws/        IWS annotator-tag files.
```

pgn-extract already contains the hard part (a correct move parser with board and
material tracking, and the `-y`/`-z` material matcher). The reports reuse that
engine and the shared classifiers, adding only their aggregation/output. No
subprocess spawning, no external `.exe`s, and no per-engine filter passes: EAS
labels every game once and answers each engine from memory (about 8x faster
than the multi-pass version on a 1071-game, 116-player file).

## Build (Windows, MSYS2 UCRT64)

Requires `gcc` and the `regex` library (pgn-extract uses POSIX regex, which
mingw's libc lacks — the build links `-lregex`).

**Double-click `build.bat`** to build everything. It links `spct.exe`
statically (no MSYS2 DLLs needed at run time) and runs the smoke test. It then
stages a portable `dist\SPCT\` (the exe, `data\` and the docs), checks that the
staged exe runs from an unrelated folder, and, if Inno Setup 6 is installed,
builds `dist\SPCT-Setup-<version>.exe`. It finds MSYS2 through `gcc` on PATH,
`MSYS2_ROOT` or `C:\msys64`. `build.bat check` runs the regression tests first.

The installer installs for the current user by default (no admin rights;
all-users is offered). It can add SPCT to PATH, adds Start-menu entries (a
command prompt with `spct` ready, the README, uninstall), and its uninstaller
removes all of that, restoring PATH exactly. The version comes from the
`VERSION` file (`spct --version`).

For development builds:

```bash
./build.sh            # -> build/spct.exe (dynamically linked)
SPCT_STATIC=1 ./build.sh   # static, as build.bat does
```

There is also a `CMakeLists.txt` for IDE/CMake builds.

`spct` reads its pattern/annotation files from `data/`. It looks for `$SPCT_DATA`,
then `./data`, then `data/` beside the executable or up to two levels above it
(so `build/spct.exe` works from any directory); CMake builds also fall back to
the source tree's `data/`. Reports are written to the current directory.

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

All three files are byte-identical to what `EAS_Tool_V6.0.bat` writes, quirks
included. The medal table is ordered as Windows `sort` orders its text (so
`33.33%` ranks above `100.0%`). `interesting_wins.pgn` takes its before-endgame
and material-imbalance games from the last player processed only, as the batch
does.

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

## Run SGS

```bash
./build/spct.exe sgs yourgames.pgn [--level L] [--moves N]
# or, interactively (the batch's three questions):
./build/spct.exe sgs
```

Finds the won games in which the loser held a material advantage for several
consecutive moves, i.e. the winner sacrificed. Comments, NAGs and variations are
kept.
- `--level 0` (default): full search. Each game goes to its highest category,
  `sacgames_1_pawns.pgn` … `sacgames_5_pawns.pgn` or `sacgames_queensacs.pgn`,
  and `statistics.txt` gets the counts.
- `--level 1`..`5`, or `9` for queen sacrifices: one search for that many pawn
  units or more. The games go to `games_with_sacrifices.pgn` (white wins, then
  black wins), with no statistics.
- `--moves N`: the longest won game to consider (default 80; below 40 means 80,
  and 250 is the maximum). Wins shorter than 15 moves are never considered.

Note: the port counts games as parsed games. The batch counted lines containing
`[White ` (`find /C`), which also counts any comment that quotes such a tag. On
ordinary files the numbers are identical.

### SGS comfort tool

```bash
./build/spct.exe sgs --comfort yourgames.pgn [--moves N]
```

The port of `SGS_Comfort_Tool_V1.2.bat`. It looks for sacrifices of 2+ pawn
units only, strips comments, NAGs and variations, and puts
`{SGS-tool: sac found}` (or `Queen sac found`) in front of the move where it
places the sacrifice. As Stefan's readme says, that placement is a heuristic:
a game gets one comment, and games with several sacrifices can be marked at
the wrong one. It writes `sacgames_2_pawns.pgn`, `sacgames_3_pawns.pgn`,
`sacgames_5_pawns.pgn`, `sacgames_queensacs.pgn` and `statistics.txt`. The
batch edits every game as text, one at a time. The port computes the same
positions from the per-game labels, so a file the batch needs minutes for takes
a second or two.

## Run SGA

```bash
./build/spct.exe sga yourgames.pgn [--no-endgame]
```

The Short Games Analyzer. For every engine/player it reports how many of its
wins, draws and losses were short (up to 30, 35, 40, … 60 moves), how many of the
short wins contain a sacrifice, and its score, draw rate and average game
lengths. It writes:
- `SGA_full_statistics.txt` — everything, ranked by short wins minus short
  draws minus short losses
- `SGA_wins_statistics.txt`, `SGA_draws_statistics.txt`,
  `SGA_losses_statistics.txt` — ranked by short wins (most first), short draws
  and short losses (fewest first)
- `shortgames.pgn` — all short decisive games, then all short draws, comments
  kept
- `short_sac_games.pgn` — the short wins with a sacrifice, queen sacs first

`--no-endgame` is `Short_Games_Analyzer_no_endgames_V3.3.bat`: it counts only
short games that ended before an endgame, and writes the same files with
`_no_endgame` in their names. Players are found and ordered the way Pollock's
`nameList` does it, and average lengths are computed as `summary.exe` does
(see the EAS note under Validation), so no external tools are needed.

## Run GamePairs

```bash
./build/spct.exe gamepairs yourmatch.pgn --plies 16 --ref-engine "Stockfish 17"     [--ref-elo 3500] [--ordo PATH | --no-ordo]
```

The gamepair rescorer (`Auto_Gamepairs_Rescorer_V1.6.bat`). For engine matches
played with each opening twice, once with each colour, it scores each pair of
games as one result: 2-0 and 1.5-0.5 count as a win, 1-1 as a draw. It writes
the pairs to `Gamepairs_final.pgn` and rates them with Ordo into
`Gamepairs_rating.txt` and `Gamepairs_head-to-head.txt`. `--plies` is the
length of the opening lines (the batch's `opening_plies`). `--ref-engine` and
`--ref-elo` are Ordo's anchor; run without a file, it asks for all four.

As the batch did, it runs Ordo as an external program: `--ordo PATH`, else
`$SPCT_ORDO`, else the `ordo-win64.exe` installed next to `spct.exe` (the
installer ships Ordo 1.2.6; `build.bat` bundles it from `ORDO_EXE` or your copy
of Stefan's GamePairs tool), else `ordo-win64.exe` or `ordo.exe` on PATH. Without Ordo (or with
`--no-ordo`) it still writes the pairs and their counts. Like the batch, it
handles at most 995 head-to-head pairings.

## Validation

`tests/check.sh` is the regression gate: it re-runs every report variant on the
test corpora and compares all outputs to recorded goldens (see `tests/README.md`).

`tests/oracle.sh` runs one of Stefan's original batch tools (from your own copy
of his release) on a file, optionally with a different `pgn-extract.exe`. With a
stock build of the pgn-extract version SPCT vendors, a comparison isolates the
port itself. SGS and the comfort tool match the batches byte for byte on every
output, over every level, move limit and prompt edge case. The corpora were GM
blitz, the same plus duplicates, error terminations and FEN starts, a heavily
commented variant, an edge set (truncated and FEN-cut sacrifice games, `*` in
tags, UTF-8 names), 15,000 engine games and a mega-database sample with queen
sacrifices. The exceptions are the counting note above and the tag-order note
below.

GamePairs matches the batch byte for byte on all three outputs, Ordo's
rating list and head-to-head included, using the same Ordo 1.2.6 the batch
ships. The test input was a synthetic tournament with every pair outcome,
openings played once or three times, games shorter than the opening, unfinished
games, and engine names that are prefixes of each other, cut at 8, 16 and 20
plies. It also matches on a 2,000-game engine round robin (738 pairings),
including when Ordo refuses to run.

SGA (both variants) matches the batches byte for byte on every output, all four
rating lists and both PGN files: on GM blitz, on its variant with duplicates,
error terminations and FEN starts, on the commented corpus, and on a file with
non-standard tags.

EAS matches the batch byte for byte on all three outputs, the rating-list
report included: on GM blitz, on its variant with duplicates, error terminations
and FEN starts, and on two tiny files. The report's line text is generated from
the batch itself (`tools/eas/eas_templates.h`), and its lists are ordered the
way Windows `sort.exe` orders them. `pgnu_sort_compare` reproduces that: the
user locale's case-insensitive string sort of lines read in the console code
page, measured against `sort.exe` character by character. Like the batch, the
port counts a player's wins on their de-duplicated, error-free wins, and
processes players in `nameList` order.

Average game lengths (EAS, SGA) follow the batch exactly. The batch reads the
mean ply count from Pollock's `summary.exe`, which prints it rounded half up to
two decimals with trailing zeros dropped. The batch rounds up only when the two
digits after the point are 50 or more, so a mean printed as `93.5` or `93.7` is
not rounded up. The port reproduces that.

See `NOTICE.md` for the v24-11 vs v26-04 caveat. Besides material matching, the
versions order some tags differently (e.g. `WhiteFideId`/`BlackFideId`), so
games written by the port can differ from the released tools' in tag order.

Tag order can also differ for tags pgn-extract doesn't know (e.g. ChessBase's
`WhiteTeamCountry`). pgn-extract prints those in the order it first met them.
The batches run one pgn-extract per step, so that order depends on whichever
intermediate file the last step read. The port lists them in the order they
first appear in your source file. The tags and their values are the same either
way; on ChessBase Magazine data, sorting each game's tags makes every SGS and
comfort output identical to the batch's.

## License

The tools' idea, design, algorithms and scoring are (C) 2024-2025, Stefan Pohl
(SPCC), www.sp-cc.de. The code is GPLv3 or later (see `LICENSE` and
`NOTICE.md`), as required because it links pgn-extract's GPL source.
