/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ctui/hexview.h>

static unsigned char DATA[100];

static void fixture(ctui_hexview *v)
{
	for (size_t i = 0; i < sizeof DATA; i++)
		DATA[i] = (unsigned char)i;
	ctui_hexview_init(v, DATA, sizeof DATA);
}

static void test_layout(void)
{
	ctui_hexview v;
	fixture(&v);
	ctui_hexlayout l;
	CHECK(ctui_hexview_layout(&v, 80, &l));
	CHECK(l.per_row == 16 && l.hex_x == 10 && l.text_x == 60 &&
	      l.width == 76);
	CHECK(ctui_hexview_layout(&v, 75, &l) && l.per_row == 8 &&
	      l.text_x == 35);
	CHECK(ctui_hexview_layout(&v, 30, &l) && l.per_row == 4);
	CHECK(!ctui_hexview_layout(&v, 26, &l));
	CHECK(ctui_hexview_rows(&v) == 7);
	ctui_hexview empty;
	ctui_hexview_init(&empty, NULL, 5);
	CHECK(empty.size == 0 && ctui_hexview_rows(&empty) == 1);
}

static void test_keys(void)
{
	ctui_hexview v;
	fixture(&v);
	CHECK(!ctui_hexview_key(&v, KEY_LEFT, 5) && v.cursor == 0);
	CHECK(!ctui_hexview_key(&v, KEY_UP, 5));
	CHECK(ctui_hexview_key(&v, KEY_RIGHT, 5) && v.cursor == 1);
	CHECK(ctui_hexview_key(&v, KEY_DOWN, 5) && v.cursor == 17);
	CHECK(ctui_hexview_key(&v, KEY_END, 5) && v.cursor == 31);
	CHECK(ctui_hexview_key(&v, KEY_HOME, 5) && v.cursor == 16);
	CHECK(ctui_hexview_key(&v, KEY_NPAGE, 2) && v.cursor == 48);
	CHECK(ctui_hexview_key(&v, KEY_PPAGE, 2) && v.cursor == 16);
	CHECK(ctui_hexview_key(&v, KEY_PPAGE, 9) && v.cursor == 0);
	CHECK(!ctui_hexview_key(&v, 'x', 5));
	/* The short last row: down from column 15 lands on the last byte. */
	v.cursor = 95;
	CHECK(ctui_hexview_key(&v, KEY_DOWN, 5) && v.cursor == 99);
	CHECK(!ctui_hexview_key(&v, KEY_DOWN, 5) && v.cursor == 99);
	CHECK(ctui_hexview_key(&v, KEY_END, 5) == false && v.cursor == 99);
	CHECK(!ctui_hexview_key(&v, KEY_RIGHT, 5));
	ctui_hexview none;
	ctui_hexview_init(&none, NULL, 0);
	CHECK(!ctui_hexview_key(&none, KEY_RIGHT, 5));
}

static void test_selection(void)
{
	ctui_hexview v;
	fixture(&v);
	v.cursor = 10;
	CHECK(ctui_hexview_key(&v, KEY_SRIGHT, 5) && v.selecting &&
	      v.anchor == 10 && v.cursor == 11);
	CHECK(ctui_hexview_key(&v, KEY_SF, 5) && v.cursor == 27);
	size_t a, b;
	CHECK(ctui_hexview_range(&v, &a, &b) && a == 10 && b == 27);
	CHECK(ctui_hexview_selected(&v, 10) && ctui_hexview_selected(&v, 27));
	CHECK(!ctui_hexview_selected(&v, 9) && !ctui_hexview_selected(&v, 28));
	CHECK(ctui_hexview_key(&v, KEY_LEFT, 5) && !v.selecting &&
	      v.cursor == 26);
	CHECK(!ctui_hexview_range(&v, &a, &b));
	CHECK(ctui_hexview_key(&v, KEY_SLEFT, 5) &&
	      ctui_hexview_key(&v, 27, 5) && !v.selecting);
	CHECK(!ctui_hexview_key(&v, 27, 5));
	CHECK(ctui_hexview_key(&v, 1, 5) && v.anchor == 0 && v.cursor == 99);
	CHECK(ctui_hexview_range(&v, &a, &b) && a == 0 && b == 99);
}

