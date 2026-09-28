/*
 * GamePairs - the automatic gamepair rescorer
 * Native C port of Stefan Pohl's Auto_Gamepairs_Rescorer_V1.6.bat.
 *
 * Scores engine matches by game pairs instead of games (after an idea by
 * Joost VandeVondele): each opening is played twice, once with each colour,
 * and the pair counts as one win (2-0 or 1.5-0.5), draw (1-1 by two draws
 * or by a win each) or loss. The pairs are written to Gamepairs_final.pgn
 * and rated with Ordo (Gamepairs_rating.txt, Gamepairs_head-to-head.txt).
 *
 * The batch cut every game after the opening, split the file into one file
 * per head-to-head with Pollock's pairSplit, and found each opening's two
 * games with pgn-extract's duplicate detection (the cut games of one opening
 * end in the same position). All of that is done here on the corpus labels
 * of the cut games; Ordo is run as an external program, as the batch did.
 *
 * GamePairs rescoring tool: idea and design (C) 2024, Stefan Pohl,
 * www.sp-cc.de. pgn-extract (C) David J. Barnes. Ordo (C) Miguel A. Ballicora.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include "pgnx.h"
#include "pgnu.h"
#include "corpus.h"

#define P    pgnu_wp
#define xrun pgnu_run

static CorpusGame *G;
static int NG;

static void die(const char *msg) { fprintf(stderr, "gamepairs: %s\n", msg); exit(1); }

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (p == NULL) die("out of memory");
    return p;
}

/* pgn-extract's -Tw/-Tb: prefix match. */
static int is_player(const char *tag, const char *name)
{
    return strncmp(tag, name, strlen(name)) == 0;
}

/* ---- index lists ---- */
typedef struct { int *v; int n, cap; } List;

static void push(List *l, int g)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->v = (int *) realloc(l->v, (size_t)l->cap * sizeof(int));
        if (l->v == NULL) die("out of memory");
    }
    l->v[l->n++] = g;
}

static void cat(List *dst, const List *a)
{
    for (int i = 0; i < a->n; i++) push(dst, a->v[i]);
}

/* Duplicate detection over games read in order (pgn-extract -D and -d):
 * exact keys are final + cumulative position hash; fuzzy (--fuzzydepth 0)
 * is the final position alone. */
typedef struct { unsigned long long f, c; } Key;
typedef struct { Key *k; unsigned char *used; size_t mask; } KeySet;

static void ks_init(KeySet *s, int expected)
{
    size_t cap = 16;
    while (cap < (size_t)expected * 2 + 1) cap <<= 1;
    s->k = (Key *) xmalloc(cap * sizeof(Key));
    s->used = (unsigned char *) calloc(cap, 1);
    if (s->used == NULL) die("out of memory");
    s->mask = cap - 1;
}

static void ks_free(KeySet *s) { free(s->k); free(s->used); }

/* Insert; return 1 if already present. */
static int ks_add(KeySet *s, unsigned long long f, unsigned long long c)
{
    size_t h = (size_t)((f ^ (c * 0x9E3779B97F4A7C15ULL)) >> 7) & s->mask;
    while (s->used[h]) {
        if (s->k[h].f == f && s->k[h].c == c) return 1;
        h = (h + 1) & s->mask;
    }
    s->used[h] = 1;
    s->k[h].f = f;
    s->k[h].c = c;
    return 0;
}

/* "pgn-extract -U -d<out> a b": the games of a then b that repeat an
 * earlier one (the second game of each pair). */
static void dups(const List *a, const List *b, List *out)
{
    KeySet s;
    ks_init(&s, a->n + (b != NULL ? b->n : 0));
    const List *src[2] = { a, b };
    for (int j = 0; j < 2; j++) {
        if (src[j] == NULL) continue;
        for (int i = 0; i < src[j]->n; i++) {
            const CorpusGame *g = &G[src[j]->v[i]];
            if (ks_add(&s, g->hash_final, g->hash_cumul)) push(out, src[j]->v[i]);
        }
    }
    ks_free(&s);
}

