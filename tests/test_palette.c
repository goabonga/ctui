/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_palette.c - fuzzy scoring, ranking and the command palette dialog.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctui/palette.h>

static void test_score_basics(void)
{
	CHECK(ctui_palette_score("", "anything", NULL, 0) == 0);
	CHECK(ctui_palette_score(NULL, "anything", NULL, 0) == 0);
	CHECK(ctui_palette_score("   ", "", NULL, 0) == 0);
	/* first char: 10 + 1 case + 4 first + 8 word; then 2 x (10 + 1 + 15) */
	CHECK(ctui_palette_score("abc", "abc", NULL, 0) == 75);
	CHECK(ctui_palette_score("ABC", "abc", NULL, 0) ==
	      72); /* no exact case bonus */
	CHECK(ctui_palette_score("abc", "ABC", NULL, 0) ==
	      72); /* case folds both ways */
	CHECK(ctui_palette_score("abd", "abc", NULL, 0) ==
	      CTUI_PALETTE_NO_MATCH);
	CHECK(ctui_palette_score("cba", "abc", NULL, 0) ==
	      CTUI_PALETTE_NO_MATCH); /* order matters */
	CHECK(ctui_palette_score("abcd", "abc", NULL, 0) ==
	      CTUI_PALETTE_NO_MATCH);
	CHECK(ctui_palette_score("a", "", NULL, 0) == CTUI_PALETTE_NO_MATCH);
	CHECK(ctui_palette_score("a", NULL, NULL, 0) == CTUI_PALETTE_NO_MATCH);
	/* spaces in the query are ignored */
	CHECK(ctui_palette_score("o f", "OpenFile", NULL, 0) ==
	      ctui_palette_score("of", "OpenFile", NULL, 0));
}

static void test_score_bonuses(void)
{
	/* a camel case boundary is a word start: 10 + 8 - 3 leading, no exact
	 * case */
	CHECK(ctui_palette_score("f", "openFile", NULL, 0) == 15);
	CHECK(ctui_palette_score("f", "opensfile", NULL, 0) == 8);
	/* after a separator */
	CHECK(ctui_palette_score("b", "foo-bar", NULL, 0) == 10 + 1 + 8 - 3);
	CHECK(ctui_palette_score("b", "foobar", NULL, 0) == 10 + 1 - 3);
	/* consecutive beats scattered */
	CHECK(ctui_palette_score("ab", "xxab", NULL, 0) >
	      ctui_palette_score("ab", "xaxb", NULL, 0));
	/* the start of the text beats the middle */
	CHECK(ctui_palette_score("s", "save", NULL, 0) >
	      ctui_palette_score("s", "close", NULL, 0));
	/* a longer gap costs more */
	CHECK(ctui_palette_score("ab", "a-b", NULL, 0) >
	      ctui_palette_score("ab", "a----b", NULL, 0));
	/* word starts beat a tight run in the middle of a word */
	CHECK(ctui_palette_score("of", "open file", NULL, 0) >
	      ctui_palette_score("of", "software", NULL, 0));
	/* the best alignment wins, not the first: "ab" in "a_ab" is the run at
	 * 2 */
	unsigned short pos[2];
	CHECK(ctui_palette_score("ab", "a_ab", pos, 2) > 0 && pos[0] == 2 &&
	      pos[1] == 3);
}

static void test_score_positions(void)
{
	unsigned short pos[8];
	CHECK(ctui_palette_score("fb", "foo bar", pos, 8) > 0);
	CHECK(pos[0] == 0 && pos[1] == 4);
	/* only max_positions are stored, and the call is fine without room */
	unsigned short two[2] = {99, 99};
	CHECK(ctui_palette_score("abc", "abc", two, 2) == 75 && two[0] == 0 &&
	      two[1] == 1);
	CHECK(ctui_palette_score("abc", "abc", NULL, 0) == 75);
}

