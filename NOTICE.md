# SPCT — credits and licensing

SPCT (Stefan Pohl Chess Tools, C port) reimplements Stefan Pohl's computer-chess
analysis tools as native C programs built on a vendored, in-process copy of
pgn-extract.

## Upstream authors

- **The tools' design, algorithms, and scoring** (EAS, IWS, SGS, SGA, GamePairs,
  DecisionTimeStats) are by **Stefan Pohl (SPCC)**, https://www.sp-cc.de/ .
  This project is a faithful C reimplementation of that work, offered to Stefan.
- **pgn-extract** © **David J. Barnes** — GPLv3. Vendored under
  `vendor/pgn-extract/` (lightly patched; see `vendor/pgn-extract/PATCHES.md`).
- **Ordo / ordoprep** © **Miguel A. Ballicora** — GPLv3. Used (where needed, by
  the GamePairs tool) as an external executable, not linked.
- The **pgn-tools** utilities (nameList, summary, tagCreate, …) © **Norman
  Pollock**. The C port reimplements the small pieces it needs natively rather
  than shelling out to them.

## License

Because SPCT links pgn-extract's GPLv3 source, the combined work is licensed
under the **GNU General Public License v3.0** (see `LICENSE`). This is fully
compatible with the two intended outcomes for this project: a public repository
offered to Stefan Pohl, or private personal use. It cannot be made closed-source.

Material-matching note: Stefan's released tools ship pgn-extract **v24-11**;
this port vendors **v26-04**. The `-y`/`-z` material-match semantics are stable
across these versions in the cases validated so far, but exact byte-for-byte
parity with the released tools should be confirmed with Stefan before any public
release.
