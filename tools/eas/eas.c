/*
 * EAS - Engine Aggressiveness Statistics Tool
 * Native C port of Stefan Pohl's EAS_Tool_V6.0.bat (www.sp-cc.de).
 *
 * The original is a Windows batch script that shells out to pgn-extract
 * (and Norman Pollock's pgn-tools) dozens of times per engine, passing PGN
 * subsets through temp files. This port labels every game in one streaming
 * pass over a vendored, in-process pgn-extract (core/corpus.h, using
 * pgn-extract's own -y/-z matcher), answers the batch's per-engine filter
 * passes from those labels, and reproduces the scoring arithmetic natively
 * with the same integer semantics as the batch's `set /A` math, so
 * EAS-Scores match.
 *
 * Output: statistics_EAS_ratinglist.txt (three rating lists + single-stats)
 *         errorgames.pgn (games with non-regular terminations)
 *
 * Engine Aggressiveness Statistics Tool: idea, design and scoring
 * (C) 2025, Stefan Pohl, www.sp-cc.de. pgn-extract (C) David J. Barnes.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "pgnx.h"
#include "pgnu.h"

/* Data directories, resolved at startup via pgnu_data_dir(). */
static char PATTERN_DIR[1300];     /* <data>/patterns */
static char ANNO_DIR[1300];        /* <data>/anno */
static char TERM_ERROR[1300];      /* <data>/anno/termination_error */

static int hard_moveaverage = 0;   /* --hardavg N override (0 = compute) */
static char WORK[1024];            /* working directory for intermediates */

static void die(const char *msg) { fprintf(stderr, "eas: %s\n", msg); exit(1); }

/* Shared plumbing lives in core/pgnu; alias to this tool's names so the
 * validated scoring/report bodies below are unchanged. */
#define P               pgnu_wp
#define xrun            pgnu_run
#define count_games     pgnu_count
#define annotate_append pgnu_annotate_append

static void read_anno(const char *name, char *out, size_t n)
{
    pgnu_read_anno(ANNO_DIR, name, out, n);
}

/* EAS length-sort uses 5-move buckets. */
static const int EAS_SLLO[] = {0,20,25,30,35,40,45,50,55,60,65,70,80,90,100,110,120};
static const int EAS_SLHI[] = {19,24,29,34,39,44,49,54,59,64,69,79,89,99,109,119,0};
static void sortlength(const char *src, const char *dst)
{
    pgnu_sortlength(src, dst, EAS_SLLO, EAS_SLHI, 17);
}

/* ---- streaming corpus: every per-game fact (result, length, players,
 * duplicate key, sacrifice/endgame/imbalance labels) labelled in one pass,
 * replacing the batch's per-engine filter passes. ---- */
#include "corpus.h"
static CorpusGame *CORP = NULL;
static int NCORP = 0;

/* Terminations counted as errors: the substrings pgn-extract's
 * --tagsubstr -t termination_error matches (data/anno/termination_error). */
static int corp_is_error(const char *t)
{
    static const char *sub[9] = {
        "bandon", "talled", "onnect", "orfeit", "ime",
        "llegal", "raction", "ules", "oose",
    };
    if (t == NULL || t[0] == '\0') return 0;
    for (int i = 0; i < 9; i++) if (strstr(t, sub[i])) return 1;
    return 0;
}
/* -Tw/-Tb/-Tp (without --tagsubstr) match a tag by PREFIX, so mirror that:
 * "Stockfish" selects "Stockfish 17" too, exactly as the batch's passes do. */
static int corp_is(const char *tag, const char *E)
{
    return strncmp(tag, E, strlen(E)) == 0;
}
static int corp_played_draw(const CorpusGame *c, const char *E)
{
    return c->result == 0 &&
           (corp_is(c->white, E) || corp_is(c->black, E));
}
static int corp_count_draw(const char *E)
{
    int n = 0;
    for (int i = 0; i < NCORP; i++) if (corp_played_draw(&CORP[i], E)) n++;
    return n;
}
/* Average length in moves of all decisive games (the batch's
 * newsource_onlywins: not de-duplicated), exactly as its :moveaverage. */
static int corp_avg_decisive(void)
{
    long long total = 0, cnt = 0;
    for (int i = 0; i < NCORP; i++)
        if (CORP[i].result == 1 || CORP[i].result == -1) { total += CORP[i].plies; cnt++; }
    return pgnu_moveaverage(total, cnt);
}

/* ---- percentage, matching the batch :percent exactly (core/pgnu) ---- */
typedef PgnuPct Pct;
#define pct pgnu_pct

