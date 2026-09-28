/*
 * SGA - Short Games Analyzer Tool
 * Native C port of Stefan Pohl's Short_Games_Analyzer_V3.3.bat and
 * Short_Games_Analyzer_no_endgames_V3.3.bat (--no-endgame).
 *
 * Per engine/player: the share of its wins, draws and losses that were
 * short (up to 30, 35, ..., 60 moves), and how many of its short wins
 * contain a sacrifice, plus score, draw rate and average game lengths.
 * Four rating lists (full, wins, draws, losses), all short games, and the
 * short sacrifice games. The no-endgame variant counts only short games
 * that ended before an endgame was reached.
 *
 * Every game is labelled in one streaming pass (core/corpus.h); the batch's
 * per-engine filter passes, Pollock's nameList and summary, and its sort
 * pipeline are all reproduced in memory.
 *
 * Short Games Analyzer Tool: idea and design (C) 2024, Stefan Pohl,
 * www.sp-cc.de. pgn-extract (C) David J. Barnes.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pgnx.h"
#include "pgnu.h"
#include "corpus.h"
#include "sga_templates.h"

#define P    pgnu_wp
#define xrun pgnu_run

static char PATTERN_DIR[1300];
static char WORK[1024];
static CorpusGame *G;
static int NG;

static void die(const char *msg) { fprintf(stderr, "sga: %s\n", msg); exit(1); }

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (p == NULL) die("out of memory");
    return p;
}

/* ---- players (Pollock's nameList: unique names in byte order) ---- */

static char **NAMES;
static int NNAMES;

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Open-addressing map from player name to index, probed with a hash that
 * is extended one character at a time, so every prefix of a name can be
 * looked up in a single left-to-right scan. */
static int *MAP;
static size_t MAPMASK;

static unsigned long long hstep(unsigned long long h, unsigned char c)
{
    return (h ^ c) * 1099511628211ULL;   /* FNV-1a */
}
#define HSEED 14695981039346656037ULL

static void map_build(void)
{
    size_t cap = 16;
    while (cap < (size_t)NNAMES * 2 + 1) cap <<= 1;
    MAP = (int *) xmalloc(cap * sizeof(int));
    for (size_t i = 0; i < cap; i++) MAP[i] = -1;
    MAPMASK = cap - 1;
    for (int e = 0; e < NNAMES; e++) {
        unsigned long long h = HSEED;
        for (const char *s = NAMES[e]; *s; s++) h = hstep(h, (unsigned char)*s);
        size_t k = (size_t)h & MAPMASK;
        while (MAP[k] >= 0) k = (k + 1) & MAPMASK;
        MAP[k] = e;
    }
}

/* The player whose full name is name[0..len), or -1. */
static int map_find(unsigned long long h, const char *name, size_t len)
{
    for (size_t k = (size_t)h & MAPMASK; MAP[k] >= 0; k = (k + 1) & MAPMASK) {
        const char *n = NAMES[MAP[k]];
        if (strlen(n) == len && strncmp(n, name, len) == 0) return MAP[k];
    }
    return -1;
}

/* ---- each player's games: pgn-extract's -Tp/-Tw/-Tb match by prefix, so
 * a game side belongs to every player whose name is a prefix of it ---- */

typedef struct { int g; unsigned char w, b; } Entry;   /* game, player is white/black */
static Entry **ENT;
static int *NENT, *CAPENT;

static void add_entry(int e, int g, int white)
{
    if (NENT[e] > 0 && ENT[e][NENT[e] - 1].g == g) {    /* both sides */
        if (white) ENT[e][NENT[e] - 1].w = 1; else ENT[e][NENT[e] - 1].b = 1;
        return;
    }
    if (NENT[e] == CAPENT[e]) {
        CAPENT[e] = CAPENT[e] ? CAPENT[e] * 2 : 16;
        ENT[e] = (Entry *) realloc(ENT[e], (size_t)CAPENT[e] * sizeof(Entry));
        if (ENT[e] == NULL) die("out of memory");
    }
    Entry *x = &ENT[e][NENT[e]++];
    x->g = g;
    x->w = white ? 1 : 0;
    x->b = white ? 0 : 1;
}

