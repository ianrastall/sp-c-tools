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
/* A win by E excludes error games (as EAS strips them before counting wins). */
static int corp_win(const CorpusGame *c, const char *E)
{
    int won = (c->result == 1 && corp_is(c->white, E)) ||
              (c->result == -1 && corp_is(c->black, E));
    return won && !corp_is_error(c->termination);
}
static int corp_played_draw(const CorpusGame *c, const char *E)
{
    return c->result == 0 &&
           (corp_is(c->white, E) || corp_is(c->black, E));
}
static int corp_count_win(const char *E)
{
    int n = 0;
    for (int i = 0; i < NCORP; i++) if (corp_win(&CORP[i], E)) n++;
    return n;
}
static int corp_count_draw(const char *E)
{
    int n = 0;
    for (int i = 0; i < NCORP; i++) if (corp_played_draw(&CORP[i], E)) n++;
    return n;
}
static int corp_count_win_le(const char *E, int moves)
{
    int n = 0;
    for (int i = 0; i < NCORP; i++)
        if (corp_win(&CORP[i], E) &&
            CORPUS_MOVES_LE(CORP[i].ply_offset + CORP[i].plies, moves)) n++;
    return n;
}
/* Average length in MOVES = round(mean plies)/2, matching :moveaverage. */
static int corp_avg(int decisive_only, const char *E)
{
    long total = 0, cnt = 0;
    for (int i = 0; i < NCORP; i++) {
        int take = decisive_only ? (CORP[i].result == 1 || CORP[i].result == -1)
                                 : corp_win(&CORP[i], E);
        if (take) { total += CORP[i].plies; cnt++; }
    }
    if (cnt <= 0) return 0;
    long whole = total / cnt, rem = total % cnt;
    if (rem * 2 >= cnt) whole += 1;
    return (int)(whole / 2);
}

/* ---- percentage, matching the batch :percent exactly ---- */
typedef struct { char s[10]; long x100; } Pct;

static Pct pct(long base, long count)
{
    Pct r;
    if (base <= 0) { strcpy(r.s, "00.00%"); r.x100 = 0; return r; }
    long long l_base = (1000000000LL / base) * count;
    long long l_percent = l_base / 1000000;
    long long l_rest1 = l_percent % 10;
    l_percent /= 10;
    long long l_rest2 = (l_base / 100000) % 10;
    long long l_rest3 = (l_base / 10000) % 10;
    if (l_rest3 >= 5) l_rest2 += 1;
    if (l_rest2 >= 10) { l_rest2 -= 10; l_rest1 += 1; }
    if (l_rest1 >= 10) { l_rest1 -= 10; l_percent += 1; }
    if (l_percent < 10)
        snprintf(r.s, sizeof r.s, "0%lld.%lld%lld%%", l_percent, l_rest1, l_rest2);
    else
        snprintf(r.s, sizeof r.s, "%lld.%lld%lld%%", l_percent, l_rest1, l_rest2);
    r.x100 = (long)(l_percent * 100 + l_rest1 * 10 + l_rest2);
    if (l_percent > 99) { strcpy(r.s, "100.0%"); r.x100 = 10000; }
    return r;
}

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
#define MAX_ENGINES 512
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

static void write_single_stats(FILE *o, int sh4);

/* ---- process one engine ----
 * Everything below is answered from the corpus labels; the batch's
 * per-engine pgn-extract passes are mirrored as index-list operations,
 * and the games destined for errorgames.pgn / interesting_wins.pgn are
 * copied straight from the source text. */
static int *L_wnc, *L_bnc, *L_errw, *L_errb, *L_w, *L_b, *L_all, *L_tmp, *L_tmp2;

