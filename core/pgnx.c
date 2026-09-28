/*
 * SPCT core: in-process pgn-extract pass runner and state reset.
 * See core/pgnx.h and vendor/pgn-extract/PATCHES.md.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include "bool.h"
#include "mymalloc.h"
#include "defs.h"
#include "typedef.h"
#include "tokens.h"
#include "taglist.h"
#include "lex.h"
#include "moves.h"
#include "map.h"
#include "lists.h"
#include "output.h"
#include "end.h"
#include "grammar.h"
#include "hashing.h"
#include "pgnx.h"

/* Conservatively slightly smaller than the PGN export standard of 80,
 * matching pgn-extract's own MAX_LINE_LENGTH. */
#define PGNX_MAX_LINE_LENGTH 75

/* Defined (canonically) in the vendored main.c. */
extern int pgnx_run(int argc, char *argv[]);

void
pgnx_prepare_pass(void)
{
    static Boolean tables_ready = FALSE;
    static Boolean have_pristine = FALSE;
    static StateInfo pristine;

    /* Restore GlobalState to its start-of-program defaults. On the very
     * first call GlobalState still holds the file-scope initializer from
     * main.c, so capture that as the pristine template. */
    if (!have_pristine) {
        pristine = GlobalState;
        have_pristine = TRUE;
    }
    else {
        GlobalState = pristine;
    }
    GlobalState.outputfile = stdout;
    GlobalState.logfile = stderr;
    set_output_line_length(PGNX_MAX_LINE_LENGTH);

    /* One-time lexical/hash table setup (safe to do once per process). */
    if (!tables_ready) {
        init_hashtab();
        init_lex_tables();
        tables_ready = TRUE;
    }

    /* Per-pass state that would otherwise leak across passes. The game
     * header starts at the original tag count, but the lexer's tag table
     * persists and keeps (with the same indices) any non-standard tags an
     * earlier pass met; the lexer only grows the header for tags it has not
     * seen, so grow it here to cover them all. */
    init_game_header();
    if (spct_known_tag_count() > ORIGINAL_NUMBER_OF_TAGS)
        increase_game_header_tags_length(spct_known_tag_count());
    init_tag_lists();
    reset_line_number();
    reset_input_source_list();
    reset_endings_to_match();
    reset_games_to_keep();
}

void
pgnx_finish_pass(void)
{
    FILE *f = GlobalState.outputfile;
    if (f != NULL && f != stdout && f != stderr) {
        (void) fclose(f);
    }
    GlobalState.outputfile = stdout;

    if (GlobalState.non_matching_file != NULL &&
        GlobalState.non_matching_file != stdout &&
        GlobalState.non_matching_file != stderr) {
        (void) fclose(GlobalState.non_matching_file);
        GlobalState.non_matching_file = NULL;
    }
    if (GlobalState.duplicate_file != NULL &&
        GlobalState.duplicate_file != stdout &&
        GlobalState.duplicate_file != stderr) {
        (void) fclose(GlobalState.duplicate_file);
        GlobalState.duplicate_file = NULL;
    }
}

unsigned long
pgnx_games_matched(void)
{
    return GlobalState.num_games_matched;
}

unsigned long
pgnx_games_processed(void)
{
    return GlobalState.num_games_processed;
}

int
pgnx_runv(char *const argv[])
{
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    /* pgnx_run does not modify argv contents. */
    return pgnx_run(argc, (char **) argv);
}
