/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/keyboard.h>

static void test_init(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 2);
	CHECK(kb.first == 60 && kb.octaves == 2 && kb.cursor == 60 &&
	      kb.base == 60);
	CHECK(kb.last == -1 && kb.mouse_note == -1 && !kb.selection &&
	      !kb.dragging);
	CHECK(ctui_keyboard_held_count(&kb) == 0);
	ctui_keyboard_init(&kb, 500, 0);
	CHECK(kb.first == 127 && kb.octaves == 1);
	ctui_keyboard_init(&kb, -3, 99);
	CHECK(kb.first == 0 && kb.octaves == 10);
}

static void test_black_and_names(void)
{
	const bool black[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
	for (int i = 0; i < 12; i++)
		CHECK(ctui_keyboard_is_black(60 + i) == black[i]);
	CHECK(!ctui_keyboard_is_black(-1));

	char buf[8];
	CHECK_STR(ctui_keyboard_name(buf, sizeof buf, 60), "C4");
	CHECK_STR(ctui_keyboard_name(buf, sizeof buf, 61), "C#4");
	CHECK_STR(ctui_keyboard_name(buf, sizeof buf, 69), "A4");
	CHECK_STR(ctui_keyboard_name(buf, sizeof buf, 0), "C-1");
	CHECK_STR(ctui_keyboard_name(buf, sizeof buf, 127), "G9");
	CHECK_STR(ctui_keyboard_name(buf, sizeof buf, 128), "");
	CHECK_STR(ctui_keyboard_name(buf, sizeof buf, -1), "");
	CHECK_STR(ctui_keyboard_name(buf, 3, 61),
		  "C#"); /* cut, still terminated */
	CHECK_STR(ctui_keyboard_name(buf, 1, 61), "");
}

static void test_typing_keys(void)
{
	const char *keys = "awsedftgyhujkolp";
	for (int i = 0; keys[i]; i++)
		CHECK(ctui_keyboard_key_note(keys[i], 60) == 60 + i);
	CHECK(ctui_keyboard_key_note('q', 60) == -1);
	CHECK(ctui_keyboard_key_note('A', 60) == -1);
	CHECK(ctui_keyboard_key_note(0, 60) == -1);
	CHECK(ctui_keyboard_key_note(KEY_LEFT, 60) == -1);
	CHECK(ctui_keyboard_key_note('a', 127) == 127);
	CHECK(ctui_keyboard_key_note('s', 127) == -1); /* above G9 */
	CHECK(ctui_keyboard_key_note('a', 0) == 0);
}

static void test_range_and_size(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 2);
	CHECK(ctui_keyboard_last(&kb) == 83);
	CHECK(ctui_keyboard_contains(&kb, 60) &&
	      ctui_keyboard_contains(&kb, 83));
	CHECK(!ctui_keyboard_contains(&kb, 59) &&
	      !ctui_keyboard_contains(&kb, 84));
	CHECK(ctui_keyboard_whites(&kb) == 14);
	CHECK(ctui_keyboard_width(&kb) == 56);
	kb.key_width = 3;
	CHECK(ctui_keyboard_width(&kb) == 42);
	kb.key_width = 100; /* clamped to 8 */
	CHECK(ctui_keyboard_width(&kb) == 112);
	kb.key_width = 0; /* clamped to 2 */
	CHECK(ctui_keyboard_width(&kb) == 28);

	ctui_keyboard_init(&kb, 120, 3); /* stops at G9 */
	CHECK(ctui_keyboard_last(&kb) == 127);
	CHECK(ctui_keyboard_whites(&kb) == 5); /* C D E F G */

	ctui_keyboard_init(&kb, 61, 1); /* starts on a black key: C# .. C */
	CHECK(ctui_keyboard_last(&kb) == 72);
	CHECK(ctui_keyboard_whites(&kb) == 7);
}