static void test_scroll(void)
{
	ctui_hexview v;
	fixture(&v);
	v.cursor = 80; /* the sixth row of 16 bytes */
	ctui_hexview_reveal(&v, 3);
	CHECK(v.top == 3);
	v.cursor = 0;
	ctui_hexview_reveal(&v, 3);
	CHECK(v.top == 0);
	ctui_hexview_scroll_to(&v, 3, 4);
	CHECK(v.top == 4 && v.cursor == 64);
	ctui_hexview_scroll_to(&v, 3, 99);
	CHECK(v.top == 4); /* 7 rows, 3 shown */
	ctui_hexview_scroll_to(&v, 3, 0);
	CHECK(v.top == 0 && v.cursor == 32);
	v.cursor = 99;
	ctui_hexview_scroll_to(&v, 3, 0);
	CHECK(v.cursor == 35); /* the last row shown, same column */
}

static void test_hit(void)
{
	ctui_hexview v;
	fixture(&v);
	size_t at;
	CHECK(ctui_hexview_hit(&v, 80, 0, 10, &at) && at == 0);
	CHECK(ctui_hexview_hit(&v, 80, 0, 11, &at) && at == 0);
	CHECK(!ctui_hexview_hit(&v, 80, 0, 12, &at)); /* between two bytes */
	CHECK(ctui_hexview_hit(&v, 80, 0, 13, &at) && at == 1);
	CHECK(ctui_hexview_hit(&v, 80, 0, 10 + 8 * 3 + 1, &at) &&
	      at == 8); /* after the wide gap */
	CHECK(ctui_hexview_hit(&v, 80, 1, 60 + 3, &at) && at == 19);
	CHECK(!ctui_hexview_hit(&v, 80, 0, 5, &at));
	CHECK(!ctui_hexview_hit(&v, 80, 0, 76, &at));
	CHECK(!ctui_hexview_hit(&v, 80, 6, 10 + 4 * 3,
				&at)); /* past the data: byte 100 */
	CHECK(ctui_hexview_hit(&v, 80, 6, 10 + 3 * 3, &at) && at == 99);
	v.top = 2;
	CHECK(ctui_hexview_hit(&v, 80, 0, 10, &at) && at == 32);
	CHECK(!ctui_hexview_hit(&v, 30, 0, 100, &at));
}

static void test_mouse(void)
{
	ctui_hexview v;
	fixture(&v);
	v.selecting = true;
	CHECK(ctui_hexview_press(&v, 80, 1, 10 + 2 * 3) && v.cursor == 18 &&
	      !v.selecting && v.dragging);
	CHECK(!ctui_hexview_drag(&v, 80, 1, 10 + 2 * 3)); /* same byte */
	CHECK(ctui_hexview_drag(&v, 80, 2, 61) && v.selecting &&
	      v.anchor == 18 && v.cursor == 33);
	CHECK(ctui_hexview_drag(&v, 80, -3, 0) &&
	      v.cursor == 0); /* above and left: nearest */
	CHECK(ctui_hexview_drag(&v, 80, 50, 79) && v.cursor == 99);
	CHECK(ctui_hexview_release(&v) && !v.dragging && v.selecting);
	CHECK(!ctui_hexview_release(&v));
	CHECK(!ctui_hexview_drag(&v, 80, 1, 10));
	CHECK(!ctui_hexview_press(&v, 80, 0, 3) && !v.dragging);
}

static void test_load(void)
{
	char path[] = "/tmp/ctui-hex-XXXXXX";
	int fd = mkstemp(path);
	CHECK(fd >= 0);
	unsigned char bytes[6000];
	for (size_t i = 0; i < sizeof bytes; i++)
		bytes[i] = (unsigned char)(i * 7);
	CHECK(write(fd, bytes, sizeof bytes) == (ssize_t)sizeof bytes);
	close(fd);
	ctui_hexview v;
	CHECK(ctui_hexview_load(&v, path, 0));
	CHECK(v.size == sizeof bytes &&
	      memcmp(v.data, bytes, sizeof bytes) == 0);
	ctui_hexview_free(&v);
	CHECK(v.size == 0 && v.owned == NULL);
	CHECK(!ctui_hexview_load(&v, path, 5000) && errno == EFBIG &&
	      v.size == 0);
	CHECK(ctui_hexview_load(&v, path, sizeof bytes) &&
	      v.size == sizeof bytes);
	ctui_hexview_free(&v);
	CHECK(!ctui_hexview_load(&v, "/nonexistent/file", 0) &&
	      errno == ENOENT);
	CHECK(!ctui_hexview_load(&v, "/tmp", 0) && errno == EISDIR);
	unlink(path);
}

