/*
 * SPCT shared utilities. See core/pgnu.h.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "pgnx.h"
#include "pgnu.h"

static char WORK[1024];

static void die(const char *msg) { fprintf(stderr, "spct: %s\n", msg); exit(1); }

static int is_data_dir(const char *dir)
{
    char probe[1300];
    struct stat st;
    snprintf(probe, sizeof probe, "%s/patterns/1_pawnsac_white", dir);
    return stat(probe, &st) == 0;
}

/* Directory holding the running executable, or "" if unknown. */
static void exe_dir(char *out, size_t n)
{
    out[0] = '\0';
#ifdef _WIN32
    DWORD k = GetModuleFileNameA(NULL, out, (DWORD)n);
    if (k == 0 || k >= n) { out[0] = '\0'; return; }
#else
    ssize_t k = readlink("/proc/self/exe", out, n - 1);
    if (k <= 0) { out[0] = '\0'; return; }
    out[k] = '\0';
#endif
    char *slash = strrchr(out, '/');
    char *bslash = strrchr(out, '\\');
    if (bslash > slash) slash = bslash;
    if (slash) *slash = '\0'; else out[0] = '\0';
}

const char *pgnu_data_dir(void)
{
    static char dir[1300];
    if (dir[0]) return dir;

    const char *env = getenv("SPCT_DATA");
    if (env && env[0]) {
        snprintf(dir, sizeof dir, "%s", env);
        if (!is_data_dir(dir)) {
            fprintf(stderr, "spct: SPCT_DATA=%s has no patterns/ files\n", env);
            exit(1);
        }
        return dir;
    }
    snprintf(dir, sizeof dir, "data");
    if (is_data_dir(dir)) return dir;

    char ed[1024];
    exe_dir(ed, sizeof ed);
    /* build/spct.exe, or a CMake multi-config build/Release/spct.exe. */
    static const char *up[3] = { "data", "../data", "../../data" };
    for (int i = 0; ed[0] && i < 3; i++) {
        snprintf(dir, sizeof dir, "%s/%s", ed, up[i]);
        if (is_data_dir(dir)) return dir;
    }
#ifdef SPCT_DEFAULT_DATA
    /* Source-tree data/, baked in by CMake for out-of-tree build dirs. */
    snprintf(dir, sizeof dir, "%s", SPCT_DEFAULT_DATA);
    if (is_data_dir(dir)) return dir;
#endif
    dir[0] = '\0';
    die("cannot find the data/ directory (patterns/, anno/); "
        "run from the SPCT root or set SPCT_DATA");
    return NULL;
}

void pgnu_init(const char *work_dir)
{
    snprintf(WORK, sizeof WORK, "%s", work_dir);
    /* Create every missing component of the work dir (e.g. build/eas_work). */
    char part[1024];
    snprintf(part, sizeof part, "%s", WORK);
    for (char *p = part + 1; ; p++) {
        char c = *p;
        if (c == '/' || c == '\\' || c == '\0') {
            *p = '\0';
#ifdef _WIN32
            _mkdir(part);
#else
            mkdir(part, 0777);
#endif
            *p = c;
            if (c == '\0') break;
        }
    }
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

char *pgnu_flag(const char *prefix, const char *value)
{
    static char buf[8][1400];
    static int i = 0;
    i = (i + 1) & 7;
    snprintf(buf[i], sizeof buf[i], "%s%s", prefix, value);
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

PgnuPct pgnu_pct(long base, long count)
{
    PgnuPct r;
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

int pgnu_moveaverage(long long total_plies, long long count)
{
    if (count <= 0) return 0;
    /* summary.exe: the mean rounded half up to 2 decimals... */
    long long r = (200 * total_plies + count) / (2 * count);
    long long whole = r / 100, dec = r % 100;
    /* ...printed without trailing zeros (93.74, 93.5, 94.0); the batch
     * reads the digits after the point and rounds up if they are >= 50,
     * which a single digit never is. */
    if (dec % 10 != 0 && dec >= 50) whole++;
    return (int)(whole / 2);
}

/* Portable stand-in: the order sort.exe gives the printable ASCII
 * characters (measured), letters case-folded; other bytes after them. */
static int sort_weight(unsigned char c)
{
    static const char order[] =
        "'- !\"#$%&()*,./:;?@[\\]^_`{|}~+<=>0123456789abcdefghijklmnopqrstuvwxyz";
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
    const char *p = c ? strchr(order, c) : NULL;
    return p ? (int)(p - order) : 256 + c;
}

int pgnu_sort_compare(const char *a, const char *b)
{
#ifdef _WIN32
    /* sort.exe reads lines in the console (OEM) code page and compares them
     * with the user locale's string sort, ignoring case. */
    int la = MultiByteToWideChar(CP_OEMCP, 0, a, -1, NULL, 0);
    int lb = MultiByteToWideChar(CP_OEMCP, 0, b, -1, NULL, 0);
    WCHAR *wa = (WCHAR *) malloc((size_t)la * sizeof(WCHAR));
    WCHAR *wb = (WCHAR *) malloc((size_t)lb * sizeof(WCHAR));
    if (wa != NULL && wb != NULL) {
        MultiByteToWideChar(CP_OEMCP, 0, a, -1, wa, la);
        MultiByteToWideChar(CP_OEMCP, 0, b, -1, wb, lb);
        int r = CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE | SORT_STRINGSORT,
                               wa, la - 1, wb, lb - 1);
        free(wa);
        free(wb);
        if (r != 0) return r - 2;   /* CSTR_LESS_THAN = 1 ... CSTR_GREATER_THAN = 3 */
    } else {
        free(wa);
        free(wb);
    }
#endif
    for (;; a++, b++) {
        if (*a == '\0' || *b == '\0')
            return (*a != '\0') - (*b != '\0');
        int wa2 = sort_weight((unsigned char)*a), wb2 = sort_weight((unsigned char)*b);
        if (wa2 != wb2) return wa2 < wb2 ? -1 : 1;
    }
}
