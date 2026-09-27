/*
 * SPCT streaming corpus: label every game in one in-process pass.
 *
 * This is the first step of moving off multi-pass pgn-extract orchestration.
 * Instead of running dozens of result/length/player filter passes and
 * counting the resulting files, corpus_load() streams the PGN once through
 * pgn-extract (via the spct_game_hook seam) and records each game's
 * parameter-free facts. Reports then answer win/draw counts, length buckets,
 * averages and player enumeration in memory.
 *
 * Only parameter-free labels live here for now (result, length, players,
 * termination). Material labels (sacrifice depth, endgame, imbalance) still
 * come from pgn-extract's -y/-z passes; they are the next label to migrate,
 * and a `sac_category` field is reserved for that.
 */
#ifndef SPCT_CORPUS_H
#define SPCT_CORPUS_H

#define CORPUS_NAME_LEN 200

typedef struct {
    char white[CORPUS_NAME_LEN];
    char black[CORPUS_NAME_LEN];
    int  result;      /* +1 = 1-0, -1 = 0-1, 0 = 1/2-1/2, 2 = other/unknown */
    int  plies;       /* half-moves in the game (pgn-extract moves = (plies+1)/2) */
    char termination[64];

    /* Material labels, filled only when corpus_load gets a pattern dir. They
     * reuse pgn-extract's own -y/-z matcher per game (no reimplementation).
     * For now these are computed only where a report needs them. */
    int mat_def_white;         /* white had a >=1 pawn-unit deficit (1_pawnsac_white) */
    int mat_def_black;         /* black had a >=1 pawn-unit deficit (1_pawnsac_black) */
    int reached_endgame_draw;  /* matches the no_endgame_draws -z pattern */
    int sac_category;          /* reserved: winner's sac depth (1-5, 9=queen) */
} CorpusGame;

/* Stream pgn_path once, filling an internal table. If pattern_dir is non-NULL
 * the material labels above are computed too (from data files in that dir).
 * On return *out points at the table (owned by the module, reused next call,
 * do not free) and *n is the count. Returns 0 on success. */
int corpus_load(const char *pgn_path, const char *pattern_dir,
                CorpusGame **out, int *n);

/* pgn-extract includes a game under -buN iff its move count <= N, and the
 * move count is (plies+1)/2, so the game qualifies iff plies <= 2*N. */
#define CORPUS_MOVES_LE(plies, n) ((plies) <= 2 * (n))

#endif /* SPCT_CORPUS_H */
