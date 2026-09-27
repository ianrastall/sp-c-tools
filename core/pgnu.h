/*
 * SPCT shared utilities: the common ground under every report.
 *
 * All of Stefan Pohl's tools are the same pipeline - ingest a PGN, label
 * each game with facts (sacrifice depth, length, endgame reached, material
 * imbalance, ...), then aggregate or filter those labels. This module owns
 * the shared machinery so each report (EAS, IWS, SGS, SGA, ...) stays thin.
 *
 * It currently labels by delegating to the vendored in-process pgn-extract
 * (core/pgnx.h): -y/-z material patterns, -bu/-bl length bounds, result and
 * player tags. That is the validated baseline; individual labels can later
 * migrate to a single streaming pass (the spct_game_hook seam) behind the
 * same interface, one at a time, checked against the oracle.
 */
#ifndef SPCT_PGNU_H
#define SPCT_PGNU_H

#include <stddef.h>

/* Locate SPCT's data/ directory (patterns/, anno/, ...): $SPCT_DATA if set,
 * else ./data, else data/ next to the executable or up to two levels above
 * it (so build/spct.exe finds the repo's data/ from any working directory).
 * Exits with a message if none contains the pattern files. Cached. */
const char *pgnu_data_dir(void);

/* One-time setup: the working directory for intermediate PGNs and the
 * directory holding the -y/-z pattern files. Creates the work dir. */
void pgnu_init(const char *work_dir, const char *pattern_dir);

/* Stable path for a fixed intermediate filename inside the work dir. The
 * same name always returns the same buffer (safe to hold many live). */
const char *pgnu_wp(const char *name);
/* Path to a pattern file inside the pattern dir (transient buffer). */
const char *pgnu_pat(const char *name);

/* Run one in-process pgn-extract pass from a NULL-terminated variadic arg
 * list (argv[0] is supplied). Returns the number of games matched. */
unsigned long pgnu_run(char *a0, ...);

/* Count games in a PGN file (occurrences of the White tag). */
long pgnu_count(const char *path);

/* File helpers. */
void pgnu_copy(const char *dst, const char *src);
void pgnu_concat2(const char *dst, const char *a, const char *b);
void pgnu_truncate(const char *path);   /* create/empty a file */

/* Build "prefix+value" (e.g. flag("-Tw","Stockfish"), flag("-c",path)) and
 * "-buN", in transient ring buffers. */
char *pgnu_flag(const char *prefix, const char *value);
char *pgnu_bu(int n);

/* Length-sort src into dst ascending, using the given move-length buckets
 * (lo[i]==0 means no lower bound, hi[i]==0 means no upper bound). */
void pgnu_sortlength(const char *src, const char *dst,
                     const int *lo, const int *hi, int n);

/* Read a one-line annotator tag from <anno_dir>/<name>. */
void pgnu_read_anno(const char *anno_dir, const char *name, char *out, size_t n);
/* Append each game of src to dst (append mode), inserting annoline as the
 * last header tag (native replacement for Pollock tagCreate). */
void pgnu_annotate_append(const char *src, const char *annoline, const char *dst);

/* ---- shared sacrifice classifier ----
 * The sac search both EAS and IWS run: find games where the winner made a
 * material sacrifice, split by depth (1..5+ pawn units, and queen sacs).
 *
 * Step 1: search the 1+ pawn sacrifices. Narrows whitewins/blackwins IN
 * PLACE to the 1+sac games and writes their union to r1. (EAS needs this
 * split point for its early-sac bonus before continuing.)
 */
void pgnu_sac_1plus(const char *whitewins, const char *blackwins, const char *r1);

/* Step 2: search 2/3/4/5/queen sacrifices from the (narrowed) win files,
 * then de-duplicate so each game lands in its highest category. Fills
 * unique[0..5] (paths the caller owns) for categories 1,2,3,4,5,queen and
 * counts[0..5] with their game counts. r1 is the file from step 1. */
void pgnu_sac_rest(const char *whitewins, const char *blackwins,
                   const char *r1, const char *const unique[6], int counts[6]);

#endif /* SPCT_PGNU_H */