static void test_key_x(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 1);
	int w;
	CHECK(ctui_keyboard_key_x(&kb, 60, &w) == 0 && w == 4);
	CHECK(ctui_keyboard_key_x(&kb, 62, &w) == 4 && w == 4);
	CHECK(ctui_keyboard_key_x(&kb, 71, &w) == 24 && w == 4);
	CHECK(ctui_keyboard_key_x(&kb, 61, &w) == 3 &&
	      w == 3); /* centred on the border at 4 */
	CHECK(ctui_keyboard_key_x(&kb, 63, &w) == 7 && w == 3);
	CHECK(ctui_keyboard_key_x(&kb, 59, &w) == -1);
	CHECK(ctui_keyboard_key_x(&kb, 72, NULL) == -1);
	CHECK(ctui_keyboard_key_x(&kb, 62, NULL) == 4);
}

static void test_note_at(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 1);
	int h = 4; /* two rows of black keys */
	CHECK(ctui_keyboard_note_at(&kb, 3, 0, h) == 60);
	CHECK(ctui_keyboard_note_at(&kb, 3, 3, h) == 60); /* divider column */
	CHECK(ctui_keyboard_note_at(&kb, 3, 4, h) == 62);
	CHECK(ctui_keyboard_note_at(&kb, 3, 27, h) == 71);
	CHECK(ctui_keyboard_note_at(&kb, 0, 0, h) == 60);
	CHECK(ctui_keyboard_note_at(&kb, 0, 2, h) == 60);
	CHECK(ctui_keyboard_note_at(&kb, 0, 3, h) ==
	      61); /* the black key C# covers 3..5 */
	CHECK(ctui_keyboard_note_at(&kb, 0, 5, h) == 61);
	CHECK(ctui_keyboard_note_at(&kb, 1, 5, h) == 61);
	CHECK(ctui_keyboard_note_at(&kb, 2, 5, h) ==
	      62); /* below the black keys */
	CHECK(ctui_keyboard_note_at(&kb, 2, 3, h) == 60);
	CHECK(ctui_keyboard_note_at(&kb, 0, 6, h) == 62);
	CHECK(ctui_keyboard_note_at(&kb, 0, 7, h) == 63);
	/* no black key between E and F */
	CHECK(ctui_keyboard_note_at(&kb, 0, 11, h) == 64);
	CHECK(ctui_keyboard_note_at(&kb, 0, 12, h) == 65);
	CHECK(ctui_keyboard_note_at(&kb, 0, 14, h) == 65);
	CHECK(ctui_keyboard_note_at(&kb, 0, 15, h) == 66);
	/* outside */
	CHECK(ctui_keyboard_note_at(&kb, -1, 0, h) == -1);
	CHECK(ctui_keyboard_note_at(&kb, 4, 0, h) == -1);
	CHECK(ctui_keyboard_note_at(&kb, 0, -1, h) == -1);
	CHECK(ctui_keyboard_note_at(&kb, 0, 28, h) == -1);
	/* three rows: a single row of black keys */
	CHECK(ctui_keyboard_note_at(&kb, 0, 3, 3) == 61);
	CHECK(ctui_keyboard_note_at(&kb, 1, 3, 3) == 60);

	/* every cell of the keyboard is a key of the range, and every key is
	 * found */
	int seen[128] = {0};
	for (int y = 0; y < h; y++)
		for (int x = 0; x < ctui_keyboard_width(&kb); x++) {
			int n = ctui_keyboard_note_at(&kb, y, x, h);
			CHECK(ctui_keyboard_contains(&kb, n));
			if (n >= 0 && n < 128)
				seen[n]++;
		}
	for (int n = 60; n < 72; n++)
		CHECK(seen[n] > 0);

	/* the left edge of a keyboard starting on a black key hangs out of it
	 */
	ctui_keyboard_init(&kb, 61, 1);
	CHECK(ctui_keyboard_note_at(&kb, 0, 0, h) == 61);
	CHECK(ctui_keyboard_note_at(&kb, 0, 2, h) == 62);
	CHECK(ctui_keyboard_note_at(&kb, 3, 0, h) == 62);
}

