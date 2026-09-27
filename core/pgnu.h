/*
 * SPCT shared utilities: the common ground under every report.
 *
 * All of Stefan Pohl's tools are the same pipeline - ingest a PGN, label
 * each game with facts (sacrifice depth, length, endgame reached, material
 * imbalance, ...), then aggregate or filter those labels. The labelling
 * lives in core/corpus.h (one streaming pass). This module holds the rest
 * of the shared plumbing: locating data files, the work directory, running
 * the remaining whole-file pgn-extract passes (the initial clean-up, the
 * length sort and the final de-duplication of each output), and annotation.
 */
#ifndef SPCT_PGNU_H
#define SPCT_PGNU_H

#include <stddef.h>

/* Locate SPCT's data/ directory (patterns/, anno/, ...): $SPCT_DATA if set,
 * else ./data, else data/ next to the executable or up to two levels above
 * it (so build/spct.exe finds the repo's data/ from any working directory).
 * Exits with a message if none contains the pattern files. Cached. */
const char *pgnu_data_dir(void);

/* One-time setup: the working directory for intermediate PGNs. Creates it. */
void pgnu_init(const char *work_dir);

/* Stable path for a fixed intermediate filename inside the work dir. The
 * same name always returns the same buffer (safe to hold many live). */
const char *pgnu_wp(const char *name);

/* Run one in-process pgn-extract pass from a NULL-terminated variadic arg
 * list (argv[0] is supplied). Returns the number of games matched. */
unsigned long pgnu_run(char *a0, ...);

/* Count games in a PGN file (occurrences of the White tag). */
long pgnu_count(const char *path);

/* Create or empty a file. */
void pgnu_truncate(const char *path);

/* Build "prefix+value" (e.g. flag("-Tw","Stockfish"), flag("-a",path)) in a
 * transient ring buffer. */
char *pgnu_flag(const char *prefix, const char *value);

/* Length-sort src into dst ascending, using the given move-length buckets
 * (lo[i]==0 means no lower bound, hi[i]==0 means no upper bound). */
void pgnu_sortlength(const char *src, const char *dst,
                     const int *lo, const int *hi, int n);

/* Read a one-line annotator tag from <anno_dir>/<name>. */
void pgnu_read_anno(const char *anno_dir, const char *name, char *out, size_t n);
/* Append each game of src to dst (append mode), inserting annoline as the
 * last header tag (native replacement for Pollock tagCreate). */
void pgnu_annotate_append(const char *src, const char *annoline, const char *dst);

#endif /* SPCT_PGNU_H */
