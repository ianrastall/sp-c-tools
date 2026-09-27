/*
 * SPCT streaming corpus. See core/corpus.h.
 * Uses the vendored pgn-extract's per-game hook (spct_game_hook) to label
 * each game in a single pass, with no intermediate files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bool.h"
#include "mymalloc.h"
#include "defs.h"
#include "typedef.h"
#include "tokens.h"
#include "taglist.h"
#include "lex.h"
#include "moves.h"
#include "apply.h"       /* new_game_board, free_board */
#include "end.h"         /* spct_build_material_list, spct_game_matches_material */
#include "grammar.h"     /* spct_game_hook */
#include "pgnx.h"
#include "corpus.h"

static CorpusGame *g_arr = NULL;
static int g_count = 0;
static int g_cap = 0;
static char g_path[1300];   /* the loaded file, for corpus_append */

/* Standalone material-pattern lists (built once per load from the pattern
 * dir). Sacrifice chain lists are indexed 0..5 = levels 1,2,3,4,5,queen. */
static Material_details *L_ne_draw = NULL;  /* no_endgame_draws (-z) */
static Material_details *L_sac[2][CORPUS_SAC_LEVELS]; /* [0]=white, [1]=black (-y) */
static Material_details *L_no_endgame = NULL;  /* no_endgame (-z) */
static Material_details *L_imbalance = NULL;   /* imbalance (-z) */
static int have_material = 0;

static void die(const char *msg)
{
    fprintf(stderr, "spct: %s\n", msg);
    exit(1);
}

static const char *cg_tag(const Game *g, int idx)
{
    if (g->tags != NULL && idx < g->tags_length) return g->tags[idx];
    return NULL;
}

static void cg_copy(char *dst, size_t n, const char *src)
{
    if (src == NULL) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

static void corpus_hook(const Game *g)
{
    if (g_count >= g_cap) {
        int ncap = g_cap ? g_cap * 2 : 8192;
        CorpusGame *na = (CorpusGame *) realloc(g_arr, (size_t)ncap * sizeof *na);
        if (na == NULL) {
            /* Dropping games would silently skew every statistic. */
            fprintf(stderr, "spct: out of memory labelling game %d\n", g_count + 1);
            exit(1);
        }
        g_arr = na;
        g_cap = ncap;
    }
    CorpusGame *c = &g_arr[g_count];

    cg_copy(c->white, sizeof c->white, cg_tag(g, WHITE_TAG));
    cg_copy(c->black, sizeof c->black, cg_tag(g, BLACK_TAG));
    cg_copy(c->termination, sizeof c->termination, cg_tag(g, TERMINATION_TAG));

    const char *r = cg_tag(g, RESULT_TAG);
    if (r != NULL && strcmp(r, "1-0") == 0) c->result = 1;
    else if (r != NULL && strcmp(r, "0-1") == 0) c->result = -1;
    else if (r != NULL && strcmp(r, "1/2-1/2") == 0) c->result = 0;
    else c->result = 2;

    int plies = 0;
    for (const Move *m = g->moves; m != NULL; m = m->next) plies++;
    c->plies = plies;

    /* pgn-extract numbers plies from move 1 of the FEN's move counter:
     * the first move is ply 2*move_number-1 (+1 if black is to move). */
    c->ply_offset = 0;
    const char *fen = cg_tag(g, FEN_TAG);
    if (fen != NULL) {
        Board *b = new_game_board(fen);
        c->ply_offset = 2 * (int)b->move_number - 2 + (b->to_move == BLACK ? 1 : 0);
        free_board(b);
    }

    c->hash_final = g->final_hash_value;
    c->hash_cumul = g->cumulative_hash_value;
    c->text_off = c->text_len = 0;

    c->mat_def_white = 0;
    c->mat_def_black = 0;
    c->reached_endgame_draw = 0;
    c->sac_depth = 0;
    c->sac1_ply = -1;
    c->no_endgame = 0;
    c->imbalance = 0;
    /* The spct_game_* matchers replay the game with pgn-extract's own -y/-z
     * matcher; casting away const is safe (they do not mutate it here). */
    Game *gm = (Game *) g;
    if (have_material && c->result == 0) {
        /* Draws: bad-draw detection. */
        c->reached_endgame_draw = spct_game_matches_material(gm, L_ne_draw) ? 1 : 0;
        c->mat_def_white = spct_game_matches_material(gm, L_sac[0][0]) ? 1 : 0;
        c->mat_def_black = spct_game_matches_material(gm, L_sac[1][0]) ? 1 : 0;
    }
    else if (have_material && (c->result == 1 || c->result == -1)) {
        /* Wins: the winner's sacrifice chain. The batch narrows the win
         * files level by level, so level K is tested only on games that
         * matched every level below it. */
        Material_details **L = L_sac[c->result == 1 ? 0 : 1];
        c->sac1_ply = spct_game_material_match_ply(gm, L[0]);
        if (c->sac1_ply >= 0) {
            c->sac_depth = 1;
            while (c->sac_depth < CORPUS_SAC_LEVELS &&
                   spct_game_matches_material(gm, L[c->sac_depth]))
                c->sac_depth++;
        }
        c->no_endgame = spct_game_matches_material(gm, L_no_endgame) ? 1 : 0;
        c->imbalance = spct_game_matches_material(gm, L_imbalance) ? 1 : 0;
    }

    g_count++;
}

/* Record each game's byte range. The file is pgn-extract output with
 * comments stripped, so a game starts exactly at a '[' line that follows a
 * non-tag line (or the start of the file), and runs to the next start. */
static void index_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) die("cannot reopen the corpus file");
    char buf[65536];
    size_t k;
    long pos = 0;
    int at_line_start = 1, prev_tag = 0, cur_tag = 0, found = 0;
    while ((k = fread(buf, 1, sizeof buf, f)) > 0) {
        for (size_t i = 0; i < k; i++, pos++) {
            if (at_line_start) {
                cur_tag = buf[i] == '[';
                if (cur_tag && !prev_tag) {
                    if (found < g_count) g_arr[found].text_off = pos;
                    found++;
                }
                at_line_start = 0;
            }
            if (buf[i] == '\n') {
                at_line_start = 1;
                prev_tag = cur_tag;
                cur_tag = 0;
            }
        }
    }
    fclose(f);
    if (found != g_count) {
        fprintf(stderr, "spct: %s has %d game headers but %d parsed games\n",
                path, found, g_count);
        exit(1);
    }
    for (int i = 0; i < g_count; i++)
        g_arr[i].text_len = (i + 1 < g_count ? g_arr[i + 1].text_off : pos)
                            - g_arr[i].text_off;
}

