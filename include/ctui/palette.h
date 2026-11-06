/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * palette.h - command palette overlay (Ctrl-P in editors).
 *
 * A centred dialog with an input line over a filtered, ranked list of
 * commands. The commands stay with the caller; the dialog owns the query,
 * the cursor and a result buffer the caller provides, so typing never
 * allocates and thousands of commands are fine.
 *
 * Fuzzy matching (ctui_palette_score): every character of the query must
 * appear in the text, in order, compared by UTF-8 character with ASCII
 * case folding only (A-Z fold to a-z; other characters must be equal).
 * Spaces in the query are ignored. Among the possible alignments the best
 * one wins; a match scores
 *   +10 per matched character,
 *   + 8 when it starts a word (first character, or after a character that
 *       is not a letter, digit or non-ASCII, or a lower to upper case
 *       change such as "oF" in "openFile"),
 *   +15 when it directly follows the previous matched character,
 *   + 1 when its case is exactly the query's,
 *   + 4 when it is the first character of the text,
 *   - 1 per skipped character between two matches,
 *   - 1 per skipped leading character (at most 3).
 * Only the first CTUI_PALETTE_QUERY_MAX query characters and the first
 * CTUI_PALETTE_TEXT_MAX text characters count. The score of a match is never
 * negative; an empty query matches everything with score 0.
 *
 * ctui_palette_filter() ranks the commands by score, best first; equal
 * scores keep the order of the array (the sort is stable). The dialog
 * (ctui_cmdpal) is a plain state struct like the other dialogs: feed keys
 * and clicks, redraw each turn. The caller shows the cursor (curs_set(1));
 * ctui_cmdpal_draw() places it on the input line.
 *
 *   keys:  typing, Backspace, Ctrl-U (clear), Up / Down (Ctrl-P / Ctrl-N),
 *          PgUp / PgDn, Enter chooses, Esc and Ctrl-C cancel.
 *   mouse: a click on an entry chooses it, the close button cancels, the
 *          wheel scrolls the list.
 */
#ifndef CTUI_PALETTE_H
#define CTUI_PALETTE_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dialog.h>
#include <ctui/labels.h>
#include <ctui/theme.h>

#define CTUI_PALETTE_QUERY_MAX 32 /* query characters that count */
#define CTUI_PALETTE_TEXT_MAX 128 /* text characters that count */
#define CTUI_PALETTE_NO_MATCH (-1)

typedef struct ctui_command {
	const char *name;	 /* what is matched and shown */
	const char *shortcut;	 /* shown at the right, may be NULL */
	const char *description; /* shown after the name, may be NULL */
} ctui_command;

/* Score of query against text, or CTUI_PALETTE_NO_MATCH. When positions is
 * not NULL it receives the byte offset in text of each matched character
 * (up to max_positions of them, in order). */
int ctui_palette_score(const char *query, const char *text,
		       unsigned short *positions, size_t max_positions);

typedef struct ctui_palette_hit {
	size_t index; /* into the command array */
	int score;
	unsigned short count; /* matched characters of the name */
	unsigned short
	    pos[CTUI_PALETTE_QUERY_MAX]; /* their byte offsets in the name */
} ctui_palette_hit;

/* Rank the commands whose name matches query into out (capacity cap):
 * best score first, ties in array order. Returns the number of hits; when
 * more than cap commands match, the first cap in array order are ranked.
 * A NULL query is empty. Allocates nothing. */
size_t ctui_palette_filter(const ctui_command *commands, size_t count,
			   const char *query, ctui_palette_hit *out,
			   size_t cap);

#define CTUI_PALETTE_INPUT_MAX 96 /* bytes of the query, incl. the NUL */

typedef struct ctui_cmdpal {
	const ctui_command *commands;
	size_t count;
	ctui_palette_hit *hits; /* caller buffer for the ranked matches */
	size_t hit_cap; /* its size; at least `count` to rank them all */
	char query[CTUI_PALETTE_INPUT_MAX];
	size_t used;	 /* strlen(query) */
	size_t matches;	 /* hits in use */
	size_t selected; /* into hits, meaningful when matches > 0 */
	size_t first;	 /* first visible hit */
	const ctui_labels
	    *labels; /* override of the global labels, may be NULL */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_cmdpal;

/* Open with an empty query and every command listed. */
void ctui_cmdpal_init(ctui_cmdpal *p, const ctui_command *commands,
		      size_t count, ctui_palette_hit *hits, size_t hit_cap);

/* Replace the query and refilter (the cursor goes to the best match). */
void ctui_cmdpal_set_query(ctui_cmdpal *p, const char *query);

/* Apply a key on a lines x cols screen. ACCEPTED means a command was
 * chosen (see ctui_cmdpal_chosen); Enter with no match does nothing. */
ctui_dialog_result ctui_cmdpal_key(ctui_cmdpal *p, int key, int lines,
				   int cols);

/* Index in the command array of the highlighted match, or (size_t)-1. */
size_t ctui_cmdpal_chosen(const ctui_cmdpal *p);

/* The box and the number of entry rows; false when the screen is too small. */
bool ctui_cmdpal_geometry(const ctui_cmdpal *p, int lines, int cols,
			  ctui_rect *box, int *rows);

/* A left click at screen cell (y, x): on an entry it highlights and chooses
 * it (ACCEPTED), on the close button it cancels, elsewhere nothing. */
ctui_dialog_result ctui_cmdpal_click(ctui_cmdpal *p, int lines, int cols, int y,
				     int x);

/* Wheel: dir > 0 scrolls down, < 0 up, three entries per notch. */
void ctui_cmdpal_wheel(ctui_cmdpal *p, int lines, int cols, int dir);

void ctui_cmdpal_draw(const ctui_cmdpal *p);

#endif /* CTUI_PALETTE_H */
