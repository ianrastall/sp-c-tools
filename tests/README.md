# Tests

`data/sample.pgn` is a tiny hand-made file committed for a smoke run. Large real
corpora are gitignored — drop your own engine/tournament PGNs into `tests/data/`
to exercise the tools, e.g.:

```bash
./build.sh smoke && ./build/smoke.exe tests/data/yourgames.pgn
./build/spct.exe eas tests/data/yourgames.pgn
```

`smoke` cross-checks the streaming corpus against pgn-extract's own counts and
exits non-zero on any mismatch.

## Regression check

```bash
tests/check.sh            # build, smoke, then every golden case
tests/check.sh --update   # re-record goldens after an INTENDED output change
```

For each corpus under `golden/<name>/`, it runs the report variants listed in
`golden/<name>/cases` on `data/<name>.pgn` in a scratch directory and compares
each output (reports, PGNs, stdout) to the recorded SHA-256 hashes. EAS reports
are also kept as readable `*.statistics.txt` snapshots, so a mismatch shows the
actual differences. Only hashes are stored for PGN outputs, so no game text from
local corpora is committed.

- `sample` — the committed toy file; always runs.
- `parity` — the 1071-game GM blitz corpus behind the oracle check below. It's
  gitignored, so this corpus is skipped unless your local copy matches
  `golden/parity/input.sha256`.
- `stress` — parity plus duplicate games, error terminations and FEN-start
  games, which parity lacks. Regenerate it with `tests/mkstress.py` (needs
  parity and a stock pgn-extract; see the script's header).

Run it before committing any change to the core, the patterns, or a report.
Output is expected to stay byte-identical unless a change is meant to alter it.

## Oracle check (EAS)

EAS was validated by replaying the original batch's exact pgn-extract pass
sequence for one engine with a stock pgn-extract build and comparing raw counts
(wins, short-win buckets, the sacrifice-detection + dedup chain) to the port's
output. They matched exactly on a real GM blitz tournament (1071 games, 116
players). Re-run that check whenever the scoring or pass logic changes.