static void test_map(void)
{
	char path[] = "/tmp/ctui-map-XXXXXX";
	int fd = mkstemp(path);
	CHECK(fd >= 0);
	unsigned char bytes[9000];
	for (size_t i = 0; i < sizeof bytes; i++)
		bytes[i] = (unsigned char)(i * 13 + 1);
	CHECK(write(fd, bytes, sizeof bytes) == (ssize_t)sizeof bytes);
	close(fd);
	ctui_hexview v;
	CHECK(ctui_hexview_map(&v, path));
	CHECK(v.size == sizeof bytes && v.mapping != NULL && v.owned == NULL);
	CHECK(memcmp(v.data, bytes, sizeof bytes) == 0);
	CHECK(ctui_hexview_byte(&v, 0) == 1 &&
	      ctui_hexview_byte(&v, 8999) == bytes[8999]);
	CHECK(ctui_hexview_byte(&v, 9000) == 0);
	ctui_hexview_free(&v);
	CHECK(v.size == 0 && v.mapping == NULL);
	/* Empty file: an empty view. */
	CHECK(truncate(path, 0) == 0);
	CHECK(ctui_hexview_map(&v, path) && v.size == 0 && v.mapping == NULL);
	ctui_hexview_free(&v);
	unlink(path);
	CHECK(!ctui_hexview_map(&v, "/nonexistent/file") && errno == ENOENT &&
	      v.size == 0);
	CHECK(!ctui_hexview_map(&v, "/tmp") && errno == EISDIR);
	/* Not a regular file: read like ctui_hexview_load(). */
	CHECK(ctui_hexview_map(&v, "/dev/null") && v.size == 0);
	ctui_hexview_free(&v);
}

static void test_edit(void)
{
	ctui_hexview v;
	fixture(&v);
	CHECK(ctui_hexview_modified(&v) == 0 && ctui_hexview_byte(&v, 5) == 5);
	CHECK(ctui_hexview_set(&v, 50, 0xAA) &&
	      ctui_hexview_set(&v, 10, 0xBB) && ctui_hexview_set(&v, 70, 0xCC));
	CHECK(ctui_hexview_modified(&v) == 3 && v.edits[0].offset == 10 &&
	      v.edits[1].offset == 50 && v.edits[2].offset == 70);
	CHECK(ctui_hexview_byte(&v, 10) == 0xBB &&
	      ctui_hexview_byte(&v, 11) == 11);
	CHECK(ctui_hexview_edited(&v, 50) && !ctui_hexview_edited(&v, 51));
	CHECK(DATA[10] == 10 && ctui_hexview_original(&v, 10) ==
				    10); /* the source is untouched */
	CHECK(ctui_hexview_set(&v, 50, 0xAB) &&
	      ctui_hexview_modified(&v) == 3 &&
	      ctui_hexview_byte(&v, 50) == 0xAB);
	CHECK(ctui_hexview_set(&v, 50, 50) && ctui_hexview_modified(&v) == 2 &&
	      !ctui_hexview_edited(&v, 50));
	CHECK(!ctui_hexview_set(&v, 100, 1) && ctui_hexview_modified(&v) == 2);
	ctui_hexview_revert(&v);
	CHECK(ctui_hexview_modified(&v) == 0 &&
	      ctui_hexview_byte(&v, 10) == 10);
	for (size_t i = 0; i < 90; i++) /* grows past the first allocation */
		CHECK(ctui_hexview_set(&v, 89 - i, 0xFF));
	CHECK(ctui_hexview_modified(&v) == 90 && v.edits[0].offset == 0 &&
	      v.edits[89].offset == 89);
	ctui_hexview_free(&v);
}