int corpus_load(const char *pgn_path, const char *pattern_dir,
                CorpusGame **out, int *n)
{
    g_count = 0;
    have_material = 0;
    snprintf(g_path, sizeof g_path, "%s", pgn_path);
    if (pattern_dir != NULL) {
        static const char *lvl[CORPUS_SAC_LEVELS] = { "1", "2", "3", "4", "5", "queen" };
        char p[1300];
        snprintf(p, sizeof p, "%s/no_endgame_draws", pattern_dir);
        L_ne_draw = spct_build_material_list(p, TRUE);
        for (int k = 0; k < CORPUS_SAC_LEVELS; k++) {
            const char *sep = k < 5 ? "_pawn" : "";
            snprintf(p, sizeof p, "%s/%s%ssac_white", pattern_dir, lvl[k], sep);
            L_sac[0][k] = spct_build_material_list(p, FALSE);
            snprintf(p, sizeof p, "%s/%s%ssac_black", pattern_dir, lvl[k], sep);
            L_sac[1][k] = spct_build_material_list(p, FALSE);
        }
        snprintf(p, sizeof p, "%s/no_endgame", pattern_dir);
        L_no_endgame = spct_build_material_list(p, TRUE);
        snprintf(p, sizeof p, "%s/imbalance", pattern_dir);
        L_imbalance = spct_build_material_list(p, TRUE);
        have_material = 1;
    }
    spct_game_hook = corpus_hook;
    /* -r (check only): parse and process every game, produce no output. */
    char *av[] = { "pgn-extract", "--quiet", "-r", (char *)pgn_path, NULL };
    pgnx_runv(av);
    spct_game_hook = NULL;
    index_text(pgn_path);
    *out = g_arr;
    *n = g_count;
    return 0;
}

