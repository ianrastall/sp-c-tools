/*
 * SGS - Sacrifice Games Search and Statistics Tool
 * Native C port of Stefan Pohl's Sacrifice_Games_Search_Tool_V3.2.bat.
 *
 * Finds the won games in which the eventual loser held a material advantage
 * (a sacrifice by the winner) for several consecutive moves. Unlike EAS and
 * IWS it keeps the games' comments, NAGs and variations.
 *
 *   level 0 (default): full search - every game in its highest category
 *     (queen, 5+, 4, 3, 2, 1 pawn units) in sacgames_*.pgn, plus
 *     statistics.txt
 *   level 1..5 / 9:    one search, for that many pawn units or more (9 =
 *     queen sacs); the games go to games_with_sacrifices.pgn, no statistics
 *
 * Every game is labelled in one streaming pass (core/corpus.h); the batch's
 * per-level -y passes and -c/-D chain are answered from those labels.
 *
 * Sacrifice Games Search and Statistics Tool: idea and design
 * (C) 2024, Stefan Pohl, www.sp-cc.de. pgn-extract (C) David J. Barnes.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pgnx.h"
#include "pgnu.h"
#include "corpus.h"

static char PATTERN_DIR[1300];     /* <data>/patterns, set in cmd_sgs */
static char WORK[1024];

#define P           pgnu_wp
#define xrun        pgnu_run

static const char *SACFILE[CORPUS_SAC_LEVELS] = {
    "sacgames_1_pawns.pgn", "sacgames_2_pawns.pgn", "sacgames_3_pawns.pgn",
    "sacgames_4_pawns.pgn", "sacgames_5_pawns.pgn", "sacgames_queensacs.pgn",
};
#define SINGLEFILE "games_with_sacrifices.pgn"
#define STATSFILE  "statistics.txt"

/* Read one answer line from stdin ("" at EOF). */
static void ask(const char *prompt, char *buf, size_t n)
{
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) buf[0] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';
}

static const char *const STARS63 =
    "***************************************************************";
static const char *const STARS85 =
    "*************************************************************************************";
static const char *const STARS52 =
    "****************************************************";

/* The batch built statistics.txt with `@echo ... >>statistics.txt`, which
 * keeps the space before the redirection: every line ends in a space. */
static void sline(FILE *o, const char *s) { fprintf(o, "%s \n", s); }

static void stat_row(FILE *o, const char *label, long n, long origin, long wins)
{
    PgnuPct all = pgnu_pct(origin, n), won = pgnu_pct(wins, n);
    fprintf(o, "%s: %ld (%s of all games) (%s of won games) \n",
            label, n, all.s, won.s);
}

static int write_statistics(const char *gamebase, long numb_origin, long numb_wins,
                            int movelimit, const int n[CORPUS_SAC_LEVELS])
{
    FILE *o = fopen(STATSFILE, "w");
    if (!o) { fprintf(stderr, "sgs: cannot write %s\n", STATSFILE); return 1; }
    char buf[1300];
    sline(o, STARS63);
    sline(o, "***     Sacrifice Games Search and Statistics Tool V3.2     ***");
    sline(o, "***           (C) 2024, Stefan Pohl, www.sp-cc.de           ***");
    sline(o, STARS63);
    snprintf(buf, sizeof buf, "*** Sacrifice-statistics of the file %s", gamebase);
    sline(o, buf);
    sline(o, STARS63);
    sline(o, STARS85);
    snprintf(buf, sizeof buf, "Source pgn-file: %s", gamebase);
    sline(o, buf);
    sline(o, STARS85);
    snprintf(buf, sizeof buf, "Number of games found in the source file: %ld", numb_origin);
    sline(o, buf);
    sline(o, STARS85);
    snprintf(buf, sizeof buf, "Number of won games found in the source file: %ld", numb_wins);
    sline(o, buf);
    sline(o, STARS85);
    snprintf(buf, sizeof buf, "Max. allowed length of games, investigated: %d moves", movelimit);
    sline(o, buf);
    sline(o, STARS85);
    long sum = 0;
    for (int k = 0; k < CORPUS_SAC_LEVELS; k++) sum += n[k];
    stat_row(o, "Number of Queen sacrifices (5+)  ", n[5], numb_origin, numb_wins);
    stat_row(o, "Number of 5+ pawnunits sacrifices", n[4], numb_origin, numb_wins);
    stat_row(o, "Number of 4 pawnunits sacrifices ", n[3], numb_origin, numb_wins);
    stat_row(o, "Number of 3 pawnunits sacrifices ", n[2], numb_origin, numb_wins);
    stat_row(o, "Number of 2 pawnunits sacrifices ", n[1], numb_origin, numb_wins);
    stat_row(o, "Number of 1 pawnunits sacrifices ", n[0], numb_origin, numb_wins);
    stat_row(o, "Number of all sacrifices         ", sum, numb_origin, numb_wins);
    sline(o, STARS85);
    sline(o, STARS52);
    sline(o, "*** SGS-tool (C) 2024 Stefan Pohl (www.sp-cc.de) ***");
    sline(o, STARS52);
    fclose(o);
    return 0;
}

static void print_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[2048];
    while (fgets(line, sizeof line, f)) fputs(line, stdout);
    fclose(f);
}

