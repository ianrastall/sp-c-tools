/*
 * SPCT streaming corpus: label every game in one in-process pass.
 *
 * corpus_load() streams a PGN once through pgn-extract (via the
 * spct_game_hook seam) and records each game's facts: result, length,
 * players, termination, pgn-extract's duplicate key, where its text sits in
 * the file, and - given a pattern dir - its material labels (sacrifice
 * depth, first-sacrifice ply, endgame reached, material imbalance). The
 * material labels come from pgn-extract's own -y/-z matcher, run per game
 * against in-memory pattern lists, so no matching logic is reimplemented.
 *
 * Reports then answer counts, length buckets, sacrifice categories and the
 * batch's -c/-D set operations in memory, and write selected games straight
 * from the source text (pgn-extract output is a fixpoint, so a game's slice
 * is byte-identical to what a filter pass would re-emit).
 */
#ifndef SPCT_CORPUS_H
#define SPCT_CORPUS_H

#define CORPUS_NAME_LEN 200

/* Sacrifice levels in chain order: 1..5 pawn units, then the queen. */
#define CORPUS_SAC_LEVELS 6

typedef struct {
    char white[CORPUS_NAME_LEN];
    char black[CORPUS_NAME_LEN];
    int  result;      /* +1 = 1-0, -1 = 0-1, 0 = 1/2-1/2, 2 = other/unknown */
    int  plies;       /* half-moves in the game (pgn-extract moves = (plies+1)/2) */
    int  ply_offset;  /* plies before the first move: 0, or more for a FEN start.
                       * pgn-extract's -bu/-bl and --plylimit count plies from
                       * move 1, i.e. ply_offset + plies. */
    char termination[128];

    /* pgn-extract's duplicate key (-D, -c): final + cumulative position hash. */
    unsigned long long hash_final, hash_cumul;

    /* Byte range of this game's text in the loaded file. */
    long text_off, text_len;

    /* Material labels, filled only when corpus_load gets a pattern dir. */
    /* Draws (bad-draw detection): */
    int mat_def_white;         /* white had a >=1 pawn-unit deficit (1_pawnsac_white) */
    int mat_def_black;         /* black had a >=1 pawn-unit deficit (1_pawnsac_black) */
    int reached_endgame_draw;  /* matches the no_endgame_draws -z pattern */
    /* Decisive games, for the winner's colour: */
    int sac_depth;     /* length of the matched chain 1,2,3,4,5,queen (0..6):
                        * the game is in the batch's results_optK file iff
                        * sac_depth >= level K (queen = 6) */
    int sac1_ply;      /* position index of the first 1-pawn-sac match, -1 if none */
    int no_endgame;    /* matches the no_endgame -z pattern (endgame reached) */
    int imbalance;     /* matches the imbalance -z pattern */
} CorpusGame;

/* Stream pgn_path once, filling an internal table. If pattern_dir is non-NULL
 * the material labels above are computed too (from data files in that dir).
 * On return *out points at the table (owned by the module, reused next call,
 * do not free) and *n is the count. Returns 0 on success. */
int corpus_load(const char *pgn_path, const char *pattern_dir,
                CorpusGame **out, int *n);

/* pgn-extract includes a game under -buN iff its move count <= N, and the
 * move count is (plies+1)/2, so the game qualifies iff plies <= 2*N. Pass
 * ply_offset + plies for a FEN-start game. */
#define CORPUS_MOVES_LE(plies, n) ((plies) <= 2 * (n))

/* Append the text of games idx[0..n) (corpus indices, in that order) to the
 * file at path, exactly as a pgn-extract pass writing them would. */
void corpus_append(const char *path, const int *idx, int n);

/* The batch's "-c exclude -D -o out in": keep the games of in[0..n) whose
 * duplicate key is not shared by any game in exclude[0..ne) and is not a
 * repeat of an earlier kept game. Writes to out (room for n), returns count.
 * out may alias in. */
int corpus_dedup(const int *in, int n, const int *exclude, int ne, int *out);

/* The shared sacrifice classifier (EAS, IWS): given the winner lists W
 * (white wins) and B (black wins), build the batch's per-level result sets
 * (W's matches, then B's) and de-duplicate each against the next level up,
 * so every game lands only in its highest category. list[k]/n[k] for
 * k = 0..5 are categories 1,2,3,4,5,queen. */
typedef struct {
    int *list[CORPUS_SAC_LEVELS];
    int n[CORPUS_SAC_LEVELS];
} CorpusSacSets;
void corpus_sac_classify(const int *W, int nW, const int *B, int nB,
                         CorpusSacSets *out);
void corpus_sac_free(CorpusSacSets *s);

#endif /* SPCT_CORPUS_H */