/* Assemble interesting_wins.pgn from the per-engine collectors, in category
 * order (queen, 5, 4, 3, 2, 1 pawn sacs; very short; before-endgame;
 * imbalance), each length-sorted and annotated, then de-duplicated. */
static int build_interesting_wins(void)
{
    const char *found = P("foundgames.pgn");
    FILE *f = fopen(found, "wb"); if (f) fclose(f);
    const char *sorted = P("sortedlength.pgn");
    static const char *coll[9] = {
        "collect_sacgames_9.pgn", "collect_sacgames_5.pgn", "collect_sacgames_4.pgn",
        "collect_sacgames_3.pgn", "collect_sacgames_2.pgn", "collect_sacgames_1.pgn",
        "collect_shorts.pgn", "collect_no_endgame.pgn", "collect_imbalance.pgn",
    };
    static const char *anno[9] = {
        "anno_9sac", "anno_5sac", "anno_4sac", "anno_3sac", "anno_2sac", "anno_1sac",
        "anno_very_short_game", "anno_before_endgame", "anno_material_imbalance",
    };
    for (int i = 0; i < 9; i++) {
        const char *c = P(coll[i]);
        if (count_games(c) <= 0) continue;
        sortlength(c, sorted);
        char a[256];
        read_anno(anno[i], a, sizeof a);
        annotate_append(sorted, a, found);
    }
    xrun("--quiet", "-D", (char *)found, "--output", "interesting_wins.pgn", NULL);
    return (int)count_games("interesting_wins.pgn");
}

/* ---- engine roster (names come from the streaming corpus) ---- */
#define MAX_ENGINES 4096
#define NAME_LEN 200
static char engines[MAX_ENGINES][NAME_LEN];
static long engine_gamecount[MAX_ENGINES];
static int num_engines = 0;

/* Record one appearance of a player, adding it if new. */
static void add_engine_name(const char *name)
{
    for (int i = 0; i < num_engines; i++)
        if (strcmp(engines[i], name) == 0) { engine_gamecount[i]++; return; }
    if (num_engines < MAX_ENGINES) {
        strncpy(engines[num_engines], name, NAME_LEN - 1);
        engines[num_engines][NAME_LEN - 1] = '\0';
        engine_gamecount[num_engines] = 1;
        num_engines++;
    } else {
        static int warned = 0;
        if (!warned) {
            fprintf(stderr, "eas: more than %d players; the rest are skipped\n",
                    MAX_ENGINES);
            warned = 1;
        }
    }
}

/* Sort the roster bytewise by name (keeping each name's game count). */
static int cmp_engine_idx(const void *a, const void *b)
{
    return strcmp(engines[*(const int *)a], engines[*(const int *)b]);
}

static void sort_engines(void)
{
    static int order[MAX_ENGINES];
    static char names[MAX_ENGINES][NAME_LEN];
    static long counts[MAX_ENGINES];
    for (int i = 0; i < num_engines; i++) order[i] = i;
    qsort(order, (size_t)num_engines, sizeof order[0], cmp_engine_idx);
    for (int i = 0; i < num_engines; i++) {
        memcpy(names[i], engines[order[i]], NAME_LEN);
        counts[i] = engine_gamecount[order[i]];
    }
    memcpy(engines, names, (size_t)num_engines * NAME_LEN);
    memcpy(engine_gamecount, counts, (size_t)num_engines * sizeof counts[0]);
}

/* ---- per-engine result record ---- */
typedef struct {
    char name[NAME_LEN];
    long eas, eas_sacs, eas_earlysacs, eas_short_wins, eas_bad_draws;
    int numb_wins, avg_len_eng_wins, warning;
    Pct all_sacs, early, all_shorts, bad_draws;
    Pct sac9, sac5, sac4, sac3, sac2, sac1;
    Pct s40, s45, s50, s55, s60;
    Pct A_early, B_allsacs, C_shortC, D_allshorts, F_baddraws;
    int E_avglen;
} EngineResult;

static EngineResult results[MAX_ENGINES];
static int num_results = 0;

/* ---- process one engine ----
 * Everything below is answered from the corpus labels; the batch's
 * per-engine pgn-extract passes are mirrored as index-list operations,
 * and the games destined for errorgames.pgn / interesting_wins.pgn are
 * copied straight from the source text. */
static int *L_wnc, *L_bnc, *L_errw, *L_errb, *L_w, *L_b, *L_all, *L_tmp, *L_tmp2;
static int *L_d, *L_bad2;

