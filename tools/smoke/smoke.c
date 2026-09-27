/* SPCT core smoke test: (1) the in-process pass runner + reset, and
 * (2) the streaming corpus cross-checked against pgn-extract's own counts.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include "pgnx.h"
#include "corpus.h"

static unsigned long pass(char *argv[])
{
    pgnx_runv(argv);
    return pgnx_games_matched();
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s source.pgn\n", argv[0]);
        return 2;
    }
    const char *src = argv[1];

    /* --- pgn-extract-derived counts (the reference) --- */
    char *aw[] = {"pgn-extract", "--quiet", "-Tr1-0", (char *)src, "--output", "build/_sm_w.pgn", NULL};
    char *ab[] = {"pgn-extract", "--quiet", "-Tr0-1", (char *)src, "--output", "build/_sm_b.pgn", NULL};
    char *ad[] = {"pgn-extract", "--quiet", "-Tr1/2-1/2", (char *)src, "--output", "build/_sm_d.pgn", NULL};
    char *a20[] = {"pgn-extract", "--quiet", "-bu20", (char *)src, "--output", "build/_sm_20.pgn", NULL};
    unsigned long ref_w = pass(aw);
    unsigned long ref_b = pass(ab);
    unsigned long ref_d = pass(ad);
    unsigned long ref_le20 = pass(a20);

    /* re-run first filter: proves the reset reproduces it */
    unsigned long again = pass(aw);

    /* --- streaming corpus counts (the thing under test) --- */
    CorpusGame *g;
    int n;
    corpus_load(src, NULL, &g, &n);
    unsigned long cw = 0, cb = 0, cd = 0, cle20 = 0;
    for (int i = 0; i < n; i++) {
        if (g[i].result == 1) cw++;
        else if (g[i].result == -1) cb++;
        else if (g[i].result == 0) cd++;
        if (CORPUS_MOVES_LE(g[i].ply_offset + g[i].plies, 20)) cle20++;
    }

    printf("                 pgn-extract   corpus\n");
    printf("white wins (1-0) %11lu   %6lu   %s\n", ref_w, cw, ref_w == cw ? "OK" : "MISMATCH");
    printf("black wins (0-1) %11lu   %6lu   %s\n", ref_b, cb, ref_b == cb ? "OK" : "MISMATCH");
    printf("draws            %11lu   %6lu   %s\n", ref_d, cd, ref_d == cd ? "OK" : "MISMATCH");
    printf("<= 20 moves      %11lu   %6lu   %s\n", ref_le20, cle20, ref_le20 == cle20 ? "OK" : "MISMATCH");
    printf("reset reproduces pass 1: %s\n", ref_w == again ? "YES" : "NO");

    int ok = (ref_w == cw) && (ref_b == cb) && (ref_d == cd) &&
             (ref_le20 == cle20) && (ref_w == again);
    return ok ? 0 : 1;
}