static void test_score_utf8(void)
{
	unsigned short pos[4];
	CHECK(ctui_palette_score("\xc3\xa9", "caf\xc3\xa9", pos, 4) > 0 &&
	      pos[0] == 3); /* byte offset */
	CHECK(ctui_palette_score("caf\xc3\xa9", "CAF\xc3\xa9", NULL, 0) > 0);
	/* only ASCII folds: E acute does not match e acute */
	CHECK(ctui_palette_score("\xc3\x89", "caf\xc3\xa9", NULL, 0) ==
	      CTUI_PALETTE_NO_MATCH);
	CHECK(ctui_palette_score("e", "caf\xc3\xa9", NULL, 0) ==
	      CTUI_PALETTE_NO_MATCH);
	CHECK(ctui_palette_score("\xe6\x97\xa5\xe6\x9c\xac",
				 "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", pos,
				 4) > 0);
	CHECK(pos[0] == 0 && pos[1] == 3);
	/* a character is never split: a lone lead byte matches nothing in valid
	 * text */
	CHECK(ctui_palette_score("\xc3", "caf\xc3\xa9", NULL, 0) ==
	      CTUI_PALETTE_NO_MATCH);
	/* invalid bytes do not crash */
	CHECK(ctui_palette_score("a",
				 "\xff\xfe"
				 "a",
				 NULL, 0) > 0);
}

static void test_score_limits(void)
{
	char longq[200], longt[400];
	memset(longq, 'a', sizeof longq - 1);
	longq[sizeof longq - 1] = '\0';
	memset(longt, 'a', sizeof longt - 1);
	longt[sizeof longt - 1] = '\0';
	CHECK(ctui_palette_score(longq, longt, NULL, 0) >
	      0); /* extra characters are ignored */
	longt[10] = '\0';
	CHECK(ctui_palette_score(longq, longt, NULL, 0) ==
	      CTUI_PALETTE_NO_MATCH); /* 32 needed, 10 there */
	/* a match past the text limit is not seen */
	memset(longt, 'x', sizeof longt - 1);
	longt[300] = 'z';
	longt[399] = '\0';
	CHECK(ctui_palette_score("z", longt, NULL, 0) == CTUI_PALETTE_NO_MATCH);
}

static const ctui_command SAMPLE[] = {
    {"Save File", "Ctrl-S", "Write the buffer"},
    {"Save As", NULL, NULL},
    {"Open File", "Ctrl-O", NULL},
    {"Close", NULL, NULL},
    {"Say Hello", NULL, NULL},
};
#define SAMPLE_N (sizeof SAMPLE / sizeof SAMPLE[0])

static void test_filter_order(void)
{
	ctui_palette_hit hits[SAMPLE_N];
	/* empty query: everything, in array order, score 0 */
	size_t n = ctui_palette_filter(SAMPLE, SAMPLE_N, "", hits, SAMPLE_N);
	CHECK(n == SAMPLE_N);
	for (size_t i = 0; i < n; i++)
		CHECK(hits[i].index == i && hits[i].score == 0 &&
		      hits[i].count == 0);
	CHECK(ctui_palette_filter(SAMPLE, SAMPLE_N, NULL, hits, SAMPLE_N) ==
	      SAMPLE_N);

	/* "sa": Save File, Save As and Say Hello tie on "Sa": array order */
	n = ctui_palette_filter(SAMPLE, SAMPLE_N, "sa", hits, SAMPLE_N);
	CHECK(n == 3 && hits[0].index == 0 && hits[1].index == 1 &&
	      hits[2].index == 4);
	CHECK(hits[0].score == hits[1].score && hits[1].score == hits[2].score);
	CHECK(hits[0].count == 2 && hits[0].pos[0] == 0 && hits[0].pos[1] == 1);

	/* "sf": word starts, best first */
	n = ctui_palette_filter(SAMPLE, SAMPLE_N, "sf", hits, SAMPLE_N);
	CHECK(n == 1 && hits[0].index == 0 && hits[0].pos[1] == 5);
	n = ctui_palette_filter(SAMPLE, SAMPLE_N, "fi", hits, SAMPLE_N);
	CHECK(n == 2 && hits[0].index == 0 &&
	      hits[1].index == 2); /* tie again */

	/* "se": Close has an s then e, Save File matches earlier: ranked above
	 */
	n = ctui_palette_filter(SAMPLE, SAMPLE_N, "se", hits, SAMPLE_N);
	CHECK(n >= 2);
	for (size_t i = 1; i < n; i++)
		CHECK(hits[i - 1].score >= hits[i].score);

	CHECK(ctui_palette_filter(SAMPLE, SAMPLE_N, "zzz", hits, SAMPLE_N) ==
	      0);
	CHECK(ctui_palette_filter(SAMPLE, SAMPLE_N, "", hits, 2) ==
	      2); /* cap */
	CHECK(ctui_palette_filter(SAMPLE, SAMPLE_N, "", hits, 0) == 0);
	CHECK(ctui_palette_filter(SAMPLE, 0, "a", hits, SAMPLE_N) == 0);

	/* a command without a name only matches the empty query */
	ctui_command nameless[] = {{NULL, NULL, NULL}};
	CHECK(ctui_palette_filter(nameless, 1, "a", hits, 1) == 0);
	CHECK(ctui_palette_filter(nameless, 1, "", hits, 1) == 1);
}

