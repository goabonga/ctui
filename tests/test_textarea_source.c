/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <ctui/textarea.h>
#include <stdint.h>

/* Deliberately split UTF-8, newlines and edits across seven-byte pieces. */
#define PIECE 7
#define CAPACITY 16384
typedef struct pieces {
	char data[(CAPACITY + PIECE - 1) / PIECE][PIECE];
	size_t len, calls, bytes, largest;
	bool fail_read, fail_replace;
} pieces;

static char byte_at(const pieces *p, size_t i)
{
	return p->data[i / PIECE][i % PIECE];
}
static void assign(pieces *p, const char *s, size_t n)
{
	CHECK(n <= CAPACITY);
	for (size_t i = 0; i < n; i++)
		p->data[i / PIECE][i % PIECE] = s[i];
	p->len = n;
}
static size_t length(void *ctx)
{
	return ((pieces *)ctx)->len;
}
static size_t lines(void *ctx)
{
	pieces *p = ctx;
	size_t n = 1;
	for (size_t i = 0; i < p->len; i++)
		n += byte_at(p, i) == '\n';
	return n;
}
static size_t start(void *ctx, size_t line)
{
	pieces *p = ctx;
	if (!line)
		return 0;
	for (size_t i = 0; i < p->len; i++)
		if (byte_at(p, i) == '\n' && --line == 0)
			return i + 1;
	CHECK(false);
	return 0;
}
static bool read_pieces(void *ctx, size_t offset, char *out, size_t n)
{
	pieces *p = ctx;
	CHECK(offset <= p->len && n <= p->len - offset);
	CHECK(n && n <= CTUI_TEXTAREA_CHUNK);
	p->calls++;
	p->bytes += n;
	if (n > p->largest)
		p->largest = n;
	if (p->fail_read)
		return false;
	for (size_t i = 0; i < n; i++)
		out[i] = byte_at(p, offset + i);
	return true;
}
static bool replace_pieces(void *ctx, size_t pos, size_t del, const char *ins,
			   size_t n)
{
	pieces *p = ctx;
	CHECK(pos <= p->len && del <= p->len - pos);
	if (p->fail_replace || n > CAPACITY - (p->len - del))
		return false;
	char copy[CAPACITY];
	for (size_t i = 0; i < pos; i++)
		copy[i] = byte_at(p, i);
	memcpy(copy + pos, ins, n);
	for (size_t i = pos + del; i < p->len; i++)
		copy[i - del + n] = byte_at(p, i);
	assign(p, copy, p->len - del + n);
	return true;
}
static ctui_textarea_source source(pieces *p)
{
	return (ctui_textarea_source){p,     length,	  lines,
				      start, read_pieces, replace_pieces};
}
static void expect(ctui_textarea *t, const char *text)
{
	char actual[CAPACITY + 1];
	CHECK(t->len == strlen(text));
	CHECK(t->len <= CAPACITY);
	if (t->len > CAPACITY)
		return;
	CHECK(ctui_textarea_read(t, 0, actual, t->len));
	actual[t->len] = 0;
	CHECK_STR(actual, text);
	CHECK(t->text == NULL && t->starts == NULL && t->cap == 0 &&
	      t->starts_cap == 0);
	CHECK(ctui_textarea_text(t) == NULL);
}

