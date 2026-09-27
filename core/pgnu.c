/*
 * SPCT shared utilities. See core/pgnu.h.
 * Bodies are lifted verbatim from the validated eas.c/iws.c so behaviour is
 * identical; the sac chain is the search both tools ran inline.
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

static char WORK[1024];
static char PATDIR[1024];

static void die(const char *msg) { fprintf(stderr, "spct: %s\n", msg); exit(1); }

void pgnu_init(const char *work_dir, const char *pattern_dir)
{
    snprintf(WORK, sizeof WORK, "%s", work_dir);
    snprintf(PATDIR, sizeof PATDIR, "%s", pattern_dir);
#ifdef _WIN32
    _mkdir("build");
    _mkdir(WORK);
#else
    mkdir("build", 0777);
    mkdir(WORK, 0777);
#endif
}

const char *pgnu_wp(const char *name)
{
    static char store[96][1300];
    static char names[96][64];
    static int count = 0;
    for (int i = 0; i < count; i++)
        if (strcmp(names[i], name) == 0) return store[i];
    if (count >= 96) die("too many intermediate file names");
    snprintf(store[count], 1300, "%s/%s", WORK, name);
    snprintf(names[count], 64, "%s", name);
    return store[count++];
}

const char *pgnu_pat(const char *name)
{
    static char buf[4][1300];
    static int i = 0;
    i = (i + 1) & 3;
    snprintf(buf[i], sizeof buf[i], "%s/%s", PATDIR, name);
    return buf[i];
}

char *pgnu_flag(const char *prefix, const char *value)
{
    static char buf[8][1400];
    static int i = 0;
    i = (i + 1) & 7;
    snprintf(buf[i], sizeof buf[i], "%s%s", prefix, value);
    return buf[i];
}

char *pgnu_bu(int n)
{
    static char buf[4][16];
    static int i = 0;
    i = (i + 1) & 3;
    snprintf(buf[i], sizeof buf[i], "-bu%d", n);
    return buf[i];
}

unsigned long pgnu_run(char *a0, ...)
{
    char *argv[28];
    int n = 0;
    argv[n++] = "pgn-extract";
    argv[n++] = a0;
    va_list ap;
    va_start(ap, a0);
    char *s;
    while ((s = va_arg(ap, char *)) != NULL && n < 27) argv[n++] = s;
    va_end(ap);
    argv[n] = NULL;
    pgnx_runv(argv);
    return pgnx_games_matched();
}

long pgnu_count(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    long n = 0;
    char line[8192];
    while (fgets(line, sizeof line, f))
        if (strncmp(line, "[White ", 7) == 0) n++;
    fclose(f);
    return n;
}

void pgnu_copy(const char *dst, const char *src)
{
    FILE *in = fopen(src, "rb");
    FILE *out = fopen(dst, "wb");
    if (!out) die("cannot create file");
    if (in) {
        char buf[65536];
        size_t k;
        while ((k = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, k, out);
        fclose(in);
    }
    fclose(out);
}

void pgnu_concat2(const char *dst, const char *a, const char *b)
{
    FILE *o = fopen(dst, "wb");
    if (!o) die("cannot create concat output");
    char buf[65536];
    size_t k;
    for (int i = 0; i < 2; i++) {
        FILE *in = fopen(i == 0 ? a : b, "rb");
        if (!in) continue;
        while ((k = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, k, o);
        fclose(in);
    }
    fclose(o);
}

void pgnu_truncate(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f) fclose(f);
}

void pgnu_sortlength(const char *src, const char *dst,
                     const int *lo, const int *hi, int n)
{
    pgnu_truncate(dst);
    for (int i = 0; i < n; i++) {
        char buA[16], buB[16];
        char *av[12];
        int k = 0;
        av[k++] = "pgn-extract";
        av[k++] = "--quiet";
        if (hi[i] > 0) { snprintf(buA, sizeof buA, "-bu%d", hi[i]); av[k++] = buA; }
        if (lo[i] > 0) { snprintf(buB, sizeof buB, "-bl%d", lo[i]); av[k++] = buB; }
        av[k++] = (char *)src;
        av[k++] = pgnu_flag("-a", dst);
        av[k] = NULL;
        pgnx_runv(av);
    }
}

void pgnu_read_anno(const char *anno_dir, const char *name, char *out, size_t n)
{
    char path[1300];
    snprintf(path, sizeof path, "%s/%s", anno_dir, name);
    out[0] = '\0';
    FILE *f = fopen(path, "rb");
    if (f) { if (fgets(out, (int)n, f)) out[strcspn(out, "\r\n")] = '\0'; fclose(f); }
}

void pgnu_annotate_append(const char *src, const char *annoline, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) return;
    FILE *out = fopen(dst, "ab");
    if (!out) { fclose(in); return; }
    char line[8192];
    int in_tags = 0;
    while (fgets(line, sizeof line, in)) {
        if (line[0] == '[') { in_tags = 1; fputs(line, out); }
        else {
            if (in_tags && annoline[0]) { fputs(annoline, out); fputc('\n', out); }
            in_tags = 0;
            fputs(line, out);
        }
    }
    fclose(in);
    fclose(out);
}

/* ---- shared sacrifice classifier ---- */