static void test_filter_large(void)
{
	enum { N = 5000 };
	ctui_command *cmds = calloc(N, sizeof *cmds);
	char (*names)[24] = calloc(N, sizeof *names);
	ctui_palette_hit *hits = calloc(N, sizeof *hits);
	CHECK(cmds && names && hits);
	for (size_t i = 0; i < N; i++) {
		snprintf(names[i], sizeof names[i], "Command %04zu", i);
		cmds[i].name = names[i];
	}
	CHECK(ctui_palette_filter(cmds, N, "", hits, N) == N);
	size_t n = ctui_palette_filter(cmds, N, "c12", hits, N);
	CHECK(n > 0 && n < N);
	bool sorted = true, stable = true;
	for (size_t i = 1; i < n; i++) {
		sorted = sorted && hits[i - 1].score >= hits[i].score;
		stable = stable && (hits[i - 1].score > hits[i].score ||
				    hits[i - 1].index < hits[i].index);
	}
	CHECK(sorted && stable);
	/* "c1234" is the exact tail: command 1234 comes first */
	n = ctui_palette_filter(cmds, N, "1234", hits, N);
	CHECK(n >= 1 && hits[0].index == 1234);
	/* identical names keep the array order */
	for (size_t i = 0; i < N; i++)
		cmds[i].name = "same";
	n = ctui_palette_filter(cmds, N, "sm", hits, N);
	CHECK(n == N);
	bool order = true;
	for (size_t i = 0; i < n; i++)
		order = order && hits[i].index == i;
	CHECK(order);
	free(cmds);
	free(names);
	free(hits);
}

static void type(ctui_cmdpal *p, const char *s)
{
	for (; *s; s++)
		ctui_cmdpal_key(p, (unsigned char)*s, 24, 80);
}