static void test_source_lifetime_and_ranges(void)
{
	pieces p = {0};
	assign(&p, "one\ntwo\n", 8);
	ctui_textarea_source s = source(&p);
	ctui_textarea t;
	CHECK(ctui_textarea_init_source(&t, &s));
	memset(&s, 0,
	       sizeof s); /* the descriptor, not the context, was copied */
	CHECK(p.calls == 0 && t.line_count == 3);
	expect(&t, "one\ntwo\n");
	size_t offset, len, line, byte;
	ctui_textarea_line_bounds(&t, 1, &offset, &len);
	CHECK(offset == 4 && len == 3);
	CHECK(ctui_textarea_line(&t, 1, &len) == NULL && len == 3);
	ctui_textarea_line_bounds(&t, SIZE_MAX, &offset, &len);
	CHECK(offset == 8 && len == 0);
	ctui_textarea_locate(&t, SIZE_MAX, &line, &byte);
	CHECK(line == 2 && byte == 0);
	CHECK(ctui_textarea_offset(&t, 1, SIZE_MAX) == 7);
	char out[8];
	CHECK(ctui_textarea_read(&t, 2, out, 5) &&
	      memcmp(out, "e\ntwo", 5) == 0);
	CHECK(ctui_textarea_read(&t, t.len, NULL, 0));
	CHECK(!ctui_textarea_read(&t, t.len + 1, out, 0));
	CHECK(!ctui_textarea_read(&t, 0, out, SIZE_MAX));
	CHECK(!ctui_textarea_read(&t, 0, NULL, 1));
	ctui_textarea_free(&t);
	CHECK(p.len == 8 && byte_at(&p, 0) == 'o');
	CHECK(ctui_textarea_init(&t, "owned"));
	CHECK(ctui_textarea_read(&t, 1, out, 3) && memcmp(out, "wne", 3) == 0);
	ctui_textarea_line_bounds(&t, 0, &offset, &len);
	CHECK(offset == 0 && len == 5);
	ctui_textarea_free(&t);
}

static size_t no_lines(void *ctx)
{
	(void)ctx;
	return 0;
}
static size_t oversized(void *ctx)
{
	(void)ctx;
	return SIZE_MAX;
}
static size_t bad_start(void *ctx, size_t line)
{
	(void)ctx;
	(void)line;
	return 1;
}
static void test_source_metadata_and_chunked_reads(void)
{
	pieces p = {0};
	char text[CTUI_TEXTAREA_CHUNK * 2 + 8], out[sizeof text];
	memset(text, 'z', sizeof text);
	assign(&p, text, sizeof text);
	ctui_textarea_source s = source(&p), bad = s;
	ctui_textarea t;
	bad.lines = no_lines;
	CHECK(!ctui_textarea_init_source(&t, &bad));
	ctui_textarea_free(&t);
	bad = s;
	bad.length = oversized;
	CHECK(!ctui_textarea_init_source(&t, &bad));
	ctui_textarea_free(&t);
	bad = s;
	bad.line_start = bad_start;
	CHECK(!ctui_textarea_init_source(&t, &bad));
	ctui_textarea_free(&t);
	CHECK(ctui_textarea_init_source(&t, &s));
	CHECK(p.calls == 0);
	CHECK(ctui_textarea_read(&t, 0, out, sizeof out));
	CHECK(memcmp(out, text, sizeof out) == 0);
	CHECK(p.calls == 3 && p.largest == CTUI_TEXTAREA_CHUNK);
	ctui_textarea_free(&t);
}

static void test_source_edits_and_history(void)
{
	pieces p = {0};
	assign(&p, "hello\nworld", 11);
	ctui_textarea t;
	ctui_textarea_source s = source(&p);
	CHECK(ctui_textarea_init_source(&t, &s));
	CHECK(ctui_textarea_replace(&t, 3, 5, "P\nQ\n", 4));
	expect(&t, "helP\nQ\nrld");
	CHECK(t.line_count == 3 && t.modified);
	CHECK(ctui_textarea_undo(&t));
	expect(&t, "hello\nworld");
	CHECK(t.line_count == 2);
	CHECK(ctui_textarea_redo(&t));
	expect(&t, "helP\nQ\nrld");
	t.anchor = 2;
	t.cursor = 8;
	t.selecting = true;
	CHECK(ctui_textarea_copy(&t));
	CHECK_STR(ctui_textarea_clipboard(&t), "lP\nQ\nr");
	CHECK(ctui_textarea_cut(&t));
	expect(&t, "held");
	CHECK(ctui_textarea_paste(&t));
	expect(&t, "helP\nQ\nrld");
	CHECK(ctui_textarea_set_text(&t, "reset\n"));
	expect(&t, "reset\n");
	CHECK(t.line_count == 2 && t.cursor == 0 && !t.modified &&
	      !t.undo_len && !t.redo_len);
	CHECK(ctui_textarea_key(&t, 'a', 3));
	CHECK(ctui_textarea_key(&t, 'b', 3));
	CHECK(t.undo_len == 1);
	CHECK(ctui_textarea_undo(&t));
	expect(&t, "reset\n");
	CHECK(ctui_textarea_redo(&t));
	expect(&t, "abreset\n");
	ctui_textarea_free(&t);
}