static void test_held(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 0, 10);
	ctui_keyboard_set_held(&kb, 60, true);
	ctui_keyboard_set_held(&kb, 0, true);
	ctui_keyboard_set_held(&kb, 127, true);
	ctui_keyboard_set_held(&kb, 32, true);
	ctui_keyboard_set_held(&kb, 200, true); /* ignored */
	ctui_keyboard_set_held(&kb, -5, true);
	CHECK(ctui_keyboard_is_held(&kb, 60) && ctui_keyboard_is_held(&kb, 0));
	CHECK(ctui_keyboard_is_held(&kb, 127) &&
	      ctui_keyboard_is_held(&kb, 32));
	CHECK(!ctui_keyboard_is_held(&kb, 61) &&
	      !ctui_keyboard_is_held(&kb, 200) &&
	      !ctui_keyboard_is_held(&kb, -5));
	CHECK(ctui_keyboard_held_count(&kb) == 4);
	ctui_keyboard_set_held(&kb, 60, false);
	CHECK(!ctui_keyboard_is_held(&kb, 60) &&
	      ctui_keyboard_held_count(&kb) == 3);
	ctui_keyboard_release_all(&kb);
	CHECK(ctui_keyboard_held_count(&kb) == 0);
}

static void test_cursor_keys(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 48, 3); /* C3 .. B5, 48..83 */
	CHECK(ctui_keyboard_key(&kb, KEY_RIGHT) && kb.cursor == 49);
	ctui_keyboard_key(&kb, KEY_LEFT);
	ctui_keyboard_key(&kb, KEY_LEFT);
	CHECK(kb.cursor == 48);
	ctui_keyboard_key(&kb, KEY_UP);
	CHECK(kb.cursor == 60);
	ctui_keyboard_key(&kb, KEY_DOWN);
	ctui_keyboard_key(&kb, KEY_DOWN);
	CHECK(kb.cursor == 48);
	ctui_keyboard_key(&kb, KEY_END);
	CHECK(kb.cursor == 83);
	ctui_keyboard_key(&kb, KEY_RIGHT);
	CHECK(kb.cursor == 83);
	ctui_keyboard_key(&kb, KEY_UP);
	CHECK(kb.cursor == 83);
	ctui_keyboard_key(&kb, KEY_HOME);
	CHECK(kb.cursor == 48);
	CHECK(!ctui_keyboard_key(&kb, 'q') &&
	      !ctui_keyboard_key(&kb, KEY_F(1)));
	CHECK(kb.last == -1 && ctui_keyboard_held_count(&kb) == 0);
}

static void test_selection(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 48, 2);
	ctui_keyboard_key(&kb, KEY_RIGHT); /* 49 */
	ctui_keyboard_key(&kb, KEY_SRIGHT);
	ctui_keyboard_key(&kb, KEY_SRIGHT);
	CHECK(kb.selection && kb.sel_first == 49 && kb.sel_last == 51 &&
	      kb.cursor == 51);
	ctui_keyboard_key(&kb, KEY_SLEFT);
	CHECK(kb.sel_first == 49 && kb.sel_last == 50);
	ctui_keyboard_key(&kb, KEY_SLEFT);
	ctui_keyboard_key(&kb, KEY_SLEFT); /* past the anchor: leftwards */
	CHECK(kb.sel_first == 48 && kb.sel_last == 49 && kb.cursor == 48);
	ctui_keyboard_key(&kb, KEY_SLEFT); /* at the edge */
	CHECK(kb.sel_first == 48 && kb.sel_last == 49);
	ctui_keyboard_key(&kb, KEY_RIGHT); /* a plain move drops it */
	CHECK(!kb.selection && kb.cursor == 49);
	ctui_keyboard_key(&kb, KEY_SRIGHT);
	ctui_keyboard_clear_selection(&kb);
	CHECK(!kb.selection);
	CHECK(ctui_keyboard_move(&kb, 3, false) && kb.cursor == 53);
	CHECK(!ctui_keyboard_move(&kb, 100, false) || kb.cursor == 71);
	CHECK(!ctui_keyboard_move(&kb, 1, false) && kb.cursor == 71);
}