void corpus_append(const char *path, const int *idx, int n)
{
    FILE *out = fopen(path, "ab");
    if (out == NULL) die("cannot append to an output file");
    FILE *in = fopen(g_path, "rb");
    if (in == NULL) die("cannot reopen the corpus file");
    char buf[65536];
    for (int i = 0; i < n; i++) {
        const CorpusGame *c = &g_arr[idx[i]];
        if (fseek(in, c->text_off, SEEK_SET) != 0) die("seek failed in corpus file");
        long left = c->text_len;
        while (left > 0) {
            size_t want = left < (long)sizeof buf ? (size_t)left : sizeof buf;
            size_t got = fread(buf, 1, want, in);
            if (got == 0) die("short read in corpus file");
            fwrite(buf, 1, got, out);
            left -= (long)got;
        }
    }
    fclose(in);
    fclose(out);
}

/* ---- duplicate-key set (open addressing on pgn-extract's hash pair) ---- */
typedef struct {
    unsigned long long *f, *c;
    unsigned char *used;
    size_t mask;
} KeySet;

static void ks_init(KeySet *s, int expected)
{
    size_t cap = 16;
    while (cap < (size_t)expected * 2 + 1) cap <<= 1;
    s->f = (unsigned long long *) malloc(cap * sizeof *s->f);
    s->c = (unsigned long long *) malloc(cap * sizeof *s->c);
    s->used = (unsigned char *) calloc(cap, 1);
    if (!s->f || !s->c || !s->used) die("out of memory (duplicate set)");
    s->mask = cap - 1;
}

static void ks_free(KeySet *s)
{
    free(s->f); free(s->c); free(s->used);
}

/* Insert the game's key; return 1 if it was already present. */
static int ks_add(KeySet *s, const CorpusGame *g)
{
    size_t h = (size_t)((g->hash_final ^ (g->hash_cumul * 0x9E3779B97F4A7C15ULL))
                        >> 7) & s->mask;
    while (s->used[h]) {
        if (s->f[h] == g->hash_final && s->c[h] == g->hash_cumul) return 1;
        h = (h + 1) & s->mask;
    }
    s->used[h] = 1;
    s->f[h] = g->hash_final;
    s->c[h] = g->hash_cumul;
    return 0;
}

int corpus_dedup(const int *in, int n, const int *exclude, int ne, int *out)
{
    KeySet s;
    ks_init(&s, n + ne);
    for (int i = 0; i < ne; i++) (void) ks_add(&s, &g_arr[exclude[i]]);
    int m = 0;
    for (int i = 0; i < n; i++)
        if (!ks_add(&s, &g_arr[in[i]])) out[m++] = in[i];
    ks_free(&s);
    return m;
}

void corpus_sac_classify(const int *W, int nW, const int *B, int nB,
                         CorpusSacSets *out)
{
    int cap = nW + nB > 0 ? nW + nB : 1;
    int *r[CORPUS_SAC_LEVELS], rn[CORPUS_SAC_LEVELS];
    for (int k = 0; k < CORPUS_SAC_LEVELS; k++) {
        r[k] = (int *) malloc((size_t)cap * sizeof(int));
        out->list[k] = (int *) malloc((size_t)cap * sizeof(int));
        if (!r[k] || !out->list[k]) die("out of memory (sacrifice sets)");
        /* results_opt(K): W's level-K matches, then B's. */
        rn[k] = 0;
        for (int i = 0; i < nW; i++)
            if (g_arr[W[i]].sac_depth > k) r[k][rn[k]++] = W[i];
        for (int i = 0; i < nB; i++)
            if (g_arr[B[i]].sac_depth > k) r[k][rn[k]++] = B[i];
    }
    /* unique_opt(K) = results_opt(K) minus results_opt(K+1), de-duplicated. */
    int top = CORPUS_SAC_LEVELS - 1;
    out->n[top] = corpus_dedup(r[top], rn[top], NULL, 0, out->list[top]);
    for (int k = top - 1; k >= 0; k--)
        out->n[k] = corpus_dedup(r[k], rn[k], r[k + 1], rn[k + 1], out->list[k]);
    for (int k = 0; k < CORPUS_SAC_LEVELS; k++) free(r[k]);
}

void corpus_sac_free(CorpusSacSets *s)
{
    for (int k = 0; k < CORPUS_SAC_LEVELS; k++) {
        free(s->list[k]);
        s->list[k] = NULL;
        s->n[k] = 0;
    }
}