static void alloc_lists(void)
{
    /* bad_draws2 can hold every draw up to three times (it gets two
     * appends), so size everything for that. */
    int **all[11] = { &L_wnc, &L_bnc, &L_errw, &L_errb, &L_w, &L_b, &L_all,
                      &L_tmp, &L_tmp2, &L_d, &L_bad2 };
    for (int i = 0; i < 11; i++) {
        *all[i] = (int *) malloc(((size_t)NCORP * 3 + 1) * sizeof(int));
        if (*all[i] == NULL) die("out of memory");
    }
}

/* :moveaverage over a list of games, and -bu<moves> counts over it. */
static int list_avg(const int *list, int n)
{
    long long plies = 0;
    for (int i = 0; i < n; i++) plies += CORP[list[i]].plies;
    return pgnu_moveaverage(plies, n);
}

static int list_count_le(const int *list, int n, int moves)
{
    int k = 0;
    for (int i = 0; i < n; i++)
        if (CORPUS_MOVES_LE(CORP[list[i]].ply_offset + CORP[list[i]].plies, moves)) k++;
    return k;
}

static void process_engine(const char *engine, int avg_length_all_wins,
                           int sh1, int sh2, int sh3, int sh4, int sh5,
                           int earlysac_limit, const char *errorcollect)
{
    EngineResult *R = &results[num_results];
    memset(R, 0, sizeof *R);
    strncpy(R->name, engine, NAME_LEN - 1);

    /* The batch's whitewins/blackwins: games the engine won with each
     * colour, in file order, minus error terminations and duplicates
     * (-c errg -D). The error games themselves go to errorgames.pgn. */
    int nwnc = 0, nbnc = 0, nerrw = 0, nerrb = 0;
    for (int i = 0; i < NCORP; i++) {
        const CorpusGame *c = &CORP[i];
        if (c->result == 1 && corp_is(c->white, engine)) {
            L_wnc[nwnc++] = i;
            if (corp_is_error(c->termination)) L_errw[nerrw++] = i;
        } else if (c->result == -1 && corp_is(c->black, engine)) {
            L_bnc[nbnc++] = i;
            if (corp_is_error(c->termination)) L_errb[nerrb++] = i;
        }
    }
    int nw = corpus_dedup(L_wnc, nwnc, L_errw, nerrw, L_w);
    int nb = corpus_dedup(L_bnc, nbnc, L_errb, nerrb, L_b);
    corpus_append(errorcollect, L_errw, nerrw);
    corpus_append(errorcollect, L_errb, nerrb);
    int nall = 0;   /* allwins = whitewins then blackwins */
    for (int i = 0; i < nw; i++) L_all[nall++] = L_w[i];
    for (int i = 0; i < nb; i++) L_all[nall++] = L_b[i];

    /* The batch counted and averaged its allwins file - whitewins then
     * blackwins, de-duplicated and without error terminations - not every
     * win the engine has. */
    int numb_wins = nall;
    R->avg_len_eng_wins = list_avg(L_all, nall);
    R->E_avglen = R->avg_len_eng_wins;
    int numb_draws = corp_count_draw(engine);   /* enginedraws: not de-duplicated */
    R->numb_wins = numb_wins;
    R->warning = (numb_wins < 50 || numb_draws < 30) ? 1 : 0;

    long engine_eas = 0;

    /* ---- bad draws: the batch's set operations on enginedraws ----
     * bad_draws2 = draws that ended before an endgame (-c on those that
     * reached one, -D), then appended: draws where the engine had a
     * material advantage; minus (-c, -D) the "saved" draws in it where the
     * engine had a material disadvantage; then -D once more. */
    int nd = 0;
    for (int i = 0; i < NCORP; i++) {
        const CorpusGame *c = &CORP[i];
        if (c->result == 0 && (corp_is(c->white, engine) || corp_is(c->black, engine)))
            L_d[nd++] = i;
    }
    int nr = 0;
    for (int i = 0; i < nd; i++) if (CORP[L_d[i]].reached_endgame_draw) L_tmp[nr++] = L_d[i];
    int nb2 = corpus_dedup(L_d, nd, L_tmp, nr, L_bad2);
    for (int i = 0; i < nd; i++)
        if (corp_is(CORP[L_d[i]].white, engine) && CORP[L_d[i]].mat_def_black)
            L_bad2[nb2++] = L_d[i];
    for (int i = 0; i < nd; i++)
        if (corp_is(CORP[L_d[i]].black, engine) && CORP[L_d[i]].mat_def_white)
            L_bad2[nb2++] = L_d[i];
    int nsaved = 0;
    for (int i = 0; i < nb2; i++)
        if (corp_is(CORP[L_bad2[i]].white, engine) && CORP[L_bad2[i]].mat_def_white)
            L_tmp[nsaved++] = L_bad2[i];
    for (int i = 0; i < nb2; i++)
        if (corp_is(CORP[L_bad2[i]].black, engine) && CORP[L_bad2[i]].mat_def_black)
            L_tmp[nsaved++] = L_bad2[i];
    int numb_bad_draws = corpus_dedup(L_bad2, nb2, L_tmp, nsaved, L_tmp2);
    numb_bad_draws = corpus_dedup(L_tmp2, numb_bad_draws, NULL, 0, L_tmp2);

    R->bad_draws = pct(numb_draws, numb_bad_draws);
    R->F_baddraws = R->bad_draws;
    int numb_good_draws = numb_draws - numb_bad_draws;
    long gd = pct(numb_draws, numb_good_draws).x100 / 100;
    long temp_bd = (gd * gd * gd) / 3000;
    temp_bd = temp_bd * temp_bd;
    engine_eas += temp_bd;
    R->eas_bad_draws = engine_eas;

    /* ---- short wins: bucket counts over allwins ---- */
    int won_40 = list_count_le(L_all, nall, sh5);
    R->s40 = pct(numb_wins, won_40);
    engine_eas += R->s40.x100 * 100;
    int shortC = won_40;

    int won_45 = list_count_le(L_all, nall, sh4);
    R->s45 = pct(numb_wins, won_45 - won_40);
    engine_eas += R->s45.x100 * 68;
    shortC += (won_45 - won_40);
    R->C_shortC = pct(numb_wins, shortC);

    int won_50 = list_count_le(L_all, nall, sh3);
    R->s50 = pct(numb_wins, won_50 - won_45);
    engine_eas += R->s50.x100 * 42;

    int won_55 = list_count_le(L_all, nall, sh2);
    R->s55 = pct(numb_wins, won_55 - won_50);
    engine_eas += R->s55.x100 * 27;

    int won_60 = list_count_le(L_all, nall, sh1);
    R->all_shorts = pct(numb_wins, won_60);
    R->D_allshorts = R->all_shorts;
    R->s60 = pct(numb_wins, won_60 - won_55);
    engine_eas += R->s60.x100 * 18;

    R->eas_short_wins = engine_eas - R->eas_bad_draws;

    /* ---- early sac bonus ----
     * Of the 1+ pawn-sac wins, those that already show the sacrifice within
     * the first earlysac_limit+8 plies. (The batch cut the games with
     * --plylimit and re-matched; the matcher stops at its first match, so
     * that is the first-match ply compared against the cut.) */
    int earlysearch = earlysac_limit + 8;
    int numb_1sacs = 0, numb_early = 0;
    for (int i = 0; i < nall; i++) {
        const CorpusGame *c = &CORP[L_all[i]];
        if (c->sac_depth < 1) continue;
        numb_1sacs++;
        int cut = earlysearch - c->ply_offset;   /* plies left after a FEN start */
        if (c->sac1_ply <= (cut > 0 ? cut : 0)) numb_early++;
    }
    R->early = pct(numb_1sacs, numb_early);
    R->A_early = R->early;
    long earlysacs_points = (R->early.x100 / 18) * (R->early.x100 / 18);
    int check_average = avg_length_all_wins - R->avg_len_eng_wins;
    if (check_average > 0) earlysacs_points += (long)check_average * 5000;
    engine_eas += earlysacs_points;
    R->eas_earlysacs = earlysacs_points;

    /* ---- sac categories 1,2,3,4,5,queen (each game in its highest) ---- */
    CorpusSacSets S;
    corpus_sac_classify(L_w, nw, L_b, nb, &S);
    int n1 = S.n[0], n2 = S.n[1], n3 = S.n[2];
    int n4 = S.n[3], n5 = S.n[4], n9 = S.n[5];
    int numb_sum = n1 + n2 + n3 + n4 + n5 + n9;
    R->all_sacs = pct(numb_wins, numb_sum);
    R->B_allsacs = R->all_sacs;

    long eas_sacs = 0;
    R->sac1 = pct(numb_wins, n1); eas_sacs += R->sac1.x100 * 10;
    R->sac2 = pct(numb_wins, n2); eas_sacs += R->sac2.x100 * 50;
    R->sac3 = pct(numb_wins, n3); eas_sacs += R->sac3.x100 * 100;
    R->sac4 = pct(numb_wins, n4); eas_sacs += R->sac4.x100 * 150;
    R->sac5 = pct(numb_wins, n5); eas_sacs += R->sac5.x100 * 200;
    R->sac9 = pct(numb_wins, n9); eas_sacs += R->sac9.x100 * 250;
    engine_eas += eas_sacs;
    R->eas_sacs = eas_sacs;

    R->eas = engine_eas;

    /* ---- collect games for interesting_wins.pgn ---- */
    static const char *collsac[CORPUS_SAC_LEVELS] = {
        "collect_sacgames_1.pgn", "collect_sacgames_2.pgn", "collect_sacgames_3.pgn",
        "collect_sacgames_4.pgn", "collect_sacgames_5.pgn", "collect_sacgames_9.pgn",
    };
    for (int k = 0; k < CORPUS_SAC_LEVELS; k++)
        corpus_append(P(collsac[k]), S.list[k], S.n[k]);
    corpus_sac_free(&S);
    /* very short wins (<= sh4 moves) */
    int m = 0;
    for (int i = 0; i < nall; i++) {
        const CorpusGame *c = &CORP[L_all[i]];
        if (CORPUS_MOVES_LE(c->ply_offset + c->plies, sh4)) L_tmp[m++] = L_all[i];
    }
    corpus_append(P("collect_shorts.pgn"), L_tmp, m);
    /* Wins that ended before an endgame was reached (allwins minus reached),
     * and wins with a material imbalance. The batch built these two
     * categories after its engine loop, from the allwins.pgn the last engine
     * left behind, so only the last engine's games count: start them over. */
    pgnu_truncate(P("collect_no_endgame.pgn"));
    pgnu_truncate(P("collect_imbalance.pgn"));
    m = 0;
    for (int i = 0; i < nall; i++)
        if (CORP[L_all[i]].no_endgame) L_tmp[m++] = L_all[i];
    int nne = corpus_dedup(L_all, nall, L_tmp, m, L_tmp2);
    corpus_append(P("collect_no_endgame.pgn"), L_tmp2, nne);
    m = 0;
    for (int i = 0; i < nall; i++)
        if (CORP[L_all[i]].imbalance) L_tmp[m++] = L_all[i];
    corpus_append(P("collect_imbalance.pgn"), L_tmp, m);

    num_results++;
}