static void index_players(void)
{
    /* nameList: every distinct White/Black name, in byte order. */
    char **all = (char **) xmalloc((size_t)NG * 2 * sizeof(char *));
    int n = 0;
    for (int i = 0; i < NG; i++) { all[n++] = G[i].white; all[n++] = G[i].black; }
    qsort(all, (size_t)n, sizeof(char *), cmp_str);
    NAMES = (char **) xmalloc((size_t)(n ? n : 1) * sizeof(char *));
    NNAMES = 0;
    for (int i = 0; i < n; i++)
        if (NNAMES == 0 || strcmp(NAMES[NNAMES - 1], all[i]) != 0) NAMES[NNAMES++] = all[i];
    free(all);
    map_build();

    ENT = (Entry **) xmalloc((size_t)(NNAMES ? NNAMES : 1) * sizeof(Entry *));
    NENT = (int *) xmalloc((size_t)(NNAMES ? NNAMES : 1) * sizeof(int));
    CAPENT = (int *) xmalloc((size_t)(NNAMES ? NNAMES : 1) * sizeof(int));
    for (int e = 0; e < NNAMES; e++) { ENT[e] = NULL; NENT[e] = CAPENT[e] = 0; }
    for (int i = 0; i < NG; i++)
        for (int side = 0; side < 2; side++) {
            const char *name = side == 0 ? G[i].white : G[i].black;
            unsigned long long h = HSEED;
            for (size_t len = 1; name[len - 1]; len++) {
                h = hstep(h, (unsigned char)name[len - 1]);
                int e = map_find(h, name, len);
                if (e >= 0) add_entry(e, i, side == 0);
            }
        }
}

/* ---- per-player figures ---- */

static int le_moves(int g, int moves)   /* pgn-extract -bu<moves> */
{
    return CORPUS_MOVES_LE(G[g].ply_offset + G[g].plies, moves);
}

static int avg_moves(const int *list, int n)
{
    long long plies = 0;
    for (int i = 0; i < n; i++) plies += G[list[i]].plies;
    return pgnu_moveaverage(plies, n);
}

/* The batch's :formatnumb: right-aligned in 6 columns. */
static void fmt6(char *out, long v)
{
    snprintf(out, 16, "%6ld", v);
}

/* A player's report values, by the batch's variable names. */
#define NVALS 96
typedef struct {
    const char *key[NVALS];
    char val[NVALS][24];                 /* numbers and percentages */
    char engine[CORPUS_NAME_LEN + 4];    /* "%engine%": the quoted name */
    int n;
    int engine_no;       /* 1-based, in nameList order */
    long points, points_wins, points_draws, points_losses;
} Report;

static void setv(Report *r, const char *key, const char *val)
{
    if (r->n >= NVALS) die("internal: too many report values");
    r->key[r->n] = key;
    snprintf(r->val[r->n], sizeof r->val[0], "%s", val);
    r->n++;
}

static void setl(Report *r, const char *key, long v)
{
    char b[32];
    snprintf(b, sizeof b, "%ld", v);
    setv(r, key, b);
}

static const char *getv(const Report *r, const char *key, size_t klen)
{
    if (klen == 6 && strncmp(key, "engine", 6) == 0) return r->engine;
    for (int i = 0; i < r->n; i++)
        if (strlen(r->key[i]) == klen && strncmp(r->key[i], key, klen) == 0)
            return r->val[i];
    return "";   /* an undefined batch variable expands to nothing */
}

/* Keep the games of in[] that are within 60 moves and did not reach an
 * endgame: the no-endgame batch's "-bu60 -zno_endgame" then
 * "-bu60 -c<those> -D" over each of its win/draw/loss files. */
static int no_endgame_filter(int *in, int n, int *tmp)
{
    int m = 0, k = 0;
    for (int i = 0; i < n; i++) if (le_moves(in[i], 60)) in[m++] = in[i];
    for (int i = 0; i < m; i++) if (G[in[i]].no_endgame) tmp[k++] = in[i];
    return corpus_dedup(in, m, tmp, k, in);
}