static void test_keys(void)
{
	ctui_palette_hit hits[SAMPLE_N];
	ctui_cmdpal p;
	ctui_cmdpal_init(&p, SAMPLE, SAMPLE_N, hits, SAMPLE_N);
	CHECK(p.matches == SAMPLE_N && p.selected == 0 && p.used == 0);
	CHECK(ctui_cmdpal_chosen(&p) == 0);

	type(&p, "sa");
	CHECK(p.matches == 3 && p.used == 2);
	CHECK(ctui_cmdpal_key(&p, KEY_DOWN, 24, 80) == CTUI_DIALOG_OPEN &&
	      p.selected == 1);
	CHECK(ctui_cmdpal_chosen(&p) == 1);
	ctui_cmdpal_key(&p, KEY_DOWN, 24, 80);
	ctui_cmdpal_key(&p, KEY_DOWN, 24, 80);
	CHECK(p.selected == 0); /* wraps */
	ctui_cmdpal_key(&p, KEY_UP, 24, 80);
	CHECK(p.selected == 2 && ctui_cmdpal_chosen(&p) == 4);
	ctui_cmdpal_key(&p, 16, 24, 80); /* Ctrl-P */
	CHECK(p.selected == 1);
	ctui_cmdpal_key(&p, 14, 24, 80); /* Ctrl-N */
	CHECK(p.selected == 2);
	CHECK(ctui_cmdpal_key(&p, '\n', 24, 80) == CTUI_DIALOG_ACCEPTED);
	CHECK(ctui_cmdpal_key(&p, KEY_ENTER, 24, 80) == CTUI_DIALOG_ACCEPTED);

	/* typing resets the cursor to the best match */
	type(&p, "v");
	CHECK(p.selected == 0 && p.matches == 2 &&
	      ctui_cmdpal_chosen(&p) == 0); /* "sav" */
	ctui_cmdpal_key(&p, KEY_BACKSPACE, 24, 80);
	CHECK(p.used == 2 && p.matches == 3);
	ctui_cmdpal_key(&p, 127, 24, 80);
	ctui_cmdpal_key(&p, 8, 24, 80);
	ctui_cmdpal_key(&p, 127, 24, 80); /* nothing left to delete */
	CHECK(p.used == 0 && p.matches == SAMPLE_N);

	/* no match: Enter does nothing, nothing is chosen, movement is inert */
	type(&p, "zzz");
	CHECK(p.matches == 0 && ctui_cmdpal_chosen(&p) == (size_t)-1);
	CHECK(ctui_cmdpal_key(&p, '\n', 24, 80) == CTUI_DIALOG_OPEN);
	ctui_cmdpal_key(&p, KEY_DOWN, 24, 80);
	ctui_cmdpal_key(&p, KEY_UP, 24, 80);
	ctui_cmdpal_key(&p, KEY_NPAGE, 24, 80);
	ctui_cmdpal_key(&p, KEY_END, 24, 80);
	CHECK(p.selected == 0);

	/* Ctrl-U clears the query */
	ctui_cmdpal_key(&p, 21, 24, 80);
	CHECK(p.used == 0 && p.matches == SAMPLE_N);

	CHECK(ctui_cmdpal_key(&p, 27, 24, 80) == CTUI_DIALOG_CANCELLED);
	CHECK(ctui_cmdpal_key(&p, 3, 24, 80) == CTUI_DIALOG_CANCELLED);
	CHECK(ctui_cmdpal_key(&p, KEY_F(5), 24, 80) == CTUI_DIALOG_OPEN);
	CHECK(p.used == 0);

	ctui_cmdpal_key(&p, KEY_END, 24, 80);
	CHECK(p.selected == SAMPLE_N - 1);
	ctui_cmdpal_key(&p, KEY_HOME, 24, 80);
	CHECK(p.selected == 0);
}

static void test_keys_utf8(void)
{
	ctui_command cmds[] = {{"caf\xc3\xa9", NULL, NULL},
			       {"cafe", NULL, NULL}};
	ctui_palette_hit hits[2];
	ctui_cmdpal p;
	ctui_cmdpal_init(&p, cmds, 2, hits, 2);
	ctui_cmdpal_key(&p, 'c', 24, 80);
	ctui_cmdpal_key(&p, 0xC3, 24,
			80); /* half a character: the list is not refiltered */
	CHECK(p.matches == 2);
	ctui_cmdpal_key(&p, 0xA9, 24, 80);
	CHECK(p.used == 3 &&
	      p.matches == 1); /* only the first "cafÃ©" has the accent */
	ctui_cmdpal_key(&p, KEY_BACKSPACE, 24, 80); /* removes both bytes */
	CHECK(p.used == 1 && p.matches == 2);
	ctui_cmdpal_set_query(&p, "caf\xc3\xa9");
	CHECK(p.matches == 1 && ctui_cmdpal_chosen(&p) == 0);
	ctui_cmdpal_set_query(&p, NULL);
	CHECK(p.used == 0 && p.matches == 2);
	/* the input line is bounded */
	for (int i = 0; i < 300; i++)
		ctui_cmdpal_key(&p, 'z', 24, 80);
	CHECK(p.used == sizeof p.query - 1);
}