/* ---- the report, exactly as the batch assembles it ----
 * Per engine the batch echoed one line into each of three work files and
 * one into each of six single-statistics files; it then ordered them with
 * Windows sort.exe and numbered them. The lines are rendered here from the
 * batch's own text (eas_templates.h) and ordered with pgnu_sort_compare. */
#include "eas_templates.h"

typedef struct { const char *k; char v[CORPUS_NAME_LEN + 8]; } KV;
typedef struct { KV kv[48]; int n; } Vars;

static void var_set(Vars *vs, const char *k, const char *v)
{
    if (vs->n >= 48) die("internal: too many report values");
    vs->kv[vs->n].k = k;
    snprintf(vs->kv[vs->n].v, sizeof vs->kv[0].v, "%s", v);
    vs->n++;
}

static void var_setl(Vars *vs, const char *k, long v)
{
    char b[32];
    snprintf(b, sizeof b, "%ld", v);
    var_set(vs, k, b);
}

/* Fill %name% placeholders; NULL if one names a variable that is not set
 * (the batch's echo of such a line did not reach the file). */
static char *render(const char *tmpl, const Vars *vs)
{
    size_t cap = strlen(tmpl) + 1;
    for (int i = 0; i < vs->n; i++) cap += strlen(vs->kv[i].v);
    char *out = (char *) malloc(cap * 2 + 16), *o = out;
    if (out == NULL) die("out of memory");
    for (const char *p = tmpl; *p; ) {
        const char *q = *p == '%' ? strchr(p + 1, '%') : NULL;
        if (q == NULL) { *o++ = *p++; continue; }
        size_t kl = (size_t)(q - p - 1);
        const char *val = NULL;
        for (int i = 0; i < vs->n && val == NULL; i++)
            if (strlen(vs->kv[i].k) == kl && strncmp(vs->kv[i].k, p + 1, kl) == 0)
                val = vs->kv[i].v;
        if (val == NULL) { free(out); return NULL; }
        size_t vl = strlen(val);
        memcpy(o, val, vl);
        o += vl;
        p = q + 1;
    }
    *o = '\0';
    return out;
}