/* Short-game buckets of set[]: counts up to 30, 35, ..., 60 moves, as
 * percentages of base. prefix is "wins", "draws" or "losses". Returns the
 * percentage (x100) of all short games. */
static long buckets(Report *r, const char *prefix, const char *all_pct_key,
                    const char *all_n_key, const int *set, int n, long base)
{
    static const int lim[7] = { 30, 35, 40, 45, 50, 55, 60 };
    static char keys[3][7][2][40];
    int slot = prefix[0] == 'w' ? 0 : prefix[0] == 'd' ? 1 : 2;
    long cum[7] = { 0 };
    for (int i = 0; i < n; i++)
        for (int b = 0; b < 7; b++)
            if (le_moves(set[i], lim[b])) cum[b]++;
    char buf[16];
    long prev = 0;
    for (int b = 0; b < 7; b++) {
        long cnt = cum[b] - prev;
        prev = cum[b];
        snprintf(keys[slot][b][0], 40, "list_%s_%dmvs", prefix, lim[b]);
        snprintf(keys[slot][b][1], 40, "list_%s_%dmvs_pcnt", prefix, lim[b]);
        fmt6(buf, cnt);
        setv(r, keys[slot][b][0], buf);
        setv(r, keys[slot][b][1], pgnu_pct(base, cnt).s);
    }
    PgnuPct all = pgnu_pct(base, cum[6]);
    fmt6(buf, cum[6]);
    setv(r, all_n_key, buf);
    setv(r, all_pct_key, all.s);
    return all.x100;
}

/* Sacrifice games found per level (0..5 = 1,2,3,4,5,queen), all players. */
static int *SAC[CORPUS_SAC_LEVELS];
static int NSAC[CORPUS_SAC_LEVELS], CAPSAC[CORPUS_SAC_LEVELS];

static void sac_collect(int k, const int *list, int n)
{
    if (NSAC[k] + n > CAPSAC[k]) {
        CAPSAC[k] = (NSAC[k] + n) * 2;
        SAC[k] = (int *) realloc(SAC[k], (size_t)CAPSAC[k] * sizeof(int));
        if (SAC[k] == NULL) die("out of memory");
    }
    memcpy(SAC[k] + NSAC[k], list, (size_t)n * sizeof(int));
    NSAC[k] += n;
}