static void test_edit_keys(void)
{
	ctui_hexview v;
	fixture(&v);
	CHECK(!ctui_hexview_key(&v, 'a', 5) &&
	      ctui_hexview_modified(&v) == 0); /* not editing */
	v.editing = true;
	v.cursor = 0x20;
	CHECK(ctui_hexview_key(&v, 'a', 5) && v.pending &&
	      ctui_hexview_byte(&v, 0x20) == 0xA0 && v.cursor == 0x20);
	CHECK(ctui_hexview_key(&v, 'F', 5) && !v.pending &&
	      ctui_hexview_byte(&v, 0x20) == 0xAF && v.cursor == 0x21);
	CHECK(ctui_hexview_key(&v, '7', 5) && ctui_hexview_key(&v, '1', 5) &&
	      ctui_hexview_byte(&v, 0x21) == 0x71);
	CHECK(!ctui_hexview_key(&v, 'x', 5) && !ctui_hexview_key(&v, 'g', 5));
	/* The high nibble keeps the low one of the byte. */
	v.cursor = 0x30;
	CHECK(ctui_hexview_key(&v, '9', 5) &&
	      ctui_hexview_byte(&v, 0x30) == 0x90);
	/* Navigation cancels the pending nibble. */
	CHECK(ctui_hexview_key(&v, KEY_RIGHT, 5) && !v.pending &&
	      v.cursor == 0x31);
	CHECK(ctui_hexview_key(&v, '5', 5) && v.pending);
	CHECK(ctui_hexview_key(&v, '2', 5) && v.cursor == 0x32 &&
	      ctui_hexview_byte(&v, 0x31) == 0x52);
	/* Backspace reverts the previous byte and steps back. */
	CHECK(ctui_hexview_key(&v, KEY_BACKSPACE, 5) && v.cursor == 0x31 &&
	      ctui_hexview_byte(&v, 0x31) == 0x31);
	CHECK(!ctui_hexview_edited(&v, 0x31));
	/* ... or the cursor byte when a nibble is pending. */
	CHECK(ctui_hexview_key(&v, 'c', 5) && v.pending);
	CHECK(ctui_hexview_key(&v, 127, 5) && !v.pending && v.cursor == 0x31 &&
	      !ctui_hexview_edited(&v, 0x31));
	/* Tab switches the pane; a printable character sets the byte. */
	CHECK(ctui_hexview_key(&v, '\t', 5) && v.pane == CTUI_HEXPANE_TEXT);
	CHECK(ctui_hexview_key(&v, 'z', 5) &&
	      ctui_hexview_byte(&v, 0x31) == 'z' && v.cursor == 0x32);
	CHECK(ctui_hexview_key(&v, '0', 5) &&
	      ctui_hexview_byte(&v, 0x32) == '0');
	CHECK(!ctui_hexview_key(&v, 5, 5)); /* a control character */
	CHECK(ctui_hexview_key(&v, '\t', 5) && v.pane == CTUI_HEXPANE_HEX);
	/* The last byte: typing stays on it. */
	v.cursor = 99;
	CHECK(ctui_hexview_key(&v, '0', 5) && ctui_hexview_key(&v, '0', 5) &&
	      v.cursor == 99 && ctui_hexview_byte(&v, 99) == 0);
	v.cursor = 0;
	CHECK(ctui_hexview_key(&v, KEY_BACKSPACE, 5) && v.cursor == 0);
	/* Typing the original value back removes the edit. */
	size_t before = ctui_hexview_modified(&v);
	v.cursor = 0x40;
	CHECK(ctui_hexview_key(&v, '4', 5) && ctui_hexview_key(&v, '0', 5));
	CHECK(ctui_hexview_modified(&v) == before &&
	      !ctui_hexview_edited(&v, 0x40));
	ctui_hexview_free(&v);
}