static void alloc_lists(void)
{
    int **all[9] = { &L_wnc, &L_bnc, &L_errw, &L_errb, &L_w, &L_b, &L_all,
                     &L_tmp, &L_tmp2 };
    for (int i = 0; i < 9; i++) {
        *all[i] = (int *) malloc(((size_t)NCORP + 1) * sizeof(int));
        if (*all[i] == NULL) die("out of memory");
    }
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

    R->avg_len_eng_wins = corp_avg(0, engine);   /* E's non-error wins */
    R->E_avglen = R->avg_len_eng_wins;
    int numb_wins = corp_count_win(engine);
    int numb_draws = corp_count_draw(engine);
    R->numb_wins = numb_wins;
    R->warning = (numb_wins < 50 || numb_draws < 30) ? 1 : 0;

    long engine_eas = 0;

    /* ---- bad draws (from the streaming material labels) ----
     * A draw is bad if it ended before an endgame OR the engine had a
     * material advantage, but not if the engine had a material disadvantage.
     * (Mirrors the batch's -z/-y set operations, per game.) */
    int numb_bad_draws = 0;
    for (int i = 0; i < NCORP; i++) {
        const CorpusGame *c = &CORP[i];
        if (c->result != 0) continue;                 /* draws only */
        int ew = corp_is(c->white, engine);
        int eb = corp_is(c->black, engine);
        if (!ew && !eb) continue;                      /* engine must have played */
        int in_bad2 = (!c->reached_endgame_draw)       /* ended before endgame */
                      || (ew && c->mat_def_black)       /* engine had advantage */
                      || (eb && c->mat_def_white);
        int saved = (ew && c->mat_def_white)            /* engine had disadvantage */
                    || (eb && c->mat_def_black);
        if (in_bad2 && !saved) numb_bad_draws++;
    }

    R->bad_draws = pct(numb_draws, numb_bad_draws);
    R->F_baddraws = R->bad_draws;
    int numb_good_draws = numb_draws - numb_bad_draws;
    long gd = pct(numb_draws, numb_good_draws).x100 / 100;
    long temp_bd = (gd * gd * gd) / 3000;
    temp_bd = temp_bd * temp_bd;
    engine_eas += temp_bd;
    R->eas_bad_draws = engine_eas;

    /* ---- short wins (bucket counts from the streaming corpus) ---- */
    int won_40 = corp_count_win_le(engine, sh5);
    R->s40 = pct(numb_wins, won_40);
    engine_eas += R->s40.x100 * 100;
    int shortC = won_40;

    int won_45 = corp_count_win_le(engine, sh4);
    R->s45 = pct(numb_wins, won_45 - won_40);
    engine_eas += R->s45.x100 * 68;
    shortC += (won_45 - won_40);
    R->C_shortC = pct(numb_wins, shortC);

    int won_50 = corp_count_win_le(engine, sh3);
    R->s50 = pct(numb_wins, won_50 - won_45);
    engine_eas += R->s50.x100 * 42;

    int won_55 = corp_count_win_le(engine, sh2);
    R->s55 = pct(numb_wins, won_55 - won_50);
    engine_eas += R->s55.x100 * 27;

    int won_60 = corp_count_win_le(engine, sh1);
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
    /* wins that ended before an endgame was reached (allwins minus reached) */
    m = 0;
    for (int i = 0; i < nall; i++)
        if (CORP[L_all[i]].no_endgame) L_tmp[m++] = L_all[i];
    int nne = corpus_dedup(L_all, nall, L_tmp, m, L_tmp2);
    corpus_append(P("collect_no_endgame.pgn"), L_tmp2, nne);
    /* wins with a material imbalance */
    m = 0;
    for (int i = 0; i < nall; i++)
        if (CORP[L_all[i]].imbalance) L_tmp[m++] = L_all[i];
    corpus_append(P("collect_imbalance.pgn"), L_tmp, m);

    num_results++;
}

/* ---- output ---- */
static int cmp_eas(const void *a, const void *b)
{
    const EngineResult *x = a, *y = b;
    if (y->eas != x->eas) return (y->eas > x->eas) ? 1 : -1;
    return 0;
}

static int medal_cat;
static int cmp_cat(const void *a, const void *b)
{
    const EngineResult *x = a, *y = b;
    long xv = 0, yv = 0;
    switch (medal_cat) {
        case 0: xv = x->A_early.x100; yv = y->A_early.x100; break;
        case 1: xv = x->B_allsacs.x100; yv = y->B_allsacs.x100; break;
        case 2: xv = x->C_shortC.x100; yv = y->C_shortC.x100; break;
        case 3: xv = x->D_allshorts.x100; yv = y->D_allshorts.x100; break;
        case 4: return x->E_avglen - y->E_avglen;               /* ascending */
        case 5: return (int)(x->F_baddraws.x100 - y->F_baddraws.x100);/* asc */
    }
    if (yv != xv) return (yv > xv) ? 1 : -1;
    return 0;
}