int cmd_sgs(int argc, char *argv[])
{
    const char *infile = NULL;
    const char *level_arg = NULL, *moves_arg = NULL;
    WORK[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--level") == 0 && i + 1 < argc)
            level_arg = argv[++i];
        else if (strcmp(argv[i], "--moves") == 0 && i + 1 < argc)
            moves_arg = argv[++i];
        else if (strncmp(argv[i], "--work=", 7) == 0)
            snprintf(WORK, sizeof WORK, "%s", argv[i] + 7);
        else
            infile = argv[i];
    }

    /* With no file named, ask the batch's three questions. */
    char namebuf[1024], levelbuf[64], movesbuf[64];
    if (!infile) {
        ask("Enter the name of your games pgn-file (with or without .pgn):\n",
            namebuf, sizeof namebuf);
        infile = namebuf;
        if (!level_arg) {
            ask("Enter 0 (or just Return) for all sacrifices (1-5+ pawnunits) and "
                "statistics,\n1,2,3,4 or 5 to search that many pawnunits or more, "
                "or 9 for queen sacs only:\n", levelbuf, sizeof levelbuf);
            level_arg = levelbuf;
        }
        if (!moves_arg) {
            ask("Enter the maximum length of the non-draw games to investigate "
                "(40..250, Return = 80):\n", movesbuf, sizeof movesbuf);
            moves_arg = movesbuf;
        }
    }
    char gamebase[1100];
    size_t il = strlen(infile);
    if (il > 4 && strcmp(infile + il - 4, ".pgn") == 0)
        snprintf(gamebase, sizeof gamebase, "%s", infile);
    else
        snprintf(gamebase, sizeof gamebase, "%s.pgn", infile);

    /* The batch's clamps: level <0 -> 0, >5 -> 9 (queen); moves <40 -> 80,
     * >=250 -> 250 (so a missing answer means 80). */
    int level = level_arg ? atoi(level_arg) : 0;
    if (level < 0) level = 0;
    if (level > 5) level = 9;
    int movelimit = moves_arg ? atoi(moves_arg) : 80;
    if (movelimit < 40) movelimit = 80;
    if (movelimit >= 250) movelimit = 250;

    const char *data = pgnu_data_dir();
    snprintf(PATTERN_DIR, sizeof PATTERN_DIR, "%s/patterns", data);
    if (WORK[0] == '\0') snprintf(WORK, sizeof WORK, "build/sgs_work");
    pgnu_init(WORK);

    remove(SINGLEFILE);
    if (level != 0)
        for (int k = 0; k < CORPUS_SAC_LEVELS; k++) remove(SACFILE[k]);

    /* The won games between 15 and movelimit moves, comments kept. (The
     * batch wrote white and black wins to separate files; one pass keeps
     * both in file order, and the corpus splits them.) */
    const char *newsource = P("newsource.pgn");
    char bulimit[16];
    snprintf(bulimit, sizeof bulimit, "-bu%d", movelimit);
    xrun("--quiet", "--fixresulttags", "-bl15", bulimit, "-Tr1-0", "-Tr0-1",
         gamebase, "--output", (char *)newsource, NULL);
    /* Games are counted as parsed games throughout. (The batch counted lines
     * containing "[White " with `find /C`, which also counts any comment
     * quoting such a tag; on ordinary files the numbers are the same.) */
    long numb_origin = (long) pgnx_games_processed();
    if (level == 0 && numb_origin <= 0) {
        fprintf(stderr, "sgs: no games found in %s\n", gamebase);
        return 1;
    }

    CorpusGame *G;
    int NG;
    corpus_load(newsource, PATTERN_DIR, &G, &NG);
    int *W = (int *) malloc(((size_t)NG + 1) * sizeof(int));
    int *B = (int *) malloc(((size_t)NG + 1) * sizeof(int));
    if (!W || !B) { fprintf(stderr, "sgs: out of memory\n"); return 1; }
    int nW = 0, nB = 0;
    for (int i = 0; i < NG; i++) {
        if (G[i].result == 1) W[nW++] = i;
        else if (G[i].result == -1) B[nB++] = i;
    }
    long numb_wins = nW + nB;

    if (level == 0) {
        CorpusSacSets S;
        corpus_sac_classify(W, nW, B, nB, &S);
        if (write_statistics(gamebase, numb_origin, numb_wins, movelimit, S.n)) return 1;
        for (int k = 0; k < CORPUS_SAC_LEVELS; k++) {
            pgnu_truncate(SACFILE[k]);
            corpus_append(SACFILE[k], S.list[k], S.n[k]);
        }
        corpus_sac_free(&S);
        print_file(STATSFILE);
        printf("Games stored in 6 files: sacgames_(1-5)_pawns.pgn and "
               "sacgames_queensacs.pgn\n");
    } else {
        /* One level on its own (not the chain): white wins matching the
         * white pattern, then black wins matching the black one, deduped. */
        int bit = 1 << (level == 9 ? 5 : level - 1);
        int *found = (int *) malloc(((size_t)NG + 1) * sizeof(int));
        if (!found) { fprintf(stderr, "sgs: out of memory\n"); return 1; }
        int m = 0;
        for (int i = 0; i < nW; i++) if (G[W[i]].sac_mask & bit) found[m++] = W[i];
        for (int i = 0; i < nB; i++) if (G[B[i]].sac_mask & bit) found[m++] = B[i];
        int total = corpus_dedup(found, m, NULL, 0, found);
        if (total > 0) {
            pgnu_truncate(SINGLEFILE);
            corpus_append(SINGLEFILE, found, total);
            printf("Number of games with sacrifices found in the database: %d\n", total);
            printf("The games are sorted by white wins, followed by black wins, in "
                   SINGLEFILE "\n");
        } else {
            printf("No sacrifices with the chosen pawnunits found in the source file.\n");
        }
        free(found);
    }
    free(W);
    free(B);
    return 0;
}