static ctui_command *many(size_t n, char (**names_out)[24])
{
	ctui_command *cmds = calloc(n, sizeof *cmds);
	char (*names)[24] = calloc(n, sizeof *names);
	for (size_t i = 0; i < n; i++) {
		snprintf(names[i], sizeof names[i], "Item %03zu", i);
		cmds[i].name = names[i];
	}
	*names_out = names;
	return cmds;
}

static void test_paging_and_mouse(void)
{
	enum { N = 100 };
	char (*names)[24];
	ctui_command *cmds = many(N, &names);
	ctui_palette_hit *hits = calloc(N, sizeof *hits);
	ctui_cmdpal p;
	ctui_cmdpal_init(&p, cmds, N, hits, N);

	ctui_rect box;
	int rows;
	CHECK(ctui_cmdpal_geometry(&p, 24, 80, &box, &rows));
	CHECK(box.height == 18 && box.width == 76 && box.y == 3 && box.x == 2 &&
	      rows == 14);
	CHECK(!ctui_cmdpal_geometry(&p, 8, 80, &box, &rows));
	CHECK(!ctui_cmdpal_geometry(&p, 24, 20, &box, &rows));
	CHECK(ctui_cmdpal_geometry(&p, 24, 200, &box, &rows) &&
	      box.width == 76);

	ctui_cmdpal_key(&p, KEY_NPAGE, 24, 80);
	CHECK(p.selected == 13);
	ctui_cmdpal_key(&p, KEY_NPAGE, 24, 80);
	CHECK(p.selected == 26 && p.first == 13); /* the window follows */
	ctui_cmdpal_key(&p, KEY_PPAGE, 24, 80);
	CHECK(p.selected == 13);
	for (int i = 0; i < 20; i++)
		ctui_cmdpal_key(&p, KEY_PPAGE, 24, 80);
	CHECK(p.selected == 0 && p.first == 0);
	ctui_cmdpal_key(&p, KEY_END, 24, 80);
	CHECK(p.selected == N - 1 && p.first == N - 14);

	/* a click on an entry chooses it */
	CHECK(ctui_cmdpal_geometry(&p, 24, 80, &box, &rows));
	ctui_cmdpal_init(&p, cmds, N, hits, N);
	CHECK(ctui_cmdpal_click(&p, 24, 80, box.y + 3 + 2, box.x + 5) ==
	      CTUI_DIALOG_ACCEPTED);
	CHECK(p.selected == 2 && ctui_cmdpal_chosen(&p) == 2);
	/* inside the frame but not on an entry */
	CHECK(ctui_cmdpal_click(&p, 24, 80, box.y + 1, box.x + 5) ==
	      CTUI_DIALOG_OPEN); /* input line */
	CHECK(ctui_cmdpal_click(&p, 24, 80, box.y + 2, box.x + 5) ==
	      CTUI_DIALOG_OPEN); /* rule */
	CHECK(ctui_cmdpal_click(&p, 24, 80, box.y + 3, box.x) ==
	      CTUI_DIALOG_OPEN); /* the border */
	CHECK(ctui_cmdpal_click(&p, 24, 80, 0, 0) ==
	      CTUI_DIALOG_OPEN); /* outside */
	CHECK(ctui_cmdpal_click(&p, 24, 80, box.y + box.height - 1,
				box.x + 5) == CTUI_DIALOG_OPEN);
	CHECK(p.selected == 2);
	/* the close button */
	CHECK(ctui_cmdpal_click(&p, 24, 80, box.y, box.x + box.width - 3) ==
	      CTUI_DIALOG_CANCELLED);
	CHECK(ctui_cmdpal_click(&p, 5, 80, box.y, box.x) == CTUI_DIALOG_OPEN);

	/* a click on an empty row below few matches */
	ctui_cmdpal_set_query(&p, "item 055");
	CHECK(p.matches > 0 && p.matches < 14);
	CHECK(ctui_cmdpal_click(&p, 24, 80, box.y + 3 + 13, box.x + 5) ==
	      CTUI_DIALOG_OPEN);

	/* wheel */
	ctui_cmdpal_init(&p, cmds, N, hits, N);
	ctui_cmdpal_wheel(&p, 24, 80, 1);
	CHECK(p.first == 3 && p.selected == 3);
	ctui_cmdpal_wheel(&p, 24, 80, 1);
	CHECK(p.first == 6);
	ctui_cmdpal_wheel(&p, 24, 80, -1);
	CHECK(p.first == 3);
	for (int i = 0; i < 100; i++)
		ctui_cmdpal_wheel(&p, 24, 80, 1);
	CHECK(p.first == N - 14 && p.selected >= p.first && p.selected < N);
	for (int i = 0; i < 100; i++)
		ctui_cmdpal_wheel(&p, 24, 80, -1);
	CHECK(p.first == 0 && p.selected <= 13);
	ctui_cmdpal_wheel(&p, 24, 80, 0);
	ctui_cmdpal_wheel(&p, 3, 3, 1); /* too small: nothing happens */
	/* clicking after a wheel scroll hits the scrolled entry */
	ctui_cmdpal_wheel(&p, 24, 80, 1);
	ctui_cmdpal_click(&p, 24, 80, box.y + 3, box.x + 5);
	CHECK(p.selected == 3);

	free(cmds);
	free(names);
	free(hits);
}

