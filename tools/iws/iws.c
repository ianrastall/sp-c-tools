/*
 * IWS - Interesting Wins Search Tool
 * Native C port of Stefan Pohl's Interesting_Wins_Search_Tool_V4.1.bat.
 *
 * Filters the "spectacular" wins out of a PGN into two sorted, annotated
 * files - no statistics, just the games. It is essentially the
 * interesting_wins.pgn half of EAS, with a game-length limit, an optional
 * player filter, and two output tiers. Like EAS, it labels every game in
 * one streaming pass (core/corpus.h) and uses the vendored in-process
 * pgn-extract only for the initial filter, length sort and final dedup.
 *
 * Outputs:
 *   interesting_wins.pgn       - queen/5/4/3/2/1 pawn sacs, wins before the
 *                                endgame, and material imbalances
 *   very_interesting_wins.pgn  - same minus the 1-pawn sacs and imbalances
 *
 * Idea and original tool (C) Stefan Pohl. pgn-extract (C) David J. Barnes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "pgnx.h"
#include "pgnu.h"
#include "corpus.h"

/* Data directories, resolved at startup via pgnu_data_dir(). */
static char PATTERN_DIR[1300];     /* <data>/patterns */
static char ANNO_DIR[1300];        /* <data>/anno_iws */

static char WORK[1024];

/* Shared plumbing lives in core/pgnu; alias to this tool's names. */
#define P               pgnu_wp
#define flag            pgnu_flag
#define xrun            pgnu_run
#define count_games     pgnu_count
#define annotate_append pgnu_annotate_append

static void read_anno(const char *name, char *out, size_t n)
{
    pgnu_read_anno(ANNO_DIR, name, out, n);
}

/* IWS length-sort uses 10-move buckets. */
static const int IWS_SLLO[] = {0,20,30,40,50,60,70,80,90,100,110,120};
static const int IWS_SLHI[] = {19,29,39,49,59,69,79,89,99,109,119,0};
static void sortlength(const char *src, const char *dst)
{
    pgnu_sortlength(src, dst, IWS_SLLO, IWS_SLHI, 12);
}