static void write_single_stats(FILE *o, int sh4)
{
    static const char *titles[6] = {
        "A: Early sacrifices (percent of all sacs)",
        "B: Most sacrifices overall",
        "C: Very short wins",
        "D: Most short wins overall",
        "E: Average length of all won games (shortest)",
        "F: Smallest number of bad draws",
    };
    static EngineResult tmp[MAX_ENGINES];
    for (int c = 0; c < 6; c++) {
        memcpy(tmp, results, num_results * sizeof(EngineResult));
        medal_cat = c;
        qsort(tmp, num_results, sizeof tmp[0], cmp_cat);
        if (c == 2) fprintf(o, "%s (%d moves or less):\n", titles[c], sh4);
        else fprintf(o, "%s:\n", titles[c]);
        int top = num_results < 5 ? num_results : 5;
        for (int i = 0; i < top; i++) {
            EngineResult *R = &tmp[i];
            char val[32];
            switch (c) {
                case 0: snprintf(val, sizeof val, "%s", R->A_early.s); break;
                case 1: snprintf(val, sizeof val, "%s", R->B_allsacs.s); break;
                case 2: snprintf(val, sizeof val, "%s", R->C_shortC.s); break;
                case 3: snprintf(val, sizeof val, "%s", R->D_allshorts.s); break;
                case 4: snprintf(val, sizeof val, "%d moves", R->E_avglen); break;
                case 5: snprintf(val, sizeof val, "%s", R->F_baddraws.s); break;
            }
            fprintf(o, "     [%d] %-9s %s\n", i + 1, val, R->name);
        }
    }
}