static void analyse(int e, int no_endgame, Report *r)
{
    const Entry *x = ENT[e];
    int n = NENT[e];
    size_t cap = (size_t)n * 2 + 1;
    int *all = (int *) xmalloc(cap * sizeof(int)), nall = 0;
    int *ww = (int *) xmalloc(cap * sizeof(int)), nww = 0;   /* white wins */
    int *bw = (int *) xmalloc(cap * sizeof(int)), nbw = 0;   /* black wins */
    int *wins = (int *) xmalloc(cap * sizeof(int)), nwins = 0;
    int *draws = (int *) xmalloc(cap * sizeof(int)), ndraws = 0;
    int *losses = (int *) xmalloc(cap * sizeof(int)), nlosses = 0;
    int *wl = (int *) xmalloc(cap * sizeof(int)), nwl = 0;   /* white losses */
    int *bl = (int *) xmalloc(cap * sizeof(int)), nbl = 0;   /* black losses */
    int *tmp = (int *) xmalloc(cap * sizeof(int));
    int *side[2][4];   /* [white/black][all, wins, draws, losses] */
    int nside[2][4] = { { 0 } };
    for (int s = 0; s < 2; s++)
        for (int k = 0; k < 4; k++) side[s][k] = (int *) xmalloc(cap * sizeof(int));

    for (int i = 0; i < n; i++) {
        int g = x[i].g, res = G[g].result;
        all[nall++] = g;
        if (x[i].w && res == 1) ww[nww++] = g;
        if (x[i].b && res == -1) bw[nbw++] = g;
        if (res == 0) draws[ndraws++] = g;
        if (x[i].w && res == -1) wl[nwl++] = g;
        if (x[i].b && res == 1) bl[nbl++] = g;
        if (x[i].w) {
            side[0][0][nside[0][0]++] = g;
            if (res == 1) side[0][1][nside[0][1]++] = g;
            else if (res == 0) side[0][2][nside[0][2]++] = g;
            else if (res == -1) side[0][3][nside[0][3]++] = g;
        }
        if (x[i].b) {
            side[1][0][nside[1][0]++] = g;
            if (res == -1) side[1][1][nside[1][1]++] = g;
            else if (res == 0) side[1][2][nside[1][2]++] = g;
            else if (res == 1) side[1][3][nside[1][3]++] = g;
        }
    }
    for (int i = 0; i < nww; i++) wins[nwins++] = ww[i];
    for (int i = 0; i < nbw; i++) wins[nwins++] = bw[i];
    for (int i = 0; i < nwl; i++) losses[nlosses++] = wl[i];
    for (int i = 0; i < nbl; i++) losses[nlosses++] = bl[i];

    long numb_wins = nwins, numb_draws = ndraws, numb_losses = nlosses;
    long numb_allgames = numb_wins + numb_draws + numb_losses;
    /* The batch echoed the name in quotes into its work files. */
    snprintf(r->engine, sizeof r->engine, "\"%s\"", NAMES[e]);
    setl(r, "numb_allgames", numb_allgames);
    setl(r, "numb_wins", numb_wins);
    setl(r, "numb_draws", numb_draws);
    setl(r, "numb_losses", numb_losses);
    setv(r, "engine_score", pgnu_pct(numb_allgames * 10, numb_wins * 10 + numb_draws * 5).s);
    setv(r, "engine_drawrate", pgnu_pct(numb_allgames, numb_draws).s);

    setl(r, "length_all", avg_moves(all, nall));
    setl(r, "length_wins", avg_moves(wins, nwins));
    setl(r, "length_draws", avg_moves(draws, ndraws));
    setl(r, "length_loss", avg_moves(losses, nlosses));
    static const char *lk[2][4] = {
        { "length_all_w", "length_wins_w", "length_draws_w", "length_loss_w" },
        { "length_all_b", "length_wins_b", "length_draws_b", "length_loss_b" },
    };
    for (int s = 0; s < 2; s++)
        for (int k = 0; k < 4; k++) setl(r, lk[s][k], avg_moves(side[s][k], nside[s][k]));

    if (no_endgame) {
        nwins = no_endgame_filter(wins, nwins, tmp);
        ndraws = no_endgame_filter(draws, ndraws, tmp);
        nlosses = no_endgame_filter(losses, nlosses, tmp);
    }

    /* The short wins (the batch's win60games), in wins order. */
    int nw60 = 0;
    for (int i = 0; i < nwins; i++) if (le_moves(wins[i], 60)) wins[nw60++] = wins[i];
    long pw = buckets(r, "wins", "percent_all_shortwins", "list_all_wins",
                      wins, nw60, numb_wins);
    long pd = buckets(r, "draws", "percent_all_shortdraws", "list_all_draws",
                      draws, ndraws, numb_draws);
    long pl = buckets(r, "losses", "percent_all_shortlosses", "list_all_losses",
                      losses, nlosses, numb_losses);
    r->points = 50000 + pw - pd - pl;
    r->points_wins = 10000 + pw;
    r->points_draws = 10000 + pd;
    r->points_losses = 10000 + pl;

    /* Sacrifices in the short wins: white wins then black wins. */
    int nW = 0, nB = 0;
    for (int i = 0; i < nw60; i++) {
        if (G[wins[i]].result == 1) ww[nW++] = wins[i];
        else bw[nB++] = wins[i];
    }
    CorpusSacSets S;
    corpus_sac_classify(ww, nW, bw, nB, &S);
    long sum = 0;
    for (int k = 0; k < CORPUS_SAC_LEVELS; k++) {
        sum += S.n[k];
        sac_collect(k, S.list[k], S.n[k]);
    }
    static const char *pk[CORPUS_SAC_LEVELS] = {
        "pcnt_sac1", "pcnt_sac2", "pcnt_sac3", "pcnt_sac4", "pcnt_sac5", "pcnt_sac9" };
    static const char *nk[CORPUS_SAC_LEVELS] = {
        "numb_sac1", "numb_sac2", "numb_sac3", "numb_sac4", "numb_sac5", "numb_sac9" };
    char buf[16];
    for (int k = 0; k < CORPUS_SAC_LEVELS; k++) {
        setv(r, pk[k], pgnu_pct(numb_wins, S.n[k]).s);
        fmt6(buf, S.n[k]);
        setv(r, nk[k], buf);
    }
    setv(r, "pcnt_sumsac", pgnu_pct(numb_wins, sum).s);
    fmt6(buf, sum);
    setv(r, "numb_sumsac", buf);
    corpus_sac_free(&S);

    free(all); free(ww); free(bw); free(wins); free(draws); free(losses);
    free(wl); free(bl); free(tmp);
    for (int s = 0; s < 2; s++) for (int k = 0; k < 4; k++) free(side[s][k]);
}

