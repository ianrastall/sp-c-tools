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
#include "end.h"         /* spct_build_material_list, spct_game_matches_material */
#include "grammar.h"     /* spct_game_hook */
#include "pgnx.h"
#include "corpus.h"

static CorpusGame *g_arr = NULL;
static int g_count = 0;
static int g_cap = 0;

/* Standalone material-pattern lists (built once from the pattern dir). */
static Material_details *L_ne_draw = NULL;  /* no_endgame_draws (-z) */
static Material_details *L_1white = NULL;   /* 1_pawnsac_white (-y) */
static Material_details *L_1black = NULL;   /* 1_pawnsac_black (-y) */
static int have_material = 0;

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

    c->mat_def_white = 0;
    c->mat_def_black = 0;
    c->reached_endgame_draw = 0;
    c->sac_category = 0;
    /* Material labels are only needed for draws so far (bad-draw detection).
     * spct_game_matches_material replays the game with pgn-extract's own
     * -y/-z matcher; casting away const is safe (it does not mutate here). */
    if (have_material && c->result == 0) {
        Game *gm = (Game *) g;
        c->reached_endgame_draw = spct_game_matches_material(gm, L_ne_draw) ? 1 : 0;
        c->mat_def_white = spct_game_matches_material(gm, L_1white) ? 1 : 0;
        c->mat_def_black = spct_game_matches_material(gm, L_1black) ? 1 : 0;
    }

    g_count++;
}

int corpus_load(const char *pgn_path, const char *pattern_dir,
                CorpusGame **out, int *n)
{
    g_count = 0;
    have_material = 0;
    if (pattern_dir != NULL) {
        char p[1300];
        snprintf(p, sizeof p, "%s/no_endgame_draws", pattern_dir);
        L_ne_draw = spct_build_material_list(p, TRUE);
        snprintf(p, sizeof p, "%s/1_pawnsac_white", pattern_dir);
        L_1white = spct_build_material_list(p, FALSE);
        snprintf(p, sizeof p, "%s/1_pawnsac_black", pattern_dir);
        L_1black = spct_build_material_list(p, FALSE);
        have_material = 1;
    }
    spct_game_hook = corpus_hook;
    /* -r (check only): parse and process every game, produce no output. */
    char *av[] = { "pgn-extract", "--quiet", "-r", (char *)pgn_path, NULL };
    pgnx_runv(av);
    spct_game_hook = NULL;
    *out = g_arr;
    *n = g_count;
    return 0;
}