/* The batch's :format_* routines: right-align in a fixed width. A value
 * too wide for the routine leaves %formatted% as the previous call set it. */
static char g_formatted[32] = "";

static const char *fmt_width(long v, int width, long max)
{
    if (v <= max) {
        char digits[24];
        snprintf(digits, sizeof digits, "%ld", v);
        int pad = 1;   /* the batch's thresholds are 9, 99, 999, ... */
        for (long t = 9; v > t && pad <= width; t = t * 10 + 9) pad++;
        snprintf(g_formatted, sizeof g_formatted, "%*s%s", width - pad, "", digits);
    }
    return g_formatted;
}
static const char *fmt_eas(long v) { return fmt_width(v, 7, 9999999L); }
static const char *fmt_wins(long v) { return fmt_width(v, 6, 999999L); }
static const char *fmt_engwins(long v) { return fmt_width(v, 3, 999L); }
static const char *fmt_engines(long v) { return fmt_width(v, 4, 9999L); }

/* Rendered lines per engine: the three lists and single-stats A..F. */
typedef struct { char *w1, *w2, *w3, *ss[6]; } EngineLines;
static EngineLines lines_of[MAX_ENGINES];
static char avg_all_fmt[32];   /* %avg_length_all_wins% after the loop */

static void render_engine(const EngineResult *R, int avg_length_all_wins, EngineLines *L)
{
    Vars v = { .n = 0 };
    char quoted[CORPUS_NAME_LEN + 4];
    snprintf(quoted, sizeof quoted, "\"%s\"", R->name);

    /* Single-stats lines were echoed while the name still had its quotes. */
    char ma[16];
    snprintf(ma, sizeof ma, "%s%d", R->avg_len_eng_wins <= 9 ? "00" :
             R->avg_len_eng_wins <= 99 ? "0" : "", R->avg_len_eng_wins);
    const char *ssval[6] = { R->early.s, R->all_sacs.s, R->C_shortC.s,
                             R->all_shorts.s, ma, R->bad_draws.s };
    const char *sstmpl[6] = { EAS_SS_A, EAS_SS_B, EAS_SS_C, EAS_SS_D, EAS_SS_E, EAS_SS_F };
    for (int k = 0; k < 6; k++) {
        Vars s = { .n = 0 };
        var_set(&s, k == 4 ? "ma_moveaverage" : "percent", ssval[k]);
        var_set(&s, "engine", quoted);
        L->ss[k] = render(sstmpl[k], &s);
    }

    /* The work lines, formatted in the batch's order of :format_* calls. */
    var_set(&v, "engine_eas", fmt_eas(R->eas));
    var_set(&v, "eas_bad_draws", fmt_eas(R->eas_bad_draws));
    var_set(&v, "eas_short_wins", fmt_eas(R->eas_short_wins));
    var_set(&v, "eas_sacs", fmt_eas(R->eas_sacs));
    var_set(&v, "eas_earlysacs", fmt_eas(R->eas_earlysacs));
    var_set(&v, "winform", fmt_wins(R->numb_wins));
    var_set(&v, "avg_length_eng_wins", fmt_engwins(R->avg_len_eng_wins));
    snprintf(avg_all_fmt, sizeof avg_all_fmt, "%s", fmt_engwins(avg_length_all_wins));
    var_set(&v, "percent_all_sacs", R->all_sacs.s);
    var_set(&v, "earlysacs_percent", R->early.s);
    var_set(&v, "percent_all_shorts", R->all_shorts.s);
    var_set(&v, "percent_bad_draws", R->bad_draws.s);
    var_set(&v, "perc_sac9", R->sac9.s);
    var_set(&v, "perc_sac5", R->sac5.s);
    var_set(&v, "perc_sac4", R->sac4.s);
    var_set(&v, "perc_sac3", R->sac3.s);
    var_set(&v, "perc_sac2", R->sac2.s);
    var_set(&v, "perc_sac1", R->sac1.s);
    var_set(&v, "perc_40mvs", R->s40.s);
    var_set(&v, "perc_45mvs", R->s45.s);
    var_set(&v, "perc_50mvs", R->s50.s);
    var_set(&v, "perc_55mvs", R->s55.s);
    var_set(&v, "perc_60mvs", R->s60.s);
    var_set(&v, "engine", R->name);   /* the quotes are stripped by then */
    L->w1 = render(R->warning ? EAS_W1_WARN : EAS_W1, &v);
    L->w2 = render(EAS_W2, &v);
    L->w3 = render(EAS_W3, &v);
}

