# Tests

`data/sample.pgn` is a tiny hand-made file committed for a smoke run. Large real
corpora are gitignored — drop your own engine/tournament PGNs into `data/` to
exercise the tools, e.g.:

```bash
./build/eas.exe tests/data/yourgames.pgn
```

## Oracle check (EAS)

EAS was validated by replaying the original batch's exact pgn-extract pass
sequence for one engine with a stock pgn-extract build and comparing raw counts
(wins, short-win buckets, the sacrifice-detection + dedup chain) to the port's
output. They matched exactly on a real GM blitz tournament (1071 games, 116
players). Re-run that check whenever the scoring or pass logic changes.