void pgnu_sac_1plus(const char *whitewins, const char *blackwins, const char *r1)
{
    const char *blacksacs = pgnu_wp("blacksacs.pgn");
    pgnu_run("--quiet", pgnu_flag("-y", pgnu_pat("1_pawnsac_white")),
             (char *)whitewins, "--output", (char *)r1, NULL);
    pgnu_copy(whitewins, r1);
    pgnu_run("--quiet", pgnu_flag("-y", pgnu_pat("1_pawnsac_black")),
             (char *)blackwins, "--output", (char *)blacksacs, NULL);
    pgnu_run("--quiet", pgnu_flag("-a", r1), (char *)blacksacs, NULL);
    pgnu_copy(blackwins, blacksacs);
}

void pgnu_sac_rest(const char *whitewins, const char *blackwins,
                   const char *r1, const char *const unique[6], int counts[6])
{
    const char *blacksacs = pgnu_wp("blacksacs.pgn");
    const char *tmp2 = pgnu_wp("sac_tmp2.pgn");
    const char *r2 = pgnu_wp("results_opt2.pgn");
    const char *r3 = pgnu_wp("results_opt3.pgn");
    const char *r4 = pgnu_wp("results_opt4.pgn");
    const char *r5 = pgnu_wp("results_opt5.pgn");
    const char *r9 = pgnu_wp("results_opt9.pgn");
    struct { const char *pw, *pb, *out; } lv[5] = {
        { "2_pawnsac_white", "2_pawnsac_black", r2 },
        { "3_pawnsac_white", "3_pawnsac_black", r3 },
        { "4_pawnsac_white", "4_pawnsac_black", r4 },
        { "5_pawnsac_white", "5_pawnsac_black", r5 },
        { "queensac_white",  "queensac_black",  r9 },
    };
    for (int i = 0; i < 5; i++) {
        pgnu_run("--quiet", pgnu_flag("-y", pgnu_pat(lv[i].pw)), (char *)whitewins,
                 "--output", (char *)lv[i].out, NULL);
        pgnu_run("--quiet", pgnu_flag("-y", pgnu_pat(lv[i].pb)), (char *)blackwins,
                 "--output", (char *)blacksacs, NULL);
        pgnu_run("--quiet", pgnu_flag("-a", lv[i].out), (char *)blacksacs, NULL);
        if (i < 4) {   /* narrow the win files for the next (deeper) level */
            pgnu_run("--quiet", pgnu_flag("-y", pgnu_pat(lv[i].pw)), (char *)whitewins,
                     "--output", (char *)tmp2, NULL);
            pgnu_copy(whitewins, tmp2);
            pgnu_run("--quiet", pgnu_flag("-y", pgnu_pat(lv[i].pb)), (char *)blackwins,
                     "--output", (char *)tmp2, NULL);
            pgnu_copy(blackwins, tmp2);
        }
    }
    /* De-duplicate so each game counts only in its highest category. */
    pgnu_run("--quiet", "-D", (char *)r9, "--output", (char *)unique[5], NULL);
    pgnu_run("--quiet", pgnu_flag("-c", r9), "-D", pgnu_flag("-o", unique[4]), (char *)r5, NULL);
    pgnu_run("--quiet", pgnu_flag("-c", r5), "-D", pgnu_flag("-o", unique[3]), (char *)r4, NULL);
    pgnu_run("--quiet", pgnu_flag("-c", r4), "-D", pgnu_flag("-o", unique[2]), (char *)r3, NULL);
    pgnu_run("--quiet", pgnu_flag("-c", r3), "-D", pgnu_flag("-o", unique[1]), (char *)r2, NULL);
    pgnu_run("--quiet", pgnu_flag("-c", r2), "-D", pgnu_flag("-o", unique[0]), (char *)r1, NULL);
    for (int i = 0; i < 6; i++) counts[i] = (int)pgnu_count(unique[i]);
}