/* ---- the four rating lists ---- */

static void put_line(FILE *o, const char *tmpl, const Report *r, int rank)
{
    if (rank > 0) fprintf(o, "*** Rank %d: ", rank);
    for (const char *p = tmpl; *p; ) {
        if (*p == '%') {
            const char *q = strchr(p + 1, '%');
            if (q != NULL) {
                fputs(getv(r, p + 1, (size_t)(q - p - 1)), o);
                p = q + 1;
                continue;
            }
        }
        fputc(*p++, o);
    }
    fputc('\n', o);
}

static const Report *SORTR;
static int SORTKEY;   /* 0 full, 1 wins: descending; 2 draws, 3 losses: ascending */

static long rkey(const Report *r)
{
    switch (SORTKEY) {
        case 0: return r->points;
        case 1: return r->points_wins;
        case 2: return r->points_draws;
        default: return r->points_losses;
    }
}

/* The batch sorted the lines by "<points><3-digit engine number>": reverse
 * for full/wins, ascending for draws/losses, so ties go by engine number. */
static int cmp_rep(const void *a, const void *b)
{
    const Report *x = &SORTR[*(const int *)a], *y = &SORTR[*(const int *)b];
    long kx = rkey(x), ky = rkey(y);
    int c = kx != ky ? (kx < ky ? -1 : 1) : (x->engine_no < y->engine_no ? -1 : 1);
    return SORTKEY <= 1 ? -c : c;
}

static int write_list(const char *path, const char *gamebase, int no_endgame,
                      const Report *reps, int n, int which,
                      const char *const *tmpl, int lines)
{
    FILE *o = fopen(path, "w");
    if (!o) { fprintf(stderr, "sga: cannot write %s\n", path); return 1; }
    fputs("***************************************************************\n"
          "***              Short Games Analyzer Tool V3.3             ***\n"
          "***           (C) 2024, Stefan Pohl, www.sp-cc.de           ***\n"
          "***************************************************************\n", o);
    fprintf(o, "*** Evaluated file: %s \n", gamebase);
    if (no_endgame)
        fputs("*** Evaluated games: All ended before endgame was reached  \n", o);
    fputs("***************************************************************\n", o);
    int *order = (int *) xmalloc((size_t)(n ? n : 1) * sizeof(int));
    for (int i = 0; i < n; i++) order[i] = i;
    SORTR = reps;
    SORTKEY = which;
    qsort(order, (size_t)n, sizeof(int), cmp_rep);
    for (int i = 0; i < n; i++)
        for (int l = 0; l < lines; l++)
            put_line(o, tmpl[l], &reps[order[i]], l == 1 ? i + 1 : 0);
    free(order);
    fclose(o);
    return 0;
}

