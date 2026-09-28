/*
 * spct - Stefan Pohl Chess Tools, unified front-end.
 *
 * One binary, one shared classification core (core/corpus over core/pgnx),
 * and a report per subcommand. Each subcommand is a thin layer over the same
 * labelling machinery; see tools/<name>/<name>.c.
 *
 * The tools' idea, design, algorithms and scoring
 * (C) 2024-2025, Stefan Pohl (SPCC), www.sp-cc.de.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <string.h>

/* Subcommand entry points (each treats argv[0] as the command name). */
int cmd_eas(int argc, char *argv[]);
int cmd_iws(int argc, char *argv[]);
int cmd_sgs(int argc, char *argv[]);
int cmd_sga(int argc, char *argv[]);

/* Set from the VERSION file by build.sh / CMake. */
#ifndef SPCT_VERSION
#define SPCT_VERSION "unknown"
#endif

static int usage(const char *prog)
{
    fprintf(stderr,
        "SPCT - Stefan Pohl Chess Tools (C port) " SPCT_VERSION "\n"
        "Tools (C) 2024-2025, Stefan Pohl, www.sp-cc.de; C port under GPLv3+\n\n"
        "usage: %s <command> [options] file.pgn\n\n"
        "commands:\n"
        "  eas   Engine Aggressiveness Statistics - rating lists, interesting_wins,\n"
        "        errorgames; --gauntlet, --hardavg N\n"
        "  iws   Interesting Wins Search - two-tier spectacular-game filter;\n"
        "        --moves N, --player NAME\n"
        "  sgs   Sacrifice Games Search - sacrifice games by depth + statistics;\n"
        "        --level 0|1-5|9 (0 = full search), --moves N;\n"
        "        --comfort: the SGS comfort tool (marks the sac move)\n"
        "  sga   Short Games Analyzer - short wins/draws/losses and short sacs\n"
        "        per engine, four rating lists; --no-endgame\n\n"
        "Run a command with no file to get its interactive prompt.\n"
        "spct --version prints the version.\n",
        prog);
    return 2;
}

int main(int argc, char *argv[])
{
    if (argc < 2) return usage(argv[0]);
    const char *cmd = argv[1];
    if (strcmp(cmd, "eas") == 0) return cmd_eas(argc - 1, argv + 1);
    if (strcmp(cmd, "iws") == 0) return cmd_iws(argc - 1, argv + 1);
    if (strcmp(cmd, "sgs") == 0) return cmd_sgs(argc - 1, argv + 1);
    if (strcmp(cmd, "sga") == 0) return cmd_sga(argc - 1, argv + 1);
    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) { usage(argv[0]); return 0; }
    if (strcmp(cmd, "--version") == 0) { printf("spct %s\n", SPCT_VERSION); return 0; }
    fprintf(stderr, "spct: unknown command '%s'\n\n", cmd);
    return usage(argv[0]);
}