static void test_save(void)
{
	char dir[] = "/tmp/ctui-save-XXXXXX";
	CHECK(mkdtemp(dir) != NULL);
	char src[128], dst[128];
	snprintf(src, sizeof src, "%s/src.bin", dir);
	snprintf(dst, sizeof dst, "%s/dst.bin", dir);
	size_t n = 200000; /* several chunks */
	unsigned char *bytes = malloc(n), *back = malloc(n);
	CHECK(bytes && back);
	for (size_t i = 0; i < n; i++)
		bytes[i] = (unsigned char)(i * 31 + 7);
	FILE *f = fopen(src, "wb");
	CHECK(f && fwrite(bytes, 1, n, f) == n);
	fclose(f);
	ctui_hexview v;
	CHECK(ctui_hexview_map(&v, src));
	size_t offs[] = {0, 1, 65535, 65536, 131071, 131072, n - 1};
	for (size_t i = 0; i < sizeof offs / sizeof offs[0]; i++) {
		bytes[offs[i]] = (unsigned char)(bytes[offs[i]] ^ 0x5AU);
		CHECK(ctui_hexview_set(&v, offs[i], bytes[offs[i]]));
	}
	CHECK(ctui_hexview_save(&v, dst));
	f = fopen(dst, "rb");
	CHECK(f && fread(back, 1, n, f) == n && fgetc(f) == EOF &&
	      memcmp(back, bytes, n) == 0);
	fclose(f);
	/* The source file and the mapping still have the original bytes. */
	CHECK(ctui_hexview_original(&v, 0) ==
	      (unsigned char)(bytes[0] ^ 0x5AU));
	/* Saving over the file the view reads. */
	CHECK(ctui_hexview_save(&v, src));
	f = fopen(src, "rb");
	CHECK(f && fread(back, 1, n, f) == n && memcmp(back, bytes, n) == 0);
	fclose(f);
	ctui_hexview_free(&v);
	/* A failure leaves no temporary file behind. */
	ctui_hexview e;
	fixture(&e);
	ctui_hexview_set(&e, 3, 9);
	char bad[160];
	snprintf(bad, sizeof bad, "%s/missing/out.bin", dir);
	CHECK(!ctui_hexview_save(&e, bad) && errno == ENOENT);
	ctui_hexview_free(&e);
	free(bytes);
	free(back);
	unlink(src);
	unlink(dst);
	CHECK(rmdir(dir) == 0); /* nothing else was left in it */
}

static void test_draw_edit(void)
{
	screen_open(10, 80);
	ctui_hexview v;
	fixture(&v);
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 80, 5, false);
	int err = ctui_attr(CTUI_PAIR_ERROR, false);
	CHECK(!screen_has_color(err));
	ctui_hexview_set(&v, 3, 0xEE);
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 80, 5, false);
	CHECK(screen_has_color(err) && screen_has_text("00 01 02 ee 04"));
	ctui_style hot = {0};
	int orange = ctui_attr_color(16, 214);
	hot.attr[CTUI_PAIR_ERROR] = orange;
	v.style = &hot;
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 80, 5, false);
	CHECK(screen_has_color(orange));
	ctui_hexview_free(&v);
	endwin();
}

static void test_parse(void)
{
	unsigned char out[8];
	size_t n = 99;
	CHECK(ctui_hexview_parse("48 65 6c", out, sizeof out, &n) && n == 3 &&
	      out[0] == 0x48 && out[2] == 0x6c);
	CHECK(ctui_hexview_parse("  DEadbe ef ", out, sizeof out, &n) &&
	      n == 4 && out[0] == 0xde && out[3] == 0xef);
	CHECK(ctui_hexview_parse("\"ab c\"", out, sizeof out, &n) && n == 4 &&
	      memcmp(out, "ab c", 4) == 0);
	CHECK(ctui_hexview_parse("\"ab\"", out, sizeof out, &n) && n == 2 &&
	      out[0] == 'a'); /* not hex 0xab */
	CHECK(ctui_hexview_parse("hello", out, sizeof out, &n) && n == 5 &&
	      memcmp(out, "hello", 5) == 0);
	CHECK(ctui_hexview_parse("abc", out, sizeof out, &n) && n == 3 &&
	      memcmp(out, "abc", 3) == 0); /* odd digits */
	CHECK(ctui_hexview_parse("4 8", out, sizeof out, &n) && n == 1 &&
	      out[0] == 0x48);
	CHECK(ctui_hexview_parse("12 3x", out, sizeof out, &n) && n == 5 &&
	      memcmp(out, "12 3x", 5) == 0);
	CHECK(!ctui_hexview_parse("", out, sizeof out, &n) &&
	      !ctui_hexview_parse("   ", out, sizeof out, &n));
	CHECK(!ctui_hexview_parse("\"\"", out, sizeof out, &n));
	CHECK(!ctui_hexview_parse("\"123456789\"", out, sizeof out, &n));
	CHECK(!ctui_hexview_parse("00 11 22 33 44 55 66 77 88", out, sizeof out,
				  &n));
	CHECK(ctui_hexview_parse("0011223344556677", out, sizeof out, &n) &&
	      n == 8);
	CHECK(ctui_hexview_parse("\"", out, sizeof out, &n) && n == 1 &&
	      out[0] == '"');
}

