/*
 * SPCT core: pgn-extract driven in-process.
 *
 * pgn-extract is a command-line program: one invocation, one pass over
 * some PGN, then process exit clears everything. SPCT needs to run many
 * such passes inside a single long-lived process (the EAS tool alone runs
 * ~30 per engine). pgnx_run() executes one pass given an argv vector
 * exactly as the command line would, and pgnx_prepare_pass() resets every
 * piece of accumulating global/static state between passes.
 *
 * This file is the only public surface the SPCT tools use to reach the
 * vendored, lightly-patched pgn-extract (see vendor/pgn-extract/PATCHES.md).
 */
#ifndef SPCT_PGNX_H
#define SPCT_PGNX_H

/* Run one pass. argv is a NULL-free vector of length argc, argv[0] is the
 * program name (ignored), the rest are pgn-extract flags/filenames exactly
 * as on a command line. Returns 0 on success. After it returns,
 * pgnx_games_matched()/pgnx_games_processed() report that pass's counts.
 */
int pgnx_run(int argc, char *argv[]);

/* Convenience: run a pass from a NULL-terminated argv (argv[0]="pgn-extract"
 * supplied automatically is NOT done here - pass your own argv[0]).
 */
int pgnx_runv(char *const argv[]);

/* Reset all accumulating state for a fresh pass. Called by pgnx_run; also
 * safe to call directly. One-time table setup happens on first call. */
void pgnx_prepare_pass(void);

/* Close the per-pass output files (-o/-a/-n/-d). Called by pgnx_run at the
 * end of each pass so descriptors are freed and output is flushed. */
void pgnx_finish_pass(void);

/* Counts from the most recent pass. */
unsigned long pgnx_games_matched(void);
unsigned long pgnx_games_processed(void);

#endif /* SPCT_PGNX_H */