static void test_small_buffer(void)
{
	ctui_palette_hit hits[3];
	ctui_cmdpal p;
	ctui_cmdpal_init(&p, SAMPLE, SAMPLE_N, hits,
			 3); /* ranks only the first 3 matches */
	CHECK(p.matches == 3);
	ctui_cmdpal_init(&p, SAMPLE, SAMPLE_N, NULL, 0);
	CHECK(p.matches == 0 && ctui_cmdpal_chosen(&p) == (size_t)-1);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_palette_hit hits[SAMPLE_N];
	ctui_cmdpal p;
	ctui_cmdpal_init(&p, SAMPLE, SAMPLE_N, hits, SAMPLE_N);

	screen_clear();
	ctui_cmdpal_draw(&p);
	CHECK(screen_has_text("Command palette"));
	CHECK(screen_has_text("Save File") && screen_has_text("Ctrl-S"));
	CHECK(screen_has_text("Write the buffer"));
	CHECK(screen_has_text("Close"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, true)));

	type(&p, "sf");
	screen_clear();
	ctui_cmdpal_draw(&p);
	CHECK(screen_has_text("> sf"));
	CHECK(screen_has_text("Save File") && !screen_has_text("Open File"));

	type(&p, "zz");
	screen_clear();
	ctui_cmdpal_draw(&p);
	CHECK(screen_has_text("No matching command"));

	/* per-instance labels and style */
	ctui_labels mine = {.palette_title = "Commandes",
			    .palette_empty = "Rien"};
	p.labels = &mine;
	ctui_style hot = {0};
	hot.attr[CTUI_PAIR_SELECTION] = ctui_attr_color(16, 214);
	p.style = &hot;
	screen_clear();
	ctui_cmdpal_draw(&p);
	CHECK(screen_has_text("Commandes") && screen_has_text("Rien") &&
	      !screen_has_text("Command palette"));

	/* too small a screen draws nothing and does not crash */
	ctui_cmdpal_set_query(&p, "");
	screen_clear();
	ctui_cmdpal_draw(&p);
	endwin();
}

int main(void)
{
	RUN(test_score_basics);
	RUN(test_score_bonuses);
	RUN(test_score_positions);
	RUN(test_score_utf8);
	RUN(test_score_limits);
	RUN(test_filter_order);
	RUN(test_filter_large);
	RUN(test_keys);
	RUN(test_keys_utf8);
	RUN(test_paging_and_mouse);
	RUN(test_small_buffer);
	RUN(test_draw);
	return CHECK_DONE();
}