/* Order the given lines as sort.exe (reverse=1: sort /r) would; lines it
 * finds equal fall back to byte order. */
static int g_reverse;
static int cmp_sortexe(const void *a, const void *b)
{
    const char *x = *(char *const *)a, *y = *(char *const *)b;
    int c = pgnu_sort_compare(x, y);
    if (c == 0) c = strcmp(x, y);
    return g_reverse ? -c : c;
}

static char **sorted_copy(char *const *src, int n, int reverse)
{
    char **v = (char **) malloc((size_t)(n ? n : 1) * sizeof(char *));
    if (v == NULL) die("out of memory");
    memcpy(v, src, (size_t)n * sizeof(char *));
    g_reverse = reverse;
    qsort(v, (size_t)n, sizeof(char *), cmp_sortexe);
    return v;
}

static void put_block(FILE *o, const char *const *block, const Vars *vs)
{
    for (int i = 0; block[i] != NULL; i++) {
        char *s = render(block[i], vs);
        if (s != NULL) { fputs(s, o); fputc('\n', o); free(s); }
    }
}

/* One ranked list: "<rank>  <work line> " per engine, best first. */
static void put_list(FILE *o, char **work, int n)
{
    char **v = sorted_copy(work, n, 1);
    for (int i = 0; i < n; i++)
        fprintf(o, "%s  %s \n", fmt_engines(i + 1), v[i]);
    free(v);
}

