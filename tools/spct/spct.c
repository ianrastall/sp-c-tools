/*
 * spct - Stefan Pohl Chess Tools, unified front-end.
 *
 * One binary, one shared classification core (core/pgnx + core/pgnu), and a
 * report per subcommand. Each subcommand is a thin layer over the same
 * labelling machinery; see tools/<name>/<name>.c.
 */
#include <stdio.h>
#include <string.h>

/* Subcommand entry points (each treats argv[0] as the command name). */
int cmd_eas(int argc, char *argv[]);
int cmd_iws(int argc, char *argv[]);

static int usage(const char *prog)
{
    fprintf(stderr,
        "SPCT - Stefan Pohl Chess Tools (C port)\n"
        "usage: %s <command> [options] file.pgn\n\n"
        "commands:\n"
        "  eas   Engine Aggressiveness Statistics - rating lists, interesting_wins,\n"
        "        errorgames; --gauntlet, --hardavg N\n"
        "  iws   Interesting Wins Search - two-tier spectacular-game filter;\n"
        "        --moves N, --player NAME\n\n"
        "Run a command with no file to get its interactive prompt.\n",
        prog);
    return 2;
}

int main(int argc, char *argv[])
{
    if (argc < 2) return usage(argv[0]);
    const char *cmd = argv[1];
    if (strcmp(cmd, "eas") == 0) return cmd_eas(argc - 1, argv + 1);
    if (strcmp(cmd, "iws") == 0) return cmd_iws(argc - 1, argv + 1);
    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) { usage(argv[0]); return 0; }
    fprintf(stderr, "spct: unknown command '%s'\n\n", cmd);
    return usage(argv[0]);
}