static void test_source_failures_and_limits(void)
{
	pieces p = {0};
	assign(&p, "abc", 3);
	ctui_textarea_source s = source(&p);
	ctui_textarea t;
	CHECK(!ctui_textarea_init_source(&t, NULL));
	ctui_textarea_free(&t);
	ctui_textarea_source bad = s;
	bad.read = NULL;
	CHECK(!ctui_textarea_init_source(&t, &bad));
	ctui_textarea_free(&t);
	CHECK(ctui_textarea_init_source(&t, &s));
	CHECK(ctui_textarea_key(&t, 'x', 3));
	p.fail_replace = true;
	CHECK(!ctui_textarea_key(&t, 'y', 3)); /* failed merged insertion */
	CHECK(!ctui_textarea_undo(&t));
	CHECK(t.cursor == 1 && t.undo_len == 1 && !t.redo_len);
	p.fail_replace = false;
	CHECK(ctui_textarea_undo(&t));
	p.fail_read = true;
	CHECK(!ctui_textarea_delete(&t, 0, 1));
	CHECK(!ctui_textarea_read(&t, 0, (char[3]){0}, 3));
	t.anchor = 0;
	t.cursor = 2;
	t.selecting = true;
	CHECK(!ctui_textarea_copy(&t));
	p.fail_read = false;
	expect(&t, "abc");
	CHECK(!ctui_textarea_replace(&t, SIZE_MAX, 1, "", 0));
	CHECK(!ctui_textarea_replace(&t, 1, SIZE_MAX, "", 0));
	CHECK(!ctui_textarea_insert(&t, 0, "x", SIZE_MAX));
	t.max_bytes = 3;
	CHECK(!ctui_textarea_insert(&t, 0, "x", 1));
	CHECK(t.redo_len == 1);
	t.read_only = true;
	CHECK(!ctui_textarea_set_text(&t, ""));
	CHECK(!ctui_textarea_redo(&t));
	ctui_textarea_free(&t);
	s.replace = NULL;
	CHECK(ctui_textarea_init_source(&t, &s) && t.read_only);
	t.read_only = false; /* the absent callback still prevents writes */
	CHECK(!ctui_textarea_insert(&t, 0, "x", 1));
	CHECK(!ctui_textarea_set_text(&t, ""));
	expect(&t, "abc");
	ctui_textarea_free(&t);
	assign(&p, "", 0);
	CHECK(ctui_textarea_init_source(&t, &s));
	CHECK(t.line_count == 1 && t.len == 0);
	CHECK(ctui_textarea_read(&t, 0, NULL, 0));
	ctui_textarea_free(&t);
}