/* "pgn-extract --fuzzydepth 0 -D": first game of each final position. */
static void unique_final(const List *a, List *out)
{
    KeySet s;
    ks_init(&s, a->n);
    for (int i = 0; i < a->n; i++)
        if (!ks_add(&s, G[a->v[i]].hash_final, 0)) push(out, a->v[i]);
    ks_free(&s);
}

/* Replace every occurrence of from with to (Pollock's textReplace). */
static char *replace_all(char *s, const char *from, const char *to)
{
    size_t fl = strlen(from), tl = strlen(to), n = 0;
    for (const char *p = s; (p = strstr(p, from)) != NULL; p += fl) n++;
    if (n == 0) return s;
    char *out = (char *) xmalloc(strlen(s) + n * (tl > fl ? tl - fl : 0) + 1), *o = out;
    const char *p = s, *q;
    while ((q = strstr(p, from)) != NULL) {
        memcpy(o, p, (size_t)(q - p));
        o += q - p;
        memcpy(o, to, tl);
        o += tl;
        p = q + fl;
    }
    strcpy(o, p);
    free(s);
    return out;
}

static void write_games(FILE *o, const List *l, int as_draws)
{
    for (int i = 0; i < l->n; i++) {
        long len;
        char *t = corpus_text(l->v[i], &len);
        if (as_draws) {   /* the batch's textReplace over the whole file */
            t = replace_all(t, "1-0", "1/2-1/2");
            t = replace_all(t, "0-1", "1/2-1/2");
        }
        fputs(t, o);
        free(t);
    }
}

/* ---- the head-to-head files (Pollock's pairSplit) ---- */
typedef struct { const char *lo, *hi; List games; } Box;

static int cmp_box(const void *a, const void *b)
{
    const Box *x = a, *y = b;
    int c = strcmp(x->lo, y->lo);
    return c ? c : strcmp(x->hi, y->hi);
}

static const char *const *SORTNAMES;
static int cmp_game_pair(const void *a, const void *b)
{
    int ga = *(const int *)a, gb = *(const int *)b;
    const char *la = SORTNAMES[2 * ga], *ha = SORTNAMES[2 * ga + 1];
    const char *lb = SORTNAMES[2 * gb], *hb = SORTNAMES[2 * gb + 1];
    int c = strcmp(la, lb);
    if (c == 0) c = strcmp(ha, hb);
    return c ? c : (ga < gb ? -1 : ga > gb);
}

static Box *split_pairs(int *nbox)
{
    const char **names = (const char **) xmalloc((size_t)NG * 2 * sizeof(char *));
    int *order = (int *) xmalloc((size_t)NG * sizeof(int));
    for (int i = 0; i < NG; i++) {
        int sw = strcmp(G[i].white, G[i].black) > 0;
        names[2 * i] = sw ? G[i].black : G[i].white;
        names[2 * i + 1] = sw ? G[i].white : G[i].black;
        order[i] = i;
    }
    SORTNAMES = names;
    qsort(order, (size_t)NG, sizeof(int), cmp_game_pair);   /* stable by index */
    Box *boxes = (Box *) xmalloc((size_t)(NG ? NG : 1) * sizeof(Box));
    int nb = 0;
    for (int k = 0; k < NG; k++) {
        int g = order[k];
        if (nb == 0 || strcmp(boxes[nb - 1].lo, names[2 * g]) != 0 ||
            strcmp(boxes[nb - 1].hi, names[2 * g + 1]) != 0) {
            boxes[nb].lo = names[2 * g];
            boxes[nb].hi = names[2 * g + 1];
            memset(&boxes[nb].games, 0, sizeof(List));
            nb++;
        }
        push(&boxes[nb - 1].games, g);
    }
    free(order);
    /* names stay referenced by the boxes; freed at exit */
    qsort(boxes, (size_t)nb, sizeof(Box), cmp_box);
    *nbox = nb;
    return boxes;
}

/* ---- Ordo ---- */

/* The Ordo executable: --ordo, then $SPCT_ORDO, then the copy installed
 * next to spct (build.bat bundles it), then ordo-win64 / ordo on PATH. */