static void test_find(void)
{
	ctui_hexview v;
	fixture(&v);
	const unsigned char p3[] = {40, 41, 42}, p1[] = {99}, none[] = {42, 41};
	size_t pos = 0;
	CHECK(ctui_hexview_find(&v, p3, 3, &pos, true, 0) ==
		  CTUI_HEXFIND_FOUND &&
	      pos == 40);
	pos = 41;
	CHECK(ctui_hexview_find(&v, p3, 3, &pos, true, 0) ==
	      CTUI_HEXFIND_NOTFOUND);
	pos = 99;
	CHECK(ctui_hexview_find(&v, p3, 3, &pos, false, 0) ==
		  CTUI_HEXFIND_FOUND &&
	      pos == 40);
	pos = 39;
	CHECK(ctui_hexview_find(&v, p3, 3, &pos, false, 0) ==
	      CTUI_HEXFIND_NOTFOUND);
	pos = 99;
	CHECK(ctui_hexview_find(&v, p1, 1, &pos, true, 0) ==
		  CTUI_HEXFIND_FOUND &&
	      pos == 99); /* the last byte */
	pos = 0;
	CHECK(ctui_hexview_find(&v, none, 2, &pos, true, 0) ==
	      CTUI_HEXFIND_NOTFOUND);
	CHECK(ctui_hexview_find(&v, none, 0, &pos, true, 0) ==
	      CTUI_HEXFIND_NOTFOUND);
	/* A budget slices the scan; MORE gives the offset to resume at. */
	pos = 0;
	ctui_hexfind r = ctui_hexview_find(&v, p3, 3, &pos, true, 10);
	CHECK(r == CTUI_HEXFIND_MORE && pos == 10);
	int calls = 1;
	while (r == CTUI_HEXFIND_MORE && calls < 20) {
		r = ctui_hexview_find(&v, p3, 3, &pos, true, 10);
		calls++;
	}
	CHECK(r == CTUI_HEXFIND_FOUND && pos == 40 && calls == 5);
	pos = 99;
	r = ctui_hexview_find(&v, p3, 3, &pos, false, 30);
	CHECK(r == CTUI_HEXFIND_MORE && pos == 67);
	r = ctui_hexview_find(&v, p3, 3, &pos, false, 30);
	CHECK(r == CTUI_HEXFIND_FOUND && pos == 40);
	/* The edited bytes are searched, not the source. */
	ctui_hexview_set(&v, 40, 0xEE);
	pos = 0;
	CHECK(ctui_hexview_find(&v, p3, 3, &pos, true, 0) ==
	      CTUI_HEXFIND_NOTFOUND);
	const unsigned char e3[] = {0xEE, 41, 42};
	CHECK(ctui_hexview_find(&v, e3, 3, &pos, true, 0) ==
		  CTUI_HEXFIND_FOUND &&
	      pos == 40);
	ctui_hexview_free(&v);
}

