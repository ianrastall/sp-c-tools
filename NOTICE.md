# SPCT — credits and licensing

SPCT (Stefan Pohl Chess Tools, C port) reimplements Stefan Pohl's computer-chess
analysis tools as native C programs built on a vendored, in-process copy of
pgn-extract.

## Copyright

**Engine Aggressiveness Statistics Tool, Interesting Wins Search Tool, Sacrifice
Games Search Tool, Short Games Analyzer Tool, GamePairs rescoring tool,
DecisionTimeStats: idea, design, algorithms and scoring
(C) 2024-2025, Stefan Pohl (SPCC), https://www.sp-cc.de/**

This project is a faithful C reimplementation of that work, offered to Stefan.
The pattern and annotator files under `data/` come from his tools' releases.
His original tools themselves (batch files, binaries, manuals) are **not** part
of this repository; get them from https://www.sp-cc.de/ .

## Other upstream authors

- **pgn-extract** © **David J. Barnes** — GPLv3 or later. Vendored under
  `vendor/pgn-extract/` (lightly patched; see `vendor/pgn-extract/PATCHES.md`).
- **Ordo / ordoprep** © **Miguel A. Ballicora** — GPLv3. Used (where needed, by
  the GamePairs tool) as an external executable, not linked.
- The **pgn-tools** utilities (nameList, summary, tagCreate, …) © **Norman
  Pollock**. The C port reimplements the small pieces it needs natively rather
  than shelling out to them.

## License

Stefan's tools carry a copyright notice but no license grant. This port keeps
his copyright notice everywhere his work is reproduced (`LICENSE`, this file,
the README, and the report sources and output).

The code is licensed under the **GNU General Public License, version 3 or (at
your option) any later version** (see `LICENSE`). SPCT links pgn-extract's GPL
source, and the GPL requires the combined work to be distributed under the same
terms, so no other license is possible for a distributable build. That covers
both intended outcomes for this project: a public repository offered to Stefan
Pohl, or private personal use. It cannot be made closed-source.

Material-matching note: Stefan's released tools ship pgn-extract **v24-11**;
this port vendors **v26-04**. The `-y`/`-z` material-match semantics are stable
across these versions in the cases validated so far, but exact byte-for-byte
parity with the released tools should be confirmed with Stefan before any public
release.