static void write_ratinglist(const char *gamebase, int avg_length_all_wins,
                             int shortwin_movelimit, int earlysac_limit,
                             int sh1, int sh2, int sh3, int sh4, int sh5)
{
    int n = num_results;
    if (n == 0) snprintf(avg_all_fmt, sizeof avg_all_fmt, "%d", avg_length_all_wins);
    Vars g = { .n = 0 };
    var_set(&g, "gamebase", gamebase);
    var_set(&g, "avg_length_all_wins", avg_all_fmt);
    var_setl(&g, "earlysac_limit", earlysac_limit);
    var_setl(&g, "shortwin_movelimit", shortwin_movelimit);
    var_setl(&g, "sh_level1", sh1);
    var_setl(&g, "sh_level2", sh2);
    var_setl(&g, "sh_level3", sh3);
    var_setl(&g, "sh_level4", sh4);
    var_setl(&g, "sh_level5", sh5);

    /* The medal table: each category's lines sorted (A-D reversed), the
     * first five taken with their quotes removed. */
    static const char *cat = "ABCDEF";
    static const char *place[5] = { "gold", "silver", "bronze", "fourth", "fifth" };
    static char keys[6][5][24];
    char **col = (char **) malloc((size_t)(n ? n : 1) * sizeof(char *));
    if (col == NULL) die("out of memory");
    for (int k = 0; k < 6; k++) {
        for (int i = 0; i < n; i++) col[i] = lines_of[i].ss[k];
        char **v = sorted_copy(col, n, k < 4);
        for (int p = 0; p < 5 && p < n; p++) {
            char medal[CORPUS_NAME_LEN + 32], *m = medal;
            for (const char *s = v[p]; *s && m < medal + sizeof medal - 1; s++)
                if (*s != '"') *m++ = *s;
            *m = '\0';
            snprintf(keys[k][p], sizeof keys[k][p], "eas_%c_%smedal", cat[k], place[p]);
            var_set(&g, keys[k][p], medal);
        }
        free(v);
    }

    FILE *o = fopen("statistics_EAS_ratinglist.txt", "w");
    if (!o) die("cannot write statistics_EAS_ratinglist.txt");
    put_block(o, EAS_HEAD, &g);
    for (int i = 0; i < n; i++) col[i] = lines_of[i].w1;
    put_list(o, col, n);
    put_block(o, EAS_AFTER1, &g);
    put_block(o, EAS_MEDALS, &g);
    put_block(o, EAS_HEAD2, &g);
    for (int i = 0; i < n; i++) col[i] = lines_of[i].w2;
    put_list(o, col, n);
    put_block(o, EAS_HEAD3, &g);
    for (int i = 0; i < n; i++) col[i] = lines_of[i].w3;
    put_list(o, col, n);
    put_block(o, EAS_TAIL, &g);
    fclose(o);
    free(col);
}