/* Generated multi-gigabyte documents: no text allocation or line index. */
typedef struct generated {
	size_t len, width, calls, bytes, largest;
} generated;
static size_t generated_length(void *ctx)
{
	return ((generated *)ctx)->len;
}
static size_t generated_lines(void *ctx)
{
	generated *g = ctx;
	return g->len / g->width + 1;
}
static size_t generated_start(void *ctx, size_t line)
{
	return line * ((generated *)ctx)->width;
}
static bool generated_read(void *ctx, size_t offset, char *out, size_t n)
{
	generated *g = ctx;
	CHECK(offset <= g->len && n <= g->len - offset &&
	      n <= CTUI_TEXTAREA_CHUNK);
	g->calls++;
	g->bytes += n;
	if (n > g->largest)
		g->largest = n;
	for (size_t i = 0; i < n; i++)
		out[i] = (offset + i + 1) % g->width ? 'X' : '\n';
	return true;
}
static void test_large_source_viewport(void)
{
	if (SIZE_MAX <= UINT32_MAX)
		return;
	generated g = {.len = (size_t)UINT32_MAX + 1, .width = 128};
	ctui_textarea_source s = {&g,
				  generated_length,
				  generated_lines,
				  generated_start,
				  generated_read,
				  NULL};
	ctui_textarea t;
	CHECK(ctui_textarea_init_source(&t, &s));
	CHECK(g.calls == 0 && !t.text && !t.starts);
	ctui_textarea_scroll_to(&t, 5, t.line_count - 8);
	CHECK(t.top == t.line_count - 8);
	screen_clear();
	ctui_textarea_draw(&t, 0, 0, 40, 5, true);
	CHECK(screen_has_text("XXXXXXXXXXXXXXXX"));
	CHECK(g.bytes < 8192 && g.largest <= CTUI_TEXTAREA_CHUNK);
	CHECK(!t.text && !t.starts && !t.undo && !t.redo);
	size_t offset;
	CHECK(ctui_textarea_hit(&t, 40, 5, 3, 10, &offset));
	CHECK(offset == (t.top + 3) * 128 + 10);
	ctui_textarea_free(&t);
	/* One huge line must not be copied in order to show its first columns.
	 */
	g.width = g.len + 1;
	g.bytes = g.calls = 0;
	CHECK(ctui_textarea_init_source(&t, &s));
	screen_clear();
	ctui_textarea_draw(&t, 0, 0, 40, 5, true);
	CHECK(g.bytes <= CTUI_TEXTAREA_CHUNK &&
	      screen_has_text("XXXXXXXXXXXXXXXX"));
	ctui_textarea_free(&t);
}

static void test_source_utf8_and_draw(void)
{
	pieces p = {0};
	const char *text = "123456\xe6\x97\xa5\twide\ne\xcc\x81\n\x1b[31m\x07";
	assign(&p, text, strlen(text));
	ctui_textarea_source s = source(&p);
	ctui_textarea t, owned;
	CHECK(ctui_textarea_init_source(&t, &s));
	CHECK(ctui_textarea_init(&owned, text));
	for (size_t i = 0; i <= t.len; i++) {
		CHECK(ctui_textarea_next(&t, i) ==
		      ctui_textarea_next(&owned, i));
		CHECK(ctui_textarea_prev(&t, i) ==
		      ctui_textarea_prev(&owned, i));
		t.cursor = owned.cursor = i;
		CHECK(ctui_textarea_column(&t) == ctui_textarea_column(&owned));
	}
	t.cursor = 0;
	t.line_numbers = true;
	screen_clear();
	ctui_textarea_draw(&t, 0, 0, 40, 5, true);
	CHECK(screen_has_text("1 123456\xe6\x97\xa5"));
	CHECK(screen_has_text("?[31m?"));
	t.cursor = 6;
	CHECK(ctui_textarea_key(&t, KEY_DC, 5));
	expect(&t, "123456\twide\ne\xcc\x81\n\x1b[31m\x07");
	CHECK(ctui_textarea_undo(&t));
	expect(&t, text);
	ctui_textarea_free(&t);
	ctui_textarea_free(&owned);
}