static void test_typing(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 2);
	CHECK(ctui_keyboard_key(&kb, 'a'));
	CHECK(kb.last == 60 && kb.cursor == 60 &&
	      ctui_keyboard_is_held(&kb, 60));
	CHECK(ctui_keyboard_key(&kb, 'w'));
	CHECK(kb.last == 61 && ctui_keyboard_is_held(&kb, 61) &&
	      ctui_keyboard_held_count(&kb) == 2);
	CHECK(ctui_keyboard_key(&kb, 'p')); /* 75, inside two octaves */
	CHECK(kb.last == 75);
	/* the octave shift moves the typing keys, not the keyboard */
	ctui_keyboard_release_all(&kb);
	CHECK(ctui_keyboard_key(&kb, 'x') && kb.base == 72 && kb.first == 60);
	CHECK(ctui_keyboard_key(&kb, 'a') && kb.last == 72);
	CHECK(!ctui_keyboard_key(&kb, 'k')); /* 84 is above the keyboard */
	CHECK(kb.last == 72);
	ctui_keyboard_key(&kb, 'z');
	ctui_keyboard_key(&kb, 'z');
	CHECK(kb.base == 48);
	CHECK(!ctui_keyboard_key(&kb, 'a')); /* 48 is below the keyboard */
	for (int i = 0; i < 20; i++)
		ctui_keyboard_key(&kb, 'z');
	CHECK(kb.base == 0);
	for (int i = 0; i < 20; i++)
		ctui_keyboard_key(&kb, 'x');
	CHECK(kb.base == 120);
	/* a typed note drops the selection */
	ctui_keyboard_init(&kb, 60, 1);
	ctui_keyboard_key(&kb, KEY_SRIGHT);
	ctui_keyboard_key(&kb, 's');
	CHECK(!kb.selection && kb.cursor == 62);
}

static void test_toggle(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 1);
	ctui_keyboard_key(&kb, KEY_RIGHT);
	CHECK(ctui_keyboard_key(&kb, ' ') && ctui_keyboard_is_held(&kb, 61) &&
	      kb.last == 61);
	kb.last = -1;
	CHECK(ctui_keyboard_key(&kb, '\n') && !ctui_keyboard_is_held(&kb, 61) &&
	      kb.last == -1);
	CHECK(ctui_keyboard_key(&kb, KEY_ENTER) &&
	      ctui_keyboard_is_held(&kb, 61));
}

