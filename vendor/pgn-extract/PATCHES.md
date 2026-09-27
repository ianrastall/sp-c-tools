# Local patches to pgn-extract

This directory is a **vendored copy of pgn-extract 26-04** by David J. Barnes
(https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/), GPLv3. SPCT links
it in-process as a library rather than running the `pgn-extract` executable.

Every change to the upstream source is listed below so this remains an
auditable derivative. The changes are small and additive: they let the parser
be driven repeatedly inside one long-lived process (upstream relies on process
exit to release state and files) and expose a per-game classifier hook.

## Changes

1. **`main.c` — `main()` renamed to `pgnx_run(int, char**)`.**
   - The startup init block is replaced by a single `pgnx_prepare_pass()` call
     (defined in `core/pgnx.c`), which resets all accumulating state so the
     function can be called once per "pass" (one equivalent command line),
     many times in a process.
   - `pgnx_finish_pass()` is called before `return 0` to close the per-pass
     output files (`-o`/`-a`/`-n`/`-d`) that upstream leaves for process exit
     to close — otherwise we exhaust file descriptors after a few hundred
     passes and reads of just-written output are unflushed.
   - `#include "pgnx.h"` added.

2. **`lex.c` / `lex.h` — added `reset_input_source_list()`.**
   Frees the file-static source-file list (`list_of_files`), rewinds
   `current_file_num`, and clears `yyin`, so a new pass starts from an empty
   input list. The old list contents are intentionally leaked (passes are
   bounded and short-lived).

3. **`end.c` / `end.h` — added `reset_endings_to_match()`.**
   Sets the file-static material-pattern list head (`endings_to_match`) back to
   NULL so `-y`/`-z` patterns from a previous pass do not leak into the next.

4. **`moves.c` / `moves.h` — added `reset_games_to_keep()`.**
   Sets the file-static textual/positional variation list head
   (`games_to_keep`) back to NULL for the same reason.

5. **`grammar.c` / `grammar.h` — added the `spct_game_hook` classifier tap.**
   A global `void (*spct_game_hook)(const Game *)`, default NULL. When set, it
   is invoked in `deal_with_game()` after `apply_move_list()` has populated the
   board/material fields but before `free_tags()`/`free_move_list()` release the
   game, so both tags and the applied move list are valid. `core/corpus.c` uses
   it to label every game in one pass. It is a no-op while NULL.

6. **`end.c` / `end.h` — added `spct_build_material_list` and
   `spct_game_matches_material`.**
   The first reads a `-y`/`-z` pattern file into a STANDALONE `Material_details`
   list (by swapping the global `endings_to_match` around the existing
   `build_endings`, then restoring it). The second runs the existing static
   `look_for_material_match` against an explicit list instead of the global
   one (same swap-and-restore). Together they let a caller hold several pattern
   lists and test each game against them in memory, reusing pgn-extract's exact
   matcher, instead of running one filter pass per pattern over PGN subsets.
   No matching logic is reimplemented.

## Not changed

Everything else is upstream 26-04 verbatim, including the parser, board and
material tracking, and the `-y`/`-z` material matcher that SPCT relies on.

## Rebuilding the reference executable

The vendored source no longer has a `main()` (it is `pgnx_run`), so it builds
only as part of an SPCT tool. To build a stock `pgn-extract` for comparison,
use an unmodified upstream 26-04 tree.