static const char *find_ordo(const char *opt)
{
    if (opt != NULL && opt[0]) return opt;
    const char *env = getenv("SPCT_ORDO");
    if (env != NULL && env[0]) return env;
    static char found[1024];
    char dir[1024];
    pgnu_exe_dir(dir, sizeof dir);
    if (dir[0]) {
        static const char *local[3] = { "ordo-win64.exe", "ordo.exe", "ordo" };
        for (int c = 0; c < 3; c++) {
            snprintf(found, sizeof found, "%s/%s", dir, local[c]);
            FILE *f = fopen(found, "rb");
            if (f) { fclose(f); return found; }
        }
    }
#ifdef _WIN32
    const char *cands[2] = { "ordo-win64.exe", "ordo.exe" };
    const char *path = getenv("PATH");
    for (int c = 0; c < 2 && path != NULL; c++) {
        const char *p = path;
        while (*p) {
            const char *e = strchr(p, ';');
            size_t n = e ? (size_t)(e - p) : strlen(p);
            if (n > 0 && n < sizeof found - 32) {
                snprintf(found, sizeof found, "%.*s\\%s", (int)n, p, cands[c]);
                FILE *f = fopen(found, "rb");
                if (f) { fclose(f); return found; }
            }
            p += n + (e ? 1 : 0);
            if (!e) break;
        }
    }
    return NULL;
#else
    return "ordo";
#endif
}

/* Run a program with arguments; returns its exit status (-1 if it could not
 * be started). On Windows each argument is quoted for the child's parser. */
static int run_prog(const char *prog, char *const argv[])
{
#ifdef _WIN32
    char *qv[64];
    int n = 0;
    for (; argv[n] != NULL && n < 63; n++) {
        const char *a = argv[n];
        char *q = (char *) xmalloc(strlen(a) * 2 + 3), *o = q;
        *o++ = '"';
        for (; *a; a++) { if (*a == '"') *o++ = '\\'; *o++ = *a; }
        *o++ = '"';
        *o = '\0';
        qv[n] = q;
    }
    qv[n] = NULL;
    fflush(stdout);
    intptr_t rc = _spawnv(_P_WAIT, prog, (const char *const *) qv);
    for (int i = 0; i < n; i++) free(qv[i]);
    return (int) rc;
#else
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) { dup2(dn, 1); close(dn); }
        execvp(prog, argv);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
#endif
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* Read a whole file (NULL if missing). */
static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = (char *) xmalloc((size_t)n + 1);
    size_t got = fread(b, 1, (size_t)n, f);
    b[got] = '\0';
    fclose(f);
    return b;
}

static void ask(const char *prompt, char *buf, size_t n)
{
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) buf[0] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';
}