static void test_mouse(void)
{
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 1);
	CHECK(!ctui_keyboard_drag(&kb, 3, 0, 4)); /* no press yet */
	CHECK(!ctui_keyboard_release(&kb));
	CHECK(!ctui_keyboard_press(&kb, 3, 40, 4)); /* off the keys */
	CHECK(!kb.dragging && ctui_keyboard_held_count(&kb) == 0);

	CHECK(ctui_keyboard_press(&kb, 3, 5, 4)); /* D */
	CHECK(kb.dragging && kb.cursor == 62 && kb.last == 62 &&
	      ctui_keyboard_is_held(&kb, 62));
	CHECK(ctui_keyboard_drag(&kb, 3, 6, 4)); /* same key: nothing changes */
	CHECK(ctui_keyboard_held_count(&kb) == 1);
	CHECK(ctui_keyboard_drag(&kb, 3, 9, 4)); /* glide to E: D is let go */
	CHECK(ctui_keyboard_is_held(&kb, 64) &&
	      !ctui_keyboard_is_held(&kb, 62) && kb.last == 64);
	CHECK(ctui_keyboard_drag(&kb, 0, 7, 4)); /* up onto D# */
	CHECK(ctui_keyboard_is_held(&kb, 63) &&
	      !ctui_keyboard_is_held(&kb, 64));
	CHECK(ctui_keyboard_drag(&kb, 9, 100, 4)); /* outside: stays on D# */
	CHECK(ctui_keyboard_is_held(&kb, 63) && kb.last == 63);
	CHECK(ctui_keyboard_release(&kb));
	CHECK(ctui_keyboard_held_count(&kb) == 0 && !kb.dragging &&
	      kb.mouse_note == -1);
	CHECK(!ctui_keyboard_release(&kb));

	/* a note held by a key survives a mouse press elsewhere */
	ctui_keyboard_key(&kb, 'a');
	ctui_keyboard_press(&kb, 3, 5, 4);
	ctui_keyboard_release(&kb);
	CHECK(ctui_keyboard_is_held(&kb, 60) &&
	      !ctui_keyboard_is_held(&kb, 62));
	/* a new press lets go of the previous mouse note even without a release
	 */
	ctui_keyboard_press(&kb, 3, 5, 4);
	ctui_keyboard_press(&kb, 3, 9, 4);
	CHECK(!ctui_keyboard_is_held(&kb, 62) &&
	      ctui_keyboard_is_held(&kb, 64));
	/* a note latched by a key stays held when the mouse presses and
	 * releases it */
	ctui_keyboard_release_all(&kb);
	ctui_keyboard_set_held(&kb, 62, true);
	ctui_keyboard_press(&kb, 3, 5, 4);
	ctui_keyboard_release(&kb);
	CHECK(ctui_keyboard_is_held(&kb, 62));
	ctui_keyboard_press(&kb, 3, 5, 4);
	ctui_keyboard_drag(&kb, 3, 9, 4); /* glide away from the latched D */
	CHECK(ctui_keyboard_is_held(&kb, 62) && ctui_keyboard_is_held(&kb, 64));
	ctui_keyboard_release(&kb);
	CHECK(ctui_keyboard_is_held(&kb, 62) &&
	      !ctui_keyboard_is_held(&kb, 64));
	/* a press drops the selection */
	ctui_keyboard_release(&kb);
	ctui_keyboard_key(&kb, KEY_SRIGHT);
	ctui_keyboard_press(&kb, 3, 1, 4);
	CHECK(!kb.selection);
}