static void write_ratinglist(const char *gamebase, int avg_length_all_wins,
                             int shortwin_movelimit, int earlysac_limit,
                             int sh1, int sh2, int sh3, int sh4, int sh5,
                             int numb_errors)
{
    qsort(results, num_results, sizeof results[0], cmp_eas);
    FILE *o = fopen("statistics_EAS_ratinglist.txt", "wb");
    if (!o) die("cannot write statistics_EAS_ratinglist.txt");

    fprintf(o, "*****************************************************************************\n");
    fprintf(o, "*** Engine Aggressiveness Tool V6.0 Score points Ratinglist (SPCT C port)\n");
    fprintf(o, "*****************************************************************************\n");
    fprintf(o, "*** Evaluated file: %s\n", gamebase);
    fprintf(o, "*****************************************************************************\n");
    fprintf(o, "                         early           bad  avg.win\n");
    fprintf(o, "Rank  EAS-Score  sacs    sacs   shorts  draws  moves  Engine/player\n");
    fprintf(o, "---------------------------------------------------------------------------\n");
    for (int i = 0; i < num_results; i++) {
        EngineResult *R = &results[i];
        fprintf(o, "%3d %8ld  %6s  %6s  %6s  %6s  %5d   %s%s\n",
                i + 1, R->eas, R->all_sacs.s, R->early.s, R->all_shorts.s,
                R->bad_draws.s, R->avg_len_eng_wins, R->name,
                R->warning ? "   XXXXX WARNING: not enough games (need 50+ wins, 30+ draws) XXXXX" : "");
    }
    fprintf(o, "-------------------------------------------------------------------\n");
    fprintf(o, "*** Average length of all won games: %d moves\n", avg_length_all_wins);
    fprintf(o, "*** Movelimit for early sac bonus  : %d moves\n", earlysac_limit);

    fprintf(o, "*****************************************************************************\n");
    fprintf(o, "*** EAS single-statistics (6 categories, each with Top5 engines):\n");
    fprintf(o, "*****************************************************************************\n");
    write_single_stats(o, sh4);

    fprintf(o, "*****************************************************************************\n");
    fprintf(o, "*** 2nd Ratinglist with more stats in percent-values\n");
    fprintf(o, "*****************************************************************************\n");
    fprintf(o, "*** Average length of all won games                  : %d moves\n", avg_length_all_wins);
    fprintf(o, "*** Calculated limit for short wins giving EAS-points: %d moves\n", shortwin_movelimit);
    fprintf(o, "*** Movelimit for early sac bonus                    : %d moves\n", earlysac_limit);
    fprintf(o, "Rank EAS-Score wins amoves  allsacs =[ sacQ + sac5 + sac4 + sac3 + sac2 + sac1] esacs  shorts =[ s%d + s%d + s%d + s%d + s%d] bdraws Engine\n",
            sh5, sh4, sh3, sh2, sh1);
    fprintf(o, "-------------------------------------------------------------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < num_results; i++) {
        EngineResult *R = &results[i];
        fprintf(o, "%3d %8ld %5d %5d  %6s =[%6s +%6s +%6s +%6s +%6s +%6s] %6s  %6s =[%6s +%6s +%6s +%6s +%6s] %6s %s\n",
                i + 1, R->eas, R->numb_wins, R->avg_len_eng_wins,
                R->all_sacs.s, R->sac9.s, R->sac5.s, R->sac4.s, R->sac3.s, R->sac2.s, R->sac1.s,
                R->early.s, R->all_shorts.s, R->s40.s, R->s45.s, R->s50.s, R->s55.s, R->s60.s,
                R->bad_draws.s, R->name);
    }

    fprintf(o, "*****************************************************************************\n");
    fprintf(o, "*** 3rd Ratinglist, showing EAS-points instead of percents\n");
    fprintf(o, "*****************************************************************************\n");
    fprintf(o, "                            early          bad\n");
    fprintf(o, "Rank  EAS-Score    sacs    sacs   shorts   draws    Engine/player\n");
    fprintf(o, "-------------------------------------------------------------------------------------\n");
    for (int i = 0; i < num_results; i++) {
        EngineResult *R = &results[i];
        fprintf(o, "%3d %8ld  %6ld  %6ld  %6ld  %6ld    %s\n",
                i + 1, R->eas, R->eas_sacs, R->eas_earlysacs,
                R->eas_short_wins, R->eas_bad_draws, R->name);
    }
    fprintf(o, "********************************************************************************************\n");
    fprintf(o, "*** %d games with non-regular endings are stored in errorgames.pgn\n", numb_errors);
    fprintf(o, "****************************************************\n");
    fprintf(o, "*** EAS-Tool (C) Stefan Pohl (www.sp-cc.de), C port\n");
    fprintf(o, "****************************************************\n");
    fclose(o);
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

    int avg_length_all_wins = corp_avg(1, NULL);   /* all decisive games */
    if (hard_moveaverage > 0) avg_length_all_wins = hard_moveaverage;
    int shortwin_movelimit = avg_length_all_wins - 15;
    if (shortwin_movelimit < 30) shortwin_movelimit = 30;
    if (shortwin_movelimit > 95) shortwin_movelimit = 95;
    int sh1 = shortwin_movelimit, sh2 = shortwin_movelimit - 5;
    int sh3 = shortwin_movelimit - 10, sh4 = shortwin_movelimit - 15;
    int sh5 = shortwin_movelimit - 20;
    int earlysac_limit = avg_length_all_wins / 2;
    if (earlysac_limit < 10) earlysac_limit = 10;

    /* Enumerate engines from the corpus: distinct names in file order,
     * engine_gamecount filled for --gauntlet (same as the old file scan). */
    for (int i = 0; i < NCORP; i++) {
        add_engine_name(CORP[i].white);
        add_engine_name(CORP[i].black);
    }
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
    } else {
        for (int i = 0; i < num_engines; i++) {
            printf("  [%d/%d] %s\n", i + 1, num_engines, engines[i]);
            process_engine(engines[i], avg_length_all_wins,
                           sh1, sh2, sh3, sh4, sh5, earlysac_limit, errorcollect);
        }
    }

    xrun("--quiet", "-D", (char *)errorcollect, "--output", "errorgames.pgn", NULL);
    int numb_errors = (int)count_games("errorgames.pgn");

    int numb_interesting = build_interesting_wins();

    write_ratinglist(gamebase, avg_length_all_wins, shortwin_movelimit,
                     earlysac_limit, sh1, sh2, sh3, sh4, sh5, numb_errors);

    printf("Done. See statistics_EAS_ratinglist.txt (%d engines evaluated).\n",
           num_results);
    printf("  interesting_wins.pgn: %d games   errorgames.pgn: %d games\n",
           numb_interesting, numb_errors);
    return 0;
}