int cmd_gamepairs(int argc, char *argv[])
{
    const char *infile = NULL, *ref_engine = NULL, *ordo_opt = NULL;
    const char *plies_arg = NULL, *elo_arg = NULL;
    int no_ordo = 0;
    char work[1024] = "";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--plies") == 0 && i + 1 < argc) plies_arg = argv[++i];
        else if (strcmp(argv[i], "--ref-engine") == 0 && i + 1 < argc) ref_engine = argv[++i];
        else if (strcmp(argv[i], "--ref-elo") == 0 && i + 1 < argc) elo_arg = argv[++i];
        else if (strcmp(argv[i], "--ordo") == 0 && i + 1 < argc) ordo_opt = argv[++i];
        else if (strcmp(argv[i], "--no-ordo") == 0) no_ordo = 1;
        else if (strncmp(argv[i], "--work=", 7) == 0) snprintf(work, sizeof work, "%s", argv[i] + 7);
        else infile = argv[i];
    }
    /* The batch took its four parameters from lines at its top; ask for
     * them when no file is named. */
    char namebuf[1024], pliesbuf[32], refbuf[256], elobuf[32];
    if (!infile) {
        ask("Enter the name of your games pgn-file (with or without .pgn):\n",
            namebuf, sizeof namebuf);
        infile = namebuf;
        if (!plies_arg) {
            ask("Length of the opening lines in plies (Return = 16):\n", pliesbuf, sizeof pliesbuf);
            plies_arg = pliesbuf;
        }
        if (!ref_engine) {
            ask("Reference engine for Ordo (Return = none):\n", refbuf, sizeof refbuf);
            ref_engine = refbuf;
        }
        if (!elo_arg) {
            ask("Elo of the reference engine (Return = 0):\n", elobuf, sizeof elobuf);
            elo_arg = elobuf;
        }
    }
    char gamebase[1100];
    size_t il = strlen(infile);
    if (il > 4 && strcmp(infile + il - 4, ".pgn") == 0)
        snprintf(gamebase, sizeof gamebase, "%s", infile);
    else
        snprintf(gamebase, sizeof gamebase, "%s.pgn", infile);
    int plies = plies_arg && plies_arg[0] ? atoi(plies_arg) : 16;
    int ref_elo = elo_arg && elo_arg[0] ? atoi(elo_arg) : 0;
    if (ref_engine != NULL && ref_engine[0] == '\0') ref_engine = NULL;

    (void) pgnu_data_dir();
    if (work[0] == '\0') snprintf(work, sizeof work, "build/gp_work");
    pgnu_init(work);

    const char *F_FINAL = "Gamepairs_final.pgn";
    const char *F_H2H = "Gamepairs_head-to-head.txt";
    const char *F_RATING = "Gamepairs_rating.txt";
    remove(F_FINAL);
    remove(F_H2H);
    remove(F_RATING);

    /* Step 1: every game cut after the opening, seven tags, no comments. */
    const char *cut = P("cut.pgn");
    char plyarg[16];
    snprintf(plyarg, sizeof plyarg, "%d", plies);
    xrun("--quiet", "-C", "-N", "-V", "--seven", "--plylimit", plyarg, gamebase,
         "--output", (char *)cut, NULL);
    corpus_load(cut, NULL, &G, &NG);

    /* Step 2: one box per head-to-head. */
    int nbox;
    Box *boxes = split_pairs(&nbox);
    printf("Number of head-to-head files generated: %d\n", nbox);
    if (nbox >= 996) {
        fprintf(stderr, "gamepairs: ERROR: Overflow! Too many head-to-head files "
                "(the batch handles at most 995)\n");
        return 1;
    }

    /* Step 3: the pairs of every box, from the point of view of its
     * nameList-first engine. */
    const char *raw = P("pairs_raw.pgn");
    FILE *o = fopen(raw, "wb");
    if (!o) die("cannot write the pairs file");
    long all_drawpairs = 0, all_winwinpairs = 0;
    for (int b = 0; b < nbox; b++) {
        const char *e1 = boxes[b].lo;
        List ww = {0}, wb = {0}, lb = {0}, lw = {0}, dr = {0}, ud = {0};
        for (int i = 0; i < boxes[b].games.n; i++) {
            int g = boxes[b].games.v[i];
            const CorpusGame *c = &G[g];
            if (is_player(c->white, e1) && c->result == 1) push(&ww, g);
            if (is_player(c->black, e1) && c->result == -1) push(&wb, g);
            if (is_player(c->black, e1) && c->result == 1) push(&lb, g);
            if (is_player(c->white, e1) && c->result == -1) push(&lw, g);
            if (c->result == 0) push(&dr, g);
        }
        unique_final(&dr, &ud);

        List p2000 = {0}, pA = {0}, pB = {0}, pC = {0}, pD = {0}, pE = {0};
        List p1505A = {0}, p1505B = {0}, p1505 = {0};
        List p0515A = {0}, p0515B = {0}, p0515 = {0}, p0020 = {0};
        dups(&ww, &wb, &p2000);
        dups(&dr, NULL, &pA);
        all_drawpairs += pA.n;
        dups(&ww, &lb, &pB);
        dups(&lw, &wb, &pC);
        all_winwinpairs += pB.n + pC.n;
        cat(&pD, &pA); cat(&pD, &pB);
        cat(&pE, &pC); cat(&pE, &pD);
        dups(&ud, &ww, &p1505A);
        dups(&ud, &wb, &p1505B);
        cat(&p1505, &p1505A); cat(&p1505, &p1505B);
        dups(&ud, &lw, &p0515A);
        dups(&ud, &lb, &p0515B);
        cat(&p0515, &p0515A); cat(&p0515, &p0515B);
        dups(&lw, &lb, &p0020);

        /* won pairs, the drawn ones (relabelled as draws), lost pairs */
        write_games(o, &p2000, 0);
        write_games(o, &p1505, 0);
        write_games(o, &pE, 1);
        write_games(o, &p0020, 0);
        write_games(o, &p0515, 0);

        List *all[] = { &ww, &wb, &lb, &lw, &dr, &ud, &p2000, &pA, &pB, &pC, &pD, &pE,
                        &p1505A, &p1505B, &p1505, &p0515A, &p0515B, &p0515, &p0020 };
        for (size_t k = 0; k < sizeof all / sizeof all[0]; k++) free(all[k]->v);
    }
    fclose(o);
    xrun("--quiet", "-C", "-N", "-V", (char *)raw, "--output", (char *)F_FINAL, NULL);

    /* Step 4: Ordo, with the batch's arguments, and its table relabelled. */
    const char *ordo = no_ordo ? NULL : find_ordo(ordo_opt);
    const char *rating = P("rating.dat");
    remove(rating);
    int ordo_ok = 0;
    if (ordo != NULL) {
        char elo[32];
        snprintf(elo, sizeof elo, "%d", ref_elo);
        char *av[32];
        int n = 0;
        av[n++] = (char *)ordo;
        av[n++] = "-n"; av[n++] = "1"; av[n++] = "-J";
        av[n++] = "-j"; av[n++] = (char *)F_H2H;
        av[n++] = "-q"; av[n++] = "-N"; av[n++] = "0,1";
        av[n++] = "-U"; av[n++] = "0,1,2,4,7,8,9,5,6";
        av[n++] = "-a"; av[n++] = elo;
        if (ref_engine != NULL) { av[n++] = "-A"; av[n++] = (char *)ref_engine; }
        av[n++] = "-o"; av[n++] = (char *)rating;
        av[n++] = "-p"; av[n++] = (char *)F_FINAL;
        av[n++] = "-s500";
        av[n] = NULL;
        int rc = run_prog(ordo, av);
        ordo_ok = rc == 0 && file_exists(rating);
        if (!ordo_ok)
            fprintf(stderr, "gamepairs: Ordo (%s) failed; no rating list\n", ordo);
    } else if (!no_ordo) {
        fprintf(stderr, "gamepairs: Ordo not found (use --ordo PATH or set SPCT_ORDO);"
                " writing the pairs and their counts only\n");
    }

    FILE *r = fopen(F_RATING, "wb");
    if (!r) die("cannot write Gamepairs_rating.txt");
    if (ordo_ok) {
        char *t = slurp(rating);
        /* The batch's textReplace calls. Its " (%) " lost the % to cmd's
         * variable expansion, so it looks for " () ", which never occurs. */
        t = replace_all(t, "RATING", "  Celo");
        t = replace_all(t, " () ", "Score");
        t = replace_all(t, "PLAYED", " Pairs");
        t = replace_all(t, "ERROR", "Error");
        fputs(t, r);
        free(t);
    }
    long all_pairs = pgnu_count(F_FINAL);
    long drawn = all_drawpairs + all_winwinpairs;
    fprintf(r, "------------------------------------------------------------------- \r\n");
    fprintf(r, "--- Number of all Gamepairs          : %ld \r\n", all_pairs);
    fprintf(r, "--- Number of drawn Gamepairs overall: %ld (= %s) \r\n", drawn,
            pgnu_pct(all_pairs, drawn).s);
    fprintf(r, "--- Number of 1:1 drawn Gamepairs    : %ld (= %s) \r\n", all_winwinpairs,
            pgnu_pct(all_pairs, all_winwinpairs).s);
    fprintf(r, "--- Number of 2-draws drawn Gamepairs: %ld (= %s) \r\n", all_drawpairs,
            pgnu_pct(all_pairs, all_drawpairs).s);
    fprintf(r, "------------------------------------------------------------------- \r\n");
    fclose(r);

    char *shown = slurp(F_RATING);
    if (shown) { fputs(shown, stdout); free(shown); }
    printf("Gamepairs: %ld in %s\n", all_pairs, F_FINAL);
    return 0;
}