static void test_find_next(void)
{
	ctui_hexview v;
	fixture(&v);
	for (size_t i = 0; i < 100; i++) /* 0 1 2 ... 9 repeated */
		DATA[i] = (unsigned char)(i % 10);
	const unsigned char pat[] = {3, 4};
	v.cursor = 0;
	CHECK(ctui_hexview_find_next(&v, pat, 2, true) && v.cursor == 3 &&
	      v.selecting && v.anchor == 4);
	size_t a, b;
	CHECK(ctui_hexview_range(&v, &a, &b) && a == 3 && b == 4);
	CHECK(ctui_hexview_find_next(&v, pat, 2, true) && v.cursor == 13);
	CHECK(ctui_hexview_find_next(&v, pat, 2, false) && v.cursor == 3);
	CHECK(ctui_hexview_find_next(&v, pat, 2, false) &&
	      v.cursor == 93); /* wraps backward */
	CHECK(ctui_hexview_find_next(&v, pat, 2, true) &&
	      v.cursor == 3); /* and forward */
	const unsigned char tail[] = {8, 9};
	v.cursor = 99;
	CHECK(ctui_hexview_find_next(&v, tail, 2, true) && v.cursor == 8);
	v.cursor = 0;
	CHECK(ctui_hexview_find_next(&v, tail, 2, false) && v.cursor == 98);
	/* The only match is the one at the cursor: it is found again. */
	const unsigned char one[] = {0xAA};
	ctui_hexview_set(&v, 50, 0xAA);
	v.cursor = 50;
	CHECK(ctui_hexview_find_next(&v, one, 1, true) && v.cursor == 50);
	CHECK(ctui_hexview_find_next(&v, one, 1, false) && v.cursor == 50);
	const unsigned char nope[] = {0xBB};
	v.cursor = 7;
	v.selecting = false;
	CHECK(!ctui_hexview_find_next(&v, nope, 1, true) && v.cursor == 7 &&
	      !v.selecting);
	ctui_hexview_free(&v);
	ctui_hexview empty;
	ctui_hexview_init(&empty, NULL, 0);
	CHECK(!ctui_hexview_find_next(&empty, one, 1, true));
}

static void test_goto(void)
{
	ctui_hexview v;
	fixture(&v);
	v.selecting = true;
	CHECK(ctui_hexview_goto(&v, 77) && v.cursor == 77 && !v.selecting);
	CHECK(ctui_hexview_goto(&v, 99) && v.cursor == 99);
	CHECK(!ctui_hexview_goto(&v, 100) && v.cursor == 99);
	ctui_hexview_free(&v);
}

static void test_draw(void)
{
	screen_open(10, 80);
	unsigned char data[40] = "Hello, world!\x00\x01\xff";
	ctui_hexview v;
	ctui_hexview_init(&v, data, sizeof data);
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 80, 5, true);
	CHECK(screen_has_text(
	    "00000000  48 65 6c 6c 6f 2c 20 77  6f 72 6c 64 21 00 01 ff"));
	CHECK(screen_has_text("Hello, world!..."));
	CHECK(screen_has_text("00000010"));
	CHECK(v.row_bytes == 16);
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 50, 5, true);
	CHECK(v.row_bytes == 8);
	CHECK(screen_has_text("00000008") && screen_has_text("00000020"));
	/* Past the end of the data nothing is drawn. */
	ctui_hexview_init(&v, data, 3);
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 80, 5, true);
	CHECK(screen_has_text("48 65 6c") && !screen_has_text("00000010"));
	/* The cursor follows scrolling. */
	ctui_hexview_init(&v, DATA, sizeof DATA);
	v.cursor = 90;
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 80, 3, true);
	CHECK(v.top == 3 && screen_has_text("00000050") &&
	      !screen_has_text("00000000"));
	/* Per-instance style. */
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 80, 3, false);
	int orange = ctui_attr_color(16, 214);
	CHECK(!screen_has_color(orange));
	ctui_style hot = {0};
	hot.attr[CTUI_PAIR_SELECTION] = orange;
	v.style = &hot;
	v.selecting = true;
	v.anchor = 50;
	screen_clear();
	ctui_hexview_draw(&v, 0, 0, 80, 3, false);
	CHECK(screen_has_color(orange));
	endwin();
}

int main(void)
{
	RUN(test_layout);
	RUN(test_keys);
	RUN(test_selection);
	RUN(test_scroll);
	RUN(test_hit);
	RUN(test_mouse);
	RUN(test_load);
	RUN(test_map);
	RUN(test_draw);
	RUN(test_edit);
	RUN(test_edit_keys);
	RUN(test_save);
	RUN(test_draw_edit);
	RUN(test_parse);
	RUN(test_find);
	RUN(test_find_next);
	RUN(test_goto);
	return CHECK_DONE();
}