int cmd_iws(int argc, char *argv[])
{
    const char *infile = NULL;
    const char *player = NULL;
    int movelimit = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--moves") == 0 && i + 1 < argc)
            movelimit = atoi(argv[++i]);
        else if (strcmp(argv[i], "--player") == 0 && i + 1 < argc)
            player = argv[++i];
        else if (strncmp(argv[i], "--work=", 7) == 0)
            snprintf(WORK, sizeof WORK, "%s", argv[i] + 7);
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

    /* Movelimit: batch default 100, <30 -> 100, >=250 -> 250. */
    if (movelimit < 30) movelimit = 100;
    if (movelimit >= 250) movelimit = 250;
    char bulimit[16];
    snprintf(bulimit, sizeof bulimit, "-bu%d", movelimit);

    const char *data = pgnu_data_dir();
    snprintf(PATTERN_DIR, sizeof PATTERN_DIR, "%s/patterns", data);
    snprintf(ANNO_DIR, sizeof ANNO_DIR, "%s/anno_iws", data);

    if (WORK[0] == '\0') snprintf(WORK, sizeof WORK, "build/iws_work");
    pgnu_init(WORK);

    const char *newsource = P("newsource.pgn");
    /* Won games up to the move limit; strip comments/NAGs/variations. */
    if (!player) {
        xrun("--quiet", bulimit, "--fixresulttags", "-Tr1-0", "-Tr0-1",
             "-C", "-N", "-V", (char *)gamebase, "--output", (char *)newsource, NULL);
    } else {
        xrun("--quiet", flag("-Tw", player), bulimit, "--fixresulttags", "-Tr1-0",
             "-C", "-N", "-V", (char *)gamebase, "--output", (char *)newsource, NULL);
        xrun("--quiet", flag("-Tb", player), bulimit, "--fixresulttags", "-Tr0-1",
             "-C", "-N", "-V", (char *)gamebase, flag("-a", newsource), NULL);
    }
    if (count_games(newsource) == 0) {
        fprintf(stderr, "iws: no matching won games in %s\n", gamebase);
        return 1;
    }

    /* ---- one streaming pass labels every won game ----
     * (sacrifice chain, endgame reached, material imbalance); the batch's
     * filter passes become index-list operations, and each category file is
     * written straight from newsource's text. */
    CorpusGame *G;
    int NG;
    corpus_load(newsource, PATTERN_DIR, &G, &NG);
    int *W = (int *) malloc(((size_t)NG + 1) * sizeof(int));
    int *B = (int *) malloc(((size_t)NG + 1) * sizeof(int));
    int *all = (int *) malloc(((size_t)NG + 1) * sizeof(int));
    int *sel = (int *) malloc(((size_t)NG + 1) * sizeof(int));
    int *kept = (int *) malloc(((size_t)NG + 1) * sizeof(int));
    if (!W || !B || !all || !sel || !kept) {
        fprintf(stderr, "iws: out of memory\n");
        return 1;
    }
    int nW = 0, nB = 0;
    for (int i = 0; i < NG; i++) {
        all[i] = i;
        if (G[i].result == 1) W[nW++] = i;
        else if (G[i].result == -1) B[nB++] = i;
    }

    /* ---- sacrifice categories (shared classifier) ---- */
    const char *u1 = P("unique_opt1.pgn");
    const char *u2 = P("unique_opt2.pgn");
    const char *u3 = P("unique_opt3.pgn");
    const char *u4 = P("unique_opt4.pgn");
    const char *u5 = P("unique_opt5.pgn");
    const char *u9 = P("unique_opt9.pgn");
    const char *const unique[CORPUS_SAC_LEVELS] = { u1, u2, u3, u4, u5, u9 };
    CorpusSacSets S;
    corpus_sac_classify(W, nW, B, nB, &S);
    for (int k = 0; k < CORPUS_SAC_LEVELS; k++) {
        pgnu_truncate(unique[k]);
        corpus_append(unique[k], S.list[k], S.n[k]);
    }
    corpus_sac_free(&S);

    /* ---- wins before an endgame, and material imbalances ---- */
    const char *no_endgame = P("no_endgame_wins.pgn");
    const char *imbalance = P("imbalance.pgn");
    int m = 0;
    for (int i = 0; i < NG; i++) if (G[i].no_endgame) sel[m++] = i;
    int nne = corpus_dedup(all, NG, sel, m, kept);   /* newsource minus reached */
    pgnu_truncate(no_endgame);
    corpus_append(no_endgame, kept, nne);
    m = 0;
    for (int i = 0; i < NG; i++) if (G[i].imbalance) sel[m++] = i;
    pgnu_truncate(imbalance);
    corpus_append(imbalance, sel, m);
    free(W); free(B); free(all); free(sel); free(kept);

    /* ---- assemble the two tiers ----
     * interesting:      9,5,4,3,2,1 sacs, before-endgame, imbalance
     * very_interesting: 9,5,4,3,2   sacs, before-endgame  (no 1-sac, no imbalance)
     */
    const char *found = P("foundgames.pgn");
    const char *foundtop = P("foundtopgames.pgn");
    const char *sorted = P("sortedlength.pgn");
    { FILE *f = fopen(found, "wb"); if (f) fclose(f); }
    { FILE *f = fopen(foundtop, "wb"); if (f) fclose(f); }

    struct { const char *src, *anno; int top; } cats[8] = {
        { u9, "anno_9sac", 1 },
        { u5, "anno_5sac", 1 },
        { u4, "anno_4sac", 1 },
        { u3, "anno_3sac", 1 },
        { u2, "anno_2sac", 1 },
        { u1, "anno_1sac", 0 },
        { no_endgame, "anno_before_endgame", 1 },
        { imbalance, "anno_material_imbalance", 0 },
    };
    for (int i = 0; i < 8; i++) {
        if (count_games(cats[i].src) <= 0) continue;
        sortlength(cats[i].src, sorted);
        char a[256];
        read_anno(cats[i].anno, a, sizeof a);
        annotate_append(sorted, a, found);
        if (cats[i].top) annotate_append(sorted, a, foundtop);
    }

    xrun("--quiet", "-D", (char *)found, "--output", "interesting_wins.pgn", NULL);
    xrun("--quiet", "-D", (char *)foundtop, "--output", "very_interesting_wins.pgn", NULL);

    int ni = (int)count_games("interesting_wins.pgn");
    int nv = (int)count_games("very_interesting_wins.pgn");
    printf("Done. movelimit=%d%s%s\n", movelimit,
           player ? "  player=" : "", player ? player : "");
    printf("  interesting_wins.pgn:      %d games\n", ni);
    printf("  very_interesting_wins.pgn: %d games\n", nv);
    return 0;
}