int cmd_sga(int argc, char *argv[])
{
    const char *infile = NULL;
    int no_endgame = 0;
    WORK[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-endgame") == 0) no_endgame = 1;
        else if (strncmp(argv[i], "--work=", 7) == 0)
            snprintf(WORK, sizeof WORK, "%s", argv[i] + 7);
        else infile = argv[i];
    }
    char namebuf[1024];
    if (!infile) {
        printf("Enter the name of your games pgn-file (with or without .pgn):\n");
        fflush(stdout);
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
    if (WORK[0] == '\0') snprintf(WORK, sizeof WORK, "build/sga_work");
    pgnu_init(WORK);

    const char *sfx = no_endgame ? "_no_endgame" : "";
    char f_full[64], f_wins[64], f_draws[64], f_losses[64], f_short[64], f_sac[64];
    snprintf(f_full, sizeof f_full, "SGA%s_full_statistics.txt", sfx);
    snprintf(f_wins, sizeof f_wins, "SGA%s_wins_statistics.txt", sfx);
    snprintf(f_draws, sizeof f_draws, "SGA%s_draws_statistics.txt", sfx);
    snprintf(f_losses, sizeof f_losses, "SGA%s_losses_statistics.txt", sfx);
    snprintf(f_short, sizeof f_short, "shortgames%s.pgn", sfx);
    snprintf(f_sac, sizeof f_sac, "short_sac_games%s.pgn", sfx);
    const char *outs[6] = { f_full, f_wins, f_draws, f_losses, f_short, f_sac };
    for (int i = 0; i < 6; i++) remove(outs[i]);

    /* Every game without comments, NAGs or variations, results fixed. */
    const char *newsource = P("newsource.pgn");
    xrun("--quiet", "-C", "-N", "-V", "--fixresulttags", "--plycount", gamebase,
         "--output", (char *)newsource, NULL);
    if (pgnx_games_processed() == 0) {
        fprintf(stderr, "sga: no games found in %s\n", gamebase);
        return 1;
    }

    /* All short games (comments kept): decisive ones, then draws. */
    const char *shortdst = no_endgame ? P("temp_output.pgn") : f_short;
    xrun("--quiet", "--fixresulttags", "--plycount", "-bu60", "-Tr0-1", "-Tr1-0",
         gamebase, "--output", (char *)shortdst, NULL);
    xrun("--quiet", "--fixresulttags", "--plycount", "-bu60", "-Tr1/2-1/2",
         gamebase, pgnu_flag("-a", shortdst), NULL);
    if (no_endgame) {
        char zpat[1400];
        snprintf(zpat, sizeof zpat, "-z%s/no_endgame_sga", PATTERN_DIR);
        const char *reached = P("results.pgn");
        xrun("--quiet", zpat, (char *)shortdst, "--output", (char *)reached, NULL);
        xrun("--quiet", "-bu60", pgnu_flag("-c", reached), "-D",
             pgnu_flag("-o", f_short), (char *)shortdst, NULL);
    }

    corpus_set_no_endgame_pattern("no_endgame_sga");
    corpus_load(newsource, PATTERN_DIR, &G, &NG);
    corpus_set_no_endgame_pattern(NULL);
    index_players();
    printf("Engines/players found: %d\n", NNAMES);

    Report *reps = (Report *) xmalloc((size_t)(NNAMES ? NNAMES : 1) * sizeof(Report));
    for (int e = 0; e < NNAMES; e++) {
        memset(&reps[e], 0, sizeof reps[e]);
        reps[e].engine_no = e + 1;
        analyse(e, no_endgame, &reps[e]);
    }

    /* The short sacrifice games: all players' queen sacs, then 5, 4, 3, 2
     * and 1 pawn units. */
    pgnu_truncate(f_sac);
    for (int k = CORPUS_SAC_LEVELS - 1; k >= 0; k--)
        corpus_append(f_sac, SAC[k], NSAC[k]);

    int rc = 0;
    rc |= write_list(f_full, gamebase, no_endgame, reps, NNAMES, 0, SGA_FULL, 28);
    rc |= write_list(f_wins, gamebase, no_endgame, reps, NNAMES, 1, SGA_WINS, 18);
    rc |= write_list(f_draws, gamebase, no_endgame, reps, NNAMES, 2, SGA_DRAWS, 13);
    rc |= write_list(f_losses, gamebase, no_endgame, reps, NNAMES, 3, SGA_LOSSES, 13);
    free(reps);
    if (rc) return 1;
    printf("Done. See %s (and the wins/draws/losses lists).\n", f_full);
    printf("  %s: %ld games   %s: %ld games\n", f_short, pgnu_count(f_short),
           f_sac, pgnu_count(f_sac));
    return 0;
}