int cmd_eas(int argc, char *argv[])
{
    const char *infile = NULL;
    int gauntlet = 0;
    hard_moveaverage = 0;
    WORK[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hardavg") == 0 && i + 1 < argc)
            hard_moveaverage = atoi(argv[++i]);
        else if (strncmp(argv[i], "--work=", 7) == 0)
            snprintf(WORK, sizeof WORK, "%s", argv[i] + 7);
        else if (strcmp(argv[i], "--gauntlet") == 0)
            gauntlet = 1;
        else
            infile = argv[i];
    }
    char namebuf[1024];
    if (!infile) {
        printf("Enter the name of your games pgn-file (with or without .pgn):\n");
        if (!fgets(namebuf, sizeof namebuf, stdin)) return 1;
        namebuf[strcspn(namebuf, "\r\n")] = '\0';
        infile = namebuf;
    }
    char gamebase[1100];
    size_t il = strlen(infile);
    if (il > 4 && strcmp(infile + il - 4, ".pgn") == 0)
        snprintf(gamebase, sizeof gamebase, "%s", infile);
    else
        snprintf(gamebase, sizeof gamebase, "%s.pgn", infile);

    const char *data = pgnu_data_dir();
    snprintf(PATTERN_DIR, sizeof PATTERN_DIR, "%s/patterns", data);
    snprintf(ANNO_DIR, sizeof ANNO_DIR, "%s/anno", data);
    snprintf(TERM_ERROR, sizeof TERM_ERROR, "%s/termination_error", ANNO_DIR);

    if (WORK[0] == '\0') snprintf(WORK, sizeof WORK, "build/eas_work");
    pgnu_init(WORK);

    const char *newsource = P("newsource.pgn");
    xrun("--quiet", "--fixresulttags", "-C", "-N", "-V", "--plycount",
         (char *)gamebase, "--output", (char *)newsource, NULL);
    if (count_games(newsource) == 0) {
        fprintf(stderr, "eas: no games read from %s\n", gamebase);
        return 1;
    }

    /* One streaming pass labels every game (result, length, players,
     * duplicate key, sacrifice/endgame/imbalance); every per-engine count,
     * sacrifice category and output collection is then answered in memory
     * instead of by per-engine filter passes. */
    corpus_load(newsource, PATTERN_DIR, &CORP, &NCORP);
    alloc_lists();

    int avg_length_all_wins = corp_avg_decisive();
    if (hard_moveaverage > 0) avg_length_all_wins = hard_moveaverage;
    int shortwin_movelimit = avg_length_all_wins - 15;
    if (shortwin_movelimit < 30) shortwin_movelimit = 30;
    if (shortwin_movelimit > 95) shortwin_movelimit = 95;
    int sh1 = shortwin_movelimit, sh2 = shortwin_movelimit - 5;
    int sh3 = shortwin_movelimit - 10, sh4 = shortwin_movelimit - 15;
    int sh5 = shortwin_movelimit - 20;
    int earlysac_limit = avg_length_all_wins / 2;
    if (earlysac_limit < 10) earlysac_limit = 10;

    /* Enumerate engines from the corpus (engine_gamecount filled for
     * --gauntlet), then put them in the order the batch processed them:
     * Pollock's nameList sorts the names bytewise. That order decides the
     * order of errorgames.pgn and of each interesting_wins.pgn category. */
    for (int i = 0; i < NCORP; i++) {
        add_engine_name(CORP[i].white);
        add_engine_name(CORP[i].black);
    }
    sort_engines();
    printf("Engines found: %d\n", num_engines);

    const char *errorcollect = P("errorgames_collect.pgn");
    { FILE *e = fopen(errorcollect, "wb"); if (e) fclose(e); }

    /* Start the interesting_wins.pgn collectors empty. */
    static const char *colls[9] = {
        "collect_sacgames_9.pgn", "collect_sacgames_5.pgn", "collect_sacgames_4.pgn",
        "collect_sacgames_3.pgn", "collect_sacgames_2.pgn", "collect_sacgames_1.pgn",
        "collect_shorts.pgn", "collect_no_endgame.pgn", "collect_imbalance.pgn",
    };
    for (int i = 0; i < 9; i++) { FILE *e = fopen(P(colls[i]), "wb"); if (e) fclose(e); }

    if (gauntlet) {
        /* Gauntlet mode: evaluate only the engine that played the most games. */
        int gi = 0;
        for (int i = 1; i < num_engines; i++)
            if (engine_gamecount[i] > engine_gamecount[gi]) gi = i;
        printf("Gauntlet engine: %s (%ld games)\n", engines[gi], engine_gamecount[gi]);
        process_engine(engines[gi], avg_length_all_wins,
                       sh1, sh2, sh3, sh4, sh5, earlysac_limit, errorcollect);
        render_engine(&results[0], avg_length_all_wins, &lines_of[0]);
    } else {
        for (int i = 0; i < num_engines; i++) {
            printf("  [%d/%d] %s\n", i + 1, num_engines, engines[i]);
            process_engine(engines[i], avg_length_all_wins,
                           sh1, sh2, sh3, sh4, sh5, earlysac_limit, errorcollect);
            render_engine(&results[num_results - 1], avg_length_all_wins,
                          &lines_of[num_results - 1]);
        }
    }

    xrun("--quiet", "-D", (char *)errorcollect, "--output", "errorgames.pgn", NULL);
    int numb_errors = (int)count_games("errorgames.pgn");

    int numb_interesting = build_interesting_wins();

    write_ratinglist(gamebase, avg_length_all_wins, shortwin_movelimit,
                     earlysac_limit, sh1, sh2, sh3, sh4, sh5);

    printf("Done. See statistics_EAS_ratinglist.txt (%d engines evaluated).\n",
           num_results);
    printf("  interesting_wins.pgn: %d games   errorgames.pgn: %d games\n",
           numb_interesting, numb_errors);
    return 0;
}