static void test_chunk_boundary_utf8(void)
{

	pieces p = {0};
	char text[CTUI_TEXTAREA_CHUNK + 8];
	memset(text, 'a', sizeof text);
	const char emoji[] = "\xf0\x9f\x98\x80";
	for (size_t i = 0; i < 4; i++)
		text[CTUI_TEXTAREA_CHUNK - 1 + i] = emoji[i];
	assign(&p, text, sizeof text);
	ctui_textarea_source s = source(&p);
	ctui_textarea t;
	CHECK(ctui_textarea_init_source(&t, &s));
	t.cursor = CTUI_TEXTAREA_CHUNK - 1;
	CHECK(ctui_textarea_next(&t, t.cursor) == CTUI_TEXTAREA_CHUNK + 3);
	CHECK(ctui_textarea_prev(&t, CTUI_TEXTAREA_CHUNK + 3) ==
	      CTUI_TEXTAREA_CHUNK - 1);
	t.cursor = sizeof text;
	CHECK(ctui_textarea_column(&t) == sizeof text - 2);
	t.cursor = CTUI_TEXTAREA_CHUNK - 1;
	t.left = CTUI_TEXTAREA_CHUNK - 2;
	screen_clear();
	ctui_textarea_draw(&t, 0, 0, 10, 3, true);
	CHECK(screen_has_text("\xf0\x9f\x98\x80"));
	ctui_textarea_free(&t);
}

static void test_source_matches_owned_operations(void)
{
	pieces p = {0};
	const char *text = "one\n\t\xe6\x97\xa5two\nlast\n";
	assign(&p, text, strlen(text));
	ctui_textarea t, owned;
	ctui_textarea_source s = source(&p);
	CHECK(ctui_textarea_init_source(&t, &s));
	char initial[CAPACITY + 1];
	CHECK(ctui_textarea_read(&t, 0, initial, p.len));
	initial[p.len] = 0;
	CHECK(ctui_textarea_init(&owned, initial));
	const int keys[] = {'a',       '\n',	  '\t',	      KEY_LEFT,
			    KEY_RIGHT, KEY_UP,	  KEY_DOWN,   KEY_HOME,
			    KEY_END,   KEY_NPAGE, KEY_PPAGE,  KEY_BACKSPACE,
			    KEY_DC,    KEY_SLEFT, KEY_SRIGHT, KEY_SR,
			    KEY_SF,    1,	  3,	      24,
			    22,	       26,	  25,	      27,
			    0xc3,      0xa9};
	uint32_t random = 42;
	for (size_t i = 0; i < 600; i++) {
		random = random * 1664525u + 1013904223u;
		int key = keys[(random >> 16u) % (sizeof keys / sizeof *keys)];
		CHECK(ctui_textarea_key(&t, key, 4) ==
		      ctui_textarea_key(&owned, key, 4));
		expect(&t, ctui_textarea_text(&owned));
		CHECK(t.cursor == owned.cursor && t.anchor == owned.anchor &&
		      t.selecting == owned.selecting && t.goal == owned.goal);
		CHECK(t.line_count == owned.line_count &&
		      t.modified == owned.modified);
		CHECK(t.undo_len == owned.undo_len &&
		      t.redo_len == owned.redo_len);
		if (i % 13 == 0) {
			ctui_textarea_scroll_to(&t, 3, 2);
			ctui_textarea_scroll_to(&owned, 3, 2);
			CHECK(t.top == owned.top && t.cursor == owned.cursor);
			CHECK(ctui_textarea_press(&t, 20, 3, 0, 2) ==
			      ctui_textarea_press(&owned, 20, 3, 0, 2));
			CHECK(ctui_textarea_drag(&t, 20, 3, 2, 5) ==
			      ctui_textarea_drag(&owned, 20, 3, 2, 5));
			CHECK(ctui_textarea_release(&t) ==
			      ctui_textarea_release(&owned));
			CHECK(t.cursor == owned.cursor &&
			      t.anchor == owned.anchor);
		}
	}
	ctui_textarea_free(&t);
	ctui_textarea_free(&owned);
}

int main(void)
{
	RUN(test_source_lifetime_and_ranges);
	RUN(test_source_metadata_and_chunked_reads);
	RUN(test_source_edits_and_history);
	RUN(test_source_failures_and_limits);
	screen_open(24, 80);
	RUN(test_large_source_viewport);
	RUN(test_source_utf8_and_draw);
	RUN(test_chunk_boundary_utf8);
	RUN(test_source_matches_owned_operations);
	return CHECK_DONE();
}