static int count_text(const char *s)
{
	char row[1024];
	int n = 0;
	for (int y = 0; y < LINES; y++)
		if (mvinnstr(y, 0, row, (int)sizeof row - 1) != ERR)
			for (const char *p = row; (p = strstr(p, s));
			     p += strlen(s))
				n++;
	return n;
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 2);
	kb.cursor = 67; /* G4 */
	screen_clear();
	ctui_keyboard_draw(&kb, 2, 3, 56, 4);
	CHECK(screen_has_text("C4") && screen_has_text("C5") &&
	      screen_has_text("G4"));
	CHECK(!screen_has_text("D4") && !screen_has_text("C#4"));
	CHECK(count_text("\xe2\x94\x82") >=
	      2 * 14); /* one divider per white key and row */
	CHECK(screen_has_color(ctui_attr_color(ctui_theme_palette()->bar,
					       ctui_theme_palette()->text)));
	CHECK(screen_has_color(ctui_attr_color(ctui_theme_palette()->text,
					       ctui_theme_palette()->black)));
	CHECK(screen_has_color(ctui_attr_color(ctui_theme_palette()->bar,
					       ctui_theme_palette()->cyan)));
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, false)));
	CHECK(!screen_has_color(ctui_attr_color(ctui_theme_palette()->bar,
						ctui_theme_palette()->yellow)));

	/* a cursor on a black key is named too */
	ctui_keyboard_init(&kb, 60, 2);
	kb.cursor = 66; /* F#4 */
	screen_clear();
	ctui_keyboard_draw(&kb, 2, 3, 56, 4);
	CHECK(screen_has_text("F#4") && screen_has_text("C4") &&
	      !screen_has_text("G4"));

	/* a held note and a selection are highlighted */
	ctui_keyboard_init(&kb, 60, 2);
	kb.cursor = 60;
	ctui_keyboard_set_held(&kb, 64, true);
	ctui_keyboard_set_held(&kb, 70, true);
	kb.selection = true;
	kb.sel_first = 72;
	kb.sel_last = 74;
	screen_clear();
	ctui_keyboard_draw(&kb, 0, 0, 56, 4);
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, false)));
	CHECK(screen_has_color(ctui_attr_color(ctui_theme_palette()->bar,
					       ctui_theme_palette()->yellow)));

	/* the held key is pressed over the cells of that key only */
	ctui_keyboard_init(&kb, 60, 1);
	ctui_keyboard_set_held(&kb, 62, true);
	screen_clear();
	ctui_keyboard_draw(&kb, 0, 0, 28, 4);
	int sel = ctui_attr(CTUI_PAIR_SELECTION, false);
	short want = (short)PAIR_NUMBER((chtype)sel);
	int pressed = 0;
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 28; x++) {
			cchar_t cell;
			wchar_t wch[CCHARW_MAX];
			attr_t attrs;
			short pair;
			if (mvin_wch(y, x, &cell) == OK &&
			    getcchar(&cell, wch, &attrs, &pair, NULL) == OK &&
			    pair == want) {
				pressed++;
				CHECK(x >= 4 &&
				      x < 8); /* the D key, black keys apart */
			}
		}
	CHECK(pressed > 0);

	/* the window clips the keyboard; too short draws nothing */
	screen_clear();
	ctui_keyboard_draw(&kb, 0, 0, 10, 4);
	CHECK(screen_has_text("C4") && !screen_has_text("C5"));
	int c = 0;
	for (int x = 10; x < 30; x++)
		c += (int)(mvinch(0, x) & A_CHARTEXT) != ' ';
	CHECK(c == 0);
	screen_clear();
	ctui_keyboard_draw(&kb, 0, 0, 28, 2);
	CHECK(!screen_has_text("C4"));
	ctui_keyboard_draw(&kb, 0, 0, 0, 4);

	/* a narrow column count keeps the labels inside it */
	kb.key_width = 2;
	screen_clear();
	ctui_keyboard_draw(&kb, 0, 0, 14, 3);
	CHECK(screen_has_text("C"));
	CHECK(!screen_has_text("C4"));
	endwin();
}

static void test_draw_ascii(void)
{
	screen_open(24, 80);
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 1);
	ctui_keyboard_set_held(&kb, 61, true);
	ctui_keyboard_set_held(&kb, 62, true);
	screen_clear();
	ctui_keyboard_draw(&kb, 0, 0, 28, 4);
	CHECK(screen_has_text("C4"));
	CHECK(screen_has_text("#") && screen_has_text("@") &&
	      screen_has_text("=") && screen_has_text("|"));
	CHECK(!screen_has_text("\xe2\x94\x82") &&
	      !screen_has_text("\xe2\x96\x92"));
	unsetenv("CTUI_ASCII");
	endwin();
}

static void test_draw_style(void)
{
	screen_open(24, 80);
	ctui_style style = {0};
	style.glyphs.box_v = "!";
	ctui_keyboard kb;
	ctui_keyboard_init(&kb, 60, 1);
	kb.style = &style;
	screen_clear();
	ctui_keyboard_draw(&kb, 0, 0, 28, 4);
	CHECK(screen_has_text("!"));
	CHECK(!screen_has_text("\xe2\x94\x82"));
	kb.style = NULL;
	screen_clear();
	ctui_keyboard_draw(&kb, 0, 0, 28, 4);
	CHECK(screen_has_text("\xe2\x94\x82"));
	endwin();
}

int main(void)
{
	RUN(test_init);
	RUN(test_black_and_names);
	RUN(test_typing_keys);
	RUN(test_range_and_size);
	RUN(test_key_x);
	RUN(test_note_at);
	RUN(test_held);
	RUN(test_cursor_keys);
	RUN(test_selection);
	RUN(test_typing);
	RUN(test_toggle);
	RUN(test_mouse);
	RUN(test_draw);
	RUN(test_draw_ascii);
	RUN(test_draw_style);
	return CHECK_DONE();
}
